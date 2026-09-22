// 导入：把 QDUOJ / FPS / Hydro / HOJ 题目包转成 poly 工作区里的题目。
// 网页工作台（poly ui）的导入页调用的是同一套 importer，这里只是命令行入口。
#include <string>
#include <vector>

#include "commands.h"
#include "format.h"
#include "fsutil.h"
#include "importer.h"
#include "log.h"
#include "strutil.h"
#include "zip.h"

namespace poly {

namespace {

const char* kUsage = "用法: poly import [qduoj|fps|hydro|hoj] <题目包文件>";

// zip 包：看目录结构分辨三种压缩包（HOJ 原生的特征是根目录就有 json）。
bool detect_zip_format(const std::string& data, ImportFormat& out) {
    ZipReader zip;
    std::string err;
    if (!zip.open_data(data, err)) return false;
    bool rootJson = false, nestedProblemJson = false, yaml = false;
    for (const ZipEntryInfo& e : zip.entries()) {
        std::string path = replace_all(e.name, "\\", "/");
        std::string lower = to_lower(path);
        if (ends_with(lower, "problem.yaml") || ends_with(lower, "problem.yml")) yaml = true;
        if (path.find('/') == std::string::npos && ends_with(lower, ".json")) rootJson = true;
        if (lower == "problem.json" || ends_with(lower, "/problem.json")) nestedProblemJson = true;
    }
    if (yaml) {
        out = ImportFormat::Hydro;
        return true;
    }
    if (rootJson) {
        out = ImportFormat::Hoj;
        return true;
    }
    if (nestedProblemJson) {
        out = ImportFormat::Qduoj;
        return true;
    }
    return false;
}

// 没写格式时按内容猜：zip 看目录结构，其余按 XML 文本判断。
bool detect_format(const std::string& filename, const std::string& data, ImportFormat& out) {
    if (data.size() >= 4 && data.compare(0, 4, "PK\x03\x04") == 0) {
        return detect_zip_format(data, out);
    }
    std::string head = to_lower(data.substr(0, 4096));
    if (head.find("<!doctype fps") != std::string::npos || head.find("<fps") != std::string::npos) {
        out = ImportFormat::Fps;
        return true;
    }
    (void)filename;
    return false;
}

}  // namespace

int cmd_import(Context& ctx, const Args& args) {
    if (args.positional.empty()) {
        log_err(kUsage);
        return 1;
    }
    std::string first = args.positional[0];
    std::string path;
    ImportFormat format = ImportFormat::Fps;
    bool haveFormat = parse_import_format(first, format);

    if (haveFormat) {
        if (args.positional.size() < 2) {
            log_err(kUsage);
            return 1;
        }
        path = args.positional[1];
    } else {
        path = first;
        if (!fs::is_file(path)) {
            log_err(str("找不到文件 {}", path));
            return 1;
        }
        std::string probe = fs::read_file(path);
        if (!detect_format(path, probe, format)) {
            log_err("无法识别题目包格式，请显式指定：poly import qduoj|fps|hydro|hoj <文件>");
            return 1;
        }
        log_info(str("按内容识别为 {} 格式", import_format_name(format)));
    }

    std::string data;
    if (!fs::read_file(path, data)) {
        log_err(str("无法读取 {}", path));
        return 1;
    }
    if (data.empty()) {
        log_err(str("{} 是空文件", path));
        return 1;
    }

    log_step(str("导入 {}（{}）", fs::basename(path), human_size(static_cast<long long>(data.size()))));
    std::vector<ImportOutcome> outcomes;
    std::string err;
    if (!import_into_workspace(ctx, format, fs::basename(path), data, outcomes, err)) {
        log_err(err);
        return 1;
    }

    for (const ImportOutcome& o : outcomes) {
        log_ok(str("{} → {}", o.name, fs::relative_to(o.dir, ctx.workspace)));
        log_info(str("        测试点 {} 个，checker {}，{}", o.tests,
                     o.hasChecker ? "已导入" : "内置 ncmp", o.interactive ? "交互题" : "传统题"));
        for (const std::string& note : o.notes) log_warn(note);
    }
    log_info(str("共导入 {} 道题。下一步：poly build {} && poly test {}", outcomes.size(),
                 outcomes.front().name, outcomes.front().name));
    return 0;
}

}  // namespace poly
