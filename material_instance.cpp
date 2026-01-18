#include "material_instance.h"

MaterialInstance::MaterialInstance(MaterialTemplate* templ)
    : templ(templ) {}

void MaterialInstance::set(const std::string& name, MaterialValue value) {
    values[name] = value;
}

void MaterialInstance::bind() const {
    if (!templ || !templ->program)
        return;

    templ->bind();

    int texture_unit = 0;

    for (const auto& [name, value] : values) {
        std::visit([&](auto&& v) {
            using T = std::decay_t<decltype(v)>;

            // if constexpr (std::is_same_v<T, float>) {
            //     templ->program->set_float(name, v);
            // }
            if constexpr (std::is_same_v<T, glm::vec3>) {
                templ->program->set_vec3(name, v);
            }
            else if constexpr (std::is_same_v<T, glm::mat4>) {
                templ->program->set_mat4(name, v);
            }
            // else if constexpr (std::is_same_v<T, Texture*>) {
            //     if (!v) return;
            //     v->bind(texture_unit);
            //     templ->program->set_int(name, texture_unit);
            //     texture_unit++;
            // }
        }, value);
    }
}
