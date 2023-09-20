#include "vk_engine.hpp"

#include <SDL.h>
#include <SDL_vulkan.h>
#include <VkBootstrap.h>

#include <algorithm>
#include <cmath>
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
      m_graphics_queue_family,
      VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)};
  vk_check(vkCreateCommandPool(m_device, &command_pool_info, nullptr,
                               &m_command_pool));

  auto cmd_alloc_info{vkinit::command_buffer_allocate_info(m_command_pool, 1)};
  vk_check(vkAllocateCommandBuffers(m_device, &cmd_alloc_info,
                                    &m_main_command_buffer));
}

void VulkanEngine::init_default_renderpass()
{
  VkAttachmentDescription color_attachment{};
  color_attachment.format         = m_swapchain_image_format;
  color_attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
  color_attachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color_attachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
  color_attachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference color_attachment_ref{};
  color_attachment_ref.attachment = 0;
  color_attachment_ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments    = &color_attachment_ref;

  VkRenderPassCreateInfo render_pass_info{};
  render_pass_info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_info.attachmentCount = 1;
  render_pass_info.pAttachments    = &color_attachment;
  render_pass_info.subpassCount    = 1;
  render_pass_info.pSubpasses      = &subpass;
  vk_check(
      vkCreateRenderPass(m_device, &render_pass_info, nullptr, &m_render_pass));
}

void VulkanEngine::init_framebuffers()
{
  VkFramebufferCreateInfo fb_info{};
  fb_info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  fb_info.pNext           = nullptr;
  fb_info.renderPass      = m_render_pass;
  fb_info.attachmentCount = 1;
  fb_info.width           = m_window_extend.width;
  fb_info.height          = m_window_extend.height;
  fb_info.layers          = 1;

  const uint32_t swapchain_image_count = m_swapchain_images.size();
  m_frame_buffers = std::vector<VkFramebuffer>(swapchain_image_count);
  for (auto i = 0; i < swapchain_image_count; ++i) {
    fb_info.pAttachments = &m_swapchain_image_views[i];
    auto result =
        vkCreateFramebuffer(m_device, &fb_info, nullptr, &m_frame_buffers[i]);
    vk_check(result);
  }
}

void VulkanEngine::init_sync_structures()
{
  VkFenceCreateInfo fence_create_info{};
  fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_create_info.pNext = nullptr;
  fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  vk_check(
      vkCreateFence(m_device, &fence_create_info, nullptr, &m_render_fence));

  VkSemaphoreCreateInfo semaphore_create_info{};
  semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semaphore_create_info.pNext = nullptr;
  semaphore_create_info.flags = 0;
  vk_check(vkCreateSemaphore(m_device, &semaphore_create_info, nullptr,
                             &m_present_semaphore));
  vk_check(vkCreateSemaphore(m_device, &semaphore_create_info, nullptr,
                             &m_render_semaphore));
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
  init_default_renderpass();
  init_framebuffers();
  init_sync_structures();
  m_is_initialized = true;
}

void VulkanEngine::draw()
{
  // Wait until the GPU has finished, with a 1s timeout and reset the fence
  vk_check(vkWaitForFences(m_device, 1, &m_render_fence, true, 1'000'000'000));
  vk_check(vkResetFences(m_device, 1, &m_render_fence));
  // Request the image from the swapchain with a 1s timeout
  std::uint32_t swapchain_image_index;
  vk_check(vkAcquireNextImageKHR(m_device, m_swapchain, 1'000'000'000,
                                 m_present_semaphore, nullptr,
                                 &swapchain_image_index));
  // Reset the command buffer
  vk_check(vkResetCommandBuffer(m_main_command_buffer, 0));
  // Begin the command buffer recording. We'll use the buffer exactly once
  VkCommandBufferBeginInfo cb_info{};
  cb_info.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  cb_info.pNext            = nullptr;
  cb_info.pInheritanceInfo = nullptr;
  cb_info.flags            = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
  vk_check(vkBeginCommandBuffer(m_main_command_buffer, &cb_info));
  // Make some color
  VkClearValue clear_value;
  float flash       = std::abs(std::sin(m_frame_number / 120.f));
  clear_value.color = {{0.0f, 0.0f, flash, 1.0f}};
  // Start the main render pass
  VkRenderPassBeginInfo rp_info{};
  rp_info.sType               = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  rp_info.pNext               = nullptr;
  rp_info.renderPass          = m_render_pass;
  rp_info.renderArea.offset.x = 0;
  rp_info.renderArea.offset.y = 0;
  rp_info.renderArea.extent   = m_window_extend;
  rp_info.framebuffer         = m_frame_buffers[swapchain_image_index];
  rp_info.clearValueCount     = 1;
  rp_info.pClearValues        = &clear_value;
  vkCmdBeginRenderPass(m_main_command_buffer, &rp_info,
                       VK_SUBPASS_CONTENTS_INLINE);
  // Render stuff
  // End the main render pass and the command buffer;
  vkCmdEndRenderPass(m_main_command_buffer);
  vk_check(vkEndCommandBuffer(m_main_command_buffer));
  // Submit the command buffer to the command queue
  VkPipelineStageFlags wait_stage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submit_info{};
  submit_info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.pNext                = nullptr;
  submit_info.pWaitDstStageMask    = &wait_stage;
  submit_info.waitSemaphoreCount   = 1;
  submit_info.pWaitSemaphores      = &m_present_semaphore;
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores    = &m_render_semaphore;
  submit_info.commandBufferCount   = 1;
  submit_info.pCommandBuffers      = &m_main_command_buffer;
  vk_check(vkQueueSubmit(m_graphics_queue, 1, &submit_info, m_render_fence));
  // Display the image to the screen
  VkPresentInfoKHR present_info{};
  present_info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.pNext              = nullptr;
  present_info.swapchainCount     = 1;
  present_info.pSwapchains        = &m_swapchain;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores    = &m_render_semaphore;
  present_info.pImageIndices      = &swapchain_image_index;
  vk_check(vkQueuePresentKHR(m_graphics_queue, &present_info));
  ++m_frame_number;
}

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
    vkDestroySemaphore(m_device, m_render_semaphore, nullptr);
    vkDestroySemaphore(m_device, m_present_semaphore, nullptr);
    vkDestroyFence(m_device, m_render_fence, nullptr);
    vkDestroyCommandPool(m_device, m_command_pool, nullptr);
    vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
    vkDestroyRenderPass(m_device, m_render_pass, nullptr);
    for (auto frame_buffer : m_frame_buffers) {
      vkDestroyFramebuffer(m_device, frame_buffer, nullptr);
    }
    for (auto image_view : m_swapchain_image_views) {
      vkDestroyImageView(m_device, image_view, nullptr);
    }
    vkDestroyDevice(m_device, nullptr);
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    vkb::destroy_debug_utils_messenger(m_instance, m_debug_messenger);
    vkDestroyInstance(m_instance, nullptr);
    SDL_DestroyWindow(m_window);
  }
}
