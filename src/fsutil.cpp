#include "fsutil.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <random>
#include <system_error>

#include <filesystem>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include "plat.h"
#include "strutil.h"

namespace stdfs = std::filesystem;

namespace poly {
namespace fs {

#ifdef _WIN32
static stdfs::path to_path(const std::string& utf8) { return stdfs::path(to_wide(utf8)); }
static std::string from_path(const stdfs::path& p) { return to_utf8(p.wstring()); }
#else
static stdfs::path to_path(const std::string& utf8) { return stdfs::path(utf8); }
static std::string from_path(const stdfs::path& p) { return p.string(); }
#endif

std::string separator() { return std::string(1, static_cast<char>(stdfs::path::preferred_separator)); }

std::string join(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    stdfs::path pa = to_path(a);
    return from_path(pa / to_path(b));
}

std::string join(const std::string& a, const std::string& b, const std::string& c) {
    return join(join(a, b), c);
}

std::string basename(const std::string& p) {
    if (p.empty()) return p;
    return from_path(to_path(p).filename());
}

std::string dirname(const std::string& p) {
    if (p.empty()) return p;
    std::string d = from_path(to_path(p).parent_path());
    return d.empty() ? std::string(".") : d;
}

std::string stem(const std::string& p) { return from_path(to_path(p).stem()); }

std::string extension(const std::string& p) { return from_path(to_path(p).extension()); }

std::string normalize(const std::string& p) {
    stdfs::path path = to_path(p);
    std::string out = from_path(path.lexically_normal());
    if (out.size() > 1 && (out.back() == '/' || out.back() == '\\')) out.pop_back();
    return out.empty() ? std::string(".") : out;
}

std::string absolute(const std::string& p, const std::string& base) {
    stdfs::path path = to_path(p);
    if (path.is_absolute()) return from_path(path.lexically_normal());
    stdfs::path root = base.empty() ? stdfs::current_path() : to_path(base);
    return from_path((root / path).lexically_normal());
}

bool is_absolute(const std::string& p) { return to_path(p).is_absolute(); }

std::string relative_to(const std::string& p, const std::string& base) {
    std::error_code ec;
    stdfs::path r = stdfs::relative(to_path(p), to_path(base), ec);
    if (ec) return p;
    return from_path(r);
}

bool exists(const std::string& p) {
    std::error_code ec;
    return stdfs::exists(to_path(p), ec);
}

bool is_file(const std::string& p) {
    std::error_code ec;
    return stdfs::is_regular_file(to_path(p), ec);
}

bool is_dir(const std::string& p) {
    std::error_code ec;
    return stdfs::is_directory(to_path(p), ec);
}

bool read_file(const std::string& p, std::string& out) {
    std::ifstream in(to_path(p), std::ios::binary);
    if (!in) return false;
    std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    out.swap(data);
    return true;
}

std::string read_file(const std::string& p) {
    std::string s;
    read_file(p, s);
    return s;
}

bool write_file(const std::string& p, const std::string& data, bool createParents) {
    if (createParents) {
        std::string d = dirname(p);
        if (!d.empty() && d != "." && !is_dir(d)) mkdirs(d);
    }
    std::ofstream out(to_path(p), std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    return out.good() || out.eof();
}

bool append_file(const std::string& p, const std::string& data) {
    std::ofstream out(to_path(p), std::ios::binary | std::ios::app);
    if (!out) return false;
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    return true;
}

bool mkdirs(const std::string& p) {
    std::error_code ec;
    stdfs::create_directories(to_path(p), ec);
    return is_dir(p);
}

bool remove_file(const std::string& p) {
    std::error_code ec;
    return stdfs::remove(to_path(p), ec);
}

bool remove_all(const std::string& p) {
    std::error_code ec;
    if (!stdfs::exists(to_path(p), ec)) return true;
    stdfs::remove_all(to_path(p), ec);
    return !stdfs::exists(to_path(p), ec);
}

bool copy_file(const std::string& from, const std::string& to, bool createParents) {
    if (createParents) {
        std::string d = dirname(to);
        if (!d.empty() && d != "." && !is_dir(d)) mkdirs(d);
    }
    std::error_code ec;
    stdfs::copy_file(to_path(from), to_path(to), stdfs::copy_options::overwrite_existing, ec);
    return !ec;
}

bool copy_tree(const std::string& from, const std::string& to) {
    std::error_code ec;
    stdfs::copy(to_path(from), to_path(to), stdfs::copy_options::recursive | stdfs::copy_options::overwrite_existing, ec);
    return !ec;
}

bool rename_path(const std::string& from, const std::string& to) {
    std::error_code ec;
    stdfs::rename(to_path(from), to_path(to), ec);
    return !ec;
}

std::vector<std::string> list_dir(const std::string& p, bool filesOnly) {
    std::vector<std::string> out;
    std::error_code ec;
    for (stdfs::directory_iterator it(to_path(p), ec), end; !ec && it != end; it.increment(ec)) {
        const stdfs::directory_entry& e = *it;
        std::error_code ec2;
        bool isDir = e.is_directory(ec2);
        if (filesOnly && isDir) continue;
        out.push_back(from_path(e.path()));
    }
    return out;
}

std::vector<std::string> list_dir_sorted(const std::string& p, bool filesOnly) {
    std::vector<std::string> out = list_dir(p, filesOnly);
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<std::string> list_files_recursive(const std::string& p) {
    std::vector<std::string> out;
    std::error_code ec;
    for (stdfs::recursive_directory_iterator it(to_path(p), ec), end; !ec && it != end; it.increment(ec)) {
        std::error_code ec2;
        if (it->is_regular_file(ec2)) out.push_back(from_path(it->path()));
    }
    std::sort(out.begin(), out.end());
    return out;
}

long long file_size(const std::string& p) {
    std::error_code ec;
    auto n = stdfs::file_size(to_path(p), ec);
    return ec ? -1 : static_cast<long long>(n);
}

long long modified_time(const std::string& p) {
    std::error_code ec;
    auto t = stdfs::last_write_time(to_path(p), ec);
    if (ec) return -1;
    return static_cast<long long>(t.time_since_epoch().count());
}

std::string cwd() { return from_path(stdfs::current_path()); }

std::string temp_dir() {
    std::error_code ec;
    auto t = stdfs::temp_directory_path(ec);
    if (ec) return cwd();
    return from_path(t);
}

std::string unique_temp_path(const std::string& prefix) {
    static std::mt19937_64 rng(static_cast<unsigned long long>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    return join(temp_dir(), fmt("%s%016llx", prefix.c_str(), static_cast<unsigned long long>(rng())));
}

std::string exe_path() {
#ifdef _WIN32
    wchar_t buf[4096];
    DWORD n = GetModuleFileNameW(nullptr, buf, 4096);
    if (n > 0 && n < 4096) return to_utf8(std::wstring(buf, n));
#endif
    return cwd();
}

std::string exe_dir() { return dirname(exe_path()); }

std::string home_dir() {
    std::string h = env_get("USERPROFILE");
    if (h.empty()) h = env_get("HOME");
    if (h.empty()) return cwd();
    return h;
}

std::string resolve_executable(const std::string& name) {
    if (name.empty()) return std::string();
    if (contains(name, "/") || contains(name, "\\")) return is_file(name) ? name : std::string();
    std::string path = env_get("PATH");
    char sep = path.find(';') != std::string::npos ? ';' : ':';
    std::vector<std::string> suffixes;
    suffixes.push_back("");
#ifdef _WIN32
    std::string lower = to_lower(name);
    if (!ends_with(lower, ".exe")) suffixes.push_back(".exe");
    if (!ends_with(lower, ".bat")) suffixes.push_back(".bat");
#endif
    for (std::string d : split(path, sep)) {
        while (!d.empty() && (d.back() == '/' || d.back() == '\\')) d.pop_back();
        if (d.empty()) continue;
        for (const std::string& sfx : suffixes) {
            std::string cand = join(d, name + sfx);
            if (is_file(cand)) return cand;
        }
    }
    return std::string();
}

}  // namespace fs
}  // namespace poly
