#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <vector>
#include <set>
#include <map>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <algorithm>
#include <sstream>
#include <cmath>
#include <filesystem>
#include <cstdlib>
#include <cctype>
#include <mutex>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include "src/template_engine.h"

using namespace std;
using json = nlohmann::json;

string API_URL;
string WEATHER_API_KEY;
set<long long> active_users;
map<long long, string> user_city;
map<long long, bool> user_notifications;
map<long long, string> user_language;
const string USERS_FILE = "users.txt";
const string CITIES_FILE = "cities.txt";
const string NOTIFICATIONS_FILE = "notifications.txt";
const string LANGUAGE_FILE = "language.txt";
const string LIVE_MESSAGES_FILE = "live_messages.txt";
const string SUPPLEMENT_MESSAGES_FILE = "supplement_messages.txt";
const string SCREEN_DIR = "bot_screens";
const string SCREEN_TEMPLATE_FILE = "templates/screen.html";
const string SCREEN_CSS_FILE = "templates/screen.css";
double last_alert_kp = 0.0;
time_t last_alert_time = 0;
map<long long, bool> waiting_for_city;
map<long long, bool> waiting_for_weather;
map<long long, int> live_message_id;
map<long long, int> supplement_message_id;
mutex live_message_mutex;
mutex state_mutex;
mutex conversation_mutex;
bool screen_renderer_available = true;
bool storm_alert_active = false;
const int TELEGRAM_SHORT_TIMEOUT_MS = 10000;
const int TELEGRAM_SEND_TIMEOUT_MS = 20000;
long long dev_chat_id = 0;

struct KpForecast {
    string date;
    double max_kp;
    string status;
    vector<double> values;
};

struct WeatherInfo {
    bool ok = false;
    string name;
    string description;
    string icon;
    int temp = 0;
    int feels_like = 0;
    int humidity = 0;
    double wind_speed = 0.0;
};

struct WeatherForecastSlot {
    string time;
    string icon;
    string description;
    int temp = 0;
    int feels_like = 0;
    int humidity = 0;
    double wind_speed = 0.0;
    int pop = 0;
    double rain_mm = 0.0;
    double snow_mm = 0.0;
};

// Texts for different languages.
map<string, map<string, string>> TEXTS = {
    {"ru", {
        {"welcome", "🌤 **Здравствуйте!**\n\nЯ бот для отслеживания магнитных бурь и погоды в Беларуси.\n\n📅 **Что я умею:**\n• Текущий индекс - состояние прямо сейчас\n• Прогноз на 3 дня - прогноз магнитных бурь\n• Погода сейчас - погода в любом городе Беларуси\n• Изменить город - установить город для утренней рассылки\n\n⏰ **Утренний отчёт** приходит в 9:00\n⚠️ **Оповещение о бурях** при Kp ≥ 5.0\n\nПо умолчанию город для рассылки - Минск.\nИспользуйте кнопку \"Изменить город\" чтобы изменить его!"},
        {"morning_greeting", "🌅 **Доброе утро!**"},
        {"weather_in", "☁️ **Погода в {}:**"},
        {"magnetic_status", "🛰 **Магнитная обстановка:**"},
        {"kp_now", "📊 Kp {} — {}"},
        {"forecast_3days", "📊 **Прогноз на 3 дня:**"},
        {"wish", "✨ Желаю вам прекрасного настроения и отличного самочувствия!\n🌸 Берегите себя и будьте здоровы!"},
        {"alert_title", "⚠️ **ВНИМАНИЕ! Геомагнитная буря!**"},
        {"kp_current", "📈 Текущий индекс: **Kp {}**"},
        {"recommendations", "💊 **Рекомендации:**\n• Снизьте лишние нагрузки\n• Пейте воду и отдыхайте\n• При серьёзных симптомах обращайтесь к врачу\n\n🌸 Берегите себя!"},
        {"city_saved", "✅ Город **{}** сохранён!\n\n📍 Теперь утренняя рассылка будет показывать погоду для этого города."},
        {"city_not_found", "❌ Населенный пункт не найден.\n\nПожалуйста, введите корректное название города или деревни Беларуси."},
        {"weather_api_missing", "⚠️ Погода временно недоступна: не задан OPENWEATHER_API_KEY."},
        {"enter_city", "🏙️ Напишите название вашего города/деревни для утренней рассылки\n(например: Минск, Гомель, Брест, Витебск, Гродно, Могилёв или любой другой населённый пункт Беларуси)"},
        {"enter_city_weather", "🇧🇾 Напишите название любого населённого пункта Беларуси\n(город, деревня, посёлок)"},
        {"notifications_on", "✅ Уведомления о магнитных бурях **включены**!\n\n⚠️ Вы будете получать оповещения при Kp ≥ 5.0"},
        {"notifications_off", "🔕 Уведомления о магнитных бурях **отключены**!\n\nВы можете снова включить их в любой момент."},
        {"current_index", "🛰 **Геомагнитная обстановка:**\n\n📊 **Индекс сейчас:** Kp {}\n{}"},
        {"forecast_title", "📈 **Прогноз на 3 дня**\n\n"},
        {"language_changed", "✅ Язык изменён на русский!"},
        {"btn_current", "Текущий индекс"},
        {"btn_forecast", "Прогноз на 3 дня"},
        {"btn_weather", "Погода сейчас"},
        {"btn_mycity", "Изменить город"},
        {"btn_notify_on", "Включить уведомления"},
        {"btn_notify_off", "Отключить уведомления"},
        {"btn_forecast_prev", "← День назад"},
        {"btn_forecast_next", "День вперёд →"},
        {"btn_lang", "🇬🇧 English"}
    }},
    {"be", {
        {"welcome", "🌤 **Вітаю!**\n\nЯ бот для адсочвання магнітных бур і надвор'я ў Беларусі.\n\n📅 **Што я ўмею:**\n• Бягучы індэкс - стан прама зараз\n• Прагноз на 3 дні - прагноз магнітных бур\n• Надвор'е цяпер - надвор'е ў любым горадзе Беларусі\n• Змяніць горад - усталяваць горад для ранішняй рассылкі\n\n⏰ **Ранішняя справаздача** прыходзіць у 9:00\n⚠️ **Апавяшчэнне пра буры** пры Kp ≥ 5.0\n\nПа змаўчанні горад для рассылкі - Мінск.\nВыкарыстоўвайце кнопку \"Змяніць горад\" каб змяніць яго!"},
        {"morning_greeting", "🌅 **Добрай раніцы!**"},
        {"weather_in", "☁️ **Надвор'е ў {}:**"},
        {"magnetic_status", "🛰 **Магнітная абстаноўка:**"},
        {"kp_now", "📊 Kp {} — {}"},
        {"forecast_3days", "📊 **Прагноз на 3 дні:**"},
        {"wish", "✨ Жадаю вам выдатнага настрою і выдатнага самаадчування!\n🌸 Беражыце сябе і будзьце здаровыя!"},
        {"alert_title", "⚠️ **УВАГА! Геамагнітная бура!**"},
        {"kp_current", "📈 Бягучы індэкс: **Kp {}**"},
        {"recommendations", "💊 **Рэкамендацыі:**\n• Знізьце лішнія нагрузкі\n• Піце ваду і адпачывайце\n• Пры сур'ёзных сімптомах звяртайцеся да ўрача\n\n🌸 Беражыце сябе!"},
        {"city_saved", "✅ Горад **{}** захаваны!\n\n📍 Цяпер ранішняя рассылка будзе паказваць надвор'е для гэтага горада."},
        {"city_not_found", "❌ Населены пункт не знойдзены.\n\nКалі ласка, увядзіце карэктную назву горада ці вёскі Беларусі."},
        {"weather_api_missing", "⚠️ Надвор'е часова недаступнае: не зададзены OPENWEATHER_API_KEY."},
        {"enter_city", "🏙️ Напішыце назву вашага горада/вёскі для ранішняй рассылкі\n(напрыклад: Мінск, Гомель, Брэст, Віцебск, Гродна, Магілёў ці любы іншы населены пункт Беларусі)"},
        {"enter_city_weather", "🇧🇾 Напішыце назву любога населенага пункта Беларусі\n(горад, вёска, пасёлак)"},
        {"notifications_on", "✅ Апавяшчэнні аб магнітных бурах **уключаны**!\n\n⚠️ Вы будзеце атрымліваць апавяшчэнні пры Kp ≥ 5.0"},
        {"notifications_off", "🔕 Апавяшчэнні аб магнітных бурах **адключаны**!\n\nВы можаце зноў уключыць іх у любы момант."},
        {"current_index", "🛰 **Геамагнітная абстаноўка:**\n\n📊 **Індэкс цяпер:** Kp {}\n{}"},
        {"forecast_title", "📈 **Прагноз на 3 дні**\n\n"},
        {"language_changed", "✅ Мова зменена на беларускую!"},
        {"btn_current", "Бягучы індэкс"},
        {"btn_forecast", "Прагноз на 3 дні"},
        {"btn_weather", "Надвор'е цяпер"},
        {"btn_mycity", "Змяніць горад"},
        {"btn_notify_on", "Уключыць апавяшчэнні"},
        {"btn_notify_off", "Выключыць апавяшчэнні"},
        {"btn_forecast_prev", "← Дзень назад"},
        {"btn_forecast_next", "Дзень наперад →"},
        {"btn_lang", "🇬🇧 English"}
    }},
    {"en", {
        {"welcome", "🌤 **Hello!**\n\nI'm a bot for tracking magnetic storms and weather in Belarus.\n\n📅 **What I can do:**\n• Current index - current status\n• 3-day forecast - magnetic storm forecast\n• Weather now - weather in any Belarusian city\n• Change city - set city for morning report\n\n⏰ **Morning report** at 9:00 AM\n⚠️ **Storm alerts** at Kp ≥ 5.0\n\nDefault city for reports is Minsk.\nUse the \"Change city\" button to change it!"},
        {"morning_greeting", "🌅 **Good morning!**"},
        {"weather_in", "☁️ **Weather in {}:**"},
        {"magnetic_status", "🛰 **Magnetic situation:**"},
        {"kp_now", "📊 Kp {} — {}"},
        {"forecast_3days", "📊 **3-day forecast:**"},
        {"wish", "✨ Wishing you a great mood and excellent health!\n🌸 Take care of yourself!"},
        {"alert_title", "⚠️ **ATTENTION! Geomagnetic storm!**"},
        {"kp_current", "📈 Current index: **Kp {}**"},
        {"recommendations", "💊 **Recommendations:**\n• Reduce unnecessary strain\n• Drink water and rest\n• Seek medical help for serious symptoms\n\n🌸 Take care of yourself!"},
        {"city_saved", "✅ City **{}** saved!\n\n📍 Now the morning report will show weather for this city."},
        {"city_not_found", "❌ Location not found.\n\nPlease enter a valid city or village name in Belarus."},
        {"weather_api_missing", "⚠️ Weather is temporarily unavailable: OPENWEATHER_API_KEY is not set."},
        {"enter_city", "🏙️ Enter your city/village name for the morning report\n(e.g., Minsk, Gomel, Brest, Vitebsk, Grodno, Mogilev or any other Belarusian location)"},
        {"enter_city_weather", "🇧🇾 Enter any Belarusian location name\n(city, village, town)"},
        {"notifications_on", "✅ Storm notifications **enabled**!\n\n⚠️ You will receive alerts at Kp ≥ 5.0"},
        {"notifications_off", "🔕 Storm notifications **disabled**!\n\nYou can re-enable them anytime."},
        {"current_index", "🛰 **Geomagnetic situation:**\n\n📊 **Current index:** Kp {}\n{}"},
        {"forecast_title", "📈 **3-day forecast**\n\n"},
        {"language_changed", "✅ Language changed to English!"},
        {"btn_current", "Current index"},
        {"btn_forecast", "3-day forecast"},
        {"btn_weather", "Weather now"},
        {"btn_mycity", "Change city"},
        {"btn_notify_on", "Enable notifications"},
        {"btn_notify_off", "Disable notifications"},
        {"btn_forecast_prev", "← Previous day"},
        {"btn_forecast_next", "Next day →"},
        {"btn_lang", "🇧🇾 Беларуская"}
    }}
};

string lang_of(long long chat_id);

string get_text(long long chat_id, const string& key, const string& arg1 = "", const string& arg2 = "") {
    string lang;
    {
        lock_guard<mutex> lock(state_mutex);
        lang = user_language.count(chat_id) ? user_language[chat_id] : "ru";
    }
    if (!TEXTS.count(lang) || !TEXTS[lang].count(key)) {
        lang = "ru";
    }
    string text = TEXTS[lang].count(key) ? TEXTS[lang][key] : "";

    if (!arg1.empty()) {
        size_t pos = text.find("{}");
        if (pos != string::npos) {
            text.replace(pos, 2, arg1);
        }
    }
    if (!arg2.empty()) {
        size_t pos = text.find("{}");
        if (pos != string::npos) {
            text.replace(pos, 2, arg2);
        }
    }
    return text;
}

string trim_copy(const string& value) {
    const string whitespace = " \t\r\n";
    size_t first = value.find_first_not_of(whitespace);
    if (first == string::npos) return "";
    size_t last = value.find_last_not_of(whitespace);
    return value.substr(first, last - first + 1);
}

string unquote_env_value(const string& value) {
    string trimmed = trim_copy(value);
    if (trimmed.size() >= 2) {
        char first = trimmed.front();
        char last = trimmed.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            return trimmed.substr(1, trimmed.size() - 2);
        }
    }
    return trimmed;
}

