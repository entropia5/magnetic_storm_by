#include "callback_handler.h"

#include "bot_screens.h"
#include "conversation_state.h"
#include "localization.h"
#include "runtime_state.h"
#include "storage.h"
#include "telegram_client.h"
#include "telegram_supplement.h"

#include <cpr/cpr.h>
#include <iostream>

using namespace std;

void answer_callback_query(const string& callback_id, const string& text) {
    cpr::Payload payload{{"callback_query_id", callback_id}};
    if (!text.empty()) {
        payload.Add({"text", text});
    }
    cpr::Post(cpr::Url{API_URL + "/answerCallbackQuery"}, payload, cpr::Timeout{TELEGRAM_SHORT_TIMEOUT_MS});
}

void handle_callback(long long chat_id, int message_id, const string& callback_id, const string& data) {
    save_user(chat_id);
    int current_live_message_id = known_live_message_id(chat_id);
    if (message_id > 0) {
        if (current_live_message_id > 0 && current_live_message_id != message_id) {
            cout << "↪️ live target updated from callback chat_id=" << chat_id
                 << " callback_message_id=" << message_id
                 << " current_live_message_id=" << current_live_message_id << endl;
            delete_telegram_message(chat_id, current_live_message_id);
        }
        save_live_message_id(chat_id, message_id);
    }
    delete_supplement_message(chat_id);
    clear_waiting_state(chat_id);

    answer_callback_query(callback_id);

    if (data == "current") {
        show_current_screen(chat_id);
    } else if (data == "forecast" || data.rfind("forecast:", 0) == 0) {
        int page = 0;
        if (data.rfind("forecast:", 0) == 0) {
            try {
                page = stoi(data.substr(9));
            } catch (...) {
                page = 0;
            }
        }
        show_forecast_screen(chat_id, page);
    } else if (data.rfind("morning:", 0) == 0) {
        int page = 0;
        try {
            page = stoi(data.substr(8));
        } catch (...) {
            page = 0;
        }
        send_morning_report(chat_id, page);
    } else if (data == "weather") {
        show_weather_result_screen(chat_id, user_city_or_default(chat_id), false);
    } else if (data == "mycity") {
        set_waiting_for_city(chat_id, true);
        show_city_prompt_screen(chat_id);
    } else if (data == "notify") {
        bool new_status = !is_notifications_enabled(chat_id);
        save_notification_status(chat_id, new_status);
        show_notifications_screen(chat_id, new_status);
    } else if (data == "lang") {
        string current_lang = lang_of(chat_id);
        string new_lang = "ru";
        if (current_lang == "ru") new_lang = "en";
        else if (current_lang == "en") new_lang = "be";
        else new_lang = "ru";
        save_user_language(chat_id, new_lang);
        show_language_screen(chat_id);
    } else {
        show_home_screen(chat_id);
    }
}
