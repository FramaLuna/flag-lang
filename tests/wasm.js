// runs a module that flagc -target wasm-wasm32 or wasm-wasm64 wrote, in a thread with room for deep recursion:
// env.write goes to the descriptor it names, env.exit and main give the exit code, and a trap ends with 134 after its message and the functions it went through.
// A library, with no main, shows what it imports and exports, and what each function without parameters gives, with an n after a BigInt.
const { Worker, isMainThread, workerData, parentPort } = require("worker_threads")
const fs = require("fs")
if (isMainThread) {
    const worker = new Worker(__filename, { workerData: process.argv[2], resourceLimits: { stackSizeMb: 256 } })
    worker.on("message", (code) => (process.exitCode = code & 255))
} else {
    const module = new WebAssembly.Module(fs.readFileSync(workerData))
    let memory
    const exit = {}
    const env = {
        write(fd, at, size) {
            fs.writeSync(fd, new Uint8Array(memory.buffer, Number(at), Number(size)))
            return Number(size)
        },
        exit(code) {
            exit.code = code
            throw exit
        },
    }
    const say = (text) => fs.writeSync(1, text + "\n")
    const instance = new WebAssembly.Instance(module, { env })
    memory = instance.exports.memory
    try {
        if (instance.exports.main) {
            parentPort.postMessage(instance.exports.main())
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
