// 打包导出：Polygon / QDUOJ / FPS / Hydro / HOJ 五种题目包格式。
//
// 外部格式的字段约定来自各 OJ 的实现（互为参考，均已在本机核对过源码）：
//   · QDUOJ  zip：`<n>/problem.json` + `<n>/testcase/*.in|*.out`
//     （QingdaoU/OnlineJudge problem/views/admin.py 的 ExportProblemAPI / ImportProblemAPI）
//   · FPS    xml：freeproblemset（zhblue/freeproblemset，DTD fps.current.dtd）
//     HUSTOJ 与 QDUOJ 都会读它；QDUOJ 的解析器只接受 version 1.1 / 1.2
//   · Hydro  zip：`<pid>/problem.yaml` + `problem.md` + `testdata/`
//     （hydro-dev/Hydro packages/hydrooj/src/model/problem.ts 的 import/export）
//   · HOJ    zip：`problem_<id>.json` + `problem_<id>/*.in|*.out`
//     （HimitZH/HOJ hoj-springboot/DataBackup/.../manager/file/ProblemFileManager.java
//       的 importProblem / exportProblem，格式即后端 VO ImportProblemVO）
//
// 公共部分（题面切分、测试点收集）在下面独立出来，避免各格式各写一套取数逻辑。
#include <algorithm>
#include <map>
#include <set>

#include "exporter.h"
#include "format.h"
#include "fsutil.h"
#include "judge.h"
#include "json.h"
#include "log.h"
#include "markdown.h"
#include "pipeline.h"
#include "script.h"
#include "strutil.h"
#include "table.h"
#include "zip.h"

