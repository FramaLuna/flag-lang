// runs a module that flagc -target wasm-wasm32 or wasm-wasm64 wrote, in a thread with room for deep recursion, with the
// functions of WASI preview 1 that core calls: on wasm64 each pointer and size is 64 bits, in memory and as a BigInt.
// _start ends with proc_exit, and a trap ends with 134 after its message and the functions it went through.
// env.apply(f, x) calls back the function value f of the module, through the table it exports.
// A library, with no _start, shows what it imports and exports, and what each function without parameters gives, with
// an n after a BigInt.
const { Worker, isMainThread, workerData, parentPort } = require("worker_threads")
const fs = require("fs")
if (isMainThread) {
    const worker = new Worker(__filename, { workerData: process.argv.slice(2), resourceLimits: { stackSizeMb: 256 } })
    worker.on("message", (code) => (process.exitCode = code & 255))
} else {
    const module = new WebAssembly.Module(fs.readFileSync(workerData[0]))
    let instance
    const exit = {}
    const address = (at) => (typeof at == "bigint" ? Number(at) : at >>> 0) // on wasm32 past 2 GiB it is an i32 below 0
    const view = () => new DataView(instance.exports.memory.buffer) // a new one after memory grows
    const get = (wide, at) => (wide ? Number(view().getBigUint64(at, true)) : view().getUint32(at, true))
    const put = (wide, at, n) => (wide ? view().setBigUint64(at, BigInt(n), true) : view().setUint32(at, n, true))
    const moved = (fd, pieces, count, done, move) => {
        const wide = typeof pieces == "bigint"
        let total = 0
        try {
            for (let k = 0, at = address(pieces); k < address(count); k++, at += wide ? 16 : 8) {
                const bytes = new Uint8Array(instance.exports.memory.buffer, get(wide, at), get(wide, at + (wide ? 8 : 4)))
                const n = move(fd, bytes)
                total += n
                if (n < bytes.length) break
            }
        } catch (e) {
            return { EAGAIN: 6, EBADF: 8, EPIPE: 64 }[e.code] ?? 29 // or EIO
        }
        put(wide, address(done), total)
        return 0
    }
    const args = workerData.map((a) => Buffer.from(a + "\0"))
    const wasi_snapshot_preview1 = {
        fd_write: (fd, pieces, count, done) => moved(fd, pieces, count, done, fs.writeSync),
        fd_read: (fd, pieces, count, done) => moved(fd, pieces, count, done, fs.readSync),
        args_sizes_get(count, size) {
            const wide = typeof count == "bigint"
            put(wide, address(count), args.length)
            put(wide, address(size), args.reduce((sum, a) => sum + a.length, 0))
            return 0
        },
        args_get(starts, text) {
            const wide = typeof starts == "bigint"
            let at = address(text)
            args.forEach((a, k) => {
                put(wide, address(starts) + k * (wide ? 8 : 4), at)
                new Uint8Array(instance.exports.memory.buffer).set(a, at)
                at += a.length
            })
            return 0
        },
        proc_exit(code) {
            exit.code = code
            throw exit
        },
    }
    const env = { apply: (f, x) => instance.exports.__indirect_function_table.get(Number(f))(x) }
    const say = (text) => fs.writeSync(1, text + "\n")
    instance = new WebAssembly.Instance(module, { wasi_snapshot_preview1, env })
    try {
        if (instance.exports._start) {
            instance.exports._start()
            parentPort.postMessage(0)
        } else {
            const shown = (list) => list.map((e) => (e.module ? e.module + "." : "") + e.name + " " + e.kind).join(", ") || "nothing"
            say("imports " + shown(WebAssembly.Module.imports(module)))
            say("exports " + shown(WebAssembly.Module.exports(module)))
            for (const { name, kind } of WebAssembly.Module.exports(module))
                if (kind == "function" && instance.exports[name].length == 0) {
                    const given = instance.exports[name]()
                    say(name + "() = " + given + (typeof given == "bigint" ? "n" : ""))
                }
            parentPort.postMessage(0)
        }
    } catch (e) {
        if (e !== exit) {
            fs.writeSync(2, "flag: " + e.message + "\n")
            for (const [, name] of e.stack.matchAll(/at (\S+) \(wasm:/g))
                fs.writeSync(2, "  in " + name + "\n")
        }
        parentPort.postMessage(e === exit ? exit.code : 134)
    }
}
