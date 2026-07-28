#include "storage.h"

#include "runtime_state.h"
#include "utilities.h"

#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <set>

using json = nlohmann::json;

void save_bot_state() {
    std::map<long long, int> live;
    std::map<long long, int> supplements;
    std::map<long long, std::string> languages;
    {
        std::lock_guard<std::mutex> lock(live_message_mutex);
        live = live_message_id;
        supplements = supplement_message_id;
    }
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        languages = user_language;
    }

    std::set<long long> chat_ids;
    for (const auto& [id, unused] : live) chat_ids.insert(id);
    for (const auto& [id, unused] : supplements) chat_ids.insert(id);
    for (const auto& [id, unused] : languages) chat_ids.insert(id);

    json state;
    state["chats"] = json::object();
    for (const long long chat_id : chat_ids) {
        json chat = json::object();
        const auto live_id = live.find(chat_id);
        if (live_id != live.end() && live_id->second > 0)
            chat["live_dashboard_message_id"] = live_id->second;
        const auto supplement = supplements.find(chat_id);
        if (supplement != supplements.end() && supplement->second > 0)
            chat["last_alert_text_message_id"] = supplement->second;
        const auto language = languages.find(chat_id);
        if (language != languages.end() && !language->second.empty())
            chat["language"] = language->second;
        if (!chat.empty()) state["chats"][std::to_string(chat_id)] = std::move(chat);
    }
    if (!write_json_atomic(BOT_STATE_FILE, state))
        std::cerr << "Не удалось сохранить " << BOT_STATE_FILE << '\n';
}

void load_bot_state() {
    std::ifstream input(BOT_STATE_FILE);
    if (!input) return;
    try {
        const json state = json::parse(input);
        if (!state.contains("chats") || !state["chats"].is_object()) {
            std::cerr << "⚠️ " << BOT_STATE_FILE << " не содержит объект chats\n";
            return;
        }
        std::map<long long, int> live;
        std::map<long long, int> supplements;
        std::map<long long, std::string> languages;
        for (const auto& item : state["chats"].items()) {
            long long chat_id = 0;
            try { chat_id = std::stoll(item.key()); } catch (...) { continue; }
            if (chat_id <= 0 || !item.value().is_object()) continue;
            const json& chat = item.value();
            if (chat.contains("live_dashboard_message_id")
                && chat["live_dashboard_message_id"].is_number_integer()) {
                const int id = chat["live_dashboard_message_id"].get<int>();
                if (id > 0) live[chat_id] = id;
            }
            if (chat.contains("last_alert_text_message_id")
                && chat["last_alert_text_message_id"].is_number_integer()) {
                const int id = chat["last_alert_text_message_id"].get<int>();
                if (id > 0) supplements[chat_id] = id;
            }
            if (chat.contains("language") && chat["language"].is_string()) {
                const std::string language = chat["language"].get<std::string>();
                if (!language.empty()) languages[chat_id] = language;
            }
        }
        {
            std::lock_guard<std::mutex> lock(live_message_mutex);
            for (const auto& [chat_id, message_id] : live)
                live_message_id[chat_id] = message_id;
            for (const auto& [chat_id, message_id] : supplements)
                supplement_message_id[chat_id] = message_id;
        }
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            for (const auto& [chat_id, language] : languages)
                user_language[chat_id] = language;
        }
        std::cout << "✅ Загружен " << BOT_STATE_FILE
                  << ": chats=" << state["chats"].size() << '\n';
    } catch (const std::exception& error) {
        std::cerr << "⚠️ Не удалось прочитать " << BOT_STATE_FILE
                  << ": " << error.what() << '\n';
    }
}

void save_user(long long chat_id) {
    std::lock_guard<std::mutex> lock(state_mutex);
    if (active_users.find(chat_id) == active_users.end()) {
        active_users.insert(chat_id);
        std::vector<std::string> lines;
        for (const long long user_id : active_users) {
            lines.push_back(std::to_string(user_id));
        }
        write_lines_atomic(USERS_FILE, lines);
    }
}

void load_users() {
    std::lock_guard<std::mutex> lock(state_mutex);
    std::ifstream input(USERS_FILE);
    long long chat_id = 0;
    while (input >> chat_id) {
        active_users.insert(chat_id);
    }
}

