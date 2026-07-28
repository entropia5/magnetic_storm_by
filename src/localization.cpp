#include "localization.h"

#include "runtime_state.h"
#include "storage.h"
#include "translations.h"
#include "utilities.h"

#include <fstream>
#include <mutex>
#include <vector>

std::string lang_of(long long chat_id) {
    std::lock_guard<std::mutex> lock(state_mutex);
    const auto language = user_language.find(chat_id);
    return language != user_language.end() ? language->second : "ru";
}

std::string localize(
    long long chat_id,
    const std::string& russian,
    const std::string& belarusian,
    const std::string& english
) {
    const std::string language = lang_of(chat_id);
    if (language == "en") {
        return english;
    }
    if (language == "be") {
        return belarusian;
    }
    return russian;
}

std::string wind_unit(long long chat_id) {
    return lang_of(chat_id) == "en" ? "m/s" : "м/с";
}

void save_user_language(long long chat_id, const std::string& language) {
    std::vector<std::string> lines;
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        user_language[chat_id] = language;
        for (const auto& [user_id, saved_language] : user_language) {
            lines.push_back(std::to_string(user_id) + " " + saved_language);
        }
    }
    write_lines_atomic(LANGUAGE_FILE, lines);
    save_bot_state();
}

void load_languages() {
    std::lock_guard<std::mutex> lock(state_mutex);
    std::ifstream input(LANGUAGE_FILE);
    long long chat_id = 0;
    std::string language;
    while (input >> chat_id >> language) {
        user_language[chat_id] = language;
    }
}

std::string get_month_name(int month, long long chat_id) {
    static const std::vector<std::string> english = {
        "", "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };
    static const std::vector<std::string> belarusian = {
        "", "студзеня", "лютага", "сакавіка", "красавіка", "мая", "чэрвеня",
        "ліпеня", "жніўня", "верасня", "кастрычніка", "лістапада", "снежня"
    };
    static const std::vector<std::string> russian = {
        "", "января", "февраля", "марта", "апреля", "мая", "июня",
        "июля", "августа", "сентября", "октября", "ноября", "декабря"
    };
    if (month < 1 || month > 12) return {};
    const std::string language = lang_of(chat_id);
    if (language == "en") return english[month];
    if (language == "be") return belarusian[month];
    return russian[month];
}

std::string get_text(
    long long chat_id,
    const std::string& key,
    const std::string& first_argument,
    const std::string& second_argument
) {
    std::string language = lang_of(chat_id);
    const auto language_catalog = TEXTS.find(language);
    if (language_catalog == TEXTS.end()
        || language_catalog->second.find(key) == language_catalog->second.end()) {
        language = "ru";
    }

    const auto fallback = TEXTS.find(language);
    if (fallback == TEXTS.end()) {
        return {};
    }
    const auto translation = fallback->second.find(key);
    if (translation == fallback->second.end()) {
        return {};
    }

    std::string text = translation->second;
    const auto replace_argument = [&text](const std::string& argument) {
        if (argument.empty()) {
            return;
        }
        const std::size_t placeholder = text.find("{}");
        if (placeholder != std::string::npos) {
            text.replace(placeholder, 2, argument);
        }
    };
    replace_argument(first_argument);
    replace_argument(second_argument);
    return text;
}
