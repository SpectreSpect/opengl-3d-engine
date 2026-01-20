#include "render_object.h"

void RenderObject::draw(RenderState state) {
    if (!mesh || !material) return;

    glm::mat4 world = get_world_transform();
    glm::mat4 mvp = state.vp * world;

    material->bind();
    material->set("uModel", world);
    material->set("uMVP", mvp);

    if (state.camera) {
        material->set("uViewPos", state.camera->position);
    }

    mesh->vao->bind();
    glDrawElements(GL_TRIANGLES, mesh->ebo->num_indices, GL_UNSIGNED_INT, 0);
    mesh->vao->unbind();
}


// void RenderObject::draw_instanced