namespace poly {

namespace {

const char* kFpsDtdUrl = "http://hustoj.com/fps.current.dtd";
const char* kFpsRepoUrl = "https://github.com/zhblue/freeproblemset/";
// QDUOJ 的 FPS 解析器只认 1.1/1.2，HUSTOJ 与 Hydro 不校验版本，故统一写 1.2。
const char* kFpsVersion = "1.2";

// 题面缺少某一节时的占位文本（QDUOJ 的导入要求这些字段非空）。
const char* kMissingSection = "<p>无</p>";

// ---------------------------------------------------------------- XML 小工具

std::string xml_escape(const std::string& s) {
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

// CDATA 段：内部的 "]]>" 必须拆开，否则会提前结束 CDATA。
std::string cdata(const std::string& s) {
    std::string clean;
    clean.reserve(s.size());
    for (char c : s) {
        unsigned char u = static_cast<unsigned char>(c);
        if (u == 0x1a) continue;  // HUSTOJ 导出时同样会剔除这个杂字符
        if (u < 0x20 && c != '\n' && c != '\r' && c != '\t') continue;
        clean.push_back(c);
    }
    return "<![CDATA[" + replace_all(clean, "]]>", "]]]]><![CDATA[>") + "]]>";
}

std::string xml_text(const std::string& tag, const std::string& body) {
    return "<" + tag + ">" + cdata(body) + "</" + tag + ">\n";
}

// ---------------------------------------------------------------- 语言 / YAML

std::string language_of(const std::string& path) {
    std::string ext = to_lower(fs::extension(path));
    if (ext == "cpp" || ext == "cc" || ext == "cxx" || ext == "c++") return "C++";
    if (ext == "c") return "C";
    if (ext == "py") return "Python";
    if (ext == "java") return "Java";
    if (ext == "pas") return "Pascal";
    if (ext == "js") return "JavaScript";
    if (ext == "go") return "Go";
    if (ext == "rs") return "Rust";
    if (ext == "cs") return "C#";
    if (ext == "rb") return "Ruby";
    return "C++";
}

// YAML 双引号标量：可安全承载任意标题（含冒号、引号、中文）。
std::string yaml_string(const std::string& s) {
    std::string out = "\"";
    out.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(c);
        }
    }
    out += "\"";
    return out;
}

// ---------------------------------------------------------------- 题面切分

struct Sample {
    std::string input;
    std::string output;
};

struct StatementSections {
    std::string title;
    std::string description;
    std::string input;
    std::string output;
    std::string hint;
    std::string full;  // 原始 markdown
};

enum class Bucket { Skip, Description, Input, Output, Hint };

// 返回 (归属字段, 是否保留标题行)。保留标题是为了不丢「数据范围」这类小节名。
std::pair<Bucket, bool> classify_heading(const std::string& name) {
    if (contains(name, "样例解释") || contains(name, "样例说明")) return {Bucket::Hint, true};
    if (contains(name, "样例")) return {Bucket::Skip, false};
    for (const char* w : {"数据范围", "提示", "说明", "注意", "约定", "子任务", "评分"}) {
        if (contains(name, w)) return {Bucket::Hint, true};
    }
    if (contains(name, "输入")) return {Bucket::Input, false};
    if (contains(name, "输出")) return {Bucket::Output, false};
    if (contains(name, "描述") || contains(name, "背景") || contains(name, "题意"))
        return {Bucket::Description, false};
    return {Bucket::Description, true};
}

std::string clean_marker(const std::string& s) {
    return trim(replace_all(s, "{{samples}}", ""));
}

StatementSections split_statement(const std::string& md) {
    StatementSections out;
    out.full = md;

    Bucket cur = Bucket::Description;
    std::string buf;
    bool seenTitle = false;

    auto flush = [&]() {
        std::string text = clean_marker(buf);
        buf.clear();
        if (text.empty() || cur == Bucket::Skip) return;
        std::string* target = &out.description;
        if (cur == Bucket::Input) target = &out.input;
        else if (cur == Bucket::Output) target = &out.output;
        else if (cur == Bucket::Hint) target = &out.hint;
        if (!target->empty()) *target += "\n\n";
        *target += text;
    };

    for (const std::string& line : split_lines(strip_cr(md))) {
        size_t hashes = 0;
        while (hashes < line.size() && line[hashes] == '#') hashes++;
        bool isHeading = hashes > 0 && hashes <= 6 && (hashes == line.size() || line[hashes] == ' ');
        if (!isHeading) {
            buf += line;
            buf += "\n";
            continue;
        }
        std::string name = trim(line.substr(hashes));
        if (hashes == 1) {
            if (!seenTitle) {
                flush();
                seenTitle = true;
                out.title = name;
                cur = Bucket::Description;
                continue;
            }
            // 正文里再出现一级标题时，按普通内容处理。
        }
        if (hashes > 2) {
            buf += line;
            buf += "\n";
            continue;
        }
        flush();
        auto [bucket, keep] = classify_heading(name);
        cur = bucket;
        if (keep && bucket != Bucket::Skip) buf = "## " + name + "\n";
    }
    flush();
    return out;
}

std::string section_html(const std::string& md) {
    if (trim(md).empty()) return {};
    return trim(markdown_to_html(md));
}

// markdown_to_html 已经处理过数学公式的透传，这里只做空值兜底。
std::string section_html_or_default(const std::string& md, bool& missing, const char* fallback) {
    std::string html = section_html(md);
    if (html.empty()) {
        missing = true;
        return fallback;
    }
    return html;
}

// ---------------------------------------------------------------- 测试点收集

struct TestItem {
    int index = 0;        // 题目内的测试点编号
    std::string inRel;    // 相对题目目录的输入文件
    std::string outRel;   // 相对题目目录的答案文件（可能不存在）
    std::string inName;   // 包内文件名：1.in
    std::string outName;  // 包内文件名：1.out
    bool hasInput = false;
    bool hasOutput = false;
};

std::vector<TestItem> collect_test_items(const Problem& p, const std::vector<int>& tests) {
    std::vector<TestItem> items;
    int ordinal = 0;
    for (int t : tests) {
        TestItem it;
        it.index = t;
        ordinal++;
        std::string base = p.testFileName(t);
        it.inRel = fs::join("tests", base);
        it.outRel = answer_rel(t, p);
        it.inName = num(ordinal) + ".in";
        it.outName = num(ordinal) + ".out";
        it.hasInput = fs::is_file(p.path(it.inRel));
        // 交互题的标准答案由交互器现场决定，output/answers 里的空文件不算答案。
        it.hasOutput = !p.interactive && fs::is_file(p.path(it.outRel));
        items.push_back(it);
    }
    return items;
}

std::vector<Sample> collect_samples(const Problem& p) {
    std::vector<Sample> samples;
    std::vector<int> idxs = p.sampleTests;
    if (idxs.empty()) {
        for (int t : p.testIndices()) {
            idxs.push_back(t);
            if (idxs.size() >= 2) break;
        }
    }
    for (int t : idxs) {
        Sample s;
        fs::read_file(p.path(fs::join("tests", p.testFileName(t))), s.input);
        if (!p.interactive) fs::read_file(p.path(answer_rel(t, p)), s.output);
        if (trim(s.input).empty()) continue;
        samples.push_back(s);
    }
    return samples;
}

// Hydro 的 markdown 渲染把 ```input1 / ```output1 代码块识别成样例。
std::string markdown_with_samples(const std::string& md, const std::vector<Sample>& samples) {
    std::string block;
    for (size_t i = 0; i < samples.size(); i++) {
        block += str("```input{}\n{}\n```\n\n", i + 1, rtrim(strip_cr(samples[i].input)));
        if (!trim(samples[i].output).empty()) {
            block += str("```output{}\n{}\n```\n\n", i + 1, rtrim(strip_cr(samples[i].output)));
        }
    }
    if (md.find("{{samples}}") != std::string::npos) return replace_all(md, "{{samples}}", block);
    if (block.empty()) return md;
    return md + "\n## 样例\n\n" + block;
}

// ---------------------------------------------------------------- 公共取数

struct ExportView {
    StatementSections stmt;
    std::vector<Sample> samples;
    std::vector<TestItem> items;
    bool missingDescription = false;
    bool missingInput = false;
    bool missingOutput = false;
    std::string descriptionHtml;
    std::string inputHtml;
    std::string outputHtml;
    std::string hintHtml;
    std::string source;      // 标签串
    long long memoryMb = 256;
};

bool build_view(const Problem& p, const std::vector<int>& tests, ExportView& view, std::string& err) {
    std::string md;
    if (!fs::read_file(fs::join(p.statementsDir(), "statement.md"), md)) {
        err = str("找不到题面 {}", fs::join(p.statementsDir(), "statement.md"));
        return false;
    }
    view.stmt = split_statement(md);
    view.samples = collect_samples(p);
    view.items = collect_test_items(p, tests);
    if (view.items.empty()) {
        err = "没有测试点（先执行 poly gen）";
        return false;
    }
    view.descriptionHtml = section_html_or_default(view.stmt.description, view.missingDescription,
                                                  kMissingSection);
    view.inputHtml = section_html_or_default(view.stmt.input, view.missingInput, kMissingSection);
    view.outputHtml = section_html_or_default(view.stmt.output, view.missingOutput, kMissingSection);
    view.hintHtml = section_html(view.stmt.hint);
    view.source = join(p.tags, " ");
    view.memoryMb = std::max<long long>(1, (p.memoryLimitKb + 1023) / 1024);
    if (view.missingDescription) {
        // 描述整段缺失时退回全文，至少不会出现「无」。
        std::string whole = section_html(md);
        if (!whole.empty()) view.descriptionHtml = whole;
    }
    return true;
}

void warn_missing_sections(const Problem& p, const ExportView& view) {
    std::vector<std::string> missing;
    if (view.missingDescription) missing.push_back("题目描述");
    // 交互题本来就没有传统意义上的输入/输出节，不提醒。
    if (!p.interactive) {
        if (view.missingInput) missing.push_back("输入格式");
        if (view.missingOutput) missing.push_back("输出格式");
    }
    if (!missing.empty()) {
        log_warn(str("题面缺少小节：{}（已用占位内容，建议补全 statements/statement.md）",
                     join(missing, "、")));
    }
}

// special judge 的源码：自定义 checker 优先，没有时交互题退回交互器
// （QDUOJ 的交互题就是走 spj 通道；HUSTOJ 见到 <interactor> 会自行按交互题处理）。
std::string spj_source(const Problem& p) {
    if (!p.checkerFile.empty() && fs::is_file(p.path(p.checkerFile))) return p.checkerFile;
    if (p.interactive && !p.interactor.empty() && fs::is_file(p.path(p.interactor)))
        return p.interactor;
    return std::string();
}

// 供 FPS / QDUOJ 使用的可导入样例：两侧都要有内容。
std::vector<Sample> importable_samples(const std::vector<Sample>& samples) {
    std::vector<Sample> out;
    for (const Sample& s : samples) {
        if (trim(s.input).empty() || trim(s.output).empty()) continue;
        out.push_back(s);
    }
    return out;
}

// ---------------------------------------------------------------- 输出路径

std::string resolve_output(const Context& ctx, const Problem& p, ExportFormat format,
                           const Args& args) {
    std::string name = default_export_name(p, format);
    std::string out = args.get("output");
    if (out.empty()) out = fs::join(ctx.workspace, name);
    else if (fs::is_dir(out) || ends_with(out, "/") || ends_with(out, "\\"))
        out = fs::join(out, name);
    return fs::absolute(out);
}

// ================================================================ Polygon

std::map<int, std::string> script_commands_by_test(const Problem& p) {
    std::map<int, std::string> out;
    std::string text;
    if (!fs::read_file(p.path(p.testScript), text)) return out;
    Script script;
    std::string err;
    if (!parse_test_script(text, script, err)) return out;
    int autoIndex = 1;
    for (const ScriptCommand& cmd : script.commands) {
        int from = cmd.hasTarget ? cmd.from : autoIndex;
        int to = cmd.hasTarget ? cmd.to : autoIndex;
        std::string cmdText;
        for (const std::vector<std::string>& stage : cmd.stages) {
            if (!cmdText.empty()) cmdText += " | ";
            cmdText += join(stage, " ");
        }
        for (int idx = from; idx <= to; idx++) {
            out[idx] = cmdText;
            if (!cmd.hasTarget) autoIndex = idx + 1;
        }
        if (cmd.hasTarget && to >= autoIndex) autoIndex = to + 1;
    }
    return out;
}

std::string build_problem_xml(const Problem& p, const std::vector<int>& tests) {
    std::map<int, std::string> cmds = script_commands_by_test(p);
    std::set<int> samples(p.sampleTests.begin(), p.sampleTests.end());

    std::string x;
    x += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    x += "<problem short-name=\"" + xml_escape(p.name) + "\">\n";
    x += "  <names>\n";
    x += "    <name language=\"chinese\" value=\"" + xml_escape(p.name) + "\"/>\n";
    x += "    <name language=\"english\" value=\"" + xml_escape(p.name) + "\"/>\n";
    x += "  </names>\n";

    x += "  <statements>\n";
    if (fs::is_file(p.path("statements/statement.md")))
        x += "    <statement path=\"statements/statement.md\" type=\"text/markdown\"/>\n";
    if (fs::is_file(p.path("statements/statement.pdf")))
        x += "    <statement path=\"statements/statement.pdf\" type=\"application/pdf\"/>\n";
    if (fs::is_file(p.path("statements/statement.html")))
        x += "    <statement path=\"statements/statement.html\" type=\"text/html\"/>\n";
    if (fs::is_file(p.path("statements/tutorial.md")))
        x += "    <tutorial path=\"statements/tutorial.md\" type=\"text/markdown\"/>\n";
    x += "  </statements>\n";

    x += "  <judging>\n";
    x += "    <testset name=\"tests\">\n";
    x += str("      <time-limit>{}</time-limit>\n", p.timeLimitMs);
    x += str("      <memory-limit>{}</memory-limit>\n", p.memoryLimitKb * 1024);
    x += str("      <test-count>{}</test-count>\n", tests.size());
    x += "      <input-path-pattern>tests/%s</input-path-pattern>\n";
    x += "      <answer-path-pattern>tests/%s.a</answer-path-pattern>\n";
    x += "      <tests>\n";
    for (int t : tests) {
        std::string name = p.testFileName(t);
        std::string cmd = cmds.count(t) ? cmds[t] : std::string("gen");
        x += str("        <test method=\"generated\" sample=\"{}\" cmd=\"{}\">tests/{}</test>\n",
                 samples.count(t) ? "true" : "false", xml_escape(cmd), xml_escape(name));
    }
    x += "      </tests>\n";
    x += "    </testset>\n";
    x += "  </judging>\n";

    x += "  <files>\n";
    x += "    <resources>\n";
    for (const std::string& g : p.generators) {
        if (fs::is_file(p.path(g))) x += "      <resource path=\"" + xml_escape(g) + "\"/>\n";
    }
    if (!p.testScript.empty() && fs::is_file(p.path(p.testScript)))
        x += "      <resource path=\"" + xml_escape(p.testScript) + "\"/>\n";
    x += "    </resources>\n";
    x += "    <executables/>\n";
    x += "  </files>\n";

    if (p.checkerFile.empty()) {
        x += "  <checker type=\"standard\" name=\"" + xml_escape(p.checker) + "\"/>\n";
    } else {
        x += "  <checker type=\"testlib\" source=\"" + xml_escape(p.checkerFile) + "\"/>\n";
    }
    if (!p.validator.empty() && fs::is_file(p.path(p.validator)))
        x += "  <validator source=\"" + xml_escape(p.validator) + "\"/>\n";
    if (p.interactive && !p.interactor.empty() && fs::is_file(p.path(p.interactor)))
        x += "  <interactor source=\"" + xml_escape(p.interactor) + "\"/>\n";

    x += "  <solutions>\n";
    for (const SolutionInfo& s : p.solutions) {
        if (!fs::is_file(p.path(s.file))) continue;
        std::string tag = s.tag.empty() ? (s.expected == "ac" ? "correct" : "wrong") : s.tag;
        x += "    <solution tag=\"" + xml_escape(tag) + "\" source=\"" + xml_escape(s.file) + "\"/>\n";
    }
    x += "  </solutions>\n";
    x += "</problem>\n";
    return x;
}

bool export_polygon(const Context& ctx, const Problem& p, const std::vector<int>& tests,
                    const Args& args, const std::vector<TestItem>& items) {
    std::string outPath = resolve_output(ctx, p, ExportFormat::Polygon, args);
    log_step(str("导出 Polygon 兼容包（{} 个测试点）", tests.size()));

    ZipWriter zip;
    if (!zip.open(outPath)) {
        log_err(zip.error());
        return false;
    }
    zip.add("problem.xml", build_problem_xml(p, tests));
    for (const TestItem& it : items) {
        if (it.hasInput) zip.add_file("tests/" + p.testFileName(it.index), p.path(it.inRel));
        if (it.hasOutput) zip.add_file("tests/" + p.testFileName(it.index) + ".a", p.path(it.outRel));
    }
    for (const SolutionInfo& s : p.solutions) {
        if (fs::is_file(p.path(s.file))) zip.add_file(s.file, p.path(s.file));
    }
    for (const std::string& rel :
         {p.checkerFile, p.validator, p.interactor, p.testScript, std::string("files/testlib.h")}) {
        if (!rel.empty() && fs::is_file(p.path(rel))) zip.add_file(rel, p.path(rel));
    }
    for (const std::string& g : p.generators) {
        if (fs::is_file(p.path(g))) zip.add_file(g, p.path(g));
    }
    static const char* const kExtraFiles[] = {"statements/statement.md", "statements/tutorial.md",
                                             "statements/statement.html", "statements/tutorial.html",
                                             "statements/statement.tex", "statements/statement.pdf",
                                             "README.md"};
    for (const char* rel : kExtraFiles) {
        if (fs::is_file(p.path(rel))) zip.add_file(rel, p.path(rel));
    }
    if (!zip.close()) {
        log_err(zip.error());
        return false;
    }
    log_ok(str("已生成 {}（{} 个文件，{}）", outPath, zip.entries(), human_size(fs::file_size(outPath))));
    return true;
}

// ================================================================ FPS

// 选作 <solution> 的解法：标程优先，其次任何期望 AC 的解法；按语言去重。
std::vector<const SolutionInfo*> fps_solutions(const Problem& p) {
    std::vector<const SolutionInfo*> out;
    std::set<std::string> langs;
    for (int pass = 0; pass < 2; pass++) {
        for (const SolutionInfo& s : p.solutions) {
            bool main = s.tag == "main" || s.name == "main";
            if (pass == 0 && !main) continue;
            if (pass == 1 && (main || s.expected != "ac")) continue;
            if (!fs::is_file(p.path(s.file))) continue;
            std::string lang = language_of(s.file);
            if (!langs.insert(lang).second) continue;
            out.push_back(&s);
        }
    }
    return out;
}

bool export_fps(const Context& ctx, const Problem& p, const std::vector<int>& tests, const Args& args,
                const ExportView& view) {
    (void)tests;
    std::string outPath = resolve_output(ctx, p, ExportFormat::Fps, args);
    warn_missing_sections(p, view);

    std::string x;
    x += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    x += "<!DOCTYPE fps PUBLIC\n";
    x += "  \"-//freeproblemset//An opensource XML standard for AlgorithmContest Problem Set//EN\"\n";
    x += str("  \"{}\" >\n", kFpsDtdUrl);
    x += str("<fps version=\"{}\" url=\"{}\">\n", kFpsVersion, kFpsRepoUrl);
    x += "<generator name=\"poly\" />\n";
    x += "<item>\n";
    x += xml_text("title", view.stmt.title.empty() ? p.name : view.stmt.title);

    if (p.timeLimitMs % 1000 == 0) {
        x += str("<time_limit unit=\"s\">{}</time_limit>\n", p.timeLimitMs / 1000);
    } else {
        x += str("<time_limit unit=\"ms\">{}</time_limit>\n", p.timeLimitMs);
    }
    x += str("<memory_limit unit=\"mb\">{}</memory_limit>\n", view.memoryMb);

    x += xml_text("description", view.descriptionHtml);
    x += xml_text("input", view.inputHtml);
    x += xml_text("output", view.outputHtml);

    for (const Sample& s : importable_samples(view.samples)) {
        x += xml_text("sample_input", strip_cr(s.input));
        x += xml_text("sample_output", strip_cr(s.output));
    }

    // 测试数据内嵌在 CDATA 里；name 用「序号」而非「序号.in」：
    // HUSTOJ 会拼成 name+".in"，Hydro 会拼成 name+".in"（无 filename 属性时）。
    // 每个 <test_input> 后面必须紧跟同名 <test_output>：QDUOJ 的解析器会在
    // 连续两个 test_input 时报 "error 'test_input' tag order"。
    bool anyOutput = false;
    for (const TestItem& it : view.items) {
        std::string in, out;
        fs::read_file(p.path(it.inRel), in);
        if (it.hasOutput) {
            fs::read_file(p.path(it.outRel), out);
            anyOutput = true;
        }
        std::string id = fs::stem(it.inName);
        x += str("<test_input name=\"{}\">{}</test_input>\n", id, cdata(in));
        x += str("<test_output name=\"{}\">{}</test_output>\n", id, cdata(out));
    }
    if (!anyOutput) log_warn("该题目没有答案文件，FPS 包内的 <test_output> 为空（交互题通常如此）");

    if (!trim(view.hintHtml).empty()) x += xml_text("hint", view.hintHtml);
    if (!view.source.empty()) x += xml_text("source", view.source);

    for (const SolutionInfo* s : fps_solutions(p)) {
        std::string code = fs::read_file(p.path(s->file));
        if (code.empty()) continue;
        x += str("<solution language=\"{}\">{}</solution>\n", language_of(s->file), cdata(code));
    }

    std::string spjFile = spj_source(p);
    if (!spjFile.empty()) {
        std::string code = fs::read_file(p.path(spjFile));
        x += str("<spj language=\"{}\">{}</spj>\n", language_of(spjFile), cdata(code));
    }
    if (p.interactive && !p.interactor.empty() && fs::is_file(p.path(p.interactor))) {
        std::string code = fs::read_file(p.path(p.interactor));
        x += str("<interactor language=\"{}\">{}</interactor>\n", language_of(p.interactor),
                 cdata(code));
    }

    x += "</item>\n";
    x += "</fps>\n";

    log_step(str("导出 FPS（{} 个测试点）", view.items.size()));
    if (!fs::write_file(outPath, x)) {
        log_err(str("无法写入 {}", outPath));
        return false;
    }
    log_ok(str("已生成 {}（{}）", outPath, human_size(fs::file_size(outPath))));
    return true;
}

// ================================================================ QDUOJ

// 由题目名派生一个两边都接受的题目 ID：
//   · QDUOJ 用 display_id 作为题目 _id（长度上限 24）
//   · Hydro 的 pid 必须匹配 /^(?:[a-z0-9]{1,10}-)?[a-z][0-9a-z]*$/i
// 因此只保留字母数字，并在首字符不是字母时补一个 P；非法名会被对方自动分配 ID，
// 这里尽量给出一个稳定的候选值。
std::string sanitize_pid(const std::string& name) {
    std::string out;
    for (char c : name) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
            out.push_back(c);
    }
    if (out.size() > 24) out = out.substr(0, 24);
    if (out.empty()) return "P1";
    if (!((out[0] >= 'A' && out[0] <= 'Z') || (out[0] >= 'a' && out[0] <= 'z'))) out = "P" + out;
    if (out.size() > 24) out = out.substr(0, 24);
    return out;
}