void load_env_file(const string& path) {
    ifstream file(path);
    if (!file) return;

    string line;
    while (getline(file, line)) {
        string trimmed = trim_copy(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;
        if (trimmed.rfind("export ", 0) == 0) {
            trimmed = trim_copy(trimmed.substr(7));
        }

        size_t eq = trimmed.find('=');
        if (eq == string::npos) continue;

        string key = trim_copy(trimmed.substr(0, eq));
        string value = unquote_env_value(trimmed.substr(eq + 1));
        if (key.empty() || getenv(key.c_str()) != nullptr) continue;
        setenv(key.c_str(), value.c_str(), 0);
    }
}

const char* first_env_value(initializer_list<const char*> names) {
    for (const char* name : names) {
        const char* value = getenv(name);
        if (value && string(value).size() > 0) {
            return value;
        }
    }
    return nullptr;
}

long long parse_optional_chat_id(const char* value, const string& env_name) {
    if (!value || string(value).empty()) {
        return 0;
    }

    string trimmed = trim_copy(value);
    try {
        size_t parsed = 0;
        long long chat_id = stoll(trimmed, &parsed);
        if (parsed != trimmed.size() || chat_id <= 0) {
            cerr << "⚠️ " << env_name << " должен быть положительным числом, dev-команды отключены" << endl;
            return 0;
        }
        return chat_id;
    } catch (...) {
        cerr << "⚠️ " << env_name << " не удалось прочитать как Telegram chat_id, dev-команды отключены" << endl;
        return 0;
    }
}

string ascii_lower_copy(string value) {
    for (char& c : value) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (uc < 128) c = static_cast<char>(tolower(uc));
    }
    return value;
}

bool write_lines_atomic(const string& path, const vector<string>& lines) {
    auto nonce = chrono::duration_cast<chrono::nanoseconds>(
        chrono::steady_clock::now().time_since_epoch()
    ).count();
    string temp_path = path + ".tmp." + to_string(nonce);

    {
        ofstream outfile(temp_path, ios::trunc);
        if (!outfile) {
            cerr << "Не удалось открыть временный файл для записи: " << temp_path << endl;
            return false;
        }
        for (const string& line : lines) {
            outfile << line << '\n';
        }
        outfile.flush();
        if (!outfile) {
            cerr << "Не удалось записать временный файл: " << temp_path << endl;
            filesystem::remove(temp_path);
            return false;
        }
    }

    error_code ec;
    filesystem::rename(temp_path, path, ec);
    if (ec) {
        cerr << "Не удалось заменить файл " << path << ": " << ec.message() << endl;
        filesystem::remove(temp_path);
        return false;
    }
    return true;
}

void set_waiting_for_city(long long chat_id, bool value) {
    lock_guard<mutex> lock(conversation_mutex);
    waiting_for_city[chat_id] = value;
}

void set_waiting_for_weather(long long chat_id, bool value) {
    lock_guard<mutex> lock(conversation_mutex);
    waiting_for_weather[chat_id] = value;
}

bool consume_waiting_for_city(long long chat_id) {
    lock_guard<mutex> lock(conversation_mutex);
    bool value = waiting_for_city[chat_id];
    waiting_for_city[chat_id] = false;
    return value;
}

bool consume_waiting_for_weather(long long chat_id) {
    lock_guard<mutex> lock(conversation_mutex);
    bool value = waiting_for_weather[chat_id];
    waiting_for_weather[chat_id] = false;
    return value;
}

void clear_waiting_state(long long chat_id) {
    lock_guard<mutex> lock(conversation_mutex);
    waiting_for_city[chat_id] = false;
    waiting_for_weather[chat_id] = false;
}

bool is_command(const string& text, const string& command) {
    string first_token = trim_copy(text);
    size_t space = first_token.find_first_of(" \t\r\n");
    if (space != string::npos) {
        first_token = first_token.substr(0, space);
    }
    return first_token == command || first_token.rfind(command + "@", 0) == 0;
}

string telegram_user_display_name(const json& message) {
    if (!message.contains("from") || !message["from"].is_object()) {
        return "";
    }

    const auto& from = message["from"];
    string name;
    if (from.contains("first_name") && from["first_name"].is_string()) {
        name = from["first_name"].get<string>();
    }
    if (from.contains("last_name") && from["last_name"].is_string()) {
        if (!name.empty()) name += " ";
        name += from["last_name"].get<string>();
    }
    name = trim_copy(name);
    if (!name.empty()) {
        return name;
    }
    if (from.contains("username") && from["username"].is_string()) {
        return "@" + from["username"].get<string>();
    }
    return "";
}

bool weather_configured() {
    return !WEATHER_API_KEY.empty();
}

string weather_api_lang(long long chat_id) {
    string lang = lang_of(chat_id);
    if (lang == "en") return "en";
    if (lang == "be") return "be";
    return "ru";
}

cpr::Response fetch_weather_response(const string& query, long long chat_id) {
    cpr::Response empty;
    if (!weather_configured()) {
        empty.status_code = 0;
        return empty;
    }

    return cpr::Get(
        cpr::Url{"https://api.openweathermap.org/data/2.5/weather"},
        cpr::Parameters{
            {"q", query},
            {"units", "metric"},
            {"lang", weather_api_lang(chat_id)},
            {"appid", WEATHER_API_KEY}
        },
        cpr::Timeout{8000}
    );
}

cpr::Response fetch_weather_forecast_response(const string& query, long long chat_id) {
    cpr::Response empty;
    if (!weather_configured()) {
        empty.status_code = 0;
        return empty;
    }

    return cpr::Get(
        cpr::Url{"https://api.openweathermap.org/data/2.5/forecast"},
        cpr::Parameters{
            {"q", query},
            {"units", "metric"},
            {"lang", weather_api_lang(chat_id)},
            {"appid", WEATHER_API_KEY}
        },
        cpr::Timeout{8000}
    );
}

string weather_emoji_from_code(const string& icon_code, const string& description) {
    if (icon_code.rfind("01", 0) == 0) return "☀️";
    if (icon_code.rfind("02", 0) == 0) return "🌤️";
    if (icon_code.rfind("03", 0) == 0 || icon_code.rfind("04", 0) == 0) return "☁️";
    if (icon_code.rfind("09", 0) == 0) return "🌧️";
    if (icon_code.rfind("10", 0) == 0) return "🌦️";
    if (icon_code.rfind("11", 0) == 0) return "⛈️";
    if (icon_code.rfind("13", 0) == 0) return "❄️";
    if (icon_code.rfind("50", 0) == 0) return "🌫️";
    if (description.find("дожд") != string::npos) return "🌧️";
    if (description.find("снег") != string::npos) return "❄️";
    if (description.find("гроз") != string::npos) return "⛈️";
    return "🌡️";
}

string weather_icon_for_description(const string& description) {
    if (description.find("ясно") != string::npos || description.find("солнечно") != string::npos) return "☀️";
    if (description.find("облачно") != string::npos) return "☁️";
    if (description.find("дожд") != string::npos) return "🌧️";
    if (description.find("снег") != string::npos) return "❄️";
    if (description.find("туман") != string::npos) return "🌫️";
    if (description.find("гроз") != string::npos) return "⛈️";
    return "🌡️";
}

// Normalize location name by correcting common misspellings and variations. Returns corrected city name or original input if not recognized.
string normalize_location(const string& location) {
    string cleaned = trim_copy(location);
    if (cleaned.empty()) return cleaned;

    // Map of common misspellings and variations to correct city names
    map<string, string> city_map = {
        {"гомель", "Гомель"}, {"гомел", "Гомель"}, {"homel", "Гомель"}, {"gomel", "Гомель"},
        {"Гомель", "Гомель"}, {"Гомел", "Гомель"},
        {"минск", "Минск"}, {"minsk", "Минск"}, {"менск", "Мінск"},
        {"Минск", "Минск"}, {"Менск", "Мінск"},
        {"брест", "Брест"}, {"brest", "Брест"}, {"брэст", "Брэст"},
        {"Брест", "Брест"}, {"Брэст", "Брэст"},
        {"витебск", "Витебск"}, {"vitebsk", "Витебск"}, {"віцебск", "Віцебск"},
        {"Витебск", "Витебск"}, {"Віцебск", "Віцебск"},
        {"гродно", "Гродно"}, {"grodno", "Гродно"}, {"гародня", "Гродна"},
        {"Гродно", "Гродно"}, {"Гародня", "Гродна"},
        {"могилёв", "Могилёв"}, {"могилев", "Могилёв"}, {"mogilev", "Могилёв"}, {"магілёў", "Магілёў"},
        {"Могилёв", "Могилёв"}, {"Могилев", "Могилёв"}, {"Магілёў", "Магілёў"},
        {"копыль", "Копыль"}, {"копыл", "Копыль"}, {"капы́ль", "Копыль"}, {"kapyl", "Kapyl"}, {"kopyl", "Kapyl"},
        {"Копыль", "Копыль"}, {"Копыл", "Копыль"}
    };

    string lower = ascii_lower_copy(cleaned);

    if (city_map.count(lower)) {
        return city_map[lower];
    }
    if (city_map.count(cleaned)) {
        return city_map[cleaned];
    }


    return cleaned;
}

WeatherInfo fetch_weather_info(string location, long long chat_id) {
    WeatherInfo info;
    location = normalize_location(location);

    auto r = fetch_weather_response(location + ",BY", chat_id);

    if (r.status_code != 200) {
        r = fetch_weather_response(location, chat_id);
    }

    if (r.status_code != 200) {
        return info;
    }

    try {
        auto data = json::parse(r.text);
        if (data.contains("sys") && data["sys"].contains("country") &&
            data["sys"]["country"].get<string>() != "BY") {
            return info;
        }

        info.ok = true;
        info.name = data["name"].get<string>();
        info.description = data["weather"][0]["description"].get<string>();
        info.temp = (int)round(data["main"]["temp"].get<double>());
        info.feels_like = (int)round(data["main"]["feels_like"].get<double>());
        info.humidity = data["main"]["humidity"].get<int>();
        info.wind_speed = data["wind"]["speed"].get<double>();

        string icon_code = data["weather"][0].contains("icon") ? data["weather"][0]["icon"].get<string>() : "";
        info.icon = weather_emoji_from_code(icon_code, info.description);
    } catch (...) {
        info.ok = false;
    }

    return info;
}

vector<WeatherForecastSlot> fetch_weather_forecast_slots(string location, long long chat_id, size_t limit = 8) {
    vector<WeatherForecastSlot> slots;
    location = normalize_location(location);

    auto r = fetch_weather_forecast_response(location + ",BY", chat_id);
    if (r.status_code != 200) {
        r = fetch_weather_forecast_response(location, chat_id);
    }
    if (r.status_code != 200) {
        return slots;
    }

    try {
        auto data = json::parse(r.text);
        if (!data.contains("list") || !data["list"].is_array()) {
            return slots;
        }
        if (data.contains("city") && data["city"].contains("country") &&
            data["city"]["country"].get<string>() != "BY") {
            return slots;
        }

        for (const auto& item : data["list"]) {
            if (slots.size() >= limit) break;

            WeatherForecastSlot slot;
            string dt_txt = item.contains("dt_txt") ? item["dt_txt"].get<string>() : "";
            slot.time = dt_txt.size() >= 16 ? dt_txt.substr(11, 5) : "";
            slot.temp = (int)round(item["main"]["temp"].get<double>());
            slot.feels_like = (int)round(item["main"]["feels_like"].get<double>());
            slot.humidity = item["main"]["humidity"].get<int>();
            slot.wind_speed = item.contains("wind") && item["wind"].contains("speed")
                ? item["wind"]["speed"].get<double>()
                : 0.0;
            slot.pop = item.contains("pop") ? (int)round(item["pop"].get<double>() * 100.0) : 0;

            if (item.contains("weather") && item["weather"].is_array() && !item["weather"].empty()) {
                slot.description = item["weather"][0].contains("description")
                    ? item["weather"][0]["description"].get<string>()
                    : "";
                string icon_code = item["weather"][0].contains("icon")
                    ? item["weather"][0]["icon"].get<string>()
                    : "";
                slot.icon = weather_emoji_from_code(icon_code, slot.description);
            } else {
                slot.icon = "🌡️";
            }

            if (item.contains("rain") && item["rain"].contains("3h")) {
                slot.rain_mm = item["rain"]["3h"].get<double>();
            }
            if (item.contains("snow") && item["snow"].contains("3h")) {
                slot.snow_mm = item["snow"]["3h"].get<double>();
            }

            slots.push_back(slot);
        }
    } catch (...) {
        slots.clear();
    }

    return slots;
}

void save_user_language(long long chat_id, const string& lang) {
    lock_guard<mutex> lock(state_mutex);
    user_language[chat_id] = lang;
    vector<string> lines;
    for (const auto& [uid, lg] : user_language) {
        lines.push_back(to_string(uid) + " " + lg);
    }
    write_lines_atomic(LANGUAGE_FILE, lines);
}

void load_languages() {
    lock_guard<mutex> lock(state_mutex);
    ifstream infile(LANGUAGE_FILE);
    long long id;
    string lang;
    while (infile >> id >> lang) {
        user_language[id] = lang;
    }
    infile.close();
}

// month: 1-12 name. Returns month name in user's language.
string get_month_name(int month, long long chat_id) {
    string lang = lang_of(chat_id);
    if (lang == "en") {
        switch(month) {
            case 1: return "January";
            case 2: return "February";
            case 3: return "March";
            case 4: return "April";
            case 5: return "May";
            case 6: return "June";
            case 7: return "July";
            case 8: return "August";
            case 9: return "September";
            case 10: return "October";
            case 11: return "November";
            case 12: return "December";
            default: return "";
        }
    } else if (lang == "be") {
        switch(month) {
            case 1: return "студзеня";
            case 2: return "лютага";
            case 3: return "сакавіка";
            case 4: return "красавіка";
            case 5: return "мая";
            case 6: return "чэрвеня";
            case 7: return "ліпеня";
            case 8: return "жніўня";
            case 9: return "верасня";
            case 10: return "кастрычніка";
            case 11: return "лістапада";
            case 12: return "снежня";
            default: return "";
        }
    } else {
        switch(month) {
            case 1: return "января";
            case 2: return "февраля";
            case 3: return "марта";
            case 4: return "апреля";
            case 5: return "мая";
            case 6: return "июня";
            case 7: return "июля";
            case 8: return "августа";
            case 9: return "сентября";
            case 10: return "октября";
            case 11: return "ноября";
            case 12: return "декабря";
            default: return "";
        }
    }
}

