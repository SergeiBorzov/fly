#include <stdio.h>
#include <tiny_obj_loader.h>

#include "core/clock.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "assets/import_obj.h"

int main()
{
    InitThreadContext();
    Arena& arena = GetScratchArena();
    if (!InitLogger())
    {
        return -1;
    }

    u64 hlsStart = Hls::ClockNow();
    Hls::ObjData objData;
    Hls::ObjImportSettings settings;
    settings.scale = 0.01f;
    settings.uvOriginBottom = false;
    settings.flipFaceOrientation = true;
    if (!Hls::ImportWavefrontObj("sponza.obj", settings, objData))
    {
        fprintf(stderr, "Failed to load file. Benchmark is not valid.");
        return -1;
    }
    u64 hlsEnd = Hls::ClockNow();

    const char* filename = "sponza.obj";
    u64 tinyobjloaderStart = Hls::ClockNow();
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn;
    std::string err;
    bool ret =
        tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename);
    if (!ret)
    {
        fprintf(stderr, "Failed to load file. Benchmark is not valid");
        return -1;
    }
    u64 tinyobjloaderEnd = Hls::ClockNow();

    u64 hlsTime = hlsEnd - hlsStart;
    u64 tinyobjloaderTime = tinyobjloaderEnd - tinyobjloaderStart;

    printf("HLS time is %f\n", Hls::ToSeconds(hlsTime));
    printf("tinyobjloader time is %f\n", Hls::ToSeconds(tinyobjloaderTime));

    if (hlsTime < tinyobjloaderTime)
    {
        f64 ratio = Hls::ToSeconds(tinyobjloaderTime) / Hls::ToSeconds(hlsTime);
        printf("HLS is %f times faster than tinyobjloader\n", ratio);
    }
    else
    {
        f64 ratio = Hls::ToSeconds(tinyobjloaderTime) / Hls::ToSeconds(hlsTime);
        printf("tinyobjloader is %f times faster than HLS\n", ratio);
    }

    ShutdownLogger();
    ReleaseThreadContext();
    return 0;
}
