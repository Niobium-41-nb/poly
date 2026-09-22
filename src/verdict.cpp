#include "verdict.h"

#include <vector>

#include "strutil.h"

namespace poly {

const char* verdict_text(Verdict v) {
    switch (v) {
        case Verdict::OK: return "OK";
        case Verdict::WA: return "WA";
        case Verdict::PE: return "PE";
        case Verdict::TL: return "TL";
        case Verdict::ML: return "ML";
        case Verdict::RE: return "RE";
        case Verdict::CE: return "CE";
        case Verdict::FAIL: return "FAIL";
        case Verdict::SKIP: return "SKIP";
    }
    return "?";
}

Verdict verdict_parse(const std::string& s, Verdict def) {
    std::string v = to_upper(trim(s));
    if (v == "OK" || v == "AC" || v == "ACCEPTED") return Verdict::OK;
    if (v == "WA" || v == "WRONG" || v == "WRONG_ANSWER") return Verdict::WA;
    if (v == "PE" || v == "PRESENTATION" || v == "PRESENTATION_ERROR") return Verdict::PE;
    if (v == "TL" || v == "TLE" || v == "TIMELIMIT" || v == "TIME_LIMIT") return Verdict::TL;
    if (v == "ML" || v == "MLE" || v == "MEMORYLIMIT" || v == "MEMORY_LIMIT") return Verdict::ML;
    if (v == "RE" || v == "RTE" || v == "RUNTIME" || v == "RUNTIME_ERROR") return Verdict::RE;
    if (v == "CE" || v == "COMPILE" || v == "COMPILE_ERROR") return Verdict::CE;
    if (v == "FAIL" || v == "FAILED") return Verdict::FAIL;
    if (v == "SKIP" || v == "ANY") return Verdict::SKIP;
    return def;
}

std::string first_meaningful_line(const std::string& text) {
    std::vector<std::string> lines = split_lines(strip_cr(text));
    for (const std::string& raw : lines) {
        std::string line = trim(raw);
        if (!line.empty()) {
            if (line.size() > 200) line = line.substr(0, 197) + "...";
            return line;
        }
    }
    return std::string();
}

Verdict verdict_from_process(int exitCode, const std::string& text, std::string& message) {
    std::string t = to_lower(trim(text));
    message = first_meaningful_line(text);
    switch (exitCode) {
        case 0: return Verdict::OK;
        case 1: return Verdict::WA;
        case 2: return Verdict::PE;
        case 3: return Verdict::FAIL;
        case 4: return Verdict::FAIL;
        case 5: return Verdict::OK;
        case 7: return Verdict::WA;
        case 8: return Verdict::WA;
        default: break;
    }
    if (starts_with(t, "ok")) return Verdict::OK;
    if (contains(t, "wrong answer")) return Verdict::WA;
    if (contains(t, "presentation error")) return Verdict::PE;
    if (contains(t, "unexpected eof") || contains(t, "unexpected end of file")) return Verdict::WA;
    if (contains(t, "partially") || contains(t, "points")) return Verdict::OK;
    if (contains(t, "fail")) return Verdict::FAIL;
    return Verdict::FAIL;
}

bool verdict_is_expected(Verdict actual, const std::string& expected) {
    std::string e = to_lower(trim(expected));
    if (e.empty() || e == "ac" || e == "ok") return actual == Verdict::OK;
    if (e == "any" || e == "*") return true;
    if (e == "wa") return actual == Verdict::WA || actual == Verdict::PE;
    if (e == "pe") return actual == Verdict::PE;
    if (e == "tl") return actual == Verdict::TL;
    if (e == "ml") return actual == Verdict::ML;
    if (e == "re") return actual == Verdict::RE;
    if (e == "ce") return actual == Verdict::CE;
    if (e == "nok") return actual != Verdict::OK;
    if (e == "fail") return actual == Verdict::FAIL;
    return actual == Verdict::OK;
}

}  // namespace poly
