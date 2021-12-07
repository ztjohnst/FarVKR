#include <SDL2/SDL_video.h>
#include <vulkan/vulkan_core.h>
#ifdef __linux__ 
#define VK_USE_PLATFORM_XLIB_KHR
#endif

#include <stdio.h>
#include <cassert>
#include <vulkan/vulkan.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_vulkan.h>

#define _DEBUG

#define VK_CHECK(call) do { VkResult result_ = call; assert(result_ == VK_SUCCESS); } while(0)

VkPhysicalDevice pickPhysicalDevice(VkPhysicalDevice* physicalDevices, uint32_t physicalDeviceCount)
{
  for(int i = 0 ; i < physicalDeviceCount ; i++)
  {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevices[i], &props);

    if(props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
    {
      printf("Picking discrete GPU: %s\n", props.deviceName);
      return physicalDevices[i];
    }
  }

  if(physicalDeviceCount > 0)
  {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevices[0], &props);

    printf("Picking fallback GPU: %s\n", props.deviceName);
    return physicalDevices[0];
  }

  printf("No devices or drivers support Vulkan!\n");
  return VK_NULL_HANDLE;

}

VkInstance createInstance()
{
  // Create vulkan instance
  // TODO: Should probably check if the device supports vulkan 1.2 via vkEnumerateInstanceVersion.
  VkApplicationInfo appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
  appInfo.apiVersion = VK_API_VERSION_1_2;


  VkInstanceCreateInfo createInfo = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  createInfo.pApplicationInfo = &appInfo;

  // Add validation layers
#ifdef _DEBUG
  const char* debugLayers[] = 
  {
    "VK_LAYER_KHRONOS_validation"
  };

  createInfo.ppEnabledLayerNames = debugLayers;
  createInfo.enabledLayerCount = sizeof(debugLayers) / sizeof(debugLayers[0]);
#endif

  // Add surface extension
  const char* extensions[] = 
  {
    VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef VK_USE_PLATFORM_XLIB_KHR
    VK_KHR_XLIB_SURFACE_EXTENSION_NAME
#endif
  };

  createInfo.ppEnabledExtensionNames = extensions;
  createInfo.enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]);

  VkInstance instance = 0;
  VK_CHECK(vkCreateInstance(&createInfo, 0, &instance));

  return instance;
}

VkSurfaceKHR createSurface(SDL_Window* window, VkInstance instance, SDL_SysWMinfo* wm_info)
{
  VkSurfaceKHR surface;

#ifdef VK_USE_PLATFORM_XLIB_KHR
  VkXlibSurfaceCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR };

  createInfo.dpy = wm_info->info.x11.display;
  createInfo.window = wm_info->info.x11.window;
#endif

  VK_CHECK(vkCreateXlibSurfaceKHR(instance, &createInfo, 0, &surface));
  return surface;
}

VkSurfaceKHR createSurfaceFromSDL(SDL_Window* window, VkInstance instance)
{
  VkSurfaceKHR surface;
  SDL_bool res = SDL_Vulkan_CreateSurface(window, instance, &surface);
  assert(res == SDL_TRUE);
  return surface;
}


VkDevice createDevice(VkInstance instance, VkPhysicalDevice physicalDevice, uint32_t* familyIndex)
{
  *familyIndex = 0;

  float queuePriorities[] = {1.0f};

  VkDeviceQueueCreateInfo queueInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
  queueInfo.queueFamilyIndex = 0; // TODO: this needs to be looked up from queue properties
  queueInfo.queueCount = 1;
  queueInfo.pQueuePriorities = queuePriorities;

  // Add swap chain extension
  const char* extensions[] = 
  {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
  };

  // Create logical device
  VkDeviceCreateInfo createInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
  createInfo.queueCreateInfoCount = 1;
  createInfo.pQueueCreateInfos = &queueInfo;
  createInfo.enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]);
  createInfo.ppEnabledExtensionNames = extensions;
  VkDevice device = 0;
  VK_CHECK(vkCreateDevice(physicalDevice, &createInfo, 0, &device));

  return device;
}

