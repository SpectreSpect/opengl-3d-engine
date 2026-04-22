#include "logger.h"

Logger logger;

void Logger::push_level(const MultiColorString& str) {
    level_stack.push_back(str);
}

void Logger::pop_level() {
    if (level_stack.empty()) {
        throw std::runtime_error("Level stack was empty.");
    }

    level_stack.pop_back();
}

void Logger::try_pop_level() noexcept {
    if (!level_stack.empty()) {
        level_stack.pop_back();
    } 
}

void Logger::log(const MultiColorString& message) const {
    write(std::cout, message);
}

void Logger::log(std::string_view message) const {
    write(std::cout, message);
}

void Logger::log_error(const MultiColorString& error_message) const {
    write(std::cerr, error_message);
}

void Logger::log_error(std::string_view error_message) const {
    write(std::cerr, ColoredString(error_message, LoggerPalette::error));
}

void Logger::log_traceback() const {
    std::cerr << ColoredString("Traceback: ", LoggerPalette::error) << std::endl; 
    
    for (const auto& s : level_stack) {
        std::cerr << ColoredString("<<<<<< ", LoggerPalette::error) << s << std::endl;
    }
}

void Logger::write(std::ostream& os, const MultiColorString& message) const {
    if (level_stack.empty()) {
        os << message << '\n';
    } else {
        os << level_stack.back() << ": " << message << '\n';
    }
}
