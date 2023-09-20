#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "vk_types.hpp"
#include <vector>
#include <cinttypes>

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

  void init_vulkan();
  void init_swapchain();
  void init_commands();
  void init_default_renderpass();
  void init_framebuffers();
  void init_sync_structures();

 public:
  void init();
  void draw();
  void run();
  void cleanup();
};
#endif // ENGINE_HPP