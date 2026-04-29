#pragma once
#include <optional>
#include <cstdint>

struct QueueFamilyIndices {
    std::optional<uint32_t> graphics_family;
    std::optional<uint32_t> present_family;

    inline bool is_complete() const noexcept {
        return graphics_family.has_value() && present_family.has_value();
    }
};