void save_user(long long chat_id) {
    lock_guard<mutex> lock(state_mutex);
    if (active_users.find(chat_id) == active_users.end()) {
        active_users.insert(chat_id);
        vector<string> lines;
        for (long long uid : active_users) {
            lines.push_back(to_string(uid));
        }
        write_lines_atomic(USERS_FILE, lines);
    }
}

void load_users() {
    lock_guard<mutex> lock(state_mutex);
    ifstream infile(USERS_FILE);
    long long chat_id;
    while (infile >> chat_id) active_users.insert(chat_id);
    infile.close();
}

void save_user_city(long long chat_id, const string& city) {
    lock_guard<mutex> lock(state_mutex);
    user_city[chat_id] = city;
    vector<string> lines;
    for (const auto& [uid, ucity] : user_city) {
        lines.push_back(to_string(uid) + " " + ucity);
    }
    write_lines_atomic(CITIES_FILE, lines);
}

void load_user_cities() {
    lock_guard<mutex> lock(state_mutex);
    ifstream infile(CITIES_FILE);
    long long id;
    string city;
    while (infile >> id >> ws && getline(infile, city)) {
        user_city[id] = city;
    }
    infile.close();
}

void save_notification_status(long long chat_id, bool enabled) {
    lock_guard<mutex> lock(state_mutex);
    user_notifications[chat_id] = enabled;
    vector<string> lines;
    for (const auto& [uid, st] : user_notifications) {
        lines.push_back(to_string(uid) + " " + to_string(st));
    }
    write_lines_atomic(NOTIFICATIONS_FILE, lines);
}

void load_notifications() {
    lock_guard<mutex> lock(state_mutex);
    ifstream infile(NOTIFICATIONS_FILE);
    long long id;
    bool status;
    while (infile >> id >> status) {
        user_notifications[id] = status;
    }
    infile.close();
}

void save_live_message_id(long long chat_id, int message_id) {
    lock_guard<mutex> lock(live_message_mutex);
    live_message_id[chat_id] = message_id;

    vector<string> lines;
    for (const auto& [uid, saved_mid] : live_message_id) {
        lines.push_back(to_string(uid) + " " + to_string(saved_mid));
    }
    write_lines_atomic(LIVE_MESSAGES_FILE, lines);
}

int known_live_message_id(long long chat_id) {
    lock_guard<mutex> lock(live_message_mutex);
    auto it = live_message_id.find(chat_id);
    return it == live_message_id.end() ? 0 : it->second;
}

void save_supplement_message_id(long long chat_id, int message_id) {
    lock_guard<mutex> lock(live_message_mutex);
    if (message_id > 0) {
        supplement_message_id[chat_id] = message_id;
    } else {
        supplement_message_id.erase(chat_id);
    }

    vector<string> lines;
    for (const auto& [uid, saved_mid] : supplement_message_id) {
        lines.push_back(to_string(uid) + " " + to_string(saved_mid));
    }
    write_lines_atomic(SUPPLEMENT_MESSAGES_FILE, lines);
}

void load_live_messages() {
    lock_guard<mutex> lock(live_message_mutex);
    ifstream infile(LIVE_MESSAGES_FILE);
    long long id;
    int mid;
    while (infile >> id >> mid) {
        live_message_id[id] = mid;
    }
    infile.close();
}

void load_supplement_messages() {
    lock_guard<mutex> lock(live_message_mutex);
    ifstream infile(SUPPLEMENT_MESSAGES_FILE);
    long long id;
    int mid;
    while (infile >> id >> mid) {
        supplement_message_id[id] = mid;
    }
    infile.close();
}

bool is_notifications_enabled(long long chat_id) {
    lock_guard<mutex> lock(state_mutex);
    auto it = user_notifications.find(chat_id);
    if (it != user_notifications.end()) {
        return it->second;
    }
    return true;
}

vector<long long> active_user_snapshot() {
    lock_guard<mutex> lock(state_mutex);
    return vector<long long>(active_users.begin(), active_users.end());
}

string user_city_or_default(long long chat_id) {
    lock_guard<mutex> lock(state_mutex);
    auto it = user_city.find(chat_id);
    if (it != user_city.end() && !it->second.empty()) {
        return it->second;
    }
    return "Минск";
}

tm get_minsk_time(int offset_days = 0) {
    auto now = chrono::system_clock::now();
    auto minsk_now = now + chrono::hours(3) + chrono::hours(24 * offset_days);
    time_t t = chrono::system_clock::to_time_t(minsk_now);
    tm result;
#ifdef _WIN32
    gmtime_s(&result, &t);
#else
    gmtime_r(&t, &result);
#endif
    return result;
}

string get_weekday_name(int offset_days, long long chat_id) {
    tm ltm = get_minsk_time(offset_days);
    char buf[64];
    strftime(buf, sizeof(buf), "%A", &ltm);
    string w(buf);

    string lang = lang_of(chat_id);
    if (lang == "en") {
        return w;
    } else if (lang == "be") {
        if (w == "Monday") return "панядзелак";
        if (w == "Tuesday") return "аўторак";
        if (w == "Wednesday") return "серада";
        if (w == "Thursday") return "чацвер";
        if (w == "Friday") return "пятніца";
        if (w == "Saturday") return "субота";
        if (w == "Sunday") return "нядзеля";
        return w;
    } else {
        if (w == "Monday") return "Понедельник";
        if (w == "Tuesday") return "Вторник";
        if (w == "Wednesday") return "Среда";
        if (w == "Thursday") return "Четверг";
        if (w == "Friday") return "Пятница";
        if (w == "Saturday") return "Суббота";
        if (w == "Sunday") return "Воскресенье";
        return w;
    }
}

string get_kp_status(double kp, long long chat_id) {
    string lang = lang_of(chat_id);

    if (lang == "en") {
        if (kp < 4.0) return "Geomagnetic conditions are quiet now.";
        if (kp < 5.0) return "Geomagnetic conditions are mildly disturbed now.";
        if (kp < 6.0) return "Geomagnetic conditions now: G1 magnetic storm. Sensitive people may feel discomfort.";
        if (kp < 7.0) return "Geomagnetic conditions now: G2 magnetic storm. Reduce unnecessary stress if you feel unwell.";
        if (kp < 8.0) return "Geomagnetic conditions now: strong G3 storm. Monitor how you feel and keep routines calmer.";
        if (kp < 9.0) return "Geomagnetic conditions now: severe G4 storm. Be attentive to wellbeing and official space-weather updates.";
        return "Geomagnetic conditions now: extreme G5 storm. Follow official updates and seek medical help if symptoms are serious.";
    } else if (lang == "be") {
        if (kp < 4.0) return "Цяпер геамагнітная абстаноўка спакойная.";
        if (kp < 5.0) return "Цяпер геамагнітная абстаноўка слаба ўзрушаная.";
        if (kp < 6.0) return "Цяпер геамагнітная абстаноўка: магнітная бура G1. Адчувальныя людзі могуць адчуваць дыскамфорт.";
        if (kp < 7.0) return "Цяпер геамагнітная абстаноўка: магнітная бура G2. Калі самаадчуванне горшае, знізьце лішнія нагрузкі.";
        if (kp < 8.0) return "Цяпер геамагнітная абстаноўка: моцная бура G3. Сачыце за самаадчуваннем і зрабіце дзень спакайнейшым.";
        if (kp < 9.0) return "Цяпер геамагнітная абстаноўка: вельмі моцная бура G4. Будзьце ўважлівыя да сябе і афіцыйных абнаўленняў.";
        return "Цяпер геамагнітная абстаноўка: экстрэмальная бура G5. Сачыце за афіцыйнымі абнаўленнямі і звяртайцеся па меддапамогу пры сур'ёзных сімптомах.";
    } else {
        if (kp < 4.0) return "Сейчас геомагнитная обстановка спокойная.";
        if (kp < 5.0) return "Сейчас геомагнитная обстановка слегка возмущённая.";
        if (kp < 6.0) return "Сейчас геомагнитная обстановка: магнитная буря G1. Чувствительные люди могут ощущать дискомфорт.";
        if (kp < 7.0) return "Сейчас геомагнитная обстановка: магнитная буря G2. Если самочувствие хуже, снизьте лишние нагрузки.";
        if (kp < 8.0) return "Сейчас геомагнитная обстановка: сильная буря G3. Следите за самочувствием и сделайте день спокойнее.";
        if (kp < 9.0) return "Сейчас геомагнитная обстановка: очень сильная буря G4. Будьте внимательны к себе и официальным обновлениям.";
        return "Сейчас геомагнитная обстановка: экстремальная буря G5. Следите за официальными обновлениями и обращайтесь за медпомощью при серьёзных симптомах.";
    }
}

string get_current_kp_guidance(double kp, long long chat_id) {
    string lang = lang_of(chat_id);

    if (lang == "en") {
        if (kp < 4.0) {
            return "**Status:** quiet geomagnetic field. The background is stable; no magnetic storm is expected right now.\n\n"
                   "**Recommendations:** keep your normal routine, hydrate as usual, and use the forecast screen only for planning the next days.";
        }
        if (kp < 5.0) {
            return "**Status:** unsettled geomagnetic field. Weak disturbances are possible, but this is still below storm level.\n\n"
                   "**Recommendations:** keep the day steady, avoid unnecessary overload if you are weather-sensitive, and watch for Kp growth toward 5.";
        }
        if (kp < 6.0) {
            return "**Status:** minor geomagnetic storm G1. Sensitive people may notice headache, fatigue, sleepiness or pressure discomfort.\n\n"
                   "**Recommendations:** monitor blood pressure, pulse and heartbeat; note dizziness, unusual weakness, headache or shortness of breath. Keep the load light, drink water, rest, and take prescribed medicines only as your doctor instructed.";
        }
        if (kp < 7.0) {
            return "**Status:** moderate geomagnetic storm G2. The disturbance is noticeable; sensitive people should be more careful.\n\n"
                   "**Recommendations:** check arterial pressure and pulse more carefully, watch for palpitations, chest pressure, shortness of breath, dizziness or unusual fatigue. Avoid heavy exertion, alcohol and overwork. Seek urgent medical help for chest pain, fainting, severe shortness of breath, confusion, weakness or numbness.";
        }
        return "**Status:** strong geomagnetic storm. Conditions are disturbed and can stay unstable for several hours.\n\n"
               "**Recommendations:** monitor blood pressure, pulse rhythm, heartbeat and overall condition. Keep the day calm, avoid intense exercise and stress. If you have heart disease, hypertension or arrhythmia, follow your doctor's plan. Seek urgent care for chest pain, fainting, severe shortness of breath, sudden weakness, numbness, confusion or vision/speech problems.";
    }

    if (lang == "be") {
        if (kp < 4.0) {
            return "**Статус:** спакойнае геамагнітнае поле. Фон стабільны, магнітнай буры цяпер няма.\n\n"
                   "**Рэкамендацыі:** захоўвайце звычайны рэжым, піце ваду як звычайна і глядзіце прагноз толькі для планавання наступных дзён.";
        }
        if (kp < 5.0) {
            return "**Статус:** няўстойлівае геамагнітнае поле. Магчымыя слабыя ўзрушэнні, але гэта яшчэ не ўзровень буры.\n\n"
                   "**Рэкамендацыі:** зрабіце дзень раўнейшым, пазбягайце лішняй нагрузкі пры метэаадчувальнасці і сачыце, ці не расце Kp да 5.";
        }
        if (kp < 6.0) {
            return "**Статус:** слабая магнітная бура G1. Адчувальныя людзі могуць заўважыць галаўны боль, стомленасць, санлівасць або дыскамфорт з ціскам.\n\n"
                   "**Рэкамендацыі:** кантралюйце артэрыяльны ціск, пульс і сэрцабіцце; звяртайце ўвагу на галавакружэнне, незвычайную слабасць, галаўны боль або задышку. Знізьце нагрузку, піце ваду, адпачывайце і прымайце прызначаныя лекі толькі па схеме лекара.";
        }
        if (kp < 7.0) {
            return "**Статус:** умераная магнітная бура G2. Узрушэнне ўжо прыкметнае; адчувальным людзям варта быць больш уважлівымі.\n\n"
                   "**Рэкамендацыі:** часцей правярайце артэрыяльны ціск і пульс, сачыце за сэрцабіццем, ціскам у грудзях, задышкай, галавакружэннем і незвычайнай стомленасцю. Пазбягайце цяжкай нагрузкі, алкаголю і ператамлення. Тэрмінова звяртайцеся па меддапамогу пры болі ў грудзях, непрытомнасці, моцнай задышцы, спутанасці, слабасці або здранцвенні.";
        }
        return "**Статус:** моцная магнітная бура. Абстаноўка ўзрушаная і можа заставацца нестабільнай некалькі гадзін.\n\n"
               "**Рэкамендацыі:** кантралюйце ціск, пульс, рытм сэрца і агульны стан. Захоўвайце спакойны рэжым, пазбягайце інтэнсіўных нагрузак і стрэсу. Калі ёсць хваробы сэрца, гіпертанія або арытмія, дзейнічайце па плане лекара. Тэрмінова па дапамогу пры болі ў грудзях, непрытомнасці, моцнай задышцы, раптоўнай слабасці, здранцвенні, спутанасці, парушэнні зроку або маўлення.";
    }

    if (kp < 4.0) {
        return "**Статус:** спокойное геомагнитное поле. Фон стабильный, магнитной бури сейчас нет.\n\n"
               "**Рекомендации:** сохраняйте обычный режим, пейте воду как обычно и используйте прогноз только для планирования ближайших дней.";
    }
    if (kp < 5.0) {
        return "**Статус:** неустойчивое геомагнитное поле. Возможны слабые возмущения, но это ещё не уровень магнитной бури.\n\n"
               "**Рекомендации:** держите день ровным, избегайте лишней нагрузки при метеочувствительности и следите, не растёт ли Kp к 5.";
    }
    if (kp < 6.0) {
        return "**Статус:** слабая магнитная буря G1. Чувствительные люди могут заметить головную боль, усталость, сонливость или дискомфорт с давлением.\n\n"
               "**Рекомендации:** контролируйте артериальное давление, пульс и сердцебиение; отслеживайте головокружение, необычную слабость, головную боль или одышку. Снизьте нагрузку, пейте воду, отдыхайте и принимайте назначенные препараты только по схеме врача.";
    }
    if (kp < 7.0) {
        return "**Статус:** умеренная магнитная буря G2. Возмущение уже заметное; чувствительным людям стоит быть внимательнее.\n\n"
               "**Рекомендации:** чаще проверяйте артериальное давление и пульс, следите за сердцебиением, давлением в груди, одышкой, головокружением и необычной усталостью. Избегайте тяжёлой нагрузки, алкоголя и переработки. Срочно обращайтесь за медпомощью при боли в груди, сильной одышке, спутанности, слабости или онемении.";
    }
    return "**Статус:** сильная магнитная буря. Обстановка возмущённая и может оставаться нестабильной несколько часов.\n\n"
           "**Рекомендации:** контролируйте давление, пульс, ритм сердца и общее состояние. Держите день спокойным, избегайте интенсивных тренировок и стресса. Если есть болезни сердца, гипертония или аритмия, действуйте по плану врача. Срочно за помощью при боли в груди, сильной одышке, внезапной слабости, онемении, спутанности, нарушении зрения или речи.";
}

