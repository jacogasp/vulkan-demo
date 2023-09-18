#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "vk_types.hpp"
#include <vector>

class VulkanEngine {
  bool m_is_initialized { false };
  int m_frame_number { 0 };
  VkExtent2D m_window_extend { 1280, 600 };
  struct SDL_Window* m_window { nullptr };

  VkInstance m_instance;
  VkDebugUtilsMessengerEXT m_debug_messenger;
  VkPhysicalDevice m_chosen_gpu;
  VkDevice m_device;
  VkSurfaceKHR m_surface;
  VkSwapchainKHR m_swapchain;
  VkFormat m_swapchain_image_format;

  std::vector<VkImage> m_swapchain_images;
  std::vector<VkImageView> m_swapchain_image_views;

  void init_vulkan();
  void init_swapchain();

  public:
  void init();
  void draw();
  void run();
  void cleanup();
};
#endif // ENGINE_HPP