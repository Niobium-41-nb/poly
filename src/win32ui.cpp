// 出题工作台（Win32 原生窗口版）。
//
// 纯 Win32 API 实现，不用 MFC/Qt：主窗口 = 工具栏按钮 + 题目列表(ListView)
// + 右侧操作面板(按钮/下拉框/编辑框) + 底部日志框(只读 EDIT) + 状态栏，
// 另有「导入题目」窗口（四张卡片：QDUOJ / FPS / Hydro / HOJ）。
//
// 所有耗时操作（编译、造数据、评测、对拍、导入、导出）都放到工作线程里跑，
// 通过自定义消息把日志送回界面线程，因此窗口不会假死。
// 具体执行逻辑在 ops.cpp / importer.cpp，与命令行、网页版完全一致。
#include "win32ui.h"

#ifndef _WIN32

#include "log.h"

namespace poly {
int run_win32_ui(Context& ctx) {
    (void)ctx;
    log_err("原生窗口界面只在 Windows 上可用（可用 poly ui --web 打开网页版工作台）");
    return 1;
}
}  // namespace poly

#else

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601  // SetProcessDPIAware 需要 >= Windows 7
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
// windows.h 必须在其它 Win32 头文件之前（commctrl/shellapi 都依赖它的基础类型）
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>

#include <algorithm>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>

#include "exporter.h"
#include "format.h"
#include "fsutil.h"
#include "importer.h"
#include "log.h"
#include "ops.h"
#include "plat.h"
#include "problem.h"
#include "strutil.h"
#include "version.h"

namespace poly {

namespace {

// ---------------------------------------------------------------- 常量与控件 ID

const wchar_t* kMainClass = L"PolyWorkbenchWnd";
const wchar_t* kMainTitle = L"poly 出题工作台";
const wchar_t* kImportClass = L"PolyImportWnd";
// 导入窗口的卡片数（格式顺序 = AppState::cards 的顺序）
constexpr int kImportCards = 4;
const wchar_t* kImportTitle = L"导入题目";

// 自定义消息：工作线程 → 界面线程
const UINT WM_APP_OP_DONE = WM_APP + 1;

enum : int {
    IDC_LIST = 100,
    IDC_LOG,
    IDC_STATUS,
    IDC_REFRESH,
    IDC_IMPORT,
    IDC_OPENDIR,
    IDC_DOCTOR,
    IDC_INFO,
    IDC_LBL_GROUP1 = 120,
    IDC_LBL_GROUP2,
    IDC_LBL_GROUP3,
    IDC_RUN,
    IDC_STRESS,
    IDC_TEST_ALL,
    IDC_STATEMENT,
    IDC_OPEN_STMT,
    IDC_EXPORT,
    IDC_CLEAN,
    IDC_BUILD,
    IDC_GEN,
    IDC_VALIDATE,
    IDC_SOLUTION = 140,
    IDC_TESTINDEX,
    IDC_GOOD,
    IDC_BAD,
    IDC_ROUNDS,
    IDC_FORMAT,
    IDC_ABOUT = 160,
    IDC_EXIT,
    IDC_MENU_IMPORT,
    IDC_MENU_OPENDIR,
    IDC_IMP_PICK = 200,   // 200,201,202
    IDC_IMP_NAME = 210,   // 210,211,212
    IDC_IMP_UP = 220,     // 220,221,222
    IDC_IMP_CLOSE = 230,
    IDC_IMPORT_STATUS,
};

// 设计尺寸（96 DPI 下的像素），实际布局按 DPI 缩放
const int kMargin = 10;
const int kGap = 6;
const int kRowH = 28;
const int kPanelW = 320;
const int kStatusH = 22;
const int kListH = 260;
const int kLogH = 220;

COLORREF kColText = RGB(0x1f, 0x23, 0x28);
COLORREF kColMuted = RGB(0x57, 0x60, 0x6a);
COLORREF kColBlue = RGB(0x09, 0x69, 0xda);
COLORREF kColGreen = RGB(0x1a, 0x7f, 0x37);
COLORREF kColGrey = RGB(0xf6, 0xf8, 0xfa);
COLORREF kColBorder = RGB(0xd0, 0xd7, 0xde);
COLORREF kColRed = RGB(0xcf, 0x22, 0x2e);

struct ImportCard {
    ImportFormat format;
    const wchar_t* title;
    const wchar_t* hint;
    std::string path;  // 选中的文件（UTF-8）
};

struct AppState {
    Context* ctx = nullptr;
    HWND main = nullptr;
    HWND list = nullptr;
    HWND logEdit = nullptr;
    HWND status = nullptr;
    HWND importWnd = nullptr;

    int dpi = 96;
    HFONT uiFont = nullptr;
    HFONT monoFont = nullptr;
    HFONT titleFont = nullptr;

    std::string logText;             // 日志框内容（自己维护，只读 EDIT 不能追加）
    std::vector<std::string> names;  // 列表行 → 题目名

    bool busy = false;
    std::string busyLabel;

