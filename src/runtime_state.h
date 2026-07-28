#pragma once

#include <ctime>
#include <map>
#include <mutex>
#include <set>
#include <string>

extern std::string API_URL;
extern std::string WEATHER_API_KEY;

extern std::set<long long> active_users;
extern std::map<long long, std::string> user_city;
extern std::map<long long, bool> user_notifications;
extern std::map<long long, std::string> user_language;
extern std::map<long long, bool> waiting_for_city;
extern std::map<long long, bool> waiting_for_weather;
extern std::map<long long, int> live_message_id;
extern std::map<long long, int> supplement_message_id;

extern std::mutex live_message_mutex;
extern std::mutex state_mutex;
extern std::mutex conversation_mutex;

extern double last_alert_kp;
extern std::time_t last_alert_time;
extern bool screen_renderer_available;
extern bool storm_alert_active;
extern long long dev_chat_id;

extern const std::string USERS_FILE;
extern const std::string CITIES_FILE;
extern const std::string NOTIFICATIONS_FILE;
extern const std::string LANGUAGE_FILE;
extern const std::string LIVE_MESSAGES_FILE;
extern const std::string SUPPLEMENT_MESSAGES_FILE;
extern const std::string BOT_STATE_FILE;
extern const std::string SCREEN_DIR;
extern const std::string SCREEN_TEMPLATE_FILE;
extern const std::string SCREEN_CSS_FILE;

inline constexpr int TELEGRAM_SHORT_TIMEOUT_MS = 10000;
inline constexpr int TELEGRAM_SEND_TIMEOUT_MS = 20000;
