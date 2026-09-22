#include "testlib_source.h"

#include "fsutil.h"
#include "plat.h"

#if __has_include("testlib_embed.inc")
#include "testlib_embed.inc"
#define POLY_HAS_EMBEDDED_TESTLIB 1
#else
#define POLY_HAS_EMBEDDED_TESTLIB 0
#endif

namespace poly {

namespace {

std::vector<std::string> candidate_paths() {
    std::vector<std::string> out;
    std::string exeDir = fs::exe_dir();
    out.push_back(fs::join(exeDir, "testlib", "testlib.h"));
    out.push_back(fs::join(fs::dirname(exeDir), "testlib", "testlib.h"));
    std::string home = env_get("POLY_HOME");
    if (!home.empty()) out.push_back(fs::join(home, "testlib", "testlib.h"));
    out.push_back(fs::join(fs::cwd(), "testlib", "testlib.h"));
    return out;
}

std::string find_on_disk() {
    for (const std::string& p : candidate_paths()) {
        if (fs::is_file(p)) return p;
    }
    return std::string();
}

}  // namespace

std::string testlib_disk_path() { return find_on_disk(); }

const std::string& testlib_source() {
    static std::string cached;
    if (!cached.empty()) return cached;
#if POLY_HAS_EMBEDDED_TESTLIB
    const char* embedded = embedded_testlib_source();
    if (embedded && embedded[0]) cached = embedded;
#endif
    if (cached.empty()) {
        std::string p = find_on_disk();
        if (!p.empty()) fs::read_file(p, cached);
    }
    return cached;
}

}  // namespace poly
