#include "telegram_client.h"

#include "runtime_state.h"
#include "utilities.h"

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <ctime>
#include <iostream>
#include <sstream>
#include <thread>

using namespace std;
using json = nlohmann::json;

bool telegram_ok(const cpr::Response& response) {
    if (response.status_code != 200) return false;
    try {
        auto data = json::parse(response.text);
        return data.contains("ok") && data["ok"].get<bool>();
    } catch (...) {
        return false;
    }
}

bool telegram_not_modified(const cpr::Response& response) {
    try {
        auto data = json::parse(response.text);
        if (!data.contains("description")) return false;
        string description = data["description"].get<string>();
        return description.find("message is not modified") != string::npos;
    } catch (...) {
        return false;
    }
}

bool telegram_retryable_failure(const cpr::Response& response) {
    if (response.status_code == 0 || response.status_code == 408 || response.status_code == 429) {
        return true;
    }
    if (response.status_code >= 500 && response.status_code < 600) {
        return true;
    }

    string text = ascii_lower_copy(response.text);
    return text.find("timeout") != string::npos
        || text.find("timed out") != string::npos
        || text.find("too many requests") != string::npos
        || text.find("bad gateway") != string::npos
        || text.find("service unavailable") != string::npos;
}

bool telegram_edit_target_invalid(const cpr::Response& response) {
    if (telegram_retryable_failure(response)) {
        return false;
    }

    string text = ascii_lower_copy(response.text);
    return text.find("message to edit not found") != string::npos
        || text.find("message not found") != string::npos
        || text.find("message can't be edited") != string::npos
        || text.find("message cannot be edited") != string::npos
        || text.find("message is not a media message") != string::npos
        || text.find("there is no media") != string::npos
        || text.find("there is no photo") != string::npos
        || text.find("message is not a photo") != string::npos
        || text.find("wrong type of the message content") != string::npos
        || text.find("type of file mismatch") != string::npos
        || text.find("message_id_invalid") != string::npos;
}

string telegram_error_summary(const cpr::Response& response) {
    stringstream ss;
    ss << "HTTP " << response.status_code;
    if (!response.text.empty()) {
        ss << " | " << response.text.substr(0, 500);
    }
    return ss.str();
}

void log_incoming_message(long long chat_id, const string& text) {
    cout << "⬇️ message chat_id=" << chat_id << " text=\"" << text.substr(0, 120) << "\"" << endl;
}

void log_incoming_callback(long long chat_id, int message_id, const string& data) {
    cout << "⬇️ callback chat_id=" << chat_id
         << " message_id=" << message_id
         << " data=\"" << data << "\"" << endl;
}

bool validate_telegram_connection() {
    auto response = cpr::Get(cpr::Url{API_URL + "/getMe"}, cpr::Timeout{TELEGRAM_SHORT_TIMEOUT_MS});
    if (telegram_ok(response)) {
        cout << "✅ Telegram getMe успешно" << endl;
        return true;
    }

    cerr << "❌ Telegram API недоступен или токен неверный: "
         << telegram_error_summary(response) << endl;
    return false;
}

bool ensure_long_polling_mode() {
    auto response = cpr::Post(
        cpr::Url{API_URL + "/deleteWebhook"},
        cpr::Payload{{"drop_pending_updates", "false"}},
        cpr::Timeout{TELEGRAM_SHORT_TIMEOUT_MS}
    );

    if (telegram_ok(response)) {
        cout << "✅ Telegram webhook отключён, long polling активен" << endl;
        return true;
    }

    cerr << "❌ Не удалось отключить webhook для long polling: "
         << telegram_error_summary(response) << endl;
    return false;
}

void configure_bot_commands() {
    json commands = json::array({
        {{"command", "start"}, {"description", "Open live dashboard"}},
        {{"command", "current"}, {"description", "Current Kp index"}},
        {{"command", "forecast"}, {"description", "3-day geomagnetic forecast"}},
        {{"command", "weather"}, {"description", "Weather for a Belarus location"}},
        {{"command", "city"}, {"description", "Change morning report city"}}
    });

    auto response = cpr::Post(
        cpr::Url{API_URL + "/setMyCommands"},
        cpr::Payload{{"commands", commands.dump()}},
        cpr::Timeout{TELEGRAM_SHORT_TIMEOUT_MS}
    );

    if (!telegram_ok(response)) {
        cerr << "⚠️ Не удалось обновить Telegram bot commands: "
             << telegram_error_summary(response) << endl;
    }
}

void log_polling_error(const cpr::Response& response) {
    static time_t last_log = 0;
    time_t now = time(nullptr);
    if (now - last_log < 30) return;

    cerr << "getUpdates не прошёл: " << telegram_error_summary(response) << endl;
    last_log = now;
}

void delete_telegram_message(long long chat_id, int message_id) {
    if (message_id <= 0) return;

    auto response = cpr::Post(
        cpr::Url{API_URL + "/deleteMessage"},
        cpr::Payload{
            {"chat_id", to_string(chat_id)},
            {"message_id", to_string(message_id)}
        },
        cpr::Timeout{TELEGRAM_SHORT_TIMEOUT_MS}
    );

    if (!telegram_ok(response)) {
        cerr << "deleteMessage не прошёл для chat_id=" << chat_id
             << ", message_id=" << message_id << ": "
             << telegram_error_summary(response) << endl;
    }
}
