#include "telegram_screen_service.h"

#include "localization.h"
#include "runtime_state.h"
#include "screen_renderer.h"
#include "storage.h"
#include "telegram_client.h"
#include "telegram_supplement.h"
#include "text_format.h"
#include "utilities.h"

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <iostream>
#include <mutex>

using namespace std;
using json = nlohmann::json;

json make_inline_keyboard(long long chat_id, int forecast_page, int forecast_total, const string& page_callback);
string get_kp_status(double kp, long long chat_id);

string photo_caption_for_screen(const ScreenView& view) {
    string text = trim_copy(view.supplement);
    if (text.empty()) {
        return "";
    }

    const size_t max_caption_size = 950;
    if (text.size() > max_caption_size) {
        text = text.substr(0, max_caption_size) + "\n...";
    }
    return markdown_to_telegram_html(text);
}

string fallback_text_for_screen(long long chat_id, const ScreenView& view) {
    string text;
    if (!view.title.empty()) {
        text += view.title;
    }
    if (!view.subtitle.empty()) {
        if (!text.empty()) text += "\n\n";
        text += view.subtitle;
    }
    if (view.kp >= 0.0) {
        if (!text.empty()) text += "\n\n";
        text += "Kp " + format_double_1(view.kp) + "\n" + get_kp_status(view.kp, chat_id);
    }
    if (view.show_weather && view.weather.ok) {
        if (!text.empty()) text += "\n\n";
        text += view.weather.icon + " " + view.weather.name + "\n";
        text += view.weather.description + "\n";
        text += "Температура: " + to_string(view.weather.temp) + "°C";
        text += ", ощущается как " + to_string(view.weather.feels_like) + "°C";
        text += "\nВлажность: " + to_string(view.weather.humidity) + "%";
        text += ", " + localize(chat_id, "ветер", "вецер", "wind") + ": "
             + to_string((int)view.weather.wind_speed) + " " + wind_unit(chat_id);
    }
    if (!view.forecast.empty()) {
        if (!text.empty()) text += "\n\n";
        for (const auto& fc : view.forecast) {
            text += "📅 " + fc.date + " | max Kp " + format_double_1(fc.max_kp) + " " + fc.status + "\n";
        }
    }
    if (!view.body.empty()) {
        if (!text.empty()) text += "\n\n";
        text += view.body;
    }
    if (!view.supplement.empty()) {
        if (!text.empty()) text += "\n\n";
        text += view.supplement;
    }
    if (text.empty()) {
        text = localize(chat_id, "Экран обновлён.", "Экран абноўлены.", "Screen updated.");
    }
    return text;
}

bool edit_live_text_message(long long chat_id, int message_id, const string& text, const json& kb, cpr::Response* out_response = nullptr) {
    auto edit = cpr::Post(cpr::Url{API_URL + "/editMessageText"},
                          cpr::Payload{
                              {"chat_id", to_string(chat_id)},
                              {"message_id", to_string(message_id)},
                              {"text", markdown_to_telegram_html(text)},
                              {"parse_mode", "HTML"},
                              {"disable_web_page_preview", "true"},
                              {"reply_markup", kb.dump()}
                          },
                          cpr::Timeout{TELEGRAM_SEND_TIMEOUT_MS});
    if (out_response) {
        *out_response = edit;
    }

    if (telegram_ok(edit) || telegram_not_modified(edit)) {
        cout << "➡️ editMessageText live ok chat_id=" << chat_id
             << " message_id=" << message_id << endl;
        return true;
    }

    cerr << "editMessageText live не прошёл для chat_id=" << chat_id
         << ", message_id=" << message_id << ": "
         << telegram_error_summary(edit) << endl;
    return false;
}

bool fallback_text_message(long long chat_id, const string& text, const json& kb) {
    auto sent = cpr::Post(cpr::Url{API_URL + "/sendMessage"},
                          cpr::Payload{
                              {"chat_id", to_string(chat_id)},
                              {"text", markdown_to_telegram_html(text)},
                              {"parse_mode", "HTML"},
                              {"reply_markup", kb.dump()}
                          },
                          cpr::Timeout{TELEGRAM_SEND_TIMEOUT_MS});

    if (telegram_ok(sent)) {
        try {
            auto data = json::parse(sent.text);
            int message_id = data["result"]["message_id"].get<int>();
            save_live_message_id(chat_id, message_id);
            cout << "➡️ fallback sendMessage ok chat_id=" << chat_id
                 << " message_id=" << message_id << endl;
        } catch (...) {}
        return true;
    } else {
        cerr << "fallback sendMessage не прошёл для chat_id=" << chat_id << ": "
             << telegram_error_summary(sent) << endl;
    }
    return false;
}

bool send_plain_fallback_text(long long chat_id, const string& text, const json& kb) {
    auto sent = cpr::Post(cpr::Url{API_URL + "/sendMessage"},
                          cpr::Payload{
                              {"chat_id", to_string(chat_id)},
                              {"text", text},
                              {"reply_markup", kb.dump()}
                          },
                          cpr::Timeout{TELEGRAM_SEND_TIMEOUT_MS});

    if (telegram_ok(sent)) {
        try {
            auto data = json::parse(sent.text);
            int message_id = data["result"]["message_id"].get<int>();
            save_live_message_id(chat_id, message_id);
            cout << "➡️ plain fallback sendMessage ok chat_id=" << chat_id
                 << " message_id=" << message_id << endl;
        } catch (...) {}
        return true;
    }

    cerr << "plain fallback sendMessage не прошёл для chat_id=" << chat_id << ": "
         << telegram_error_summary(sent) << endl;
    return false;
}

