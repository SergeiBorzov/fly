#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/filesystem.h"
#include "core/memory.h"

#include "export_image.h"
#include "image.h"
#include "import_image.h"

using namespace Fly;

enum class TransformType
{
    Invalid,
    None,
    Resize,
    Eq2Cube,
};

struct Input
{
    char** inputs = nullptr;
    char** outputs = nullptr;
    u32 inputCount = 0;
    u32 outputCount = 0;
    TransformType transform;
};

static TransformType ParseTransform(const char* str)
{
    if (strcmp(str, "eq2cube"))
    {
        return TransformType::Eq2Cube;
    }
    if (strcmp(str, "resize"))
    {
        return TransformType::Resize;
    }
    return TransformType::Invalid;
}

static bool IsOption(const char* str) { return str[0] == '-'; }

static i32 ParseArray(int argc, char* argv[], i32 start, char* arr[])
{
    i32 count = 0;
    for (i32 i = start; i < argc; i++)
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

static void ParseCommandLine(int argc, char* argv[], Input& data)
{
    for (i32 i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "-i") == 0)
        {
            data.inputCount = ParseArray(argc, argv, i + 1, nullptr);
            if (data.inputCount == 0)
            {
                fprintf(stderr, "Parse error: no input specified\n");
                exit(-1);
            }
            data.inputs = static_cast<char**>(
                Fly::Alloc(sizeof(char*) * data.inputCount));
            ParseArray(argc, argv, i + 1, data.inputs);
        }
        else if (strcmp(argv[i], "-o") == 0)
        {
            data.outputCount = ParseArray(argc, argv, i + 1, nullptr);
            if (data.outputCount == 0)
            {
                fprintf(stderr, "Parse error: no output specified\n");
                exit(-2);
            }
            data.outputs = static_cast<char**>(
                Fly::Alloc(sizeof(char*) * data.outputCount));
            ParseArray(argc, argv, i + 1, data.outputs);
        }
        else if (strcmp(argv[i], "-t") == 0)
        {
            if (i + 1 >= argc)
            {
                fprintf(stderr, "Parse error: no codec type specified\n");
                exit(-4);
            }
            data.transform = ParseTransform(argv[i++]);
            if (data.transform == TransformType::Invalid)
            {
                fprintf(stderr, "Parse error: invalid codec type\n");
                exit(-4);
            }
        }
    }
}

static void CheckSemantic(const Input& input)
{
    if (input.inputCount == 0)
    {
        fprintf(stderr, "Semantic error: No input specified\n");
        exit(-5);
    }

    if (input.outputCount > input.inputCount)
    {
        fprintf(stderr, "Semantic warning: Ignoring extra outputs\n");
    }
}

static void Shutdown(Input& input)
{
    for (u32 i = input.outputCount; i < input.inputCount; i++)
    {
        Fly::Free(input.outputs[i]);
    }

    if (input.inputs)
    {
        Fly::Free(input.inputs);
    }

    if (input.outputs)
    {
        Fly::Free(input.outputs);
    }
}

static void FillOutputs(Input& input)
{
    if (input.outputCount == input.inputCount)
    {
        return;
    }

    char** outputs =
        static_cast<char**>(Fly::Alloc(sizeof(char*) * input.inputCount));
    for (u32 i = 0; i < input.outputCount; i++)
    {
        outputs[i] = input.outputs[i];
    }

    for (u32 i = input.outputCount; i < input.inputCount; i++)
    {
        size_t len = strlen(input.inputs[i]);
        outputs[i] = static_cast<char*>(Fly::Alloc(sizeof(char) * (len + 5)));
        outputs[i][0] = '\0';
        strcat(outputs[i], "out_");
        strcat(outputs[i], input.inputs[i]);

        if (!outputs[i])
        {
            fprintf(stderr, "Failed to autofill output path");
            exit(-9);
        }
    }
    input.outputs = outputs;
}

static u8 GetCompressedImageChannelCount(const char* path)
{
    String8 pathStr = Fly::String8(path, strlen(path));

    if (pathStr.EndsWith(FLY_STRING8_LITERAL(".fbc1")))
    {
        return 4;
    }
    else if (pathStr.EndsWith(FLY_STRING8_LITERAL(".fbc3")))
    {
        return 4;
    }
    else if (pathStr.EndsWith(FLY_STRING8_LITERAL(".fbc4")))
    {
        return 1;
    }
    else if (pathStr.EndsWith(FLY_STRING8_LITERAL(".fbc5")))
    {
        return 2;
    }

    return 0;
}

void ProcessInput(Input& input)
{
    for (u32 i = 0; i < input.inputCount; i++)
    {
        Image image;
        u32 channelCount = GetCompressedImageChannelCount(input.outputs[i]);
        if (!LoadImageFromFile(input.inputs[i], image, channelCount))
        {
            fprintf(stderr, "Import error: failed to load image %s\n",
                    input.inputs[i]);
            exit(-7);
        }

        if (!ExportImage(input.outputs[i], image, false))
        {
            fprintf(stderr,
                    "Export error: failed to write compressed image %s\n",
                    input.outputs[i]);
            exit(-8);
        }

        FreeImage(image);
    }
}

int main(int argc, char* argv[])
{
    Input input;
    ParseCommandLine(argc, argv, input);
    CheckSemantic(input);
    FillOutputs(input);
    ProcessInput(input);
    Shutdown(input);
    return 0;
}
