#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <vector>
#include <set>
#include <fstream>
#include <ctime>
#include <cstdlib>
#include <algorithm>
#include <sstream>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

string API_URL;
string WEATHER_API_KEY = "60699f459685a5a585b1ee8839ce491b";
set<long long> active_users;
const string USERS_FILE = "users.txt";

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

tm get_gomel_time_safe(int offset_days = 0) {
    auto now = chrono::system_clock::now();
    auto gomel_now = now + chrono::hours(3) + chrono::hours(24 * offset_days);
    time_t t = chrono::system_clock::to_time_t(gomel_now);
    tm result;
#ifdef _WIN32
    gmtime_s(&result, &t);
#else
    gmtime_r(&t, &result);
#endif
    return result;
}

string get_date_str(int offset_days) {
    tm ltm = get_gomel_time_safe(offset_days);
    char buf[11];
    strftime(buf, sizeof(buf), "%Y-%m-%d", &ltm);
    return string(buf);
}

string get_weekday_name(int offset_days) {
    tm ltm = get_gomel_time_safe(offset_days);
    char buf[64];
    strftime(buf, sizeof(buf), "%A", &ltm);
    string w(buf);
    if (w == "Monday") return "Понедельник";
    if (w == "Tuesday") return "Вторник";
    if (w == "Wednesday") return "Среда";
    if (w == "Thursday") return "Четверг";
    if (w == "Friday") return "Пятница";
    if (w == "Saturday") return "Суббота";
    if (w == "Sunday") return "Воскресенье";
    return w;
}

string parse_noaa_txt(const string& txt) {
    try {
        stringstream ss(txt);
        string line, last_valid_line;
        while (getline(ss, line)) {
            if (line.length() > 20 && isdigit(line[0])) last_valid_line = line;
        }
        if (last_valid_line.empty()) return "";

        stringstream line_ss(last_valid_line);
        string val;
        vector<string> tokens;
        while (line_ss >> val) tokens.push_back(val);

        string kp_val = "0";
        for (int i = tokens.size() - 1; i >= 3; --i) {
            if (tokens[i].find("-") == string::npos) {
                kp_val = tokens[i];
                break;
            }
        }
        double kp = stod(kp_val);
        string res = "📖 **Данные из журнала (SWPC): " + kp_val + "**\n\n";
        if (kp < 4) res += "🟢 Магнитосфера спокойная.";
        else if (kp < 5) res += "🟡 Небольшие возмущения.";
        else res += "🔴 **ВНИМАНИЕ: Магнитная буря!**";
        return res;
    } catch (...) { return ""; }
}

string parse_gfz_json(const string& json_text) {
    try {
        json data = json::parse(json_text);
        if (!data.contains("index_values") || data["index_values"].empty()) return "";
        double kp = data["index_values"].back().get<double>();
        string kp_val = to_string(kp).substr(0, 4);
        string res = "🇩🇪 **Германия (GFZ): " + kp_val + "**\n\n";
        if (kp < 4) res += "🟢 Магнитосфера спокойная.";
        else if (kp < 5) res += "🟡 Небольшие возмущения.";
        else res += "🔴 **ВНИМАНИЕ: Магнитная буря!**";
        return res;
    } catch (...) { return ""; }
}

