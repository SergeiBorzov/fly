#ifndef HLS_ASSETS_IMPORT_OBJ_H
#define HLS_ASSETS_IMPORT_OBJ_H

#include "math/vec.h"
#include "core/filesystem.h"

namespace Hls
{

struct ObjImportSettings
{
    f32 scale = 1.0f;
    bool uvOriginBottom = true;
    bool flipFaceOrientation = false;
};

struct ObjData
{
    struct Material
    {
        char name[128] = {0};
        Math::Vec3 ambient;
        Math::Vec3 diffuse;
        Math::Vec3 specular;
        Math::Vec3 emission;
        Math::Vec3 transmittance;
        Math::Vec3 transmissionFilter;
        f32 shininess = 0.0f;
        f32 ior = 0.0f;
        f32 alpha = 1.0f;
        i32 illuminationModel = 2;

        i32 ambientTextureIndex = -1;
        i32 diffuseTextureIndex = -1;
        i32 specularTextureIndex = -1;
        i32 emissionTextureIndex = -1;
        i32 transmittanceTextureIndex = -1;
        i32 shininessTextureIndex = -1;
        i32 iorTextureIndex = -1;
        i32 alphaTextureIndex = -1;
        i32 bumpTextureIndex = -1;
    };

    struct Index
    {
        u32 vertexIndex = 0;
        u32 normalIndex = 0;
        u32 texCoordIndex = 0;
    };

    struct Face
    {
        Index indices[3];
        i32 materialIndex = -1;
    };

    struct Shape
    {
        char name[128] = {0};
        u32 firstFaceIndex = 0;
        u32 faceCount = 0;
    };

    Path objDirectoryPath;
    const char** textures = nullptr;
    Face* faces = nullptr;
    Math::Vec3* vertices = nullptr;
    Math::Vec3* normals = nullptr;
    Math::Vec2* texCoords = nullptr;
    Shape* shapes = nullptr;
    Material* materials = nullptr;

    u32 textureCount = 0;
    u32 vertexCount = 0;
    u32 normalCount = 0;
    u32 texCoordCount = 0;
    u32 faceCount = 0;
    u32 shapeCount = 0;
    u32 materialCount = 0;
    u32 textureCapacity = 0;
    u32 vertexCapacity = 0;
    u32 normalCapacity = 0;
    u32 texCoordCapacity = 0;
    u32 faceCapacity = 0;
    u32 shapeCapacity = 0;
    u32 materialCapacity = 0;

    i32 currentMaterialIndex = -1;
};
bool ImportWavefrontObj(const char* path, const ObjImportSettings& settings,
                        ObjData& objData);
bool ImportWavefrontObj(const Path& path, const ObjImportSettings& settings,
                        ObjData& objData);
void FreeWavefrontObj(ObjData& objData);

} // namespace Hls

#endif /* End of HLS_ASSETS_IMPORT_OBJ_H */
