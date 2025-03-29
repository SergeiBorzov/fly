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
    u64 line = 0;
    ObjParseErrorType error = ObjParseErrorType::Success;
};

struct ObjData
{
    struct Index
    {
        i64 vertexIndex = 0;
        i64 normalIndex = 0;
        i64 texCoordIndex = 0;
    };

    struct Face
    {
        Index indices[3];
    };

    struct Shape
    {
        char name[128] = {0};
        u64 firstFaceIndex = 0;
        u64 faceCount = 0;
    };

    Face* faces = nullptr;
    Math::Vec3* vertices = nullptr;
    Math::Vec3* normals = nullptr;
    Math::Vec2* texCoords = nullptr;
    Shape* shapes = nullptr;

    u64 vertexCount = 0;
    u64 normalCount = 0;
    u64 texCoordCount = 0;
    u64 faceCount = 0;
    u64 shapeCount = 0;
};
ObjParseResult ImportWavefrontObj(Arena& arena, const char* filename,
                                  ObjData& objData);

} // namespace Hls

#endif /* End of HLS_ASSETS_IMPORT_OBJ_H */