string get_current_kp() {
    auto r1 = cpr::Get(cpr::Url{"https://services.swpc.noaa.gov/products/noaa-planetary-k-index.json"}, 
                       cpr::Timeout{8000}, cpr::Redirect{true}, cpr::VerifySsl{false});
    if (r1.status_code == 200) {
        try {
            json data = json::parse(r1.text);
            for (int i = data.size() - 1; i >= 0; --i) {
                if (data[i].is_array() && data[i].size() >= 2 && data[i][1] != "kp") {
                    string val = data[i][1].is_string() ? data[i][1].get<string>() : to_string(data[i][1].get<double>());
                    if (!val.empty() && val.find_first_of("0123456789") != string::npos) {
                        double kp = stod(val);
                        string res = "🇧🇾 **Индекс сейчас (NOAA): " + val.substr(0, 4) + "**\n\n";
                        if (kp < 4) res += "🟢 Магнитосфера спокойная.";
                        else if (kp < 5) res += "🟡 Небольшие возмущения.";
                        else res += "🔴 **ВНИМАНИЕ: Магнитная буря!**";
                        return res;
                    }
                }
            }
        } catch (...) {}
    }

    auto r2 = cpr::Get(cpr::Url{"https://kp.gfz.de/app/json/?start=now-24h&end=now&index=Kp"}, 
                       cpr::Timeout{8000}, cpr::Redirect{true}, cpr::VerifySsl{false});
    if (r2.status_code == 200) {
        string res = parse_gfz_json(r2.text);
        if (!res.empty()) return res;
    }

    auto r3 = cpr::Get(cpr::Url{"https://services.swpc.noaa.gov/text/daily-geomagnetic-indices.txt"}, 
                       cpr::Timeout{8000}, cpr::VerifySsl{false});
    if (r3.status_code == 200) {
        string res = parse_noaa_txt(r3.text);
        if (!res.empty()) return res;
    }

    return "⚠️ Серверы погоды США и Германии сейчас недоступны.";
}

string get_daily_forecast() {
    auto r = cpr::Get(cpr::Url{"https://services.swpc.noaa.gov/products/noaa-planetary-k-index-forecast.json"}, 
                      cpr::Timeout{10000}, cpr::Redirect{true}, cpr::VerifySsl{false});
    if (r.status_code != 200) return "⚠️ Ошибка прогноза (NOAA Offline).";
    try {
        json data = json::parse(r.text);
        string today = get_date_str(0), tomorrow = get_date_str(1);
        string report = "📅 **Прогноз бурь: " + get_weekday_name(0) + " — " + get_weekday_name(1) + "**\n\n";
        double max_kp = 0;
        bool found = false;
        for (size_t i = 1; i < data.size(); ++i) {
            if (!data[i].is_array() || data[i].size() < 2) continue;
            string full_time = data[i][0];
            if (full_time.length() < 13) continue;
            int hour = stoi(full_time.substr(11, 2));
            string date_part = full_time.substr(0, 10);
            if ((date_part == today && hour >= 9) || (date_part == tomorrow && hour <= 9)) {
                string kp_str = data[i][1].is_string() ? data[i][1].get<string>() : to_string(data[i][1].get<double>());
                if (kp_str.empty() || kp_str.find_first_of("0123456789") == string::npos) continue;
                double kp = stod(kp_str);
                found = true;
                if (kp > max_kp) max_kp = kp;
                report += "`" + (hour < 10 ? string("0") : "") + to_string(hour) + ":00` " + (hour >= 6 && hour <= 18 ? "☀️" : "🌙") + "Kp **" + kp_str.substr(0, 3) + "**" + (kp >= 5 ? " 🔴" : (kp >= 4 ? " 🟡" : "")) + "\n";
            }
        }
        if (!found) return "❌ Актуальный прогноз пока не обновлен.";
        report += "\n📊 **Пик за сутки:** " + string(max_kp < 4 ? "🟢 Низкий" : (max_kp < 5 ? "🟡 Средний" : "🔴 ВЫСОКИЙ"));
        return report;
    } catch (...) { return "❌ Ошибка прогноза."; }
}

