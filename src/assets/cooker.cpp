#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "image_bc.h"

enum class Mode
{
    Invalid,
    Compress,
    Transform
};

enum class CodecArg
{
    Invalid,
    BC1,
    BC3,
    BC4,
    BC5,
};

enum class TransformArg
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
        CodecArg codec;
        TransformArg transform;
    } modeArg;
};

TransformArg ParseTransform(const char* str)
{
    if (strcmp(str, "eq2cube"))
    {
        return TransformArg::Eq2Cube;
    }
    return TransformArg::Invalid;
}

CodecArg ParseCodec(const char* str)
{
    if (strcmp(str, "bc1") == 0)
    {
        return CodecArg::BC1;
    }
    if (strcmp(str, "bc3") == 0)
    {
        return CodecArg::BC3;
    }
    if (strcmp(str, "bc4") == 0)
    {
        return CodecArg::BC4;
    }
    if (strcmp(str, "bc5") == 0)
    {
        return CodecArg::BC5;
    }
    return CodecArg::Invalid;
}

bool IsOption(const char* str) { return str[0] == '-'; }

i32 ParseArray(int argc, char* argv[], i32 start, char* arr[])
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

void ParseCommandLine(int argc, char* argv[], Input& data)
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
            data.inputs =
                static_cast<char**>(malloc(sizeof(char*) * data.inputCount));
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
            data.outputs =
                static_cast<char**>(malloc(sizeof(char*) * data.outputCount));
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
            if (data.modeArg.codec == CodecArg::Invalid)
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
            if (data.modeArg.transform == TransformArg::Invalid)
            {
                fprintf(stderr, "Parse error: invalid codec type\n");
                exit(-4);
            }
        }
    }
}

void CheckSemantic(const Input& input)
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

void Shutdown(Input& input)
{
    if (input.inputs)
    {
        free(input.inputs);
    }

    if (input.outputs)
    {
        free(input.outputs);
    }
}

int main(int argc, char* argv[])
{
    Input input;
    ParseCommandLine(argc, argv, input);
    CheckSemantic(input);
    Shutdown(input);
    return 0;
}
