#include "conversation_state.h"

#include "runtime_state.h"

#include <mutex>

void set_waiting_for_city(long long chat_id, bool value) {
    std::lock_guard<std::mutex> lock(conversation_mutex);
    waiting_for_city[chat_id] = value;
}

void set_waiting_for_weather(long long chat_id, bool value) {
    std::lock_guard<std::mutex> lock(conversation_mutex);
    waiting_for_weather[chat_id] = value;
}

bool consume_waiting_for_city(long long chat_id) {
    std::lock_guard<std::mutex> lock(conversation_mutex);
    const bool value = waiting_for_city[chat_id];
    waiting_for_city[chat_id] = false;
    return value;
}

bool consume_waiting_for_weather(long long chat_id) {
    std::lock_guard<std::mutex> lock(conversation_mutex);
    const bool value = waiting_for_weather[chat_id];
    waiting_for_weather[chat_id] = false;
    return value;
}

void clear_waiting_state(long long chat_id) {
    std::lock_guard<std::mutex> lock(conversation_mutex);
    waiting_for_city[chat_id] = false;
    waiting_for_weather[chat_id] = false;
}
