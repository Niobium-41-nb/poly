#pragma once

#include <string>

namespace poly {

enum class Verdict {
    OK,
    WA,
    PE,
    TL,
    ML,
    RE,
    CE,
    FAIL,
    SKIP,
};

const char* verdict_text(Verdict v);
Verdict verdict_parse(const std::string& s, Verdict def = Verdict::FAIL);
bool verdict_is_expected(Verdict actual, const std::string& expected);

// Maps the exit code (and message) of a checker / interactor built on testlib
// conventions onto a verdict.  `message` receives the first meaningful line.
Verdict verdict_from_process(int exitCode, const std::string& text, std::string& message);

std::string first_meaningful_line(const std::string& text);

}  // namespace poly
