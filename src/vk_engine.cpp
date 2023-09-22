#include "vk_engine.hpp"

#include <SDL.h>
#include <SDL_vulkan.h>
#include <VkBootstrap.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>

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

void VulkanEngine::init_pipelines()
{
  VkShaderModule vert_shader;
  if (load_shader_module("shaders/triangle.vert.spv", &vert_shader)) {
    std::cerr << "Triangle vertex shader successfully loaded\n";
  } else {
    std::cerr << "Error loading triangle vertex shader\n";
  }

  VkShaderModule frag_shader;
  if (load_shader_module("shaders/triangle.frag.spv", &frag_shader)) {
    std::cerr << "Triangle fragment shader successfully loaded\n";
  } else {
    std::cerr << "Error loading triangle fragment shader\n";
  }

  VkShaderModule red_vert_shader;
  if (load_shader_module("shaders/triangle_red.vert.spv", &red_vert_shader)) {
    std::cerr << "Red triangle vertex shader successfully loaded\n";
  } else {
    std::cerr << "Error loading red triangle vertex shader\n";
  }

  VkShaderModule red_frag_shader;
  if (load_shader_module("shaders/triangle_red.frag.spv", &red_frag_shader)) {
    std::cerr << "Red triangle fragment shader successfully loaded\n";
  } else {
    std::cerr << "Error loading red triangle fragment shader\n";
  }

  auto pipeline_layout_info = vkinit::pipeline_layout_create_info();
  vk_check(vkCreatePipelineLayout(m_device, &pipeline_layout_info, nullptr,
                                  &m_triangle_pipeline_layout));

  PipelineBuilder pipeline_builder;
  pipeline_builder.push_back(vkinit::pipeline_shader_stage_create_info(
      VK_SHADER_STAGE_VERTEX_BIT, vert_shader));
  pipeline_builder.push_back(vkinit::pipeline_shader_stage_create_info(
      VK_SHADER_STAGE_FRAGMENT_BIT, frag_shader));

  pipeline_builder.set_vertex_input_info(
      vkinit::vertex_input_state_create_info());
  pipeline_builder.set_input_assembly_info(
      vkinit::init_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST));
  pipeline_builder.set_viewport(
      {0.0f, 0.0f, static_cast<float>(m_window_extend.width),
       static_cast<float>(m_window_extend.height), 0.0f, 1.0f});
  pipeline_builder.set_scissor({{0, 0}, m_window_extend});
  pipeline_builder.set_rasterizer_info(
      vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL));
  pipeline_builder.set_multisampling_info(
      vkinit::multisampling_state_create_info());
  pipeline_builder.set_color_blend_attachment_state(
      vkinit::color_blench_attachment_state());
  pipeline_builder.set_pipeline_layout(m_triangle_pipeline_layout);
  // Build pipelines
  m_triangle_pipeline =
      pipeline_builder.build_pipeline(m_device, m_render_pass);

  pipeline_builder.clear_shaders();

  pipeline_builder.push_back(vkinit::pipeline_shader_stage_create_info(
      VK_SHADER_STAGE_VERTEX_BIT, red_vert_shader));
  pipeline_builder.push_back(vkinit::pipeline_shader_stage_create_info(
      VK_SHADER_STAGE_FRAGMENT_BIT, red_frag_shader));
  m_red_triangle_pipeline =
      pipeline_builder.build_pipeline(m_device, m_render_pass);
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
  init_pipelines();
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
  if (m_selected_shader == 0) {
    vkCmdBindPipeline(m_main_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      m_triangle_pipeline);
  } else {
    vkCmdBindPipeline(m_main_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      m_red_triangle_pipeline);
  }
  vkCmdDraw(m_main_command_buffer, 3, 1, 0, 0);

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
      } else if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_SPACE) {
          std::cerr << "switch shader\n";
          m_selected_shader =
              m_selected_shader == 1 ? 0 : m_selected_shader + 1;
        }
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

