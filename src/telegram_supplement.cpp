#include "telegram_supplement.h"

#include "runtime_state.h"
#include "storage.h"
#include "telegram_client.h"
#include "text_format.h"
#include "utilities.h"

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

#include <iostream>
#include <mutex>

using namespace std;
using json = nlohmann::json;

int known_supplement_message_id(long long chat_id) {
    lock_guard<mutex> lock(live_message_mutex);
    auto it = supplement_message_id.find(chat_id);
    return it == supplement_message_id.end() ? 0 : it->second;
}

void delete_supplement_message(long long chat_id) {
    int message_id = known_supplement_message_id(chat_id);
    if (message_id > 0) {
        delete_telegram_message(chat_id, message_id);
    }
    save_supplement_message_id(chat_id, 0);
}

bool edit_supplement_message(long long chat_id, int message_id, const string& text, cpr::Response* out_response = nullptr) {
    auto response = cpr::Post(
        cpr::Url{API_URL + "/editMessageText"},
        cpr::Payload{
            {"chat_id", to_string(chat_id)},
            {"message_id", to_string(message_id)},
            {"text", markdown_to_telegram_html(text)},
            {"parse_mode", "HTML"},
            {"disable_web_page_preview", "true"}
        },
        cpr::Timeout{TELEGRAM_SEND_TIMEOUT_MS}
    );
    if (out_response) {
        *out_response = response;
    }

    if (telegram_ok(response) || telegram_not_modified(response)) {
        return true;
    }

    cerr << "editMessageText supplement не прошёл для chat_id=" << chat_id
         << ", message_id=" << message_id << ": "
         << telegram_error_summary(response) << endl;
    return false;
}

bool send_supplement_message(long long chat_id, const string& text) {
    auto response = cpr::Post(
        cpr::Url{API_URL + "/sendMessage"},
        cpr::Payload{
            {"chat_id", to_string(chat_id)},
            {"text", markdown_to_telegram_html(text)},
            {"parse_mode", "HTML"},
            {"disable_web_page_preview", "true"}
        },
        cpr::Timeout{TELEGRAM_SEND_TIMEOUT_MS}
    );

    if (telegram_ok(response)) {
        try {
            auto data = json::parse(response.text);
            int message_id = data["result"]["message_id"].get<int>();
            save_supplement_message_id(chat_id, message_id);
            cout << "➡️ supplement sendMessage ok chat_id=" << chat_id
                 << " message_id=" << message_id << endl;
        } catch (...) {}
        return true;
    }

    cerr << "sendMessage supplement не прошёл для chat_id=" << chat_id << ": "
         << telegram_error_summary(response) << endl;
    return false;
}

void sync_supplement_message(long long chat_id, const ScreenView& view) {
    string text = trim_copy(view.supplement);
    if (text.empty()) {
        delete_supplement_message(chat_id);
        return;
    }

    const size_t max_text_size = 3900;
    if (text.size() > max_text_size) {
        text = text.substr(0, max_text_size) + "\n\n...";
    }

    int message_id = known_supplement_message_id(chat_id);
    if (message_id > 0) {
        cpr::Response edit_response;
        if (edit_supplement_message(chat_id, message_id, text, &edit_response)) {
            return;
        }
        if (telegram_edit_target_invalid(edit_response)) {
            save_supplement_message_id(chat_id, 0);
        } else {
            cerr << "Временная/неизвестная ошибка editMessageText supplement для chat_id="
                 << chat_id << ", новый helper не создаю" << endl;
            return;
        }
    }

    send_supplement_message(chat_id, text);
}

