#include "utilities.h"

#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

std::string trim_copy(const std::string& value) {
    const std::string whitespace = " \t\r\n";
    const std::size_t first = value.find_first_not_of(whitespace);
    if (first == std::string::npos) {
        return {};
    }
    const std::size_t last = value.find_last_not_of(whitespace);
    return value.substr(first, last - first + 1);
}

std::string ascii_lower_copy(std::string value) {
    for (char& character : value) {
        const auto byte = static_cast<unsigned char>(character);
        if (byte < 128) {
            character = static_cast<char>(std::tolower(byte));
        }
    }
    return value;
}

bool write_lines_atomic(const std::string& path, const std::vector<std::string>& lines) {
    const auto nonce = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
    const std::string temp_path = path + ".tmp." + std::to_string(nonce);

    {
        std::ofstream output(temp_path, std::ios::trunc);
        if (!output) {
            std::cerr << "Не удалось открыть временный файл для записи: " << temp_path << '\n';
            return false;
        }
        for (const std::string& line : lines) {
            output << line << '\n';
        }
        output.flush();
        if (!output) {
            std::cerr << "Не удалось записать временный файл: " << temp_path << '\n';
            std::filesystem::remove(temp_path);
            return false;
        }
    }

    std::error_code error;
    std::filesystem::rename(temp_path, path, error);
    if (error) {
        std::cerr << "Не удалось заменить файл " << path << ": " << error.message() << '\n';
        std::filesystem::remove(temp_path);
        return false;
    }
    return true;
}

bool write_json_atomic(const std::string& path, const nlohmann::json& data) {
    const auto nonce = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
    const std::string temp_path = path + ".tmp." + std::to_string(nonce);

    {
        std::ofstream output(temp_path, std::ios::trunc);
        if (!output) {
            std::cerr << "Не удалось открыть временный JSON-файл для записи: "
                      << temp_path << '\n';
            return false;
        }
        output << data.dump(2) << '\n';
        output.flush();
        if (!output) {
            std::cerr << "Не удалось записать временный JSON-файл: " << temp_path << '\n';
            std::filesystem::remove(temp_path);
            return false;
        }
    }

    std::error_code error;
    std::filesystem::rename(temp_path, path, error);
    if (error) {
        std::cerr << "Не удалось заменить JSON-файл " << path << ": "
                  << error.message() << '\n';
        std::filesystem::remove(temp_path);
        return false;
    }
    return true;
}
