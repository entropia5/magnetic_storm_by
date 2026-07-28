#include "runtime_state.h"

std::string API_URL;
std::string WEATHER_API_KEY;

std::set<long long> active_users;
std::map<long long, std::string> user_city;
std::map<long long, bool> user_notifications;
std::map<long long, std::string> user_language;
std::map<long long, bool> waiting_for_city;
std::map<long long, bool> waiting_for_weather;
std::map<long long, int> live_message_id;
std::map<long long, int> supplement_message_id;

std::mutex live_message_mutex;
std::mutex state_mutex;
std::mutex conversation_mutex;

double last_alert_kp = 0.0;
std::time_t last_alert_time = 0;
bool screen_renderer_available = true;
bool storm_alert_active = false;
long long dev_chat_id = 0;

const std::string USERS_FILE = "users.txt";
const std::string CITIES_FILE = "cities.txt";
const std::string NOTIFICATIONS_FILE = "notifications.txt";
const std::string LANGUAGE_FILE = "language.txt";
const std::string LIVE_MESSAGES_FILE = "live_messages.txt";
const std::string SUPPLEMENT_MESSAGES_FILE = "supplement_messages.txt";
const std::string BOT_STATE_FILE = "bot_state.json";
const std::string SCREEN_DIR = "bot_screens";
const std::string SCREEN_TEMPLATE_FILE = "templates/screen.html";
const std::string SCREEN_CSS_FILE = "templates/screen.css";
