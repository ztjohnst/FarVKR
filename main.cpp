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
#include <vector>
#include <algorithm>

#define _DEBUG

#define VK_CHECK(call) do { VkResult result_ = call; assert(result_ == VK_SUCCESS); } while(0)

VkPhysicalDevice pickPhysicalDevice(VkPhysicalDevice* physicalDevices, uint32_t physicalDeviceCount)
{
  for(uint32_t i = 0 ; i < physicalDeviceCount ; i++)
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
    VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#endif
    VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
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

uint32_t getGraphicsQueueFamily(VkPhysicalDevice physicalDevice)
{
  uint32_t queueFamilyPropertyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertyCount, 0);

  std::vector<VkQueueFamilyProperties> queueFamilyProperties;
  queueFamilyProperties.resize(queueFamilyPropertyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertyCount, queueFamilyProperties.data());

  for(uint32_t i = 0 ; i < queueFamilyPropertyCount ; i++)
  {
    if(queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
      return i;
  }

  assert(0);
  return 0;
}

VkDevice createDevice(VkInstance instance, VkPhysicalDevice physicalDevice)
{

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

VkFormat getSwapchainFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
  uint32_t formatCount = 0;
  VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, 0));
  assert(formatCount > 0);

  std::vector<VkSurfaceFormatKHR> formats(formatCount);
  VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data()));

  // TODO: Maybe check for HDR? Don't have the hardware to test this now, but might me nice to add.

  // If format count is 1 and it is undefined, than the device supports any format
  if((formatCount == 1) && (formats[0].format == VK_FORMAT_UNDEFINED))
    return VK_FORMAT_R8G8B8A8_UNORM;

  for(uint32_t i = 0 ; i < formatCount ; i++)
  {
    if((formats[i].format == VK_FORMAT_R8G8B8A8_UNORM) || (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM))
      return formats[i].format;
  }

  printf("FORMAT COUNT: %d\n",formatCount);

  assert(0);

  return formats[0].format;
}

VkSwapchainKHR createSwapchain(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkFormat swapchainFormat, uint32_t* familyIndex, SDL_Window* window)
{

  // Get surface capabilities
  VkSurfaceCapabilitiesKHR surfaceCaps;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCaps);

  VkCompositeAlphaFlagBitsKHR surfaceCompostite = (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) 
  ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR : (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) 
  ? VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR : (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR) ? VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR
  : VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;


  // Create swap chain
  int width, height;
  SDL_GetWindowSize(window, &width, &height);

  VkBool32 supported;
  VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, *familyIndex, surface, &supported));
  assert(supported == VK_TRUE);

  VkSwapchainCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };

  createInfo.surface = surface;
  createInfo.minImageCount = std::max(2u, surfaceCaps.minImageCount); // For double buffer setup
  createInfo.imageFormat = swapchainFormat; // TODO: find format that device supports
  createInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  createInfo.imageExtent.width = width;
  createInfo.imageExtent.height = height;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  createInfo.queueFamilyIndexCount = 1;
  createInfo.pQueueFamilyIndices = familyIndex;
  createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  createInfo.compositeAlpha = surfaceCompostite;
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

VkRenderPass createRenderPass(VkDevice device, VkFormat format)
{

  VkAttachmentDescription attachments[1] = {};
  attachments[0].format = format;
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

VkImageView createImageView(VkDevice device, VkImage image, VkFormat format)
{
  VkImageViewCreateInfo createInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
  createInfo.image = image;
  createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  createInfo.format = format;
  createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  createInfo.subresourceRange.levelCount = 1;
  createInfo.subresourceRange.layerCount = 1;

  VkImageView view;
  VK_CHECK(vkCreateImageView(device, &createInfo, 0, &view));
  
  return view;
}

VkShaderModule loadShader(VkDevice device, const char* path)
{
  FILE* file = fopen(path, "rb");
  assert(file);

  fseek(file, 0, SEEK_END);
  uint32_t length = ftell(file);
  assert(length >= 0);
  fseek(file, 0, SEEK_SET);

  char* buffer = (char*)malloc(length);
  assert(buffer);

  size_t rc = fread(buffer, 1, length, file);
  assert(rc == size_t(length));
  fclose(file);

  //assert(length % 4 == 0); // Code size is uint32_t, so 4 bytes

  VkShaderModuleCreateInfo createInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
  createInfo.codeSize = length;
  createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer);
  VkShaderModule shaderModule;
  VK_CHECK(vkCreateShaderModule(device, &createInfo, NULL, &shaderModule));

  return shaderModule;
}

