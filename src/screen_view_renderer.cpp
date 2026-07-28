#include "screen_view_renderer.h"

#include "localization.h"
#include "screen_assets.h"
#include "template_engine.h"
#include "text_format.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

using namespace std;

string kp_color(double kp);
string kp_short_label(double kp, long long chat_id);
string get_kp_status(double kp, long long chat_id);
string format_precipitation(const WeatherForecastSlot& slot, long long chat_id);
string format_precipitation_compact(const WeatherForecastSlot& slot, long long chat_id);
string current_minsk_datetime(long long chat_id);

string screen_body_class(const ScreenView& view) {
    string classes = "screen-" + view.kind;
    if (view.alert) {
        classes += " alert-mode";
    }
    return classes;
}

string kp_slot_time(size_t index) {
    stringstream ss;
    ss << setw(2) << setfill('0') << (int)(index * 3) << ":00";
    return ss.str();
}

string render_daily_storm_summary_html(long long chat_id, const vector<KpForecast>& summary) {
    if (summary.empty()) {
        return "";
    }

    stringstream html;
    html << "<section class='forecast-grid morning-storm-summary'>";
    for (const auto& fc : summary) {
        double min_kp = fc.values.empty() ? fc.max_kp : fc.values.front();
        double max_kp = fc.values.empty() ? fc.max_kp : fc.values.front();
        size_t min_index = 0;
        size_t max_index = 0;
        for (size_t i = 0; i < fc.values.size(); i++) {
            if (fc.values[i] < min_kp) {
                min_kp = fc.values[i];
                min_index = i;
            }
            if (fc.values[i] > max_kp) {
                max_kp = fc.values[i];
                max_index = i;
            }
        }

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
        html << "<b>" << format_double_1(min_kp) << "</b>";
        html << "<em>" << html_escape(kp_slot_time(min_index)) << "</em></div>";
        html << "<div class='forecast-stat' style='background:" << kp_color(max_kp) << "'>";
        html << "<small>" << html_escape(localize(chat_id, "Максимум за день", "Максімум за дзень", "Daily maximum")) << "</small>";
        html << "<b>" << format_double_1(max_kp) << "</b>";
        html << "<em>" << html_escape(kp_slot_time(max_index)) << "</em></div>";
        html << "<div class='forecast-stat forecast-peak' style='background:" << kp_color(max_kp) << "'>";
        html << "<small>" << html_escape(localize(chat_id, "Пик дня", "Пік дня", "Daily peak")) << "</small>";
        html << "<b>" << html_escape(kp_slot_time(max_index)) << "</b>";
        html << "<em>Kp " << format_double_1(max_kp) << "</em></div>";
        html << "</div></div>";
    }
    html << "</section>";
    return html.str();
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

    if (view.kind == "morning") {
        html << render_daily_storm_summary_html(chat_id, view.daily_storm_summary);
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
                if (view.kind == "morning") {
                    html << "<div class='weather-slot-meta weather-slot-meta-compact'>"
                         << html_escape(format_precipitation_compact(slot, chat_id)) << " · "
                         << (int)round(slot.wind_speed) << " " << html_escape(wind_unit(chat_id)) << "</div>";
                } else {
                    html << "<div class='weather-slot-meta'>"
                         << html_escape(localize(chat_id, "Осадки", "Ападкі", "Rain")) << ": "
                         << html_escape(format_precipitation(slot, chat_id)) << "<br>"
                         << html_escape(localize(chat_id, "Ветер", "Вецер", "Wind")) << ": "
                         << (int)round(slot.wind_speed) << " " << html_escape(wind_unit(chat_id)) << "</div>";
                }
                html << "</div>";
            }
            html << "</div>";
        }
        html << "</section>";
    }

    if (view.kind != "morning") {
        html << render_daily_storm_summary_html(chat_id, view.daily_storm_summary);
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

