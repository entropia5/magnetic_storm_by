#pragma once

#include <string>
#include <vector>

void save_bot_state();
void load_bot_state();
void save_user(long long chat_id);
void load_users();
void save_user_city(long long chat_id, const std::string& city);
void load_user_cities();
void save_notification_status(long long chat_id, bool enabled);
void load_notifications();
void save_live_message_id(long long chat_id, int message_id);
int known_live_message_id(long long chat_id);
void save_supplement_message_id(long long chat_id, int message_id);
void load_live_messages();
void load_supplement_messages();
bool is_notifications_enabled(long long chat_id);
std::vector<long long> active_user_snapshot();
std::string user_city_or_default(long long chat_id);
