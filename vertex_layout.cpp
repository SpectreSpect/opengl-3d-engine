#include "vertex_layout.h"


void VertexLayout::add(const VertexAttribute& attr) {
    attributes.push_back(attr);
}

void VertexLayout::apply() const {
    for (const auto& a : attributes) {
        glEnableVertexAttribArray(a.index);
        glVertexAttribPointer(
            a.index,
            a.size,
            a.type,
            a.normalized,
            a.stride,
            reinterpret_cast<const void*>(a.offset)
        );

        if (a.divisor > 0) {
            glVertexAttribDivisor(a.index, a.divisor);
        }
    }
}

int VertexLayout::get_next_attrib_index() const {
    int max_index = -1;
    for (const auto &a : attributes) {
        if (a.index > max_index) max_index = a.index;
    }
    return max_index + 1;
}
