#include "core/log.h"

#include "frame_graph.h"

namespace Fly
{
namespace RHI
{

bool FrameGraph::Build(Arena& arena)
{
    PassNode* curr = head_;

    while (curr)
    {
        FLY_LOG("Pass name %s", curr->name);
        curr = curr->next;
    }
    return true;
}

} // namespace RHI
} // namespace Fly
