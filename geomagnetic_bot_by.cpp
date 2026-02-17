#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <vector>
#include <set>
#include <fstream> // Для работы с файлами
#include <ctime>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

const string TOKEN = "REMOVED_TELEGRAM_TOKEN"; //здесь должен быть Ваш токен, BotFather.
const string API_URL = "https://api.telegram.org/bot" + TOKEN;

set<long long> active_users;
const string USERS_FILE = "users.txt";

// Ф-ции для работы с базой данных, чтобы новые пользователи продолжали получать рассылку по плану.

void save_user(long long chat_id) {
    if (active_users.find(chat_id) == active_users.end()) {
        active_users.insert(chat_id);
        ofstream outfile;
        outfile.open(USERS_FILE, ios_base::app); // Открываем для дозаписи
        outfile << chat_id << endl;
        outfile.close();
        cout << "[DB] Новый пользователь сохранен: " << chat_id << endl;
    }
}

void load_users() {
    ifstream infile(USERS_FILE);
    long long chat_id;
    while (infile >> chat_id) {
        active_users.insert(chat_id);
    }
    infile.close();
    cout << "[DB] Загружено пользователей из файла: " << active_users.size() << endl;
}



string get_date_str(int offset_days) {
    auto now = chrono::system_clock::to_time_t(chrono::system_clock::now() + chrono::hours(24 * offset_days));
    tm* ltm = localtime(&now);
    char buf[11];
    strftime(buf, sizeof(buf), "%Y-%m-%d", ltm);
    return string(buf);
}

string get_weekday_name(int offset_days) {
    auto now = chrono::system_clock::to_time_t(chrono::system_clock::now() + chrono::hours(24 * offset_days));
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
    string url = "https://services.swpc.noaa.gov/products/noaa-planetary-k-index.json";
    auto r = cpr::Get(cpr::Url{url});
    if (r.status_code != 200) return "⚠️ Ошибка связи с сервером.";
    try {
        json data = json::parse(r.text);
        auto last_entry = data.back();
        string kp_val = last_entry[1];
        double kp = stod(kp_val);
        string res = "⚡️ **Текущий индекс Kp: " + kp_val + "**\n";
        res += "⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯\n";
        if (kp < 4) res += "✅ Состояние магнитосферы cейчас спокойное.";
        else if (kp < 5) res += "🟡 Наблюдаются небольшие возмущения.";
        else res += "🔴 **ВНИМАНИЕ: Магнитная буря!**";
        return res;
    } catch (...) { return "❌ Ошибка данных."; }
}

string get_daily_forecast() {
    string url = "https://services.swpc.noaa.gov/products/noaa-planetary-k-index-forecast.json";
    auto r = cpr::Get(cpr::Url{url});
    if (r.status_code != 200) return "⚠️ Ошибка NOAA.";
    try {
        json data = json::parse(r.text);
        string today = get_date_str(0);
        string tomorrow = get_date_str(1);
        string report = "🏢 **Гомель, Беларусь**\n";
        report += "📅 **Прогноз: " + get_weekday_name(0) + " — " + get_weekday_name(1) + "**\n";
        report += "⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯\n";
        double max_kp = 0;
        for (size_t i = 1; i < data.size(); ++i) {
            string full_time = data[i][0];
            string date_part = full_time.substr(0, 10);
            int hour = stoi(full_time.substr(11, 2));
            if ((date_part == today && hour >= 9) || (date_part == tomorrow && hour <= 9)) {
                string kp_str = data[i][1];
                double kp = stod(kp_str);
                if (kp > max_kp) max_kp = kp;

                // Здесь определяем иконку времени (день/ночь)
                string time_icon = (hour >= 6 && hour <= 18) ? "☀️" : "🌙";
                
                // Акцент на индикаторах бурь
                string alert = "";
                if (kp >= 5)      alert = " 🔴 БУРЯ";
                else if (kp >= 4) alert = " 🟡";
                else if (kp >= 3) alert = " ⚠️";

                report += "`" + string(hour < 10 ? "0" : "") + to_string(hour) + ":00` " + time_icon + " Kp **" + kp_str.substr(0, 3) + "**" + alert + "\n";
            }
        }
        report += "⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯\n📊 **Пик за сутки:** ";
        if (max_kp < 4) report += "🟢 Низкий\n💡 День будет отличным!";
        else if (max_kp < 5) report += "🟡 Средний\n💡 Возможна усталость.";
        else report += "🔴 ВЫСОКИЙ\n💡 Избегайте нагрузок!";
        return report;
    } catch (...) { return "❌ Ошибка прогноза."; }
}

void send_styled_msg(long long chat_id, const string& text) {
    json keyboard = {
        {"keyboard", {{{{"text", "⚡️ Текущий индекс"}, {"text", "🌋 Прогноз 09:00 - 09:00"}}}}},
        {"resize_keyboard", true}
    };
    cpr::Post(cpr::Url{API_URL + "/sendMessage"}, cpr::Payload{
        {"chat_id", to_string(chat_id)}, {"text", text},
        {"reply_markup", keyboard.dump()}, {"parse_mode", "Markdown"}
    });
}

void scheduler() {
    bool sent = false;
    while (true) {
        time_t now = chrono::system_clock::to_time_t(chrono::system_clock::now());
        tm* ltm = localtime(&now);
        if (ltm->tm_hour == 9 && ltm->tm_min == 0 && !sent) {
            string rep = "📢 **Ежедневная сводка для Гомеля**\n\n" + get_daily_forecast();
            for (long long uid : active_users) send_styled_msg(uid, rep);
            sent = true;
        }
        if (ltm->tm_hour == 10) sent = false;
        this_thread::sleep_for(chrono::seconds(30));
    }
}

int main() {
    load_users(); // Загружаем ID из файла при старте
    cout << "=== Бот запущен (База пользователей активна) ===" << endl;
    thread(scheduler).detach();

    int last_id = 0;
    while (true) {
        auto response = cpr::Get(cpr::Url{API_URL + "/getUpdates"},
            cpr::Parameters{{"offset", to_string(last_id + 1)}, {"timeout", "20"}});

        if (response.status_code == 200) {
            json data = json::parse(response.text);
            for (auto& update : data["result"]) {
                last_id = update["update_id"];
                if (update.contains("message") && update["message"].contains("text")) {
                    long long cid = update["message"]["chat"]["id"];
                    string txt = update["message"]["text"];
                    
                    save_user(cid); // Сохраняем пользователя в файл

                    if (txt == "/start") {
                        send_styled_msg(cid, "👋 **Доброго времени суток, Гомель!**\n\nЯ слежу за космической погодой в Вашем городе. Прогноз каждое утро в 09:00.");
                    } else if (txt == "⚡️ Текущий индекс") {
                        send_styled_msg(cid, get_current_kp());
                    } else if (txt == "🌋 Прогноз 09:00 - 09:00") {
                        send_styled_msg(cid, get_daily_forecast());
                    }
                }
            }
        }
        this_thread::sleep_for(chrono::milliseconds(200));
    }
}