bool export_qduoj(const Context& ctx, const Problem& p, const std::vector<int>& tests,
                  const Args& args, const ExportView& view) {
    (void)tests;
    std::string outPath = resolve_output(ctx, p, ExportFormat::Qduoj, args);
    warn_missing_sections(p, view);

    // 没有答案文件时（含交互题）QDUOJ 侧按 spj 处理。
    std::string spjFile = spj_source(p);
    bool spj = !spjFile.empty();
    for (const TestItem& it : view.items) {
        if (!it.hasOutput) spj = true;
    }

    Json root = Json::object();
    root.set("display_id", Json(sanitize_pid(p.name)));
    root.set("title", Json(view.stmt.title.empty() ? p.name : view.stmt.title));
    auto html_value = [](const std::string& html) {
        Json value = Json::object();
        value.set("format", Json(std::string("html")));
        value.set("value", Json(html));
        return value;
    };
    root.set("description", html_value(view.descriptionHtml));
    root.set("input_description", html_value(view.inputHtml));
    root.set("output_description", html_value(view.outputHtml));
    root.set("hint", html_value(view.hintHtml));
    root.set("time_limit", Json(static_cast<long long>(p.timeLimitMs)));
    root.set("memory_limit", Json(view.memoryMb));
    root.set("rule_type", Json(std::string("ACM")));
    root.set("source", Json(view.source));
    Json tags = Json::array();
    for (const std::string& t : p.tags) tags.push_back(Json(t));
    root.set("tags", tags);
    root.set("template", Json::object());
    Json answers = Json::array();
    root.set("answers", answers);

    Json samples = Json::array();
    for (const Sample& s : importable_samples(view.samples)) {
        Json item = Json::object();
        item.set("input", Json(strip_cr(s.input)));
        item.set("output", Json(strip_cr(s.output)));
        samples.push_back(item);
    }
    root.set("samples", samples);

    if (spj && !spjFile.empty()) {
        Json spjNode = Json::object();
        spjNode.set("language", Json(language_of(spjFile)));
        spjNode.set("code", Json(fs::read_file(p.path(spjFile))));
        root.set("spj", spjNode);
    } else {
        root.set("spj", Json(nullptr));
    }

    Json scores = Json::array();
    for (const TestItem& it : view.items) {
        Json item = Json::object();
        item.set("score", Json(static_cast<long long>(100)));
        item.set("input_name", Json(it.inName));
        item.set("output_name", Json(it.hasOutput ? it.outName : std::string("-")));
        scores.push_back(item);
    }
    root.set("test_case_score", scores);

    log_step(str("导出 QDUOJ 压缩包（{} 个测试点{}）", view.items.size(),
                 spj ? "，spj" : ""));
    ZipWriter zip;
    if (!zip.open(outPath)) {
        log_err(zip.error());
        return false;
    }
    zip.add("1/problem.json", root.dump(2) + "\n");
    for (const TestItem& it : view.items) {
        if (it.hasInput) zip.add_file("1/testcase/" + it.inName, p.path(it.inRel));
        if (it.hasOutput) zip.add_file("1/testcase/" + it.outName, p.path(it.outRel));
    }
    if (!zip.close()) {
        log_err(zip.error());
        return false;
    }
    log_ok(str("已生成 {}（{} 个文件，{}）", outPath, zip.entries(), human_size(fs::file_size(outPath))));
    return true;
}

