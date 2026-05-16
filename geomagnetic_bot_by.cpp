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
#include <regex>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

string API_URL;
string WEATHER_API_KEY = "60699f459685a5a585b1ee8839ce491b";
set<long long> active_users;
map<long long, string> user_city;
map<long long, bool> user_notifications;
map<long long, string> user_language;
const string USERS_FILE = "users.txt";
const string CITIES_FILE = "cities.txt";
const string NOTIFICATIONS_FILE = "notifications.txt";
const string LANGUAGE_FILE = "language.txt";
double last_alert_kp = 0.0;
time_t last_alert_time = 0;
map<long long, bool> waiting_for_city;

struct KpForecast {
    string date;
    double max_kp;
    string status;
    vector<double> values;
};

// Texts for different languages.
map<string, map<string, string>> TEXTS = {
    {"ru", {
        {"welcome", "🌤 **Здравствуйте!**\n\nЯ бот для отслеживания магнитных бурь и погоды в Беларуси.\n\n📅 **Что я умею:**\n• 📊 Текущий индекс - состояние прямо сейчас\n• 📈 Прогноз на 3 дня - прогноз магнитных бурь\n• ☁️ Погода сейчас - погода в любом городе Беларуси\n• 📍 Мой город - установить город для утренней рассылки\n\n⏰ **Утренний отчёт** приходит в 9:00\n⚠️ **Оповещение о бурях** при Kp ≥ 5.0\n\n📍 По умолчанию город для рассылки - Минск.\nИспользуйте кнопку \"📍 Мой город\" чтобы изменить его!"},
        {"morning_greeting", "🌅 **Доброе утро!**"},
        {"weather_in", "☁️ **Погода в {}:**"},
        {"magnetic_status", "🛰 **Магнитная обстановка:**"},
        {"kp_now", "📊 Kp {} — {}"},
        {"forecast_3days", "📊 **Прогноз на 3 дня:**"},
        {"wish", "✨ Желаю вам прекрасного настроения и отличного самочувствия!\n🌸 Берегите себя и будьте здоровы!"},
        {"alert_title", "⚠️ **ВНИМАНИЕ! Геомагнитная буря!**"},
        {"kp_current", "📈 Текущий индекс: **Kp {}**"},
        {"recommendations", "💊 **Рекомендации:**\n• Больше отдыхайте\n• Пейте больше воды\n• Избегайте стрессов\n\n🌸 Берегите своё здоровье!"},
        {"city_saved", "✅ Город **{}** сохранён!\n\n📍 Теперь утренняя рассылка будет показывать погоду для этого города."},
        {"city_not_found", "❌ Населенный пункт не найден.\n\nПожалуйста, введите корректное название города или деревни Беларуси."},
        {"enter_city", "🏙️ Напишите название вашего города/деревни для утренней рассылки\n(например: Минск, Гомель, Брест, Витебск, Гродно, Могилёв или любой другой населённый пункт Беларуси)"},
        {"enter_city_weather", "🇧🇾 Напишите название любого населённого пункта Беларуси\n(город, деревня, посёлок)"},
        {"notifications_on", "✅ Уведомления о магнитных бурях **включены**!\n\n⚠️ Вы будете получать оповещения при Kp ≥ 5.0"},
        {"notifications_off", "🔕 Уведомления о магнитных бурях **отключены**!\n\nВы можете снова включить их в любой момент."},
        {"current_index", "🛰 **Геомагнитная обстановка:**\n\n📊 **Индекс сейчас:** Kp {}\n{}"},
        {"forecast_title", "📈 **Прогноз на 3 дня**\n\n"},
        {"help_text", "ℹ️ **СПРАВКА**\n\nЯ отслеживаю геомагнитную обстановку по данным NOAA.\n\n🔹 **Утренний отчёт** в 9:00 с погодой и прогнозом\n🔹 **При буре** (Kp ≥ 5.0) — мгновенное предупреждение\n🔹 **Погода сейчас** — введите любой населённый пункт Беларуси\n🔹 **Мой город** — установите населённый пункт для утренней рассылки\n🔹 **Текущий индекс** — состояние прямо сейчас\n🔹 **Прогноз на 3 дня** — прогноз магнитных бурь\n\n📊 **Магнитный барометр:**\n🟢 0-3.9 - Спокойно\n🟡 4.0-4.9 - Возмущения\n🟠 5.0-5.9 - Буря G1\n🔴 6.0-6.9 - Буря G2\n🟣 7.0+ - Сильная буря G3+\n\nБерегите здоровье!"},
        {"language_changed", "✅ Язык изменён на русский!"},
        {"btn_current", "📊 Текущий индекс"},
        {"btn_forecast", "📈 Прогноз на 3 дня"},
        {"btn_weather", "☁️ Погода сейчас"},
        {"btn_mycity", "📍 Мой город"},
        {"btn_help", "📖 Справка"},
        {"btn_notify_on", "🔔 Включить уведомления"},
        {"btn_notify_off", "🔕 Отключить уведомления"},
        {"btn_lang", "🇬🇧 English"}
    }},
    {"be", {
        {"welcome", "🌤 **Вітаю!**\n\nЯ бот для адсочвання магнітных бур і надвор'я ў Беларусі.\n\n📅 **Што я ўмею:**\n• 📊 Бягучы індэкс - стан прама зараз\n• 📈 Прагноз на 3 дні - прагноз магнітных бур\n• ☁️ Надвор'е цяпер - надвор'е ў любым горадзе Беларусі\n• 📍 Мой горад - усталяваць горад для ранішняй рассылкі\n\n⏰ **Ранішняя справаздача** прыходзіць у 9:00\n⚠️ **Апавяшчэнне пра буры** пры Kp ≥ 5.0\n\n📍 Па змаўчанні горад для рассылкі - Мінск.\nВыкарыстоўвайце кнопку \"📍 Мой горад\" каб змяніць яго!"},
        {"morning_greeting", "🌅 **Добрай раніцы!**"},
        {"weather_in", "☁️ **Надвор'е ў {}:**"},
        {"magnetic_status", "🛰 **Магнітная абстаноўка:**"},
        {"kp_now", "📊 Kp {} — {}"},
        {"forecast_3days", "📊 **Прагноз на 3 дні:**"},
        {"wish", "✨ Жадаю вам выдатнага настрою і выдатнага самаадчування!\n🌸 Беражыце сябе і будзьце здаровыя!"},
        {"alert_title", "⚠️ **УВАГА! Геамагнітная бура!**"},
        {"kp_current", "📈 Бягучы індэкс: **Kp {}**"},
        {"recommendations", "💊 **Рэкамендацыі:**\n• Больш адпачывайце\n• Піце больш вады\n• Пазбягайце стрэсаў\n\n🌸 Беражыце сваё здароўе!"},
        {"city_saved", "✅ Горад **{}** захаваны!\n\n📍 Цяпер ранішняя рассылка будзе паказваць надвор'е для гэтага горада."},
        {"city_not_found", "❌ Населены пункт не знойдзены.\n\nКалі ласка, увядзіце карэктную назву горада ці вёскі Беларусі."},
        {"enter_city", "🏙️ Напішыце назву вашага горада/вёскі для ранішняй рассылкі\n(напрыклад: Мінск, Гомель, Брэст, Віцебск, Гродна, Магілёў ці любы іншы населены пункт Беларусі)"},
        {"enter_city_weather", "🇧🇾 Напішыце назву любога населенага пункта Беларусі\n(горад, вёска, пасёлак)"},
        {"notifications_on", "✅ Апавяшчэнні аб магнітных бурах **уключаны**!\n\n⚠️ Вы будзеце атрымліваць апавяшчэнні пры Kp ≥ 5.0"},
        {"notifications_off", "🔕 Апавяшчэнні аб магнітных бурах **адключаны**!\n\nВы можаце зноў уключыць іх у любы момант."},
        {"current_index", "🛰 **Геамагнітная абстаноўка:**\n\n📊 **Індэкс цяпер:** Kp {}\n{}"},
        {"forecast_title", "📈 **Прагноз на 3 дні**\n\n"},
        {"help_text", "ℹ️ **ДАВЕДКА**\n\nЯ адсочваю геамагнітную абстаноўку па дадзеных NOAA.\n\n🔹 **Ранішняя справаздача** у 9:00 з надвор'ем і прагнозам\n🔹 **Пры буры** (Kp ≥ 5.0) — імгненнае папярэджанне\n🔹 **Надвор'е цяпер** — увядзіце любы населены пункт Беларусі\n🔹 **Мой горад** — усталюйце населены пункт для ранішняй рассылкі\n🔹 **Бягучы індэкс** — стан прама зараз\n🔹 **Прагноз на 3 дні** — прагноз магнітных бур\n\n📊 **Магнітны барометр:**\n🟢 0-3.9 - Спакойна\n🟡 4.0-4.9 - Узрушэнні\n🟠 5.0-5.9 - Бура G1\n🔴 6.0-6.9 - Бура G2\n🟣 7.0+ - Моцная бура G3+\n\nБеражыце здароўе!"},
        {"language_changed", "✅ Мова зменена на беларускую!"},
        {"btn_current", "📊 Бягучы індэкс"},
        {"btn_forecast", "📈 Прагноз на 3 дні"},
        {"btn_weather", "☁️ Надвор'е цяпер"},
        {"btn_mycity", "📍 Мой горад"},
        {"btn_help", "📖 Даведка"},
        {"btn_notify_on", "🔔 Уключыць апавяшчэнні"},
        {"btn_notify_off", "🔕 Выключыць апавяшчэнні"},
        {"btn_lang", "🇬🇧 English"}
    }},
    {"en", {
        {"welcome", "🌤 **Hello!**\n\nI'm a bot for tracking magnetic storms and weather in Belarus.\n\n📅 **What I can do:**\n• 📊 Current index - current status\n• 📈 3-day forecast - magnetic storm forecast\n• ☁️ Weather now - weather in any Belarusian city\n• 📍 My city - set city for morning report\n\n⏰ **Morning report** at 9:00 AM\n⚠️ **Storm alerts** at Kp ≥ 5.0\n\n📍 Default city for reports is Minsk.\nUse the \"📍 My city\" button to change it!"},
        {"morning_greeting", "🌅 **Good morning!**"},
        {"weather_in", "☁️ **Weather in {}:**"},
        {"magnetic_status", "🛰 **Magnetic situation:**"},
        {"kp_now", "📊 Kp {} — {}"},
        {"forecast_3days", "📊 **3-day forecast:**"},
        {"wish", "✨ Wishing you a great mood and excellent health!\n🌸 Take care of yourself!"},
        {"alert_title", "⚠️ **ATTENTION! Geomagnetic storm!**"},
        {"kp_current", "📈 Current index: **Kp {}**"},
        {"recommendations", "💊 **Recommendations:**\n• Get more rest\n• Drink more water\n• Avoid stress\n\n🌸 Take care of your health!"},
        {"city_saved", "✅ City **{}** saved!\n\n📍 Now the morning report will show weather for this city."},
        {"city_not_found", "❌ Location not found.\n\nPlease enter a valid city or village name in Belarus."},
        {"enter_city", "🏙️ Enter your city/village name for the morning report\n(e.g., Minsk, Gomel, Brest, Vitebsk, Grodno, Mogilev or any other Belarusian location)"},
        {"enter_city_weather", "🇧🇾 Enter any Belarusian location name\n(city, village, town)"},
        {"notifications_on", "✅ Storm notifications **enabled**!\n\n⚠️ You will receive alerts at Kp ≥ 5.0"},
        {"notifications_off", "🔕 Storm notifications **disabled**!\n\nYou can re-enable them anytime."},
        {"current_index", "🛰 **Geomagnetic situation:**\n\n📊 **Current index:** Kp {}\n{}"},
        {"forecast_title", "📈 **3-day forecast**\n\n"},
        {"help_text", "ℹ️ **HELP**\n\nI track geomagnetic conditions using NOAA data.\n\n🔹 **Morning report** at 9:00 AM with weather and forecast\n🔹 **Storm alert** (Kp ≥ 5.0) — instant warning\n🔹 **Weather now** — enter any Belarusian location\n🔹 **My city** — set location for morning report\n🔹 **Current index** — current status\n🔹 **3-day forecast** — magnetic storm forecast\n\n📊 **Magnetic barometer:**\n🟢 0-3.9 - Quiet\n🟡 4.0-4.9 - Unsettled\n🟠 5.0-5.9 - Storm G1\n🔴 6.0-6.9 - Storm G2\n🟣 7.0+ - Strong storm G3+\n\nTake care of your health!"},
        {"language_changed", "✅ Language changed to English!"},
        {"btn_current", "📊 Current index"},
        {"btn_forecast", "📈 3-day forecast"},
        {"btn_weather", "☁️ Weather now"},
        {"btn_mycity", "📍 My city"},
        {"btn_help", "📖 Help"},
        {"btn_notify_on", "🔔 Enable notifications"},
        {"btn_notify_off", "🔕 Disable notifications"},
        {"btn_lang", "🇧🇾 Беларуская"}
    }}
};

