#pragma once
#include <array>
#include <vector>
#include <string>
#include <iostream>
#include <concepts>
#include <stdexcept>
#include <string_view>

#include "colored_string.h"
#include "palette.h"

class Logger {
public:
    void push_level(const MultiColorString& str);
    void pop_level();
    void try_pop_level() noexcept;
    
    void log(const MultiColorString& message) const;
    void log(std::string_view message) const;
    void log_error(const MultiColorString& error_message) const;
    void log_error(std::string_view error_message) const;
    void log_traceback() const;
    
    template<class Exception = std::runtime_error>
    requires std::derived_from<Exception, std::exception> &&
             std::constructible_from<Exception, std::string>
    inline void throw_error(const MultiColorString& error_message) const {
        log_traceback();

        std::cerr << ColoredString("Error: ", LoggerPalette::error) << error_message << std::endl;
        throw Exception(static_cast<std::string>(error_message));
    }

    template<class Exception = std::runtime_error>
    requires std::derived_from<Exception, std::exception> &&
             std::constructible_from<Exception, std::string>
    inline void throw_error(std::string_view error_message) const {
        throw_error<Exception>(ColoredString(error_message, LoggerPalette::error));
    }

    template<class Exception = std::runtime_error>
    requires std::derived_from<Exception, std::exception> &&
             std::constructible_from<Exception, std::string>
    inline void assert(bool condition, const MultiColorString& error_message) const {
        if (!condition) {
            throw_error<Exception>(error_message);
        }
    }

    template<class Exception = std::runtime_error>
    requires std::derived_from<Exception, std::exception> &&
             std::constructible_from<Exception, std::string>
    inline void assert(bool condition, std::string_view error_message) const {
        if (!condition) {
            throw_error<Exception>(ColoredString(error_message, LoggerPalette::error));
        }
    }

private:
    void write(std::ostream& os, const MultiColorString& message) const;
    std::vector<MultiColorString> level_stack;
};