// ================================================================ Hydro

bool export_hydro(const Context& ctx, const Problem& p, const std::vector<int>& tests,
                  const Args& args, const ExportView& view) {
    (void)tests;
    std::string outPath = resolve_output(ctx, p, ExportFormat::Hydro, args);
    std::string pid = sanitize_pid(p.name);

    bool customChecker = !p.checkerFile.empty() && fs::is_file(p.path(p.checkerFile));
    bool customInteractor = p.interactive && !p.interactor.empty() && fs::is_file(p.path(p.interactor));

    std::string problemYaml;
    problemYaml += str("pid: {}\n", yaml_string(pid));
    problemYaml += str("title: {}\n", yaml_string(view.stmt.title.empty() ? p.name : view.stmt.title));
    problemYaml += "tag: [";
    for (size_t i = 0; i < p.tags.size(); i++) {
        if (i) problemYaml += ", ";
        problemYaml += yaml_string(p.tags[i]);
    }
    problemYaml += "]\n";

    std::string configYaml;
    configYaml += str("time: {}ms\n", p.timeLimitMs);
    configYaml += str("memory: {}m\n", view.memoryMb);
    if (customInteractor) configYaml += "type: interactive\n";
    if (customChecker) {
        configYaml += "checker_type: testlib\n";
        configYaml += str("checker: {}\n", yaml_string(fs::basename(p.checkerFile)));
    }
    if (customInteractor) {
        configYaml += str("interactor: {}\n", yaml_string(fs::basename(p.interactor)));
    }

    // 每个测试点一个子任务，分数合计 100。
    size_t n = view.items.size();
    configYaml += "subtasks:\n";
    for (size_t i = 0; i < n; i++) {
        long long score = static_cast<long long>(100 / (n ? n : 1));
        if (n && i < static_cast<size_t>(100 % n)) score += 1;
        if (score <= 0) score = 1;
        const TestItem& it = view.items[i];
        configYaml += str("  - score: {}\n    cases:\n", score);
        configYaml += str("      - input: {}\n", yaml_string(it.inName));
        configYaml += str("        output: {}\n",
                          yaml_string(it.hasOutput ? it.outName : std::string("/dev/null")));
    }

    std::string statement = markdown_with_samples(view.stmt.full, view.samples);

    log_step(str("导出 Hydro 压缩包（{} 个测试点）", n));
    ZipWriter zip;
    if (!zip.open(outPath)) {
        log_err(zip.error());
        return false;
    }
    std::string root = pid + "/";
    zip.add(root + "problem.yaml", problemYaml);
    zip.add(root + "problem.md", statement);
    zip.add(root + "testdata/config.yaml", configYaml);
    for (const TestItem& it : view.items) {
        if (it.hasInput) zip.add_file(root + "testdata/" + it.inName, p.path(it.inRel));
        if (it.hasOutput) zip.add_file(root + "testdata/" + it.outName, p.path(it.outRel));
    }
    if (customChecker) {
        zip.add(root + "testdata/" + fs::basename(p.checkerFile),
                fs::read_file(p.path(p.checkerFile)));
    }
    if (customInteractor) {
        zip.add(root + "testdata/" + fs::basename(p.interactor),
                fs::read_file(p.path(p.interactor)));
    }
    std::string tutorial = fs::read_file(fs::join(p.statementsDir(), "tutorial.md"));
    if (!trim(tutorial).empty()) zip.add(root + "solution/tutorial.md", tutorial);

    if (!zip.close()) {
        log_err(zip.error());
        return false;
    }
    log_ok(str("已生成 {}（{} 个文件，{}）", outPath, zip.entries(), human_size(fs::file_size(outPath))));
    return true;
}

