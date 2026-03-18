#pragma once
#include "mesh.h"

class Cube : public Drawable, public Transformable {
public:
    static Mesh* mesh;

    Cube();
    void draw(RenderState state) override;
};