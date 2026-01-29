// ssbo.cpp
#include "ssbo.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

static void require_gl_buffer_storage() {
    // Для persistent нужен OpenGL 4.4 или расширение ARB_buffer_storage
    if (!(GLEW_VERSION_4_4 || GLEW_ARB_buffer_storage)) {
        throw std::runtime_error("SSBO persistent mapping requires OpenGL 4.4 or GL_ARB_buffer_storage");
    }
}

SSBO::SSBO(std::size_t size_bytes, GLenum usage, const void* initial_data)
{
    if (size_bytes == 0) {
        throw std::invalid_argument("SSBO: size_bytes must be > 0");
    }

    glGenBuffers(1, &id_);
    if (id_ == 0) {
        throw std::runtime_error("SSBO: glGenBuffers failed (no GL context?)");
    }

    usage_ = usage;
    size_bytes_ = size_bytes;
    capacity_bytes_ = size_bytes;

    bind();
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 static_cast<GLsizeiptr>(capacity_bytes_),
                 initial_data,
                 usage_);
    unbind();
}

SSBO SSBO::create_persistent(std::size_t size_bytes,
                            GLbitfield storage_flags,
                            GLbitfield map_flags,
                            const void* initial_data)
{
    require_gl_buffer_storage();

    if (size_bytes == 0) {
        throw std::invalid_argument("SSBO: size_bytes must be > 0");
    }
    if ((storage_flags & GL_MAP_PERSISTENT_BIT) == 0 || (map_flags & GL_MAP_PERSISTENT_BIT) == 0) {
        throw std::invalid_argument("SSBO persistent: GL_MAP_PERSISTENT_BIT must be set in both storage_flags and map_flags");
    }
    if ((map_flags & GL_MAP_WRITE_BIT) == 0) {
        throw std::invalid_argument("SSBO persistent: map_flags must include GL_MAP_WRITE_BIT (for uploads)");
    }

    SSBO b;
    glGenBuffers(1, &b.id_);
    if (b.id_ == 0) {
        throw std::runtime_error("SSBO: glGenBuffers failed (no GL context?)");
    }

    b.usage_ = GL_DYNAMIC_DRAW; // usage тут не используется (bufferStorage), но оставим “разумное”
    b.size_bytes_ = size_bytes;
    b.capacity_bytes_ = size_bytes;
    b.storage_flags_ = storage_flags;
    b.map_flags_ = map_flags;

    b.bind();
    glBufferStorage(GL_SHADER_STORAGE_BUFFER,
                    static_cast<GLsizeiptr>(b.capacity_bytes_),
                    initial_data,
                    storage_flags);

    void* ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER,
                                0,
                                static_cast<GLsizeiptr>(b.capacity_bytes_),
                                map_flags);
    if (!ptr) {
        b.unbind();
        b.destroy();
        throw std::runtime_error("SSBO persistent: glMapBufferRange failed");
    }

    b.mapped_ptr_ = ptr;
    b.unbind();

    return b;
}

SSBO::~SSBO() {
    destroy();
}

void SSBO::destroy() noexcept {
    if (id_ != 0) {
        // Если буфер маппился — аккуратно размэпим
        // (в persistent режиме это допустимо сделать перед удалением)
        if (mapped_ptr_) {
            bind();
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
            unbind();
            mapped_ptr_ = nullptr;
        }

        glDeleteBuffers(1, &id_);
        id_ = 0;
    }
    size_bytes_ = 0;
    capacity_bytes_ = 0;
    usage_ = GL_DYNAMIC_DRAW;
    storage_flags_ = 0;
    map_flags_ = 0;
}

SSBO::SSBO(SSBO&& other) noexcept {
    move_from(std::move(other));
}

SSBO& SSBO::operator=(SSBO&& other) noexcept {
    if (this != &other) {
        destroy();
        move_from(std::move(other));
    }
    return *this;
}

void SSBO::move_from(SSBO&& other) noexcept {
    id_ = other.id_;
    size_bytes_ = other.size_bytes_;
    capacity_bytes_ = other.capacity_bytes_;
    usage_ = other.usage_;
    mapped_ptr_ = other.mapped_ptr_;
    storage_flags_ = other.storage_flags_;
    map_flags_ = other.map_flags_;

    other.id_ = 0;
    other.size_bytes_ = 0;
    other.capacity_bytes_ = 0;
    other.mapped_ptr_ = nullptr;
    other.storage_flags_ = 0;
    other.map_flags_ = 0;
}

void SSBO::bind() const noexcept {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, id_);
}

void SSBO::unbind() noexcept {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void SSBO::bind_base(GLuint binding) const noexcept {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, id_);
}

void SSBO::bind_range(GLuint binding, std::size_t offset_bytes, std::size_t size_bytes) const noexcept {
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER,
                      binding,
                      id_,
                      static_cast<GLintptr>(offset_bytes),
                      static_cast<GLsizeiptr>(size_bytes));
}