VkPipelineLayout createPipelineLayout(VkDevice device)
{
  VkPipelineLayoutCreateInfo createInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

  VkPipelineLayout pipelineLayout;
  VK_CHECK(vkCreatePipelineLayout(device, &createInfo, NULL, &pipelineLayout));

  return pipelineLayout;
}

VkPipeline createGraphicsPipeline(VkDevice device, VkPipelineCache pipelineCache, VkRenderPass renderPass, VkPipelineLayout layout, VkShaderModule triangleVertSM, VkShaderModule triangleFragSM)
{
  // TODO: Do this next time
  // Pipeline cache is really important
  VkGraphicsPipelineCreateInfo createInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };

  VkPipelineShaderStageCreateInfo stages[2] {};
  stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = triangleVertSM;
  stages[0].pName = "main";
  stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = triangleFragSM;
  stages[1].pName = "main";

  createInfo.stageCount = sizeof(stages) / sizeof(stages[0]);
  createInfo.pStages = stages;

  VkPipelineVertexInputStateCreateInfo vertexInputInfo = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
  createInfo.pVertexInputState = &vertexInputInfo;

  VkPipelineInputAssemblyStateCreateInfo inputAssembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  createInfo.pInputAssemblyState = &inputAssembly;

  VkPipelineTessellationStateCreateInfo tessellationState = { VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO };
  createInfo.pTessellationState = &tessellationState;

  VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;
  createInfo.pViewportState = &viewportState;

  VkPipelineRasterizationStateCreateInfo rasterizationState = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
  rasterizationState.lineWidth = 1.0f;
  createInfo.pRasterizationState = &rasterizationState;

  VkPipelineMultisampleStateCreateInfo multisampleState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
  multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  createInfo.pMultisampleState = &multisampleState;

  VkPipelineDepthStencilStateCreateInfo depthStencilState = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
  createInfo.pDepthStencilState = &depthStencilState;


  VkPipelineColorBlendAttachmentState colorAttachmentState = {};
  colorAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  VkPipelineColorBlendStateCreateInfo colorBlendState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
  colorBlendState.pAttachments = &colorAttachmentState;
  colorBlendState.attachmentCount = 1;
  createInfo.pColorBlendState = &colorBlendState;

  VkPipelineDynamicStateCreateInfo dynamicState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
  VkDynamicState                dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  dynamicState.dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]);
  dynamicState.pDynamicStates = dynamicStates;
  createInfo.pDynamicState = &dynamicState;

  createInfo.layout = layout;
  createInfo.renderPass = renderPass;

  VkPipeline pipeline;
  VK_CHECK(vkCreateGraphicsPipelines(device, pipelineCache, 1, &createInfo, NULL, &pipeline));

  return pipeline;
}

