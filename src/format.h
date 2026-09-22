#pragma once

#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

#include "strutil.h"

namespace poly {

namespace fmt_detail {

inline std::string text(const std::string& v) { return v; }
inline std::string text(std::string& v) { return v; }
inline std::string text(const char* v) { return v ? std::string(v) : std::string(); }
inline std::string text(char* v) { return v ? std::string(v) : std::string(); }
inline std::string text(char v) { return std::string(1, v); }
inline std::string text(bool v) { return v ? "true" : "false"; }
inline std::string text(std::nullptr_t) { return "null"; }

inline std::string text(signed char v) { return num(static_cast<long long>(v)); }
inline std::string text(short v) { return num(static_cast<long long>(v)); }
inline std::string text(int v) { return num(static_cast<long long>(v)); }
inline std::string text(long v) { return num(static_cast<long long>(v)); }
inline std::string text(long long v) { return num(v); }
inline std::string text(unsigned char v) { return num(static_cast<long long>(v)); }
inline std::string text(unsigned short v) { return num(static_cast<long long>(v)); }
inline std::string text(unsigned int v) { return num(static_cast<long long>(v)); }
inline std::string text(unsigned long v) { return num(static_cast<long long>(v)); }
inline std::string text(unsigned long long v) { return num(static_cast<long long>(v)); }
inline std::string text(float v) {
    std::string s = num_fixed(v, 6);
    while (s.size() > 1 && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    return s;
}
inline std::string text(double v) { return text(static_cast<float>(v)); }

inline void collect(std::vector<std::string>&) {}

template <typename T, typename... Rest>
void collect(std::vector<std::string>& out, T&& v, Rest&&... rest) {
    out.push_back(fmt_detail::text(std::forward<T>(v)));
    collect(out, std::forward<Rest>(rest)...);
}

}  // namespace fmt_detail

// Minimal "{}" substitution formatter.  "{{" and "}}" produce literal braces.
template <typename... Args>
std::string str(const std::string& f, Args&&... args) {
    std::vector<std::string> vals;
    vals.reserve(sizeof...(Args));
    fmt_detail::collect(vals, std::forward<Args>(args)...);
    std::string out;
    out.reserve(f.size() + 16 * vals.size());
    size_t vi = 0;
    for (size_t i = 0; i < f.size();) {
        if (f[i] == '{' && i + 1 < f.size() && f[i + 1] == '}') {
            out += vi < vals.size() ? vals[vi++] : std::string("{}");
            i += 2;
        } else if (f[i] == '{' && i + 1 < f.size() && f[i + 1] == '{') {
            out += '{';
            i += 2;
        } else if (f[i] == '}' && i + 1 < f.size() && f[i + 1] == '}') {
            out += '}';
            i += 2;
        } else {
            out += f[i++];
        }
    }
    return out;
}

inline std::string str(const std::string& f) { return f; }
inline std::string str(const char* f) { return f ? std::string(f) : std::string(); }

}  // namespace poly