string get_weather(string city) {
    string url = "http://api.openweathermap.org/data/2.5/weather?q=" + city + ",BY&units=metric&lang=ru&appid=" + WEATHER_API_KEY;
    auto r = cpr::Get(cpr::Url{url}, cpr::Timeout{10000});
    if (r.status_code != 200) return "⚠️ Простите, я не смог найти этот городок в Беларуси.";
    try {
        json data = json::parse(r.text);
        int current_temp = data["main"]["temp"];
        int feels_like = data["main"]["feels_like"];
        string desc = data["weather"][0]["description"];
        string city_name = data["name"];
        string advice;
        if (current_temp <= -15) advice = "Ох, на улице лютый мороз! Пожалуйста, одевайтесь как можно теплее.\nБерегите себя...🧊";
        else if (current_temp <= 0) advice = "На улице морозно, не забудьте надеть шапку и рукавицы.";
        else if (current_temp <= 10) advice = "На улице зябко. Одевайтесь, чтобы не простудиться.";
        else if (current_temp <= 20) advice = "Погода приятная, но ещё не совсем лето...";
        else advice = "Какая замечательная теплынь! Наслаждайтесь теплом...☀️";
        if (desc.find("дождь") != string::npos || desc.find("морось") != string::npos) advice += "\n\nЗахватите зонтик...☔️";
        else if (desc.find("снег") != string::npos) advice += "\n\nИдет снежок... Осторожно, может быть скользко ❄️";
        string res = "🌡 **Погода в г. " + city_name + "**\n\nСейчас " + desc + ".\nТемпература **" + to_string(current_temp) + "°C** (ощущается как **" + to_string(feels_like) + "°C**).\n\n **Рекомендация:** " + advice;
        return res;
    } catch (...) { return "❌ Ошибка системы."; }
}

void send_styled_msg(long long chat_id, const string& text) {
    json kb = {
        {"keyboard", {{{{"text", "⚡️ Магнитные бури"}}, {{"text", "☁️ Прогноз погоды"}}}, {{{"text", "📖 Справка"}}}}},
        {"resize_keyboard", true}
    };
    cpr::Post(cpr::Url{API_URL + "/sendMessage"}, cpr::Payload{{"chat_id", to_string(chat_id)}, {"text", text}, {"reply_markup", kb.dump()}, {"parse_mode", "Markdown"}});
}

void scheduler() {
    bool sent = false;
    while (true) {
        tm ltm = get_gomel_time_safe();
        if (ltm.tm_hour == 9 && ltm.tm_min == 0 && !sent) {
            string rep = "📢 **Доброе утро! Ежедневная сводка по бурям в Республике 🇧🇾 :**\n\n" + get_daily_forecast();
            for (long long uid : active_users) send_styled_msg(uid, rep);
            sent = true;
        }
        if (ltm.tm_hour == 10) sent = false;
        this_thread::sleep_for(chrono::seconds(30));
    }
}

int main() {
    const char* env_token = getenv("TG_BOT_TOKEN");
    if (!env_token) return 1;
    API_URL = "https://api.telegram.org/bot" + string(env_token);
    load_users();
    thread(scheduler).detach();
    int last_id = 0;
    while (true) {
        auto r = cpr::Get(cpr::Url{API_URL + "/getUpdates"}, cpr::Parameters{{"offset", to_string(last_id + 1)},{"timeout","20"}}, cpr::Timeout{30000});
        if (r.status_code == 200) {
            try {
                json data = json::parse(r.text);
                for (auto& update : data["result"]) {
                    last_id = update["update_id"];
                    if (update.contains("message") && update["message"].contains("text")) {
                        long long cid = update["message"]["chat"]["id"];
                        string txt = update["message"]["text"];
                        save_user(cid);
                        if (txt == "/start") send_styled_msg(cid, "Здравствуйте!\nЯ Ваш метео-помощник.🇧🇾");
                        else if (txt == "⚡️ Магнитные бури") send_styled_msg(cid, get_current_kp() + "\n\n" + get_daily_forecast());
                        else if (txt == "☁️ Прогноз погоды") send_styled_msg(cid, "📍 Напишите название любого города Беларуси:");
                        else if (txt == "📖 Справка") {
                            string s = "📊 **Справка**\n\nПрогноз бурь в 09:00.\n\n🟩 0–3: Спокойно\n🟨 4: Возбуждённо\n🟥 5+: Буря";
                            send_styled_msg(cid, s);
                        } else {
                            txt.erase(0, txt.find_first_not_of(" "));
                            txt.erase(txt.find_last_not_of(" ") + 1);
                            if (!txt.empty()) send_styled_msg(cid, get_weather(txt));
                        }
                    }
                }
            } catch (...) {}
        }
        this_thread::sleep_for(chrono::milliseconds(200));
    }
    return 0;
}