// ================================================================ HOJ

// HOJ 的 examples 是一整串拼起来的样例块，前端用
//   /<input>([\s\S]*?)<\/input><output>([\s\S]*?)<\/output>/
// 解析（hoj-vue/src/common/utils.js 的 stringToExamples），所以 </input> 与 <output>
// 之间不能插进任何字符。样例正文不做转义：HOJ 前端是在 <pre>{{ }} 里插值的。
std::string hoj_examples(const std::vector<Sample>& samples) {
    std::string out;
    for (const Sample& s : samples) {
        out += "<input>";
        out += strip_cr(s.input);
        out += "</input><output>";
        out += strip_cr(s.output);
        out += "</output>";
    }
    return out;
}

// HOJ 的特判/交互程序共用 spj_code + spj_language，且语言必须是 language 表里
// is_spj=1 的那几个（自带库里只有 C / C++）。
std::string hoj_spj_language(const std::string& lang) {
    return to_lower(trim(lang)) == "c" ? "C" : "C++";
}

bool export_hoj(const Context& ctx, const Problem& p, const std::vector<int>& tests,
                const Args& args, const ExportView& view) {
    (void)tests;
    std::string outPath = resolve_output(ctx, p, ExportFormat::Hoj, args);
    std::string pid = sanitize_pid(p.name);
    // HOJ 靠「json 文件名 == 测试数据目录名」配对（见 ProblemFileManager.importProblem），
    // 两边必须用同一个 key。
    std::string key = "problem_" + pid;

    std::string spjFile;
    if (!p.checkerFile.empty() && fs::is_file(p.path(p.checkerFile))) spjFile = p.checkerFile;
    if (p.interactive && !p.interactor.empty() && fs::is_file(p.path(p.interactor)))
        spjFile = p.interactor;
    bool hasSpj = !spjFile.empty();
    std::string judgeMode = "default";
    if (p.interactive) judgeMode = "interactive";
    else if (hasSpj) judgeMode = "spj";

    // OI 计分：poly 里只要给某个解法标了 points，就按 OI 题型导出（测试点分数合计 100）。
    bool oi = false;
    for (const SolutionInfo& s : p.solutions) {
        if (s.points >= 0) oi = true;
    }

    // 难度：poly 的 problem.json 没有这个字段，留好 extra.difficulty 的口子（HOJ 取 0/1/2）。
    long long difficulty = 0;
    if (p.extra.is_object() && p.extra.has("difficulty")) {
        long long d = p.extra["difficulty"].as_int(-1);
        if (d >= 0 && d <= 3) difficulty = d;
    }

    log_step(str("导出 HOJ 原生包（{} 个测试点{}）", view.items.size(), oi ? "，OI 计分" : ""));
    if (hasSpj) log_info(str("判题模式 {}（跟随题目的 checker / 交互器）", judgeMode));

    Json problem = Json::object();
    problem.set("problemId", Json(pid));
    problem.set("title", Json(view.stmt.title.empty() ? p.name : view.stmt.title));
    problem.set("type", Json(static_cast<long long>(oi ? 1 : 0)));
    problem.set("timeLimit", Json(static_cast<long long>(p.timeLimitMs)));  // HOJ 单位 ms
    problem.set("memoryLimit", Json(p.memoryLimitKb));                     // HOJ 单位 kb
    problem.set("stackLimit", Json(static_cast<long long>(128)));
    problem.set("description", Json(view.descriptionHtml));
    problem.set("input", Json(view.inputHtml));
    problem.set("output", Json(view.outputHtml));
    problem.set("examples", Json(hoj_examples(view.samples)));
    problem.set("hint", Json(view.hintHtml));
    problem.set("source", Json(view.source));
    problem.set("difficulty", Json(difficulty));
    problem.set("auth", Json(static_cast<long long>(1)));  // 1 = 公开
    problem.set("ioScore", Json(static_cast<long long>(oi ? 100 : 0)));
    problem.set("codeShare", Json(true));
    problem.set("isRemote", Json(false));
    problem.set("isGroup", Json(false));
    problem.set("isUploadCase", Json(true));
    problem.set("isRemoveEndBlank", Json(true));
    problem.set("openCaseResult", Json(true));
    problem.set("judgeMode", Json(judgeMode));
    problem.set("judgeCaseMode", Json(std::string("default")));
    if (hasSpj) {
        problem.set("spjLanguage", Json(hoj_spj_language(language_of(spjFile))));
        problem.set("spjCode", Json(fs::read_file(p.path(spjFile))));
    }
    // author 故意不写：HOJ 发现为空会自动填成导入者用户名（author 列带外键指向 user_info，
    // 写一个库里不存在的用户名会让整次导入失败）。

    // HOJ 的 samples 存的是「判题数据文件名」，真正的数据在 <key>/ 目录里，由
    // initUploadTestCase 拷到判题目录；分数只在 OI 题型下有意义。
    Json samples = Json::array();
    size_t n = view.items.size();
    for (size_t i = 0; i < n; i++) {
        const TestItem& it = view.items[i];
        Json item = Json::object();
        item.set("input", Json(it.inName));
        item.set("output", Json(it.outName));
        if (oi) {
            long long score = static_cast<long long>(100 / (n ? n : 1));
            if (n && i < static_cast<size_t>(100 % n)) score += 1;
            item.set("score", Json(score > 0 ? score : 1));
            item.set("groupNum", Json(static_cast<long long>(1)));
        }
        samples.push_back(item);
    }

    Json tags = Json::array();
    for (const std::string& t : p.tags) tags.push_back(Json(t));

    // 字段一览对应后端的 ImportProblemVO：缺数组字段会让它 NPE，所以空也要写出来。
    Json root = Json::object();
    root.set("problem", problem);
    root.set("languages", Json::array());  // 空 = 允许全部语言（HOJ 会自动补齐）
    root.set("samples", samples);
    root.set("tags", tags);
    root.set("codeTemplates", Json::array());
    root.set("judgeMode", Json(judgeMode));

    ZipWriter zip;
    if (!zip.open(outPath)) {
        log_err(zip.error());
        return false;
    }
    // 压缩包根目录只能有 json / 目录成对出现，否则 HOJ 会报「文件格式错误，请使用json文件」。
    zip.add(key + ".json", root.dump(2) + "\n");
    for (const TestItem& it : view.items) {
        std::string input;
        if (it.hasInput) fs::read_file(p.path(it.inRel), input);
        zip.add(key + "/" + it.inName, input);
        std::string output;
        if (it.hasOutput) fs::read_file(p.path(it.outRel), output);
        // HOJ 的 initUploadTestCase 会给缺失的 .out 建空文件，这里直接写空，保持一一对应。
        zip.add(key + "/" + it.outName, output);
    }
    if (!zip.close()) {
        log_err(zip.error());
        return false;
    }
    log_ok(str("已生成 {}（{} 个文件，{}）", outPath, zip.entries(),
               human_size(fs::file_size(outPath))));
    log_info("在 HOJ 后台「题目管理 → 导入题目 → HOJ」上传这个 zip 即可。");
    return true;
}

}  // namespace