    ImportCard cards[kImportCards] = {
        {ImportFormat::Qduoj, L"导入QDUOJ的题目",
         L"QDUOJ 后台导出的 .zip（内含 <编号>/problem.json 与 testcase/）", std::string()},
        {ImportFormat::Fps, L"导入FPS格式的题目",
         L"HUSTOJ 的 freeproblemset .xml（测试数据内嵌在 XML 里）", std::string()},
        {ImportFormat::Hydro, L"导入Hydro的题目",
         L"HydroOJ 导出的 .zip（内含 problem.yaml 与 testdata/）", std::string()},
        {ImportFormat::Hoj, L"导入HOJ的题目",
         L"HOJ 后台导出的 .zip（problem_编号.json + 同名测试数据目录）", std::string()},
    };
    std::string importStatus;
};

int scale(int value, int dpi) { return MulDiv(value, dpi, 96); }

std::wstring w(const std::string& s) { return to_wide(s); }

void set_font(HWND h, HFONT f) {
    if (h && f) SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(f), TRUE);
}

// 追加一段日志并滚动到底（只读 EDIT 只能整体替换文本）
void append_log(AppState& app, const std::string& text) {
    if (text.empty()) return;
    // EDIT 控件换行必须用 CRLF，否则所有内容会挤在一行
    for (char c : text) {
        if (c == '\n') {
            app.logText += "\r\n";
        } else if (c != '\r') {
            app.logText.push_back(c);
        }
    }
    // 只保留尾部，避免日志框无限增长
    const size_t kMaxLog = 400000;
    if (app.logText.size() > kMaxLog) {
        app.logText.erase(0, app.logText.size() - kMaxLog);
    }
    if (!app.logEdit) return;
    SetWindowTextW(app.logEdit, w(app.logText).c_str());
    SendMessageW(app.logEdit, EM_SETSEL, static_cast<WPARAM>(-1), static_cast<LPARAM>(-1));
    SendMessageW(app.logEdit, EM_SCROLLCARET, 0, 0);
}

void set_status(AppState& app, const std::string& text) {
    if (app.status) SetWindowTextW(app.status, w(text).c_str());
}

// STATIC 默认会画一块灰底，这里统一改成透明背景 + 按用途选颜色。
LRESULT handle_ctlcolor_static(HWND ctl, HDC dc) {
    int id = ctl ? GetDlgCtrlID(ctl) : 0;
    bool muted = (id == IDC_LBL_GROUP1 || id == IDC_LBL_GROUP2 || id == IDC_LBL_GROUP3 ||
                  id == IDC_STATUS || id == IDC_IMPORT_STATUS);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, muted ? kColMuted : kColText);
    return reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH));
}

std::string selected_problem(AppState& app) {
    if (!app.list) return std::string();
    int row = ListView_GetNextItem(app.list, -1, LVNI_SELECTED);
    if (row < 0 || row >= static_cast<int>(app.names.size())) return std::string();
    return app.names[static_cast<size_t>(row)];
}

Problem* load_selected(AppState& app, Problem& out) {
    std::string name = selected_problem(app);
    if (name.empty()) return nullptr;
    std::string err;
    if (!load_problem(app.ctx->workspace, name, out, err)) {
        append_log(app, "  " + err + "\n");
        return nullptr;
    }
    return &out;
}

// ---------------------------------------------------------------- 后台任务

enum class TaskKind { Operation, Import };

struct OpTask {
    AppState* app = nullptr;
    TaskKind kind = TaskKind::Operation;
    OpRequest request;
    std::string label;
    // import
    ImportFormat importFormat = ImportFormat::Fps;
    std::string importFilename;
    std::string importData;
    // 结果
    std::string log;
    int code = 0;
    double elapsedMs = 0;
    bool importOk = false;
    std::string importError;
    std::vector<ImportOutcome> outcomes;
};

std::mutex g_taskMutex;  // 同时只跑一个任务（编译/评测/对拍都不并发安全）

void enable_controls(AppState& app, bool enabled);

