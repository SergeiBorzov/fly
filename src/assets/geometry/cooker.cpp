#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/memory.h"
#include "core/thread_context.h"
#include "geometry.h"

#include <meshoptimizer.h>

using namespace Fly;

struct Input
{
    String8* inputs = nullptr;
    String8* outputs = nullptr;
    u32 inputCount = 0;
    u32 outputCount = 0;
    f32 scale = 1.0f;
    bool flipForward = false;
    bool flipWindingOrder = false;
    CoordSystem coordSystem = CoordSystem::XYZ;
};

static bool IsOption(String8 str) { return str[0] == '-'; }

static u32 ParseArray(u32 argc, String8* argv, i32 start, String8* arr)
{
    u32 count = 0;
    for (u32 i = start; i < argc; i++)
    {
        if (IsOption(argv[i]))
        {
            break;
        }
        count++;
        if (arr)
        {
            arr[i - start] = argv[i];
        }
    }
    return count;
}

static bool ParseCoordSystem(String8 str, CoordSystem& coordSystem)
{
    if (str == FLY_STRING8_LITERAL("xyz"))
    {
        coordSystem = CoordSystem::XYZ;
        return true;
    }
    else if (str == FLY_STRING8_LITERAL("xzy"))
    {
        coordSystem = CoordSystem::XZY;
        return true;
    }
    else if (str == FLY_STRING8_LITERAL("yxz"))
    {
        coordSystem = CoordSystem::YXZ;
        return true;
    }
    else if (str == FLY_STRING8_LITERAL("yzx"))
    {
        coordSystem = CoordSystem::YZX;
        return true;
    }
    else if (str == FLY_STRING8_LITERAL("zxy"))
    {
        coordSystem = CoordSystem::ZXY;
        return true;
    }
    else if (str == FLY_STRING8_LITERAL("zyx"))
    {
        coordSystem = CoordSystem::ZYX;
        return true;
    }
    return false;
}

static void ParseCommandLine(Arena& arena, u32 argc, String8* argv, Input& data)
{
    for (u32 i = 0; i < argc; i++)
    {
        if (argv[i].StartsWith(FLY_STRING8_LITERAL("-i")))
        {
            data.inputCount = ParseArray(argc, argv, i + 1, nullptr);
            if (data.inputCount == 0)
            {
                fprintf(stderr, "Parse error: no input specified\n");
                exit(-1);
            }
            data.inputs = FLY_PUSH_ARENA(arena, String8, data.inputCount);
            ParseArray(argc, argv, i + 1, data.inputs);
        }
        else if (argv[i].StartsWith(FLY_STRING8_LITERAL("-o")))
        {
            data.outputCount = ParseArray(argc, argv, i + 1, nullptr);
            if (data.outputCount == 0)
            {
                fprintf(stderr, "Parse error: no output specified\n");
                exit(-2);
            }
            data.outputs = FLY_PUSH_ARENA(arena, String8, data.outputCount);
            ParseArray(argc, argv, i + 1, data.outputs);
        }
        else if (argv[i].StartsWith(FLY_STRING8_LITERAL("-s")))
        {
            if (!String8::ParseF32(argv[++i], data.scale))
            {
                fprintf(stderr, "Parse error: unable to parse scale float");
                exit(-3);
            }
        }
        else if (argv[i].StartsWith(FLY_STRING8_LITERAL("-c")))
        {
            if (!ParseCoordSystem(argv[++i], data.coordSystem))
            {
                fprintf(stderr, "Parse error: failed to parse coord system");
                exit(-4);
            }
        }
        else if (argv[i].StartsWith(FLY_STRING8_LITERAL("-ff")))
        {
            data.flipForward = true;
        }
        else if (argv[i].StartsWith(FLY_STRING8_LITERAL("-fw")))
        {
            data.flipWindingOrder = true;
        }
    }
}

static void CheckSemantic(const Input& input)
{
    if (input.inputCount == 0)
    {
        fprintf(stderr, "Semantic error: No input specified\n");
        exit(-3);
    }

    if (input.outputCount > input.inputCount)
    {
        fprintf(stderr, "Semantic warning: Ignoring extra outputs\n");
    }
}

static void FillOutputs(Arena& arena, Input& input)
{
    if (input.outputCount == input.inputCount)
    {
        return;
    }

    String8* outputs = FLY_PUSH_ARENA(arena, String8, input.inputCount);
    for (u32 i = 0; i < input.outputCount; i++)
    {
        outputs[i] = input.outputs[i];
    }

    for (u32 i = input.outputCount; i < input.inputCount; i++)
    {
        u64 len = input.inputs[i].Size();

        char* buffer = FLY_PUSH_ARENA(arena, char, len + 5);
        buffer[0] = '\0';
        strcat(buffer, "out_");
        strncat(buffer, input.inputs[i].Data(), input.inputs[i].Size());

        outputs[i] = String8(buffer, len + 5);

        if (!outputs[i])
        {
            fprintf(stderr, "Failed to autofill output path\n");
            exit(-4);
        }
    }
    input.outputs = outputs;
}

static void ProcessInput(Input& input)
{
    for (u32 i = 0; i < input.inputCount; i++)
    {
        Geometry geometry;
        if (!ImportGeometry(input.inputs[i], geometry))
        {
            fprintf(stderr, "Failed to import geometry %s\n",
                    input.inputs[i].Data());
            exit(-5);
        }

        if (input.scale != 1.0f || input.coordSystem != CoordSystem::XYZ ||
            input.flipForward)
        {
            TransformGeometry(input.scale, input.coordSystem, input.flipForward,
                              geometry);
        }

        if (input.flipWindingOrder)
        {
            FlipGeometryWindingOrder(geometry);
        }

        ReindexGeometry(geometry);
        OptimizeGeometryVertexCache(geometry);
        OptimizeGeometryOverdraw(geometry, 1.05f);

        if (!ExportGeometry(input.outputs[i], geometry))
        {
            fprintf(stderr, "Failed to export geometry %s\n",
                    input.outputs[i].Data());
            exit(-6);
        }
    }
}

int main(int argc, char* argv[])
{
    InitArenas();
    meshopt_setAllocator(Fly::Alloc, Fly::Free);

    Arena& arena = GetScratchArena();
    String8* argvStrings = FLY_PUSH_ARENA(arena, String8, argc);
    for (i32 i = 0; i < argc; i++)
    {
        argvStrings[i] = String8(argv[i], strlen(argv[i]));
    }

    Input input;
    ParseCommandLine(arena, argc, argvStrings, input);
    CheckSemantic(input);
    FillOutputs(arena, input);
    ProcessInput(input);

    ReleaseThreadContext();
    return 0;
}