string get_text(long long chat_id, const string& key, const string& arg1 = "", const string& arg2 = "") {
    string lang = user_language.count(chat_id) ? user_language[chat_id] : "ru";
    string text = TEXTS[lang][key];

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

// Normalize location name by correcting common misspellings and variations. Returns corrected city name or original input if not recognized.
string normalize_location(const string& location) {
    if (location.empty()) return location;

    // Map of common misspellings and variations to correct city names
    map<string, string> city_map = {
        {"гомель", "Гомель"}, {"гомел", "Гомель"}, {"homel", "Гомель"}, {"gomel", "Гомель"},
        {"минск", "Минск"}, {"minsk", "Минск"}, {"менск", "Мінск"},
        {"брест", "Брест"}, {"brest", "Брест"}, {"брэст", "Брэст"},
        {"витебск", "Витебск"}, {"vitebsk", "Витебск"}, {"віцебск", "Віцебск"},
        {"гродно", "Гродно"}, {"grodno", "Гродно"}, {"гародня", "Гродна"},
        {"могилёв", "Могилёв"}, {"могилев", "Могилёв"}, {"mogilev", "Могилёв"}, {"магілёў", "Магілёў"}
    };

    string lower = location;
    for (char& c : lower) c = tolower(c);

    if (city_map.count(lower)) {
        return city_map[lower];
    }


    string normalized = location;
    for (size_t i = 0; i < normalized.length(); i++) {
        if (i == 0) normalized[i] = toupper(normalized[i]);
        else normalized[i] = tolower(normalized[i]);
    }

    return normalized;
}

// Get weather for a given location. Returns formatted string with weather info or error message if city not found.
string get_weather_by_location(string location, long long chat_id) {
    location = normalize_location(location);

    string url = "http://api.openweathermap.org/data/2.5/weather?q=" + location + ",BY&units=metric&lang=ru&appid=" + WEATHER_API_KEY;
    auto r = cpr::Get(cpr::Url{url}, cpr::Timeout{8000});

    if (r.status_code != 200) {

        url = "http://api.openweathermap.org/data/2.5/weather?q=" + location + "&units=metric&lang=ru&appid=" + WEATHER_API_KEY;
        r = cpr::Get(cpr::Url{url}, cpr::Timeout{8000});

        if (r.status_code != 200) {
            return get_text(chat_id, "city_not_found");
        }
    }

    try {
        auto data = json::parse(r.text);
        int temp = (int)round(data["main"]["temp"].get<double>());
        int feels_like = (int)round(data["main"]["feels_like"].get<double>());
        string desc = data["weather"][0]["description"];
        string name = data["name"];
        int humidity = data["main"]["humidity"].get<int>();
        double wind_speed = data["wind"]["speed"].get<double>();

        string icon = "🌡️";
        if (desc.find("ясно") != string::npos || desc.find("солнечно") != string::npos) icon = "☀️";
        else if (desc.find("облачно") != string::npos) icon = "☁️";
        else if (desc.find("дождь") != string::npos) icon = "🌧️";
        else if (desc.find("снег") != string::npos) icon = "❄️";
        else if (desc.find("туман") != string::npos) icon = "🌫️";

        string advice;
        if (temp <= -15) advice = "🥶 Сильный мороз! Одевайтесь максимально тепло!";
        else if (temp <= 0) advice = "На улице морозно, не забудьте тёплую одежду.";
        else if (temp <= 10) advice = "Прохладно, возьмите с собой куртку.";
        else if (temp <= 20) advice = "Погода приятная, наслаждайтесь прогулкой 😊";
        else advice = "На улице жарко! Пейте больше воды и носите головной убор 🥵";

        if (desc.find("дождь") != string::npos) advice += " ☔️ Не забудьте зонт!";
        else if (desc.find("снег") != string::npos) advice += " ❄️ Осторожно, гололёд!";

        string lang = user_language.count(chat_id) ? user_language[chat_id] : "ru";
        string result;

        if (lang == "en") {
            result = "🏘️ **" + name + ", Belarus**\n";
            result += icon + " **Weather:** " + desc + "\n";
            result += "🌡️ **Temperature:** " + to_string(temp) + "°C\n";
            result += "🌡️ **Feels like:** " + to_string(feels_like) + "°C\n";
            result += "💧 **Humidity:** " + to_string(humidity) + "%\n";
            result += "💨 **Wind:** " + to_string((int)wind_speed) + " m/s\n\n";
            result += "💡 **Tip:** " + advice;
        } else if (lang == "be") {
            result = "🏘️ **" + name + ", Беларусь**\n";
            result += icon + " **Надвор'е:** " + desc + "\n";
            result += "🌡️ **Тэмпература:** " + to_string(temp) + "°C\n";
            result += "🌡️ **Адчуваецца як:** " + to_string(feels_like) + "°C\n";
            result += "💧 **Вільготнасць:** " + to_string(humidity) + "%\n";
            result += "💨 **Вецер:** " + to_string((int)wind_speed) + " м/с\n\n";
            result += "💡 **Парада:** " + advice;
        } else {
            result = "🏘️ **" + name + ", Беларусь**\n";
            result += icon + " **Погода:** " + desc + "\n";
            result += "🌡️ **Температура:** " + to_string(temp) + "°C\n";
            result += "🌡️ **Ощущается как:** " + to_string(feels_like) + "°C\n";
            result += "💧 **Влажность:** " + to_string(humidity) + "%\n";
            result += "💨 **Ветер:** " + to_string((int)wind_speed) + " м/с\n\n";
            result += "💡 **Совет:** " + advice;
        }

        return result;
    } catch (...) {
        return get_text(chat_id, "city_not_found");
    }
}

string get_weather_short(string location, long long chat_id) {
    location = normalize_location(location);

    string url = "http://api.openweathermap.org/data/2.5/weather?q=" + location + ",BY&units=metric&lang=ru&appid=" + WEATHER_API_KEY;
    auto r = cpr::Get(cpr::Url{url}, cpr::Timeout{8000});

    if (r.status_code != 200) {
        url = "http://api.openweathermap.org/data/2.5/weather?q=" + location + "&units=metric&lang=ru&appid=" + WEATHER_API_KEY;
        r = cpr::Get(cpr::Url{url}, cpr::Timeout{8000});

        if (r.status_code != 200) {
            return "⚠️ Не удалось получить погоду";
        }
    }

    try {
        auto data = json::parse(r.text);
        int temp = (int)round(data["main"]["temp"].get<double>());
        int feels_like = (int)round(data["main"]["feels_like"].get<double>());
        string desc = data["weather"][0]["description"];
        int humidity = data["main"]["humidity"].get<int>();
        double wind_speed = data["wind"]["speed"].get<double>();

        string icon = "🌡️";
        if (desc.find("ясно") != string::npos || desc.find("солнечно") != string::npos) icon = "☀️";
        else if (desc.find("облачно") != string::npos) icon = "☁️";
        else if (desc.find("дождь") != string::npos) icon = "🌧️";
        else if (desc.find("снег") != string::npos) icon = "❄️";
        else if (desc.find("туман") != string::npos) icon = "🌫️";

        string lang = user_language.count(chat_id) ? user_language[chat_id] : "ru";
        string result;

        if (lang == "en") {
            result = icon + " **" + desc + "**\n";
            result += "🌡️ " + to_string(temp) + "°C (feels like " + to_string(feels_like) + "°C)\n";
            result += "💧 Humidity: " + to_string(humidity) + "% | 💨 Wind: " + to_string((int)wind_speed) + " m/s";
        } else if (lang == "be") {
            result = icon + " **" + desc + "**\n";
            result += "🌡️ " + to_string(temp) + "°C (адчуваецца як " + to_string(feels_like) + "°C)\n";
            result += "💧 Вільготнасць: " + to_string(humidity) + "% | 💨 Вецер: " + to_string((int)wind_speed) + " м/с";
        } else {
            result = icon + " **" + desc + "**\n";
            result += "🌡️ " + to_string(temp) + "°C (ощущается как " + to_string(feels_like) + "°C)\n";
            result += "💧 Влажность: " + to_string(humidity) + "% | 💨 Ветер: " + to_string((int)wind_speed) + " м/с";
        }

        return result;
    } catch (...) {
        return "❌ Error getting weather";
    }
}

void save_user_language(long long chat_id, const string& lang) {
    user_language[chat_id] = lang;
    map<long long, string> temp;
    ifstream infile(LANGUAGE_FILE);
    long long id;
    string l;
    while (infile >> id >> l) {
        temp[id] = l;
    }
    infile.close();
    temp[chat_id] = lang;
    ofstream outfile(LANGUAGE_FILE);
    for (const auto& [uid, lg] : temp) {
        outfile << uid << " " << lg << endl;
    }
    outfile.close();
}

void load_languages() {
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
    string lang = user_language.count(chat_id) ? user_language[chat_id] : "ru";
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
    if (active_users.find(chat_id) == active_users.end()) {
        active_users.insert(chat_id);
        ofstream outfile(USERS_FILE, ios_base::app);
        outfile << chat_id << endl;
        outfile.close();
    }
}

void load_users() {
    ifstream infile(USERS_FILE);
    long long chat_id;
    while (infile >> chat_id) active_users.insert(chat_id);
    infile.close();
}

void save_user_city(long long chat_id, const string& city) {
    user_city[chat_id] = city;
    map<long long, string> temp_cities;
    ifstream infile(CITIES_FILE);
    long long id;
    string c;
    while (infile >> id >> ws && getline(infile, c)) {
        temp_cities[id] = c;
    }
    infile.close();
    temp_cities[chat_id] = city;
    ofstream outfile(CITIES_FILE);
    for (const auto& [uid, ucity] : temp_cities) {
        outfile << uid << " " << ucity << endl;
    }
    outfile.close();
}

void load_user_cities() {
    ifstream infile(CITIES_FILE);
    long long id;
    string city;
    while (infile >> id >> ws && getline(infile, city)) {
        user_city[id] = city;
    }
    infile.close();
}

void save_notification_status(long long chat_id, bool enabled) {
    user_notifications[chat_id] = enabled;
    map<long long, bool> temp;
    ifstream infile(NOTIFICATIONS_FILE);
    long long id;
    bool status;
    while (infile >> id >> status) {
        temp[id] = status;
    }
    infile.close();
    temp[chat_id] = enabled;
    ofstream outfile(NOTIFICATIONS_FILE);
    for (const auto& [uid, st] : temp) {
        outfile << uid << " " << st << endl;
    }
    outfile.close();
}

void load_notifications() {
    ifstream infile(NOTIFICATIONS_FILE);
    long long id;
    bool status;
    while (infile >> id >> status) {
        user_notifications[id] = status;
    }
    infile.close();
}

bool is_notifications_enabled(long long chat_id) {
    auto it = user_notifications.find(chat_id);
    if (it != user_notifications.end()) {
        return it->second;
    }
    return true;
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

    string lang = user_language.count(chat_id) ? user_language[chat_id] : "ru";
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
    string lang = user_language.count(chat_id) ? user_language[chat_id] : "ru";

    if (lang == "en") {
        if (kp < 4.0) return "🟢 Magnetosphere is calm. No health risks.";
        if (kp < 5.0) return "⚠️ Minor disturbances. Possible fatigue.";
        if (kp < 6.0) return "🟠 Magnetic storm (G1). Possible headaches.";
        if (kp < 7.0) return "🔴 Strong magnetic storm (G2). 🆘 Reduce stress!";
        if (kp < 8.0) return "🟣 Severe storm (G3). ⚠️ Health problems possible!";
        if (kp < 9.0) return "💀 Extreme storm (G4). 🚨 Take care!";
        return "☠️ CATASTROPHE: Maximum storm (G5)! 🚑 Immediate action!";
    } else if (lang == "be") {
        if (kp < 4.0) return "🟢 Магнітасфера спакойная. Рызыкі для здароўя адсутнічаюць.";
        if (kp < 5.0) return "⚠️ Невялікія ўзрушэнні. Магчымая падвышаная стомленасць.";
        if (kp < 6.0) return "🟠 Магнітная бура (G1). Магчымыя галаўныя болі.";
        if (kp < 7.0) return "🔴 Моцная магнітная бура (G2). 🆘 Знізьце нагрузкі!";
        if (kp < 8.0) return "🟣 Вельмі моцная бура (G3). ⚠️ Праблемы з самаадчуваннем!";
        if (kp < 9.0) return "💀 Экстрэмальная бура (G4). 🚨 Беражыце здароўе!";
        return "☠️ КАТАСТРОФА: Максімальны шторм (G5)! 🚑 Неадкладныя меры!";
    } else {
        if (kp < 4.0) return "🟢 Магнитосфера спокойная. Риски для здоровья отсутствуют.";
        if (kp < 5.0) return "⚠️ Небольшие возмущения. Возможна повышенная утомляемость.";
        if (kp < 6.0) return "🟠 Магнитная буря (G1). Возможны головные боли.";
        if (kp < 7.0) return "🔴 Сильная магнитная буря (G2). 🆘 Снизьте нагрузки!";
        if (kp < 8.0) return "🟣 Очень сильная буря (G3). ⚠️ Проблемы с самочувствием!";
        if (kp < 9.0) return "💀 Экстремальная буря (G4). 🚨 Берегите здоровье!";
        return "☠️ КАТАСТРОФА: Максимальный шторм (G5)! 🚑 Немедленные меры!";
    }
}

double fetch_current_kp() {
    auto r = cpr::Get(cpr::Url{"https://services.swpc.noaa.gov/json/planetary_k_index_1m.json"},
                      cpr::VerifySsl{false}, cpr::Timeout{8000});

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
        } catch (...) {}
    }
    return 0.67;
}

