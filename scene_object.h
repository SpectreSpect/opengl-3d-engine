#pragma once
#include "transformable.h"
#include "vector"


class SceneObject : public Transformable {
public:
    virtual ~SceneObject() = default;

    SceneObject* parent = nullptr;
    std::vector<SceneObject*> children;

    void add_child(SceneObject* child) {
        child->parent = this;
        children.push_back(child);
    }

    glm::mat4 get_world_transform() const { 
        if (parent)
            return parent->get_world_transform() * get_model_matrix();
        else
            return get_model_matrix();
    }
};
