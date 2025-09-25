#ifndef FLY_ASSETS_EXPORT_IMAGE_H
#define FLY_ASSETS_EXPORT_IMAGE_H

#include "core/types.h"

namespace Fly
{

struct Image;

bool ExportImage(const char* path, const Image& image, bool generateMips);

} // namespace Fly

#endif /* FLY_ASSETS_EXPORT_IMAGE_H */
