#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/filesystem.h"
#include "core/memory.h"
#include "core/thread_context.h"

#include "rhi/context.h"
#include "rhi/pipeline.h"

#include "export_image.h"
#include "image.h"
#include "import_image.h"
#include "transform_image.h"

#include "glsl/eq2cube_frag_spv.inl"
#include "glsl/eq2cube_vert_spv.inl"

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
    String8* inputs = nullptr;
    String8* outputs = nullptr;
    u32 inputCount = 0;
    u32 outputCount = 0;
    i32 resizeX = 0;
    i32 resizeY = 0;
    bool resize = false;
    bool generateMips = false;
    bool eq2cube = false;
};

static bool IsOption(String8 str) { return str[0] == '-'; }

static i32 ParseArray(u32 argc, String8* argv, u32 start, String8* arr)
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

static void ParseCommandLine(Arena& arena, u32 argc, String8* argv, Input& data)
{
    for (u32 i = 0; i < argc; i++)
    {
        if (argv[i] == FLY_STRING8_LITERAL("-i"))
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
        else if (argv[i] == FLY_STRING8_LITERAL("-o"))
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
        else if (argv[i] == FLY_STRING8_LITERAL("-r"))
        {
            data.resize = true;

            if (!String8::ParseI32(argv[++i], data.resizeX))
            {
                fprintf(stderr, "Parse error: no size for resize specified\n");
                exit(-4);
            }

            if (!String8::ParseI32(argv[++i], data.resizeY))
            {
                fprintf(stderr, "Parse error: no size for resize specified\n");
                exit(-4);
            }
        }
        else if (argv[i] == FLY_STRING8_LITERAL("-m"))
        {
            data.generateMips = true;
        }
        else if (argv[i] == FLY_STRING8_LITERAL("-eq2cube"))
        {
            data.eq2cube = true;
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

    if (input.resize && (input.resizeX <= 0 || input.resizeY <= 0))
    {
        fprintf(stderr, "Semantic error: Invalid resize dimensions\n");
        exit(-6);
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
        String8List strList{};
        String8Node strNodes[2] = {};

        strList.PushExplicit(&strNodes[0], FLY_STRING8_LITERAL("out_"));
        strList.PushExplicit(&strNodes[1], input.inputs[i]);

        outputs[i] = strList.Join(arena);
        if (!outputs[i])
        {
            fprintf(stderr, "Failed to autofill output path");
            exit(-9);
        }
    }
    input.outputs = outputs;
}

static u8 GetImageChannelCount(String8 inputPath, String8 outputPath)
{
    if (String8::EndsWith(outputPath, FLY_STRING8_LITERAL(".fbc1")))
    {
        return 4;
    }
    else if (String8::EndsWith(outputPath, FLY_STRING8_LITERAL(".fbc3")))
    {
        return 4;
    }
    else if (String8::EndsWith(outputPath, FLY_STRING8_LITERAL(".fbc4")))
    {
        return 1;
    }
    else if (String8::EndsWith(outputPath, FLY_STRING8_LITERAL(".fbc5")))
    {
        return 2;
    }
    else if (String8::EndsWith(inputPath, FLY_STRING8_LITERAL(".jpg")))
    {
        return 4;
    }
    else if (String8::EndsWith(inputPath, FLY_STRING8_LITERAL(".jpeg")))
    {
        return 4;
    }

    return 0;
}

static bool CreateContext(RHI::Context& context)
{
    if (volkInitialize() != VK_SUCCESS)
    {
        return false;
    }

    RHI::ContextSettings settings{};
    settings.instanceExtensions = nullptr;
    settings.instanceExtensionCount = 0;
    settings.deviceExtensions = nullptr;
    settings.deviceExtensionCount = 0;
    settings.windowPtr = nullptr;
    settings.vulkan11Features.multiview = true;

    return RHI::CreateContext(settings, context);
}

static bool CreateEq2CubePipeline(RHI::Device& device,
                                  RHI::GraphicsPipeline& pipeline)
{
    RHI::GraphicsPipelineFixedStateStage fixedState{};
    fixedState.pipelineRendering.colorAttachments[0] = VK_FORMAT_R8G8B8A8_SRGB;
    fixedState.pipelineRendering.colorAttachmentCount = 1;
    fixedState.colorBlendState.attachmentCount = 1;
    fixedState.pipelineRendering.viewMask = 0x3F;

    RHI::Shader shaders[2] = {};
    const char* shaderSources[2] = {
        reinterpret_cast<const char*>(eq2cubeVertSpv),
        reinterpret_cast<const char*>(eq2cubeFragSpv),
    };
    u64 shaderSizes[2] = {EQ2CUBEVERTSPV_SIZE, EQ2CUBEFRAGSPV_SIZE};
    RHI::Shader::Type shaderTypes[2] = {RHI::Shader::Type::Vertex,
                                        RHI::Shader::Type::Fragment};

    for (u32 i = 0; i < STACK_ARRAY_COUNT(shaders); i++)
    {
        if (!RHI::CreateShader(device, shaderTypes[i], shaderSources[i],
                               shaderSizes[i], shaders[i]))
        {
            return false;
        }
    }

    if (!RHI::CreateGraphicsPipeline(device, fixedState, shaders,
                                     STACK_ARRAY_COUNT(shaders), pipeline))
    {
        return false;
    }

    for (u32 i = 0; i < STACK_ARRAY_COUNT(shaders); i++)
    {
        RHI::DestroyShader(device, shaders[i]);
    }

    return true;
}

static bool CompressOutputImage(String8 path, Image& image)
{
    if (String8::EndsWith(path, FLY_STRING8_LITERAL(".fbc1")) &&
        image.storageType != ImageStorageType::BC1)
    {
        return CompressImage(ImageStorageType::BC1, image);
    }
    else if (String8::EndsWith(path, FLY_STRING8_LITERAL(".fbc3")) &&
             image.storageType != ImageStorageType::BC3)
    {
        return CompressImage(ImageStorageType::BC3, image);
    }
    else if (String8::EndsWith(path, FLY_STRING8_LITERAL(".fbc4")) &&
             image.storageType != ImageStorageType::BC4)
    {
        return CompressImage(ImageStorageType::BC4, image);
    }
    else if (String8::EndsWith(path, FLY_STRING8_LITERAL(".fbc5")) &&
             image.storageType != ImageStorageType::BC5)
    {
        return CompressImage(ImageStorageType::BC5, image);
    }
    else if (String8::EndsWith(path, FLY_STRING8_LITERAL(".fbc6")) &&
             image.storageType != ImageStorageType::BC6)
    {
        return false;
    }
    else if (String8::EndsWith(path, FLY_STRING8_LITERAL(".fbc7")) &&
             image.storageType != ImageStorageType::BC7)
    {
        return false;
    }
    return true;
}

void ProcessInput(Input& input)
{
    RHI::Context context;
    RHI::Device* device = nullptr;
    RHI::GraphicsPipeline eq2cubePipeline;

    if (input.eq2cube)
    {
        if (!CreateContext(context))
        {
            fprintf(stderr, "Failed to create context\n");
            exit(-32);
        }

        device = &(context.devices[0]);
        if (!CreateEq2CubePipeline(*device, eq2cubePipeline))
        {
            fprintf(stderr, "Failed to create eq2cube pipeline \n");
            exit(-32);
        }
    }

    for (u32 i = 0; i < input.inputCount; i++)
    {
        Image image{};
        u32 channelCount =
            GetImageChannelCount(input.inputs[i], input.outputs[i]);
        if (channelCount == 0 && input.generateMips)
        {
            fprintf(stderr, "Transform error: Mipmaps are only allowed for "
                            "compressed bc formats\n");
            exit(-11);
        }

        if (!LoadImageFromFile(input.inputs[i], image, channelCount))
        {
            fprintf(stderr, "Import error: failed to load image %.*s\n",
                    static_cast<int>(input.inputs[i].Size()),
                    input.inputs[i].Data());
            exit(-7);
        }

        if (input.resize)
        {
            // TODO: Pass linear / srgb flag
            if (!ResizeImageSRGB(input.resizeX, input.resizeY, image))
            {
                Free(image.data);
                fprintf(stderr, "Transform error: Failed to resize %.*s\n",
                        static_cast<int>(input.inputs[i].Size()),
                        input.inputs[i].Data());
                exit(-10);
            }
        }

        if (input.eq2cube)
        {
            if (!Eq2Cube(*device, eq2cubePipeline, image))
            {
                Free(image.data);
                fprintf(stderr,
                        "Transform error: failed to transform "
                        "equirectangular to cube %.*s\n",
                        static_cast<int>(input.inputs[i].Size()),
                        input.inputs[i].Data());
                exit(-32);
            }
        }

        if (input.generateMips)
        {
            // TODO: Pass linear / srgb flag
            if (!GenerateMips(image, false))
            {
                fprintf(stderr,
                        "Transform error: Failed to generate mipmaps %.*s\n",
                        static_cast<int>(input.inputs[i].Size()),
                        input.inputs[i].Data());
                exit(-12);
            }
        }

        if (!CompressOutputImage(input.outputs[i], image))
        {
            fprintf(stderr, "Transform error: Failed to compress %.*s\n",
                    static_cast<int>(input.outputs[i].Size()),
                    input.outputs[i].Data());
            exit(-13);
        }

        if (!ExportImage(input.outputs[i], image))
        {
            fprintf(stderr, "Export error: failed to write image %.*s\n",
                    static_cast<int>(input.outputs[i].Size()),
                    input.outputs[i].Data());
            exit(-8);
        }

        Free(image.data);
    }

    if (input.eq2cube)
    {
        RHI::WaitDeviceIdle(*device);
        RHI::DestroyGraphicsPipeline(*device, eq2cubePipeline);
        RHI::DestroyContext(context);
    }
}

int main(int argc, char* argv[])
{
    InitArenas();

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
