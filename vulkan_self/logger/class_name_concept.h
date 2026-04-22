#pragma once
#include <concepts>
#include <string_view>

template<class T>
concept has_class_name = requires {
    { T::k_class_name } -> std::convertible_to<std::string_view>;
};

#define _CLASS_NAME(NAME) static constexpr std::string_view k_class_name = #NAME
#define _XCLASS_NAME(NAME) _CLASS_NAME(NAME)

// В теории такой код можно было бы вынести в отельный utils для этого, но пока так...
