#ifdef __linux__ 
#define VK_USE_PLATFORM_XLIB_KHR
#endif

#include <stdio.h>
#include <cassert>
#include <vulkan/vulkan.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

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
    VK_KHR_XLIB_SURFACE_EXTENSION_NAME
  };

  createInfo.ppEnabledExtensionNames = extensions;
  createInfo.enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]);

  VkInstance instance = 0;
  VK_CHECK(vkCreateInstance(&createInfo, 0, &instance));

  return instance;
}

VkSurfaceKHR createSurface(SDL_Window* window, VkInstance instance)
{
  VkSurfaceKHR surface;
  VkXlibSurfaceCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR };
    //Display*                       dpy;
    //Window                         window;

  SDL_SysWMinfo wm_info;
  SDL_GetWindowWMInfo(window, &wm_info);

  createInfo.dpy = wm_info.info.x11.display;
  createInfo.window = wm_info.info.x11.window;

  VK_CHECK(vkCreateXlibSurfaceKHR(instance, &createInfo, 0, &surface));
  return surface;
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

  float queuePriorities[] = {1.0f};

  VkDeviceQueueCreateInfo queueInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
  queueInfo.queueFamilyIndex = 0; // TODO: this needs to be looked up from queue properties
  queueInfo.queueCount = 1;
  queueInfo.pQueuePriorities = queuePriorities;

  // Create logical device
  VkDeviceCreateInfo createInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
  createInfo.queueCreateInfoCount = 1;
  createInfo.pQueueCreateInfos = &queueInfo;
  VkDevice device = 0;
  VK_CHECK(vkCreateDevice(physicalDevice, &createInfo, 0, &device));

  // Create Window
  SDL_Window* window = SDL_CreateWindow("VKR", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
      1920, 1080, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);

  // Create surface
  // TODO: This is platform specific
#ifdef VK_USE_PLATFORM_XLIB_KHR
  VkSurfaceKHR surface = createSurface(window, instance);
#endif


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
    }
  }

  vkDestroySurfaceKHR(instance, surface, NULL);
  vkDestroyDevice(device, NULL);
  vkDestroyInstance(instance, NULL);

  return 0;
}