vector<KpForecast> fetch_kp_forecast_3day(long long chat_id) {
    vector<KpForecast> forecast;

    auto r = cpr::Get(cpr::Url{"https://services.swpc.noaa.gov/text/3-day-geomag-forecast.txt"},
                      cpr::VerifySsl{false}, cpr::Timeout{10000});

    if (r.status_code != 200) {
        return forecast;
    }

    try {
        stringstream ss(r.text);
        string line;

        vector<vector<double>> day_values(3);

        tm ltm = get_minsk_time();
        int current_day = ltm.tm_mday;
        int current_month = ltm.tm_mon + 1;

        auto add_day = [](int day, int month) -> pair<int, int> {
            day++;
            int days_in_month = 31;
            if (month == 4 || month == 6 || month == 9 || month == 11) days_in_month = 30;
            else if (month == 2) days_in_month = 28;

            if (day > days_in_month) {
                day = 1;
                month++;
                if (month > 12) month = 1;
            }
            return {day, month};
        };

        auto [day2, month2] = add_day(current_day, current_month);
        auto [day3, month3] = add_day(day2, month2);

        stringstream ss1, ss2, ss3;
        ss1 << setw(2) << setfill('0') << current_day << "." << setw(2) << setfill('0') << current_month;
        ss2 << setw(2) << setfill('0') << day2 << "." << setw(2) << setfill('0') << month2;
        ss3 << setw(2) << setfill('0') << day3 << "." << setw(2) << setfill('0') << month3;

        vector<string> dates = {ss1.str(), ss2.str(), ss3.str()};

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

        string lang = user_language.count(chat_id) ? user_language[chat_id] : "ru";

        for (int i = 0; i < 3; i++) {
            if (day_values[i].empty()) continue;

            KpForecast fc;
            fc.date = dates[i];
            fc.values = day_values[i];
            fc.max_kp = 0.0;

            for (double val : fc.values) {
                if (val > fc.max_kp) fc.max_kp = val;
            }

            if (lang == "en") {
                if (fc.max_kp < 4.0) fc.status = "🟢 Quiet";
                else if (fc.max_kp < 5.0) fc.status = "🟡 Unsettled";
                else if (fc.max_kp < 6.0) fc.status = "🟠 Storm G1";
                else if (fc.max_kp < 7.0) fc.status = "🔴 Storm G2";
                else if (fc.max_kp < 8.0) fc.status = "🟣 Storm G3";
                else if (fc.max_kp < 9.0) fc.status = "💀 Storm G4";
                else fc.status = "☠️ Storm G5";
            } else if (lang == "be") {
                if (fc.max_kp < 4.0) fc.status = "🟢 Спакойна";
                else if (fc.max_kp < 5.0) fc.status = "🟡 Невялікія ўзрушэнні";
                else if (fc.max_kp < 6.0) fc.status = "🟠 Магнітная бура (G1)";
                else if (fc.max_kp < 7.0) fc.status = "🔴 Моцная бура (G2)";
                else if (fc.max_kp < 8.0) fc.status = "🟣 Вельмі моцная бура (G3)";
                else if (fc.max_kp < 9.0) fc.status = "💀 Экстрэмальная бура (G4)";
                else fc.status = "☠️ Максімальны шторм (G5)";
            } else {
                if (fc.max_kp < 4.0) fc.status = "🟢 Спокойно";
                else if (fc.max_kp < 5.0) fc.status = "🟡 Небольшие возмущения";
                else if (fc.max_kp < 6.0) fc.status = "🟠 Магнитная буря (G1)";
                else if (fc.max_kp < 7.0) fc.status = "🔴 Сильная буря (G2)";
                else if (fc.max_kp < 8.0) fc.status = "🟣 Очень сильная буря (G3)";
                else if (fc.max_kp < 9.0) fc.status = "💀 Экстремальная буря (G4)";
                else fc.status = "☠️ Максимальный шторм (G5)";
            }

            forecast.push_back(fc);
        }

    } catch (const exception& e) {
        cerr << "Ошибка парсинга прогноза: " << e.what() << endl;
    }

    return forecast;
}