double fetch_current_kp() {
    auto r = cpr::Get(cpr::Url{"https://services.swpc.noaa.gov/json/planetary_k_index_1m.json"},
                      cpr::Timeout{8000});

    if (r.status_code == 200) {
        try {
            auto data = json::parse(r.text);
            if (data.is_array() && !data.empty()) {
                auto& last = data.back();
                if (last.contains("estimated_kp")) {
                    return last["estimated_kp"].get<double>();
                } else if (last.contains("kp_index")) {
                    return last["kp_index"].get<double>();
                }
            }
        } catch (const exception& e) {
            cerr << "Ошибка парсинга текущего Kp: " << e.what() << endl;
        }
    }
    cerr << "Не удалось получить текущий Kp: HTTP " << r.status_code << endl;
    return -1.0;
}

string kp_short_label(double kp, long long chat_id);

vector<KpForecast> fetch_kp_forecast_3day(long long chat_id) {
    vector<KpForecast> forecast;

    auto r = cpr::Get(cpr::Url{"https://services.swpc.noaa.gov/text/3-day-geomag-forecast.txt"},
                      cpr::Timeout{10000});

    if (r.status_code != 200) {
        return forecast;
    }

    try {
        stringstream ss(r.text);
        string line;

        vector<vector<double>> day_values(3);

        vector<string> dates;
        for (int day_offset = 0; day_offset < 3; day_offset++) {
            tm day = get_minsk_time(day_offset);
            stringstream date_ss;
            date_ss << setw(2) << setfill('0') << day.tm_mday << "."
                    << setw(2) << setfill('0') << (day.tm_mon + 1);
            dates.push_back(date_ss.str());
        }

        ss.clear();
        ss.str(r.text);

        while (getline(ss, line)) {
            if (line.find("UT") != string::npos && line.find("UTC") == string::npos) {
                vector<double> nums;
                stringstream line_ss(line);
                string token;

                while (line_ss >> token) {
                    if (token.find("UT") != string::npos) continue;
                    if (token.find("-") != string::npos) continue;
                    try {
                        double num = stod(token);
                        if (num >= 0 && num <= 10) {
                            nums.push_back(num);
                        }
                    } catch (...) {}
                }

                if (nums.size() >= 3) {
                    for (int i = 0; i < 3; i++) {
                        if (day_values[i].size() < 8) {
                            day_values[i].push_back(nums[i]);
                        }
                    }
                }
            }
        }

        string lang = lang_of(chat_id);

        for (int i = 0; i < 3; i++) {
            if (day_values[i].empty()) continue;

            KpForecast fc;
            fc.date = dates[i];
            fc.values = day_values[i];
            fc.max_kp = 0.0;

            for (double val : fc.values) {
                if (val > fc.max_kp) fc.max_kp = val;
            }

            fc.status = kp_short_label(fc.max_kp, chat_id);

            forecast.push_back(fc);
        }

    } catch (const exception& e) {
        cerr << "Ошибка парсинга прогноза: " << e.what() << endl;
    }

    return forecast;
}

struct ScreenView {
    string kind = "status";
    string eyebrow;
    string title;
    string subtitle;
    string body;
    string footer;
    string supplement;
    string city;
    double kp = -1.0;
    bool alert = false;
    bool show_weather = false;
    int forecast_page = -1;
    int forecast_total = 0;
    string page_callback = "forecast";
    WeatherInfo weather;
    vector<WeatherForecastSlot> weather_slots;
    vector<KpForecast> forecast;
    vector<KpForecast> daily_storm_summary;
};

string lang_of(long long chat_id) {
    lock_guard<mutex> lock(state_mutex);
    auto it = user_language.find(chat_id);
    return it != user_language.end() ? it->second : "ru";
}

string localize(long long chat_id, const string& ru, const string& be, const string& en) {
    string lang = lang_of(chat_id);
    if (lang == "en") return en;
    if (lang == "be") return be;
    return ru;
}

bool kp_available(double kp) {
    return kp >= 0.0;
}

string kp_unavailable_text(long long chat_id) {
    return localize(chat_id,
        "Данные NOAA сейчас недоступны. Попробуйте обновить экран позже.",
        "Дадзеныя NOAA цяпер недаступныя. Паспрабуйце абнавіць экран пазней.",
        "NOAA data is currently unavailable. Try refreshing the screen later.");
}

string wind_unit(long long chat_id) {
    return lang_of(chat_id) == "en" ? "m/s" : "м/с";
}

string html_escape(const string& value) {
    string out;
    out.reserve(value.size());
    for (char c : value) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            case '\n': out += "<br>"; break;
            default: out += c; break;
        }
    }
    return out;
}

string markdown_to_html(const string& value) {
    string out;
    bool bold = false;
    for (size_t i = 0; i < value.size(); i++) {
        if (i + 1 < value.size() && value[i] == '*' && value[i + 1] == '*') {
            out += bold ? "</b>" : "<b>";
            bold = !bold;
            i++;
            continue;
        }

        switch (value[i]) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            case '\n': out += "<br>"; break;
            default: out += value[i]; break;
        }
    }
    if (bold) out += "</b>";
    return out;
}

string markdown_to_telegram_html(const string& value) {
    string out;
    bool bold = false;
    for (size_t i = 0; i < value.size(); i++) {
        if (i + 1 < value.size() && value[i] == '*' && value[i + 1] == '*') {
            out += bold ? "</b>" : "<b>";
            bold = !bold;
            i++;
            continue;
        }

        switch (value[i]) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            default: out += value[i]; break;
        }
    }
    if (bold) out += "</b>";
    return out;
}

string shell_quote(const string& value) {
    string out = "'";
    for (char c : value) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    out += "'";
    return out;
}

string format_double_1(double value) {
    stringstream ss;
    ss << fixed << setprecision(1) << value;
    return ss.str();
}

string kp_color(double kp) {
    if (kp < 4.0) return "#1fa463";
    if (kp < 5.0) return "#d7a316";
    if (kp < 6.0) return "#e86f1c";
    if (kp < 7.0) return "#d92d20";
    if (kp < 8.0) return "#7b3fe4";
    return "#232326";
}

string storm_level_label(double kp) {
    if (kp < 5.0) return "";
    if (kp < 6.0) return "G1";
    if (kp < 7.0) return "G2";
    if (kp < 8.0) return "G3";
    if (kp < 9.0) return "G4";
    return "G5";
}

string kp_short_label(double kp, long long chat_id) {
    if (kp < 4.0) {
        return localize(chat_id,
            "Спокойное геомагнитное поле",
            "Спакойнае геамагнітнае поле",
            "Quiet geomagnetic field");
    }
    if (kp < 5.0) {
        return localize(chat_id,
            "Неустойчивое поле, слабые возмущения",
            "Няўстойлівае поле, слабыя ўзрушэнні",
            "Unsettled geomagnetic field");
    }
    if (kp < 6.0) {
        return localize(chat_id,
            "Слабая магнитная буря G1",
            "Слабая магнітная бура G1",
            "Minor geomagnetic storm G1");
    }
    if (kp < 7.0) {
        return localize(chat_id,
            "Умеренная магнитная буря G2",
            "Умераная магнітная бура G2",
            "Moderate geomagnetic storm G2");
    }
    if (kp < 8.0) {
        return localize(chat_id,
            "Сильная магнитная буря G3",
            "Моцная магнітная бура G3",
            "Strong geomagnetic storm G3");
    }
    if (kp < 9.0) {
        return localize(chat_id,
            "Тяжёлая магнитная буря G4",
            "Цяжкая магнітная бура G4",
            "Severe geomagnetic storm G4");
    }
    return localize(chat_id,
        "Экстремальная магнитная буря G5",
        "Экстрэмальная магнітная бура G5",
        "Extreme geomagnetic storm G5");
}

string format_precipitation(const WeatherForecastSlot& slot, long long chat_id) {
    double total_mm = slot.rain_mm + slot.snow_mm;
    if (total_mm >= 0.1) {
        return format_double_1(total_mm) + " мм";
    }
    if (slot.pop > 0) {
        return to_string(slot.pop) + "%";
    }
    return localize(chat_id, "без осадков", "без ападкаў", "dry");
}

string kp_slot_hour(size_t index) {
    stringstream hour;
    hour << setw(2) << setfill('0') << (int)(index * 3) << ":00";
    return hour.str();
}

string morning_kp_detail(long long chat_id, double current_kp, const vector<KpForecast>& forecast) {
    string text = localize(chat_id,
        "Сейчас Kp " + format_double_1(current_kp) + " - " + kp_short_label(current_kp, chat_id) + ".",
        "Цяпер Kp " + format_double_1(current_kp) + " - " + kp_short_label(current_kp, chat_id) + ".",
        "Current Kp is " + format_double_1(current_kp) + " - " + kp_short_label(current_kp, chat_id) + ".");

    if (forecast.empty() || forecast.front().values.empty()) {
        text += " " + localize(chat_id,
            "Диапазон Kp на день сейчас недоступен.",
            "Дыяпазон Kp на дзень цяпер недаступны.",
            "The daily Kp range is currently unavailable.");
        return text;
    }

    const KpForecast& today = forecast.front();
    double min_kp = today.values.front();
    double max_kp = today.values.front();
    size_t min_index = 0;
    size_t max_index = 0;
    for (size_t i = 0; i < today.values.size(); i++) {
        if (today.values[i] < min_kp) {
            min_kp = today.values[i];
            min_index = i;
        }
        if (today.values[i] > max_kp) {
            max_kp = today.values[i];
            max_index = i;
        }
    }

    text += " " + localize(chat_id,
        "По прогнозу NOAA на сегодня минимум ожидается Kp " + format_double_1(min_kp) +
            " около " + kp_slot_hour(min_index) + ", максимум - Kp " + format_double_1(max_kp) +
            " около " + kp_slot_hour(max_index) + ".",
        "Паводле прагнозу NOAA на сёння мінімум чакаецца Kp " + format_double_1(min_kp) +
            " каля " + kp_slot_hour(min_index) + ", максімум - Kp " + format_double_1(max_kp) +
            " каля " + kp_slot_hour(max_index) + ".",
        "NOAA forecast for today expects a minimum of Kp " + format_double_1(min_kp) +
            " around " + kp_slot_hour(min_index) + " and a maximum of Kp " + format_double_1(max_kp) +
            " around " + kp_slot_hour(max_index) + ".");

    if (max_kp >= 5.0) {
        text += " " + localize(chat_id,
            "В течение дня возможна магнитная буря уровня " + storm_level_label(max_kp) + ".",
            "На працягу дня магчыма магнітная бура ўзроўню " + storm_level_label(max_kp) + ".",
            "A " + storm_level_label(max_kp) + " geomagnetic storm is possible during the day.");
    } else if (max_kp >= 4.0) {
        text += " " + localize(chat_id,
            "До уровня бури не доходит, но возможны слабые возмущения.",
            "Да ўзроўню буры не даходзіць, але магчымыя слабыя ўзрушэнні.",
            "Storm level is not expected, but weak disturbances are possible.");
    } else {
        text += " " + localize(chat_id,
            "Магнитная буря по дневному прогнозу не ожидается.",
            "Магнітная бура паводле дзённага прагнозу не чакаецца.",
            "No geomagnetic storm is expected in the daily forecast.");
    }

    return text;
}

