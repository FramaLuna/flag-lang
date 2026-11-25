#!/bin/sh
compiler=${1:-build/flagc0}
target=${2:+-target $2}
case ${2:-} in windows-*) exe=.exe ;; *) exe= ;; esac
out=build/tests
[ -n "$target" ] && out=$PWD/$out # zig cc -shared fails on a relative -o
[ -f "$compiler.exe" ] && compiler=$compiler.exe
pass=0
fail=0
skipped=0
limit=120
line='
'
words=$IFS # how the shell splits words, which a line of name.args changes for a moment
exec 3>&1
within() {
    "$@" &
    job=$!
    (
        waited=0
        while [ $waited -lt $limit ] && kill -0 $job 2> /dev/null; do
            sleep 1
            waited=$((waited + 1))
        done
        kill -9 $job 2> /dev/null && echo "FAIL $name: still running after $limit seconds, killed" >&3
    ) > /dev/null 2>&1 &
    wait $job 2> /dev/null # on Windows a crash ends with its exception code, and the shell would say it was a signal
}
# with a test named, runs only it, and writes what it prints to name.report and how it went to name.counts
if [ -n "${3:-}" ]; then
    name=$3
    exec > "$out/$name.report" 2>&1 3>&1
    files=
    cc=
    [ -f "tests/$name.flg" ] && files=tests/$name.flg
    if [ -d "tests/$name" ]; then
        files=$(ls -d "tests/$name/"* | grep -v '\.c$')
        ls "tests/$name/"*.c > /dev/null 2>&1 && cc="-cc $(ls "tests/$name/"*.c)"
    fi
    flagc=$compiler
    vars=
    if [ -f "tests/$name.env" ]; then
        flagc=$out/bin/$(basename "$compiler")
        vars=$(cat "tests/$name.env")
    fi
    builds=" " # each line of name.args builds the test once more, with what it says
    [ -f "tests/$name.args" ] && builds=$(cat "tests/$name.args")
    [ -n "$target" ] && grep -qsE 'FLAGC|bin/flagc|: flagc ' $files "tests/$name.err" < /dev/null && builds= # it starts this compiler, or its error repeats the command line
    IFS=$line
    for build in $builds; do
        IFS=$words
        args="$target $build"
        shown=$name # with the build it failed in, when it has several
        case $builds in *"$line"*) shown="$name ($build)" ;; esac
        host= # what runs the program: nothing for one of this machine, node for a WebAssembly module
        case " $args " in
        *" -target wasm-wasm32 "*) host=$wasm32 ;;
        *" -target wasm-wasm64 "*) host=$wasm64 ;;
        esac
        if [ -f "tests/$name.err" ]; then
            if within env $vars "$flagc" $files -o "$out/$name$exe" $args $cc > "$out/$name.got" 2>&1; then
                echo "FAIL $shown: compiled, expected an error"
                fail=$((fail + 1))
            elif grep -qF -e "$(cat "tests/$name.err")" "$out/$name.got"; then
                pass=$((pass + 1))
            else
                echo "FAIL $shown: wrong error"
                cat "$out/$name.got"
                fail=$((fail + 1))
            fi
            continue
        fi
        if [ -f "tests/$name.warn" ]; then
            if ! within env $vars "$flagc" $files -o "$out/$name$exe" -memory $args $cc > "$out/$name.got" 2>&1; then
                echo "FAIL $shown: does not compile"
                cat "$out/$name.got"
                fail=$((fail + 1))
            elif [ -s "tests/$name.warn" ] && ! grep -qF -e "$(cat "tests/$name.warn")" "$out/$name.got"; then
                echo "FAIL $shown: wrong warning"
                cat "$out/$name.got"
                fail=$((fail + 1))
            elif [ ! -s "tests/$name.warn" ] && grep -q "warning:" "$out/$name.got"; then
                echo "FAIL $shown: unexpected warning"
                cat "$out/$name.got"
                fail=$((fail + 1))
            else
                pass=$((pass + 1))
            fi
            continue
        fi
        if ! within env $vars "$flagc" $files -o "$out/$name$exe" $args $cc > "$out/$name.got" 2>&1; then
            echo "FAIL $shown: does not compile"
            cat "$out/$name.got"
            fail=$((fail + 1))
            continue
        fi
        case " $args " in
        *" -check "*)
            if cmp -s "$out/$name.got" "tests/$name.out"; then
                pass=$((pass + 1))
            else
                echo "FAIL $shown: the compiler printed something else"
                diff "tests/$name.out" "$out/$name.got" | head -20
                fail=$((fail + 1))
            fi
            continue
            ;;
        esac
        if [ -s "$out/$name.got" ]; then
            echo "FAIL $shown: compiles, but not in silence"
            cat "$out/$name.got"
            fail=$((fail + 1))
            continue
        fi
        if [ "$host" = none ]; then
            skipped=$((skipped + 1)) # without a node, or one with 64-bit memory for wasm64, there is nothing to run the module
            continue
        elif [ ! -f "tests/$name.gdb" ]; then
            within env FLAGC="$compiler" $host "$out/$name$exe" < /dev/null > "$out/$name.got" 2>&1
        elif [ -z "$target" ] && command -v gdb > /dev/null; then
            within gdb -batch -nx -x "tests/$name.gdb" "$out/$name" < /dev/null 2>&1 | sed -e /libthread_db/d -e 's/^Thread [0-9]* hit //' > "$out/$name.got"
        else
            continue # without gdb there is nothing to read the DWARF
        fi
        if cmp -s "$out/$name.got" "tests/$name.out"; then
            pass=$((pass + 1))
        else
            echo "FAIL $shown: wrong output"
            diff "tests/$name.out" "$out/$name.got" | head -20
            fail=$((fail + 1))
        fi
    done
    echo "$pass $fail $skipped" > "$out/$name.counts"
    exit
fi
mkdir -p "$out/bin"
cp "$compiler" "$out/bin/"
# what runs a WebAssembly module: node for wasm32, and for wasm64 a node with 64-bit memory, which before Node 24 takes a
# flag, or none
wasm32=none
command -v node > /dev/null && wasm32="node tests/wasm.js"
wasm64=none
for node in "node" "node --experimental-wasm-memory64"; do
    if [ "$wasm64" = none ] && $node -e 'process.exit(WebAssembly.validate(new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0, 5, 3, 1, 4, 1])) ? 0 : 1)' > /dev/null 2>&1; then
        wasm64="$node tests/wasm.js"
    fi
done
export wasm32 wasm64
tests=$(for expected in tests/*.out tests/*.err tests/*.warn; do name=${expected#tests/}; echo "${name%.*}"; done)
echo "$tests" | xargs -n 1 -P "${JOBS:-$(nproc 2> /dev/null || sysctl -n hw.ncpu)}" sh "$0" "$compiler" "${2:-}"
for name in $tests; do
    [ -s "$out/$name.report" ] && cat "$out/$name.report"
    read passes fails skips < "$out/$name.counts"
    pass=$((pass + passes))
    fail=$((fail + fails))
    skipped=$((skipped + skips))
done
for src in examples/*.flg; do
    name=$src
    if within "$compiler" "$src" -check $target > "$out/example.got" 2>&1; then
        pass=$((pass + 1))
    else
        echo "FAIL $src: does not compile"
        cat "$out/example.got"
        fail=$((fail + 1))
    fi
done
[ "$skipped" -gt 0 ] && echo "$skipped skipped, with no node to run their WebAssembly module"
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
