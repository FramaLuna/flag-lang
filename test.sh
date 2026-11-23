#!/bin/sh
compiler=${1:-build/flagc0}
target=${2:+-target $2}
case ${2:-} in windows-*) exe=.exe ;; *) exe= ;; esac
out=build/tests
[ -n "$target" ] && out=$PWD/$out # zig cc -shared fails on a relative -o
mkdir -p "$out/bin"
[ -f "$compiler.exe" ] && compiler=$compiler.exe
cp "$compiler" "$out/bin/"
pass=0
fail=0
limit=120
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
for expected in tests/*.out tests/*.err tests/*.warn; do
    name=$(basename "${expected%.*}")
    files=
    cc=
    [ -f "tests/$name.flg" ] && files=tests/$name.flg
    if [ -d "tests/$name" ]; then
        files=$(ls -d "tests/$name/"* | grep -v '\.c$')
        ls "tests/$name/"*.c > /dev/null 2>&1 && cc="-cc $(ls "tests/$name/"*.c)"
    fi
    [ -n "$target" ] && grep -qsE 'FLAGC|bin/flagc|: flagc ' $files "tests/$name.err" < /dev/null && continue # it starts this compiler, or its error repeats the command line
    args=$target
    [ -f "tests/$name.args" ] && args="$target $(cat "tests/$name.args")"
    flagc=$compiler
    vars=
    if [ -f "tests/$name.env" ]; then
        flagc=$out/bin/$(basename "$compiler")
        vars=$(cat "tests/$name.env")
    fi
    if [ -f "tests/$name.err" ]; then
        if within env $vars "$flagc" $files -o "$out/$name$exe" $args $cc > "$out/$name.got" 2>&1; then
            echo "FAIL $name: compiled, expected an error"
            fail=$((fail + 1))
        elif grep -qF -e "$(cat "tests/$name.err")" "$out/$name.got"; then
            pass=$((pass + 1))
        else
            echo "FAIL $name: wrong error"
            cat "$out/$name.got"
            fail=$((fail + 1))
        fi
        continue
    fi
    if [ -f "tests/$name.warn" ]; then
        if ! within env $vars "$flagc" $files -o "$out/$name$exe" -memory $args $cc > "$out/$name.got" 2>&1; then
            echo "FAIL $name: does not compile"
            cat "$out/$name.got"
            fail=$((fail + 1))
        elif [ -s "tests/$name.warn" ] && ! grep -qF -e "$(cat "tests/$name.warn")" "$out/$name.got"; then
            echo "FAIL $name: wrong warning"
            cat "$out/$name.got"
            fail=$((fail + 1))
        elif [ ! -s "tests/$name.warn" ] && grep -q "warning:" "$out/$name.got"; then
            echo "FAIL $name: unexpected warning"
            cat "$out/$name.got"
            fail=$((fail + 1))
        else
            pass=$((pass + 1))
        fi
        continue
    fi
    if ! within env $vars "$flagc" $files -o "$out/$name$exe" $args $cc > "$out/$name.got" 2>&1; then
        echo "FAIL $name: does not compile"
        cat "$out/$name.got"
        fail=$((fail + 1))
        continue
    fi
    case " $args " in
    *" -check "*)
        if cmp -s "$out/$name.got" "tests/$name.out"; then
            pass=$((pass + 1))
        else
            echo "FAIL $name: the compiler printed something else"
            diff "tests/$name.out" "$out/$name.got" | head -20
            fail=$((fail + 1))
        fi
        continue
        ;;
    esac
    if [ -s "$out/$name.got" ]; then
        echo "FAIL $name: compiles, but not in silence"
        cat "$out/$name.got"
        fail=$((fail + 1))
        continue
    fi
    if [ ! -f "tests/$name.gdb" ]; then
        within env FLAGC="$compiler" "$out/$name$exe" < /dev/null > "$out/$name.got" 2>&1
    elif [ -z "$target" ] && command -v gdb > /dev/null; then
        within gdb -batch -nx -x "tests/$name.gdb" "$out/$name" < /dev/null 2>&1 | sed -e /libthread_db/d -e 's/^Thread [0-9]* hit //' > "$out/$name.got"
    else
        continue # without gdb there is nothing to read the DWARF
    fi
    if cmp -s "$out/$name.got" "tests/$name.out"; then
        pass=$((pass + 1))
    else
        echo "FAIL $name: wrong output"
        diff "tests/$name.out" "$out/$name.got" | head -20
        fail=$((fail + 1))
    fi
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
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
