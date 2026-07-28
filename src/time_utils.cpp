#include "time_utils.h"

#include "localization.h"

#include <chrono>

std::tm get_minsk_time(int offset_days) {
    const auto now = std::chrono::system_clock::now();
    const auto local = now + std::chrono::hours(3)
        + std::chrono::hours(24 * offset_days);
    const std::time_t value = std::chrono::system_clock::to_time_t(local);
    std::tm result{};
#ifdef _WIN32
    gmtime_s(&result, &value);
#else
    gmtime_r(&value, &result);
#endif
    return result;
}

std::string get_weekday_name(int offset_days, long long chat_id) {
    const std::tm local = get_minsk_time(offset_days);
    char buffer[64]{};
    std::strftime(buffer, sizeof(buffer), "%A", &local);
    const std::string weekday(buffer);
    const std::string language = lang_of(chat_id);
    if (language == "en") return weekday;
    if (language == "be") {
        if (weekday == "Monday") return "панядзелак";
        if (weekday == "Tuesday") return "аўторак";
        if (weekday == "Wednesday") return "серада";
        if (weekday == "Thursday") return "чацвер";
        if (weekday == "Friday") return "пятніца";
        if (weekday == "Saturday") return "субота";
        if (weekday == "Sunday") return "нядзеля";
        return weekday;
    }
    if (weekday == "Monday") return "Понедельник";
    if (weekday == "Tuesday") return "Вторник";
    if (weekday == "Wednesday") return "Среда";
    if (weekday == "Thursday") return "Четверг";
    if (weekday == "Friday") return "Пятница";
    if (weekday == "Saturday") return "Суббота";
    if (weekday == "Sunday") return "Воскресенье";
    return weekday;
}
