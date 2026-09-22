#pragma once

#include <string>
#include <vector>

namespace poly {

struct Compiler {
    std::string exe;    // path (absolute when resolvable)
    std::string binDir; // directory to put in front of PATH
    std::string version;
    std::string stdFlag = "-std=c++17";
    std::vector<std::string> flags;
    bool ok = false;
};

// Finds a C++ compiler that provably works.  `want` may be empty, a bare name
// ("g++") or a path.  On Windows/MSYS2 several prefixes may be installed whose
// runtime DLLs are mutually incompatible, so every candidate is smoke tested.
bool resolve_compiler(const std::string& want, const std::string& stdFlag, Compiler& out, std::string& err);

// A temp directory that native toolchain binaries can actually use: existing,
// writable and ASCII-only (binutils uses the ANSI file API).
std::string ascii_temp_dir();

std::string exe_suffix();

// Compiles workDir/srcRel into workDir/outRel.  Paths must be relative and
// ASCII: that is what keeps everything working when the workspace itself sits
// in a directory with non-ASCII characters.
bool compile_to(const Compiler& cc, const std::string& workDir, const std::string& srcRel,
                const std::string& outRel, const std::vector<std::string>& extraArgs, std::string& log,
                std::string& err);

std::string toolchain_hint();

}  // namespace poly