string morning_weather_detail(long long chat_id, const WeatherInfo& weather, const vector<WeatherForecastSlot>& slots) {
    string text = localize(chat_id,
        "Погода сейчас в городе " + weather.name + ": " + weather.description + ", " +
            to_string(weather.temp) + "°C, ощущается как " + to_string(weather.feels_like) +
            "°C, ветер " + to_string((int)round(weather.wind_speed)) + " " + wind_unit(chat_id) + ".",
        "Надвор'е цяпер у горадзе " + weather.name + ": " + weather.description + ", " +
            to_string(weather.temp) + "°C, адчуваецца як " + to_string(weather.feels_like) +
            "°C, вецер " + to_string((int)round(weather.wind_speed)) + " " + wind_unit(chat_id) + ".",
        "Weather now in " + weather.name + ": " + weather.description + ", " +
            to_string(weather.temp) + "°C, feels like " + to_string(weather.feels_like) +
            "°C, wind " + to_string((int)round(weather.wind_speed)) + " " + wind_unit(chat_id) + ".");

    if (slots.empty()) {
        text += " " + localize(chat_id,
            "Почасовой прогноз погоды сейчас недоступен.",
            "Пагадзінны прагноз надвор'я цяпер недаступны.",
            "The hourly weather forecast is currently unavailable.");
        return text;
    }

    int min_temp = slots.front().temp;
    int max_temp = slots.front().temp;
    int max_pop = slots.front().pop;
    double max_precip_mm = slots.front().rain_mm + slots.front().snow_mm;
    double max_wind = slots.front().wind_speed;
    const WeatherForecastSlot* last_slot = &slots.front();

    for (const auto& slot : slots) {
        min_temp = min(min_temp, slot.temp);
        max_temp = max(max_temp, slot.temp);
        max_pop = max(max_pop, slot.pop);
        max_precip_mm = max(max_precip_mm, slot.rain_mm + slot.snow_mm);
        max_wind = max(max_wind, slot.wind_speed);
        last_slot = &slot;
    }

    string range = min_temp == max_temp
        ? to_string(min_temp) + "°C"
        : to_string(min_temp) + "..." + to_string(max_temp) + "°C";

    string precipitation = max_precip_mm >= 0.1
        ? format_double_1(max_precip_mm) + " мм"
        : (max_pop > 0
            ? to_string(max_pop) + "%"
            : localize(chat_id, "без заметных осадков", "без прыкметных ападкаў", "no notable precipitation"));

    text += " " + localize(chat_id,
        "В ближайшие часы ожидается " + range + "; к " + last_slot->time + " вероятнее всего " +
            last_slot->description + ", осадки: " + precipitation + ", ветер до " +
            to_string((int)round(max_wind)) + " " + wind_unit(chat_id) + ".",
        "У найбліжэйшыя гадзіны чакаецца " + range + "; да " + last_slot->time + " найбольш верагодна " +
            last_slot->description + ", ападкі: " + precipitation + ", вецер да " +
            to_string((int)round(max_wind)) + " " + wind_unit(chat_id) + ".",
        "In the next hours expect " + range + "; by " + last_slot->time + " conditions are likely " +
            last_slot->description + ", precipitation: " + precipitation + ", wind up to " +
            to_string((int)round(max_wind)) + " " + wind_unit(chat_id) + ".");

    return text;
}

string forecast_bursts_summary(const KpForecast& fc, long long chat_id) {
    if (fc.values.empty()) {
        return localize(chat_id,
            "Почасовые значения на этот день сейчас недоступны.",
            "Пагадзінныя значэнні на гэты дзень цяпер недаступныя.",
            "Hourly values for this day are currently unavailable.");
    }

    vector<string> storm_hours;
    vector<string> disturbed_hours;
    double max_kp = -1.0;
    size_t peak_index = 0;
    for (size_t i = 0; i < fc.values.size(); i++) {
        double value = fc.values[i];
        if (value > max_kp) {
            max_kp = value;
            peak_index = i;
        }
        stringstream hour;
        hour << setw(2) << setfill('0') << (int)(i * 3) << ":00";
        string item = hour.str() + " - Kp " + format_double_1(value);
        if (value >= 5.0) {
            string level = storm_level_label(value);
            if (!level.empty()) item += " (" + level + ")";
            storm_hours.push_back(item);
        } else if (value >= 4.0) {
            disturbed_hours.push_back(item);
        }
    }

    stringstream peak_hour;
    peak_hour << setw(2) << setfill('0') << (int)(peak_index * 3) << ":00";

    if (!storm_hours.empty()) {
        string summary = localize(chat_id,
            "На протяжении дня ожидается магнитная буря. Пик около " + peak_hour.str() + ", максимум Kp " + format_double_1(max_kp) + " " + storm_level_label(max_kp) + ".",
            "На працягу дня чакаецца магнітная бура. Пік каля " + peak_hour.str() + ", максімум Kp " + format_double_1(max_kp) + " " + storm_level_label(max_kp) + ".",
            "A geomagnetic storm is expected during the day. Peak around " + peak_hour.str() + ", maximum Kp " + format_double_1(max_kp) + " " + storm_level_label(max_kp) + "."
        );
        summary += "\n" + localize(chat_id, "Часы риска: ", "Гадзіны рызыкі: ", "Risk hours: ");
        for (size_t i = 0; i < storm_hours.size(); i++) {
            if (i > 0) summary += "; ";
            summary += storm_hours[i];
        }
        return summary;
    }

    if (!disturbed_hours.empty()) {
        string summary = localize(chat_id,
            "Магнитная буря не ожидается, но возможны слабые возмущения. Максимум около " + peak_hour.str() + ": Kp " + format_double_1(max_kp) + ".",
            "Магнітная бура не чакаецца, але магчымыя слабыя ўзрушэнні. Максімум каля " + peak_hour.str() + ": Kp " + format_double_1(max_kp) + ".",
            "No geomagnetic storm is expected, but weak disturbances are possible. Peak around " + peak_hour.str() + ": Kp " + format_double_1(max_kp) + "."
        );
        summary += "\n" + localize(chat_id, "Возмущённые часы: ", "Узрушаныя гадзіны: ", "Disturbed hours: ");
        for (size_t i = 0; i < disturbed_hours.size(); i++) {
            if (i > 0) summary += "; ";
            summary += disturbed_hours[i];
        }
        return summary;
    }

    return localize(chat_id,
        "Бурь не ожидается. Фон спокойный. Пик около " + peak_hour.str() + ".",
        "Бур не чакаецца. Фон спакойны. Пік каля " + peak_hour.str() + ".",
        "No storms expected. Conditions are quiet. Peak around " + peak_hour.str() + "."
    );
}

string forecast_day_supplement(long long chat_id, const KpForecast& fc) {
    string text = localize(chat_id,
        "**Прогноз на " + fc.date + "**",
        "**Прагноз на " + fc.date + "**",
        "**Forecast for " + fc.date + "**");
    text += "\n\n";
    text += forecast_bursts_summary(fc, chat_id);

    text += "\n\n";
    if (fc.max_kp >= 5.0) {
        text += get_current_kp_guidance(fc.max_kp, chat_id);
    } else if (fc.max_kp >= 4.0) {
        text += localize(chat_id,
            "День лучше держать спокойным. Если вы метеочувствительны, следите за давлением, пульсом, сердцебиением и общим состоянием; избегайте лишней нагрузки и недосыпа.",
            "Дзень лепш трымаць спакойным. Калі вы метэаадчувальныя, сачыце за ціскам, пульсам, сэрцабіццем і агульным станам; пазбягайце лішняй нагрузкі і недасыпу.",
            "Keep the day steady. If you are weather-sensitive, monitor blood pressure, pulse, heartbeat and overall condition; avoid unnecessary load and lack of sleep.");
    } else {
        text += localize(chat_id,
            "Специальных ограничений нет. Сохраняйте обычный режим, пейте воду и проверяйте прогноз, если состояние меняется.",
            "Спецыяльных абмежаванняў няма. Захоўвайце звычайны рэжым, піце ваду і правярайце прагноз, калі стан змяняецца.",
            "No special restrictions. Keep your normal routine, hydrate, and check the forecast if your condition changes.");
    }

    return text;
}

string weather_supplement_text(long long chat_id, const WeatherInfo& weather, const vector<WeatherForecastSlot>&, bool saved_city) {
    string text = saved_city
        ? localize(chat_id,
            "**Город " + weather.name + " сохранён.** Утренняя рассылка будет показывать погоду для этого города.",
            "**Горад " + weather.name + " захаваны.** Ранішняя рассылка будзе паказваць надвор'е для гэтага горада.",
            "**City " + weather.name + " saved.** The morning report will show weather for this city.")
        : localize(chat_id,
            "**Вашему вниманию прогноз погоды в городе " + weather.name + ".**",
            "**Вашай увазе прагноз надвор'я ў горадзе " + weather.name + ".**",
            "**Weather forecast for " + weather.name + ".**");
    return text;
}

string current_minsk_datetime(long long chat_id) {
    tm ltm = get_minsk_time();
    stringstream ss;
    ss << setw(2) << setfill('0') << ltm.tm_mday << " "
       << get_month_name(ltm.tm_mon + 1, chat_id) << " "
       << (ltm.tm_year + 1900) << " · "
       << setw(2) << setfill('0') << ltm.tm_hour << ":"
       << setw(2) << setfill('0') << ltm.tm_min;
    return ss.str();
}

json make_inline_keyboard(long long chat_id, int forecast_page = -1, int forecast_total = 0, const string& page_callback = "forecast") {
    string notifications_btn = is_notifications_enabled(chat_id) ? get_text(chat_id, "btn_notify_off") : get_text(chat_id, "btn_notify_on");

    string current_lang = lang_of(chat_id);
    string lang_btn;
    if (current_lang == "ru") {
        lang_btn = "🇬🇧 English";
    } else if (current_lang == "en") {
        lang_btn = "🇧🇾 Беларуская";
    } else {
        lang_btn = "🇷🇺 Русский";
    }

    json rows = json::array();

    if (forecast_page >= 0 && forecast_total > 1) {
        json nav_row = json::array();
        if (forecast_page > 0) {
            string prev_text = page_callback == "morning"
                ? (forecast_page == 1
                    ? localize(chat_id, "← Утро", "← Раніца", "← Morning")
                    : localize(chat_id, "← Назад", "← Назад", "← Back"))
                : get_text(chat_id, "btn_forecast_prev");
            nav_row.push_back({{"text", prev_text}, {"callback_data", page_callback + ":" + to_string(forecast_page - 1)}});
        }
        if (forecast_page + 1 < forecast_total) {
            string next_text = page_callback == "morning"
                ? (forecast_page == 0
                    ? localize(chat_id, "Прогноз на 3 дня →", "Прагноз на 3 дні →", "3-day forecast →")
                    : localize(chat_id, "Дальше →", "Далей →", "Next →"))
                : get_text(chat_id, "btn_forecast_next");
            nav_row.push_back({{"text", next_text}, {"callback_data", page_callback + ":" + to_string(forecast_page + 1)}});
        }
        if (!nav_row.empty()) {
            rows.push_back(nav_row);
        }
    }

    rows.push_back(json::array({{{"text", get_text(chat_id, "btn_current")}, {"callback_data", "current"}}}));
    rows.push_back(json::array({{{"text", get_text(chat_id, "btn_forecast")}, {"callback_data", "forecast:0"}}}));
    rows.push_back(json::array({{{"text", get_text(chat_id, "btn_weather")}, {"callback_data", "weather"}}}));
    rows.push_back(json::array({{{"text", get_text(chat_id, "btn_mycity")}, {"callback_data", "mycity"}}}));
    rows.push_back(json::array({{{"text", notifications_btn}, {"callback_data", "notify"}}}));
    rows.push_back(json::array({{{"text", lang_btn}, {"callback_data", "lang"}}}));

    return json{{"inline_keyboard", rows}};
}

