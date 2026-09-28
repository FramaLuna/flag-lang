#!/bin/sh
compiler=${1:-build/flagc0}
out=build/tests
mkdir -p "$out"
pass=0
fail=0
for src in tests/*.flg tests/*/; do
    name=$(basename "$src" .flg)
    files=$src
    [ -d "$src" ] && files=$(ls -d "$src"*)
    if [ -f "tests/$name.err" ]; then
        if "$compiler" $files -o "$out/$name" > "$out/$name.got" 2>&1; then
            echo "FAIL $name: compiled, expected an error"
            fail=$((fail + 1))
        elif grep -qF "$(cat "tests/$name.err")" "$out/$name.got"; then
            pass=$((pass + 1))
        else
            echo "FAIL $name: wrong error"
            cat "$out/$name.got"
            fail=$((fail + 1))
        fi
        continue
    fi
    if [ -f "tests/$name.warn" ]; then
        if ! "$compiler" $files -o "$out/$name" -memory > "$out/$name.got" 2>&1; then
            echo "FAIL $name: does not compile"
            cat "$out/$name.got"
            fail=$((fail + 1))
        elif [ -s "tests/$name.warn" ] && ! grep -qF "$(cat "tests/$name.warn")" "$out/$name.got"; then
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
    if ! "$compiler" $files -o "$out/$name" > "$out/$name.got" 2>&1; then
        echo "FAIL $name: does not compile"
        cat "$out/$name.got"
        fail=$((fail + 1))
        continue
    fi
    "$out/$name" < /dev/null > "$out/$name.got" 2>&1
    if cmp -s "$out/$name.got" "tests/$name.out"; then
        pass=$((pass + 1))
    else
        echo "FAIL $name: wrong output"
        diff "tests/$name.out" "$out/$name.got" | head -20
        fail=$((fail + 1))
    fi
done
for src in examples/*.flg; do
    if "$compiler" "$src" -check > "$out/example.got" 2>&1; then
        pass=$((pass + 1))
    else
        echo "FAIL $src: does not compile"
        cat "$out/example.got"
        fail=$((fail + 1))
    fi
done
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
