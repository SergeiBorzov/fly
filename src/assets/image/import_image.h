#ifndef FLY_ASSETS_IMPORT_IMAGE_H
#define FLY_ASSETS_IMPORT_IMAGE_H

#include "core/string8.h"
#include "core/types.h"

namespace Fly
{

struct Path;
struct Image;

bool LoadImageFromFile(String8 path, Image& image, u8 desiredChannelCount = 4);

void FreeImage(Image& image);

} // namespace Fly

#endif /* FLY_ASSETS_IMPORT_IMAGE_H */
