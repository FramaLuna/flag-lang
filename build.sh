#!/bin/sh
set -eu
cd "$(dirname "$0")"
version=0.7.0
cc=${CC:-cc}
mkdir -p build/embed
case $(uname -m) in aarch64|arm64) machine=arm64 ;; riscv64) machine=riscv64 ;; *) machine=x86_64 ;; esac
exe=
linking=
case $(uname) in
Darwin)
    system=macos
    linking="-Wl,-stack_size,0x1000000"
    section="__TEXT,__const"
    symbol=_
    ;;
MINGW*|MSYS*|CYGWIN*)
    system=windows
    exe=.exe
    linking="-Wl,--stack,16777216 -lws2_32"
    section='.rdata,"dr"'
    symbol=
    ;;
*)
    system=linux
    section=.rodata
    symbol=
    ;;
esac
target=$system-$machine
stamp=$(cat lib/* docs/*.md | cksum | cut -d' ' -f1)
commit=$(git rev-parse --short HEAD 2>/dev/null || echo unknown)
{
    echo ".section $section"
    i=0
    for f in lib/* docs/*.md; do
        echo ".globl ${symbol}flag_file_$i"
        echo ".globl ${symbol}flag_file_${i}_end"
        echo ".p2align 3"
        echo "${symbol}flag_file_$i:"
        echo ".incbin \"$f\""
        echo "${symbol}flag_file_${i}_end:"
        i=$((i + 1))
    done
} > build/embed/files.s
{
    echo "#include <stddef.h>"
    i=0
    for f in lib/* docs/*.md; do
        echo "extern const unsigned char flag_file_$i[], flag_file_${i}_end[];"
        i=$((i + 1))
    done
    echo "static const struct { const char *name; const unsigned char *start, *end; } files[] = {"
    i=0
    for f in lib/* docs/*.md; do
        echo "    {\"${f#lib/}\", flag_file_$i, flag_file_${i}_end},"
        i=$((i + 1))
    done
    echo "};"
    echo "const unsigned char *flag_embedded(size_t i, const char **name, size_t *size) {"
    echo "    if (i >= sizeof files / sizeof files[0]) return 0;"
    echo "    *name = files[i].name;"
    echo "    *size = (size_t)(files[i].end - files[i].start);"
    echo "    return files[i].start;"
    echo "}"
    echo "const char *flag_version(void) { return \"$version ($commit)\"; }"
    echo "const char *flag_stamp(void) { return \"$version-$stamp\"; }"
    echo "const char *flag_target(void) { return \"$target\"; }"
} > build/embed/index.c
$cc -c build/embed/files.s -o build/embed/files.o
$cc -O2 -c build/embed/index.c -o build/embed/index.o
objects="build/embed/files.o build/embed/index.o"
gzip -dc boot/$target.s.gz > build/seed.s
$cc -o build/flagc0 build/seed.s $objects $linking
build/flagc0 src -o build/flagc1 -cc $objects $linking
build/flagc1 src -o build/flagc2 -keep -cc $objects $linking
build/flagc2 src -o build/flagc -keep -cc $objects $linking
cmp build/flagc2.s build/flagc.s
# a seed is the assembly of the compiler for one target, rewritten only when it changes, as gzip may differ by system
if [ "${1:-}" = seed ]; then
    for seed in linux-x86_64 linux-arm64 linux-riscv64 macos-x86_64 macos-arm64 windows-x86_64; do
        CC=true build/flagc src -target $seed -o build/$seed -keep
        gzip -dc boot/$seed.s.gz 2> /dev/null | cmp -s - build/$seed.s || gzip -9n < build/$seed.s > boot/$seed.s.gz
    done
fi
if [ "${1:-}" = cross ]; then
    case $system in
    linux) targets="linux-arm64=aarch64-linux-gnu linux-riscv64=riscv64-linux-gnu" ;;
    macos) targets="macos-x86_64=x86_64-apple-macos11" ;;
    *) targets= ;;
    esac
    for pair in $targets; do
        target=${pair%=*}
        out=build/cross/$target
        rm -rf "$out"
        mkdir -p "$out"
        for source in src tests/*.out tests/*.warn examples/*.flg; do
            name=$(basename "${source%.*}")
            files=$source
            flags=
            case $source in
            src) files=src ;;
            tests/*)
                files=tests/$name.flg
                [ -d "tests/$name" ] && files=tests/$name
                [ -f "tests/$name.args" ] && flags=$(cat "tests/$name.args")
                [ -f "tests/$name.env" ] && continue
                ;;
            esac
            case " $flags " in
            *" -check "*|*" -o "*|*" -target "*|*" -overlay "*|*" -timeout "*|*" -at "*|*" tests/"*) continue ;;
            esac
            CC=true build/flagc $files -target "$target" -o "$out/$name" -keep $flags > "$out/$name.log" 2>&1 || { echo "cross: $source does not compile for $target"; cat "$out/$name.log"; exit 1; }
            clang --target="${pair#*=}" -c "$out/$name.s" -o "$out/$name.o" || { echo "cross: $source does not assemble for $target"; exit 1; }
        done
        echo "cross: flagc, the tests and the examples compile and assemble for $target"
    done
fi
if [ "${1:-}" = package ]; then
    name=flag-$version-$target
    rm -rf "build/$name"
    mkdir -p "build/$name/bin"
    cp build/flagc$exe "build/$name/bin/"
    cp README.md LICENSE LICENSE.qbe lib/flag-gdb.py "build/$name/"
    cp lib/raylib.license "build/$name/LICENSE.raylib"
    tar -czf "build/$name.tar.gz" -C build "$name"
    echo "packaged build/$name.tar.gz"
fi
echo "flagc: self-hosted, stage 2 and stage 3 agree"