string get_full_magnetic_report(long long chat_id) {
    double current_kp = fetch_current_kp();
    char kp_str[10];
    snprintf(kp_str, sizeof(kp_str), "%.1f", current_kp);

    return get_text(chat_id, "current_index", string(kp_str), get_kp_status(current_kp, chat_id));
}

string get_forecast_text(long long chat_id) {
    vector<KpForecast> forecast = fetch_kp_forecast_3day(chat_id);

    if (forecast.empty()) {
        return get_text(chat_id, "city_not_found");
    }

    stringstream g;

    for (auto& fc : forecast) {
        g << "📅 **" << fc.date << "** | Макс Kp: " << fixed << setprecision(1) << fc.max_kp << " " << fc.status << "\n";

        if (!fc.values.empty()) {
            for (size_t i = 0; i < fc.values.size() && i < 8; i++) {
                string color;
                if (fc.values[i] < 4.0) color = "🟢";
                else if (fc.values[i] < 5.0) color = "🟡";
                else if (fc.values[i] < 6.0) color = "🟠";
                else if (fc.values[i] < 7.0) color = "🔴";
                else if (fc.values[i] < 8.0) color = "🟣";
                else color = "💀";

                g << "  " << setw(2) << (i * 3) << ":00 " << color << " "
                  << fixed << setprecision(1) << fc.values[i] << "\n";
            }
        }
        g << "\n";
    }

    return g.str();
}

