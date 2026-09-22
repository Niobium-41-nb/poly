// 出题工作台（web UI）：把 poly 的命令包成网页操作。
// 页面是服务端渲染的普通 HTML，不依赖任何前端框架；样式沿用题面 HTML 的浅色主题。
//
// 关键设计：网页上的每个操作都是「构造一个 Args → 调用同一个 cmd_* 函数」，
// 因此网页与命令行的行为完全一致，不会出现两套逻辑。
#include "webui.h"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>

#include "commands.h"
#include "exporter.h"
#include "format.h"
#include "fsutil.h"
#include "httpd.h"
#include "importer.h"
#include "judge.h"
#include "log.h"
#include "markdown.h"
#include "ops.h"
#include "plat.h"
#include "problem.h"
#include "proc.h"
#include "strutil.h"
#include "version.h"

namespace poly {

namespace {

// 工具只会串行地跑一个操作：编译器、测试点文件、标准答案都不是并发安全的。
std::mutex g_opMutex;

std::string esc(const std::string& s) { return escape_html(s); }

std::string url_for_problem(const std::string& name) {
    return "/problem/" + http_query_escape(name);
}

std::string css() {
    return
        "*{box-sizing:border-box}\n"
        "body{font-family:-apple-system,'Segoe UI','Microsoft YaHei',sans-serif;line-height:1.7;"
        "margin:0;color:#1f2328;background:#f6f8fa}\n"
        "a{color:#0969da;text-decoration:none}a:hover{text-decoration:underline}\n"
        "code{background:#f6f8fa;padding:.15em .35em;border-radius:4px;font-family:Consolas,monospace;"
        "font-size:.92em}\n"
        "pre{border-radius:6px;overflow:auto;white-space:pre-wrap;word-break:break-all;"
        "font-family:Consolas,monospace;font-size:13px;margin:8px 0}\n"
        ".topbar{background:#fff;border-bottom:1px solid #d0d7de;padding:12px 24px;display:flex;"
        "align-items:center;gap:22px;flex-wrap:wrap;position:sticky;top:0;z-index:10}\n"
        ".brand{font-weight:700;font-size:17px}\n"
        ".brand span{color:#57606a;font-weight:400;font-size:13px;margin-left:6px}\n"
        ".topbar nav{display:flex;gap:16px;font-size:14px}\n"
        ".topbar nav a.active{color:#1f2328;font-weight:600}\n"
        ".wspath{margin-left:auto;color:#57606a;font-size:12px;font-family:Consolas,monospace;"
        "max-width:46vw;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}\n"
        "main{max-width:1080px;margin:24px auto 40px;padding:0 20px}\n"
        ".card{background:#fff;border:1px solid #d0d7de;border-radius:8px;padding:18px 20px;"
        "margin-bottom:18px}\n"
        ".card h2{margin:0 0 14px;font-size:15px;border-bottom:1px solid #eaecef;padding-bottom:8px}\n"
        "h1{font-size:22px;margin:0 0 6px}\n"
        ".crumbs{font-size:13px;color:#57606a;margin-bottom:12px}\n"
        "table{border-collapse:collapse;width:100%;font-size:14px}\n"
        "th,td{border-bottom:1px solid #eaecef;padding:8px 10px;text-align:left;vertical-align:top}\n"
        "th{background:#f6f8fa;font-weight:600;color:#57606a;font-size:12px;"
        "text-transform:uppercase;letter-spacing:.04em}\n"
        "tr:last-child td{border-bottom:none}\n"
        ".btn{display:inline-flex;align-items:center;gap:6px;border:1px solid rgba(27,31,36,.15);"
        "border-radius:6px;padding:7px 14px;font-size:14px;font-family:inherit;cursor:pointer;"
        "background:#f6f8fa;color:#24292f;line-height:1.4}\n"
        ".btn:hover{background:#eef1f4}\n"
        ".btn.primary{background:#0969da;color:#fff}.btn.primary:hover{background:#0860c4}\n"
        ".btn.success{background:#1a7f37;color:#fff}.btn.success:hover{background:#166b2e}\n"
        ".btn.danger{background:#cf222e;color:#fff}.btn.danger:hover{background:#b81c28}\n"
        ".btn.small{padding:4px 10px;font-size:13px}\n"
        ".form-row{display:flex;flex-wrap:wrap;gap:12px;align-items:flex-end}\n"
        "label.f{display:block;font-size:12px;color:#57606a;margin-bottom:3px}\n"
        "select,input[type=text],input[type=number]{font-family:inherit;font-size:14px;padding:6px 8px;"
        "border:1px solid #d0d7de;border-radius:6px;background:#fff;color:#1f2328}\n"
        ".badge{display:inline-block;padding:1px 8px;border-radius:10px;font-size:12px;background:#eaecef;"
        "color:#57606a}\n"
        ".badge.ok{background:#dafbe1;color:#1a7f37}\n"
        ".badge.bad{background:#ffebe9;color:#cf222e}\n"
        ".badge.info{background:#ddf4ff;color:#0969da}\n"
        ".log{background:#0d1117;color:#c9d1d9;padding:14px;border-radius:6px;"
        "font-family:Consolas,monospace;font-size:12.5px;line-height:1.6;overflow:auto;"
        "max-height:640px;white-space:pre-wrap;word-break:break-all;margin:0}\n"
        ".kv{display:grid;grid-template-columns:repeat(auto-fit,minmax(170px,1fr));gap:12px 18px}\n"
        ".kv b{display:block;font-size:12px;color:#57606a;font-weight:400}\n"
        ".kv span{font-size:15px}\n"
        ".muted{color:#57606a;font-size:13px}\n"
        ".import-card{background:#fff;border:1px solid #d0d7de;border-radius:8px;margin-bottom:22px;"
        "overflow:hidden}\n"
        ".import-card .hd{padding:16px 22px 4px;font-size:20px;color:#0969da;font-weight:600}\n"
        ".import-card .hint{padding:0 22px;color:#57606a;font-size:13px}\n"
        ".import-card .bd{padding:14px 22px 20px;display:flex;gap:12px;align-items:center;"
        "flex-wrap:wrap}\n"
        "input[type=file]{position:absolute;width:1px;height:1px;opacity:0;overflow:hidden}\n"
        ".fname{color:#57606a;font-size:13px;font-family:Consolas,monospace;max-width:40vw;"
        "overflow:hidden;text-overflow:ellipsis;white-space:nowrap}\n"
        "fieldset{border:1px solid #eaecef;border-radius:6px;padding:12px 16px;margin:0 0 14px}\n"
        "legend{font-size:12px;color:#57606a;padding:0 6px}\n"
        ".tabs{display:flex;gap:6px;margin-bottom:14px;flex-wrap:wrap}\n"
        ".tabs a{padding:5px 12px;border:1px solid #d0d7de;border-radius:6px;background:#fff;"
        "font-size:13px}\n"
        ".tabs a.active{background:#0969da;color:#fff;border-color:#0969da}\n"
        "footer{max-width:1080px;margin:0 auto 34px;padding:0 20px;color:#8c959f;font-size:12px}\n"
        ".stmt{background:#fff;border:1px solid #d0d7de;border-radius:8px;padding:22px 26px}\n"
        ".stmt h1{font-size:1.6em;border-bottom:2px solid #eaecef;padding-bottom:.3em}\n"
        ".stmt h2{font-size:1.3em;border-bottom:1px solid #eaecef;padding-bottom:.2em;margin-top:1.5em}\n"
        ".stmt h3{font-size:1.1em;margin-top:1.2em}\n"
        ".stmt pre{background:#f6f8fa;padding:12px}\n"
        ".stmt blockquote{border-left:4px solid #d0d7de;margin:0;padding:0 14px;color:#57606a}\n"
        ".stmt table{width:auto}.stmt th,.stmt td{border:1px solid #d0d7de;padding:6px 12px}\n"
        ".sample{display:flex;gap:16px;flex-wrap:wrap;margin:12px 0}\n"
        ".sample>div{flex:1 1 320px;border:1px solid #d0d7de;border-radius:6px;overflow:hidden}\n"
        ".sample h4{margin:0;padding:6px 12px;background:#f6f8fa;border-bottom:1px solid #d0d7de;"
        "font-size:.9em}\n"
        ".sample pre{margin:0;border-radius:0;background:#fff;padding:10px}\n";
}

std::string js() {
    return
        "function polyPick(inputId){\n"
        "  var i=document.getElementById(inputId);\n"
        "  var n=document.getElementById(inputId+'-name');\n"
        "  var b=document.getElementById(inputId+'-upload');\n"
        "  i.onchange=function(){\n"
        "    if(i.files.length){n.textContent=i.files[0].name;if(b)b.disabled=false;}\n"
        "    else{n.textContent='未选择文件';if(b)b.disabled=true;}\n"
        "  };\n"
        "  i.click();\n"
        "}\n"
        "document.addEventListener('submit',function(e){\n"
        "  var f=e.target;\n"
        "  if(f.dataset && f.dataset.confirm && !confirm(f.dataset.confirm)){e.preventDefault();return;}\n"
        "  var btn=f.querySelector('button[type=submit]');\n"
        "  if(btn){btn.disabled=true;btn.textContent='执行中…';}\n"
        "},true);\n";
}

std::string layout(const Context& ctx, const std::string& title, const std::string& active,
                   const std::string& body, const std::string& mathjax = std::string()) {
    std::string h;
    h += "<!DOCTYPE html>\n<html lang=\"zh-CN\">\n<head>\n<meta charset=\"utf-8\"/>\n";
    h += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>\n";
    h += "<title>" + esc(title) + " · poly 工作台</title>\n<style>" + css() + "</style>\n";
    if (!mathjax.empty()) {
        h += "<script>MathJax={tex:{inlineMath:[['$','$']],displayMath:[['$$','$$']]}};</script>\n";
        h += "<script src=\"" + esc(mathjax) + "\" async></script>\n";
    }
    h += "</head>\n<body>\n";
    h += "<header class=\"topbar\">\n";
    h += "  <div class=\"brand\"><a href=\"/\" style=\"color:#1f2328\">poly</a>"
         "<span>出题工作台</span></div>\n";
    h += "  <nav>\n";
    auto navItem = [&](const char* href, const char* label, const char* id) {
        std::string cls = (active == id) ? " class=\"active\"" : "";
        return "    <a href=\"" + std::string(href) + "\"" + cls + ">" + label + "</a>\n";
    };
    h += navItem("/", "题目", "problems");
    h += navItem("/import", "导入题目", "import");
    h += navItem("/help", "帮助", "help");
    h += "  </nav>\n";
    h += "  <div class=\"wspath\" title=\"" + esc(ctx.workspace) + "\">工作区 " + esc(ctx.workspace) +
         "</div>\n";
    h += "</header>\n<main>\n" + body + "\n</main>\n";
    h += "<footer>poly " + std::string(POLY_VERSION) + " · 本地工作台（" +
         esc(fs::exe_dir()) + "）</footer>\n";
    h += "<script>" + js() + "</script>\n</body>\n</html>\n";
    return h;
}

// ---------------------------------------------------------------- 数据读取

std::vector<std::string> problem_names(const Context& ctx) { return list_problems(ctx.workspace); }

bool load_named(const Context& ctx, const std::string& name, Problem& p, std::string& err) {
    if (name.empty()) {
        err = "缺少题目名";
        return false;
    }
    return load_problem(ctx.workspace, name, p, err);
}

std::string samples_html(const Problem& p) {
    std::string out;
    std::vector<int> idxs = p.sampleTests;
    if (idxs.empty()) {
        for (int t : p.testIndices()) {
            idxs.push_back(t);
            if (idxs.size() >= 2) break;
        }
    }
    for (int t : idxs) {
        std::string input, output;
        fs::read_file(p.path(fs::join("tests", p.testFileName(t))), input);
        fs::read_file(p.path(answer_rel(t, p)), output);
        if (input.empty() && output.empty()) continue;
        out += "<div class=\"sample\">\n";
        out += "<div><h4>输入 #" + num(t) + "</h4><pre>" + esc(rtrim(strip_cr(input))) +
               "</pre></div>\n";
        out += "<div><h4>输出 #" + num(t) + "</h4><pre>" + esc(rtrim(strip_cr(output))) +
               "</pre></div>\n";
        out += "</div>\n";
    }
    if (out.empty()) out = "<p class=\"muted\">（还没有样例，先执行「生成测试点」）</p>\n";
    return out;
}

std::string render_statement_body(const Problem& p) {
    std::string md;
    if (!fs::read_file(p.path("statements/statement.md"), md)) {
        return "<p class=\"muted\">找不到 statements/statement.md</p>";
    }
    std::string html = markdown_to_html(md);
    std::string block = samples_html(p);
    html = replace_all(html, "<p>{{samples}}</p>", block);
    html = replace_all(html, "{{samples}}", block);
    return html;
}

std::string solution_options(const Problem& p) {
    std::string out;
    for (const SolutionInfo& s : p.solutions) {
        std::string label = s.name;
        if (!s.tag.empty()) label += " (" + s.tag + ")";
        if (s.expected != "ac" && !s.expected.empty()) label += " · 期望 " + to_upper(s.expected);
        out += "<option value=\"" + esc(s.name) + "\">" + esc(label) + "</option>\n";
    }
    if (out.empty()) out = "<option value=\"\">（没有解法）</option>\n";
    return out;
}

// ---------------------------------------------------------------- 页面：题目列表

HttpResponse page_index(const Context& ctx, const HttpRequest& req) {
    (void)req;
    std::vector<std::string> names = problem_names(ctx);
    std::string body;

    body += "<h1>题目</h1>\n";
    body += "<p class=\"muted\">工作区 " + esc(ctx.workspace) + "　共 " + num(names.size()) +
            " 道题</p>\n";

    body += "<div class=\"card\">\n";
    if (names.empty()) {
        body += "<p>工作区里还没有题目。</p>\n";
        body += "<p><a class=\"btn primary\" href=\"/import\">导入题目包</a>"
                "<span class=\"muted\" style=\"margin-left:10px\">（QDUOJ / FPS / Hydro / HOJ）</span></p>\n";
    } else {
        body += "<table>\n<thead><tr><th>题目</th><th>测试点</th><th>时限 / 内存</th>"
                "<th>checker</th><th>类型</th><th>解法</th><th></th></tr></thead>\n<tbody>\n";
        for (const std::string& name : names) {
            Problem p;
            std::string err;
            if (!load_named(ctx, name, p, err)) {
                body += "<tr><td>" + esc(name) + "</td><td colspan=\"6\">" + esc(err) + "</td></tr>\n";
                continue;
            }
            int tests = p.testCount();
            body += "<tr>\n";
            body += "<td><a href=\"" + url_for_problem(name) + "\">" + esc(name) + "</a></td>\n";
            body += "<td>" + num(tests) + "</td>\n";
            body += "<td>" + num(p.timeLimitMs) + " ms / " + num(p.memoryLimitKb / 1024) + " MB</td>\n";
            body += "<td>" + esc(p.checkerFile.empty() ? p.checker : "自定义") + "</td>\n";
            body += "<td>" + std::string(p.interactive ? "<span class=\"badge info\">交互</span>"
                                                       : "<span class=\"badge\">传统</span>") +
                    "</td>\n";
            body += "<td>" + num(p.solutions.size()) + "</td>\n";
            body += "<td style=\"white-space:nowrap\">";
            body += "<a class=\"btn small\" href=\"" + url_for_problem(name) + "\">详情</a> ";
            body += "<a class=\"btn small\" href=\"/statement/" + http_query_escape(name) +
                    "\">题面</a> ";
            body += "<a class=\"btn small\" href=\"/export/" + http_query_escape(name) +
                    "?format=polygon\">导出</a>";
            body += "</td>\n</tr>\n";
        }
        body += "</tbody></table>\n";
    }
    body += "</div>\n";

    body += "<div class=\"card\">\n<h2>工作区操作</h2>\n";
    body += "<form method=\"post\" action=\"/op\">\n";
    body += "<input type=\"hidden\" name=\"action\" value=\"doctor\"/>\n";
    body += "<button class=\"btn\" type=\"submit\">环境自检（编译器 / testlib / 工作区）</button>\n";
    body += "</form>\n";
    body += "<p class=\"muted\" style=\"margin:12px 0 0\">导入新题目："
            "<a href=\"/import\">从 QDUOJ / FPS / Hydro / HOJ 题目包导入</a>。</p>\n";
    body += "</div>\n";

    return HttpResponse::html(layout(ctx, "题目", "problems", body));
}

// ---------------------------------------------------------------- 页面：题目详情

std::string op_form(const std::string& name, const char* action, const char* label,
                    const char* buttonClass = "btn", const char* confirmText = nullptr) {
    std::string f = "<form method=\"post\" action=\"/op\"";
    if (confirmText) f += std::string(" data-confirm=\"") + esc(confirmText) + "\"";
    f += ">\n";
    f += "<input type=\"hidden\" name=\"name\" value=\"" + esc(name) + "\"/>\n";
    f += "<input type=\"hidden\" name=\"action\" value=\"" + std::string(action) + "\"/>\n";
    f += "<button class=\"" + std::string(buttonClass) + "\" type=\"submit\">" + label +
         "</button>\n";
    f += "</form>\n";
    return f;
}

HttpResponse page_problem(const Context& ctx, const HttpRequest& req) {
    std::string name = req.path.substr(std::string("/problem/").size());
    Problem p;
    std::string err;
    if (!load_named(ctx, name, p, err)) return HttpResponse::fail(404, err);
    name = p.name;

    std::string body;
    body += "<div class=\"crumbs\"><a href=\"/\">题目</a> / " + esc(name) + "</div>\n";
    body += "<h1>" + esc(name) + "</h1>\n";
    if (!p.note.empty()) body += "<p class=\"muted\">" + esc(p.note) + "</p>\n";

    // 概览
    body += "<div class=\"card\">\n<div class=\"kv\">\n";
    auto kv = [&](const char* k, const std::string& v) {
        body += "<div><b>" + std::string(k) + "</b><span>" + v + "</span></div>\n";
    };
    kv("测试点", num(p.testCount()));
    kv("时限", num(p.timeLimitMs) + " ms");
    kv("内存", num(p.memoryLimitKb / 1024) + " MB");
    kv("类型", p.interactive ? "交互题" : "传统题");
    kv("checker", esc(p.checkerFile.empty() ? p.checker : p.checkerFile));
    kv("生成器", num(static_cast<long long>(p.generators.size())));
    body += "</div>\n";
    std::string tags;
    for (const std::string& t : p.tags) tags += "<span class=\"badge\">" + esc(t) + "</span> ";
    if (!tags.empty()) body += "<p style=\"margin:12px 0 0\">" + tags + "</p>\n";
    body += "<p style=\"margin:14px 0 0\">"
            "<a class=\"btn small\" href=\"/statement/" + http_query_escape(name) + "\">预览题面</a> "
            "<a class=\"btn small\" href=\"/testdata/" + http_query_escape(name) + "\">查看测试点</a>"
            "</p>\n";
    body += "</div>\n";

    // 操作
    body += "<div class=\"card\">\n<h2>操作</h2>\n";
    body += "<fieldset><legend>造数据</legend>\n";
    body += "<div class=\"form-row\">\n";
    body += op_form(name, "build", "编译（生成器/校验器/checker/解法）");
    body += op_form(name, "gen", "生成测试点");
    body += op_form(name, "validate", "用校验器检查");
    body += "</div>\n</fieldset>\n";

    body += "<fieldset><legend>评测</legend>\n";
    body += "<div class=\"form-row\">\n";
    body += op_form(name, "test", "全部解法跑全部测试点", "btn primary");
    body += "</div>\n";
    body += "<form method=\"post\" action=\"/op\" class=\"form-row\" style=\"margin-top:12px\">\n";
    body += "<input type=\"hidden\" name=\"name\" value=\"" + esc(name) + "\"/>\n";
    body += "<input type=\"hidden\" name=\"action\" value=\"run\"/>\n";
    body += "<div><label class=\"f\">单解法运行</label><select name=\"solution\">" +
            solution_options(p) + "</select></div>\n";
    body += "<div><label class=\"f\">测试点（留空＝全部）</label>"
            "<input type=\"number\" name=\"test\" min=\"0\" style=\"width:130px\"/></div>\n";
    body += "<button class=\"btn\" type=\"submit\">运行</button>\n";
    body += "</form>\n";

    body += "<form method=\"post\" action=\"/op\" class=\"form-row\" style=\"margin-top:12px\">\n";
    body += "<input type=\"hidden\" name=\"name\" value=\"" + esc(name) + "\"/>\n";
    body += "<input type=\"hidden\" name=\"action\" value=\"stress\"/>\n";
    body += "<div><label class=\"f\">对拍：正解</label><select name=\"solution\">" +
            solution_options(p) + "</select></div>\n";
    body += "<div><label class=\"f\">对拍：待检验解</label><select name=\"solution2\">" +
            solution_options(p) + "</select></div>\n";
    body += "<div><label class=\"f\">轮数</label>"
            "<input type=\"number\" name=\"rounds\" value=\"300\" min=\"1\" style=\"width:110px\"/></div>\n";
    body += "<button class=\"btn\" type=\"submit\">开始对拍</button>\n";
    body += "</form>\n";
    body += "</fieldset>\n";

    body += "<fieldset><legend>题面与打包</legend>\n";
    body += "<div class=\"form-row\">\n";
    body += op_form(name, "statement", "渲染题面（HTML + LaTeX）");
    body += op_form(name, "clean", "清理中间产物", "btn danger",
                    "会删除 output/ 下的编译产物与标准答案缓存，确定继续？");
    body += "</div>\n";
    body += "<div class=\"form-row\" style=\"margin-top:12px\">\n";
    body += "<a class=\"btn success\" href=\"/export/" + http_query_escape(name) +
            "?format=polygon\">导出 Polygon 包</a>\n";
    body += "<a class=\"btn success\" href=\"/export/" + http_query_escape(name) +
            "?format=qduoj\">导出 QDUOJ 包</a>\n";
    body += "<a class=\"btn success\" href=\"/export/" + http_query_escape(name) +
            "?format=fps\">导出 FPS（xml）</a>\n";
    body += "<a class=\"btn success\" href=\"/export/" + http_query_escape(name) +
            "?format=hydro\">导出 Hydro 包</a>\n";
    body += "<a class=\"btn success\" href=\"/export/" + http_query_escape(name) +
            "?format=hoj\">导出 HOJ 包</a>\n";
    body += "</div>\n";
    body += "<p class=\"muted\" style=\"margin:10px 0 0\">导出前会自动编译并用标程生成缺失的标准答案。</p>\n";
    body += "</fieldset>\n";
    body += "</div>\n";

    // 解法表
    body += "<div class=\"card\">\n<h2>解法（" + num(p.solutions.size()) + "）</h2>\n";
    if (p.solutions.empty()) {
        body += "<p class=\"muted\">还没有解法，把一个 .cpp 放进 solutions/ 目录即可。</p>\n";
    } else {
        body += "<table>\n<thead><tr><th>名字</th><th>文件</th><th>期望</th><th>标签</th></tr></thead>\n<tbody>\n";
        for (const SolutionInfo& s : p.solutions) {
            body += "<tr><td>" + esc(s.name) + "</td><td><code>" + esc(s.file) + "</code></td><td>" +
                    esc(s.expected) + "</td><td>" + esc(s.tag) + "</td></tr>\n";
        }
        body += "</tbody></table>\n";
    }
    body += "</div>\n";

    // 测试点
    std::vector<int> idxs = p.testIndices();
    body += "<div class=\"card\">\n<h2>测试点（" + num(static_cast<long long>(idxs.size())) +
            "）</h2>\n";
    if (idxs.empty()) {
        body += "<p class=\"muted\">还没有测试点。</p>\n";
    } else {
        body += "<p>";
        int shown = 0;
        for (int t : idxs) {
            if (shown++ >= 60) {
                body += "…";
                break;
            }
            std::string file = p.testFileName(t);
            body += "<a class=\"btn small\" href=\"/testdata/" + http_query_escape(name) + "?n=" +
                    num(t) + "\">" + esc(file) + "</a> ";
        }
        body += "</p>\n";
        std::string first = p.testFileName(idxs.front());
        std::string input = fs::read_file(p.path(fs::join("tests", first)));
        body += "<p class=\"muted\">第一个测试点（" + esc(first) + "，" +
                std::string(fs::is_file(p.path(answer_rel(idxs.front(), p))) ? "含标准答案" : "无答案") +
                "）：</p>\n";
        body += "<pre style=\"background:#f6f8fa;padding:10px\">" +
                esc(rtrim(strip_cr(input.substr(0, 2000)))) + "</pre>\n";
    }
    body += "</div>\n";

    return HttpResponse::html(layout(ctx, name, "problems", body));
}

// ---------------------------------------------------------------- 页面：执行操作

// 把表单字段翻译成一次操作请求（具体执行在 ops.cpp，与 Win32 界面共用）。
int run_op(Context& ctx, const std::string& action, const std::string& name, const HttpRequest& req,
           std::string& label) {
    OpRequest op;
    op.action = action;
    op.problem = name;
    static const char* kParamKeys[] = {"solution", "solution2", "test", "rounds", "format"};
    for (const char* key : kParamKeys) {
        if (req.has_param(key)) op.params[key] = req.param(key);
    }
    return run_operation(ctx, op, label);
}

HttpResponse page_op(Context& ctx, const HttpRequest& req) {
    std::string action = req.param("action");
    std::string name = req.param("name");
    std::string label;
    std::string logText;
    int code = 1;
    double elapsedMs = 0;
    {
        std::lock_guard<std::mutex> lock(g_opMutex);
        bool color = log_color_enabled();
        log_set_capture(&logText);
        log_configure(false, false, false);
        auto t0 = std::chrono::steady_clock::now();
        try {
            code = run_op(ctx, action, name, req, label);
        } catch (const std::exception& e) {
            log_err(std::string("内部错误: ") + e.what());
            code = 1;
        } catch (...) {
            log_err("内部错误：未知异常");
            code = 1;
        }
        auto t1 = std::chrono::steady_clock::now();
        elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
        log_set_capture(nullptr);
        log_configure(color, log_is_verbose(), log_is_quiet());
    }

    std::string body;
    body += "<div class=\"crumbs\"><a href=\"/\">题目</a>";
    if (!name.empty()) {
        body += " / <a href=\"" + url_for_problem(name) + "\">" + esc(name) + "</a>";
    }
    body += " / " + esc(label) + "</div>\n";
    body += "<h1>" + esc(label) + "</h1>\n";
    body += "<p>" + std::string(code == 0 ? "<span class=\"badge ok\">成功</span>"
                                         : "<span class=\"badge bad\">失败（退出码 " + num(code) +
                                               "）</span>") +
            " <span class=\"muted\">耗时 " + num(static_cast<long long>(elapsedMs + 0.5)) +
            " ms</span></p>\n";

    body += "<div class=\"card\">\n<h2>输出</h2>\n";
    body += "<pre class=\"log\">" + esc(logText.empty() ? std::string("（没有输出）") : logText) +
            "</pre>\n</div>\n";

    body += "<p>";
    if (!name.empty()) {
        body += "<a class=\"btn\" href=\"" + url_for_problem(name) + "\">返回题目</a> ";
        body += "<a class=\"btn\" href=\"/testdata/" + http_query_escape(name) +
                "\">查看测试点</a> ";
    }
    body += "<a class=\"btn\" href=\"/\">返回题目列表</a></p>\n";

    return HttpResponse::html(layout(ctx, label, "problems", body));
}

// ---------------------------------------------------------------- 页面：导入

std::string import_hint_text(ImportFormat f) {
    switch (f) {
        case ImportFormat::Qduoj:
            return "QDUOJ 后台导出的 .zip（内含 <编号>/problem.json 与 testcase/）";
        case ImportFormat::Fps:
            return "HUSTOJ 的 freeproblemset .xml（测试数据内嵌在 XML 里）";
        case ImportFormat::Hydro:
            return "HydroOJ 导出的 .zip（内含 problem.yaml 与 testdata/），也可整包导入题库";
        case ImportFormat::Hoj:
            return "HOJ 后台导出的 .zip（problem_编号.json + 同名测试数据目录），可直接回传到 HOJ";
    }
    return "";
}

std::string import_card(const Context& ctx, ImportFormat format, const char* id,
                        const char* heading) {
    (void)ctx;
    std::string h;
    h += "<div class=\"import-card\">\n";
    h += "<div class=\"hd\">" + std::string(heading) + "</div>\n";
    h += "<div class=\"hint\">" + import_hint_text(format) + "</div>\n";
    h += "<form method=\"post\" action=\"/import\" enctype=\"multipart/form-data\">\n";
    h += "<div class=\"bd\">\n";
    h += "<input type=\"hidden\" name=\"format\" value=\"" + std::string(id) + "\"/>\n";
    h += "<input type=\"file\" id=\"file-" + std::string(id) + "\" name=\"file\""
         " accept=\".zip,.xml,application/zip,text/xml,application/xml\"/>\n";
    h += "<button type=\"button\" class=\"btn primary\""
         " onclick=\"polyPick('file-" + std::string(id) + "')\">选择文件</button>\n";
    h += "<span class=\"fname\" id=\"file-" + std::string(id) + "-name\">未选择文件</span>\n";
    h += "<button type=\"submit\" class=\"btn success\" id=\"file-" + std::string(id) +
         "-upload\" disabled>上传</button>\n";
    h += "</div>\n</form>\n";
    h += "</div>\n";
    return h;
}

HttpResponse page_import_get(const Context& ctx, const HttpRequest& req) {
    (void)req;
    std::string body;
    body += "<h1>导入题目</h1>\n";
    body += "<p class=\"muted\">支持四种常见题目包；导入后会在工作区里生成一个标准 poly 题目"
            "（题面、测试点、checker、标程尽可能还原）。</p>\n";
    body += import_card(ctx, ImportFormat::Qduoj, "qduoj", "导入QDUOJ的题目");
    body += import_card(ctx, ImportFormat::Fps, "fps", "导入FPS格式的题目");
    body += import_card(ctx, ImportFormat::Hydro, "hydro", "导入Hydro的题目");
    body += import_card(ctx, ImportFormat::Hoj, "hoj", "导入HOJ的题目");
    body += "<div class=\"card\"><h2>说明</h2><ul class=\"muted\" style=\"margin:0;padding-left:20px\">"
            "<li>FPS 里若含 <code>&lt;solution language=\"C++\"&gt;</code>，会写成题目的 "
            "<code>solutions/main.cpp</code>，可以直接用它生成标准答案。</li>"
            "<li>QDUOJ / Hydro / HOJ 的 spj 与交互器会分别落到 <code>files/checker.cpp</code> 与 "
            "<code>files/interactor.cpp</code>。</li>"
            "<li>导入的题目没有生成器（gen.cpp），测试数据保持原样；"
            "要重造数据请自己写生成器与 testscript。</li>"
            "<li>同名题目会自动加 <code>-2</code>、<code>-3</code> 后缀，不会覆盖已有题目。</li>"
            "</ul></div>\n";
    return HttpResponse::html(layout(ctx, "导入题目", "import", body));
}

HttpResponse page_import_post(Context& ctx, const HttpRequest& req) {
    std::string formatName = req.param("format");
    ImportFormat format;
    if (!parse_import_format(formatName, format)) {
        return HttpResponse::fail(400, "未知的导入格式: " + formatName);
    }
    if (!req.has_file("file")) {
        return HttpResponse::fail(400, "没有收到文件（请先选择要上传的题目包）");
    }
    const HttpFile& file = req.files.at("file");
    if (file.data.empty()) return HttpResponse::fail(400, "上传的文件是空的");

    std::vector<ImportOutcome> outcomes;
    std::string err;
    std::string logText;
    double elapsedMs = 0;
    bool ok = false;
    {
        std::lock_guard<std::mutex> lock(g_opMutex);
        bool color = log_color_enabled();
        log_set_capture(&logText);
        log_configure(false, false, false);
        auto t0 = std::chrono::steady_clock::now();
        log_step(str("导入 {}（{}，{}）", file.filename.empty() ? "题目包" : file.filename,
                     import_format_name(format), human_size(static_cast<long long>(file.data.size()))));
        ok = import_into_workspace(ctx, format, file.filename, file.data, outcomes, err);
        if (!ok) log_err(err);
        auto t1 = std::chrono::steady_clock::now();
        elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
        log_set_capture(nullptr);
        log_configure(color, log_is_verbose(), log_is_quiet());
    }
    if (!ok) {
        std::string body;
        body += "<h1>导入失败</h1>\n";
        body += "<p><span class=\"badge bad\">失败</span> " + esc(err) + "</p>\n";
        body += "<div class=\"card\"><pre class=\"log\">" + esc(logText) + "</pre></div>\n";
        body += "<p><a class=\"btn\" href=\"/import\">重新选择文件</a></p>\n";
        return HttpResponse::html(layout(ctx, "导入失败", "import", body));
    }

    std::string body;
    body += "<h1>导入成功</h1>\n";
    body += "<p><span class=\"badge ok\">成功</span> 共导入 " + num(outcomes.size()) + " 道题，耗时 " +
            num(static_cast<long long>(elapsedMs + 0.5)) + " ms</p>\n";
    for (const ImportOutcome& o : outcomes) {
        body += "<div class=\"card\">\n";
        body += "<h2>" + esc(o.name) + "</h2>\n";
        body += "<div class=\"kv\">\n";
        body += "<div><b>测试点</b><span>" + num(o.tests) + "</span></div>\n";
        body += "<div><b>checker</b><span>" + std::string(o.hasChecker ? "已导入" : "内置 ncmp") +
                "</span></div>\n";
        body += "<div><b>类型</b><span>" + std::string(o.interactive ? "交互题" : "传统题") +
                "</span></div>\n";
        body += "</div>\n";
        if (!o.notes.empty()) {
            body += "<ul class=\"muted\" style=\"margin:12px 0 0;padding-left:20px\">\n";
            for (const std::string& n : o.notes) body += "<li>" + esc(n) + "</li>\n";
            body += "</ul>\n";
        }
        body += "<p style=\"margin:14px 0 0\">";
        body += "<a class=\"btn primary\" href=\"" + url_for_problem(o.name) + "\">打开题目</a> ";
        body += "<a class=\"btn\" href=\"/statement/" + http_query_escape(o.name) + "\">题面</a> ";
        body += "<a class=\"btn\" href=\"/testdata/" + http_query_escape(o.name) + "\">测试点</a>";
        body += "</p>\n</div>\n";
    }
    if (!logText.empty()) {
        body += "<div class=\"card\"><h2>导入日志</h2><pre class=\"log\">" + esc(logText) +
                "</pre></div>\n";
    }
    body += "<p><a class=\"btn\" href=\"/import\">继续导入</a> "
            "<a class=\"btn\" href=\"/\">返回题目列表</a></p>\n";
    return HttpResponse::html(layout(ctx, "导入成功", "import", body));
}

// ---------------------------------------------------------------- 页面：导出下载

HttpResponse page_export(Context& ctx, const HttpRequest& req) {
    std::string name = req.path.substr(std::string("/export/").size());
    std::string format = req.param("format", "polygon");
    ExportFormat fmt;
    if (!parse_export_format(format, fmt)) return HttpResponse::fail(400, "未知的导出格式");
    Problem p;
    std::string err;
    if (!load_named(ctx, name, p, err)) return HttpResponse::fail(404, err);

    std::string temp = fs::unique_temp_path("poly-export");
    std::string ext;
    switch (fmt) {
        case ExportFormat::Fps: ext = ".fps.xml"; break;
        case ExportFormat::Qduoj: ext = ".qduoj.zip"; break;
        case ExportFormat::Hydro: ext = ".hydro.zip"; break;
        case ExportFormat::Hoj: ext = ".hoj.zip"; break;
        case ExportFormat::Polygon: ext = ".zip"; break;
    }
    std::string outPath = temp + ext;

    std::string logText;
    bool ok = false;
    {
        std::lock_guard<std::mutex> lock(g_opMutex);
        bool color = log_color_enabled();
        log_set_capture(&logText);
        log_configure(false, false, false);
        Args a;
        a.command = "package";
        ctx.problemName = p.name;
        a.positional.push_back(p.name);
        a.values["format"].push_back(export_format_name(fmt));
        a.values["output"].push_back(outPath);
        ok = cmd_package(ctx, a) == 0;
        log_set_capture(nullptr);
        log_configure(color, log_is_verbose(), log_is_quiet());
    }
    if (!ok) {
        fs::remove_file(outPath);
        std::string body = "<h1>导出失败</h1>\n<p><span class=\"badge bad\">失败</span></p>\n";
        body += "<div class=\"card\"><pre class=\"log\">" + esc(logText) + "</pre></div>\n";
        body += "<p><a class=\"btn\" href=\"" + url_for_problem(p.name) + "\">返回题目</a></p>\n";
        return HttpResponse::html(layout(ctx, "导出失败", "problems", body));
    }
    std::string data;
    if (!fs::read_file(outPath, data)) {
        fs::remove_file(outPath);
        return HttpResponse::fail(500, "导出文件读取失败");
    }
    fs::remove_file(outPath);

    std::string type = "application/octet-stream";
    if (fmt == ExportFormat::Fps) type = "text/xml; charset=utf-8";
    else if (fmt != ExportFormat::Polygon) type = "application/zip";
    std::string filename = default_export_name(p, fmt);
    log_ok(str("已下载 {}（{}）", filename, human_size(static_cast<long long>(data.size()))));
    return HttpResponse::download(filename, data, type);
}

// ---------------------------------------------------------------- 页面：题面 / 测试点

HttpResponse page_statement(const Context& ctx, const HttpRequest& req) {
    std::string name = req.path.substr(std::string("/statement/").size());
    Problem p;
    std::string err;
    if (!load_named(ctx, name, p, err)) return HttpResponse::fail(404, err);

    std::string body;
    body += "<div class=\"crumbs\"><a href=\"/\">题目</a> / <a href=\"" + url_for_problem(p.name) +
            "\">" + esc(p.name) + "</a> / 题面</div>\n";
    body += "<div class=\"stmt\">\n" + render_statement_body(p) + "</div>\n";
    body += "<p style=\"margin-top:16px\"><a class=\"btn\" href=\"" + url_for_problem(p.name) +
            "\">返回题目</a></p>\n";
    std::string mathjax = "https://cdn.jsdelivr.net/npm/mathjax@3/es5/tex-mml-chtml.js";
    if (env_get("POLY_NO_MATHJAX") == "1") mathjax.clear();
    return HttpResponse::html(layout(ctx, p.name + " 题面", "problems", body, mathjax));
}

HttpResponse page_testdata(const Context& ctx, const HttpRequest& req) {
    std::string name = req.path.substr(std::string("/testdata/").size());
    Problem p;
    std::string err;
    if (!load_named(ctx, name, p, err)) return HttpResponse::fail(404, err);
    std::vector<int> idxs = p.testIndices();
    long long n = req.param_int("n", idxs.empty() ? 0 : idxs.front());

    std::string body;
    body += "<div class=\"crumbs\"><a href=\"/\">题目</a> / <a href=\"" + url_for_problem(p.name) +
            "\">" + esc(p.name) + "</a> / 测试点</div>\n";
    body += "<h1>测试点</h1>\n";
    body += "<div class=\"tabs\">\n";
    for (int t : idxs) {
        std::string cls = (t == n) ? " class=\"active\"" : "";
        body += "<a" + cls + " href=\"/testdata/" + http_query_escape(p.name) + "?n=" + num(t) +
                "\">" + esc(p.testFileName(t)) + "</a>\n";
    }
    body += "</div>\n";

    if (idxs.empty()) {
        body += "<div class=\"card\"><p class=\"muted\">还没有测试点。</p></div>\n";
    } else {
        bool found = std::find(idxs.begin(), idxs.end(), static_cast<int>(n)) != idxs.end();
        if (!found) n = idxs.front();
        std::string file = p.testFileName(static_cast<int>(n));
        std::string input = fs::read_file(p.path(fs::join("tests", file)));
        std::string answer = fs::read_file(p.path(answer_rel(static_cast<int>(n), p)));
        long long inSize = static_cast<long long>(input.size());
        body += "<div class=\"card\">\n";
        body += "<h2>测试点 " + esc(file) + "</h2>\n";
        body += "<div class=\"kv\"><div><b>输入大小</b><span>" + human_size(inSize) +
                "</span></div>";
        body += "<div><b>标准答案</b><span>" +
                std::string(answer.empty() ? "（空 / 交互题）" : human_size(
                                                                      static_cast<long long>(answer.size()))) +
                "</span></div></div>\n";
        body += "</div>\n";
        body += "<div class=\"card\"><h2>输入</h2><pre style=\"background:#f6f8fa;padding:12px\">" +
                esc(rtrim(strip_cr(input))) + "</pre></div>\n";
        body += "<div class=\"card\"><h2>标准答案</h2><pre style=\"background:#f6f8fa;padding:12px\">" +
                esc(rtrim(strip_cr(answer))) + "</pre></div>\n";
    }
    body += "<p><a class=\"btn\" href=\"" + url_for_problem(p.name) + "\">返回题目</a></p>\n";
    return HttpResponse::html(layout(ctx, p.name + " 测试点", "problems", body));
}

// ---------------------------------------------------------------- 页面：帮助

HttpResponse page_help(const Context& ctx, const HttpRequest& req) {
    (void)req;
    std::string body;
    body += "<h1>帮助</h1>\n";
    body += "<div class=\"card\"><h2>这个工作台做什么</h2>\n";
    body += "<p class=\"muted\">它把命令行版 poly 的常用操作搬到了网页上："
            "导入题目包、造数据、评测、对拍、渲染题面、按多种格式打包导出。"
            "每个按钮背后其实就是在跑对应的 <code>poly</code> 命令，输出会原样显示在结果页。</p>\n";
    body += "<table>\n<thead><tr><th>网页操作</th><th>等价命令</th></tr></thead>\n<tbody>\n";
    auto row = [&](const char* a, const char* b) {
        body += "<tr><td>" + std::string(a) + "</td><td><code>" + b + "</code></td></tr>\n";
    };
    row("环境自检", "poly doctor");
    row("编译", "poly build &lt;题目&gt;");
    row("生成测试点", "poly gen &lt;题目&gt;");
    row("用校验器检查", "poly validate &lt;题目&gt;");
    row("全部解法跑全部测试点", "poly test &lt;题目&gt;");
    row("单解法运行", "poly run &lt;题目&gt; -s &lt;解法&gt; [--test N]");
    row("对拍", "poly stress &lt;题目&gt; -s 正解 -s 暴力 -n 轮数");
    row("渲染题面", "poly statement &lt;题目&gt; --html");
    row("导出", "poly package &lt;题目&gt; --format polygon|qduoj|fps|hydro|hoj");
    row("导入", "poly import [qduoj|fps|hydro|hoj] &lt;文件&gt;");
    body += "</tbody></table>\n</div>\n";

    body += "<div class=\"card\"><h2>题目目录结构</h2>\n";
    body += "<pre style=\"background:#f6f8fa;padding:12px\">";
    body += "problems/&lt;题目&gt;/\n";
    body += "  problem.json          题目配置（时限、内存、checker、解法期望…）\n";
    body += "  files/                生成器 / 校验器 / checker / 交互器 / testscript.txt\n";
    body += "  solutions/            各种解法，main.cpp 是标程\n";
    body += "  tests/                测试点（01, 02, …）\n";
    body += "  output/answers/       标准答案缓存（自动生成）\n";
    body += "  statements/           题面与题解的 markdown\n";
    body += "  stress/               对拍失败时留下的用例\n";
    body += "</pre></div>\n";
    body += "<p><a class=\"btn\" href=\"/\">返回题目列表</a></p>\n";
    return HttpResponse::html(layout(ctx, "帮助", "help", body));
}

}  // namespace

int run_webui(Context& ctx, const std::string& host, int port, bool openBrowser) {
    HttpServer server;
    std::string err;
    if (!server.start(host, port, err)) {
        log_err(err);
        return 1;
    }
    std::string base = str("http://{}:{}", host.empty() ? "127.0.0.1" : host, server.port());

    server.route("GET", "/", [&ctx](const HttpRequest& r) { return page_index(ctx, r); });
    server.route("GET", "/help", [&ctx](const HttpRequest& r) { return page_help(ctx, r); });
    server.route("GET", "/import", [&ctx](const HttpRequest& r) { return page_import_get(ctx, r); });
    server.route("POST", "/import", [&ctx](const HttpRequest& r) { return page_import_post(ctx, r); });
    server.route("POST", "/op", [&ctx](const HttpRequest& r) { return page_op(ctx, r); });
    server.route_prefix("GET", "/problem/",
                        [&ctx](const HttpRequest& r) { return page_problem(ctx, r); });
    server.route_prefix("GET", "/statement/",
                        [&ctx](const HttpRequest& r) { return page_statement(ctx, r); });
    server.route_prefix("GET", "/testdata/",
                        [&ctx](const HttpRequest& r) { return page_testdata(ctx, r); });
    server.route_prefix("GET", "/export/",
                        [&ctx](const HttpRequest& r) { return page_export(ctx, r); });
    server.route("GET", "/favicon.ico", [](const HttpRequest&) {
        HttpResponse r;
        r.status = 204;
        r.reason = "No Content";
        r.body.clear();
        return r;
    });

    log_step(str("poly 工作台已启动：{}", base));
    log_info(str("工作区 {}", ctx.workspace));
    log_info("按 Ctrl+C 停止");

    if (openBrowser) {
        std::string cmd = fs::resolve_executable("cmd");
#ifdef _WIN32
        if (!cmd.empty()) {
            ProcOptions o;
            o.args = {"/c", "start", "", base};
            run_process(cmd, o);
        }
#else
        std::string opener = fs::resolve_executable("xdg-open");
        if (!opener.empty()) {
            ProcOptions o;
            o.args = {base};
            run_process(opener, o);
        }
#endif
    }
    server.run();
    return 0;
}

}  // namespace poly