VkBool32 debugReportCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData)
{

  const char* type = (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT ) ? "WARNING" : (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT ) ? "PERFORMANCE WARNING" : "ERROR";

  printf("%s: %s\n",type, pMessage);
  if(flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
    assert(!"Validtion error encountered!");

  // This should always return false according to the spec.
  return VK_FALSE;
}

VkDebugReportCallbackEXT registerDebugCallback(VkInstance instance)
{

  // How to load extensions manually
  PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");

  VkDebugReportCallbackEXT debugCallback = {};
  VkDebugReportCallbackCreateInfoEXT createInfo = { VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT };
  createInfo.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT;
  createInfo.pfnCallback = debugReportCallback;
    //void*                           pUserData;
  VK_CHECK(vkCreateDebugReportCallbackEXT(instance, &createInfo, NULL, &debugCallback));

  return debugCallback;
}

VkImageMemoryBarrier imageBarrier(VkImage image, VkAccessFlags srcAccessMask, VkImageLayout srcImageLayout, VkAccessFlags dstAccessMask, VkImageLayout dstImageLayout)
{
  VkImageMemoryBarrier imMemBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
  imMemBarrier.srcAccessMask = srcAccessMask;
  imMemBarrier.dstAccessMask = dstAccessMask;
  imMemBarrier.oldLayout = srcImageLayout;
  imMemBarrier.newLayout = dstImageLayout;
  imMemBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // We won't be switching graphics queues when we do this just image layouts
  imMemBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // We won't be switching graphics queues when we do this just image layouts
  imMemBarrier.image = image;
  imMemBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // Assume that we don't have depth and are only transitioning color
  imMemBarrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS; // Transitioning entire image like all mip levels
  imMemBarrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS; // Transitioning all array layers

  return imMemBarrier;
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

  VkDebugReportCallbackEXT debugCallback = registerDebugCallback(instance);

  // Get Physical device
  VkPhysicalDevice physicalDevices[16];
  uint32_t physicalDeviceCount = sizeof(physicalDevices) / sizeof(physicalDevices[0]);
  VK_CHECK(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices));

  VkPhysicalDevice physicalDevice = pickPhysicalDevice(physicalDevices, physicalDeviceCount);
  assert(physicalDevice);

  uint32_t familyIndex = getGraphicsQueueFamily(physicalDevice);
  VkDevice device = createDevice(instance, physicalDevice);

  // Create Window
  SDL_Window* window = SDL_CreateWindow("VKR", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
      1920, 1080, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

  // Create surface
  // TODO: This is platform specific. Add wayland and windows stuff?
  // TODO: Figure this stuff out. It doesn't work, but it might be handy if we don't want to use SDL.
//#ifdef VK_USE_PLATFORM_XLIB_KHR
  //SDL_SysWMinfo wm_info;
  //SDL_GetWindowWMInfo(window, &wm_info);
  //VkSurfaceKHR surface = createSurface(window, instance, &wm_info);
//#endif

  int windowWidth,windowHeight = 0;
  SDL_GetWindowSize(window, &windowWidth, &windowHeight);

  VkSurfaceKHR surface = createSurfaceFromSDL(window, instance);

  VkBool32 presentSupprted = 0;
  vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, familyIndex, surface, &presentSupprted);
  assert(presentSupprted);

  VkFormat swapchainFormat = getSwapchainFormat(physicalDevice, surface);

  VkSwapchainKHR swapChain = createSwapchain(device, physicalDevice, surface, swapchainFormat, &familyIndex, window);

  VkSemaphore acquireSemaphore = createSemaphore(device); // For waiting for a GPU to be done with an Image before you render to it again
  VkSemaphore releaseSemaphore = createSemaphore(device); // For waiting until command buffer commands have finished executing before showing the image on screen

  VkQueue queue;
  vkGetDeviceQueue(device, familyIndex, 0, &queue);

  // Load shaders
  VkShaderModule triangleVertSM = loadShader(device, "shaders/triangle_vert.spv");
  assert(triangleVertSM);

  VkShaderModule triangleFragSM = loadShader(device, "shaders/triangle_frag.spv");
  assert(triangleFragSM);


  VkRenderPass renderPass = createRenderPass(device, swapchainFormat);

  VkPipelineLayout pipelineLayout = createPipelineLayout(device);

  // Create graphics pipeline
  // TODO: this is critical for performance
  VkPipelineCache pipelineCache = VK_NULL_HANDLE;
  VkPipeline trianglePipeline = createGraphicsPipeline(device, pipelineCache, renderPass, pipelineLayout, triangleVertSM, triangleFragSM);

  uint32_t swapchainImageCount = 0;
  VK_CHECK(vkGetSwapchainImagesKHR(device, swapChain, &swapchainImageCount, 0));
  std::vector<VkImage> swapchainImages(swapchainImageCount); 
  VK_CHECK(vkGetSwapchainImagesKHR(device, swapChain, &swapchainImageCount, swapchainImages.data()));

  std::vector<VkImageView> swapchainImageViews(swapchainImageCount);
  for(uint32_t i = 0 ; i < swapchainImageCount ; i++)
    swapchainImageViews[i] = createImageView(device, swapchainImages[i], swapchainFormat);

  std::vector<VkFramebuffer> swapchainFramebuffers(swapchainImageCount);
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

      // Need to transition to a valid rendering layout like to save on GPU bandwidth or something
      VkImageMemoryBarrier renderBeginBarrier = imageBarrier(swapchainImages[imageIndex], 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
      vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &renderBeginBarrier);

      VkClearColorValue color = { 48.0f / 255.0f , 10.0f / 255.0f , 36.0f / 255.0f , 1};

      VkClearValue clearColorValue = {};
      clearColorValue.color = color;

      VkRenderPassBeginInfo passBeginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
      passBeginInfo.renderPass = renderPass;
      passBeginInfo.framebuffer = swapchainFramebuffers[imageIndex];
      passBeginInfo.renderArea.extent.width = windowWidth;
      passBeginInfo.renderArea.extent.height = windowHeight;
      passBeginInfo.clearValueCount = 1;
      passBeginInfo.pClearValues = &clearColorValue;

      vkCmdBeginRenderPass(commandBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

      VkViewport viewport = { 0, float(windowHeight), float(windowWidth), -float(windowHeight), 0, 1 };
      VkRect2D scissor = {};
      scissor.extent.height = windowHeight;
      scissor.extent.width = windowWidth;

      vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
      vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

      // Draw calls go here
      vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline);
      vkCmdDraw(commandBuffer, 3, 1, 0, 0);

      vkCmdEndRenderPass(commandBuffer);

      // Need to transition to the present image layout before presenting to the screen
      VkImageMemoryBarrier renderEndBarrier = imageBarrier(swapchainImages[imageIndex], VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
      vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &renderEndBarrier);

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

  VK_CHECK(vkDeviceWaitIdle(device));

  vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
  vkDestroyCommandPool(device, commandPool, NULL);
  //vkDestroyDebugReportCallbackEXT(instance, debugCallback, NULL);
  for(uint32_t i = 0 ; i < swapchainImageCount ; i++)
  {
    vkDestroyFramebuffer(device, swapchainFramebuffers[i], NULL);
  }
  for(uint32_t i = 0 ; i < swapchainImageCount ; i++)
  {
    vkDestroyImageView(device, swapchainImageViews[i], NULL);
  }
  vkDestroyPipeline(device, trianglePipeline, NULL);
  vkDestroyPipelineLayout(device, pipelineLayout, NULL);
  vkDestroyPipelineCache(device, pipelineCache, NULL);
  vkDestroyShaderModule(device, triangleVertSM, NULL);
  vkDestroyShaderModule(device, triangleFragSM, NULL);
  vkDestroyRenderPass(device, renderPass, NULL);
  vkDestroySemaphore(device, releaseSemaphore, NULL);
  vkDestroySemaphore(device, acquireSemaphore, NULL);
  vkDestroySwapchainKHR(device, swapChain, 0);
  vkDestroySurfaceKHR(instance, surface, NULL);
  vkDestroyDevice(device, NULL);
  PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
  vkDestroyDebugReportCallbackEXT(instance, debugCallback, NULL);
  vkDestroyInstance(instance, NULL);
  SDL_DestroyWindow(window);

  return 0;
}
