#ifndef HLS_RHI_SURFACE_H
#define HLS_RHI_SURFACE_H

#include "core/types.h"

namespace Hls
{
struct Context;
}

namespace Hls
{

bool CreateSurface(Context& context);
void DestroySurface(Context& context);
void GetWindowSize(Context& context, i32& width, i32& height);
void PollWindowEvents(Context& context);

} // namespace Hls

#endif /* HLS_RHI_SURFACE_H */
