#ifndef HLS_RHI_PIPELINE_H
#define HLS_RHI_PIPELINE_H

#include <volk.h>

#include "core/types.h"

namespace Hls
{
    bool CreateShaderModule(VkDevice device, const char* spvSource, u64 codeSize, VkShaderModule& shaderModule);
    void DestroyShaderModule(VkDevice device, VkShaderModule shaderModule);
}

#endif /* HLS_RHI_PIPELINE_H */