bool parse_export_format(const std::string& name, ExportFormat& out) {
    std::string v = to_lower(trim(name));
    if (v.empty() || v == "polygon" || v == "poly" || v == "pkg" || v == "default") {
        out = ExportFormat::Polygon;
        return true;
    }
    if (v == "qduoj" || v == "qd" || v == "qdu") {
        out = ExportFormat::Qduoj;
        return true;
    }
    if (v == "fps" || v == "freeproblemset" || v == "hustoj" || v == "xml") {
        out = ExportFormat::Fps;
        return true;
    }
    if (v == "hydro" || v == "hydrooj") {
        out = ExportFormat::Hydro;
        return true;
    }
    // HOJ 的原生包（后端 ImportProblemVO）；以前 hoj 被当成 Hydro 的别名，现已分开。
    if (v == "hoj" || v == "hcode" || v == "hoj-zip" || v == "hoj-native") {
        out = ExportFormat::Hoj;
        return true;
    }
    return false;
}

const char* export_format_name(ExportFormat format) {
    switch (format) {
        case ExportFormat::Qduoj: return "qduoj";
        case ExportFormat::Fps: return "fps";
        case ExportFormat::Hydro: return "hydro";
        case ExportFormat::Hoj: return "hoj";
        case ExportFormat::Polygon: break;
    }
    return "polygon";
}

std::string default_export_name(const Problem& p, ExportFormat format) {
    switch (format) {
        case ExportFormat::Qduoj: return p.name + ".qduoj.zip";
        case ExportFormat::Fps: return p.name + ".fps.xml";
        case ExportFormat::Hydro: return p.name + ".hydro.zip";
        case ExportFormat::Hoj: return p.name + ".hoj.zip";
        case ExportFormat::Polygon: break;
    }
    return p.name + ".zip";
}

bool export_problem(Context& ctx, Problem& p, JudgeEnv& env, const std::vector<int>& tests,
                    ExportFormat format, const Args& args) {
    (void)env;
    ExportView view;
    std::string err;
    if (!build_view(p, tests, view, err)) {
        log_err(err);
        return false;
    }
    switch (format) {
        case ExportFormat::Qduoj: return export_qduoj(ctx, p, tests, args, view);
        case ExportFormat::Fps: return export_fps(ctx, p, tests, args, view);
        case ExportFormat::Hydro: return export_hydro(ctx, p, tests, args, view);
        case ExportFormat::Hoj: return export_hoj(ctx, p, tests, args, view);
        case ExportFormat::Polygon: break;
    }
    return export_polygon(ctx, p, tests, args, view.items);
}

}  // namespace poly
