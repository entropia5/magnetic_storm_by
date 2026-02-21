#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <vector>
#include <set>
#include <fstream>
#include <ctime>
#include <cstdlib>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

string API_URL;
set<long long> active_users;
const string USERS_FILE = "users.txt";

void save_user(long long chat_id) {
    if (active_users.find(chat_id) == active_users.end()) {
        active_users.insert(chat_id);
        ofstream outfile(USERS_FILE, ios_base::app);
        outfile << chat_id << endl;
        outfile.close();
        cout << "[DB] Новый пользователь: " << chat_id << endl;
    }
}

void load_users() {
    ifstream infile(USERS_FILE);
    long long chat_id;
    while (infile >> chat_id) {
        active_users.insert(chat_id);
    }
    infile.close();
    cout << "[DB] Загружено пользователей: " << active_users.size() << endl;
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
    if (r.status_code != 200) return "⚠️ Ошибка связи с сервером.";
    try {
        json data = json::parse(r.text);
        string kp_val = data.back()[1];
        double kp = stod(kp_val);
        string res = "⚡️ **Текущий индекс Kp: " + kp_val + "**\n\n";
        if (kp < 4) res += "🟢 Магнитосфера спокойная.";
        else if (kp < 5) res += "🟡 Небольшие возмущения.";
        else res += "🔴 **ВНИМАНИЕ: Магнитная буря!**";
        return res;
    } catch (...) { return "❌ Ошибка данных."; }
}

string get_daily_forecast() {
    auto r = cpr::Get(cpr::Url{"https://services.swpc.noaa.gov/products/noaa-planetary-k-index-forecast.json"});
    if (r.status_code != 200) return "⚠️ Ошибка NOAA.";
    try {
        json data = json::parse(r.text);
        string today = get_date_str(0), tomorrow = get_date_str(1);
        string report = "📅 **Прогноз: " + get_weekday_name(0) + " — " + get_weekday_name(1) + "**\n\n";
        double max_kp = 0;
        for (size_t i = 1; i < data.size(); ++i) {
            string full_time = data[i][0];
            int hour = stoi(full_time.substr(11, 2));
            if ((full_time.substr(0, 10) == today && hour >= 9) || (full_time.substr(0, 10) == tomorrow && hour <= 9)) {
                string kp_str = data[i][1];
                double kp = stod(kp_str);
                if (kp > max_kp) max_kp = kp;
                report += "`" + (hour < 10 ? string("0") : "") + to_string(hour) + ":00` " + (hour >= 6 && hour <= 18 ? "☀️" : "🌙") + " Kp **" + kp_str.substr(0, 3) + "**" + (kp >= 5 ? " 🔴" : (kp >= 4 ? " 🟡" : "")) + "\n";
            }
        }
        report += "\n📊 **Пик:** " + string(max_kp < 4 ? "🟢 Низкий" : (max_kp < 5 ? "🟡 Средний" : "🔴 ВЫСОКИЙ"));
        return report;
    } catch (...) { return "❌ Ошибка прогноза."; }
}

void send_styled_msg(long long chat_id, const string& text) {
    json kb = {
        {"keyboard", {
            {{"text", "⚡️ Текущий индекс"}, {"text", "🌋 Прогноз 09:00 - 09:00"}},
            {{"text", "🇧🇾 Выбрать город"}, {"text", "📖 Справка"}}
        }},
        {"resize_keyboard", true}
    };

    auto r = cpr::Post(
        cpr::Url{API_URL + "/sendMessage"},
        cpr::Payload{
            {"chat_id", to_string(chat_id)},
            {"text", text},
            {"reply_markup", kb.dump()},
            {"parse_mode", "Markdown"}
        }
    );

    // Если сообщение не отправилось, мы увидим причину в консоли
    if (r.status_code != 200) {
        cout << "[ERROR] Ошибка отправки: " << r.status_code << " | " << r.text << endl;
    } else {
        cout << "[LOG] Ответ отправлен пользователю " << chat_id << endl;
    }
}

void scheduler() {
    bool sent = false;
    while (true) {
        time_t now = chrono::system_clock::to_time_t(chrono::system_clock::now());
        tm* gmtm = gmtime(&now);
        int h = (gmtm->tm_hour + 3) % 24;
        if (h == 9 && gmtm->tm_min == 0 && !sent) {
            string rep = "📢 **Ежедневная сводка**\n\n" + get_daily_forecast();
            for (long long uid : active_users) send_styled_msg(uid, rep);
            sent = true;
        }
        if (h == 10) sent = false;
        this_thread::sleep_for(chrono::seconds(30));
    }
}

int main() {
    const char* env_token = getenv("TG_BOT_TOKEN");
    if (!env_token) {
        cerr << "Ошибка: TG_BOT_TOKEN не найден!" << endl;
        return 1;
    }
    API_URL = "https://api.telegram.org/bot" + string(env_token);
    load_users();
    cout << "=== Бот запущен ===" << endl;
    thread(scheduler).detach();

    int last_id = 0;
    while (true) {
        auto r = cpr::Get(cpr::Url{API_URL + "/getUpdates"}, cpr::Parameters{{"offset", to_string(last_id + 1)}, {"timeout", "20"}});
        if (r.status_code == 200) {
            json data = json::parse(r.text);
            for (auto& update : data["result"]) {
                last_id = update["update_id"];
                if (update.contains("message") && update["message"].contains("text")) {
                    long long cid = update["message"]["chat"]["id"];
                    string txt = update["message"]["text"];
                    save_user(cid);
                    
                    // Логируем в консоль для проверки связи
                    cout << "[LOG] Сообщение: " << txt << " от " << cid << endl;

                    if (txt == "/start") send_styled_msg(cid, "👋 **Здравствуйте!** Погода в космосе сегодня:");
                    else if (txt == "⚡️ Текущий индекс") send_styled_msg(cid, get_current_kp());
                    else if (txt == "🌋 Прогноз 09:00 - 09:00") send_styled_msg(cid, get_daily_forecast());
                    else if (txt == "📖 Справка") send_styled_msg(cid, "Данные NOAA. Kp-индекс показывает геомагнитную активность.");
                    else if (txt == "🇧🇾 Выбрать город") send_styled_msg(cid, "Напишите название города:");
                    else send_styled_msg(cid, "🏢 **Город: " + txt + "**\n\n" + get_daily_forecast());
                }
            }
        }
        this_thread::sleep_for(chrono::milliseconds(200));
    }
    return 0;
}