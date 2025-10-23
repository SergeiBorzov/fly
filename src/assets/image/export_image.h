#ifndef FLY_ASSETS_EXPORT_IMAGE_H
#define FLY_ASSETS_EXPORT_IMAGE_H

#include "core/string8.h"
#include "core/types.h"

namespace Fly
{

struct Image;

bool ExportImage(String8 path, const Image& image);

} // namespace Fly

#endif /* FLY_ASSETS_EXPORT_IMAGE_H */
