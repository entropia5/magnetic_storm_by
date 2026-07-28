#pragma once

void set_waiting_for_city(long long chat_id, bool value);
void set_waiting_for_weather(long long chat_id, bool value);
bool consume_waiting_for_city(long long chat_id);
bool consume_waiting_for_weather(long long chat_id);
void clear_waiting_state(long long chat_id);
