#include "screen_renderer.h"

#include "runtime_state.h"
#include "screen_view_renderer.h"
#include "text_format.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

std::string render_screen_image(long long chat_id, const ScreenView& view) {
    if (!screen_renderer_available) return {};

    std::filesystem::create_directories(SCREEN_DIR);
    const auto nonce = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
    const std::string base = SCREEN_DIR + "/screen_" + std::to_string(chat_id)
        + "_" + std::to_string(nonce);
    const std::string html_path = base + ".html";
    const std::string image_path = base + ".jpg";

    std::ofstream output(html_path);
    output << render_screen_html(chat_id, view);
    output.close();

    const int width = view.kind == "morning" ? 1800 : 1280;
    const std::string command = "/usr/bin/wkhtmltoimage --quiet --width "
        + std::to_string(width) + " --quality 92 " + shell_quote(html_path)
        + " " + shell_quote(image_path);
    const int result = std::system(command.c_str());
    std::filesystem::remove(html_path);
    if (result != 0 || !std::filesystem::exists(image_path)) {
        std::cerr << "Не удалось сгенерировать JPEG экран через wkhtmltoimage\n";
        return {};
    }
    return image_path;
}

bool validate_screen_renderer() {
    screen_renderer_available = std::filesystem::exists("/usr/bin/wkhtmltoimage");
    if (!screen_renderer_available) {
        std::cerr << "⚠️ /usr/bin/wkhtmltoimage не найден: бот будет отправлять "
                     "текстовый fallback вместо JPEG-экранов\n";
    }
    return screen_renderer_available;
}
