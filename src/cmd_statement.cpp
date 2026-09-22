// 题面：把 statements/*.md 渲染成 HTML / LaTeX（可选 PDF），并自动注入样例。
#include <algorithm>

#include "commands.h"
#include "format.h"
#include "fsutil.h"
#include "judge.h"
#include "log.h"
#include "markdown.h"
#include "pipeline.h"
#include "proc.h"
#include "strutil.h"

namespace poly {

namespace {

const char* kSampleMarker = "@@POLY-SAMPLES@@";

struct Sample {
    int index = 0;
    std::string input;
    std::string output;
};

std::string sample_html(const std::vector<Sample>& samples) {
    if (samples.empty()) return "<p class=\"limits\">（还没有样例：先执行 poly gen 生成测试点）</p>\n";
    std::string out;
    for (const Sample& s : samples) {
        out += "<div class=\"sample\">\n";
        out += "<div><h4>输入 #" + num(s.index) + "</h4><pre>" + escape_html(rtrim(strip_cr(s.input))) +
               "</pre></div>\n";
        out += "<div><h4>输出 #" + num(s.index) + "</h4><pre>" + escape_html(rtrim(strip_cr(s.output))) +
               "</pre></div>\n";
        out += "</div>\n";
    }
    return out;
}

std::string sample_latex(const std::vector<Sample>& samples) {
    std::string out;
    for (const Sample& s : samples) {
        out += "\\paragraph{样例输入 \\#" + num(s.index) + "}\n";
        out += "\\begin{verbatim}\n" + rtrim(strip_cr(s.input)) + "\n\\end{verbatim}\n\n";
        out += "\\paragraph{样例输出 \\#" + num(s.index) + "}\n";
        out += "\\begin{verbatim}\n" + rtrim(strip_cr(s.output)) + "\n\\end{verbatim}\n\n";
    }
    return out;
}

std::string header_html(const Problem& p) {
    std::string out = "<p class=\"limits\">";
    out += "时间限制：" + num(p.timeLimitMs) + " ms　内存限制：" + num(p.memoryLimitKb / 1024) + " MB";
    out += p.interactive ? "　本题为交互题" : "　输入输出：标准输入 / 标准输出";
    out += "</p>\n";
    return out;
}

std::string header_latex(const Problem& p) {
    std::string out = "\\noindent\\textbf{时间限制：" + num(p.timeLimitMs) + " ms}\\quad \\textbf{内存限制：" +
                      num(p.memoryLimitKb / 1024) + " MB}";
    if (p.interactive) out += "\\\\ \\textbf{本题为交互题}";
    out += "\n\n";
    return out;
}

std::vector<Sample> collect_samples(Problem& p, JudgeEnv& env, bool needAnswers) {
    std::vector<Sample> samples;
    if (needAnswers) {
        std::vector<std::string> errs;
        ensure_answers(env, std::string(), errs);
        if (!errs.empty()) log_warn(errs.front());
    }
    std::vector<int> idxs = p.sampleTests;
    if (idxs.empty()) {
        for (int t : p.testIndices()) {
            idxs.push_back(t);
            if (idxs.size() >= 2) break;
        }
    }
    for (int t : idxs) {
        Sample s;
        s.index = t;
        fs::read_file(p.path(fs::join("tests", p.testFileName(t))), s.input);
        fs::read_file(p.path(answer_rel(t, p)), s.output);
        if (s.input.empty()) continue;
        samples.push_back(s);
    }
    return samples;
}

std::string with_samples(const std::string& md) {
    if (md.find(kSampleMarker) != std::string::npos) return md;
    if (md.find("{{samples}}") != std::string::npos) return replace_all(md, "{{samples}}", kSampleMarker);
    return md + "\n## 样例\n\n" + kSampleMarker + "\n";
}

bool find_latex_engine(std::string& engine) {
    for (const char* name : {"xelatex", "pdflatex", "lualatex"}) {
        std::string path = fs::resolve_executable(name);
        if (!path.empty()) {
            engine = path;
            return true;
        }
    }
    return false;
}

void open_path(const std::string& path) {
#ifdef _WIN32
    ProcOptions o;
    o.args = {"/c", "start", "", path};
    run_process(fs::resolve_executable("cmd"), o);
#else
    ProcOptions o;
    o.args = {path};
    run_process(fs::resolve_executable("xdg-open"), o);
#endif
}

}  // namespace

int cmd_statement(Context& ctx, const Args& args) {
    Problem p;
    if (!ctx.requireProblem(p)) return 1;

    JudgeEnv env;
    std::string err;
    if (!prepare_judge(ctx, p, "solutions", env, err)) log_warn(err);

    bool wantPdf = args.has_flag("pdf");
    bool wantHtml = args.has_flag("html") || !wantPdf;
    std::string mathjax = args.get("mathjax-url");

    std::vector<Sample> samples = collect_samples(p, env, !p.interactive);
    if (!samples.empty()) log_debug(str("收集到 {} 组样例", samples.size()));

    std::string stmtPath = fs::join(p.statementsDir(), "statement.md");
    std::string tutorialPath = fs::join(p.statementsDir(), "tutorial.md");
    std::string stmtMd, tutorialMd;
    if (!fs::read_file(stmtPath, stmtMd)) {
        log_err(str("找不到题面 {}", stmtPath));
        return 1;
    }
    fs::read_file(tutorialPath, tutorialMd);

    std::string stmtHtml;
    if (wantHtml) {
        std::string frag = markdown_to_html(with_samples(stmtMd));
        std::string block = sample_html(samples);
        frag = replace_all(frag, std::string("<p>") + kSampleMarker + "</p>", block);
        frag = replace_all(frag, kSampleMarker, block);
        stmtHtml = fs::join(p.statementsDir(), "statement.html");
        if (!fs::write_file(stmtHtml, html_page(p.name, header_html(p) + frag, mathjax))) {
            log_err(str("无法写入 {}", stmtHtml));
            return 1;
        }
        log_ok(str("题面 HTML：{}", stmtHtml));
        if (!tutorialMd.empty()) {
            std::string path = fs::join(p.statementsDir(), "tutorial.html");
            fs::write_file(path, html_page(p.name + " - 题解", markdown_to_html(tutorialMd), mathjax));
            log_ok(str("题解 HTML：{}", path));
        }
    }

    std::string frag = markdown_to_latex(with_samples(stmtMd));
    frag = replace_all(frag, kSampleMarker, sample_latex(samples));
    std::string stmtTex = fs::join(p.statementsDir(), "statement.tex");
    if (!fs::write_file(stmtTex, latex_document(p.name, header_latex(p) + frag))) {
        log_err(str("无法写入 {}", stmtTex));
        return 1;
    }
    log_ok(str("题面 LaTeX：{}", stmtTex));
    if (!tutorialMd.empty()) {
        fs::write_file(fs::join(p.statementsDir(), "tutorial.tex"),
                       latex_document(p.name + " - 题解", markdown_to_latex(tutorialMd)));
    }

    if (wantPdf) {
        std::string engine;
        if (!find_latex_engine(engine)) {
            log_warn("没有找到 xelatex / pdflatex，跳过 PDF（题面 .tex 已生成）");
        } else {
            for (const char* base : {"statement", "tutorial"}) {
                std::string tex = fs::join(p.statementsDir(), std::string(base) + ".tex");
                if (!fs::is_file(tex)) continue;
                log_step(str("编译 {}（{}）", std::string(base) + ".tex", engine));
                ProcOptions o;
                o.workDir = p.statementsDir();
                o.args = {"-interaction=nonstopmode", "-halt-on-error", std::string(base) + ".tex"};
                o.captureOutput = true;
                o.wallLimitMs = 180000;
                ProcResult r = run_process(engine, o);
                std::string pdf = fs::join(p.statementsDir(), std::string(base) + ".pdf");
                if (!r.started || r.exitCode != 0 || !fs::is_file(pdf)) {
                    std::string logText = r.out.empty() ? r.err : r.out;
                    int shown = 0;
                    for (const std::string& l : split_lines(strip_cr(logText))) {
                        if (!l.empty() && l[0] == '!') {
                            log_err(trim(l));
                            if (++shown >= 5) break;
                        }
                    }
                    log_err(str("{}.tex 编译失败（退出码 {}）", base, r.exitCode));
                } else {
                    log_ok(str("PDF：{}", pdf));
                }
            }
            for (const char* ext : {".aux", ".log", ".out", ".toc"}) {
                for (const char* base : {"statement", "tutorial"}) {
                    fs::remove_file(fs::join(p.statementsDir(), std::string(base) + ext));
                }
            }
        }
    }

    if (args.has_flag("open")) {
        std::string target = wantPdf ? fs::join(p.statementsDir(), "statement.pdf") : stmtHtml;
        if (fs::is_file(target)) open_path(target);
    }
    return 0;
}

}  // namespace poly
