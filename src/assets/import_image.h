#ifndef FLY_ASSETS_IMPORT_IMAGE_H
#define FLY_ASSETS_IMPORT_IMAGE_H

#include "core/types.h"

namespace Fly
{

struct Path;
struct Image;

bool LoadImageFromFile(const Fly::Path& path, Image& image,
                       u8 desiredChannelCount = 4);
bool LoadImageFromFile(const char* path, Image& image,
                       u8 desiredChannelCount = 4);
void FreeImage(Image& image);

} // namespace Fly

#endif /* FLY_ASSETS_IMPORT_IMAGE_H */
