#include <stdio.h>
#include <string.h>

#include "core/clock.h"
#include "core/filesystem.h"
#include "core/log.h"
#include "core/memory.h"
#include "core/string8.h"
#include "core/thread_context.h"

#include "import_obj.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static const f64 sPowerPos[20] = {
    1.0e0,  1.0e1,  1.0e2,  1.0e3,  1.0e4,  1.0e5,  1.0e6,
    1.0e7,  1.0e8,  1.0e9,  1.0e10, 1.0e11, 1.0e12, 1.0e13,
    1.0e14, 1.0e15, 1.0e16, 1.0e17, 1.0e18, 1.0e19,
};

static const f64 sPowerNeg[20] = {
    1.0e0,   1.0e-1,  1.0e-2,  1.0e-3,  1.0e-4,  1.0e-5,  1.0e-6,
    1.0e-7,  1.0e-8,  1.0e-9,  1.0e-10, 1.0e-11, 1.0e-12, 1.0e-13,
    1.0e-14, 1.0e-15, 1.0e-16, 1.0e-17, 1.0e-18, 1.0e-19,
};

namespace Hls
{

static const char* SkipWhitespace(const char* ptr)
{
    while (Hls::CharIsWhitespace(*ptr))
        ptr++;

    return ptr;
}

static const char* SkipLine(const char* ptr)
{
    while (!CharIsNewline(*ptr++))
    {
    }
    return ptr;
}

static const char* ParseI32(const char* ptr, i32& val)
{
    i32 sign = 1;
    ptr = SkipWhitespace(ptr);

    if (*ptr == '+' || *ptr == '-')
    {
        sign = (*ptr == '+') * 2 - 1;
        ++ptr;
    }

    i32 num = 0;
    while (CharIsDigit(*ptr))
        num = 10 * num + (*ptr++ - '0');

    val = sign * num;
    return ptr;
}

static const char* ParseF32(const char* ptr, f32& val)
{
    f64 sign = 1.0;
    ptr = SkipWhitespace(ptr);

    if (*ptr == '+' || *ptr == '-')
    {
        sign = (*ptr == '+') * 2 - 1;
        ++ptr;
    }

    f64 num = 0.0;
    while (CharIsDigit(*ptr))
        num = 10.0 * num + static_cast<f64>(*ptr++ - '0');

    if (*ptr == '.')
        ptr++;

    f64 fraction = 0.0;
    f64 div = 1.0;

    while (CharIsDigit(*ptr))
    {
        fraction = 10.0 * fraction + (double)(*ptr++ - '0');
        div *= 10.0;
    }

    num += fraction / div;

    if (CharIsExponent(*ptr))
    {
        ptr++;

        const f64* powers = sPowerPos;
        u32 eval = 0;

        if (*ptr == '+')
        {
            ptr++;
        }
        else if (*ptr == '-')
        {
            powers = sPowerNeg;
            ptr++;
        }

        while (CharIsDigit(*ptr))
            eval = 10 * eval + (*ptr++ - '0');

        num *= (eval >= 20) ? 0.0 : powers[eval];
    }

    val = static_cast<f32>(sign * num);

    return ptr;
}

static Math::Vec3& GetNextVertex(ObjData& objData)
{
    if (objData.vertexCount >= objData.vertexCapacity)
    {
        objData.vertices = static_cast<Math::Vec3*>(Hls::Realloc(
            objData.vertices, sizeof(Math::Vec3) * objData.vertexCapacity * 2));
        objData.vertexCapacity *= 2;
    }
    return objData.vertices[objData.vertexCount++];
}

static Math::Vec3& GetNextNormal(ObjData& objData)
{
    if (objData.normalCount >= objData.normalCapacity)
    {
        objData.normals = static_cast<Math::Vec3*>(Hls::Realloc(
            objData.normals, sizeof(Math::Vec3) * objData.normalCapacity * 2));
        objData.normalCapacity *= 2;
    }
    return objData.normals[objData.normalCount++];
}

static Math::Vec2& GetNextTexCoord(ObjData& objData)
{
    if (objData.texCoordCount >= objData.texCoordCapacity)
    {
        objData.texCoords = static_cast<Math::Vec2*>(
            Hls::Realloc(objData.texCoords,
                         sizeof(Math::Vec2) * objData.texCoordCapacity * 2));
        objData.texCoordCapacity *= 2;
    }
    return objData.texCoords[objData.texCoordCount++];
}

static ObjData::Shape& GetNextShape(ObjData& objData)
{
    if (objData.shapeCount >= objData.shapeCapacity)
    {
        objData.shapes = static_cast<ObjData::Shape*>(
            Hls::Realloc(objData.shapes,
                         sizeof(ObjData::Shape) * objData.shapeCapacity * 2));
        objData.shapeCapacity *= 2;
    }
    return objData.shapes[objData.shapeCount++];
}

static ObjData::Face& GetNextFace(ObjData& objData)
{
    if (objData.faceCount >= objData.faceCapacity)
    {
        objData.faces = static_cast<ObjData::Face*>(Hls::Realloc(
            objData.faces, sizeof(ObjData::Face) * objData.faceCapacity * 2));
        objData.faceCapacity *= 2;
    }
    return objData.faces[objData.faceCount++];
}

static ObjData::Material& GetNextMaterial(ObjData& objData)
{
    if (objData.materialCount >= objData.materialCapacity)
    {
        objData.materials = static_cast<ObjData::Material*>(
            Hls::Realloc(objData.materials, sizeof(ObjData::Material) *
                                                objData.materialCapacity * 2));
        objData.materialCapacity *= 2;
    }
    return objData.materials[objData.materialCount++];
}

static const char*& GetNextTexture(ObjData& objData)
{
    if (objData.textureCount >= objData.textureCapacity)
    {
        objData.textures = static_cast<const char**>(
            Hls::Realloc(objData.textures,
                         sizeof(const char*) * objData.textureCapacity * 2));
        objData.textureCapacity *= 2;
    }
    return objData.textures[objData.textureCount++];
}

static const char* ParseVertex(const char* ptr, f32 scale, ObjData& objData)
{
    Math::Vec3& next = GetNextVertex(objData);
    for (u32 i = 0; i < 3; i++)
    {
        ptr = ParseF32(ptr, next[i]);
        next[i] *= scale;
    }
    return ptr;
}

static const char* ParseNormal(const char* ptr, ObjData& objData)
{
    Math::Vec3& next = GetNextNormal(objData);
    for (u32 i = 0; i < 3; i++)
    {
        ptr = ParseF32(ptr, next[i]);
    }
    return ptr;
}

static const char* ParseTexCoord(const char* ptr, bool uvOriginBottom,
                                 ObjData& objData)
{
    Math::Vec2& next = GetNextTexCoord(objData);
    for (u32 i = 0; i < 2; i++)
    {
        ptr = ParseF32(ptr, next[i]);
    }
    if (!uvOriginBottom)
    {
        next.y = 1.0f - next.y;
    }
    return ptr;
}

static const char* ParseFace(const char* ptr, bool flipFaceOrientation,
                             ObjData& objData)
{
    ptr = SkipWhitespace(ptr);

    i32 v = 0;
    i32 n = 0;
    i32 t = 0;

    u32 currentIndex = 0;
    u32 j = 0;
    ObjData::Index indices[3] = {};
    while (!CharIsNewline(*ptr))
    {
        ptr = ParseI32(ptr, v);
        if (*ptr == '/')
        {
            ptr++;
            if (*ptr != '/')
            {
                ptr = ParseI32(ptr, t);
            }

            if (*ptr == '/')
            {
                ptr++;
                ptr = ParseI32(ptr, n);
            }
        }

        if (v < 0)
        {
            indices[j].vertexIndex =
                static_cast<u32>(static_cast<i64>(objData.vertexCount) + v);
        }
        else if (v > 0)
        {
            indices[j].vertexIndex = static_cast<u32>(v) - 1;
        }

        if (t < 0)
        {
            indices[j].texCoordIndex =
                static_cast<u32>(static_cast<i64>(objData.texCoordCount) + t);
        }
        else if (t > 0)
        {
            indices[j].texCoordIndex = static_cast<u32>(t) - 1;
        }

        if (n < 0)
        {
            indices[j].normalIndex =
                static_cast<u32>(static_cast<i64>(objData.normalCount) + n);
        }
        else if (n > 0)
        {
            indices[j].normalIndex = static_cast<u32>(t) - 1;
        }

        ++currentIndex;
        if (currentIndex > 2)
        {
            ObjData::Face& face = GetNextFace(objData);
            memcpy(face.indices, indices, sizeof(indices));
            if (flipFaceOrientation)
            {
                ObjData::Index tmp = face.indices[1];
                face.indices[1] = face.indices[2];
                face.indices[2] = tmp;
            }
            face.materialIndex = objData.currentMaterialIndex;

            indices[1] = indices[2];
        }
        else
        {
            j++;
        }
    }

    return ptr;
}

static const char* ParseVec3(const char* ptr, Math::Vec3& v)
{
    ptr = SkipWhitespace(ptr);
    for (u32 i = 0; i < 3; i++)
    {
        ptr = ParseF32(ptr, v[i]);
    }
    return ptr;
}

static const char* ParseShapeName(const char* ptr, ObjData& objData)
{
    ptr = SkipWhitespace(ptr);

    const char* start = ptr;
    while (!CharIsNewline(*ptr))
    {
        ptr++;
    }

    while (ptr > start && CharIsWhitespace(*(ptr - 1)))
    {
        --ptr;
    }

    ObjData::Shape& shape = objData.shapes[objData.shapeCount - 1];
    u32 count = MIN(static_cast<u32>(ptr - start), 127u);
    strncpy(shape.name, start, count);
    shape.name[count] = '\0';
    return ptr;
}

static const char* ParseMaterialName(const char* ptr, ObjData& objData)
{
    ptr = SkipWhitespace(ptr);

    const char* start = ptr;
    while (!CharIsNewline(*ptr))
    {
        ptr++;
    }

    while (ptr > start && CharIsWhitespace(*(ptr - 1)))
    {
        --ptr;
    }

    ObjData::Material& material = GetNextMaterial(objData);
    material = {};
    u32 count = MIN(static_cast<u32>(ptr - start), 127u);
    strncpy(material.name, start, count);
    material.name[count] = '\0';
    return ptr;
}

static const char* ParseTexture(const char* ptr, ObjData& objData,
                                i32& textureHandle)
{
    ptr = SkipWhitespace(ptr);

    // TODO: options
    if (*ptr == '-')
        return ptr;

    const char* start = ptr;
    while (!CharIsNewline(*ptr))
    {
        ptr++;
    }

    while (ptr > start && CharIsWhitespace(*(ptr - 1)))
    {
        --ptr;
    }

    u32 count = static_cast<u32>(ptr - start);
    char* str = static_cast<char*>(Hls::Alloc(sizeof(char) * count + 1));
    strncpy(str, start, count);
    str[count] = '\0';

    i32 index = -1;
    for (u32 i = 0; i < objData.textureCount; i++)
    {
        if (strcmp(objData.textures[i], str) == 0)
        {
            index = i;
            break;
        }
    }

    if (index == -1)
    {
        index = objData.textureCount;
        const char*& texture = GetNextTexture(objData);
        texture = str;
    }
    else
    {
        Hls::Free(str);
    }

    textureHandle = index;

    return ptr;
}

static const char* ParseUseMtl(const char* ptr, ObjData& objData)
{
    ptr = SkipWhitespace(ptr);

    const char* start = ptr;
    while (!CharIsNewline(*ptr))
    {
        ptr++;
    }

    while (ptr > start && CharIsWhitespace(*(ptr - 1)))
    {
        --ptr;
    }

    char buff[128];
    u32 count = MIN(static_cast<u32>(ptr - start), 127u);
    strncpy(buff, start, count);
    buff[count] = '\0';

    i32 index = 0;
    for (u32 i = 0; i < objData.materialCount; i++)
    {
        if (strcmp(buff, objData.materials[i].name) == 0)
        {
            index = i;
            break;
        }
    }

    objData.currentMaterialIndex = index;

    return ptr;
}

static const char* ParseMtlLib(const char* ptr, ObjData& objData)
{
    Arena& scratch = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(scratch);

    ptr = SkipWhitespace(ptr);

    const char* start = ptr;
    while (!CharIsNewline(*ptr))
    {
        ptr++;
    }

    while (ptr > start && CharIsWhitespace(*(ptr - 1)))
    {
        --ptr;
    }

    String8 mtlFileName(start, ptr - start);
    String8 paths[2] = {objData.objDirectoryPath, mtlFileName};
    String8 str = ReadFileToString(scratch, AppendPaths(scratch, paths, 2));

    const char* p = str.Data();
    const char* mtlEnd = str.Data() + str.Size();

    bool isAlphaPresent = false;
    while (p != mtlEnd)
    {
        p = SkipWhitespace(p);
        switch (*p)
        {
            case 'n':
            {
                p++;
                if (p[0] == 'e' && p[1] == 'w' && p[2] == 'm' && p[3] == 't' &&
                    p[4] == 'l' && Hls::CharIsWhitespace(p[5]))
                {
                    p = ParseMaterialName(p + 5, objData);
                    isAlphaPresent = false;
                }
                break;
            }
            case 'K':
            {
                if (p[1] == 'a')
                {
                    p = ParseVec3(
                        p + 2,
                        objData.materials[objData.materialCount - 1].ambient);
                }
                else if (p[1] == 'd')
                {
                    p = ParseVec3(
                        p + 2,
                        objData.materials[objData.materialCount - 1].diffuse);
                }
                else if (p[1] == 's')
                {
                    p = ParseVec3(
                        p + 2,
                        objData.materials[objData.materialCount - 1].specular);
                }
                else if (p[1] == 'e')
                {
                    p = ParseVec3(
                        p + 2,
                        objData.materials[objData.materialCount - 1].emission);
                }
                else if (p[1] == 't')
                {
                    p = ParseVec3(p + 2,
                                  objData.materials[objData.materialCount - 1]
                                      .transmittance);
                }
                break;
            }
            case 'd':
            {
                if (CharIsWhitespace(p[1]))
                {
                    p = ParseF32(
                        p + 2,
                        objData.materials[objData.materialCount - 1].alpha);
                    isAlphaPresent = true;
                }
                break;
            }
            case 'T':
            {
                if (p[1] == 'r')
                {
                    if (!isAlphaPresent)
                    {
                        f32 tr = 0.0f;
                        p = ParseF32(p + 2, tr);
                        objData.materials[objData.materialCount - 1].alpha =
                            1.0f - tr;
                    }
                }
                else if (p[1] == 'f')
                {
                    p = ParseVec3(p + 2,
                                  objData.materials[objData.materialCount - 1]
                                      .transmissionFilter);
                }
                break;
            }
            case 'N':
            {
                if (p[1] == 's')
                {
                    p = ParseF32(
                        p + 2,
                        objData.materials[objData.materialCount - 1].shininess);
                }
                else if (p[1] == 'i')
                {
                    p = ParseF32(
                        p + 2,
                        objData.materials[objData.materialCount - 1].ior);
                }
                break;
            }
            case 'i':
            {
                p++;
                if (p[0] == 'l' && p[1] == 'l' && p[2] == 'u' && p[3] == 'm' &&
                    CharIsWhitespace(p[4]))
                {
                    p = ParseI32(p + 4,
                                 objData.materials[objData.materialCount - 1]
                                     .illuminationModel);
                }
                break;
            }
            case 'm':
            {
                p++;
                if (p[0] == 'a' && p[1] == 'p' && p[2] == '_')
                {
                    p += 3;
                    if (*p == 'K')
                    {
                        p++;
                        if (CharIsWhitespace(p[1]))
                        {
                            if (*p == 'a')
                            {
                                p = ParseTexture(
                                    p + 1, objData,
                                    objData.materials[objData.materialCount - 1]
                                        .ambientTextureIndex);
                            }
                            else if (*p == 'd')
                            {
                                p = ParseTexture(
                                    p + 1, objData,
                                    objData.materials[objData.materialCount - 1]
                                        .diffuseTextureIndex);
                            }
                            else if (*p == 's')
                            {
                                p = ParseTexture(
                                    p + 1, objData,
                                    objData.materials[objData.materialCount - 1]
                                        .specularTextureIndex);
                            }
                            else if (*p == 'e')
                            {
                                p = ParseTexture(
                                    p + 1, objData,
                                    objData.materials[objData.materialCount - 1]
                                        .emissionTextureIndex);
                            }
                            else if (*p == 't')
                            {
                                p = ParseTexture(
                                    p + 1, objData,
                                    objData.materials[objData.materialCount - 1]
                                        .transmittanceTextureIndex);
                            }
                        }
                    }
                    else if (*p == 'N')
                    {
                        p++;
                        if (CharIsWhitespace(p[1]))
                        {
                            if (*p == 's')
                            {
                                p = ParseTexture(
                                    p + 1, objData,
                                    objData.materials[objData.materialCount - 1]
                                        .shininessTextureIndex);
                            }
                            else if (*p == 'i')
                            {
                                p = ParseTexture(
                                    p + 1, objData,
                                    objData.materials[objData.materialCount - 1]
                                        .iorTextureIndex);
                            }
                        }
                    }
                    else if (*p == 'd')
                    {
                        p = ParseTexture(
                            p + 1, objData,
                            objData.materials[objData.materialCount - 1]
                                .alphaTextureIndex);
                    }
                    else if ((p[0] == 'b' || p[0] == 'B') && p[1] == 'u' &&
                             p[2] == 'm' && p[3] == 'p' &&
                             CharIsWhitespace(p[4]))
                    {
                        p = ParseTexture(
                            p + 4, objData,
                            objData.materials[objData.materialCount - 1]
                                .bumpTextureIndex);
                    }
                }
                break;
            }
        }
        p = SkipLine(p);
    }

    ArenaPopToMarker(scratch, marker);

    return ptr;
}

bool ParseObj(String8 str, const ObjImportSettings& settings, ObjData& objData)
{
    objData.vertexCount = 0;
    objData.vertexCapacity = 131136;
    objData.vertices = static_cast<Math::Vec3*>(
        Hls::Alloc(sizeof(Math::Vec3) * objData.vertexCapacity));

    objData.normalCount = 0;
    objData.normalCapacity = 131136;
    objData.normals = static_cast<Math::Vec3*>(
        Hls::Alloc(sizeof(Math::Vec3) * objData.normalCapacity));

    objData.texCoordCount = 0;
    objData.texCoordCapacity = 131136;
    objData.texCoords = static_cast<Math::Vec2*>(
        Hls::Alloc(sizeof(Math::Vec2) * objData.texCoordCapacity));

    objData.faceCount = 0;
    objData.faceCapacity = 131136;
    objData.faces = static_cast<ObjData::Face*>(
        Hls::Alloc(sizeof(ObjData::Face) * objData.faceCapacity));

    objData.shapeCount = 0;
    objData.shapeCapacity = 256;
    objData.shapes = static_cast<ObjData::Shape*>(
        Hls::Alloc(sizeof(ObjData::Shape) * objData.shapeCapacity));

    objData.materialCount = 0;
    objData.materialCapacity = 64;
    objData.materials = static_cast<ObjData::Material*>(
        Hls::Alloc(sizeof(ObjData::Material) * objData.materialCapacity));

    objData.textureCount = 0;
    objData.textureCapacity = 64;
    objData.textures = static_cast<const char**>(
        Hls::Alloc(sizeof(const char*) * objData.textureCapacity));

    const char* p = str.Data();
    const char* end = str.Data() + str.Size();

    u32 shapeStartIndex = 0;
    while (p != end)
    {
        p = SkipWhitespace(p);
        switch (*p)
        {
            case '#':
            {
                break;
            }
            case 'v':
            {
                p++;
                switch (*p++)
                {
                    case ' ':
                    case '\t':
                    {
                        p = ParseVertex(p, settings.scale, objData);
                        break;
                    }
                    case 't':
                    {
                        p = ParseTexCoord(p, settings.uvOriginBottom, objData);
                        break;
                    }
                    case 'n':
                    {
                        p = ParseNormal(p, objData);
                        break;
                    }
                    default:
                    {
                        return false;
                    }
                }
                break;
            }
            case 'f':
            {
                p++;
                switch (*p++)
                {
                    case ' ':
                    case '\t':
                    {
                        p = ParseFace(p, settings.flipFaceOrientation, objData);
                        break;
                    }
                    default:
                    {
                        return false;
                    }
                }
                break;
            }
            case 'g':
            {
                p++;
                if (objData.shapeCount == 0)
                {
                    ObjData::Shape& shape = GetNextShape(objData);
                    shape.name[0] = '\0';
                    shape.firstFaceIndex = objData.faceCount;
                }
                else
                {
                    ObjData::Shape& shape =
                        objData.shapes[objData.shapeCount - 1];
                    if (shape.firstFaceIndex != objData.faceCount)
                    {
                        shape.faceCount =
                            objData.faceCount - shape.firstFaceIndex;
                        ObjData::Shape& newShape = GetNextShape(objData);
                        newShape.name[0] = '\0';
                        newShape.firstFaceIndex = objData.faceCount;
                    }
                }
                switch (*p++)
                {
                    case ' ':
                    case '\t':
                    {
                        p = ParseShapeName(p, objData);
                        break;
                    }
                    case '\n':
                    {
                        --p;
                        break;
                    }
                    default:
                    {
                        return false;
                    }
                }
                break;
            }
            case 'o':
            {
                if (objData.shapeCount - 1 >= 0)
                {
                    ObjData::Shape& shape =
                        objData.shapes[objData.shapeCount - 1];
                    shape.faceCount = objData.faceCount - shape.firstFaceIndex;
                }
                ObjData::Shape& shape = GetNextShape(objData);
                shape.name[0] = '\0';
                shape.firstFaceIndex = objData.faceCount;
                break;
            }
            case 'm':
            {
                p++;
                if (p[0] == 't' && p[1] == 'l' && p[2] == 'l' && p[3] == 'i' &&
                    p[4] == 'b' && Hls::CharIsWhitespace(p[5]))
                {
                    p = ParseMtlLib(p + 5, objData);
                }
                break;
            }
            case 'u':
            {
                p++;
                if (p[0] == 's' && p[1] == 'e' && p[2] == 'm' && p[3] == 't' &&
                    p[4] == 'l' && CharIsWhitespace(p[5]))
                {
                    p = ParseUseMtl(p + 5, objData);
                }
                break;
            }
        }
        p = SkipLine(p);
    }

    ObjData::Shape* lastShape = nullptr;
    if (objData.shapeCount == 0)
    {
        lastShape = &(GetNextShape(objData));
        lastShape->firstFaceIndex = 0;
        lastShape->name[0] = '\0';
    }
    lastShape = &(objData.shapes[objData.shapeCount - 1]);
    lastShape->faceCount = objData.faceCount - lastShape->firstFaceIndex;

    return true;
}

bool ImportWavefrontObj(String8 filepath, const ObjImportSettings& settings,
                        ObjData& objData)
{
    Arena& scratch = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(scratch);

    objData.objDirectoryPath = GetParentDirectoryPath(filepath);
    String8 str = ReadFileToString(scratch, filepath);
    if (!str)
    {
        return false;
    }

    bool res = ParseObj(str, settings, objData);

    ArenaPopToMarker(scratch, marker);
    return res;
}

void FreeWavefrontObj(ObjData& objData)
{
    for (u32 i = 0; i < objData.textureCount; i++)
    {
        Hls::Free((void*)objData.textures[i]);
    }
    Hls::Free(objData.textures);
    Hls::Free(objData.vertices);
    Hls::Free(objData.normals);
    Hls::Free(objData.texCoords);
    Hls::Free(objData.faces);
    Hls::Free(objData.shapes);
}

} // namespace Hls