bool VulkanEngine::load_shader_module(std::filesystem::path const& file_path,
                                      VkShaderModule* out_shader_module)
{
  std::ifstream file{file_path, std::ios::ate | std::ios::binary};
  if (!file.is_open()) {
    std::cerr << file_path << " not found\n";
    return false;
  }
  std::size_t file_size = file.tellg();
  std::vector<uint32_t> buffer;
  buffer.resize(file_size / sizeof(uint32_t));
  file.seekg(0);
  file.read(reinterpret_cast<char*>(buffer.data()), file_size);

  // Create a new shader module
  VkShaderModuleCreateInfo create_info{};
  create_info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.pNext    = nullptr;
  create_info.codeSize = buffer.size() * sizeof(uint32_t);
  create_info.pCode    = buffer.data();

  VkShaderModule shader_module;
  if (vkCreateShaderModule(m_device, &create_info, nullptr, &shader_module)
      != VK_SUCCESS) {
    return false;
  }
  *out_shader_module = shader_module;
  return true;
}

VkPipeline PipelineBuilder::build_pipeline(VkDevice device, VkRenderPass pass)
{
  VkPipelineViewportStateCreateInfo viewport_state{};
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.pNext = nullptr;
  viewport_state.viewportCount = 1;
  viewport_state.pViewports    = &m_viewport;
  viewport_state.scissorCount  = 1;
  viewport_state.pScissors     = &m_scissor;

  VkPipelineColorBlendStateCreateInfo color_blending{};
  color_blending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blending.pNext           = nullptr;
  color_blending.logicOpEnable   = VK_FALSE;
  color_blending.logicOp         = VK_LOGIC_OP_COPY;
  color_blending.attachmentCount = 1;
  color_blending.pAttachments    = &m_color_blend_attachment;

  VkGraphicsPipelineCreateInfo pipeline_info{};
  pipeline_info.sType      = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_info.pNext      = nullptr;
  pipeline_info.stageCount = m_shader_stages.size();
  pipeline_info.pStages    = m_shader_stages.data();
  pipeline_info.pVertexInputState   = &m_vertex_input_info;
  pipeline_info.pInputAssemblyState = &m_input_assembly;
  pipeline_info.pViewportState      = &viewport_state;
  pipeline_info.pRasterizationState = &m_rasterizer;
  pipeline_info.pMultisampleState   = &m_multisampling;
  pipeline_info.pColorBlendState    = &color_blending;
  pipeline_info.layout              = m_pipeline_layout;
  pipeline_info.renderPass          = pass;
  pipeline_info.subpass             = 0;
  pipeline_info.basePipelineHandle  = VK_NULL_HANDLE;

  VkPipeline pipeline;
  VkResult result = vkCreateGraphicsPipelines(
      device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline);
  if (result == VK_SUCCESS) {
    return pipeline;
  } else {
    std::cerr << "failed to create pipeline with error: " << result << '\n';
    return VK_NULL_HANDLE;
  }
}

void PipelineBuilder::push_back(VkPipelineShaderStageCreateInfo&& shader_stage)
{
  m_shader_stages.push_back(std::move(shader_stage));
}

void PipelineBuilder::set_vertex_input_info(
    VkPipelineVertexInputStateCreateInfo const& info)
{
  m_vertex_input_info = info;
}

void PipelineBuilder::set_input_assembly_info(
    VkPipelineInputAssemblyStateCreateInfo const& info)
{
  m_input_assembly = info;
}

void PipelineBuilder::set_viewport(VkViewport const& viewport)
{
  m_viewport = viewport;
}

void PipelineBuilder::set_scissor(VkRect2D const& scissor)
{
  m_scissor = scissor;
}

void PipelineBuilder::set_rasterizer_info(
    VkPipelineRasterizationStateCreateInfo const& info)
{
  m_rasterizer = info;
}

void PipelineBuilder::set_color_blend_attachment_state(
    VkPipelineColorBlendAttachmentState const& state)
{
  m_color_blend_attachment = state;
}

void PipelineBuilder::set_multisampling_info(
    VkPipelineMultisampleStateCreateInfo const& info)
{
  m_multisampling = info;
}

void PipelineBuilder::set_pipeline_layout(VkPipelineLayout const& layout)
{
  m_pipeline_layout = layout;
}

void PipelineBuilder::clear_shaders()
{
  m_shader_stages.clear();
}
