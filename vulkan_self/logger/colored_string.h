#pragma once
#include <array>
#include <string>
#include <vector>
#include <cstdint>
#include <ostream>
#include <string_view>

struct ColoredString {
    std::string str = "";
    uint32_t color_rgb = 0xFFFFFFu;

    ColoredString() = default;
    ColoredString(std::string_view str) : str(str) { }
    ColoredString(std::string_view str, uint32_t color_rgb) : str(str), color_rgb(color_rgb) { }
    ColoredString(std::string_view str, std::array<uint8_t, 3> color_rgb) 
        :   str(str), 
            color_rgb((color_rgb[0] << 16) | (color_rgb[1] << 8) | color_rgb[2]) { }
    
    operator std::string() const {
        return to_color_string(str, color_rgb);
    }

    friend std::ostream& operator<<(std::ostream& os, const ColoredString& value) {
        return os << static_cast<std::string>(value);
    }

    static std::string to_color_string(std::string_view message, std::array<uint8_t, 3> color_rgb) {
        std::string r = std::to_string(color_rgb[0]);
        std::string g = std::to_string(color_rgb[1]);
        std::string b = std::to_string(color_rgb[2]);

        std::string colored_message = "\033[38;2;" + r + ";" + g + ";" + b + "m" + std::string(message) + "\033[0m";
        return colored_message;
    }

    static std::string to_color_string(std::string_view message, uint32_t color_rgb) {
        uint8_t r = (color_rgb >> 16) & 0xFFu;
        uint8_t g = (color_rgb >> 8) & 0xFFu;
        uint8_t b = color_rgb & 0xFFu;
        
        return to_color_string(message, {r, g, b});
    }
};

struct MultiColorString {
    std::vector<ColoredString> strings;
    
    MultiColorString() = default;
    MultiColorString(const ColoredString& string) : strings({string}) {}
    MultiColorString(const std::vector<ColoredString>& strings) : strings(strings) {}
    MultiColorString(std::string_view str) : strings({ColoredString(str)}) {}

    operator std::string() const {
        std::string result;

        for (const ColoredString& s : strings) {
            result += s;
        }

        return result;
    }

    friend std::ostream& operator<<(std::ostream& os, const MultiColorString& value) {
        return os << static_cast<std::string>(value);
    }
};