void send_styled_msg(long long chat_id, const string& text) {
    string notifications_btn = is_notifications_enabled(chat_id) ? get_text(chat_id, "btn_notify_off") : get_text(chat_id, "btn_notify_on");

    string current_lang = user_language.count(chat_id) ? user_language[chat_id] : "ru";
    string lang_btn;
    if (current_lang == "ru") {
        lang_btn = "🇬🇧 English";
    } else if (current_lang == "en") {
        lang_btn = "🇧🇾 Беларуская";
    } else {
        lang_btn = "🇷🇺 Русский";
    }

    json kb = {
        {"keyboard", {
            {{{"text", get_text(chat_id, "btn_current")}}, {{"text", get_text(chat_id, "btn_forecast")}}},
            {{{"text", get_text(chat_id, "btn_weather")}}, {{"text", get_text(chat_id, "btn_mycity")}}},
            {{{"text", get_text(chat_id, "btn_help")}}, {{"text", notifications_btn}}},
            {{{"text", lang_btn}}}
        }},
        {"resize_keyboard", true}
    };
    cpr::Post(cpr::Url{API_URL + "/sendMessage"},
              cpr::Payload{{"chat_id", to_string(chat_id)}, {"text", text}, {"reply_markup", kb.dump()}, {"parse_mode", "Markdown"}});
}

