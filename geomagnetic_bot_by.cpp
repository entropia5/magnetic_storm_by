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

string get_date_str(int offset) {
    auto now = chrono::system_clock::to_time_t(chrono::system_clock::now() + chrono::hours(24 * offset));
    tm* ltm = localtime(&now);
    char buf[11];
    strftime(buf, sizeof(buf), "%Y-%m-%d", ltm);
    return string(buf);
}

string get_weekday_name(int offset) {
    auto now = chrono::system_clock::to_time_t(chrono::system_clock::now() + chrono::hours(24 * offset));
    tm* ltm = localtime(&now);
    char buf[64];
    strftime(buf, sizeof(buf), "%A", ltm);
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

string get_current_kp() {
    auto r = cpr::Get(cpr::Url{"https://services.swpc.noaa.gov/products/noaa-planetary-k-index.json"});
    if (r.status_code != 200) return "⚠️ Ошибка связи с сервером NOAA.";
    try {
        json data = json::parse(r.text);
        string kp_val = data.back()[1];
        double kp = stod(kp_val);
        string res = "⚡️ **Текущий индекс Kp: " + kp_val + "**\n\n";
        if (kp < 4) res += "🟢 Магнитосфера спокойная.";
        else if (kp < 5) res += "🟡 Небольшие возмущения.";
        else res += "🔴 **ВНИМАНИЕ: Магнитная буря!**";
        return res;
    } catch (...) { return "❌ Ошибка данных Kp."; }
}

string get_daily_forecast() {
    auto r = cpr::Get(cpr::Url{"https://services.swpc.noaa.gov/products/noaa-planetary-k-index-forecast.json"});
    if (r.status_code != 200) return "⚠️ Ошибка NOAA.";
    try {
        json data = json::parse(r.text);
        string today = get_date_str(0), tomorrow = get_date_str(1);
        string report = "📅 **Прогноз бурь: " + get_weekday_name(0) + " — " + get_weekday_name(1) + "**\n\n";
        double max_kp = 0;
        for (size_t i = 1; i < data.size(); ++i) {
            string full_time = data[i][0];
            int hour = stoi(full_time.substr(11, 2));
            if ((full_time.substr(0, 10) == today && hour >= 9) || (full_time.substr(0, 10) == tomorrow && hour <= 9)) {
                string kp_str = data[i][1];
                double kp = stod(kp_str);
                if (kp > max_kp) max_kp = kp;
                report += "`" + (hour < 10 ? string("0") : "") + to_string(hour) + ":00` " + (hour >= 6 && hour <= 18 ? "☀️" : "🌙") + "Kp **" + kp_str.substr(0, 3) + "**" + (kp >= 5 ? " 🔴" : (kp >= 4 ? " 🟡" : "")) + "\n";
            }
        }
        report += "\n📊 **Пик за сутки:** " + string(max_kp < 4 ? "🟢 Низкий" : (max_kp < 5 ? "🟡 Средний" : "🔴 ВЫСОКИЙ"));
        return report;
    } catch (...) { return "❌ Ошибка прогноза."; }
}

string get_weather(string city) {
    string url = "http://api.openweathermap.org/data/2.5/weather?q=" + city + ",BY&units=metric&lang=ru&appid=" + WEATHER_API_KEY;
    auto r = cpr::Get(cpr::Url{url});
    
    if (r.status_code != 200) return "⚠️ Простите, я не смог найти этот городок в Беларуси. Проверьте, пожалуйста, нет ли ошибки в названии?";
    
    try {
        json data = json::parse(r.text);
        int current_temp = data["main"]["temp"];
        int feels_like = data["main"]["feels_like"];
        string desc = data["weather"][0]["description"];
        string city_name = data["name"];

        string advice;
        if (current_temp <= -15) {
            advice = "Ох, на улице лютый мороз! Пожалуйста, одевайтесь как можно теплее. Берегите себя...🧊";
        } else if (current_temp <= 0) {
            advice = "На улице морозно, так что не забудьте надеть любимую шапку и теплые рукавицы. Постарайтесь, чтобы Вам было уютно и тепло!";
        } else if (current_temp <= 10) {
            advice = "На улице сейчас довольно зябко. Одевайтесь, чтобы не простудиться, сейчас - самая обманчивая погода.";
        } else if (current_temp <= 20) {
            advice = "Погода приятная, но ещё не совсем лето...";
        } else {
            advice = "Какая замечательная теплынь! Можно смело выходить в лёгкой одежде. Наслаждайтесь теплом...☀️";
        }

        if (desc.find("дождь") != string::npos || desc.find("морось") != string::npos) {
            advice += "\n\nКстати, там капает дождик, так что обязательно захватите зонтик, чтобы не промокнуть...☔️";
        } else if (desc.find("снег") != string::npos) {
            advice += "\n\nБудьте осторожны, под снегом местами может быть скользко. Будьте осторожны и внимательны... ❄️";
        }

        string res = "🌡 **Погода в г. " + city_name + "**\n";
        res += "\n";
        res += "Сейчас  " + desc + ".\n";
        res += "Градусник показывает **" + to_string(current_temp) + "°C**,\n";
        res += "но по ощущениям как **" + to_string(feels_like) + "°C**.\n\n";
        res += " **Рекомендация:** " + advice;
        
        return res;
    } catch (...) {
        return "❌ Ой, что-то в системе пошло не так. Попробуйте еще разок чуть позже?";
    }
}

void send_styled_msg(long long chat_id, const string& text) {
    json kb = {
        {"keyboard", {
            {{{"text", "⚡️ Магнитные бури"}}, {{"text", "☁️ Прогноз погоды"}}},
            {{{"text", "📖 Справка"}}}
        }},
        {"resize_keyboard", true}
    };
    cpr::Post(cpr::Url{API_URL + "/sendMessage"}, cpr::Payload{
        {"chat_id", to_string(chat_id)}, {"text", text},
        {"reply_markup", kb.dump()}, {"parse_mode", "Markdown"}
    });
}

void scheduler() {
    bool sent = false;
    while (true) {
        time_t now = chrono::system_clock::to_time_t(chrono::system_clock::now());
        tm* gmtm = gmtime(&now);
        int h = (gmtm->tm_hour + 3) % 24;
        if (h == 9 && gmtm->tm_min == 0 && !sent) {
            string rep = "📢 **Доброе утро! Ежедневная сводка по бурям в Республике 🇧🇾 :**\n\n" + get_daily_forecast();
            for (long long uid : active_users) send_styled_msg(uid, rep);
            sent = true;
        }
        if (h == 10) sent = false;
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
        auto r = cpr::Get(cpr::Url{API_URL + "/getUpdates"}, cpr::Parameters{{"offset", to_string(last_id + 1)},{"timeout","20"}});
        if (r.status_code == 200) {
            json data = json::parse(r.text);
            for (auto& update : data["result"]) {
                last_id = update["update_id"];
                if (update.contains("message") && update["message"].contains("text")) {
                    long long cid = update["message"]["chat"]["id"];
                    string txt = update["message"]["text"];
                    save_user(cid);

                    if (txt == "/start") {
                        send_styled_msg(cid, "Здравствуйте!\nЯ Ваш заботливый помощник.\n Слежу за погодой и магнитными бурями по всей Республике.\n\nПостараемся не оставить без внимания даже самые маленькие городки нашей Родины🇧🇾");
                    } else if (txt == "⚡️ Магнитные бури") {
                        send_styled_msg(cid, get_current_kp() + "\n\n" + get_daily_forecast());
                    } else if (txt == "☁️ Прогноз погоды") {
                        send_styled_msg(cid, "📍 Напишите название любого города или поселка Беларуси:");
                    } else if (txt == "📖 Справка") {
                        string s = "📊 **Справка**\n\nЯ присылаю каждый день прогноз бурь в 09:00 по столичному времени.\n\n";
                        s += "🟩 0–3: Всё спокойно\n🟨 4: Возбуждённо\n🟥 5+: Магнитная буря\n\nТакже Вам доступен прогноз погоды. Нажмите кнопку Прогноз погоды и напишите город.\n\nДанные обновляются каждые 3 часа со спутников NOAA и собираются с наземных магнитометров в разных точках планеты. Каждая станция измеряет отклонение магнитного поля, затем эти данные усредняются и получается «планетарный» индекс (Kp). ";
                        send_styled_msg(cid, s);
                    } else {
                        txt.erase(0, txt.find_first_not_of(" "));
                        txt.erase(txt.find_last_not_of(" ") + 1);
                        if (!txt.empty()) {
                            send_styled_msg(cid, get_weather(txt));
                        }
                    }
                }
            }
        }
        this_thread::sleep_for(chrono::milliseconds(200));
    }
    return 0;
}