// 打包导出：把题目导出成 Polygon / QDUOJ / FPS / Hydro / HOJ 五种格式。
// 具体格式实现都在 exporter.cpp，这里只做参数解析、公共准备与分发。
#include <utility>
#include <vector>

#include "commands.h"
#include "exporter.h"
#include "format.h"
#include "fsutil.h"
#include "judge.h"
#include "log.h"
#include "pipeline.h"
#include "strutil.h"
#include "table.h"

namespace poly {

int cmd_package(Context& ctx, const Args& args) {
    Problem p;
    if (!ctx.requireProblem(p)) return 1;

    // 既支持 --format qduoj，也支持更顺手的 --qduoj / --fps / --hydro / --hoj。
    struct Alias {
        const char* flag;
        ExportFormat format;
    };
    static const Alias kAliases[] = {
        {"polygon", ExportFormat::Polygon},
        {"qduoj", ExportFormat::Qduoj},
        {"fps", ExportFormat::Fps},
        {"hydro", ExportFormat::Hydro},
        {"hoj", ExportFormat::Hoj},
    };
    ExportFormat format = ExportFormat::Polygon;
    for (const Alias& alias : kAliases) {
        if (args.has_flag(alias.flag)) format = alias.format;
    }
    if (args.has("format")) {
        std::string name = args.get("format");
        if (!parse_export_format(name, format)) {
            log_err(str("未知的导出格式：{}（可选 polygon / qduoj / fps / hydro / hoj）", name));
            return 1;
        }
    }

    JudgeEnv env;
    std::string err;
    if (!prepare_judge(ctx, p, "solutions", env, err)) {
        log_err(err);
        return 1;
    }

    std::vector<int> tests = selected_tests(p, {}, 0);
    if (tests.empty()) {
        log_err("没有测试点（先执行 poly gen）");
        return 1;
    }

    if (!p.interactive) {
        std::vector<std::string> errs;
        ensure_answers(env, args.get("answer-solution"), errs);
        if (!errs.empty()) {
            for (const std::string& e : errs) log_err(e);
            log_err("打包需要每个测试点的标准答案（tests/NN.a）");
            return 1;
        }
    }

    if (!export_problem(ctx, p, env, tests, format, args)) return 1;

    if (args.has_flag("list") && format == ExportFormat::Polygon) {
        Column colName("ENTRY"), colSize("SIZE");
        std::vector<std::string> files = fs::list_files_recursive(p.dir);
        for (size_t i = 0; i < files.size() && i < 200; i++) {
            colName.cells.push_back(fs::relative_to(files[i], p.dir));
            colSize.cells.push_back(human_size(fs::file_size(files[i])));
        }
        print_table({colName, colSize});
    }
    return 0;
}

}  // namespace poly
