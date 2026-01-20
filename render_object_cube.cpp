#include "render_object_cube.h"

RenderObjectCube::RenderObjectCube(Engine3D* engine3d) {
    material = new MaterialInstance(engine3d->material_manager->get("blinn_phong"));
    mesh = engine3d->mesh_manager->get("cube");
}