DWORD WINAPI op_thread(LPVOID param) {
    OpTask* task = static_cast<OpTask*>(param);
    std::lock_guard<std::mutex> lock(g_taskMutex);
    bool color = log_color_enabled();
    log_set_capture(&task->log);
    log_configure(false, false, false);
    auto t0 = std::chrono::steady_clock::now();
    if (task->kind == TaskKind::Import) {
        log_step(str("导入 {}（{}，{}）", task->importFilename, import_format_name(task->importFormat),
                     human_size(static_cast<long long>(task->importData.size()))));
        task->importOk = import_into_workspace(*task->app->ctx, task->importFormat,
                                              task->importFilename, task->importData,
                                              task->outcomes, task->importError);
        if (!task->importOk) log_err(task->importError);
        task->code = task->importOk ? 0 : 1;
    } else {
        try {
            task->code = run_operation(*task->app->ctx, task->request, task->label);
        } catch (const std::exception& e) {
            log_err(std::string("内部错误: ") + e.what());
            task->code = 1;
        } catch (...) {
            log_err("内部错误：未知异常");
            task->code = 1;
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    task->elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    log_set_capture(nullptr);
    log_configure(color, log_is_verbose(), log_is_quiet());
    PostMessageW(task->app->main, WM_APP_OP_DONE, 0, reinterpret_cast<LPARAM>(task));
    return 0;
}

void start_task(AppState& app, OpTask* task) {
    task->app = &app;
    enable_controls(app, false);
    app.busy = true;
    app.busyLabel = task->label.empty() ? std::string("导入题目") : task->label;
    set_status(app, "执行中… " + app.busyLabel);
    HANDLE h = CreateThread(nullptr, 0, op_thread, task, 0, nullptr);
    if (!h) {
        delete task;
        app.busy = false;
        enable_controls(app, true);
        set_status(app, "无法创建线程");
        return;
    }
    CloseHandle(h);
}

void run_action(AppState& app, const std::string& action,
                const std::map<std::string, std::string>& params = {}) {
    if (app.busy) return;
    OpTask* task = new OpTask();
    task->kind = TaskKind::Operation;
    task->request.action = action;
    task->request.problem = action == "doctor" ? std::string() : selected_problem(app);
    task->request.params = params;
    if (action != "doctor" && task->request.problem.empty()) {
        append_log(app, "  [FAIL]  请先在左侧列表里选中一道题目\n");
        set_status(app, "请先选中题目");
        delete task;
        return;
    }
    start_task(app, task);
}

// ---------------------------------------------------------------- 题目列表

void refresh_problems(AppState& app) {
    if (!app.list) return;
    SendMessageW(app.list, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(app.list);
    app.names.clear();

    std::vector<std::string> names = list_problems(app.ctx->workspace);
    for (const std::string& name : names) {
        Problem p;
        std::string err;
        std::string tests = "-";
        std::string limits = "-";
        std::string checker = "-";
        std::string kind = "-";
        std::string sols = "-";
        if (load_problem(app.ctx->workspace, name, p, err)) {
            tests = num(p.testCount());
            limits = num(p.timeLimitMs) + " ms / " + num(p.memoryLimitKb / 1024) + " MB";
            checker = p.checkerFile.empty() ? p.checker : "自定义";
            kind = p.interactive ? "交互题" : "传统题";
            sols = num(static_cast<long long>(p.solutions.size()));
        }
        int row = static_cast<int>(app.names.size());
        app.names.push_back(name);

        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = row;
        std::wstring wname = w(name);
        item.pszText = wname.data();
        int idx = ListView_InsertItem(app.list, &item);
        auto setCol = [&](int col, const std::string& text) {
            std::wstring ws = w(text);
            ListView_SetItemText(app.list, idx, col, ws.data());
        };
        setCol(1, tests);
        setCol(2, limits);
        setCol(3, checker);
        setCol(4, kind);
        setCol(5, sols);
    }
    SendMessageW(app.list, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(app.list, nullptr, TRUE);
    if (!app.names.empty() && ListView_GetNextItem(app.list, -1, LVNI_SELECTED) < 0) {
        ListView_SetItemState(app.list, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    }
    set_status(app, str("工作区 {}　共 {} 道题", app.ctx->workspace, names.size()));
}

// 选中题目后刷新右侧面板里的解法下拉框
void refresh_solution_combos(AppState& app) {
    HWND solCombo = GetDlgItem(app.main, IDC_SOLUTION);
    HWND goodCombo = GetDlgItem(app.main, IDC_GOOD);
    HWND badCombo = GetDlgItem(app.main, IDC_BAD);
    for (HWND c : {solCombo, goodCombo, badCombo}) {
        if (c) SendMessageW(c, CB_RESETCONTENT, 0, 0);
    }
    Problem p;
    if (!load_selected(app, p)) return;
    for (const SolutionInfo& s : p.solutions) {
        std::wstring label = w(s.name);
        for (HWND c : {solCombo, goodCombo, badCombo}) {
            if (c) SendMessageW(c, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        }
    }
    for (HWND c : {solCombo, goodCombo, badCombo}) {
        if (!c) continue;
        SendMessageW(c, CB_SETCURSEL, 0, 0);
    }
    if (badCombo && p.solutions.size() > 1) SendMessageW(badCombo, CB_SETCURSEL, 1, 0);
    set_status(app, str("已选中 {}：{} 个测试点，{} 个解法", p.name, p.testCount(),
                        p.solutions.size()));
}

std::string combo_text(HWND combo) {
    int sel = static_cast<int>(SendMessageW(combo, CB_GETCURSEL, 0, 0));
    if (sel < 0) return std::string();
    int len = static_cast<int>(SendMessageW(combo, CB_GETLBTEXTLEN, sel, 0));
    if (len <= 0) return std::string();
    std::wstring buf(static_cast<size_t>(len) + 1, L'\0');
    SendMessageW(combo, CB_GETLBTEXT, sel, reinterpret_cast<LPARAM>(buf.data()));
    return trim(to_utf8(buf.c_str()));
}

std::string window_text_u8(HWND h) {
    if (!h) return std::string();
    int len = GetWindowTextLengthW(h);
    std::wstring buf(static_cast<size_t>(len) + 1, L'\0');
    GetWindowTextW(h, buf.data(), len + 1);
    return trim(to_utf8(buf.c_str()));
}

// ---------------------------------------------------------------- 导出

bool ask_save_path(AppState& app, const Problem& p, const std::string& format, std::string& out) {
    ExportFormat fmt;
    if (!parse_export_format(format, fmt)) return false;
    std::string defaultName = default_export_name(p, fmt);
    std::wstring file = w(defaultName);
    std::wstring filter;
    if (fmt == ExportFormat::Fps) {
        filter = L"FPS 题目包 (*.xml)\0*.xml\0所有文件 (*.*)\0*.*\0";
    } else {
        filter = L"题目包 (*.zip)\0*.zip\0所有文件 (*.*)\0*.*\0";
    }
    wchar_t buffer[MAX_PATH * 2] = {};
    wcsncpy_s(buffer, file.c_str(), _TRUNCATE);
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = app.main;
    ofn.lpstrFilter = filter.c_str();
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = MAX_PATH * 2;
    ofn.lpstrTitle = L"导出到…";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    ofn.lpstrDefExt = (fmt == ExportFormat::Fps) ? L"xml" : L"zip";
    if (!GetSaveFileNameW(&ofn)) return false;
    out = to_utf8(buffer);
    return true;
}

void export_dialog(AppState& app) {
    std::string name = selected_problem(app);
    if (name.empty()) {
        append_log(app, "  [FAIL]  请先选中一道题目\n");
        return;
    }
    Problem p;
    std::string err;
    if (!load_problem(app.ctx->workspace, name, p, err)) {
        append_log(app, "  " + err + "\n");
        return;
    }
    std::string format = combo_text(GetDlgItem(app.main, IDC_FORMAT));
    if (format.empty()) format = "polygon";
    // 下拉框显示的是中文名，这里映射回格式名
    if (format == "Polygon 兼容包") format = "polygon";
    else if (format == "QDUOJ 压缩包") format = "qduoj";
    else if (format == "FPS（xml）") format = "fps";
    else if (format == "Hydro 压缩包") format = "hydro";
    else if (format == "HOJ 压缩包") format = "hoj";
    std::string out;
    if (!ask_save_path(app, p, format, out)) return;
    run_action(app, "package", {{"format", format}, {"output", out}});
}

// ---------------------------------------------------------------- 导入窗口

void import_pick_file(AppState& app, int index) {
    ImportCard& card = app.cards[index];
    wchar_t buffer[MAX_PATH * 2] = {};
    std::wstring filter;
    switch (card.format) {
        case ImportFormat::Fps:
            filter = L"FPS 题目包 (*.xml)\0*.xml\0压缩包 (*.zip)\0*.zip\0所有文件 (*.*)\0*.*\0";
            break;
        case ImportFormat::Qduoj:
        case ImportFormat::Hydro:
        case ImportFormat::Hoj:
            filter = L"题目压缩包 (*.zip)\0*.zip\0所有文件 (*.*)\0*.*\0";
            break;
    }
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = app.importWnd ? app.importWnd : app.main;
    ofn.lpstrFilter = filter.c_str();
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = MAX_PATH * 2;
    ofn.lpstrTitle = L"选择题目包";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    if (!GetOpenFileNameW(&ofn)) return;
    card.path = to_utf8(buffer);
    HWND nameWnd = GetDlgItem(app.importWnd, IDC_IMP_NAME + index);
    if (nameWnd) SetWindowTextW(nameWnd, w(fs::basename(card.path)).c_str());
}

void import_upload(AppState& app, int index) {
    ImportCard& card = app.cards[index];
    if (card.path.empty()) {
        SetWindowTextW(GetDlgItem(app.importWnd, IDC_IMPORT_STATUS), L"请先选择文件");
        return;
    }
    std::string data;
    if (!fs::read_file(card.path, data) || data.empty()) {
        SetWindowTextW(GetDlgItem(app.importWnd, IDC_IMPORT_STATUS), L"读取文件失败或文件为空");
        return;
    }
    if (app.busy) return;
    OpTask* task = new OpTask();
    task->kind = TaskKind::Import;
    task->importFormat = card.format;
    task->importFilename = fs::basename(card.path);
    task->importData = std::move(data);
    task->label = str("导入 {}", import_format_name(card.format));
    start_task(app, task);
}

LRESULT CALLBACK import_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    AppState* app = reinterpret_cast<AppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            app = static_cast<AppState*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
            app->importWnd = hwnd;
            int dpi = app->dpi;
            int y = scale(kMargin, dpi);
            for (int i = 0; i < kImportCards; i++) {
                const ImportCard& card = app->cards[i];
                HWND title = CreateWindowExW(0, L"STATIC", card.title, WS_CHILD | WS_VISIBLE,
                                             scale(kMargin, dpi), y, scale(560, dpi), scale(30, dpi),
                                             hwnd, nullptr, nullptr, nullptr);
                set_font(title, app->titleFont);
                y += scale(34, dpi);
                HWND hint = CreateWindowExW(0, L"STATIC", card.hint, WS_CHILD | WS_VISIBLE,
                                            scale(kMargin, dpi), y, scale(600, dpi), scale(22, dpi),
                                            hwnd, nullptr, nullptr, nullptr);
                set_font(hint, app->uiFont);
                y += scale(26, dpi);
                HWND pick = CreateWindowExW(0, L"BUTTON", L"选择文件",
                                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                            scale(kMargin, dpi), y, scale(110, dpi), scale(kRowH, dpi),
                                            hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_IMP_PICK + i)),
                                            nullptr, nullptr);
                set_font(pick, app->uiFont);
                HWND nameWnd = CreateWindowExW(0, L"STATIC", L"未选择文件", WS_CHILD | WS_VISIBLE,
                                               scale(kMargin + 120, dpi), y + scale(5, dpi),
                                               scale(280, dpi), scale(20, dpi), hwnd,
                                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_IMP_NAME + i)),
                                               nullptr, nullptr);
                set_font(nameWnd, app->monoFont);
                HWND up = CreateWindowExW(0, L"BUTTON", L"上传",
                                          WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                          scale(kMargin + 410, dpi), y, scale(110, dpi),
                                          scale(kRowH, dpi), hwnd,
                                          reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_IMP_UP + i)),
                                          nullptr, nullptr);
                set_font(up, app->uiFont);
                y += scale(kRowH + 18, dpi);
            }
            HWND status = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                          scale(kMargin, dpi), y, scale(640, dpi), scale(24, dpi),
                                          hwnd,
                                          reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_IMPORT_STATUS)),
                                          nullptr, nullptr);
            set_font(status, app->uiFont);
            y += scale(34, dpi);
            HWND close = CreateWindowExW(0, L"BUTTON", L"关闭",
                                         WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                         scale(kMargin + 640, dpi) - scale(110, dpi), y,
                                         scale(110, dpi), scale(kRowH, dpi), hwnd,
                                         reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_IMP_CLOSE)),
                                         nullptr, nullptr);
            set_font(close, app->uiFont);
            RECT rc{0, 0, scale(720, dpi), y + scale(kRowH + kMargin, dpi)};
            AdjustWindowRectEx(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE, 0);
            SetWindowPos(hwnd, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top,
                         SWP_NOMOVE | SWP_NOZORDER);
            return 0;
        }
        case WM_DRAWITEM: {
            DRAWITEMSTRUCT* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            if (!app || !dis) return FALSE;
            bool primary = false, success = false;            int id = static_cast<int>(dis->CtlID);
            if (id >= IDC_IMP_PICK && id < IDC_IMP_PICK + kImportCards) primary = true;
            if (id >= IDC_IMP_UP && id < IDC_IMP_UP + kImportCards) success = true;
            COLORREF fill = primary ? kColBlue : (success ? kColGreen : kColGrey);
            COLORREF text = (primary || success) ? RGB(255, 255, 255) : kColText;
            if ((dis->itemState & ODS_DISABLED) || (success && app->busy)) {
                fill = RGB(0xe6, 0xe9, 0xec);
                text = kColMuted;
            }
            HBRUSH brush = CreateSolidBrush(fill);
            FillRect(dis->hDC, &dis->rcItem, brush);
            DeleteObject(brush);
            FrameRect(dis->hDC, &dis->rcItem, GetSysColorBrush(COLOR_BTNSHADOW));
            wchar_t label[64] = {};
            GetWindowTextW(dis->hwndItem, label, 64);
            SetBkMode(dis->hDC, TRANSPARENT);
            SetTextColor(dis->hDC, text);
            HFONT old = static_cast<HFONT>(SelectObject(dis->hDC, app->uiFont));
            DrawTextW(dis->hDC, label, -1, &dis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(dis->hDC, old);
            return TRUE;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id >= IDC_IMP_PICK && id < IDC_IMP_PICK + kImportCards) {
                import_pick_file(*app, id - IDC_IMP_PICK);
                return 0;
            }
            if (id >= IDC_IMP_UP && id < IDC_IMP_UP + kImportCards) {
                import_upload(*app, id - IDC_IMP_UP);
                return 0;
            }
            if (id == IDC_IMP_CLOSE) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }
        case WM_CTLCOLORSTATIC:
            return handle_ctlcolor_static(reinterpret_cast<HWND>(lParam),
                                          reinterpret_cast<HDC>(wParam));
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            if (app) app->importWnd = nullptr;
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void open_import_window(AppState& app) {
    if (app.importWnd) {
        SetForegroundWindow(app.importWnd);
        return;
    }
    CreateWindowExW(WS_EX_DLGMODALFRAME, kImportClass, kImportTitle,
                    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, CW_USEDEFAULT,
                    CW_USEDEFAULT, scale(740, app.dpi), scale(430, app.dpi), app.main, nullptr,
                    nullptr, &app);
}

