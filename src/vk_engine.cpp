#include "vk_engine.hpp"

#include <SDL.h>
#include <SDL_vulkan.h>
#include <VkBootstrap.h>

#include <cstdlib>
#include <iostream>

#include "vk_init.hpp"
#include "vk_types.hpp"

void vk_check(VkResult err)
{
  if (err) {
    std::cout << "Vulkan error: " << err << '\n';
    std::abort();
  }
}

void VulkanEngine::init_vulkan()
{
  vkb::InstanceBuilder builder;
  auto instance = builder.set_app_name("Example Vulkan Application")
                      .request_validation_layers(true)
                      .require_api_version(1, 1, 0)
                      .use_default_debug_messenger()
                      .build();
  auto vkb_instance = instance.value();
  m_instance        = vkb_instance.instance;
  m_debug_messenger = vkb_instance.debug_messenger;

  auto result = SDL_Vulkan_CreateSurface(m_window, m_instance, &m_surface);
  if (result == SDL_FALSE) {
    std::cerr << "failed create SDL Vulkan Surface, SDL Error: "
              << SDL_GetError() << '\n';
    std::abort();
  }

  vkb::PhysicalDeviceSelector selector{vkb_instance};
  vkb::PhysicalDevice physical_device = selector.set_minimum_version(1, 1)
                                            .set_surface(m_surface)
                                            .select()
                                            .value();
  vkb::DeviceBuilder device_builder{physical_device};
  vkb::Device vkb_device = device_builder.build().value();

  m_device         = vkb_device.device;
  m_chosen_gpu     = physical_device.physical_device;
  m_graphics_queue = vkb_device.get_queue(vkb::QueueType::graphics).value();
  m_graphics_queue_family =
      vkb_device.get_queue_index(vkb::QueueType::graphics).value();
}

void VulkanEngine::init_swapchain()
{
  vkb::SwapchainBuilder swapchain_builder{m_chosen_gpu, m_device, m_surface};
  vkb::Swapchain vkb_swapchain =
      swapchain_builder.use_default_format_selection()
          .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR) // vsync
          .set_desired_extent(m_window_extend.width, m_window_extend.height)
          .build()
          .value();

  m_swapchain              = vkb_swapchain.swapchain;
  m_swapchain_images       = vkb_swapchain.get_images().value();
  m_swapchain_image_views  = vkb_swapchain.get_image_views().value();
  m_swapchain_image_format = vkb_swapchain.image_format;
}

void VulkanEngine::init_commands()
{
  auto command_pool_info{vkinit::command_pool_create_info(
      m_graphics_queue_family, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)};
  vk_check(vkCreateCommandPool(m_device, &command_pool_info, nullptr,
                               &m_command_pool));

  auto cmd_alloc_info{vkinit::command_buffer_allocate_info(m_command_pool, 1)};
  vk_check(vkAllocateCommandBuffers(m_device, &cmd_alloc_info,
                                    &m_main_command_buffer));
}

void VulkanEngine::init()
{
  SDL_Init(SDL_INIT_VIDEO);
  auto window_flags = static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN);
  m_window          = SDL_CreateWindow("Vulkan Engine", SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED, m_window_extend.width,
                                       m_window_extend.height, window_flags);
  if (m_window == nullptr) {
    std::cerr << "failed to create SDL Vulkan Window, SDL Error: "
              << SDL_GetError() << '\n';
  }
  init_vulkan();
  init_swapchain();
  init_commands();
  m_is_initialized = true;
}

void VulkanEngine::draw()
{}

void VulkanEngine::run()
{
  SDL_Event e;
  bool run = true;
  while (run) {
    while (SDL_PollEvent(&e) != 0) {
      if (e.type == SDL_QUIT) {
        run = false;
      }
    }
    draw();
  }
}

void VulkanEngine::cleanup()
{
  if (m_is_initialized) {
    vkDestroyCommandPool(m_device, m_command_pool, nullptr);
    vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
    for (auto&& image_view : m_swapchain_image_views) {
      vkDestroyImageView(m_device, image_view, nullptr);
    }
    vkDestroyDevice(m_device, nullptr);
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    vkb::destroy_debug_utils_messenger(m_instance, m_debug_messenger);
    vkDestroyInstance(m_instance, nullptr);
    SDL_DestroyWindow(m_window);
  }
}
