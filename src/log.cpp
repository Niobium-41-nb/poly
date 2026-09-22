#include "log.h"

#include <cstdio>
#include <string>

#include "format.h"
#include "plat.h"
#include "strutil.h"

namespace poly {

namespace {

bool g_color = true;
bool g_verbose = false;
bool g_quiet = false;
std::string* g_capture = nullptr;

const char* kReset = "\033[0m";
const char* kRed = "\033[31m";
const char* kGreen = "\033[32m";
const char* kYellow = "\033[33m";
const char* kBlue = "\033[34m";
const char* kCyan = "\033[36m";
const char* kBold = "\033[1m";
const char* kDim = "\033[2m";

const char* c(const char* code) { return g_color ? code : ""; }

// 捕获时去掉 ANSI 转义序列，其余原样追加。
void append_capture(std::string& sink, const std::string& s) {
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\033' && i + 1 < s.size() && s[i + 1] == '[') {
            size_t j = i + 2;
            while (j < s.size() && !((s[j] >= 'a' && s[j] <= 'z') || (s[j] >= 'A' && s[j] <= 'Z')))
                j++;
            i = j;
            continue;
        }
        sink.push_back(s[i]);
    }
}

void emit(const std::string& s) {
    if (g_capture) append_capture(*g_capture, s);
    std::fwrite(s.data(), 1, s.size(), stdout);
    std::fflush(stdout);
}

}  // namespace

void log_configure(bool colorEnabled, bool verbose, bool quiet) {
    g_color = colorEnabled;
    g_verbose = verbose;
    g_quiet = quiet;
}

void log_set_capture(std::string* sink) { g_capture = sink; }

bool log_color_enabled() { return g_color; }
bool log_is_verbose() { return g_verbose; }
bool log_is_quiet() { return g_quiet; }

void log_raw(const std::string& s) { emit(s); }

void log_info(const std::string& s) {
    if (g_quiet) return;
    emit(s + "\n");
}

void log_ok(const std::string& s) {
    if (g_quiet) return;
    emit(std::string(c(kGreen)) + "[ OK ]  " + c(kReset) + s + "\n");
}

void log_warn(const std::string& s) {
    if (g_quiet) return;
    emit(std::string(c(kYellow)) + "[WARN]  " + c(kReset) + s + "\n");
}

void log_err(const std::string& s) {
    std::string line = std::string(c(kRed)) + "[FAIL]  " + c(kReset) + s + "\n";
    if (g_capture) append_capture(*g_capture, line);
    std::fwrite(line.data(), 1, line.size(), stderr);
    std::fflush(stderr);
}

void log_step(const std::string& s) {
    if (g_quiet) return;
    emit(std::string(c(kCyan)) + "==> " + c(kReset) + c(kBold) + s + c(kReset) + "\n");
}

void log_debug(const std::string& s) {
    if (!g_verbose) return;
    emit(std::string(c(kDim)) + "  ... " + s + c(kReset) + "\n");
}

int verdict_color_code(const std::string& verdict) {
    if (verdict == "OK" || verdict == "AC") return 32;
    if (verdict == "WA" || verdict == "PE") return 31;
    if (verdict == "TL" || verdict == "ML") return 33;
    if (verdict == "RE" || verdict == "CE" || verdict == "FAIL") return 35;
    return 36;
}

std::string colorize_verdict(const std::string& verdict) {
    if (!g_color) return verdict;
    return str("\033[{}m{}\033[0m", verdict_color_code(verdict), verdict);
}

}  // namespace poly
