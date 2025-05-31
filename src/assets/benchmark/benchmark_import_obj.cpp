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

    u64 flyStart = Fly::ClockNow();
    Fly::ObjData objData;
    Fly::ObjImportSettings settings;
    settings.scale = 0.01f;
    settings.uvOriginBottom = false;
    settings.flipFaceOrientation = true;
    if (!Fly::ImportWavefrontObj("sponza.obj", settings, objData))
    {
        fprintf(stderr, "Failed to load file. Benchmark is not valid.");
        return -1;
    }
    u64 flyEnd = Fly::ClockNow();

    const char* filename = "sponza.obj";
    u64 tinyobjloaderStart = Fly::ClockNow();
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
    u64 tinyobjloaderEnd = Fly::ClockNow();

    u64 flyTime = flyEnd - flyStart;
    u64 tinyobjloaderTime = tinyobjloaderEnd - tinyobjloaderStart;

    printf("FLY time is %f\n", Fly::ToSeconds(flyTime));
    printf("tinyobjloader time is %f\n", Fly::ToSeconds(tinyobjloaderTime));

    if (flyTime < tinyobjloaderTime)
    {
        f64 ratio = Fly::ToSeconds(tinyobjloaderTime) / Fly::ToSeconds(flyTime);
        printf("FLY is %f times faster than tinyobjloader\n", ratio);
    }
    else
    {
        f64 ratio = Fly::ToSeconds(tinyobjloaderTime) / Fly::ToSeconds(flyTime);
        printf("tinyobjloader is %f times faster than FLY\n", ratio);
    }

    ShutdownLogger();
    ReleaseThreadContext();
    return 0;
}
