// ssbo.h
#pragma once

#include <GL/glew.h>
#include <cstddef>
#include <cstdint>
#include <type_traits>

class SSBO {
public:
    SSBO() noexcept = default;

    // Обычное выделение через glBufferData
    SSBO(std::size_t size_bytes, GLenum usage, const void* initial_data = nullptr);

    // Создание SSBO с persistent mapping (OpenGL 4.4 / GL_ARB_buffer_storage)
    // storage_flags обычно: GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | (опц.) GL_MAP_COHERENT_BIT
    // map_flags должны быть совместимы и включать GL_MAP_PERSISTENT_BIT.
    static SSBO create_persistent(std::size_t size_bytes,
                                 GLbitfield storage_flags,
                                 GLbitfield map_flags,
                                 const void* initial_data = nullptr);

    ~SSBO();

    SSBO(const SSBO&) = delete;
    SSBO& operator=(const SSBO&) = delete;

    SSBO(SSBO&& other) noexcept;
    SSBO& operator=(SSBO&& other) noexcept;

    // --- basic info ---
    GLuint handle() const noexcept { return id_; }
    std::size_t size_bytes() const noexcept { return size_bytes_; }
    std::size_t capacity_bytes() const noexcept { return capacity_bytes_; }
    GLenum usage() const noexcept { return usage_; }

    // --- binding ---
    void bind() const noexcept;
    static void unbind() noexcept;
    void bind_base(GLuint binding) const noexcept; // layout(std430, binding = N)
    void bind_range(GLuint binding, std::size_t offset_bytes, std::size_t size_bytes) const noexcept;

    // --- allocation / resize ---
    // Полностью переаллоцировать буфер (можно data=nullptr)
    void realloc(std::size_t new_capacity_bytes, GLenum usage, const void* data = nullptr);

    // Убедиться, что capacity >= min_capacity (при необходимости orphan+realloc)
    void ensure_capacity(std::size_t min_capacity_bytes);

    // --- updates ---
    // Обновить диапазон (без map). Хорошо для редких апдейтов.
    void update_subdata(std::size_t offset_bytes, const void* data, std::size_t data_bytes);

    // Быстрый путь для "перезаписать с 0": orphan (если надо) + map + memcpy.
    // Если persistent — то просто memcpy в mapped_ptr и (опц.) flush.
    void update_discard(const void* data, std::size_t data_bytes);

    // --- mapping ---
    // Для НЕ-persistent буфера. Вернёт nullptr если не удалось.
    void* map_range(std::size_t offset_bytes, std::size_t length_bytes, GLbitfield access_flags);
    bool  unmap(); // вернёт false если unmap сообщил GL_FALSE

    // --- persistent ---
    bool  is_persistent() const noexcept { return mapped_ptr_ != nullptr; }
    void* mapped_ptr() const noexcept { return mapped_ptr_; }

    // Если persistent и НЕ coherent — нужно flush диапазона
    void flush_range(std::size_t offset_bytes, std::size_t length_bytes);

    // --- readback (GPU -> CPU) ---
    void read_subdata(std::size_t offset_bytes, void* out, std::size_t out_bytes) const;

    template<class T>
    T read_scalar(std::size_t offset_bytes = 0) const {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
        T v{};
        read_subdata(offset_bytes, &v, sizeof(T));
        return v;
    }

    template<class T>
    void read_array(std::size_t offset_bytes, T* out, std::size_t count) const {
        static_assert(std::is_trivially_copyable_v<T>, "read_array requires trivially copyable T");
        read_subdata(offset_bytes, out, sizeof(T) * count);
    }

    uint32_t read_u32(std::size_t offset_bytes = 0) const;



private:
    void destroy() noexcept;
    void move_from(SSBO&& other) noexcept;

private:
    GLuint id_ = 0;

    std::size_t size_bytes_ = 0;      // "логический" размер данных
    std::size_t capacity_bytes_ = 0;  // реально выделенный размер
    GLenum usage_ = GL_DYNAMIC_DRAW;

    // persistent mapping
    void* mapped_ptr_ = nullptr;
    GLbitfield storage_flags_ = 0;
    GLbitfield map_flags_ = 0;
};