void upsert_live_screen(long long chat_id, const ScreenView& view, bool force_new_message) {
    string image_path = render_screen_image(chat_id, view);
    string fallback_text = fallback_text_for_screen(chat_id, view);
    string caption = photo_caption_for_screen(view);
    json kb = make_inline_keyboard(chat_id, view.forecast_page, view.forecast_total, view.page_callback);
    int known_message_id = 0;
    {
        lock_guard<mutex> lock(live_message_mutex);
        if (live_message_id.count(chat_id)) {
            known_message_id = live_message_id[chat_id];
        }
    }
    if (force_new_message) {
        cout << "➡️ force new live screen chat_id=" << chat_id << endl;
        delete_supplement_message(chat_id);
        if (known_message_id > 0) {
            delete_telegram_message(chat_id, known_message_id);
            save_live_message_id(chat_id, 0);
            known_message_id = 0;
        }
    } else {
        delete_supplement_message(chat_id);
    }

    if (image_path.empty()) {
        cerr << "render_screen_image вернул пустой путь, использую fallback text chat_id="
             << chat_id << endl;

        if (known_message_id > 0 && !force_new_message) {
            cpr::Response edit_response;
            if (edit_live_text_message(chat_id, known_message_id, fallback_text, kb, &edit_response)) {
                return;
            }

            if (telegram_edit_target_invalid(edit_response)) {
                cerr << "Сохранённый live text id сброшен для chat_id=" << chat_id
                     << " после постоянной ошибки editMessageText" << endl;
                delete_telegram_message(chat_id, known_message_id);
                save_live_message_id(chat_id, 0);
                known_message_id = 0;
            } else {
                cerr << "Временная/неизвестная ошибка editMessageText live для chat_id=" << chat_id
                     << ", новый экран не создаю" << endl;
                return;
            }
        }

        if (!fallback_text_message(chat_id, fallback_text, kb)) {
            send_plain_fallback_text(chat_id, fallback_text, kb);
        }
        return;
    }

    if (known_message_id > 0 && !force_new_message) {
        json media = {
            {"type", "photo"},
            {"media", "attach://screen"}
        };
        if (!caption.empty()) {
            media["caption"] = caption;
            media["parse_mode"] = "HTML";
        }

        auto edit = cpr::Post(
            cpr::Url{API_URL + "/editMessageMedia"},
            cpr::Multipart{
                {"chat_id", to_string(chat_id)},
                {"message_id", to_string(known_message_id)},
                {"media", media.dump()},
                {"screen", cpr::Files{cpr::File{image_path}}, "image/jpeg"},
                {"reply_markup", kb.dump()}
            },
            cpr::Timeout{TELEGRAM_SEND_TIMEOUT_MS}
        );

        if (telegram_ok(edit) || telegram_not_modified(edit)) {
            cout << "➡️ editMessageMedia ok chat_id=" << chat_id
                 << " message_id=" << known_message_id << endl;
            delete_supplement_message(chat_id);
            filesystem::remove(image_path);
            return;
        }

        cerr << "editMessageMedia не прошёл для chat_id=" << chat_id << ": "
             << telegram_error_summary(edit) << endl;

        if (telegram_edit_target_invalid(edit)) {
            cerr << "Сохранённый live media id сброшен для chat_id=" << chat_id
                 << " после постоянной ошибки editMessageMedia" << endl;
            delete_telegram_message(chat_id, known_message_id);
            save_live_message_id(chat_id, 0);
            known_message_id = 0;
        } else {
            cerr << "Временная/неизвестная ошибка editMessageMedia для chat_id=" << chat_id
                 << ", новый live screen не создаю" << endl;
            filesystem::remove(image_path);
            return;
        }
    }

    cpr::Multipart photo_payload{
            {"chat_id", to_string(chat_id)},
            {"photo", cpr::Files{cpr::File{image_path}}, "image/jpeg"},
            {"reply_markup", kb.dump()}
    };
    if (!caption.empty()) {
        photo_payload.parts.push_back({"caption", caption});
        photo_payload.parts.push_back({"parse_mode", "HTML"});
    }

    auto sent = cpr::Post(
        cpr::Url{API_URL + "/sendPhoto"},
        photo_payload,
        cpr::Timeout{TELEGRAM_SEND_TIMEOUT_MS}
    );

    if (telegram_ok(sent)) {
        try {
            auto data = json::parse(sent.text);
            int message_id = data["result"]["message_id"].get<int>();
            save_live_message_id(chat_id, message_id);
            cout << "➡️ sendPhoto ok chat_id=" << chat_id
                 << " message_id=" << message_id << endl;
            if (known_message_id > 0 && known_message_id != message_id) {
                delete_telegram_message(chat_id, known_message_id);
            }
            delete_supplement_message(chat_id);
        } catch (...) {}
    } else {
        cerr << "sendPhoto не прошёл для chat_id=" << chat_id << ": "
             << telegram_error_summary(sent) << endl;
        if (telegram_retryable_failure(sent)) {
            cerr << "Временная ошибка sendPhoto для chat_id=" << chat_id
                 << ", fallback sendMessage не создаю" << endl;
            filesystem::remove(image_path);
            return;
        }

        bool fallback_sent = fallback_text_message(chat_id, fallback_text, kb);
        if (!fallback_sent) {
            fallback_sent = send_plain_fallback_text(chat_id, fallback_text, kb);
        }
        if (fallback_sent && known_message_id > 0) {
            delete_telegram_message(chat_id, known_message_id);
        }
    }

    filesystem::remove(image_path);
}
