#pragma once

#include <cpr/response.h>

#include <string>

bool telegram_ok(const cpr::Response& response);
bool telegram_not_modified(const cpr::Response& response);
bool telegram_retryable_failure(const cpr::Response& response);
bool telegram_edit_target_invalid(const cpr::Response& response);
std::string telegram_error_summary(const cpr::Response& response);

void log_incoming_message(long long chat_id, const std::string& text);
void log_incoming_callback(
    long long chat_id,
    int message_id,
    const std::string& data
);
bool validate_telegram_connection();
bool ensure_long_polling_mode();
void configure_bot_commands();
void log_polling_error(const cpr::Response& response);
void delete_telegram_message(long long chat_id, int message_id);
