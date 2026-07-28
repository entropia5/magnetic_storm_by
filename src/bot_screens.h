#pragma once

#include <string>

void show_home_screen(long long chat_id, const std::string& user_name = "", bool force_new_message = false);
void show_current_screen(long long chat_id);
void show_forecast_screen(long long chat_id, int page = 0);
void show_weather_prompt_screen(long long chat_id);
void show_city_prompt_screen(long long chat_id);
void show_weather_result_screen(long long chat_id, const std::string& location, bool save_city);
void show_notifications_screen(long long chat_id, bool enabled);
void show_language_screen(long long chat_id);
void show_alert_screen(long long chat_id, double current_kp, bool force_new_message = false);
void send_morning_report(long long chat_id, int page = 0);
