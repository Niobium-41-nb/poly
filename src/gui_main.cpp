// poly-gui：出题工作台的窗口版入口。
//
// 与命令行的 bin/poly.exe 是同一份代码，只是用 GUI 子系统链接（-mwindows），
// 因此双击运行不会弹出控制台窗口；参数与 poly 完全一致（常用：-w <工作区>）。
#include <cstdio>
#include <string>
#include <vector>

#ifndef _WIN32
#include <cstdio>
int main() {
    std::fprintf(stderr, "poly-gui 只在 Windows 上可用（poly ui --web 可打开网页版工作台）\n");
    return 1;
}
#else

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <windows.h>

#include "cli.h"
#include "commands.h"
#include "format.h"
#include "fsutil.h"
#include "log.h"
#include "plat.h"
#include "version.h"
#include "win32ui.h"

namespace {

// 标准输出可用吗？（在控制台里运行、或被重定向到文件时都算）
bool stdout_usable() {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!h || h == INVALID_HANDLE_VALUE) return false;
    return GetFileType(h) != FILE_TYPE_UNKNOWN;
}

// 没有可用 stdout 时（双击运行），挂到父进程的控制台上再输出。
// 注意不要无条件重定向到 CONOUT$：那样会把 `poly-gui --version > f.txt` 的重定向弄丢。
bool ensure_stdout() {
    if (stdout_usable()) return true;
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) return false;
    if (!std::freopen("CONOUT$", "w", stdout)) {
        FreeConsole();
        return false;
    }
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    return true;
}

void show_info_box(const std::string& text) {
    MessageBoxW(nullptr, poly::to_wide(text).c_str(), L"poly-gui", MB_OK | MB_ICONINFORMATION);
}

}  // namespace

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    poly::init_console();
    poly::log_configure(false, false, false);

    std::vector<std::string> raw = poly::get_args_utf8(0, nullptr);

    // `poly-gui --version` / `poly-gui --help` 也要像命令行那样回答，
    // 而不是闷头把界面窗口打开（否则脚本里 `poly-gui --version` 会卡出一个窗口）。
    if (raw.size() > 1) {
        bool want_version = false;
        bool want_help = false;
        const std::string& first = raw[1];
        if (first == "--version" || first == "-version" || first == "version") want_version = true;
        else if (first == "--help" || first == "-help" || first == "-h" || first == "/?" || first == "help") want_help = true;
        for (size_t i = 2; i < raw.size() && !(want_version && want_help); i++) {
            if (raw[i] == "--version") want_version = true;
            else if (raw[i] == "--help") want_help = true;
        }
        if (want_version || want_help) {
            if (ensure_stdout()) {
                if (want_version) {
                    std::printf("poly %s\n", POLY_VERSION);
                } else {
                    poly::Args hargs;
                    hargs.command = "help";
                    poly::Context hctx;
                    poly::cmd_help(hctx, hargs);
                }
                std::fflush(stdout);
            } else if (want_version) {
                show_info_box(std::string("poly ") + POLY_VERSION + "\n");
            } else {
                show_info_box("poly-gui 是窗口版，没有控制台可供输出帮助。\n\n"
                              "请在命令行里运行 poly --help 查看完整帮助，"
                              "或直接使用窗口界面。\n");
            }
            return 0;
        }
    }

    poly::Args args;
    std::string err;
    if (raw.size() > 1) {
        // parse_args 把第 0 个 token 当命令名，而窗口版只接受选项（-w 等）：
        // 不补一个占位命令的话，"-w <目录>" 会被当成命令吞掉（曾导致 -w 完全不生效）。
        std::vector<std::string> rest(raw.begin() + 1, raw.end());
        rest.insert(rest.begin(), "ui");
        if (!poly::parse_args(rest, args, err)) {
            MessageBoxW(nullptr, poly::to_wide("参数错误：" + err).c_str(), L"poly-gui", MB_OK | MB_ICONERROR);
            return 2;
        }
    }
    // 工作区优先级：-w / POLY_WORKSPACE > 上次在界面里选过的目录 > 「文档\poly」。
    // 默认值不放在安装目录下：卸载会把安装目录整个删掉，用户的题目会跟着消失。
    if (args.get("workspace").empty() && poly::env_get("POLY_WORKSPACE").empty()) {
        std::string ws = poly::ui_saved_workspace();
        if (ws.empty()) {
            std::string docs = poly::documents_dir();
            if (!docs.empty()) ws = poly::fs::join(docs, "poly");
        }
        if (!ws.empty()) args.values["workspace"].push_back(ws);
    }
    poly::Context ctx;
    poly::resolve_context(args, ctx, err);
    return poly::run_win32_ui(ctx);
}

#endif  // _WIN32