VkSwapchainKHR createSwapchain(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* familyIndex, SDL_Window* window)
{
  // Create swap chain
  int width, height;
  SDL_GetWindowSize(window, &width, &height);

  VkBool32 supported;
  VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, *familyIndex, surface, &supported));

  VkSwapchainCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };

  createInfo.surface = surface;
  createInfo.minImageCount = 2; // For double buffer setup
  createInfo.imageFormat = VK_FORMAT_R8G8B8A8_UNORM; // TODO: find format that device supports
  createInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  createInfo.imageExtent.width = width;
  createInfo.imageExtent.height = height;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  createInfo.queueFamilyIndexCount = 1;
  createInfo.pQueueFamilyIndices = familyIndex;
  createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR; // TODO: check if device supports?

  VkSwapchainKHR swapChain;
  VK_CHECK(vkCreateSwapchainKHR(device, &createInfo, 0, &swapChain));

  return swapChain;
}


VkSemaphore createSemaphore(VkDevice device)
{
  // Create semaphore needed for presenting
  VkSemaphoreCreateInfo createInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

  VkSemaphore semaphore;
  VK_CHECK(vkCreateSemaphore(device, &createInfo, 0, &semaphore));

  return semaphore;
}

VkCommandPool createCommandPool(VkDevice device, uint32_t familyIndex)
{
  VkCommandPoolCreateInfo createInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };

  createInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
  createInfo.queueFamilyIndex = familyIndex;

  VkCommandPool commandPool;
  VK_CHECK(vkCreateCommandPool(device, &createInfo, 0, &commandPool));

  return commandPool;
}

VkRenderPass createRenderPass(VkDevice device)
{

  VkAttachmentDescription attachments[1] = {};
  attachments[0].format = VK_FORMAT_R8G8B8A8_UNORM;
  attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
  attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  // Need to store or we can't render the contents to the screen
  attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  // We don't need a stencil, so operations are don't care.
  attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference colorAttachments = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

  // Need at least one subpass for some reason
  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachments;

  VkRenderPassCreateInfo createInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
  createInfo.attachmentCount = 1;
  createInfo.pAttachments = attachments;
  createInfo.subpassCount = 1;
  createInfo.pSubpasses = &subpass;

  VkRenderPass renderPass;
  VK_CHECK(vkCreateRenderPass(device, &createInfo, 0, &renderPass));

  return renderPass;
}

VkFramebuffer createFramebuffer(VkDevice device, VkRenderPass renderPass, VkImageView imageView, uint32_t width, uint32_t height)
{
  VkFramebufferCreateInfo createInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
  createInfo.renderPass = renderPass;
  createInfo.attachmentCount = 1;
  createInfo.pAttachments = &imageView;
  createInfo.width = width;
  createInfo.height = height;
  createInfo.layers = 1;

  VkFramebuffer framebuffer;
  VK_CHECK(vkCreateFramebuffer(device, &createInfo, 0, &framebuffer));

  return framebuffer;
}

VkImageView createImageView(VkDevice device, VkImage image)
{
  VkImageViewCreateInfo createInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
  createInfo.image = image;
  createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  createInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
  createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  createInfo.subresourceRange.levelCount = 1;
  createInfo.subresourceRange.layerCount = 1;

  VkImageView view;
  VK_CHECK(vkCreateImageView(device, &createInfo, 0, &view));
  
  return view;
}

int main(int argc, char** argv)
{
  // Initialize SDL
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0)
  {
    printf("Failed to initialize SDL!\n");
    return -1;
  }

  VkInstance instance = createInstance();

  // Get Physical device
  VkPhysicalDevice physicalDevices[16];
  uint32_t physicalDeviceCount = sizeof(physicalDevices) / sizeof(physicalDevices[0]);
  VK_CHECK(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices));

  VkPhysicalDevice physicalDevice = pickPhysicalDevice(physicalDevices, physicalDeviceCount);
  assert(physicalDevice);

  uint32_t familyIndex = 0;
  VkDevice device = createDevice(instance, physicalDevice, &familyIndex);

  // Create Window
  SDL_Window* window = SDL_CreateWindow("VKR", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
      1920, 1080, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);

  // Create surface
  // TODO: This is platform specific. Add wayland and windows stuff?
  // TODO: Figure this stuff out. It doesn't work, but it might be handy if we don't want to use openGL.
