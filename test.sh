#!/bin/sh
compiler=${1:-build/flagc0}
out=build/tests
mkdir -p "$out"
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
for src in tests/*.flg tests/*/; do
    name=$(basename "$src" .flg)
    files=$src
    [ -d "$src" ] && files=$(ls -d "$src"*)
    args=
    [ -f "tests/$name.args" ] && args=$(cat "tests/$name.args")
    if [ -f "tests/$name.err" ]; then
        if within "$compiler" $files -o "$out/$name" $args > "$out/$name.got" 2>&1; then
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
        if ! within "$compiler" $files -o "$out/$name" -memory $args > "$out/$name.got" 2>&1; then
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
    if ! within "$compiler" $files -o "$out/$name" $args > "$out/$name.got" 2>&1; then
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
    within "$out/$name" < /dev/null > "$out/$name.got" 2>&1
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
    if within "$compiler" "$src" -check > "$out/example.got" 2>&1; then
        pass=$((pass + 1))
    else
        echo "FAIL $src: does not compile"
        cat "$out/example.got"
        fail=$((fail + 1))
    fi
done
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
