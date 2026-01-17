#pragma once
#include "render_state.h"

class Drawable {
public:
    virtual ~Drawable() = default;
    virtual void draw(RenderState state) = 0;
};