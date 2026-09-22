#include <algorithm>

#include "commands.h"
#include "compile.h"
#include "format.h"
#include "fsutil.h"
#include "log.h"
#include "plat.h"
#include "proc.h"
#include "strutil.h"
#include "table.h"
#include "templates.h"
#include "testlib_source.h"
#include "version.h"

namespace poly {

namespace {

bool valid_problem_name(const std::string& name) {
    if (name.empty()) return false;
    for (char c : name) {
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
                  c == '-' || c == '.';
        if (!ok) return false;
    }
    return name != "." && name != "..";
}

void ensure_workspace(const Context& ctx) {
    fs::mkdirs(ctx.workspace);
    std::string cfg = fs::join(ctx.workspace, "poly.json");
    if (!fs::is_file(cfg)) fs::write_file(cfg, tpl_workspace_config());
}

long long parse_memory_mb(const std::string& s, long long defMb) {
    std::string v = to_lower(trim(s));
    if (v.empty()) return defMb;
    long long mult = 1;
    if (ends_with(v, "kb") || ends_with(v, "k")) {
        mult = 1;
        v = v.substr(0, v.size() - (ends_with(v, "kb") ? 2 : 1));
    } else if (ends_with(v, "mb") || ends_with(v, "m")) {
        mult = 1024;
        v = v.substr(0, v.size() - (ends_with(v, "mb") ? 2 : 1));
    } else if (ends_with(v, "gb") || ends_with(v, "g")) {
        mult = 1024 * 1024;
        v = v.substr(0, v.size() - (ends_with(v, "gb") ? 2 : 1));
    } else {
        return to_int(s, defMb);
    }
    return to_int(v, defMb) * mult;
}
}  // namespace



int cmd_init(Context& ctx, const Args& args) {
    std::string name = args.get("name");
    if (name.empty() && !args.positional.empty()) name = args.positional[0];
    if (name.empty()) {
        log_err("用法: poly init <题目名> [--checker custom] [--interactive] [--time-limit 2000] [--memory-limit 256]");
        return 1;
    }
    if (!valid_problem_name(name)) {
        log_err(str("非法题目名: {}（只允许字母、数字、下划线、连字符和点）", name));
        return 1;
    }

    ensure_workspace(ctx);
    std::string dir = fs::join(ctx.workspace, name);
    if (fs::exists(dir)) {
        if (!args.has_flag("force")) {
            log_err(str("目录已存在: {}（使用 --force 覆盖）", dir));
            return 1;
        }
        fs::remove_all(dir);
    }

    Problem p;
    p.name = name;
    p.dir = fs::absolute(dir);
    p.timeLimitMs = static_cast<int>(args.get_int("time-limit", 2000));
    p.memoryLimitKb = parse_memory_mb(args.get("memory-limit", "256"), 256) * 1024;
    p.checker = args.get("checker-name", "ncmp");
    bool customChecker = args.has_flag("checker") || args.get("checker", "") == "custom" ||
                         args.get("checker-type", "") == "custom";
    p.interactive = args.has_flag("interactive");
    p.validator = args.has_flag("no-validator") ? std::string() : std::string("files/validator.cpp");
    p.checkerFile = customChecker ? std::string("files/checker.cpp") : std::string();
    p.interactor = p.interactive ? std::string("files/interactor.cpp") : std::string();
    p.testScript = "files/testscript.txt";
    p.generators = {"files/gen.cpp"};

    fs::mkdirs(p.filesDir());
    fs::mkdirs(p.solutionsDir());
    fs::mkdirs(p.testsDir());
    fs::mkdirs(p.statementsDir());
    fs::mkdirs(p.outputDir());
    fs::mkdirs(p.stressDir());

    const std::string& tl = testlib_source();
    if (tl.empty()) {
        log_warn("未能获取内置 testlib.h，files/testlib.h 未写入（请检查 testlib/testlib.h）");
    } else {
        fs::write_file(p.path("files/testlib.h"), tl);
    }

    fs::write_file(p.path("files/gen.cpp"), tpl_generator());
    if (!p.validator.empty()) fs::write_file(p.path(p.validator), tpl_validator());
    if (!p.checkerFile.empty()) fs::write_file(p.path(p.checkerFile), tpl_checker());
    if (!p.interactor.empty()) fs::write_file(p.path(p.interactor), tpl_interactor());
    fs::write_file(p.path(p.testScript), tpl_testscript());
    fs::write_file(fs::join(p.solutionsDir(), "main.cpp"), tpl_solution_main());
    fs::write_file(fs::join(p.statementsDir(), "statement.md"), tpl_statement());
    fs::write_file(fs::join(p.statementsDir(), "tutorial.md"), tpl_tutorial());
    fs::write_file(fs::join(p.dir, "README.md"), tpl_readme());

    sync_solutions(p);
    p.sampleTests = {1, 2};

    std::string err;
    if (!p.save(err)) {
        log_err(err);
        return 1;
    }

    log_ok(str("已创建题目 {} 于 {}", name, p.dir));
    log_info(str("  时限 {} ms，内存 {} MB，checker {}",
                 p.timeLimitMs, p.memoryLimitKb / 1024,
                 p.checkerFile.empty() ? p.checker : std::string("custom")));
    log_info("下一步：");
    log_info(str("  poly build {}", name));
    log_info(str("  poly gen {}", name));
    log_info(str("  poly validate {}", name));
    log_info(str("  poly test {}", name));
    return 0;
}

int cmd_list(Context& ctx, const Args& args) {
    (void)args;
    std::vector<std::string> names = list_problems(ctx.workspace);
    if (names.empty()) {
        log_info(str("工作区 {} 中没有题目。使用 poly init <名字> 创建。", ctx.workspace));
        return 0;
    }
    Column name{"NAME"}, tests{"TESTS"}, sols{"SOLS"}, tl{"TL"}, ml{"ML"}, chk{"CHECKER"}, inter{"INTERACTIVE"};
    for (const std::string& n : names) {
        Problem p;
        std::string err;
        if (!load_problem(ctx.workspace, n, p, err)) {
            name.cells.push_back(n);
            tests.cells.push_back("?");
            sols.cells.push_back("?");
            tl.cells.push_back("?");
            ml.cells.push_back("?");
            chk.cells.push_back("broken");
            inter.cells.push_back("-");
            continue;
        }
        name.cells.push_back(p.name);
        tests.cells.push_back(num(p.testCount()));
        sols.cells.push_back(num(static_cast<long long>(p.solutions.size())));
        tl.cells.push_back(num(p.timeLimitMs) + "ms");
        ml.cells.push_back(num(p.memoryLimitKb / 1024) + "MB");
        chk.cells.push_back(p.checkerFile.empty() ? p.checker : "custom");
        inter.cells.push_back(p.interactive ? "yes" : "no");
    }
    print_table({name, tests, sols, tl, ml, chk, inter});
    return 0;
}

int cmd_info(Context& ctx, const Args& args) {
    (void)args;
    Problem p;
    if (!ctx.requireProblem(p)) return 1;

    log_info(str("题目          {}", p.name));
    log_info(str("目录          {}", p.dir));
    log_info(str("时限/内存     {} ms / {} MB", p.timeLimitMs, p.memoryLimitKb / 1024));
    log_info(str("checker       {}", p.checkerFile.empty() ? p.checker : p.checkerFile + " (custom)"));
    log_info(str("交互题        {}", p.interactive ? "是" : "否"));
    if (!p.validator.empty()) log_info(str("validator     {}", p.validator));
    log_info(str("生成器        {}", p.generators.empty() ? std::string("(无)") : join(p.generators, ", ")));
    log_info(str("测试点        共 {} 个", p.testCount()));
    if (!p.sampleTests.empty()) log_info(str("样例点        {}", join([&] {
                 std::vector<std::string> v;
                 for (int t : p.sampleTests) v.push_back(num(t));
                 return v;
             }(), ", ")));

    if (!p.solutions.empty()) {
        log_raw("\n");
        Column n{"SOLUTION"}, f{"FILE"}, e{"EXPECTED"}, t{"TAG"};
        for (const SolutionInfo& s : p.solutions) {
            n.cells.push_back(s.name);
            f.cells.push_back(s.file);
            e.cells.push_back(s.expected);
            t.cells.push_back(s.tag.empty() ? "-" : s.tag);
        }
        print_table({n, f, e, t});
    }
    return 0;
}

int cmd_config(Context& ctx, const Args& args) {
    Problem p;
    if (!ctx.requireProblem(p)) return 1;

    std::vector<std::string> pos;
    for (const std::string& s : args.positional)
        if (s != ctx.problemName) pos.push_back(s);
    if (ctx.problemName.empty() || (pos.empty() && !args.has_flag("list"))) {
        log_err("用法: poly config <题目> <键> [值]");
        return 1;
    }
    if (args.has_flag("list") || pos.empty()) {
        log_info(str("name           {}", p.name));
        log_info(str("timeLimit      {} ms", p.timeLimitMs));
        log_info(str("memoryLimit    {} MB", p.memoryLimitKb / 1024));
        log_info(str("checker        {}", p.checker));
        log_info(str("checkerFile    {}", p.checkerFile.empty() ? "(未设置)" : p.checkerFile));
        log_info(str("validator      {}", p.validator.empty() ? "(未设置)" : p.validator));
        log_info(str("interactor     {}", p.interactor.empty() ? "(未设置)" : p.interactor));
        log_info(str("testScript     {}", p.testScript));
        log_info(str("interactive    {}", p.interactive ? "true" : "false"));
        log_info(str("multitest      {}", p.multitest ? "true" : "false"));
        log_info(str("note           {}", p.note));
        for (const SolutionInfo& s : p.solutions) {
            log_info(str("solution.{}.expected  {}", s.name, s.expected));
            log_info(str("solution.{}.tag       {}", s.name, s.tag.empty() ? "(未设置)" : s.tag));
        }
        return 0;
    }

    const std::string& key = pos[0];
    bool hasValue = pos.size() > 1;
    std::string value = hasValue ? join(std::vector<std::string>(pos.begin() + 1, pos.end()), " ") : "";

    auto lower = to_lower(key);
    bool ok = true;
    if (lower == "timelimit" || lower == "tl" || lower == "time-limit") {
        if (hasValue)
            p.timeLimitMs = static_cast<int>(to_int(value, p.timeLimitMs));
        else
            log_info(str("{}", p.timeLimitMs));
    } else if (lower == "memorylimit" || lower == "ml" || lower == "memory-limit") {
        if (hasValue)
            p.memoryLimitKb = parse_memory_mb(value, p.memoryLimitKb / 1024) * 1024;
        else
            log_info(str("{}", p.memoryLimitKb / 1024));
    } else if (lower == "checker") {
        if (hasValue) p.checker = value;
        log_info(str("{}", p.checker));
    } else if (lower == "checkerfile") {
        if (hasValue) p.checkerFile = (value == "none" || value.empty()) ? std::string() : value;
        log_info(str("{}", p.checkerFile.empty() ? "(未设置)" : p.checkerFile));
    } else if (lower == "validator") {
        if (hasValue) p.validator = (value == "none" || value.empty()) ? std::string() : value;
        log_info(str("{}", p.validator.empty() ? "(未设置)" : p.validator));
    } else if (lower == "interactor") {
        if (hasValue) p.interactor = (value == "none" || value.empty()) ? std::string() : value;
        log_info(str("{}", p.interactor.empty() ? "(未设置)" : p.interactor));
    } else if (lower == "testscript") {
        if (hasValue) p.testScript = value;
        log_info(str("{}", p.testScript));
    } else if (lower == "interactive" || lower == "multitest") {
        bool b = hasValue ? (value == "1" || to_lower(value) == "true" || to_lower(value) == "yes") : false;
        if (hasValue) {
            if (lower == "interactive") p.interactive = b;
            else p.multitest = b;
        }
        log_info(str("{}", (lower == "interactive" ? p.interactive : p.multitest) ? "true" : "false"));
    } else if (lower == "note") {
        if (hasValue) p.note = value;
        log_info(str("{}", p.note));
    } else if (starts_with(lower, "solution.")) {
        std::vector<std::string> parts = split(key, '.');
        if (parts.size() < 3) {
            log_err("键格式应为 solution.<名字>.<expected|tag|points|file>");
            return 1;
        }
        std::string sname = parts[1];
        std::string field = to_lower(parts[2]);
        SolutionInfo* target = nullptr;
        for (SolutionInfo& s : p.solutions)
            if (s.name == sname) target = &s;
        if (!target) {
            log_err(str("解法 {} 不存在", sname));
            return 1;
        }
        if (!hasValue) {
            if (field == "expected") log_info(str("{}", target->expected));
            else if (field == "tag") log_info(str("{}", target->tag));
            else if (field == "file") log_info(str("{}", target->file));
            else if (field == "points") log_info(str("{}", target->points));
            else { log_err(str("未知字段 {}", field)); return 1; }
        } else if (field == "expected") {
            target->expected = to_lower(value);
        } else if (field == "tag") {
            target->tag = value;
        } else if (field == "file") {
            target->file = value;
        } else if (field == "points") {
            target->points = static_cast<int>(to_int(value, -1));
        } else {
            log_err(str("未知字段 {}", field));
            ok = false;
        }
    } else {
        log_err(str("未知配置键 {}（可用 poly config {} --list 查看）", key, p.name));
        return 1;
    }

    if (!ok) return 1;
    if (hasValue) {
        std::string err;
        if (!p.save(err)) {
            log_err(err);
            return 1;
        }
        log_ok(str("{} = {}", key, value));
    }
    return 0;
}

int cmd_clean(Context& ctx, const Args& args) {
    Problem p;
    if (!ctx.requireProblem(p)) return 1;
    bool all = args.has_flag("all");
    bool did = false;
    if (all || args.has_flag("tests")) {
        fs::remove_all(p.testsDir());
        fs::mkdirs(p.testsDir());
        log_ok("已清空 tests/");
        did = true;
    }
    if (all || args.has_flag("stress")) {
        fs::remove_all(p.stressDir());
        log_ok("已清空 stress/");
        did = true;
    }
    if (all || (!did) ) {
        fs::remove_all(p.outputDir());
        fs::mkdirs(p.outputDir());
        log_ok("已清空 output/");
        did = true;
    }
    return 0;
}

int cmd_doctor(Context& ctx, const Args& args) {
    (void)args;
    log_step("环境检查");
    log_info(str("poly 版本     {}", POLY_VERSION));
    log_info(str("可执行文件    {}", fs::exe_path()));
    log_info(str("工作区        {}", ctx.workspace));
    log_info(str("平台          {}", POLY_PLATFORM));

    int problems = 0;
    if (!fs::is_dir(ctx.workspace)) {
        log_warn(str("工作区尚不存在（poly init 会自动创建）"));
    } else if (fs::is_file(fs::join(ctx.workspace, "poly.json"))) {
        log_ok("工作区配置 poly.json 存在");
    }
    problems = static_cast<int>(list_problems(ctx.workspace).size());
    log_info(str("已有题目      {} 个", problems));

    Compiler cc;
    std::string cerr;
    if (resolve_compiler(ctx.compiler, ctx.stdFlag, cc, cerr)) {
        log_ok(str("编译器可用：{}", cc.exe));
        if (!cc.version.empty()) log_info(str("  版本  {}", cc.version));
    } else {
        log_err(cerr);
    }

    const std::string& tl = testlib_source();
    if (!tl.empty()) {
        log_ok(str("testlib.h 可用（{} 字节{}）", tl.size(),
                   testlib_disk_path().empty() ? "，来自内置副本" : "，来自 " + testlib_disk_path()));
    } else {
        log_err("testlib.h 不可用：既没有内置副本，也没有在磁盘上找到 testlib/testlib.h");
    }

    std::string python = env_get("POLY_PYTHON");
    log_info(str("Python        {}", python.empty() ? "(用于构建期嵌入 testlib，可选项)" : python));
    return 0;
}

int cmd_testlib(Context& ctx, const Args& args) {
    const std::string& tl = testlib_source();
    if (tl.empty()) {
        log_err("testlib.h 不可用");
        return 1;
    }
    if (args.has_flag("print")) {
        log_raw(tl);
        return 0;
    }
    std::string out = args.get("output");
    if (out.empty()) {
        if (!args.positional.empty()) out = args.positional[0];
    }
    if (out.empty()) out = fs::join(ctx.workspace, "testlib.h");
    if (fs::is_dir(out)) out = fs::join(out, "testlib.h");
    if (!ends_with(out, ".h")) out = fs::join(out, "testlib.h");
    if (!fs::write_file(out, tl)) {
        log_err(str("无法写入 {}", out));
        return 1;
    }
    log_ok(str("已写出 testlib.h（{} 字节）到 {}", tl.size(), out));
    return 0;
}

}  // namespace poly
