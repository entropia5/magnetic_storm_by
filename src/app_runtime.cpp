#include "app_runtime.h"

#include "bot_screens.h"
#include "callback_handler.h"
#include "config.h"
#include "conversation_state.h"
#include "localization.h"
#include "runtime_state.h"
#include "scheduler.h"
#include "screen_renderer.h"
#include "storage.h"
#include "telegram_client.h"
#include "telegram_input.h"
#include "utilities.h"

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

using namespace std;
using json = nlohmann::json;

int run_bot() {
    load_env_file(".env");
    load_env_file(".env.local");

    const char* env_token = first_env_value({"TG_BOT_TOKEN", "TELEGRAM_BOT_TOKEN", "BOT_TOKEN"});
    if (!env_token) {
        cerr << "❌ Telegram token не найден. Задайте TG_BOT_TOKEN в окружении или .env" << endl;
        return 1;
    }
    API_URL = "https://api.telegram.org/bot" + string(env_token);
    if (!validate_telegram_connection()) {
        return 1;
    }
    if (!ensure_long_polling_mode()) {
        return 1;
    }
    configure_bot_commands();
    validate_screen_renderer();

    const char* env_weather = first_env_value({"OPENWEATHER_API_KEY", "OWM_API_KEY", "OPENWEATHERMAP_API_KEY"});
    if (env_weather && !string(env_weather).empty()) {
        WEATHER_API_KEY = env_weather;
    } else {
        cerr << "⚠️ OPENWEATHER_API_KEY не задан: погодные экраны будут недоступны" << endl;
    }
    dev_chat_id = parse_optional_chat_id(first_env_value({"GEOBOT_DEV_CHAT_ID", "DEV_CHAT_ID"}), "GEOBOT_DEV_CHAT_ID");

    load_users();
    load_user_cities();
    load_notifications();
    load_languages();
    load_live_messages();
    load_supplement_messages();
    load_bot_state();

    cout << "🤖 Белорусский бот для отслеживания магнитных бурь запущен!" << endl;
    cout << "📍 Поддерживаются любые населённые пункты Беларуси (города, деревни, посёлки)" << endl;
    cout << "🌐 Доступные языки: русский, белорусский, английский" << endl;
    cout << "✅ Активных пользователей: " << active_user_snapshot().size() << endl;

    thread(scheduler).detach();
    int last_id = 0;

    while (true) {
        auto r = cpr::Get(cpr::Url{API_URL + "/getUpdates"},
                          cpr::Parameters{{"offset", to_string(last_id + 1)}, {"timeout", "25"}},
                          cpr::Timeout{30000});

        if (r.status_code == 200) {
            try {
                json data = json::parse(r.text);
                if (!data.contains("ok") || !data["ok"].get<bool>()) {
                    log_polling_error(r);
                    this_thread::sleep_for(chrono::seconds(3));
                    continue;
                }

                for (auto& update : data["result"]) {
                    last_id = update["update_id"];

                    if (update.contains("callback_query")) {
                        auto& cb = update["callback_query"];
                        if (cb.contains("message") && cb["message"].contains("chat") && cb["message"].contains("message_id")) {
                            long long cid = cb["message"]["chat"]["id"];
                            int mid = cb["message"]["message_id"];
                            string callback_id = cb["id"];
                            string callback_data = cb.contains("data") ? cb["data"].get<string>() : "";
                            log_incoming_callback(cid, mid, callback_data);
                            handle_callback(cid, mid, callback_id, callback_data);
                        } else if (cb.contains("id")) {
                            answer_callback_query(cb["id"].get<string>());
                        }
                        continue;
                    }

                    if (update.contains("message") && update["message"].contains("text")) {
                        long long cid = update["message"]["chat"]["id"];
                        int incoming_message_id = update["message"].contains("message_id")
                            ? update["message"]["message_id"].get<int>()
                            : 0;
                        string txt = update["message"]["text"];
                        log_incoming_message(cid, txt);

                        save_user(cid);

                        if (dev_chat_id > 0 && cid == dev_chat_id) {
                            string dev_command = ascii_lower_copy(trim_copy(txt));
                            if (dev_command == "testing1") {
                                delete_telegram_message(cid, incoming_message_id);
                                send_morning_report(cid);
                                continue;
                            }
                            if (dev_command == "testing2") {
                                delete_telegram_message(cid, incoming_message_id);
                                show_alert_screen(cid, 6.2, true);
                                continue;
                            }
                        }

                        // language selection
                        if (txt == "🇷🇺 Русский" || txt == "🇬🇧 English" || txt == "🇧🇾 Беларуская") {
                            clear_waiting_state(cid);
                            string new_lang;
                            if (txt == "🇷🇺 Русский") {
                                new_lang = "ru";
                            } else if (txt == "🇬🇧 English") {
                                new_lang = "en";
                            } else {
                                new_lang = "be";
                            }

                            string current_lang = lang_of(cid);
                            if (new_lang != current_lang) {
                                save_user_language(cid, new_lang);
                                show_language_screen(cid);
                            }
                            continue;
                        }

                        if (is_command(txt, "/start")) {
                            clear_waiting_state(cid);
                            delete_telegram_message(cid, incoming_message_id);
                            show_home_screen(cid, telegram_user_display_name(update["message"]), true);
                            continue;
                        }
                        if (is_command(txt, "/current")) {
                            clear_waiting_state(cid);
                            delete_telegram_message(cid, incoming_message_id);
                            show_current_screen(cid);
                            continue;
                        }
                        if (is_command(txt, "/forecast")) {
                            clear_waiting_state(cid);
                            delete_telegram_message(cid, incoming_message_id);
                            show_forecast_screen(cid);
                            continue;
                        }
                        if (is_command(txt, "/weather")) {
                            clear_waiting_state(cid);
                            delete_telegram_message(cid, incoming_message_id);
                            set_waiting_for_weather(cid, true);
                            show_weather_prompt_screen(cid);
                            continue;
                        }
                        if (is_command(txt, "/city")) {
                            clear_waiting_state(cid);
                            delete_telegram_message(cid, incoming_message_id);
                            set_waiting_for_city(cid, true);
                            show_city_prompt_screen(cid);
                            continue;
                        }
                        if (txt == get_text(cid, "btn_current")) {
                            clear_waiting_state(cid);
                            show_current_screen(cid);
                            continue;
                        }
                        if (txt == get_text(cid, "btn_forecast")) {
                            clear_waiting_state(cid);
                            show_forecast_screen(cid);
                            continue;
                        }
                        if (txt == get_text(cid, "btn_weather")) {
                            clear_waiting_state(cid);
                            show_weather_result_screen(cid, user_city_or_default(cid), false);
                            continue;
                        }
                        if (txt == get_text(cid, "btn_mycity")) {
                            clear_waiting_state(cid);
                            set_waiting_for_city(cid, true);
                            show_city_prompt_screen(cid);
                            continue;
                        }
                        if (txt == get_text(cid, "btn_notify_on") || txt == get_text(cid, "btn_notify_off")) {
                            clear_waiting_state(cid);
                            bool current = is_notifications_enabled(cid);
                            bool new_status = !current;
                            save_notification_status(cid, new_status);
                            show_notifications_screen(cid, new_status);
                            continue;
                        }

                        // input city for weather
                        if (consume_waiting_for_city(cid)) {
                            delete_telegram_message(cid, incoming_message_id);
                            show_weather_result_screen(cid, txt, true);
                            continue;
                        }

                        if (consume_waiting_for_weather(cid)) {
                            delete_telegram_message(cid, incoming_message_id);
                            show_weather_result_screen(cid, txt, false);
                            continue;
                        }

                        delete_telegram_message(cid, incoming_message_id);
                        show_weather_result_screen(cid, txt, false);
                    }
                }
            } catch (const exception& e) {
                cerr << "Ошибка: " << e.what() << endl;
            }
        } else {
            log_polling_error(r);
        }
        this_thread::sleep_for(chrono::milliseconds(500));
    }
    return 0;
}
