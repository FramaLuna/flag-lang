#!/bin/sh
set -eu
cd "$(dirname "$0")"
version=0.1.0
cc=${CC:-cc}
mkdir -p build/objects build/embed
case $(uname -m) in aarch64|arm64) machine=arm64 ;; riscv64) machine=riscv64 ;; *) machine=x86_64 ;; esac
exe=
case $(uname) in
Darwin)
    system=macos
    case $machine in arm64) target=T_arm64_apple ;; *) target=T_amd64_apple ;; esac
    section="__TEXT,__const"
    symbol=_
    ;;
MINGW*|MSYS*|CYGWIN*)
    system=windows
    exe=.exe
    target=T_amd64_win
    section='.rdata,"dr"'
    symbol=
    ;;
*)
    system=linux
    case $machine in arm64) target=T_arm64 ;; riscv64) target=T_rv64 ;; *) target=T_amd64_sysv ;; esac
    section=.rodata
    symbol=
    ;;
esac
echo "#define Deftgt $target" > build/objects/config.new
cmp -s build/objects/config.new build/objects/config.h || mv build/objects/config.new build/objects/config.h
objects=""
for src in vendor/qbe/*.c vendor/qbe/*/*.c; do
    obj=build/objects/$(echo "${src#vendor/qbe/}" | tr / _ | sed 's/\.c$/.o/')
    [ "$obj" -nt "$src" ] && [ "$obj" -nt build/objects/config.h ] || $cc -std=c99 -O2 -w -Ibuild/objects -Dmain=qbe_main -c "$src" -o "$obj"
    objects="$objects $obj"
done
printf 'int qbe_main(int, char **);\nint main(int argc, char **argv) { return qbe_main(argc, argv); }\n' > build/embed/qbe.c
$cc -o build/qbe build/embed/qbe.c $objects
stamp=$(cat lib/* | cksum | cut -d' ' -f1)
commit=$(git rev-parse --short HEAD 2>/dev/null || echo unknown)
{
    echo ".section $section"
    i=0
    for f in lib/*; do
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
    for f in lib/*; do
        echo "extern const unsigned char flag_file_$i[], flag_file_${i}_end[];"
        i=$((i + 1))
    done
    echo "static const struct { const char *name; const unsigned char *start, *end; } files[] = {"
    i=0
    for f in lib/*; do
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
    echo "const char *flag_target(void) { return \"$system-$machine\"; }"
} > build/embed/index.c
$cc -c build/embed/files.s -o build/embed/files.o
$cc -O2 -c build/embed/index.c -o build/embed/index.o
objects="$objects build/embed/files.o build/embed/index.o"
build/qbe -o build/seed.s boot/$system.ssa
$cc -o build/flagc0 build/seed.s $objects
build/flagc0 src/*.flg $objects -o build/flagc1
build/flagc1 src/*.flg $objects -o build/flagc2 -keep
build/flagc2 src/*.flg $objects -o build/flagc -keep
cmp build/flagc2.ssa build/flagc.ssa
if [ "${1:-}" = seed ]; then
    for target in linux-x86_64 macos-arm64 windows-x86_64; do
        CC=true build/flagc src/*.flg -target $target -o build/${target%-*} -keep
        cp build/${target%-*}.ssa boot/${target%-*}.ssa
    done
elif ! cmp -s build/flagc.ssa boot/$system.ssa; then
    echo "boot/$system.ssa is older than src: ./build.sh seed refreshes it"
fi
if [ "${1:-}" = package ]; then
    name=flag-$version-$system-$machine
    rm -rf "build/$name"
    mkdir -p "build/$name/bin"
    cp build/flagc$exe "build/$name/bin/"
    cp README.md LICENSE "build/$name/"
    cp vendor/qbe/LICENSE "build/$name/LICENSE.qbe"
    cp lib/raylib.license "build/$name/LICENSE.raylib"
    tar -czf "build/$name.tar.gz" -C build "$name"
    echo "packaged build/$name.tar.gz"
fi
echo "flagc: self-hosted, stage 2 and stage 3 agree"
