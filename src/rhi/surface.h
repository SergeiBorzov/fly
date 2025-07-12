#ifndef FLY_RHI_SURFACE_H
#define FLY_RHI_SURFACE_H

#include "core/types.h"

namespace Fly
{
namespace RHI
{

struct Context;

bool CreateSurface(Context& context);
void DestroySurface(Context& context);
void GetFramebufferSize(Context& context, i32& width, i32& height);
void PollWindowEvents(Context& context);

} // namespace RHI
} // namespace Fly

#endif /* FLY_RHI_SURFACE_H */
