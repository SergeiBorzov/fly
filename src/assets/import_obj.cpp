#include <string.h>

#include "core/filesystem.h"
#include "core/string8.h"
#include "core/thread_context.h"

#include "import_obj.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

namespace Hls
{

struct Vertex
{
    Math::Vec3 pos;
    Math::Vec3 norm;
    Math::Vec2 texCoord;
};

static char* AppendPathToBinaryDirectory(Arena& arena, const char* filename)
{
    u64 filenameStrLength = strlen(filename);

    const char* binDirectoryPath = GetBinaryDirectoryPath(arena);
    u64 binDirectoryPathStrLength = strlen(binDirectoryPath);

    char* buffer =
        HLS_ALLOC(arena, char, binDirectoryPathStrLength + filenameStrLength);

    strncpy(buffer, binDirectoryPath, binDirectoryPathStrLength);
    strncpy(buffer + binDirectoryPathStrLength, filename, filenameStrLength);
    buffer[binDirectoryPathStrLength + filenameStrLength] = '\0';
    return buffer;
}

static bool ParseTexCoord(String8 values, Math::Vec2& v)
{
    values = String8::TrimLeft(values);
    String8CutPair cutPair{};

    if (!String8::Cut(values, ' ', cutPair))
    {
        return false;
    }

    if (!String8::ParseF32(cutPair.head, v.x))
    {
        return false;
    }

    // Vertex Coord is usually 2 components, but might have optional 3rd, which
    // we ignore
    String8 secondComponent = cutPair.tail;

    char buff[256] = {0};
    strncpy(buff, cutPair.tail.Data(), cutPair.tail.Size());
    if (String8::Cut(cutPair.tail, ' ', cutPair))
    {
        secondComponent = cutPair.head;
    }
    if (!String8::ParseF32(secondComponent, v.y))
    {
        return false;
    }

    return true;
}

static bool ParseVertex(String8 values, Math::Vec3& v)
{
    values = String8::TrimLeft(values);
    String8CutPair cutPair{};
    if (!String8::Cut(values, ' ', cutPair))
    {
        return false;
    }

    if (!String8::ParseF32(cutPair.head, v.x))
    {
        return false;
    }

    if (!String8::Cut(cutPair.tail, ' ', cutPair))
    {
        return false;
    }

    if (!String8::ParseF32(cutPair.head, v.y))
    {
        return false;
    }

    String8::Cut(cutPair.tail, ' ', cutPair);

    // Vertex is usually 3 components, but might have optional 4th, which we
    // ignore
    String8 thirdComponent = cutPair.tail;
    if (String8::Cut(cutPair.tail, ' ', cutPair))
    {
        thirdComponent = cutPair.head;
    }
    if (!String8::ParseF32(thirdComponent, v.z))
    {
        return false;
    }

    return true;
}

static bool ParseNormal(String8 values, Math::Vec3& v)
{
    values = String8::TrimLeft(values);
    String8CutPair cutPair{};
    if (!String8::Cut(values, ' ', cutPair))
    {
        return false;
    }

    if (!String8::ParseF32(cutPair.head, v.x))
    {
        return false;
    }

    if (!String8::Cut(cutPair.tail, ' ', cutPair))
    {
        return false;
    }

    if (!String8::ParseF32(cutPair.head, v.y))
    {
        return false;
    }

    // Normal is strictly 3 components
    if (!String8::ParseF32(cutPair.tail, v.z))
    {
        return false;
    }

    return true;
}

static bool ParseIndex(String8 str, ObjData::Index& index)
{
    String8CutPair cutPair{};
    cutPair.tail = str;

    if (!String8::Cut(cutPair.tail, '/', cutPair))
    {
        return String8::ParseI64(cutPair.tail, index.vertexIndex);
    }

    if (!String8::ParseI64(cutPair.head, index.vertexIndex))
    {
        return false;
    }

    if (!String8::Cut(cutPair.tail, '/', cutPair))
    {
        return String8::ParseI64(cutPair.tail, index.texCoordIndex);
    }

    if (cutPair.head.Size() > 0 &&
        !String8::ParseI64(cutPair.head, index.texCoordIndex))
    {
        return false;
    }

    if (cutPair.tail.Size() > 0 &&
        !String8::ParseI64(cutPair.tail, index.normalIndex))
    {
        return false;
    }

    return true;
}

static bool ConvertIndex(u64 totalCount, i64& index)
{
    if (index > 0)
    {
        --index;
    }
    else if (index < 0)
    {
        index = totalCount + index;
        if (index < 0)
        {
            return false;
        }
    }

    return true;
}

static bool ParseFace(String8 values, ObjData::Face* faces, u64 vertexCount,
                      u64 texCoordCount, u64 normalCount, u32& triangleCount)
{
    values = String8::TrimLeft(values);

    ObjData::Face face{};
    u32 i = 0;
    String8CutPair cutPair{};
    cutPair.tail = values;
    // Simple triangulation if necessary using triangle fan algorithm
    while (String8::Cut(cutPair.tail, ' ', cutPair))
    {
        ObjData::Index index;
        if (!ParseIndex(cutPair.head, index))
        {
            return false;
        }

        // Cannot be zero, obj indexing starts with 1
        if (index.vertexIndex == 0)
        {
            return false;
        }
        if (!ConvertIndex(vertexCount, index.vertexIndex))
        {
            return false;
        }
        if (!ConvertIndex(texCoordCount, index.texCoordIndex))
        {
            return false;
        }
        if (!ConvertIndex(normalCount, index.normalIndex))
        {
            return false;
        }

        if (i < 2)
        {
            face.indices[i++] = index;
            continue;
        }
        else if (i == 2)
        {
            face.indices[i++] = index;
            faces[triangleCount] = face;
            triangleCount++;
        }
        else
        {
            face.indices[1] = face.indices[2];
            face.indices[2] = index;
            faces[triangleCount++] = face;
        }
    }

    ObjData::Index index;
    if (!ParseIndex(cutPair.tail, index))
    {
        return false;
    }

    // Cannot be zero, obj indexing starts with 1
    if (index.vertexIndex == 0)
    {
        return false;
    }
    if (!ConvertIndex(vertexCount, index.vertexIndex))
    {
        return false;
    }
    if (!ConvertIndex(texCoordCount, index.texCoordIndex))
    {
        return false;
    }
    if (!ConvertIndex(normalCount, index.normalIndex))
    {
        return false;
    }

    if (i == 2)
    {
        face.indices[i++] = index;
        faces[triangleCount++] = face;
    }
    else
    {
        face.indices[1] = face.indices[2];
        face.indices[2] = index;
        faces[triangleCount++] = face;
    }

    return true;
}

static bool ParseName(String8 str, ObjData::Shape& shape)
{
    str = String8::TrimLeft(str);
    if (!str.Size())
    {
        return true;
    }

    u32 count = static_cast<u32>(MIN(str.Size(), 127));
    strncpy(shape.name, str.Data(), count);
    shape.name[count] = '\0';
    return true;
}

// In first pass we count vertices and faces to know how much to allocate
static ObjParseResult FirstPass(String8 str, ObjData& objData)
{
    const String8 sVertexToken = HLS_STRING8_LITERAL("v");
    const String8 sNormalToken = HLS_STRING8_LITERAL("vn");
    const String8 sTexCoordToken = HLS_STRING8_LITERAL("vt");
    const String8 sFaceToken = HLS_STRING8_LITERAL("f");
    const String8 sGroupToken = HLS_STRING8_LITERAL("g");
    const String8 sObjectToken = HLS_STRING8_LITERAL("o");

    String8CutPair cutPair{};
    cutPair.tail = str;
    bool isPreviousObj = false;

    // TODO: If string is not ending with newline it will cause infinite loop
    u64 lineIndex = 0;
    while (cutPair.tail.Size())
    {
        String8::Cut(cutPair.tail, '\n', cutPair);
        String8 line = String8::TrimLeft(String8::TrimRight(cutPair.head));
        lineIndex++;

        String8CutPair lineCutPair{};
        if (!String8::Cut(line, ' ', lineCutPair))
        {
            continue;
        }

        if (lineCutPair.head == sVertexToken)
        {
            ++objData.vertexCount;
        }
        else if (lineCutPair.head == sNormalToken)
        {
            ++objData.normalCount;
        }
        else if (lineCutPair.head == sTexCoordToken)
        {
            ++objData.texCoordCount;
        }
        else if (lineCutPair.head == sFaceToken)
        {

            u32 indexCount = 1;
            while (String8::Cut(lineCutPair.tail, ' ', lineCutPair))
            {
                ++indexCount;
            }

            if (indexCount < 3)
            {
                return {lineIndex, ObjParseErrorType::FaceParseError};
            }

            objData.faceCount += (indexCount - 2);
        }
        else if (lineCutPair.head == sGroupToken)
        {
            objData.shapeCount += !isPreviousObj;
            isPreviousObj = false;
        }
        else if (lineCutPair.head == sObjectToken)
        {
            objData.shapeCount += 1;
            isPreviousObj = true;
        }
    }

    if (!objData.vertexCount)
    {
        return {0, ObjParseErrorType::NoVertexData};
    }

    if (!objData.faceCount)
    {
        return {0, ObjParseErrorType::NoFaceData};
    }

    return {0, ObjParseErrorType::Success};
}

static ObjParseResult SecondPass(Arena& arena, String8 str, ObjData& objData)
{
    objData.vertices = HLS_ALLOC(arena, Math::Vec3, objData.vertexCount);
    objData.faces = HLS_ALLOC(arena, ObjData::Face, objData.faceCount);

    if (objData.normalCount)
    {
        objData.normals = HLS_ALLOC(arena, Math::Vec3, objData.normalCount);
    }
    if (objData.texCoordCount)
    {
        objData.texCoords = HLS_ALLOC(arena, Math::Vec2, objData.texCoordCount);
    }
    if (!objData.shapeCount)
    {
        objData.shapes = HLS_ALLOC(arena, ObjData::Shape, 1);
    }
    else
    {
        objData.shapes = HLS_ALLOC(arena, ObjData::Shape, objData.shapeCount);
    }

    const String8 sVertexToken = HLS_STRING8_LITERAL("v");
    const String8 sNormalToken = HLS_STRING8_LITERAL("vn");
    const String8 sTexCoordToken = HLS_STRING8_LITERAL("vt");
    const String8 sFaceToken = HLS_STRING8_LITERAL("f");
    const String8 sObjectToken = HLS_STRING8_LITERAL("o");
    const String8 sGroupToken = HLS_STRING8_LITERAL("g");

    String8CutPair cutPair{};
    cutPair.tail = str;

    u64 lineIndex = 0;
    u64 vIndex = 0;
    u64 vnIndex = 0;
    u64 vtIndex = 0;
    u64 fIndex = 0;
    u64 sIndex = 0;
    u64 sStart = 0;
    bool isPreviousObj = false;
    // TODO: If string is not ending with newline it will cause infinite loop
    while (cutPair.tail.Size())
    {
        String8::Cut(cutPair.tail, '\n', cutPair);
        String8 line = String8::TrimLeft(String8::TrimRight(cutPair.head));
        lineIndex++;

        String8CutPair lineCutPair{};
        if (!String8::Cut(line, ' ', lineCutPair) && line.Size() != 0 &&
            line[0] != '#')
        {
            return {lineIndex, ObjParseErrorType::UnknownLineToken};
        }

        if (lineCutPair.head == sVertexToken)
        {
            if (!ParseVertex(lineCutPair.tail, objData.vertices[vIndex++]))
            {
                return {lineIndex, ObjParseErrorType::VertexParseError};
            }
        }
        else if (lineCutPair.head == sNormalToken)
        {
            if (!ParseNormal(lineCutPair.tail, objData.normals[vnIndex++]))
            {
                return {lineIndex, ObjParseErrorType::NormalParseError};
            }
        }
        else if (lineCutPair.head == sTexCoordToken)
        {
            if (!ParseTexCoord(lineCutPair.tail, objData.texCoords[vtIndex++]))
            {
                return {lineIndex, ObjParseErrorType::TexCoordParseError};
            }
        }
        else if (lineCutPair.head == sFaceToken)
        {
            u32 triangleCount = 0;

            if (!ParseFace(lineCutPair.tail, objData.faces + fIndex, vIndex,
                           vtIndex, vnIndex, triangleCount))
            {
                return {lineIndex, ObjParseErrorType::FaceParseError};
            }
            fIndex += triangleCount;
        }
        else if (lineCutPair.head == sObjectToken)
        {
            if (!ParseName(lineCutPair.tail, objData.shapes[sIndex]))
            {
                return {lineIndex, ObjParseErrorType::ObjectParseError};
            }

            if (sIndex >= 1)
            {
                objData.shapes[sIndex - 1].firstFaceIndex = sStart;
                objData.shapes[sIndex - 1].faceCount = fIndex - sStart;
            }
            sStart = fIndex;
            ++sIndex;
            isPreviousObj = true;
        }
        else if (lineCutPair.head == sGroupToken)
        {
            if (isPreviousObj)
            {
                isPreviousObj = false;
                continue;
            }

            if (!ParseName(lineCutPair.tail, objData.shapes[sIndex]))
            {
                return {lineIndex, ObjParseErrorType::GroupParseError};
            }

            if (sIndex >= 1)
            {
                objData.shapes[sIndex - 1].firstFaceIndex = sStart;
                objData.shapes[sIndex - 1].faceCount = fIndex - sStart;
            }
            sStart = fIndex;
            ++sIndex;
            isPreviousObj = false;
        }
    }

    objData.shapes[sIndex - 1].firstFaceIndex = sStart;
    objData.shapes[sIndex - 1].faceCount = fIndex - sStart;

    return {0, ObjParseErrorType::Success};
}

ObjParseResult ImportWavefrontObj(Arena& arena, const char* filename,
                                  ObjData& objData)
{
    Arena scratch = GetScratchArena(&arena);
    ArenaMarker marker = ArenaGetMarker(scratch);

    char* path = AppendPathToBinaryDirectory(scratch, filename);

    u64 strLength = 0;
    char* str = ReadFileToString(scratch, path, &strLength, 1, false);
    if (!str)
    {
        return {0, ObjParseErrorType::FileReadFailed};
    }

    String8 input = {str, strLength};

    ObjParseResult res = FirstPass(input, objData);
    if (res.error != ObjParseErrorType::Success)
    {
        return res;
    }

    res = SecondPass(arena, input, objData);
    if (res.error != ObjParseErrorType::Success)
    {
        return res;
    }

    ArenaPopToMarker(scratch, marker);
    return res;
}

} // namespace Hls
