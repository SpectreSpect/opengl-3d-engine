
#include "scene_object.h"
#include "mesh.h"
#include "material_instance.h"

class RenderObject : public SceneObject {
public:
    Mesh* mesh = nullptr;
    MaterialInstance material;
};