void save_user_city(long long chat_id, const std::string& city) {
    std::lock_guard<std::mutex> lock(state_mutex);
    user_city[chat_id] = city;
    std::vector<std::string> lines;
    for (const auto& [user_id, saved_city] : user_city) {
        lines.push_back(std::to_string(user_id) + " " + saved_city);
    }
    write_lines_atomic(CITIES_FILE, lines);
}

void load_user_cities() {
    std::lock_guard<std::mutex> lock(state_mutex);
    std::ifstream input(CITIES_FILE);
    long long chat_id = 0;
    std::string city;
    while (input >> chat_id >> std::ws && std::getline(input, city)) {
        user_city[chat_id] = city;
    }
}

void save_notification_status(long long chat_id, bool enabled) {
    std::lock_guard<std::mutex> lock(state_mutex);
    user_notifications[chat_id] = enabled;
    std::vector<std::string> lines;
    for (const auto& [user_id, status] : user_notifications) {
        lines.push_back(std::to_string(user_id) + " " + std::to_string(status));
    }
    write_lines_atomic(NOTIFICATIONS_FILE, lines);
}

void load_notifications() {
    std::lock_guard<std::mutex> lock(state_mutex);
    std::ifstream input(NOTIFICATIONS_FILE);
    long long chat_id = 0;
    bool enabled = false;
    while (input >> chat_id >> enabled) {
        user_notifications[chat_id] = enabled;
    }
}

void save_live_message_id(long long chat_id, int message_id) {
    std::vector<std::string> lines;
    {
        std::lock_guard<std::mutex> lock(live_message_mutex);
        if (message_id > 0) live_message_id[chat_id] = message_id;
        else live_message_id.erase(chat_id);
        for (const auto& [user_id, saved_id] : live_message_id) {
            lines.push_back(std::to_string(user_id) + " " + std::to_string(saved_id));
        }
    }
    write_lines_atomic(LIVE_MESSAGES_FILE, lines);
    save_bot_state();
}

int known_live_message_id(long long chat_id) {
    std::lock_guard<std::mutex> lock(live_message_mutex);
    const auto message = live_message_id.find(chat_id);
    return message == live_message_id.end() ? 0 : message->second;
}

void save_supplement_message_id(long long chat_id, int message_id) {
    std::vector<std::string> lines;
    {
        std::lock_guard<std::mutex> lock(live_message_mutex);
        if (message_id > 0) supplement_message_id[chat_id] = message_id;
        else supplement_message_id.erase(chat_id);
        for (const auto& [user_id, saved_id] : supplement_message_id) {
            lines.push_back(std::to_string(user_id) + " " + std::to_string(saved_id));
        }
    }
    write_lines_atomic(SUPPLEMENT_MESSAGES_FILE, lines);
    save_bot_state();
}

void load_live_messages() {
    std::lock_guard<std::mutex> lock(live_message_mutex);
    std::ifstream input(LIVE_MESSAGES_FILE);
    long long chat_id = 0;
    int message_id = 0;
    while (input >> chat_id >> message_id) live_message_id[chat_id] = message_id;
}

void load_supplement_messages() {
    std::lock_guard<std::mutex> lock(live_message_mutex);
    std::ifstream input(SUPPLEMENT_MESSAGES_FILE);
    long long chat_id = 0;
    int message_id = 0;
    while (input >> chat_id >> message_id) supplement_message_id[chat_id] = message_id;
}

bool is_notifications_enabled(long long chat_id) {
    std::lock_guard<std::mutex> lock(state_mutex);
    const auto status = user_notifications.find(chat_id);
    return status == user_notifications.end() || status->second;
}

std::vector<long long> active_user_snapshot() {
    std::lock_guard<std::mutex> lock(state_mutex);
    return {active_users.begin(), active_users.end()};
}

std::string user_city_or_default(long long chat_id) {
    std::lock_guard<std::mutex> lock(state_mutex);
    const auto city = user_city.find(chat_id);
    return city != user_city.end() && !city->second.empty() ? city->second : "Минск";
}