// ---------------------------------------------------------------- 布局

void layout_controls(AppState& app, int W, int H) {
    int dpi = app.dpi;
    int m = scale(kMargin, dpi);
    int gap = scale(kGap, dpi);
    int statusH = scale(kStatusH, dpi);

    // 顶部工具条
    int y = m;
    struct ToolBtn {
        int id;
        int width;
    };
    const ToolBtn tools[] = {
        {IDC_REFRESH, 80}, {IDC_IMPORT, 110}, {IDC_OPENDIR, 110}, {IDC_DOCTOR, 100},
    };
    int x = m;
    int toolH = scale(kRowH, dpi);
    for (const ToolBtn& t : tools) {
        HWND h = GetDlgItem(app.main, t.id);
        if (h) MoveWindow(h, x, y, scale(t.width, dpi), toolH, TRUE);
        x += scale(t.width, dpi) + gap;
    }
    y += toolH + gap;

    int contentW = W - 2 * m;
    int contentBottom = H - m - statusH;
    if (contentW < scale(700, dpi)) contentW = scale(700, dpi);

    // 右列：操作面板；左列：题目列表 + 日志
    int panelW = scale(kPanelW, dpi);
    if (panelW > contentW - scale(320, dpi)) panelW = contentW - scale(320, dpi);
    int leftW = contentW - panelW - gap;
    int avail = contentBottom - y;
    if (avail < scale(260, dpi)) avail = scale(260, dpi);

    int listH = (avail - gap) * 55 / 100;
    int logH = avail - gap - listH;
    if (listH < scale(140, dpi)) listH = scale(140, dpi);
    if (logH < scale(120, dpi)) logH = scale(120, dpi);

    if (app.list) MoveWindow(app.list, m, y, leftW, listH, TRUE);
    if (app.logEdit) {
        MoveWindow(app.logEdit, m, y + listH + gap, leftW, logH, TRUE);
    }

    // 右侧面板：控件数固定，按可用高度反推行高，保证不溢出
    struct PanelEntry {
        int id;
        bool group;
    };
    const PanelEntry panel[] = {
        {IDC_LBL_GROUP1, true},  {IDC_BUILD, false},     {IDC_GEN, false},
        {IDC_VALIDATE, false},   {IDC_LBL_GROUP2, true}, {IDC_SOLUTION, false},
        {IDC_TESTINDEX, false},  {IDC_RUN, false},       {IDC_GOOD, false},
        {IDC_BAD, false},        {IDC_ROUNDS, false},    {IDC_STRESS, false},
        {IDC_TEST_ALL, false},   {IDC_LBL_GROUP3, true}, {IDC_STATEMENT, false},
        {IDC_OPEN_STMT, false},  {IDC_FORMAT, false},    {IDC_EXPORT, false},
        {IDC_CLEAN, false},
    };
    const size_t kPanelCount = sizeof(panel) / sizeof(panel[0]);
    size_t groups = 0;
    for (size_t i = 0; i < kPanelCount; i++) {
        if (panel[i].group) groups++;
    }
    size_t rows = kPanelCount - groups;
    int vgap = scale(2, dpi);
    int groupH = scale(22, dpi);
    int rowH = scale(kRowH, dpi);
    // 理想高度不够时逐步压缩行高（下限 20 设计像素）
    while (rowH > scale(20, dpi) &&
           static_cast<int>(groups * (groupH + vgap) + rows * (rowH + vgap)) > avail) {
        rowH -= scale(1, dpi);
    }
    int px = m + leftW + gap;
    int by = y;
    for (const PanelEntry& e : panel) {
        HWND h = GetDlgItem(app.main, e.id);
        int hh = e.group ? groupH : rowH;
        if (h) MoveWindow(h, px, by, panelW, hh, TRUE);
        by += hh + vgap;
    }

    if (app.status) MoveWindow(app.status, m, H - m - statusH, contentW, statusH, TRUE);
}

