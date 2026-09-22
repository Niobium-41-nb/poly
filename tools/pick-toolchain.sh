#!/bin/sh
# Prints the absolute path of a C++ compiler that actually compiles a smoke
# test, or nothing when the compiler already on PATH is fine... in which case
# the path of that compiler is printed too.  Nothing is printed when no
# candidate works.
#
# Two Windows/MSYS2 pitfalls are handled here:
#   1. native binaries need TMP/TEMP to point at a real Windows directory,
#      otherwise the assembler falls back to an unwritable C:\WINDOWS;
#   2. MSYS2 ships several prefixes (ucrt64 / mingw64 / clang64) whose runtime
#      DLLs are mutually incompatible - and a stale PATH entry may point at a
#      compiler that does not exist at all, which make only reports as a
#      silent failure.

if command -v cygpath >/dev/null 2>&1; then
    _win_tmp=$(cygpath -m /tmp 2>/dev/null)
elif [ -d /tmp ]; then
    _win_tmp=$( (cd /tmp && pwd -W) 2>/dev/null | sed 's#\\#/#g')
fi
if [ -n "$_win_tmp" ]; then
    TMP="$_win_tmp"
    TEMP="$_win_tmp"
    export TMP TEMP
fi

_work="${TMPDIR:-/tmp}/polytc.$$"
[ -d "$_work" ] || mkdir -p "$_work" 2>/dev/null || exit 0
printf 'int main(){return 0;}\n' > "$_work/t.cpp" 2>/dev/null || exit 0

_try() {
    _cc=$1
    _dir=$(dirname "$_cc")
    rm -f "$_work/t.o"
    PATH="$_dir:$PATH" "$_cc" -std=c++17 -c "$_work/t.cpp" -o "$_work/t.o" >/dev/null 2>&1
    [ -f "$_work/t.o" ]
}

_finish() {
    rm -rf "$_work"
    exit 0
}

_candidates=""
_add() {
    [ -x "$1" ] || return 0
    case " $_candidates " in
        *" $1 "*) return 0 ;;
    esac
    _candidates="$_candidates $1"
}

_add "$(command -v g++ 2>/dev/null)"
_add "$(command -v clang++ 2>/dev/null)"
for _p in /c/msys64 /d/msys64 /c/msys2 /d/msys2 "$HOME/msys64"; do
    [ -d "$_p" ] || continue
    for _pfx in ucrt64 mingw64 clang64 clangarm64; do
        _add "$_p/$_pfx/bin/g++"
        _add "$_p/$_pfx/bin/clang++"
    done
done
for _p in /ucrt64 /mingw64 /clang64; do
    _add "$_p/bin/g++"
    _add "$_p/bin/clang++"
done

_oldifs=$IFS
IFS=:
for _d in $PATH; do
    IFS=$_oldifs
    _add "$_d/g++"
    _add "$_d/clang++"
    IFS=:
done
IFS=$_oldifs

for _cc in $_candidates; do
    if _try "$_cc"; then
        echo "$_cc"
        _finish
    fi
done

_finish
