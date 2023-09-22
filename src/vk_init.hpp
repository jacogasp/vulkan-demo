#ifndef VK_INITIALIZERS_HPP
#define VK_INITIALIZERS_HPP

#include "vk_types.hpp"

#include <cinttypes>

namespace vkinit {

VkCommandPoolCreateInfo
command_pool_create_info(uint32_t queue_family_index,
                         VkCommandPoolCreateFlags flags = 0);

VkCommandBufferAllocateInfo command_buffer_allocate_info(
    VkCommandPool pool, uint32_t count = 1,
    VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

VkPipelineShaderStageCreateInfo
pipeline_shader_stage_create_info(VkShaderStageFlagBits stage,
                                  VkShaderModule shader_module);

VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info();

VkPipelineInputAssemblyStateCreateInfo
init_assembly_create_info(VkPrimitiveTopology topology);

VkPipelineRasterizationStateCreateInfo
rasterization_state_create_info(VkPolygonMode polygon_mode);

VkPipelineMultisampleStateCreateInfo multisampling_state_create_info();

VkPipelineColorBlendAttachmentState color_blench_attachment_state();

VkPipelineLayoutCreateInfo pipeline_layout_create_info();

} // namespace vkinit

#endif // VK_INITIALIZERS_HPP