void send_morning_report(long long chat_id) {
    tm ltm = get_minsk_time();

    string user_city_name = "Минск";
    if (user_city.count(chat_id) && !user_city[chat_id].empty()) {
        user_city_name = user_city[chat_id];
    }

    double current_kp = fetch_current_kp();
    char kp_str[10];
    snprintf(kp_str, sizeof(kp_str), "%.1f", current_kp);

    string report = get_text(chat_id, "morning_greeting") + "\n\n";
    report += "📅 " + to_string(ltm.tm_mday) + " " + get_month_name(ltm.tm_mon + 1, chat_id) + " " + to_string(ltm.tm_year + 1900) + ", " + get_weekday_name(0, chat_id) + "\n\n";

    string weather_text = get_text(chat_id, "weather_in", user_city_name);
    report += weather_text + "\n" + get_weather_short(user_city_name, chat_id) + "\n\n";
    report += get_text(chat_id, "magnetic_status") + "\n";
    report += get_text(chat_id, "kp_now", string(kp_str), get_kp_status(current_kp, chat_id)) + "\n\n";
    report += get_text(chat_id, "forecast_3days") + "\n" + get_forecast_text(chat_id) + "\n";
    report += get_text(chat_id, "wish");

    send_styled_msg(chat_id, report);
}

