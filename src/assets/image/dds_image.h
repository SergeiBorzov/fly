#ifndef FLY_DDS_IMAGE_H
#define FLY_DDS_IMAGE_H

#include "core/string8.h"
#include "core/types.h"

namespace Fly
{

struct Image;
bool LoadDDSImage(String8 path, Image& image);

} // namespace Fly

#endif /* FLY_DDS_IMAGE_H */
