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

const set<string> BELARUS_CITIES = {
    "Минск", "Гомель", "Могилев", "Витебск", "Гродно", "Брест",
    "Бобруйск", "Барановичи", "Борисов", "Пинск", "Орша", "Мозырь",
    "Солигорск", "Лида", "Новополоцк", "Малорита", "Молодечно", "Полоцк",
    "Жлобин", "Светлогорск", "Речица", "Слуцк", "Жодино", "Слоним",
    "Кобрин", "Волковыск", "Калинковичи", "Сморгонь", "Рогачев", "Горки",
    "Осиповичи", "Береза", "Новогрудок", "Вилейка", "Кричев", "Лунинец",
    "Ивацевичи", "Марьина Горка", "Поставы", "Пружаны", "Глубокое", "Добруш",
    "Лепель", "Быхов", "Иваново", "Климовичи", "Шклов", "Столбцы",
    "Костюковичи", "Житковичи", "Ошмяны", "Дрогичин", "Мосты", "Щучин"
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

string get_weather(string city) {
    string url = "http://api.openweathermap.org/data/2.5/weather?q=" + city + ",BY&units=metric&lang=ru&appid=" + WEATHER_API_KEY;
    auto r = cpr::Get(cpr::Url{url});
    if (r.status_code != 200) return "⚠️ Не удалось получить погоду для города " + city;
    try {
        json data = json::parse(r.text);
        string temp = to_string((int)data["main"]["temp"]);
        string desc = data["weather"][0]["description"];
        string feels = to_string((int)data["main"]["feels_like"]);
        string humidity = to_string((int)data["main"]["humidity"]);
        
        string res = "🌡 **Погода в г. " + city + "**\n\n";
        res += "☁️ Сейчас: " + desc + "\n";
        res += "🌡 Температура: " + temp + "°C\n";
        res += "🤝 Ощущается как: " + feels + "°C\n";
        res += "💧 Влажность: " + humidity + "%\n";
        return res;
    } catch (...) { return "❌ Ошибка обработки данных погоды."; }
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

int main() {
    const char* env_token = getenv("TG_BOT_TOKEN");
    if (!env_token) return 1;
    API_URL = "https://api.telegram.org/bot" + string(env_token);
    load_users();
    cout << "=== Бот запущен (Погода + Бури) ===" << endl;

    int last_id = 0;
    while (true) {
        auto r = cpr::Get(cpr::Url{API_URL + "/getUpdates"}, cpr::Parameters{{"offset", to_string(last_id + 1)}, {"timeout","20"}});
        if (r.status_code == 200) {
            json data = json::parse(r.text);
            for (auto& update : data["result"]) {
                last_id = update["update_id"];
                if (update.contains("message") && update["message"].contains("text")) {
                    long long cid = update["message"]["chat"]["id"];
                    string txt = update["message"]["text"];
                    save_user(cid);

                    if (txt == "/start") {
                        send_styled_msg(cid, "👋 Привет! Выберите тип прогноза:");
                    } else if (txt == "⚡️ Магнитные бури") {
                        send_styled_msg(cid, get_current_kp());
                    } else if (txt == "☁️ Прогноз погоды") {
                        send_styled_msg(cid, "📍 Напишите название города Беларуси для прогноза погоды:");
                    } else if (txt == "📖 Справка") {
                        send_styled_msg(cid, "Бот показывает планетарный Kp-индекс и локальную погоду в городах РБ.");
                    } else {
                        if (BELARUS_CITIES.find(txt) != BELARUS_CITIES.end()) {
                            send_styled_msg(cid, get_weather(txt));
                        } else {
                            send_styled_msg(cid, "❌ Город **" + txt + "** не найден в базе РБ. Попробуйте Минск, Гомель...");
                        }
                    }
                }
            }
        }
        this_thread::sleep_for(chrono::milliseconds(200));
    }
    return 0;
}