void scheduler() {
    map<long long, bool> morning_sent;

    while (true) {
        tm ltm = get_minsk_time();

        if (ltm.tm_hour == 9 && ltm.tm_min == 0) {
            for (long long uid : active_users) {
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
            time_t now_ts = time(nullptr);

            if (current_kp >= 5.0 &&
                (last_alert_time == 0 ||
                 (now_ts - last_alert_time) > 10800 ||
                 current_kp > last_alert_kp + 0.9)) {

                char kp_str[10];
                snprintf(kp_str, sizeof(kp_str), "%.1f", current_kp);

                for (long long uid : active_users) {
                    if (is_notifications_enabled(uid)) {
                        string alert = get_text(uid, "alert_title") + "\n\n";
                        alert += get_text(uid, "kp_current", string(kp_str)) + "\n\n";
                        alert += get_kp_status(current_kp, uid) + "\n\n";
                        alert += get_text(uid, "recommendations");

                        send_styled_msg(uid, alert);
                        this_thread::sleep_for(chrono::milliseconds(100));
                    }
                }

                last_alert_kp = current_kp;
                last_alert_time = now_ts;

                cout << "⚠️ Отправлено оповещение о буре! Kp: " << current_kp << endl;
            }
            if (current_kp < 4.0 && last_alert_time != 0 && (now_ts - last_alert_time) > 3600) {
                last_alert_kp = 0.0;
                last_alert_time = 0;
                cout << "🟢 Сброс last_alert (Kp: " << current_kp << ")" << endl;
            }
        }

        this_thread::sleep_for(chrono::seconds(30));
    }
}

int main() {
    const char* env_token = getenv("TG_BOT_TOKEN");
    if (!env_token) {
        cerr << "❌ Переменная окружения TG_BOT_TOKEN не найдена" << endl;
        return 1;
    }
    API_URL = "https://api.telegram.org/bot" + string(env_token);

    load_users();
    load_user_cities();
    load_notifications();
    load_languages();

    cout << "🤖 Белорусский бот для отслеживания магнитных бурь запущен!" << endl;
    cout << "📍 Поддерживаются любые населённые пункты Беларуси (города, деревни, посёлки)" << endl;
    cout << "🌐 Доступные языки: русский, белорусский, английский" << endl;
    cout << "✅ Активных пользователей: " << active_users.size() << endl;

    thread(scheduler).detach();
    int last_id = 0;

    while (true) {
        auto r = cpr::Get(cpr::Url{API_URL + "/getUpdates"},
                          cpr::Parameters{{"offset", to_string(last_id + 1)}, {"timeout", "25"}},
                          cpr::Timeout{30000});

        if (r.status_code == 200) {
            try {
                json data = json::parse(r.text);
                for (auto& update : data["result"]) {
                    last_id = update["update_id"];
                    if (update.contains("message") && update["message"].contains("text")) {
                        long long cid = update["message"]["chat"]["id"];
                        string txt = update["message"]["text"];

                        save_user(cid);

                        // language selection
                        if (txt == "🇷🇺 Русский" || txt == "🇬🇧 English" || txt == "🇧🇾 Беларуская") {
                            string new_lang;
                            if (txt == "🇷🇺 Русский") {
                                new_lang = "ru";
                            } else if (txt == "🇬🇧 English") {
                                new_lang = "en";
                            } else {
                                new_lang = "be";
                            }

                            string current_lang = user_language.count(cid) ? user_language[cid] : "ru";
                            if (new_lang != current_lang) {
                                save_user_language(cid, new_lang);
                                send_styled_msg(cid, get_text(cid, "language_changed"));
                            }
                            continue;
                        }

                        // input city for weather
                        if (waiting_for_city[cid]) {
                            waiting_for_city[cid] = false;

                            string normalized = normalize_location(txt);

                            string test_url = "http://api.openweathermap.org/data/2.5/weather?q=" + normalized + ",BY&units=metric&lang=ru&appid=" + WEATHER_API_KEY;
                            auto test_r = cpr::Get(cpr::Url{test_url}, cpr::Timeout{5000});

                            if (test_r.status_code != 200) {
                                test_url = "http://api.openweathermap.org/data/2.5/weather?q=" + normalized + "&units=metric&lang=ru&appid=" + WEATHER_API_KEY;
                                test_r = cpr::Get(cpr::Url{test_url}, cpr::Timeout{5000});
                            }

                            if (test_r.status_code == 200) {
                                save_user_city(cid, normalized);
                                send_styled_msg(cid, get_text(cid, "city_saved", normalized));
                            } else {
                                send_styled_msg(cid, get_text(cid, "city_not_found"));
                            }
                            continue;
                        }

                        if (txt == "/start") {
                            send_styled_msg(cid, get_text(cid, "welcome"));
                        }
                        else if (txt == get_text(cid, "btn_current")) {
                            send_styled_msg(cid, get_full_magnetic_report(cid));
                        }
                        else if (txt == get_text(cid, "btn_forecast")) {
                            string forecast_msg = get_text(cid, "forecast_title") + get_forecast_text(cid);
                            send_styled_msg(cid, forecast_msg);
                        }
                        else if (txt == get_text(cid, "btn_weather")) {
                            send_styled_msg(cid, get_text(cid, "enter_city_weather"));
                        }
                        else if (txt == get_text(cid, "btn_mycity")) {
                            waiting_for_city[cid] = true;
                            send_styled_msg(cid, get_text(cid, "enter_city"));
                        }
                        else if (txt == get_text(cid, "btn_help")) {
                            send_styled_msg(cid, get_text(cid, "help_text"));
                        }
                        else if (txt == get_text(cid, "btn_notify_on") || txt == get_text(cid, "btn_notify_off")) {
                            bool current = is_notifications_enabled(cid);
                            bool new_status = !current;
                            save_notification_status(cid, new_status);

                            if (new_status) {
                                send_styled_msg(cid, get_text(cid, "notifications_on"));
                            } else {
                                send_styled_msg(cid, get_text(cid, "notifications_off"));
                            }
                        }
                        else {
                            send_styled_msg(cid, get_weather_by_location(txt, cid));
                        }
                    }
                }
            } catch (const exception& e) {
                cerr << "Ошибка: " << e.what() << endl;
            }
        }
        this_thread::sleep_for(chrono::milliseconds(500));
    }
    return 0;
}
