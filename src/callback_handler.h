#pragma once

#include <string>

void answer_callback_query(const std::string& callback_id, const std::string& text = "");
void handle_callback(long long chat_id, int message_id, const std::string& callback_id, const std::string& data);
