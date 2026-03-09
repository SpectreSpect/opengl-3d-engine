#pragma once
#include "cubemap.h"
#include "mesh.h"
#include "framebuffer.h"
#include "texture.h"
#include "cube.h"
#include "quad.h"
#include "pbr_environment.h"

class Engine3D;

class TextureManager {
public:
    TextureManager(Engine3D& engine, const std::string& root_path);
    void init_textures(Engine3D& engine, const std::string& root_path);
    Texture brdf_lut;
    PBREnvironment citrus_orchard_puresky_4k;
};