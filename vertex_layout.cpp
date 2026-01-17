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
