#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "vk_types.hpp"

#include <cinttypes>
#include <filesystem>
#include <vector>

class VulkanEngine
{
  bool m_is_initialized{false};
  int m_frame_number{0};
  VkExtent2D m_window_extend{1280, 600};
  struct SDL_Window* m_window{nullptr};

  VkInstance m_instance;
  VkDebugUtilsMessengerEXT m_debug_messenger;
  VkPhysicalDevice m_chosen_gpu;
  VkDevice m_device;
  VkSurfaceKHR m_surface;
  VkSwapchainKHR m_swapchain;
  VkFormat m_swapchain_image_format;
  VkQueue m_graphics_queue;
  uint32_t m_graphics_queue_family;
  VkCommandPool m_command_pool;
  VkCommandBuffer m_main_command_buffer;

  std::vector<VkImage> m_swapchain_images;
  std::vector<VkImageView> m_swapchain_image_views;

  VkRenderPass m_render_pass;
  std::vector<VkFramebuffer> m_frame_buffers;

  VkFence m_render_fence;
  VkSemaphore m_present_semaphore;
  VkSemaphore m_render_semaphore;

  VkPipelineLayout m_triangle_pipeline_layout;
  VkPipeline m_triangle_pipeline;

  void init_vulkan();
  void init_swapchain();
  void init_commands();
  void init_default_renderpass();
  void init_framebuffers();
  void init_sync_structures();
  void init_pipelines();

 public:
  void init();
  void draw();
  void run();
  void cleanup();

  bool load_shader_module(std::filesystem::path const& file_path,
                          VkShaderModule* shader_module);
};

class PipelineBuilder
{
  std::vector<VkPipelineShaderStageCreateInfo> m_shader_stages;
  VkPipelineVertexInputStateCreateInfo m_vertex_input_info;
  VkPipelineInputAssemblyStateCreateInfo m_input_assembly;
  VkViewport m_viewport;
  VkRect2D m_scissor;
  VkPipelineRasterizationStateCreateInfo m_rasterizer;
  VkPipelineColorBlendAttachmentState m_color_blend_attachment;
  VkPipelineMultisampleStateCreateInfo m_multisampling;
  VkPipelineLayout m_pipeline_layout;

 public:
  VkPipeline build_pipeline(VkDevice device, VkRenderPass pass);
  void push_back(VkPipelineShaderStageCreateInfo&& shader_stage);
  void set_vertex_input_info(VkPipelineVertexInputStateCreateInfo const& info);
  void
  set_input_assembly_info(VkPipelineInputAssemblyStateCreateInfo const& info);
  void set_viewport(VkViewport const& viewport);
  void set_scissor(VkRect2D const& scissor);
  void set_rasterizer_info(VkPipelineRasterizationStateCreateInfo const& info);
  void set_color_blend_attachment_state(
      VkPipelineColorBlendAttachmentState const& state);
  void set_multisampling_info(VkPipelineMultisampleStateCreateInfo const& info);
  void set_pipeline_layout(VkPipelineLayout const& layout);
};

#endif // ENGINE_HPP