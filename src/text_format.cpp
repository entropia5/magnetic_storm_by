#include "text_format.h"

#include <iomanip>
#include <sstream>

namespace {

std::string markdown_to_html_impl(const std::string& value, bool escape_quotes) {
    std::string output;
    bool bold = false;
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (index + 1 < value.size() && value[index] == '*' && value[index + 1] == '*') {
            output += bold ? "</b>" : "<b>";
            bold = !bold;
            ++index;
            continue;
        }
        switch (value[index]) {
            case '&': output += "&amp;"; break;
            case '<': output += "&lt;"; break;
            case '>': output += "&gt;"; break;
            case '"': output += escape_quotes ? "&quot;" : "\""; break;
            case '\'': output += escape_quotes ? "&#39;" : "'"; break;
            case '\n': output += escape_quotes ? "<br>" : "\n"; break;
            default: output += value[index]; break;
        }
    }
    if (bold) output += "</b>";
    return output;
}

}  // namespace

std::string html_escape(const std::string& value) {
    std::string output;
    output.reserve(value.size());
    for (const char character : value) {
        switch (character) {
            case '&': output += "&amp;"; break;
            case '<': output += "&lt;"; break;
            case '>': output += "&gt;"; break;
            case '"': output += "&quot;"; break;
            case '\'': output += "&#39;"; break;
            case '\n': output += "<br>"; break;
            default: output += character; break;
        }
    }
    return output;
}

std::string markdown_to_html(const std::string& value) {
    return markdown_to_html_impl(value, true);
}

std::string markdown_to_telegram_html(const std::string& value) {
    return markdown_to_html_impl(value, false);
}

std::string shell_quote(const std::string& value) {
    std::string output = "'";
    for (const char character : value) {
        output += character == '\'' ? "'\\''" : std::string(1, character);
    }
    return output + "'";
}

std::string format_double_1(double value) {
    std::ostringstream output;
    output << std::fixed << std::setprecision(1) << value;
    return output.str();
}
