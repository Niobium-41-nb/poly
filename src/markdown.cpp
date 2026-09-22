#include "markdown.h"

#include <algorithm>
#include <vector>

#include "format.h"
#include "strutil.h"

namespace poly {

namespace {

std::string escape_text(const std::string& s) {
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

bool is_blank(const std::string& s) { return trim(s).empty(); }

bool starts_with_any(const std::string& s, const std::vector<std::string>& prefixes) {
    for (const std::string& p : prefixes)
        if (starts_with(s, p)) return true;
    return false;
}

size_t find_close(const std::string& s, size_t from, const std::string& token) {
    return s.find(token, from);
}

std::string inline_html(const std::string& src);

std::string inline_html(const std::string& src) {
    std::string out;
    size_t i = 0;
    while (i < src.size()) {
        char c = src[i];
        if (c == static_cast<char>(92) && i + 1 < src.size()) {
            out += escape_text(std::string(1, src[i + 1]));
            i += 2;
            continue;
        }
        if (c == '`') {
            size_t close = find_close(src, i + 1, "`");
            if (close != std::string::npos) {
                out += "<code>" + escape_text(src.substr(i + 1, close - i - 1)) + "</code>";
                i = close + 1;
                continue;
            }
        }
        if (src.compare(i, 2, "**") == 0) {
            size_t close = find_close(src, i + 2, "**");
            if (close != std::string::npos) {
                out += "<strong>" + inline_html(src.substr(i + 2, close - i - 2)) + "</strong>";
                i = close + 2;
                continue;
            }
        }
        if (src.compare(i, 2, "~~") == 0) {
            size_t close = find_close(src, i + 2, "~~");
            if (close != std::string::npos) {
                out += "<del>" + inline_html(src.substr(i + 2, close - i - 2)) + "</del>";
                i = close + 2;
                continue;
            }
        }
        if (c == '*' || c == '_') {
            size_t close = find_close(src, i + 1, std::string(1, c));
            if (close != std::string::npos && close > i + 1) {
                out += "<em>" + inline_html(src.substr(i + 1, close - i - 1)) + "</em>";
                i = close + 1;
                continue;
            }
        }
        if (c == '[') {
            size_t mid = src.find("](", i);
            if (mid != std::string::npos) {
                size_t close = src.find(')', mid);
                if (close != std::string::npos) {
                    std::string text = src.substr(i + 1, mid - i - 1);
                    std::string url = src.substr(mid + 2, close - mid - 2);
                    out += str("<a href=\"{}\">{}</a>", escape_text(url), inline_html(text));
                    i = close + 1;
                    continue;
                }
            }
        }
        if (c == '$') {
            size_t close = find_close(src, i + 1, "$");
            if (close != std::string::npos) {
                out += escape_text(src.substr(i, close - i + 1));
                i = close + 1;
                continue;
            }
        }
        out += escape_text(std::string(1, c));
        i++;
    }
    return out;
}

bool is_table_separator(const std::string& line) {
    std::string t = trim(line);
    if (t.empty() || t[0] != '|') return false;
    for (char c : t) {
        if (c != '|' && c != '-' && c != ':' && c != ' ') return false;
    }
    return t.find('-') != std::string::npos;
}

std::vector<std::string> split_row(const std::string& line) {
    std::string t = trim(line);
    if (!t.empty() && t.front() == '|') t.erase(t.begin());
    if (!t.empty() && t.back() == '|') t.pop_back();
    std::vector<std::string> cells;
    std::string cur;
    for (size_t i = 0; i < t.size(); i++) {
        if (t[i] == '|' && (i == 0 || t[i - 1] != static_cast<char>(92))) {
            cells.push_back(trim(cur));
            cur.clear();
        } else {
            cur.push_back(t[i]);
        }
    }
    cells.push_back(trim(cur));
    return cells;
}

}  // namespace

std::string markdown_to_html(const std::string& md) {
    std::vector<std::string> lines = split_lines(strip_cr(md));
    std::string out;
    size_t i = 0;
    while (i < lines.size()) {
        std::string line = lines[i];
        std::string t = trim(line);

        if (t.empty()) {
            i++;
            continue;
        }
        if (starts_with(t, "```")) {
            i++;
            std::string code;
            while (i < lines.size() && !starts_with(trim(lines[i]), "```")) {
                code += lines[i] + "\n";
                i++;
            }
            if (i < lines.size()) i++;
            out += "<pre><code>" + escape_text(code) + "</code></pre>\n";
            continue;
        }
        if (t.size() > 1 && t[0] == '#') {
            size_t level = 0;
            while (level < t.size() && t[level] == '#') level++;
            std::string text = trim(t.substr(level));
            if (level > 6) level = 6;
            out += str("<h{}>{}</h{}>\n", level, inline_html(text), level);
            i++;
            continue;
        }
        if (t == "---" || t == "***" || t == "___") {
            out += "<hr/>\n";
            i++;
            continue;
        }
        if (starts_with(t, "|") && i + 1 < lines.size() && is_table_separator(lines[i + 1])) {
            std::vector<std::string> header = split_row(lines[i]);
            i += 2;
            out += "<table>\n<thead><tr>";
            for (const std::string& c : header) out += "<th>" + inline_html(c) + "</th>";
            out += "</tr></thead>\n<tbody>\n";
            while (i < lines.size() && starts_with(trim(lines[i]), "|")) {
                std::vector<std::string> cells = split_row(lines[i]);
                out += "<tr>";
                for (size_t k = 0; k < header.size(); k++) {
                    out += "<td>" + inline_html(k < cells.size() ? cells[k] : std::string()) + "</td>";
                }
                out += "</tr>\n";
                i++;
            }
            out += "</tbody></table>\n";
            continue;
        }
        if (starts_with(t, "> ")) {
            std::string body;
            while (i < lines.size() && starts_with(trim(lines[i]), "> ")) {
                body += inline_html(trim(lines[i]).substr(2)) + " ";
                i++;
            }
            out += "<blockquote>" + trim(body) + "</blockquote>\n";
            continue;
        }
        bool unordered = starts_with_any(t, {"- ", "* ", "+ "});
        bool ordered = false;
        if (!unordered && t.size() > 2 && isdigit(static_cast<unsigned char>(t[0]))) {
            size_t dot = t.find('.');
            if (dot != std::string::npos && dot <= 3 && dot + 1 < t.size() && t[dot + 1] == ' ') ordered = true;
        }
        if (unordered || ordered) {
            const char* tag = ordered ? "ol" : "ul";
            out += str("<{}>\n", tag);
            while (i < lines.size()) {
                std::string item = trim(lines[i]);
                bool isItem = false;
                std::string content;
                if (unordered && starts_with_any(item, {"- ", "* ", "+ "})) {
                    isItem = true;
                    content = item.substr(2);
                } else if (ordered) {
                    size_t dot = item.find('.');
                    if (dot != std::string::npos && dot <= 3 && dot + 1 < item.size() &&
                        item[dot + 1] == ' ') {
                        isItem = true;
                        content = item.substr(dot + 2);
                    }
                }
                if (!isItem) break;
                out += "<li>" + inline_html(trim(content)) + "</li>\n";
                i++;
            }
            out += str("</{}>\n", tag);
            continue;
        }

        std::string para;
        bool first = true;
        while (i < lines.size() && !trim(lines[i]).empty()) {
            std::string cur = trim(lines[i]);
            if (!first && (starts_with(cur, "```") || starts_with(cur, "#") || starts_with(cur, "|") ||
                           starts_with(cur, "> ") || starts_with_any(cur, {"- ", "* ", "+ "}))) {
                break;
            }
            if (!para.empty()) para += " ";
            para += cur;
            first = false;
            i++;
        }
        if (!para.empty()) out += "<p>" + inline_html(para) + "</p>\n";
    }
    return out;
}

namespace {

std::string latex_inline(const std::string& src) {
    std::string out;
    size_t i = 0;
    while (i < src.size()) {
        char c = src[i];
        if (c == static_cast<char>(92) && i + 1 < src.size()) {
            out += std::string(1, src[i + 1]);
            i += 2;
            continue;
        }
        if (c == '`') {
            size_t close = src.find('`', i + 1);
            if (close != std::string::npos) {
                out += "\texttt{" + src.substr(i + 1, close - i - 1) + "}";
                i = close + 1;
                continue;
            }
        }
        if (src.compare(i, 2, "**") == 0) {
            size_t close = src.find("**", i + 2);
            if (close != std::string::npos) {
                out += "\textbf{" + latex_inline(src.substr(i + 2, close - i - 2)) + "}";
                i = close + 2;
                continue;
            }
        }
        if (c == '*' || c == '_') {
            size_t close = src.find(c, i + 1);
            if (close != std::string::npos && close > i + 1) {
                out += "\\emph{" + latex_inline(src.substr(i + 1, close - i - 1)) + "}";
                i = close + 1;
                continue;
            }
        }
        if (c == '[') {
            size_t mid = src.find("](", i);
            if (mid != std::string::npos) {
                size_t close = src.find(')', mid);
                if (close != std::string::npos) {
                    out += "\\href{" + src.substr(mid + 2, close - mid - 2) + "}{" +
                           latex_inline(src.substr(i + 1, mid - i - 1)) + "}";
                    i = close + 1;
                    continue;
                }
            }
        }
        switch (c) {
            case '&': out += "\\&"; break;
            case '%': out += "\\%"; break;
            case '#': out += "\\#"; break;
            case '_': out += "\\_"; break;
            case '{': out += "\\{"; break;
            case '}': out += "\\}"; break;
            case '~': out += "\textasciitilde{}"; break;
            case '^': out += "\textasciicircum{}"; break;
            default: out.push_back(c);
        }
        i++;
    }
    return out;
}

}  // namespace

std::string markdown_to_latex(const std::string& md) {
    std::vector<std::string> lines = split_lines(strip_cr(md));
    std::string out;
    size_t i = 0;
    while (i < lines.size()) {
        std::string t = trim(lines[i]);
        if (t.empty()) {
            i++;
            continue;
        }
        if (starts_with(t, "```")) {
            i++;
            std::string code;
            while (i < lines.size() && !starts_with(trim(lines[i]), "```")) {
                code += lines[i] + "\n";
                i++;
            }
            if (i < lines.size()) i++;
            out += "\\begin{verbatim}\n" + code + "\\end{verbatim}\n\n";
            continue;
        }
        if (t.size() > 1 && t[0] == '#') {
            size_t level = 0;
            while (level < t.size() && t[level] == '#') level++;
            std::string text = trim(t.substr(level));
            if (level <= 1)
                out += "\\section*{" + latex_inline(text) + "}\n";
            else if (level == 2)
                out += "\\subsection*{" + latex_inline(text) + "}\n";
            else
                out += "\\subsubsection*{" + latex_inline(text) + "}\n";
            i++;
            continue;
        }
        if (t == "---") {
            out += "\\par\noindent\rule{\textwidth}{0.4pt}\\par\n";
            i++;
            continue;
        }
        bool unordered = starts_with_any(t, {"- ", "* ", "+ "});
        if (unordered) {
            out += "\\begin{itemize}\n";
            while (i < lines.size() && starts_with_any(trim(lines[i]), {"- ", "* ", "+ "})) {
                out += "  \\item " + latex_inline(trim(trim(lines[i]).substr(2))) + "\n";
                i++;
            }
            out += "\\end{itemize}\n\n";
            continue;
        }
        std::string para;
        while (i < lines.size() && !trim(lines[i]).empty()) {
            if (!para.empty()) para += " ";
            para += trim(lines[i]);
            i++;
        }
        out += latex_inline(para) + "\n\n";
    }
    return out;
}

std::string latex_document(const std::string& title, const std::string& body) {
    std::string out;
    out += "\\documentclass[11pt]{ctexart}\n";
    out += "\\usepackage{amsmath,amssymb}\n";
    out += "\\usepackage[margin=2.2cm]{geometry}\n";
    out += "\\usepackage{hyperref}\n";
    out += "\\begin{document}\n";
    out += "\\begin{center}{\\LARGE\\bfseries " + latex_inline(title) + "}\\end{center}\n\\vspace{1em}\n";
    out += body;
    out += "\\end{document}\n";
    return out;
}

std::string html_page(const std::string& title, const std::string& body, const std::string& mathjaxUrl) {
    std::string out;
    out += "<!DOCTYPE html>\n<html lang=\"zh-CN\">\n<head>\n<meta charset=\"utf-8\"/>\n";
    out += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>\n";
    out += "<title>" + escape_text(title) + "</title>\n";
    out += "<style>\n";
    out += "body{font-family:-apple-system,'Segoe UI','Microsoft YaHei',sans-serif;line-height:1.7;"
           "max-width:900px;margin:0 auto;padding:32px 24px;color:#1f2328;background:#fff}\n";
    out += "h1{font-size:1.9em;border-bottom:2px solid #eaecef;padding-bottom:.3em}\n";
    out += "h2{font-size:1.4em;border-bottom:1px solid #eaecef;padding-bottom:.2em;margin-top:1.6em}\n";
    out += "h3{font-size:1.15em;margin-top:1.3em}\n";
    out += "code{background:#f6f8fa;padding:.15em .35em;border-radius:4px;font-family:Consolas,monospace;"
           "font-size:.92em}\n";
    out += "pre{background:#f6f8fa;padding:12px;border-radius:6px;overflow-x:auto}\n";
    out += "pre code{background:none;padding:0}\n";
    out += "table{border-collapse:collapse;margin:12px 0}\n";
    out += "th,td{border:1px solid #d0d7de;padding:6px 12px}\n";
    out += "th{background:#f6f8fa}\n";
    out += "blockquote{border-left:4px solid #d0d7de;margin:0;padding:0 14px;color:#57606a}\n";
    out += ".sample{display:flex;gap:16px;flex-wrap:wrap;margin:12px 0}\n";
    out += ".sample>div{flex:1 1 320px;border:1px solid #d0d7de;border-radius:6px;overflow:hidden}\n";
    out += ".sample h4{margin:0;padding:6px 12px;background:#f6f8fa;border-bottom:1px solid #d0d7de;font-size:.9em}\n";
    out += ".sample pre{margin:0;border-radius:0;background:#fff}\n";
    out += ".limits{color:#57606a;font-size:.92em}\n";
    out += "</style>\n";
    if (!mathjaxUrl.empty()) {
        out += "<script>\nMathJax={tex:{inlineMath:[['$','$']],displayMath:[['$$','$$']]}};\n</script>\n";
        out += "<script src=\"" + escape_text(mathjaxUrl) + "\" async></script>\n";
    }
    out += "</head>\n<body>\n";
    out += body;
    out += "</body>\n</html>\n";
    return out;
}

}  // namespace poly
