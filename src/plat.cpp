#include "plat.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "strutil.h"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>

#include <shellapi.h>
#endif

namespace poly {

#ifdef _WIN32

std::wstring to_wide(const std::string& utf8) {
    if (utf8.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
    if (n <= 0) {
        // fall back to the active code page
        n = MultiByteToWideChar(CP_ACP, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
        if (n <= 0) return std::wstring();
        std::wstring out(static_cast<size_t>(n), L'\0');
        MultiByteToWideChar(CP_ACP, 0, utf8.c_str(), static_cast<int>(utf8.size()), &out[0], n);
        return out;
    }
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), &out[0], n);
    return out;
}

std::string to_utf8(const std::wstring& w) {
    if (w.empty()) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return std::string();
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), &out[0], n, nullptr, nullptr);
    return out;
}

std::string to_utf8_acp(const std::string& local) {
    return to_utf8(to_wide(local));
}

static std::wstring widen_any(const std::string& s) { return to_wide(s); }

std::vector<std::string> get_args_utf8(int argc, char** argv) {
    (void)argc;
    (void)argv;
    std::vector<std::string> out;
    int wargc = 0;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    if (!wargv) return out;
    for (int i = 0; i < wargc; i++) out.push_back(to_utf8(wargv[i]));
    LocalFree(wargv);
    return out;
}

void init_console() {
    // 1 ms system timer: GetProcessTimes() is quantised to the timer tick
    // (15.6 ms by default), which makes CPU measurements useless.
    {
        typedef unsigned int(__stdcall * time_begin_period_t)(unsigned int);
        HMODULE winmm = LoadLibraryW(L"winmm.dll");
        if (winmm) {
            time_begin_period_t fn = reinterpret_cast<time_begin_period_t>(
                reinterpret_cast<void*>(GetProcAddress(winmm, "timeBeginPeriod")));
            if (fn) fn(1);
        }
    }
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h && h != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(h, &mode)) SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
}

bool stdout_is_tty() {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!h || h == INVALID_HANDLE_VALUE) return false;
    DWORD mode = 0;
    return GetConsoleMode(h, &mode) != 0;
}

#else  // !_WIN32

#include <unistd.h>

std::vector<std::string> get_args_utf8(int argc, char** argv) {
    std::vector<std::string> out;
    for (int i = 0; i < argc; i++) out.push_back(argv[i]);
    return out;
}

void init_console() {}

bool stdout_is_tty() { return isatty(1) != 0; }

#endif

std::string quote_arg(const std::string& a) {
    if (!a.empty() && a.find_first_of(" \t\n\v\"") == std::string::npos) return a;
    std::string out = "\"";
    size_t backslashes = 0;
    for (char c : a) {
        if (c == '\\') {
            backslashes++;
            continue;
        }
        if (c == '"') {
            out.append(backslashes * 2 + 1, '\\');
            backslashes = 0;
            out.push_back('"');
            continue;
        }
        out.append(backslashes, '\\');
        backslashes = 0;
        out.push_back(c);
    }
    out.append(backslashes * 2, '\\');
    out.push_back('"');
    return out;
}

std::string join_args(const std::vector<std::string>& args) {
    std::string out;
    for (size_t i = 0; i < args.size(); i++) {
        if (i) out.push_back(' ');
        out += quote_arg(args[i]);
    }
    return out;
}

std::string env_get(const std::string& name) {
    const char* v = getenv(name.c_str());
    return v ? std::string(v) : std::string();
}

void env_set(const std::string& name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name.c_str(), value.c_str());
#else
    setenv(name.c_str(), value.c_str(), 1);
#endif
}

}  // namespace poly
