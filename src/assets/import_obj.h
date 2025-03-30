#ifndef HLS_ASSETS_IMPORT_OBJ_H
#define HLS_ASSETS_IMPORT_OBJ_H

#include "math/vec.h"

namespace Hls
{

enum class ObjParseErrorType
{
    Success = 0,
    FileReadFailed,
    NoVertexData,
    NoFaceData,
    UnknownLineToken,
    FaceParseError,
    VertexParseError,
    NormalParseError,
    TexCoordParseError,
    GroupParseError,
    ObjectParseError
};

struct ObjParseResult
{
    u32 line = 0;
    ObjParseErrorType error = ObjParseErrorType::Success;
};

struct ObjData
{
    struct Index
    {
        u32 vertexIndex = 0;
        u32 normalIndex = 0;
        u32 texCoordIndex = 0;
    };

    struct Face
    {
        Index indices[3];
    };

    struct Shape
    {
        char name[128] = {0};
        u32 firstFaceIndex = 0;
        u32 faceCount = 0;
    };

    Face* faces = nullptr;
    Math::Vec3* vertices = nullptr;
    Math::Vec3* normals = nullptr;
    Math::Vec2* texCoords = nullptr;
    Shape* shapes = nullptr;

    u32 vertexCount = 0;
    u32 normalCount = 0;
    u32 texCoordCount = 0;
    u32 faceCount = 0;
    u32 shapeCount = 0;
    u32 vertexCapacity = 0;
    u32 normalCapacity = 0;
    u32 texCoordCapacity = 0;
    u32 faceCapacity = 0;
    u32 shapeCapacity = 0;
    u32 vertexCommitSize = 0;
    u32 normalCommitSize = 0;
    u32 texCoordCommitSize = 0;
    u32 faceCommitSize = 0;
    u32 shapeCommitSize = 0;
};
bool ImportWavefrontObj(const char* filename, ObjData& objData);
void FreeWavefrontObj(ObjData& objData);

} // namespace Hls

#endif /* End of HLS_ASSETS_IMPORT_OBJ_H */
