#pragma once

#include <string>

std::string html_escape(const std::string& value);
std::string markdown_to_html(const std::string& value);
std::string markdown_to_telegram_html(const std::string& value);
std::string shell_quote(const std::string& value);
std::string format_double_1(double value);
