#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/filesystem.h"
#include "core/memory.h"

#include "image.h"
#include "image_bc.h"
#include "import_image.h"

using namespace Fly;

enum class Mode
{
    Invalid,
    Compress,
    Transform
};

enum class TransformType
{
    Invalid,
    Eq2Cube
};

struct Input
{
    char** inputs = nullptr;
    char** outputs = nullptr;
    u32 inputCount = 0;
    u32 outputCount = 0;
    Mode mode = Mode::Invalid;
    union
    {
        CodecType codec;
        TransformType transform;
    } modeArg;
};

static TransformType ParseTransform(const char* str)
{
    if (strcmp(str, "eq2cube"))
    {
        return TransformType::Eq2Cube;
    }
    return TransformType::Invalid;
}

static CodecType ParseCodec(const char* str)
{
    if (strcmp(str, "bc1") == 0)
    {
        return CodecType::BC1;
    }
    if (strcmp(str, "bc3") == 0)
    {
        return CodecType::BC3;
    }
    if (strcmp(str, "bc4") == 0)
    {
        return CodecType::BC4;
    }
    if (strcmp(str, "bc5") == 0)
    {
        return CodecType::BC5;
    }
    return CodecType::Invalid;
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
        else if (strcmp(argv[i], "-c") == 0)
        {
            data.mode = Mode::Compress;
            if (i + 1 >= argc)
            {
                fprintf(stderr, "Parse error: no codec type specified\n");
                exit(-3);
            }
            data.modeArg.codec = ParseCodec(argv[++i]);
            if (data.modeArg.codec == CodecType::Invalid)
            {
                fprintf(stderr, "Parse error: invalid codec type\n");
                exit(-3);
            }
        }
        else if (strcmp(argv[i], "-t") == 0)
        {
            data.mode = Mode::Transform;
            if (i + 1 >= argc)
            {
                fprintf(stderr, "Parse error: no codec type specified\n");
                exit(-4);
            }
            data.modeArg.transform = ParseTransform(argv[i++]);
            if (data.modeArg.transform == TransformType::Invalid)
            {
                fprintf(stderr, "Parse error: invalid codec type\n");
                exit(-4);
            }
        }
    }
}

static void CheckSemantic(const Input& input)
{
    if (input.mode == Mode::Invalid)
    {
        fprintf(stderr, "Semantic error: Mode is not specified\n");
        exit(-5);
    }

    if (input.inputCount == 0)
    {
        fprintf(stderr, "Semantic error: No input specified\n");
        exit(-6);
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
        outputs[i] = ReplaceExtension(input.inputs[i],
                                      CodecToExtension(input.modeArg.codec));
        if (!outputs[i])
        {
            fprintf(stderr, "Failed to autofill output path");
            exit(-9);
        }
    }
    input.outputs = outputs;
}

void Compress(Input& input)
{
    for (u32 i = 0; i < input.inputCount; i++)
    {
        Image image;
        u32 channelCount = GetCompressedImageChannelCount(input.modeArg.codec);
        if (!LoadImageFromFile(input.inputs[i], image, channelCount))
        {
            fprintf(stderr, "Compression error: failed to load image %s\n",
                    input.inputs[i]);
            exit(-7);
        }
        u64 size = 0;
        u8* data = CompressImage(image, input.modeArg.codec, false, size);

        String8 str(reinterpret_cast<const char*>(data), size);
        if (!WriteStringToFile(str, input.outputs[i]))
        {
            fprintf(stderr,
                    "Compression error: failed to write compressed image %s\n",
                    input.outputs[i]);
            exit(-9);
        }

        Fly::Free(data);
        FreeImage(image);
    }
}

int main(int argc, char* argv[])
{
    Input input;
    ParseCommandLine(argc, argv, input);
    CheckSemantic(input);
    FillOutputs(input);
    if (input.mode == Mode::Compress)
    {
        Compress(input);
    }
    Shutdown(input);
    return 0;
}
