#pragma once
#include "unordered_map"
#include "iostream"
#include "material_template.h"
#include "memory"


class MaterialManager {
public:
    std::unordered_map<std::string, std::unique_ptr<MaterialTemplate>> material_templates;

    MaterialTemplate* get(const std::string& name) {
        return material_templates.at(name).get();
    }

    MaterialTemplate* load_blinn_phong() {
        auto it = material_templates.find("blinn_phong");
        if (it != material_templates.end())
            return it->second.get();
        
        material_templates["blinn_phong"] = std::make_unique<MaterialTemplate>(
                                                            "../shaders/blinn_phong_vs.glsl",
                                                            "../shaders/blinn_phong_fs.glsl"
                                                        );

        auto* material_template = material_templates["blinn_phong"].get();
                                                        
        material_template->params.push_back({"uMVP", MaterialParamType::Mat4});
        material_template->params.push_back({"uModel", MaterialParamType::Mat4});
        material_template->params.push_back({"uViewPos", MaterialParamType::Vec3});

        return material_template;
    }
};