void enable_controls(AppState& app, bool enabled) {
    for (int id : {IDC_RUN, IDC_STRESS, IDC_TEST_ALL, IDC_STATEMENT, IDC_OPEN_STMT, IDC_EXPORT,
                   IDC_CLEAN, IDC_BUILD, IDC_GEN, IDC_VALIDATE, IDC_REFRESH, IDC_IMPORT,
                   IDC_OPENDIR, IDC_DOCTOR, IDC_SOLUTION, IDC_TESTINDEX, IDC_GOOD, IDC_BAD,
                   IDC_ROUNDS, IDC_FORMAT}) {
        HWND h = GetDlgItem(app.main, id);
        if (h) EnableWindow(h, enabled);
    }
    if (app.importWnd) {
        for (int i = 0; i < kImportCards; i++) {
            for (int base : {IDC_IMP_PICK, IDC_IMP_UP}) {
                HWND h = GetDlgItem(app.importWnd, base + i);
                if (h) EnableWindow(h, enabled);
            }
        }
    }
    if (app.main) InvalidateRect(app.main, nullptr, TRUE);
}

// ---------------------------------------------------------------- 主窗口

void open_statement_file(AppState& app) {
    std::string name = selected_problem(app);
    if (name.empty()) {
        append_log(app, "  [FAIL]  请先选中一道题目\n");
        return;
    }
    Problem p;
    std::string err;
    if (!load_problem(app.ctx->workspace, name, p, err)) {
        append_log(app, "  " + err + "\n");
        return;
    }
    std::string html = p.path("statements/statement.html");
    if (!fs::is_file(html)) {
        append_log(app, "  [WARN]  还没有生成题面 HTML，请先点「渲染题面」\n");
        return;
    }
    ShellExecuteW(nullptr, L"open", w(html).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void open_workspace_dir(AppState& app) {
    ShellExecuteW(nullptr, L"open", w(app.ctx->workspace).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void run_solution(AppState& app) {
    std::string sol = combo_text(GetDlgItem(app.main, IDC_SOLUTION));
    if (sol.empty()) {
        append_log(app, "  [FAIL]  该题目还没有解法\n");
        return;
    }
    std::string testText = window_text_u8(GetDlgItem(app.main, IDC_TESTINDEX));
    run_action(app, "run", {{"solution", sol}, {"test", testText}});
}

void run_stress(AppState& app) {
    std::string good = combo_text(GetDlgItem(app.main, IDC_GOOD));
    std::string bad = combo_text(GetDlgItem(app.main, IDC_BAD));
    std::string rounds = window_text_u8(GetDlgItem(app.main, IDC_ROUNDS));
    if (rounds.empty()) rounds = "300";
    run_action(app, "stress", {{"solution", good}, {"solution2", bad}, {"rounds", rounds}});
}

LRESULT CALLBACK main_wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    AppState* app = reinterpret_cast<AppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            app = static_cast<AppState*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
            app->main = hwnd;

            int dpi = app->dpi;
            app->uiFont = CreateFontW(-MulDiv(9, dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
            app->monoFont = CreateFontW(-MulDiv(9, dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                        CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");
            app->titleFont = CreateFontW(-MulDiv(11, dpi, 72), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE,
                                         FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                         CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH,
                                         L"Microsoft YaHei UI");

            auto createTool = [&](int id, const wchar_t* label) {
                HWND h = CreateWindowExW(0, L"BUTTON", label,
                                         WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 10,
                                         10, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                         nullptr, nullptr);
                set_font(h, app->uiFont);
                return h;
            };
            createTool(IDC_REFRESH, L"刷新");
            createTool(IDC_IMPORT, L"导入题目…");
            createTool(IDC_OPENDIR, L"打开工作区");
            createTool(IDC_DOCTOR, L"环境自检");

            app->list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT |
                                            LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                                        0, 0, 10, 10, hwnd,
                                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_LIST)),
                                        nullptr, nullptr);
            set_font(app->list, app->uiFont);
            ListView_SetExtendedListViewStyle(app->list,
                                              LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES |
                                                  LVS_EX_DOUBLEBUFFER);
            const wchar_t* columns[] = {L"题目", L"测试点", L"时限 / 内存", L"checker", L"类型",
                                        L"解法"};
            const int widths[] = {180, 70, 130, 90, 70, 60};
            for (int i = 0; i < 6; i++) {
                LVCOLUMNW col{};
                col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
                col.pszText = const_cast<LPWSTR>(columns[i]);
                col.cx = scale(widths[i], dpi);
                col.iSubItem = i;
                ListView_InsertColumn(app->list, i, &col);
            }

            app->logEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                           WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE |
                                               ES_READONLY | ES_LEFT | ES_AUTOVSCROLL,
                                           0, 0, 10, 10, hwnd,
                                           reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_LOG)),
                                           nullptr, nullptr);
            set_font(app->logEdit, app->monoFont);

            app->status = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0,
                                          10, 10, hwnd,
                                          reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_STATUS)),
                                          nullptr, nullptr);
            set_font(app->status, app->uiFont);

            auto createGroup = [&](int id, const wchar_t* label) {
                HWND h = CreateWindowExW(0, L"STATIC", label, WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0,
                                         10, 10, hwnd,
                                         reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr,
                                         nullptr);
                set_font(h, app->uiFont);
                return h;
            };
            createGroup(IDC_LBL_GROUP1, L"造数据");
            createGroup(IDC_LBL_GROUP2, L"评测");
            createGroup(IDC_LBL_GROUP3, L"题面与导出");

            createTool(IDC_BUILD, L"编译生成器 / 校验器 / 解法");
            createTool(IDC_GEN, L"生成测试点");
            createTool(IDC_VALIDATE, L"用校验器检查");
            createTool(IDC_RUN, L"运行选中的解法");
            createTool(IDC_STRESS, L"开始对拍");
            createTool(IDC_TEST_ALL, L"评测全部解法");
            createTool(IDC_STATEMENT, L"渲染题面");
            createTool(IDC_OPEN_STMT, L"打开题面（HTML）");
            createTool(IDC_EXPORT, L"导出…");
            createTool(IDC_CLEAN, L"清理中间产物");

            auto createCombo = [&](int id, const wchar_t* items) {
                HWND h = CreateWindowExW(0, L"COMBOBOX", L"",
                                         WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST |
                                             WS_VSCROLL,
                                         0, 0, 10, 10, hwnd,
                                         reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr,
                                         nullptr);
                set_font(h, app->uiFont);
                if (items) {
                    std::wstring text(items);
                    size_t start = 0;
                    while (start <= text.size()) {
                        size_t sep = text.find(L'|', start);
                        std::wstring item =
                            text.substr(start, sep == std::wstring::npos ? std::wstring::npos
                                                                        : sep - start);
                        if (!item.empty()) SendMessageW(h, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item.c_str()));
                        if (sep == std::wstring::npos) break;
                        start = sep + 1;
                    }
                    SendMessageW(h, CB_SETCURSEL, 0, 0);
                }
                return h;
            };
            createCombo(IDC_SOLUTION, nullptr);
            createCombo(IDC_GOOD, nullptr);
            createCombo(IDC_BAD, nullptr);
            createCombo(IDC_FORMAT,
                        L"Polygon 兼容包|QDUOJ 压缩包|FPS（xml）|Hydro 压缩包|HOJ 压缩包");

            auto createEdit = [&](int id, const wchar_t* text) {
                HWND h = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", text,
                                         WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT | ES_NUMBER,
                                         0, 0, 10, 10, hwnd,
                                         reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr,
                                         nullptr);
                set_font(h, app->uiFont);
                return h;
            };
            createEdit(IDC_TESTINDEX, L"");
            createEdit(IDC_ROUNDS, L"300");

            append_log(*app,
                        str("poly {} 出题工作台\n工作区 {}\n\n", POLY_VERSION, app->ctx->workspace));
            append_log(*app, "用法：在左侧选中一道题 → 右侧执行操作；结果会打印在下面的框里。\n");
            refresh_problems(*app);
            return 0;
        }

        case WM_SIZE:
            if (app) layout_controls(*app, LOWORD(lParam), HIWORD(lParam));
            return 0;

        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
            int dpi = app ? app->dpi : 96;
            mmi->ptMinTrackSize.x = scale(900, dpi);
            mmi->ptMinTrackSize.y = scale(600, dpi);
            return 0;
        }

        case WM_DRAWITEM: {
            DRAWITEMSTRUCT* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            if (!app) return FALSE;
            int id = static_cast<int>(dis->CtlID);
            bool primary = (id == IDC_REFRESH);
            bool success = (id == IDC_EXPORT);
            COLORREF fill = success ? kColGreen : (primary ? kColBlue : kColGrey);
            COLORREF text = (primary || success) ? RGB(255, 255, 255) : kColText;
            if (dis->itemState & ODS_DISABLED) {
                fill = RGB(0xe6, 0xe9, 0xec);
                text = kColMuted;
            }
            HBRUSH brush = CreateSolidBrush(fill);
            FillRect(dis->hDC, &dis->rcItem, brush);
            DeleteObject(brush);
            FrameRect(dis->hDC, &dis->rcItem, GetSysColorBrush(COLOR_BTNSHADOW));
            wchar_t label[128] = {};
            GetWindowTextW(dis->hwndItem, label, 128);
            SetBkMode(dis->hDC, TRANSPARENT);
            SetTextColor(dis->hDC, text);
            HFONT old = static_cast<HFONT>(SelectObject(dis->hDC, app->uiFont));
            DrawTextW(dis->hDC, label, -1, &dis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(dis->hDC, old);
            return TRUE;
        }

        case WM_NOTIFY: {
            if (!app) break;
            NMHDR* hdr = reinterpret_cast<NMHDR*>(lParam);
            if (hdr->idFrom == IDC_LIST && hdr->code == LVN_ITEMCHANGED) {
                NMLISTVIEW* nv = reinterpret_cast<NMLISTVIEW*>(lParam);
                if ((nv->uNewState & LVIS_SELECTED) && !(nv->uOldState & LVIS_SELECTED)) {
                    refresh_solution_combos(*app);
                }
            }
            break;
        }

        case WM_APP_OP_DONE: {
            if (!app) return 0;
            OpTask* task = reinterpret_cast<OpTask*>(lParam);
            app->busy = false;
            enable_controls(*app, true);
            append_log(*app, task->log);
            int ms = static_cast<int>(task->elapsedMs + 0.5);
            if (task->kind == TaskKind::Import) {
                if (task->importOk) {
                    std::string summary = str("导入成功：{} 道题（{} ms）", task->outcomes.size(), ms);
                    append_log(*app, "\n  " + summary + "\n");
                    for (const ImportOutcome& o : task->outcomes) {
                        append_log(*app, str("    · {}（{} 个测试点，{}）\n", o.name, o.tests,
                                             o.hasChecker ? "含 checker" : "内置 checker"));
                    }
                    set_status(*app, summary);
                    if (app->importWnd) {
                        SetWindowTextW(GetDlgItem(app->importWnd, IDC_IMPORT_STATUS),
                                       w(summary).c_str());
                    }
                    refresh_problems(*app);
                } else {
                    set_status(*app, "导入失败：" + task->importError);
                    if (app->importWnd) {
                        SetWindowTextW(GetDlgItem(app->importWnd, IDC_IMPORT_STATUS),
                                       w("导入失败：" + task->importError).c_str());
                    }
                }
            } else {
                set_status(*app, str("{}：{}（{} ms）", task->label,
                                     task->code == 0 ? "成功" : ("失败，退出码 " + num(task->code)), ms));
                refresh_problems(*app);
                refresh_solution_combos(*app);
            }
            delete task;
            return 0;
        }

        case WM_COMMAND: {
            if (!app) break;
            int id = LOWORD(wParam);
            if (app->busy && !(id == IDC_IMPORT || id == IDC_OPENDIR)) {
                append_log(*app, "  正在执行上一个操作，请稍候…\n");
                return 0;
            }
            switch (id) {
                case IDC_REFRESH: refresh_problems(*app); return 0;
                case IDC_IMPORT: open_import_window(*app); return 0;
                case IDC_OPENDIR: open_workspace_dir(*app); return 0;
                case IDC_DOCTOR: run_action(*app, "doctor"); return 0;
                case IDC_BUILD: run_action(*app, "build"); return 0;
                case IDC_GEN: run_action(*app, "gen"); return 0;
                case IDC_VALIDATE: run_action(*app, "validate"); return 0;
                case IDC_TEST_ALL: run_action(*app, "test"); return 0;
                case IDC_RUN: run_solution(*app); return 0;
                case IDC_STRESS: run_stress(*app); return 0;
                case IDC_STATEMENT: run_action(*app, "statement"); return 0;
                case IDC_OPEN_STMT: open_statement_file(*app); return 0;
                case IDC_EXPORT: export_dialog(*app); return 0;
                case IDC_CLEAN: run_action(*app, "clean"); return 0;
                case IDC_ABOUT: {
                    std::string text = str("poly {}\n\n本地离线出题工作台\n"
                                           "所有操作最终都等价于 poly 命令行，日志原样显示在下方。",
                                           POLY_VERSION);
                    MessageBoxW(hwnd, w(text).c_str(), L"关于 poly", MB_OK | MB_ICONINFORMATION);
                    return 0;
                }
                case IDC_EXIT: DestroyWindow(hwnd); return 0;
                default: break;
            }
            break;
        }

        case WM_CTLCOLORSTATIC:
            return handle_ctlcolor_static(reinterpret_cast<HWND>(lParam),
                                          reinterpret_cast<HDC>(wParam));

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace

int run_win32_ui(Context& ctx) {
    // 高分屏：先声明 DPI 感知，所有设计尺寸再按实际 DPI 换算。
    SetProcessDPIAware();
    HDC screen = GetDC(nullptr);
    int dpi = screen ? GetDeviceCaps(screen, LOGPIXELSX) : 96;
    if (screen) ReleaseDC(nullptr, screen);
    if (dpi <= 0) dpi = 96;

    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    AppState app;
    app.ctx = &ctx;
    app.dpi = dpi;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = main_wnd_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kMainClass;
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hIconSm = wc.hIcon;
    if (!RegisterClassExW(&wc)) {
        log_err("注册窗口类失败");
        return 1;
    }

    WNDCLASSEXW wc2 = wc;
    wc2.lpfnWndProc = import_proc;
    wc2.lpszClassName = kImportClass;
    RegisterClassExW(&wc2);

    RECT rc{0, 0, scale(1180, dpi), scale(760, dpi)};
    AdjustWindowRectEx(&rc, WS_OVERLAPPEDWINDOW, FALSE, 0);
    HWND hwnd = CreateWindowExW(0, kMainClass, kMainTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                                CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr,
                                nullptr, wc.hInstance, &app);
    if (!hwnd) {
        log_err("创建窗口失败");
        return 1;
    }
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}

}  // namespace poly

#endif  // _WIN32
