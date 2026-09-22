#include "strutil.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace poly {

static bool is_space_c(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

std::string ltrim(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && is_space_c(s[i])) i++;
    return s.substr(i);
}

std::string rtrim(const std::string& s) {
    size_t i = s.size();
    while (i > 0 && is_space_c(s[i - 1])) i--;
    return s.substr(0, i);
}

std::string trim(const std::string& s) { return ltrim(rtrim(s)); }

std::string to_lower(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

std::string to_upper(std::string s) {
    for (char& c : s)
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    return s;
}

bool starts_with(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

bool ends_with(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool contains(const std::string& s, const std::string& sub) {
    return s.find(sub) != std::string::npos;
}

std::vector<std::string> split(const std::string& s, char delim, bool keepEmpty) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == delim) {
            if (keepEmpty || !cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (keepEmpty || !cur.empty()) out.push_back(cur);
    return out;
}

std::vector<std::string> split_words(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (is_space_c(c)) {
            if (!cur.empty()) {
                out.push_back(cur);
                cur.clear();
            }
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

std::vector<std::string> split_lines(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == '\n') {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    out.push_back(cur);
    return out;
}

std::string join(const std::vector<std::string>& parts, const std::string& sep) {
    std::string out;
    for (size_t i = 0; i < parts.size(); i++) {
        if (i) out += sep;
        out += parts[i];
    }
    return out;
}

std::string replace_all(std::string s, const std::string& from, const std::string& to) {
    if (from.empty()) return s;
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

std::string fmt(const char* f, ...) {
    va_list ap;
    va_start(ap, f);
    va_list ap2;
    va_copy(ap2, ap);
    int n = vsnprintf(nullptr, 0, f, ap);
    va_end(ap);
    if (n < 0) {
        va_end(ap2);
        return std::string(f);
    }
    std::string buf(static_cast<size_t>(n), '\0');
    vsnprintf(&buf[0], static_cast<size_t>(n) + 1, f, ap2);
    va_end(ap2);
    return buf;
}

bool is_integer(const std::string& s) {
    if (s.empty()) return false;
    size_t i = (s[0] == '-' || s[0] == '+') ? 1 : 0;
    if (i >= s.size()) return false;
    for (; i < s.size(); i++)
        if (s[i] < '0' || s[i] > '9') return false;
    return true;
}

long long to_int(const std::string& s, long long def) {
    if (!is_integer(s)) return def;
    return strtoll(s.c_str(), nullptr, 10);
}

double to_double(const std::string& s, double def) {
    if (s.empty()) return def;
    char* end = nullptr;
    double v = strtod(s.c_str(), &end);
    if (end == s.c_str() || *end != '\0') return def;
    return v;
}

std::string lpad(const std::string& s, size_t n, char c) {
    if (s.size() >= n) return s;
    return std::string(n - s.size(), c) + s;
}

size_t display_width(const std::string& s) {
    size_t w = 0;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        unsigned cp = 0;
        size_t len = 1;
        if (c < 0x80) {
            cp = c;
        } else if ((c >> 5) == 0x6 && i + 1 < s.size()) {
            cp = ((c & 0x1Fu) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3Fu);
            len = 2;
        } else if ((c >> 4) == 0xE && i + 2 < s.size()) {
            cp = ((c & 0x0Fu) << 12) | ((static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 6) |
                 (static_cast<unsigned char>(s[i + 2]) & 0x3Fu);
            len = 3;
        } else if ((c >> 3) == 0x1E && i + 3 < s.size()) {
            cp = ((c & 0x07u) << 18) | ((static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 12) |
                 ((static_cast<unsigned char>(s[i + 2]) & 0x3Fu) << 6) |
                 (static_cast<unsigned char>(s[i + 3]) & 0x3Fu);
            len = 4;
        } else {
            len = 1;
        }
        i += len;
        bool wide = (cp >= 0x1100 && cp <= 0x115F) || (cp >= 0x2E80 && cp <= 0xA4CF) ||
                    (cp >= 0xAC00 && cp <= 0xD7A3) || (cp >= 0xF900 && cp <= 0xFAFF) ||
                    (cp >= 0xFE30 && cp <= 0xFE6F) || (cp >= 0xFF00 && cp <= 0xFF60) ||
                    (cp >= 0xFFE0 && cp <= 0xFFE6) || (cp >= 0x20000 && cp <= 0x3FFFD);
        w += wide ? 2 : 1;
    }
    return w;
}

std::string pad_display(const std::string& s, size_t width) {
    size_t w = display_width(s);
    if (w >= width) return s;
    return s + std::string(width - w, ' ');
}

std::string rpad(const std::string& s, size_t n, char c) {
    if (s.size() >= n) return s;
    return s + std::string(n - s.size(), c);
}

std::string num(long long v) { return fmt("%lld", v); }

std::string num_fixed(double v, int digits) { return fmt("%.*f", digits, v); }

std::string human_size(long long bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double v = static_cast<double>(bytes);
    int u = 0;
    while (v >= 1024.0 && u < 4) {
        v /= 1024.0;
        u++;
    }
    if (u == 0) return fmt("%lld B", bytes);
    return fmt("%.2f %s", v, units[u]);
}

std::string human_ms(double ms) {
    if (ms < 1000.0) return fmt("%.0f ms", ms);
    return fmt("%.3f s", ms / 1000.0);
}

std::string human_mem_kb(long long kb) {
    if (kb < 1024) return fmt("%lld KB", kb);
    return fmt("%.1f MB", static_cast<double>(kb) / 1024.0);
}

std::string escape_html(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out.push_back(c);
        }
    }
    return out;
}

std::string strip_cr(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s)
        if (c != '\r') out.push_back(c);
    return out;
}

bool has_suffix_ci(const std::string& s, const std::string& suffix) {
    return ends_with(to_lower(s), to_lower(suffix));
}

}  // namespace poly
