#pragma once

#include <string>

namespace poly {

// Renders a Markdown subset (headings, lists, tables, code, emphasis, links,
// math passthrough) into an HTML fragment.
std::string markdown_to_html(const std::string& md);

// Wraps a fragment into a standalone page with builtin styling.
std::string html_page(const std::string& title, const std::string& body, const std::string& mathjaxUrl);

// Renders the same subset into a LaTeX fragment usable with ctexart.
std::string markdown_to_latex(const std::string& md);

std::string latex_document(const std::string& title, const std::string& body);

}  // namespace poly