//#ifdef VK_USE_PLATFORM_XLIB_KHR
  //SDL_SysWMinfo wm_info;
  //SDL_GetWindowWMInfo(window, &wm_info);
  //VkSurfaceKHR surface = createSurface(window, instance, &wm_info);
//#endif

  int windowWidth,windowHeight = 0;
  SDL_GetWindowSize(window, &windowWidth, &windowHeight);

  VkSurfaceKHR surface = createSurfaceFromSDL(window, instance);

  VkSwapchainKHR swapChain = createSwapchain(device, physicalDevice, surface, &familyIndex, window);

  VkSemaphore acquireSemaphore = createSemaphore(device); // For waiting for a GPU to be done with an Image before you render to it again
  VkSemaphore releaseSemaphore = createSemaphore(device); // For waiting until command buffer commands have finished executing before showing the image on screen

  VkQueue queue;
  vkGetDeviceQueue(device, familyIndex, 0, &queue);

  VkRenderPass renderPass = createRenderPass(device);

  VkImage swapchainImages[16]; // TODO: this is bad. Need to ask how many.
  uint32_t swapchainImageCount = sizeof(swapchainImages) / sizeof(swapchainImages[0]);
  VK_CHECK(vkGetSwapchainImagesKHR(device, swapChain, &swapchainImageCount, swapchainImages));

  VkImageView swapchainImageViews[16];
  for(uint32_t i = 0 ; i < swapchainImageCount ; i++)
    swapchainImageViews[i] = createImageView(device, swapchainImages[i]);

  VkFramebuffer swapchainFramebuffers[16];
  for(uint32_t i = 0 ; i < swapchainImageCount ; i++)
    swapchainFramebuffers[i] = createFramebuffer(device, renderPass, swapchainImageViews[i], windowWidth, windowHeight);

  VkCommandPool commandPool = createCommandPool(device, familyIndex);

  VkCommandBufferAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
  allocateInfo.commandPool = commandPool;
  allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocateInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer);

  bool run = true;
  while (run)
  {
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
      if (event.type == SDL_QUIT)
      {
        run = false;
      }

      // Present swap chain to window
      uint32_t imageIndex = 0;

      // The assert fails on Intel GPU for some reason
      if(vkAcquireNextImageKHR(device, swapChain, 0, acquireSemaphore, VK_NULL_HANDLE, &imageIndex) != VK_SUCCESS) // TODO: assert in a loop is slow
      {
        continue;
      }

      VK_CHECK(vkResetCommandPool(device, commandPool, 0));

      VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
      beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
      VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

      VkClearColorValue color = { 1, 0, 1, 1};
      VkImageSubresourceRange range = {};  
      range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      range.levelCount = 1;
      range.layerCount = 1;

      vkCmdClearColorImage(commandBuffer, swapchainImages[imageIndex], VK_IMAGE_LAYOUT_GENERAL, &color, 1, &range);

      VK_CHECK(vkEndCommandBuffer(commandBuffer));

      VkPipelineStageFlags submitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;


      // Need semaphore to tell GPU to not run commands until the image is ready
      VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
      submitInfo.waitSemaphoreCount = 1;
      submitInfo.pWaitSemaphores = &acquireSemaphore;
      submitInfo.pWaitDstStageMask = &submitStageMask;
      submitInfo.commandBufferCount = 1;
      submitInfo.pCommandBuffers = &commandBuffer;
      submitInfo.signalSemaphoreCount = 1;
      submitInfo.pSignalSemaphores = &releaseSemaphore;

      vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);

      VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
      presentInfo.waitSemaphoreCount = 1;
      presentInfo.pWaitSemaphores = &releaseSemaphore;
      presentInfo.swapchainCount = 1;
      presentInfo.pSwapchains = &swapChain;
      presentInfo.pImageIndices = &imageIndex;

      VK_CHECK(vkQueuePresentKHR(queue, &presentInfo));
      VK_CHECK(vkDeviceWaitIdle(device));
    }
  }

  vkDestroySwapchainKHR(device, swapChain, 0);
  vkDestroySurfaceKHR(instance, surface, NULL);
  vkDestroyDevice(device, NULL);
  vkDestroyInstance(instance, NULL);

  return 0;
}
