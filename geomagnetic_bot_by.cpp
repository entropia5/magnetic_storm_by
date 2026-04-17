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
const string USERS_FILE = "users.txt";
const string CITIES_FILE = "cities.txt";
const string NOTIFICATIONS_FILE = "notifications.txt";
double last_alert_kp = 0.0;
time_t last_alert_time = 0;
map<long long, bool> waiting_for_city;

struct KpForecast {
    string date;
    double max_kp;
    string status;
    vector<double> values;
};

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

string get_weekday_name(int offset_days) {
    tm ltm = get_minsk_time(offset_days);
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

string get_kp_status(double kp) {
    if (kp < 4.0) return "🟢 Магнитосфера спокойная. Риски для здоровья отсутствуют.";
    if (kp < 5.0) return "⚠️ Небольшие возмущения. Возможна повышенная утомляемость.";
    if (kp < 6.0) return "🟠 Магнитная буря (G1). Возможны головные боли.";
    if (kp < 7.0) return "🔴 Сильная магнитная буря (G2). 🆘 Снизьте нагрузки!";
    if (kp < 8.0) return "🟣 Очень сильная буря (G3). ⚠️ Проблемы с самочувствием!";
    if (kp < 9.0) return "💀 Экстремальная буря (G4). 🚨 Берегите здоровье!";
    return "☠️ КАТАСТРОФА: Максимальный шторм (G5)! 🚑 Немедленные меры!";
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

vector<KpForecast> fetch_kp_forecast_3day() {
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
        
        for (int i = 0; i < 3; i++) {
            if (day_values[i].empty()) continue;
            
            KpForecast fc;
            fc.date = dates[i];
            fc.values = day_values[i];
            fc.max_kp = 0.0;
            
            for (double val : fc.values) {
                if (val > fc.max_kp) fc.max_kp = val;
            }
            
            if (fc.max_kp < 4.0) fc.status = "🟢 Спокойно";
            else if (fc.max_kp < 5.0) fc.status = "🟡 Небольшие возмущения";
            else if (fc.max_kp < 6.0) fc.status = "🟠 Магнитная буря (G1)";
            else if (fc.max_kp < 7.0) fc.status = "🔴 Сильная буря (G2)";
            else if (fc.max_kp < 8.0) fc.status = "🟣 Очень сильная буря (G3)";
            else if (fc.max_kp < 9.0) fc.status = "💀 Экстремальная буря (G4)";
            else fc.status = "☠️ Максимальный шторм (G5)";
            
            forecast.push_back(fc);
        }
        
    } catch (const exception& e) {
        cerr << "Ошибка парсинга прогноза: " << e.what() << endl;
    }
    
    return forecast;
}

string get_full_magnetic_report() {
    double current_kp = fetch_current_kp();
    char kp_str[10];
    snprintf(kp_str, sizeof(kp_str), "%.1f", current_kp);
    
    string result = "🛰 **Геомагнитная обстановка:**\n\n";
    result += "📊 **Индекс сейчас:** Kp " + string(kp_str) + "\n";
    result += get_kp_status(current_kp);
    return result;
}

string get_forecast_text() {
    vector<KpForecast> forecast = fetch_kp_forecast_3day();
    
    if (forecast.empty()) {
        return "Не удалось получить прогноз. Попробуйте позже.";
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

string get_weather_by_city(string city) {
    map<string, string> city_map_en = {
        {"Минск", "Minsk"}, {"Гомель", "Gomel"}, {"Брест", "Brest"},
        {"Витебск", "Vitebsk"}, {"Гродно", "Grodno"}, {"Могилёв", "Mogilev"},
        {"Могилев", "Mogilev"}
    };
    
    string encoded_city = "Minsk";
    for (const auto& [ru, en] : city_map_en) {
        if (city == ru) {
            encoded_city = en;
            break;
        }
    }
    
    string url = "http://api.openweathermap.org/data/2.5/weather?q=" + encoded_city + ",BY&units=metric&lang=ru&appid=" + WEATHER_API_KEY;
    auto r = cpr::Get(cpr::Url{url}, cpr::Timeout{8000});
    
    if (r.status_code != 200) {
        return "⚠️ Город \"" + city + "\" не найден. Попробуйте: Минск, Гомель, Брест, Витебск, Гродно, Могилёв";
    }
    
    try {
        auto data = json::parse(r.text);
        int temp = (int)round(data["main"]["temp"].get<double>());
        int feels_like = (int)round(data["main"]["feels_like"].get<double>());
        string desc = data["weather"][0]["description"];
        string name = data["name"];
        int humidity = data["main"]["humidity"].get<int>();
        double wind_speed = data["wind"]["speed"].get<double>();
        
        map<string, string> name_map = {
            {"Gomel", "Гомель"}, {"Homel", "Гомель"}, {"Minsk", "Минск"},
            {"Brest", "Брест"}, {"Vitebsk", "Витебск"}, {"Grodno", "Гродно"},
            {"Mogilev", "Могилёв"}
        };
        
        if (name_map.count(name)) {
            name = name_map[name];
        }
        
        string icon = "🌡️";
        if (desc.find("ясно") != string::npos || desc.find("солнечно") != string::npos) icon = "☀️";
        else if (desc.find("облачно") != string::npos) icon = "☁️";
        else if (desc.find("дождь") != string::npos) icon = "🌧️";
        else if (desc.find("снег") != string::npos) icon = "❄️";
        else if (desc.find("туман") != string::npos) icon = "🌫️";
        
        string advice;
        if (temp <= -15) advice = "🥶 Лютый мороз! Одевайтесь максимально тепло!";
        else if (temp <= 0) advice = "На улице морозно, не забудьте тёплую одежду.";
        else if (temp <= 10) advice = "Прохладно, возьмите с собой куртку.";
        else if (temp <= 20) advice = "Погода приятная, наслаждайтесь прогулкой 😊";
        else advice = "На улице жарко! Пейте больше воды и носите головной убор 🥵";
        
        if (desc.find("дождь") != string::npos) advice += " ☔️ Не забудьте зонт!";
        else if (desc.find("снег") != string::npos) advice += " ❄️ Осторожно, гололёд!";
        
        string result = "🏘️ **" + name + ", Беларусь**\n";
        result += icon + " **Погода:** " + desc + "\n";
        result += "🌡️ **Температура:** " + to_string(temp) + "°C\n";
        result += "🌡️ **Ощущается как:** " + to_string(feels_like) + "°C\n";
        result += "💧 **Влажность:** " + to_string(humidity) + "%\n";
        result += "💨 **Ветер:** " + to_string((int)wind_speed) + " м/с\n\n";
        result += "💡 **Совет:** " + advice;
        
        return result;
    } catch (...) { 
        return "❌ Ошибка получения погоды для города " + city; 
    }
}

string get_weather_short(string city) {
    map<string, string> city_map_en = {
        {"Минск", "Minsk"}, {"Гомель", "Gomel"}, {"Брест", "Brest"},
        {"Витебск", "Vitebsk"}, {"Гродно", "Grodno"}, {"Могилёв", "Mogilev"},
        {"Могилев", "Mogilev"}
    };
    
    string encoded_city = "Minsk";
    for (const auto& [ru, en] : city_map_en) {
        if (city == ru) {
            encoded_city = en;
            break;
        }
    }
    
    string url = "http://api.openweathermap.org/data/2.5/weather?q=" + encoded_city + ",BY&units=metric&lang=ru&appid=" + WEATHER_API_KEY;
    auto r = cpr::Get(cpr::Url{url}, cpr::Timeout{8000});
    
    if (r.status_code != 200) {
        return "⚠️ Не удалось получить погоду";
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
        
        string result = icon + " **" + desc + "**\n";
        result += "🌡️ " + to_string(temp) + "°C (ощущается как " + to_string(feels_like) + "°C)\n";
        result += "💧 Влажность: " + to_string(humidity) + "% | 💨 Ветер: " + to_string((int)wind_speed) + " м/с";
        
        return result;
    } catch (...) { 
        return "❌ Ошибка получения погоды"; 
    }
}

void send_styled_msg(long long chat_id, const string& text) {
    string notifications_btn = is_notifications_enabled(chat_id) ? "🔕 Отключить уведомления" : "🔔 Включить уведомления";
    
    json kb = {
        {"keyboard", {
            {{{"text", "📊 Текущий индекс"}}, {{"text", "📈 Прогноз на 3 дня"}}},
            {{{"text", "☁️ Погода сейчас"}}, {{"text", "📍 Мой город"}}},
            {{{"text", "📖 Справка"}}, {{"text", notifications_btn}}}
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
    
    string report = "🌅 **Доброе утро!**\n\n";
    report += "📅 *" + to_string(ltm.tm_mday) + "." + to_string(ltm.tm_mon + 1) + ".2026, " + get_weekday_name(0) + "*\n\n";
    report += "☁️ **Погода в " + user_city_name + ":**\n" + get_weather_short(user_city_name) + "\n\n";
    report += "🛰 **Магнитная обстановка:**\n";
    report += "📊 Kp " + string(kp_str) + " — " + get_kp_status(current_kp) + "\n\n";
    report += "📊 **Прогноз на 3 дня:**\n" + get_forecast_text() + "\n";
    report += "✨ Желаю вам прекрасного настроения и отличного самочувствия!\n";
    report += "Берегите себя и будьте здоровы!";
    
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
                string alert = "⚠️ **ВНИМАНИЕ! Геомагнитная буря!**\n\n";
                alert += "📈 Текущий индекс: **Kp " + string(kp_str) + "**\n\n";
                alert += get_kp_status(current_kp) + "\n\n";
                alert += "💊 **Рекомендации:**\n";
                alert += "• Больше отдыхайте\n";
                alert += "• Пейте больше воды\n";
                alert += "• Избегайте стрессов\n\n";
                alert += "🌸 Берегите своё здоровье!";
                
                for (long long uid : active_users) {
                    if (is_notifications_enabled(uid)) {
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
    
    cout << "🤖 Белорусский бот для отслеживания магнитных бурь запущен!" << endl;
    cout << "📍 Поддерживаются города: Минск, Гомель, Брест, Витебск, Гродно, Могилёв" << endl;
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
                        
                        if (waiting_for_city[cid]) {
                            waiting_for_city[cid] = false;
                            
                            string city_lower = txt;
                            transform(city_lower.begin(), city_lower.end(), city_lower.begin(), ::tolower);
                            
                            map<string, string> city_validate = {
                                {"минск", "Минск"}, {"гомель", "Гомель"}, {"брест", "Брест"},
                                {"витебск", "Витебск"}, {"гродно", "Гродно"}, {"могилёв", "Могилёв"},
                                {"могилев", "Могилёв"}
                            };
                            
                            if (city_validate.count(city_lower)) {
                                string original_name = city_validate[city_lower];
                                save_user_city(cid, original_name);
                                send_styled_msg(cid, "✅ Город **" + original_name + "** сохранён!\n\n📍 Теперь утренняя рассылка будет показывать погоду для этого города.");
                            } else {
                                send_styled_msg(cid, "❌ Город не распознан.\n\nПожалуйста, введите один из городов Беларуси:\nМинск, Гомель, Брест, Витебск, Гродно, Могилёв");
                            }
                            continue;
                        }
                        
                        if (txt == "/start") {
                            string msg = "🌤 **Здравствуйте!**\n\n";
                            msg += "Я бот для отслеживания магнитных бурь и погоды в Беларуси.\n\n";
                            msg += "📅 **Что я умею:**\n";
                            msg += "• 📊 Текущий индекс - состояние прямо сейчас\n";
                            msg += "• 📈 Прогноз на 3 дня - прогноз магнитных бурь\n";
                            msg += "• ☁️ Погода сейчас - погода в любом городе Беларуси\n";
                            msg += "• 📍 Мой город - установить город для утренней рассылки\n\n";
                            msg += "⏰ **Утренний отчёт** приходит в 9:00\n";
                            msg += "⚠️ **Оповещение о бурях** при Kp ≥ 5.0\n\n";
                            msg += "📍 По умолчанию город для рассылки - Минск.\n";
                            msg += "Используйте кнопку \"📍 Мой город\" чтобы изменить его!";
                            send_styled_msg(cid, msg);
                        }
                        else if (txt == "📊 Текущий индекс") {
                            send_styled_msg(cid, get_full_magnetic_report());
                        }
                        else if (txt == "📈 Прогноз на 3 дня") {
                            string forecast_msg = "📈 **Прогноз на 3 дня**\n\n" + get_forecast_text();
                            send_styled_msg(cid, forecast_msg);
                        }
                        else if (txt == "☁️ Погода сейчас") {
                            send_styled_msg(cid, "🇧🇾 Напишите название любого города Беларуси\n(например: Гомель, Минск, Брест, Витебск, Гродно, Могилёв)");
                        }
                        else if (txt == "📍 Мой город") {
                            waiting_for_city[cid] = true;
                            send_styled_msg(cid, "🏙️ Напишите название вашего города для утренней рассылки\n(например: Минск, Гомель, Брест, Витебск, Гродно, Могилёв)");
                        }
                        else if (txt == "📖 Справка") {
                            string help = "ℹ️ **СПРАВКА**\n\n";
                            help += "Я отслеживаю геомагнитную обстановку по данным NOAA.\n\n";
                            help += "🔹 **Утренний отчёт** в 9:00 с погодой и прогнозом\n";
                            help += "🔹 **При буре** (Kp ≥ 5.0) — мгновенное предупреждение\n";
                            help += "🔹 **Погода сейчас** — введите любой город Беларуси\n";
                            help += "🔹 **Мой город** — установите город для утренней рассылки\n";
                            help += "🔹 **Текущий индекс** — состояние прямо сейчас\n";
                            help += "🔹 **Прогноз на 3 дня** — прогноз магнитных бурь\n\n";
                            help += "📊 **Магнитный барометр:**\n";
                            help += "🟢 0-3.9 - Спокойно\n";
                            help += "🟡 4.0-4.9 - Возмущения\n";
                            help += "🟠 5.0-5.9 - Буря G1\n";
                            help += "🔴 6.0-6.9 - Буря G2\n";
                            help += "🟣 7.0+ - Сильная буря G3+\n\n";
                            help += "Берегите здоровье!";
                            send_styled_msg(cid, help);
                        }
                        else if (txt == "🔔 Включить уведомления" || txt == "🔕 Отключить уведомления") {
                            bool current = is_notifications_enabled(cid);
                            bool new_status = !current;
                            save_notification_status(cid, new_status);
                            
                            if (new_status) {
                                send_styled_msg(cid, "✅ Уведомления о магнитных бурях **включены**!\n\n⚠️ Вы будете получать оповещения при Kp ≥ 5.0");
                            } else {
                                send_styled_msg(cid, "🔕 Уведомления о магнитных бурях **отключены**!\n\nВы можете снова включить их в любой момент.");
                            }
                        }
                        else {
                            send_styled_msg(cid, get_weather_by_city(txt));
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