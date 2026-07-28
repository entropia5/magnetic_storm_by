#include "presentation.h"

#include "localization.h"
#include "storage.h"
#include "text_format.h"
#include "time_utils.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>

using namespace std;
using json = nlohmann::json;

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

bool kp_available(double kp) {
    return kp >= 0.0;
}

string kp_unavailable_text(long long chat_id) {
    return localize(chat_id,
        "Данные NOAA сейчас недоступны. Попробуйте обновить экран позже.",
        "Дадзеныя NOAA цяпер недаступныя. Паспрабуйце абнавіць экран пазней.",
        "NOAA data is currently unavailable. Try refreshing the screen later.");
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

string format_precipitation_compact(const WeatherForecastSlot& slot, long long chat_id) {
    double total_mm = slot.rain_mm + slot.snow_mm;
    if (total_mm >= 0.1) {
        return format_double_1(total_mm) + " мм";
    }
    if (slot.pop > 0) {
        return to_string(slot.pop) + "%";
    }
    return localize(chat_id, "сухо", "суха", "dry");
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

json make_inline_keyboard(long long chat_id, int forecast_page, int forecast_total, const string& page_callback) {
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
                    ? localize(chat_id,
                        "Магнитные бури на 3 дня →",
                        "Магнітныя буры на 3 дні →",
                        "3-day storm forecast →")
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