string screen_css() {
    return template_engine::read_file_or_default(SCREEN_CSS_FILE, R"CSS(
        * { box-sizing: border-box; }
        body {
            margin: 0;
            width: 1280px;
            min-height: 1500px;
            background: #0f1113;
            color: #f6f7f4;
            font-family: "DejaVu Sans", "Liberation Sans", Arial, Helvetica, sans-serif;
            letter-spacing: 0;
        }
        .app {
            width: 1280px;
            min-height: 1500px;
            position: relative;
            overflow: hidden;
            background:
                radial-gradient(circle at 82% 8%, rgba(201,25,36,0.22), transparent 28%),
                radial-gradient(circle at 74% 76%, rgba(23,115,72,0.22), transparent 32%),
                linear-gradient(145deg, #111416 0%, #1b1f21 54%, #111416 100%);
        }
        .flag-band { height: 42px; background: linear-gradient(90deg, #c91924 0 62%, #f6f7f4 62% 70%, #177348 70% 100%); }
        .ornament {
            position: absolute;
            left: 0;
            top: 42px;
            bottom: 0;
            width: 96px;
            background:
                repeating-linear-gradient(45deg, #c91924 0 16px, #f6f7f4 16px 32px, #177348 32px 48px, #f6f7f4 48px 64px),
                #f6f7f4;
            border-right: 8px solid #177348;
            box-shadow: 12px 0 36px rgba(0,0,0,0.28);
        }
        .content { padding: 46px 50px 46px 136px; }
        .topline {
            display: block;
            text-align: right;
            color: #cfd6d2;
            font-size: 34px;
            font-weight: 700;
        }
        .pill {
            display: inline-block;
            padding: 16px 24px;
            border-radius: 8px;
            background: rgba(246,247,244,0.08);
            border: 2px solid rgba(246,247,244,0.18);
            color: #f6f7f4;
            box-shadow: 0 8px 18px rgba(0,0,0,0.24);
        }
        h1 {
            margin: 30px 0 18px;
            font-size: 86px;
            line-height: 1.02;
            letter-spacing: 0;
            color: #f6f7f4;
        }
        .subtitle {
            font-size: 44px;
            line-height: 1.24;
            color: #d8dfda;
            max-width: 1060px;
            margin-bottom: 30px;
        }
        .hero {
            display: block;
            font-size: 0;
            margin: 30px 0;
        }
        .kp-card, .panel, .weather, .forecast-card {
            background: rgba(32,36,38,0.92);
            border: 2px solid rgba(246,247,244,0.14);
            border-radius: 8px;
            box-shadow: 0 18px 44px rgba(0,0,0,0.32);
        }
        .kp-card {
            display: inline-block;
            width: 320px;
            padding: 30px;
            color: #f6f7f4;
            min-height: 286px;
            vertical-align: top;
            border-color: rgba(246,247,244,0.18);
            box-shadow: 0 0 34px rgba(255,255,255,0.20), 0 18px 44px rgba(0,0,0,0.32);
        }
        .kp-label { font-size: 34px; opacity: 0.92; font-weight: 900; }
        .kp-value { font-size: 130px; line-height: 0.92; font-weight: 900; margin-top: 22px; }
        .kp-state { font-size: 36px; line-height: 1.08; font-weight: 900; margin-top: 18px; }
        .panel {
            display: inline-block;
            width: 724px;
            min-height: 286px;
            margin-left: 20px;
            vertical-align: top;
            padding: 32px 34px;
            font-size: 40px;
            line-height: 1.33;
            color: #f0f3ef;
            border-left: 10px solid #177348;
        }
        .body {
            background: #202426;
            border-left: 10px solid #c91924;
            padding: 32px 36px;
            font-size: 42px;
            line-height: 1.38;
            border-radius: 8px;
            box-shadow: 0 12px 30px rgba(0,0,0,0.28);
            margin: 26px 0;
            color: #f0f3ef;
        }
        .screen-current .body {
            min-height: 420px;
            margin-top: 42px;
            padding: 40px 44px;
            border-left-color: #1fa463;
            background: linear-gradient(180deg, rgba(32,36,38,0.96), rgba(25,31,30,0.96));
            font-size: 41px;
            line-height: 1.34;
        }
        .screen-current .body b {
            color: #ffffff;
        }
        .footer {
            margin-top: 34px;
            padding-top: 24px;
            border-top: 2px solid rgba(246,247,244,0.16);
            color: #b7c0bb;
            font-size: 32px;
            font-weight: 800;
        }
        .weather {
            padding: 28px;
            margin: 24px 0;
            border-top: 8px solid #c91924;
            border-bottom: 8px solid #177348;
        }
        .weather-now { display: block; font-size: 0; }
        .weather-main {
            display: inline-block;
            width: 70%;
            vertical-align: top;
        }
        .weather-tempbox {
            display: inline-block;
            width: 30%;
            vertical-align: top;
            text-align: right;
        }
        .weather-city { font-size: 68px; font-weight: 900; margin-bottom: 10px; }
        .weather-desc { font-size: 42px; color: #d7ded9; font-weight: 800; }
        .weather-temp { font-size: 138px; line-height: 0.9; font-weight: 900; text-align: right; }
        .weather-icon { font-size: 112px; text-align: right; margin-bottom: 8px; }
        .metrics {
            display: block;
            margin-top: 22px;
            font-size: 0;
        }
        .metric {
            display: inline-block;
            width: 32%;
            margin-right: 2%;
            padding: 20px;
            vertical-align: top;
            background: rgba(15,17,19,0.72);
            border-radius: 8px;
            border: 1px solid rgba(246,247,244,0.10);
        }
        .metric:nth-child(3n) { margin-right: 0; }
        .metric small { display: block; color: #a9b2ad; font-size: 29px; font-weight: 900; margin-bottom: 10px; }
        .metric b { display: block; font-size: 44px; }
        .weather-strip-title {
            margin-top: 26px;
            margin-bottom: 14px;
            font-size: 42px;
            font-weight: 900;
            color: #f6f7f4;
        }
        .weather-strip { display: block; font-size: 0; }
        .weather-slot {
            display: inline-block;
            width: 23.8%;
            min-height: 226px;
            margin-right: 1.6%;
            margin-bottom: 16px;
            padding: 18px 16px;
            vertical-align: top;
            border-radius: 8px;
            background: linear-gradient(180deg, rgba(246,247,244,0.10), rgba(246,247,244,0.04));
            border: 1px solid rgba(246,247,244,0.13);
            box-shadow: 0 12px 28px rgba(0,0,0,0.24);
        }
        .weather-slot:nth-child(4n) { margin-right: 0; }
        .weather-slot-time { font-size: 31px; font-weight: 900; color: #d8dfda; }
        .weather-slot-icon { font-size: 58px; line-height: 1; margin: 10px 0 8px; }
        .weather-slot-temp { font-size: 56px; font-weight: 900; line-height: 1; }
        .weather-slot-desc { min-height: 46px; margin-top: 8px; font-size: 25px; line-height: 1.1; color: #d6ddd8; font-weight: 800; }
        .weather-slot-meta { margin-top: 10px; font-size: 24px; line-height: 1.2; color: #f6f7f4; font-weight: 900; }
        .forecast-grid { display: block; margin: 24px 0; }
        .forecast-card {
            padding: 32px 34px;
            border-top: 8px solid #c91924;
            border-bottom: 8px solid #177348;
        }
        .forecast-head {
            display: flex;
            align-items: center;
            justify-content: space-between;
            gap: 18px;
            margin-bottom: 18px;
        }
        .forecast-date { font-size: 54px; font-weight: 900; }
        .forecast-stats {
            display: block;
            margin: 22px 0 34px;
            font-size: 0;
        }
        .forecast-stat {
            display: inline-block;
            width: 48.8%;
            padding: 28px 30px;
            min-height: 164px;
            box-sizing: border-box;
            vertical-align: top;
            border-radius: 8px;
            color: #f6f7f4;
            box-shadow: 0 14px 32px rgba(0,0,0,0.30);
        }
        .forecast-stat + .forecast-stat { margin-left: 2.4%; }
        .forecast-stat small {
            display: block;
            font-size: 32px;
            font-weight: 900;
            opacity: 0.92;
            margin-bottom: 12px;
        }
        .forecast-stat b {
            display: block;
            font-size: 74px;
            line-height: 0.95;
            font-weight: 900;
        }
        .morning-storm-summary .forecast-stats { margin-bottom: 0; }
        .hour-grid {
            display: block;
            font-size: 0;
            padding-top: 12px;
            border-top: 2px solid rgba(246,247,244,0.13);
        }
        .hour-cell {
            display: inline-block;
            width: 23.6%;
            margin-right: 1.8%;
            margin-bottom: 14px;
            padding: 24px 20px;
            min-height: 158px;
            box-sizing: border-box;
            vertical-align: top;
            border-radius: 8px;
            color: #f6f7f4;
            box-shadow: 0 10px 24px rgba(0,0,0,0.24);
            border: 1px solid rgba(246,247,244,0.12);
        }
        .hour-cell:nth-child(4n) { margin-right: 0; }
        .hour-cell small {
            display: block;
            font-size: 34px;
            font-weight: 900;
            opacity: 0.9;
            margin-bottom: 18px;
        }
        .hour-cell b {
            display: block;
            font-size: 78px;
            line-height: 0.95;
            font-weight: 900;
        }
        .screen-morning,
        .screen-morning .app { width: 1800px; }
        .screen-morning .ornament { display: none; }
        .screen-morning .content { padding: 32px 24px 34px; }
        .screen-morning .subtitle { max-width: none; }
        .screen-morning h1 {
            margin: 24px 0 10px;
            font-size: 92px;
        }
        .screen-morning .subtitle {
            font-size: 46px;
            margin-bottom: 18px;
        }
        .screen-morning .hero { margin: 18px 0 22px; }
        .screen-morning .kp-card {
            width: 430px;
            min-height: 252px;
            padding: 24px;
        }
        .screen-morning .kp-label { font-size: 34px; }
        .screen-morning .kp-value {
            font-size: 128px;
            margin-top: 14px;
        }
        .screen-morning .kp-state {
            font-size: 34px;
            margin-top: 12px;
        }
        .screen-morning .panel {
            width: 1304px;
            min-height: 252px;
            margin-left: 18px;
            padding: 26px 30px;
            font-size: 52px;
            line-height: 1.22;
        }
        .screen-morning .weather,
        .screen-morning .forecast-grid,
        .screen-morning .forecast-card { width: 100%; }
        .screen-morning .weather {
            margin: 20px 0;
            padding: 24px;
        }
        .screen-morning .weather-city { font-size: 76px; }
        .screen-morning .weather-desc { font-size: 46px; }
        .screen-morning .weather-icon { font-size: 104px; }
        .screen-morning .weather-temp { font-size: 154px; }
        .screen-morning .metric {
            padding: 18px;
        }
        .screen-morning .metric small { font-size: 30px; }
        .screen-morning .metric b { font-size: 46px; }
        .screen-morning .weather-strip-title {
            margin-top: 22px;
            font-size: 44px;
        }
        .screen-morning .weather-slot {
            width: 23.95%;
            min-height: 226px;
            margin-right: 1.4%;
            padding: 16px 14px;
        }
        .screen-morning .weather-slot-time { font-size: 32px; }
        .screen-morning .weather-slot-icon { font-size: 52px; }
        .screen-morning .weather-slot-temp { font-size: 58px; }
        .screen-morning .weather-slot-desc { font-size: 27px; }
        .screen-morning .weather-slot-meta { font-size: 26px; }
        .screen-morning .forecast-card {
            padding: 28px 26px;
        }
        .screen-morning .forecast-date { font-size: 58px; }
        .screen-morning .forecast-stat small { font-size: 34px; }
        .screen-morning .forecast-stat b { font-size: 82px; }
        .screen-morning .footer {
            margin-top: 22px;
            padding-top: 18px;
            font-size: 32px;
        }
        .alert-mode .app {
            background:
                radial-gradient(circle at 82% 8%, rgba(201,25,36,0.42), transparent 30%),
                radial-gradient(circle at 74% 76%, rgba(201,25,36,0.18), transparent 34%),
                linear-gradient(180deg, #211719 0%, #151719 100%);
        }
        .alert-mode .flag-band { background: linear-gradient(90deg, #c91924 0 82%, #f6f7f4 82% 90%, #9e111b 90% 100%); }
        .alert-mode .ornament { border-right-color: #c91924; box-shadow: 12px 0 44px rgba(201,25,36,0.32); }
        .alert-mode h1 { color: #ffdde0; }
        .alert-mode .pill { border-color: rgba(255,221,224,0.34); background: rgba(201,25,36,0.16); }
        .alert-mode .panel { border-left-color: #c91924; background: #251d1f; }
        .alert-mode .kp-card { border-color: rgba(255,221,224,0.32); }
        .alert-mode .body { border-left-color: #c91924; background: #251d1f; }
    )CSS");
}

string screen_template() {
    return template_engine::read_file_or_default(SCREEN_TEMPLATE_FILE, R"HTML(<!doctype html>
<html>
<head>
    <meta charset="utf-8">
    <style>{{CSS}}</style>
</head>
<body class="{{BODY_CLASS}}">
    <div class="app">
        <div class="flag-band"></div>
        <div class="ornament"></div>
        <main class="content">{{CONTENT}}</main>
    </div>
</body>
</html>
)HTML");
}

string screen_body_class(const ScreenView& view) {
    string classes = "screen-" + view.kind;
    if (view.alert) {
        classes += " alert-mode";
    }
    return classes;
}

string render_screen_html(long long chat_id, const ScreenView& view) {
    stringstream html;
    string title = view.title.empty()
        ? localize(chat_id, "Магнитные бури Беларуси", "Магнітныя буры Беларусі", "Belarus Magnetic Weather")
        : view.title;
    html << "<div class='topline'><div class='pill'>" << html_escape(current_minsk_datetime(chat_id)) << "</div></div>";
    html << "<h1>" << html_escape(title) << "</h1>";
    if (!view.subtitle.empty()) {
        html << "<div class='subtitle'>" << markdown_to_html(view.subtitle) << "</div>";
    }

    if (view.kp >= 0.0) {
        string color = view.alert ? "#9e111b" : kp_color(view.kp);
        html << "<section class='hero'>";
        html << "<div class='kp-card' style='background:" << color << ";box-shadow:0 0 42px " << color << ", 0 18px 44px rgba(0,0,0,0.34)'>";
        html << "<div class='kp-label'>" << html_escape(localize(chat_id, "Индекс Kp", "Індэкс Kp", "Kp index")) << "</div>";
        html << "<div class='kp-value'>" << format_double_1(view.kp) << "</div>";
        html << "<div class='kp-state'>" << html_escape(kp_short_label(view.kp, chat_id)) << "</div>";
        html << "</div>";
        html << "<div class='panel'>" << markdown_to_html(get_kp_status(view.kp, chat_id)) << "</div>";
        html << "</section>";
    }

    if (view.show_weather && view.weather.ok) {
        html << "<section class='weather'>";
        html << "<div class='weather-now'>";
        html << "<div class='weather-main'><div class='weather-city'>" << html_escape(view.weather.name) << "</div>";
        html << "<div class='weather-desc'>" << html_escape(view.weather.description) << "</div></div>";
        html << "<div class='weather-tempbox'><div class='weather-icon'>" << html_escape(view.weather.icon) << "</div>";
        html << "<div class='weather-temp'>" << view.weather.temp << "°</div></div>";
        html << "</div>";
        html << "<div class='metrics'>";
        html << "<div class='metric'><small>" << html_escape(localize(chat_id, "Ощущается", "Адчуваецца", "Feels")) << "</small><b>" << view.weather.feels_like << "°C</b></div>";
        html << "<div class='metric'><small>" << html_escape(localize(chat_id, "Влажн.", "Вільг.", "Humidity")) << "</small><b>" << view.weather.humidity << "%</b></div>";
        html << "<div class='metric'><small>" << html_escape(localize(chat_id, "Ветер", "Вецер", "Wind")) << "</small><b>" << (int)view.weather.wind_speed << " " << html_escape(wind_unit(chat_id)) << "</b></div>";
        html << "</div>";
        if (!view.weather_slots.empty()) {
            html << "<div class='weather-strip-title'>" << html_escape(localize(chat_id, "Почасовой прогноз", "Прагноз па гадзінах", "Hourly forecast")) << "</div>";
            html << "<div class='weather-strip'>";
            for (const auto& slot : view.weather_slots) {
                html << "<div class='weather-slot'>";
                html << "<div class='weather-slot-time'>" << html_escape(slot.time) << "</div>";
                html << "<div class='weather-slot-icon'>" << html_escape(slot.icon) << "</div>";
                html << "<div class='weather-slot-temp'>" << slot.temp << "°</div>";
                html << "<div class='weather-slot-desc'>" << html_escape(slot.description) << "</div>";
                html << "<div class='weather-slot-meta'>"
                     << html_escape(localize(chat_id, "Осадки", "Ападкі", "Rain")) << ": "
                     << html_escape(format_precipitation(slot, chat_id)) << "<br>"
                     << html_escape(localize(chat_id, "Ветер", "Вецер", "Wind")) << ": "
                     << (int)round(slot.wind_speed) << " " << html_escape(wind_unit(chat_id)) << "</div>";
                html << "</div>";
            }
            html << "</div>";
        }
        html << "</section>";
    }

    if (!view.daily_storm_summary.empty()) {
        html << "<section class='forecast-grid morning-storm-summary'>";
        for (const auto& fc : view.daily_storm_summary) {
            double min_kp = fc.values.empty() ? fc.max_kp : *min_element(fc.values.begin(), fc.values.end());
            html << "<div class='forecast-card'>";
            html << "<div class='forecast-head'><div class='forecast-date'>"
                 << html_escape(localize(chat_id,
                        "Магнитные бури сегодня",
                        "Магнітныя буры сёння",
                        "Geomagnetic storms today"))
                 << "</div></div>";
            html << "<div class='forecast-stats'>";
            html << "<div class='forecast-stat' style='background:" << kp_color(min_kp) << "'>";
            html << "<small>" << html_escape(localize(chat_id, "Минимум за день", "Мінімум за дзень", "Daily minimum")) << "</small>";
            html << "<b>" << format_double_1(min_kp) << "</b></div>";
            html << "<div class='forecast-stat' style='background:" << kp_color(fc.max_kp) << "'>";
            html << "<small>" << html_escape(localize(chat_id, "Максимум за день", "Максімум за дзень", "Daily maximum")) << "</small>";
            html << "<b>" << format_double_1(fc.max_kp) << "</b></div>";
            html << "</div></div>";
        }
        html << "</section>";
    }

    if (!view.forecast.empty()) {
        html << "<section class='forecast-grid'>";
        for (const auto& fc : view.forecast) {
            double min_kp = fc.values.empty() ? fc.max_kp : *min_element(fc.values.begin(), fc.values.end());
            html << "<div class='forecast-card'>";
            html << "<div class='forecast-head'><div class='forecast-date'>" << html_escape(fc.date) << "</div></div>";
            html << "<div class='forecast-stats'>";
            html << "<div class='forecast-stat' style='background:" << kp_color(min_kp) << "'>";
            html << "<small>" << html_escape(localize(chat_id, "Минимум за день", "Мінімум за дзень", "Daily minimum")) << "</small>";
            html << "<b>" << format_double_1(min_kp) << "</b></div>";
            html << "<div class='forecast-stat' style='background:" << kp_color(fc.max_kp) << "'>";
            html << "<small>" << html_escape(localize(chat_id, "Максимум за день", "Максімум за дзень", "Daily maximum")) << "</small>";
            html << "<b>" << format_double_1(fc.max_kp) << "</b></div>";
            html << "</div>";
            html << "<div class='hour-grid'>";
            for (size_t i = 0; i < fc.values.size() && i < 8; i++) {
                double value = fc.values[i];
                html << "<div class='hour-cell' style='background:" << kp_color(value) << "'>";
                html << "<small>" << setw(2) << setfill('0') << (int)(i * 3) << ":00</small>";
                html << "<b>" << format_double_1(value) << "</b></div>";
            }
            html << "</div></div>";
        }
        html << "</section>";
    }

    if (!view.body.empty()) {
        html << "<section class='body'>" << markdown_to_html(view.body) << "</section>";
    }
    if (!view.footer.empty()) {
        html << "<footer class='footer'>" << markdown_to_html(view.footer) << "</footer>";
    }

    return template_engine::render(screen_template(), {
        {"BODY_CLASS", screen_body_class(view)},
        {"CONTENT", html.str()},
        {"CSS", screen_css()}
    });
}

string render_screen_image(long long chat_id, const ScreenView& view) {
    if (!screen_renderer_available) {
        return "";
    }

    filesystem::create_directories(SCREEN_DIR);
    auto nonce = chrono::duration_cast<chrono::nanoseconds>(chrono::steady_clock::now().time_since_epoch()).count();
    string base = SCREEN_DIR + "/screen_" + to_string(chat_id) + "_" + to_string(nonce);
    string html_path = base + ".html";
    string image_path = base + ".jpg";

    ofstream out(html_path);
    out << render_screen_html(chat_id, view);
    out.close();

    int render_width = view.kind == "morning" ? 1800 : 1280;
    string cmd = "/usr/bin/wkhtmltoimage --quiet --width " + to_string(render_width) +
                 " --quality 92 " + shell_quote(html_path) + " " + shell_quote(image_path);
    int rc = system(cmd.c_str());
    filesystem::remove(html_path);
    if (rc != 0 || !filesystem::exists(image_path)) {
        cerr << "Не удалось сгенерировать JPEG экран через wkhtmltoimage" << endl;
        return "";
    }
    return image_path;
}

bool validate_screen_renderer() {
    if (filesystem::exists("/usr/bin/wkhtmltoimage")) {
        screen_renderer_available = true;
        return true;
    }

    screen_renderer_available = false;
    cerr << "⚠️ /usr/bin/wkhtmltoimage не найден: бот будет отправлять текстовый fallback вместо JPEG-экранов" << endl;
    return false;
}

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

bool edit_supplement_message(long long chat_id, int message_id, const string& text) {
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
        if (edit_supplement_message(chat_id, message_id, text)) {
            return;
        }
        save_supplement_message_id(chat_id, 0);
    }

    send_supplement_message(chat_id, text);
}

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

bool fallback_text_message(long long chat_id, const string& text) {
    json kb = make_inline_keyboard(chat_id);
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

bool send_plain_fallback_text(long long chat_id, const string& text) {
    json kb = make_inline_keyboard(chat_id);
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

void upsert_live_screen(long long chat_id, const ScreenView& view, bool force_new_message = false) {
    string image_path = render_screen_image(chat_id, view);
    string fallback_text = fallback_text_for_screen(chat_id, view);
    string caption = photo_caption_for_screen(view);
    if (image_path.empty()) {
        cerr << "render_screen_image вернул пустой путь, отправляю fallback text chat_id="
             << chat_id << endl;
        if (!fallback_text_message(chat_id, fallback_text)) {
            send_plain_fallback_text(chat_id, fallback_text);
        }
        delete_supplement_message(chat_id);
        return;
    }

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
             << telegram_error_summary(edit) << ". Будет создан новый live screen" << endl;
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
        bool fallback_sent = fallback_text_message(chat_id, fallback_text);
        if (!fallback_sent) {
            fallback_sent = send_plain_fallback_text(chat_id, fallback_text);
        }
        if (fallback_sent && known_message_id > 0) {
            delete_telegram_message(chat_id, known_message_id);
        }
        if (fallback_sent) {
            delete_supplement_message(chat_id);
        }
    }

    filesystem::remove(image_path);
}

void show_home_screen(long long chat_id, const string& user_name = "", bool force_new_message = false) {
    ScreenView view;
    view.kind = "home";
    string clean_name = trim_copy(user_name).empty()
        ? localize(chat_id, "пользователь", "карыстальнік", "user")
        : trim_copy(user_name);
    view.title = localize(chat_id,
        "Здравствуйте, " + clean_name,
        "Вітаю, " + clean_name,
        "Hello, " + clean_name);
    upsert_live_screen(chat_id, view, force_new_message);
}

void show_current_screen(long long chat_id) {
    ScreenView view;
    view.kind = "current";
    view.title = localize(chat_id, "Текущий индекс", "Бягучы індэкс", "Current index");
    view.kp = fetch_current_kp();
    if (!kp_available(view.kp)) {
        view.supplement = kp_unavailable_text(chat_id);
    } else {
        view.supplement = localize(chat_id,
            "**Текущий индекс магнитных бурь на сейчас.**",
            "**Бягучы індэкс магнітных бур цяпер.**",
            "**Current geomagnetic storm index right now.**");
        view.body = get_current_kp_guidance(view.kp, chat_id);
    }
    upsert_live_screen(chat_id, view);
}

void show_forecast_screen(long long chat_id, int page = 0) {
    ScreenView view;
    view.kind = "forecast";
    view.title = localize(chat_id, "Прогноз на 3 дня", "Прагноз на 3 дні", "3-day forecast");
    vector<KpForecast> forecast = fetch_kp_forecast_3day(chat_id);
    if (forecast.empty()) {
        view.supplement = localize(chat_id,
            "Не удалось получить прогноз NOAA. Попробуйте обновить экран позже.",
            "Не атрымалася атрымаць прагноз NOAA. Паспрабуйце абнавіць экран пазней.",
            "Could not load the NOAA forecast. Try refreshing later.");
    } else {
        page = max(0, min(page, (int)forecast.size() - 1));
        view.forecast.push_back(forecast[page]);
        view.forecast_page = page;
        view.forecast_total = (int)forecast.size();
        view.supplement = forecast_day_supplement(chat_id, forecast[page]);
    }
    upsert_live_screen(chat_id, view);
}

void show_weather_prompt_screen(long long chat_id) {
    ScreenView view;
    view.kind = "weather_prompt";
    view.title = localize(chat_id, "Погода сейчас", "Надвор'е цяпер", "Weather now");
    view.subtitle = get_text(chat_id, "enter_city_weather");
    view.supplement = localize(chat_id,
        "Напишите населённый пункт...",
        "Напішыце населены пункт...",
        "Send a location...");
    upsert_live_screen(chat_id, view);
}

void show_city_prompt_screen(long long chat_id) {
    ScreenView view;
    view.kind = "city_prompt";
    view.title = localize(chat_id, "Изменить город", "Змяніць горад", "Change city");
    view.subtitle = get_text(chat_id, "enter_city");
    view.supplement = localize(chat_id,
        "Этот город будет использоваться для утреннего отчёта.",
        "Гэты горад будзе выкарыстоўвацца для ранішняй справаздачы.",
        "This city will be used for the morning report.");
    upsert_live_screen(chat_id, view);
}

void show_weather_result_screen(long long chat_id, const string& location, bool save_city) {
    string normalized = normalize_location(location);

    ScreenView view;
    view.kind = save_city ? "city_saved" : "weather";
    view.title = save_city
        ? localize(chat_id, "Город сохранён", "Горад захаваны", "City saved")
        : localize(chat_id, "Погода сейчас", "Надвор'е цяпер", "Weather now");

    if (!weather_configured()) {
        view.subtitle = get_text(chat_id, "weather_api_missing");
        view.supplement = localize(chat_id,
            "Администратору нужно задать переменную окружения OPENWEATHER_API_KEY и перезапустить бота.",
            "Адміністратару трэба задаць зменную асяроддзя OPENWEATHER_API_KEY і перазапусціць бота.",
            "Set the OPENWEATHER_API_KEY environment variable and restart the bot.");
        upsert_live_screen(chat_id, view);
        return;
    }

    WeatherInfo weather = fetch_weather_info(normalized, chat_id);

    if (weather.ok) {
        if (save_city) {
            save_user_city(chat_id, normalized);
        }
        view.weather = weather;
        view.show_weather = true;
        view.weather_slots = fetch_weather_forecast_slots(normalized, chat_id, 8);
        view.supplement = weather_supplement_text(chat_id, weather, view.weather_slots, save_city);
    } else {
        view.subtitle = get_text(chat_id, "city_not_found");
        view.supplement = localize(chat_id,
            "Проверьте название и попробуйте ещё раз.",
            "Праверце назву і паспрабуйце яшчэ раз.",
            "Check the name and try again.");
    }

    upsert_live_screen(chat_id, view);
}

void show_notifications_screen(long long chat_id, bool enabled) {
    ScreenView view;
    view.kind = "notifications";
    view.title = enabled
        ? localize(chat_id, "Уведомления включены", "Апавяшчэнні ўключаны", "Alerts enabled")
        : localize(chat_id, "Уведомления выключены", "Апавяшчэнні выключаны", "Alerts disabled");
    view.subtitle = enabled ? get_text(chat_id, "notifications_on") : get_text(chat_id, "notifications_off");
    view.supplement = enabled
        ? localize(chat_id,
            "**Уведомления включены.** Если Kp поднимется до 5.0 и выше, бот обновит live-экран алертом магнитной бури и даст рекомендации.",
            "**Апавяшчэнні ўключаны.** Калі Kp падымецца да 5.0 і вышэй, бот абновіць live-экран алертам магнітнай буры і дасць рэкамендацыі.",
            "**Alerts enabled.** If Kp rises to 5.0 or higher, the bot will update the live screen with a storm alert and recommendations.")
        : localize(chat_id,
            "**Уведомления выключены.** Бот не будет присылать алерты о магнитных бурях, пока вы снова их не включите.",
            "**Апавяшчэнні выключаны.** Бот не будзе дасылаць алерты пра магнітныя буры, пакуль вы зноў іх не ўключыце.",
            "**Alerts disabled.** The bot will not send magnetic storm alerts until you enable them again.");
    view.kp = fetch_current_kp();
    if (!kp_available(view.kp)) {
        view.supplement += "\n\n" + kp_unavailable_text(chat_id);
    }
    upsert_live_screen(chat_id, view);
}

void show_language_screen(long long chat_id) {
    ScreenView view;
    view.kind = "language";
    view.title = localize(chat_id, "Язык изменён", "Мова зменена", "Language changed");
    view.subtitle = get_text(chat_id, "language_changed");
    view.supplement = localize(chat_id,
        "Все кнопки, подписи и live-экраны теперь будут в выбранном языке.",
        "Усе кнопкі, подпісы і live-экраны цяпер будуць на выбранай мове.",
        "Buttons, labels and live screens now use the selected language.");
    upsert_live_screen(chat_id, view);
}

void show_alert_screen(long long chat_id, double current_kp, bool force_new_message = false) {
    ScreenView view;
    view.kind = "alert";
    view.alert = current_kp >= 5.0;
    string storm_level = storm_level_label(current_kp);
    view.title = current_kp >= 5.0
        ? localize(chat_id,
            "Внимание. Магнитная буря " + storm_level,
            "Увага. Магнітная бура " + storm_level,
            "Warning. Geomagnetic storm " + storm_level)
        : localize(chat_id, "Буря затихла", "Бура сціхла", "Storm has calmed");
    view.subtitle = localize(chat_id,
        "Сейчас Kp " + format_double_1(current_kp) + " - " + kp_short_label(current_kp, chat_id),
        "Цяпер Kp " + format_double_1(current_kp) + " - " + kp_short_label(current_kp, chat_id),
        "Now Kp " + format_double_1(current_kp) + " - " + kp_short_label(current_kp, chat_id));
    view.kp = current_kp;
    view.supplement = view.title + "\n\n";
    view.supplement += localize(chat_id,
        "Сейчас Kp " + format_double_1(current_kp) + ".",
        "Цяпер Kp " + format_double_1(current_kp) + ".",
        "Current Kp is " + format_double_1(current_kp) + ".");
    view.supplement += "\n\n" + get_current_kp_guidance(current_kp, chat_id);
    upsert_live_screen(chat_id, view, force_new_message);
}

void send_morning_report(long long chat_id, int page = 0) {
    tm ltm = get_minsk_time();

    string user_city_name = user_city_or_default(chat_id);

    double current_kp = fetch_current_kp();

    ScreenView view;
    view.kind = "morning";
    view.page_callback = "morning";

    vector<KpForecast> forecast = fetch_kp_forecast_3day(chat_id);
    int total_pages = forecast.empty() ? 1 : (int)forecast.size() + 1;
    page = max(0, min(page, total_pages - 1));
    view.forecast_page = page;
    view.forecast_total = total_pages;

    string date_text = to_string(ltm.tm_mday) + " " + get_month_name(ltm.tm_mon + 1, chat_id) + " " +
        to_string(ltm.tm_year + 1900) + ", " + get_weekday_name(0, chat_id);

    if (page == 0) {
        view.title = localize(chat_id, "Доброе утро", "Добрай раніцы", "Good morning");
        view.subtitle = date_text;
        view.kp = current_kp;
        view.weather = fetch_weather_info(user_city_name, chat_id);
        view.show_weather = view.weather.ok;
        if (view.weather.ok) {
            view.weather_slots = fetch_weather_forecast_slots(user_city_name, chat_id, 8);
        }
        if (!forecast.empty()) {
            view.daily_storm_summary.push_back(forecast.front());
        }
        view.supplement = localize(chat_id,
            "**Утренняя сводка на " + date_text + ".**",
            "**Ранішняя зводка на " + date_text + ".**",
            "**Morning summary for " + date_text + ".**");
    } else {
        int forecast_index = page - 1;
        view.title = localize(chat_id, "Утренний прогноз", "Ранішні прагноз", "Morning forecast");
        view.subtitle = localize(chat_id,
            "День " + to_string(page) + " из " + to_string(max(1, total_pages - 1)),
            "Дзень " + to_string(page) + " з " + to_string(max(1, total_pages - 1)),
            "Day " + to_string(page) + " of " + to_string(max(1, total_pages - 1)));
        if (forecast_index >= 0 && forecast_index < (int)forecast.size()) {
            view.forecast.push_back(forecast[forecast_index]);
            view.supplement = forecast_day_supplement(chat_id, forecast[forecast_index]);
        } else {
            view.supplement = localize(chat_id,
                "Прогноз NOAA сейчас недоступен. Вернитесь к утренней сводке.",
                "Прагноз NOAA цяпер недаступны. Вярніцеся да ранішняй зводкі.",
                "NOAA forecast is currently unavailable. Return to the morning summary.");
        }
    }

    view.footer = localize(chat_id,
        "Утренний отчёт · " + user_city_name + " · страница " + to_string(page + 1) + " из " + to_string(total_pages),
        "Ранішняя справаздача · " + user_city_name + " · старонка " + to_string(page + 1) + " з " + to_string(total_pages),
        "Morning report · " + user_city_name + " · page " + to_string(page + 1) + " of " + to_string(total_pages));

    upsert_live_screen(chat_id, view);
}

void scheduler() {
    map<long long, bool> morning_sent;

    while (true) {
        tm ltm = get_minsk_time();

        if (ltm.tm_hour == 9 && ltm.tm_min == 0) {
            for (long long uid : active_user_snapshot()) {
                if (!morning_sent[uid]) {
                    send_morning_report(uid);
                    morning_sent[uid] = true;
                    this_thread::sleep_for(chrono::milliseconds(100));
                }
            }
        }
        if (ltm.tm_hour == 10) {
            morning_sent.clear();
        }

        if (ltm.tm_min % 20 == 0 && ltm.tm_min != 0) {
            double current_kp = fetch_current_kp();
            if (!kp_available(current_kp)) {
                this_thread::sleep_for(chrono::seconds(30));
                continue;
            }
            time_t now_ts = time(nullptr);

            bool should_update_alert = false;
            string alert_log;

            if (current_kp >= 5.0) {
                if (!storm_alert_active || last_alert_time == 0) {
                    should_update_alert = true;
                    alert_log = "старт бури";
                } else if (current_kp >= last_alert_kp + 0.9) {
                    should_update_alert = true;
                    alert_log = "усиление бури";
                } else if ((last_alert_kp - current_kp) >= 0.9 && (now_ts - last_alert_time) > 3600) {
                    should_update_alert = true;
                    alert_log = "ослабление бури";
                } else if ((now_ts - last_alert_time) > 10800) {
                    should_update_alert = true;
                    alert_log = "плановое обновление бури";
                }
            } else if (storm_alert_active || last_alert_time != 0) {
                should_update_alert = true;
                alert_log = "буря затихла";
            }

            if (should_update_alert) {
                for (long long uid : active_user_snapshot()) {
                    if (is_notifications_enabled(uid)) {
                        show_alert_screen(uid, current_kp);
                        this_thread::sleep_for(chrono::milliseconds(100));
                    }
                }

                last_alert_kp = current_kp;
                last_alert_time = now_ts;
                storm_alert_active = current_kp >= 5.0;

                cout << "⚠️ Обновлён storm alert (" << alert_log << "), Kp: " << current_kp << endl;
            }
        }

        this_thread::sleep_for(chrono::seconds(30));
    }
}

void answer_callback_query(const string& callback_id, const string& text = "") {
    cpr::Payload payload{{"callback_query_id", callback_id}};
    if (!text.empty()) {
        payload.Add({"text", text});
    }
    cpr::Post(cpr::Url{API_URL + "/answerCallbackQuery"}, payload, cpr::Timeout{TELEGRAM_SHORT_TIMEOUT_MS});
}

void handle_callback(long long chat_id, int message_id, const string& callback_id, const string& data) {
    save_user(chat_id);
    int current_live_message_id = known_live_message_id(chat_id);
    if (current_live_message_id <= 0) {
        save_live_message_id(chat_id, message_id);
    } else if (current_live_message_id != message_id) {
        cout << "↪️ stale callback ignored as live target chat_id=" << chat_id
             << " callback_message_id=" << message_id
             << " current_live_message_id=" << current_live_message_id << endl;
        delete_telegram_message(chat_id, message_id);
    }
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

#ifndef UNIT_TEST
int main() {
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
#endif

#ifdef UNIT_TEST
int test_failures = 0;

void expect_true(bool condition, const string& name) {
    if (!condition) {
        cerr << "FAIL: " << name << endl;
        test_failures++;
    }
}

void expect_equal(const string& actual, const string& expected, const string& name) {
    if (actual != expected) {
        cerr << "FAIL: " << name << " | expected=\"" << expected
             << "\" actual=\"" << actual << "\"" << endl;
        test_failures++;
    }
}

void expect_contains(const string& actual, const string& needle, const string& name) {
    if (actual.find(needle) == string::npos) {
        cerr << "FAIL: " << name << " | expected substring=\"" << needle
             << "\" actual=\"" << actual << "\"" << endl;
        test_failures++;
    }
}

int main() {
    {
        lock_guard<mutex> lock(state_mutex);
        user_language[1] = "ru";
        user_language[2] = "be";
        user_language[3] = "en";
    }

    expect_equal(normalize_location(" gomel "), "Гомель", "normalize latin Gomel");
    expect_equal(normalize_location("Менск"), "Мінск", "normalize Belarusian Minsk variant");
    expect_equal(normalize_location("Kopyl"), "Kapyl", "normalize Kopyl for OpenWeather");
    expect_equal(normalize_location("Несвиж"), "Несвиж", "keep unknown location");

    expect_true(kp_available(0.0), "Kp zero is available");
    expect_true(!kp_available(-1.0), "negative Kp is unavailable");
    expect_equal(kp_short_label(3.9, 1), "Спокойное геомагнитное поле", "Russian quiet Kp label");
    expect_equal(kp_short_label(6.1, 3), "Moderate geomagnetic storm G2", "English G2 Kp label");
    expect_equal(wind_unit(3), "m/s", "English wind unit");
    expect_equal(wind_unit(1), "м/с", "Russian wind unit");
    expect_true(is_command("/start", "/start"), "plain command matching");
    expect_true(is_command("/start@geomagnetic_belarus_bot arg", "/start"), "bot-addressed command matching");
    expect_true(!is_command("/starter", "/start"), "command prefix is not enough");

    WeatherForecastSlot slot;
    expect_equal(format_precipitation(slot, 1), "без осадков", "dry precipitation label");
    slot.pop = 35;
    expect_equal(format_precipitation(slot, 3), "35%", "precipitation probability label");
    slot.rain_mm = 1.25;
    expect_equal(format_precipitation(slot, 1), "1.2 мм", "precipitation mm label");

    KpForecast morning_fc;
    morning_fc.max_kp = 4.2;
    morning_fc.values = {3.0, 2.7, 3.4, 4.2};
    string morning_kp = morning_kp_detail(1, 3.3, {morning_fc});
    expect_contains(morning_kp, "Сейчас Kp 3.3", "morning Kp starts with current index");
    expect_contains(morning_kp, "минимум ожидается Kp 2.7", "morning Kp includes expected minimum");
    expect_contains(morning_kp, "максимум - Kp 4.2", "morning Kp includes expected maximum");

    WeatherInfo morning_weather;
    morning_weather.ok = true;
    morning_weather.name = "Копыль";
    morning_weather.description = "пасмурно";
    morning_weather.temp = 12;
    morning_weather.feels_like = 11;
    morning_weather.wind_speed = 4.4;
    WeatherForecastSlot morning_slot_a;
    morning_slot_a.time = "09:00";
    morning_slot_a.description = "пасмурно";
    morning_slot_a.temp = 11;
    morning_slot_a.wind_speed = 5.0;
    WeatherForecastSlot morning_slot_b;
    morning_slot_b.time = "12:00";
    morning_slot_b.description = "небольшой дождь";
    morning_slot_b.temp = 14;
    morning_slot_b.pop = 60;
    morning_slot_b.wind_speed = 6.0;
    string morning_weather_text = morning_weather_detail(1, morning_weather, {morning_slot_a, morning_slot_b});
    expect_contains(morning_weather_text, "Погода сейчас в городе Копыль", "morning weather includes current weather");
    expect_contains(morning_weather_text, "11...14°C", "morning weather includes expected temperature range");
    expect_contains(morning_weather_text, "осадки: 60%", "morning weather includes expected precipitation");

    expect_equal(html_escape("<b>&\"'\n"), "&lt;b&gt;&amp;&quot;&#39;<br>", "HTML escaping");
    expect_equal(markdown_to_telegram_html("**Kp** < 5 & ok"), "<b>Kp</b> &lt; 5 &amp; ok", "Telegram HTML markdown conversion");

    if (validate_screen_renderer()) {
        ScreenView view;
        view.title = "Render test";
        view.subtitle = "JPEG screen generation";
        view.body = "Body";
        view.kp = 3.3;
        string image_path = render_screen_image(1, view);
        expect_true(!image_path.empty() && filesystem::exists(image_path), "JPEG render output exists");
        if (!image_path.empty()) {
            filesystem::remove(image_path);
        }
    }

    if (test_failures == 0) {
        cout << "All unit tests passed" << endl;
    }
    return test_failures == 0 ? 0 : 1;
}
#endif