void SSBO::realloc(std::size_t new_capacity_bytes, GLenum usage, const void* data) {
    if (new_capacity_bytes == 0) {
        throw std::invalid_argument("SSBO::realloc: new_capacity_bytes must be > 0");
    }
    if (is_persistent()) {
        // Persistent буфер нельзя просто так realloc-нуть через glBufferData.
        // (Можно создать новый SSBO и move-assign.)
        throw std::runtime_error("SSBO::realloc: not supported for persistent-mapped buffers (create a new buffer and move-assign)");
    }

    usage_ = usage;
    capacity_bytes_ = new_capacity_bytes;
    size_bytes_ = std::min(size_bytes_, capacity_bytes_);

    bind();
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 static_cast<GLsizeiptr>(capacity_bytes_),
                 data,
                 usage_);
    unbind();
}

void SSBO::ensure_capacity(std::size_t min_capacity_bytes) {
    if (min_capacity_bytes <= capacity_bytes_) return;
    if (is_persistent()) {
        throw std::runtime_error("SSBO::ensure_capacity: not supported for persistent-mapped buffers (create a new larger buffer)");
    }

    std::size_t new_cap = std::max(min_capacity_bytes, capacity_bytes_ ? capacity_bytes_ * 2 : min_capacity_bytes);
    realloc(new_cap, usage_, nullptr); // orphan
}

void SSBO::update_subdata(std::size_t offset_bytes, const void* data, std::size_t data_bytes) {
    if (!data || data_bytes == 0) return;
    if (offset_bytes + data_bytes > capacity_bytes_) {
        throw std::out_of_range("SSBO::update_subdata: write out of buffer capacity");
    }

    bind();
    glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                    static_cast<GLintptr>(offset_bytes),
                    static_cast<GLsizeiptr>(data_bytes),
                    data);
    unbind();

    size_bytes_ = std::max(size_bytes_, offset_bytes + data_bytes);
}

void SSBO::update_discard(const void* data, std::size_t data_bytes) {
    if (!data || data_bytes == 0) return;

    if (is_persistent()) {
        if (data_bytes > capacity_bytes_) {
            throw std::runtime_error("SSBO::update_discard: persistent buffer too small (create a new larger buffer and move-assign)");
        }
        std::memcpy(mapped_ptr_, data, data_bytes);
        size_bytes_ = data_bytes;

        if ((storage_flags_ & GL_MAP_COHERENT_BIT) == 0) {
            flush_range(0, data_bytes);
        }
        return;
    }

    if (data_bytes > capacity_bytes_) {
        ensure_capacity(data_bytes);
    }

    // orphan + map + memcpy (без UNSYNCHRONIZED)
    bind();

    // Если хочешь максимально избегать stall даже при одинаковом capacity:
    // orphan можно делать каждый раз при полном переписывании:
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 static_cast<GLsizeiptr>(capacity_bytes_),
                 nullptr,
                 usage_);

    void* ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER,
                                0,
                                static_cast<GLsizeiptr>(data_bytes),
                                GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

    if (ptr) {
        std::memcpy(ptr, data, data_bytes);
        GLboolean ok = glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        unbind();
        if (ok == GL_FALSE) {
            throw std::runtime_error("SSBO::update_discard: glUnmapBuffer reported GL_FALSE");
        }
    } else {
        // fallback
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(data_bytes), data);
        unbind();
    }

    size_bytes_ = data_bytes;
}

void* SSBO::map_range(std::size_t offset_bytes, std::size_t length_bytes, GLbitfield access_flags) {
    if (is_persistent()) {
        // В persistent режиме смысла мапить заново нет — уже есть mapped_ptr_
        // (если всё-таки нужно — лучше сделать отдельный метод, но обычно не надо)
        return nullptr;
    }
    if (offset_bytes + length_bytes > capacity_bytes_) return nullptr;

    bind();
    void* ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER,
                                static_cast<GLintptr>(offset_bytes),
                                static_cast<GLsizeiptr>(length_bytes),
                                access_flags);
    // NOTE: unbind делать нельзя, пока ptr используется (по-хорошему можно, но лучше не усложнять)
    return ptr;
}

bool SSBO::unmap() {
    if (is_persistent()) return true; // persistent не размапливаем “по ходу”
    GLboolean ok = glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    unbind();
    return ok == GL_TRUE;
}

void SSBO::flush_range(std::size_t offset_bytes, std::size_t length_bytes) {
    if (!is_persistent()) return;
    if ((storage_flags_ & GL_MAP_COHERENT_BIT) != 0) return; // coherent -> flush не нужен

    bind();
    glFlushMappedBufferRange(GL_SHADER_STORAGE_BUFFER,
                             static_cast<GLintptr>(offset_bytes),
                             static_cast<GLsizeiptr>(length_bytes));
    unbind();
}

void SSBO::read_subdata(std::size_t offset_bytes, void* out, std::size_t out_bytes) const {
    if (!out || out_bytes == 0) return;
    if (offset_bytes + out_bytes > capacity_bytes_) {
        throw std::out_of_range("SSBO::read_subdata: out of range");
    }
    
    bind();
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER,
                       (GLintptr)offset_bytes,
                       (GLsizeiptr)out_bytes,
                       out);
    unbind();
}

uint32_t SSBO::read_u32(std::size_t offset_bytes) const {
    uint32_t v = 0;
    read_subdata(offset_bytes, &v, sizeof(uint32_t));
    return v;
}
