#include "engine3d.h"
#include "vector"
#include "scene_object.h"

class Scene {
public:
    Engine3D* engine;
    std::vector<SceneObject*> objects;

    Scene(Engine3D* engine) : engine(engine) {}

    template<typename T>
    T* add_object(T* obj) {
        objects.push_back(obj);
        return obj;
    }
};
