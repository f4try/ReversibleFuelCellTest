// Dear ImGui: standalone example application for Glfw + Vulkan
// If you are new to Dear ImGui, read documentation from the docs/ folder + read
// the top of imgui.cpp. Read online:
// https://github.com/ocornut/imgui/tree/master/docs

// Important note to the reader who wish to integrate imgui_impl_vulkan.cpp/.h
// in their own engine/app.
// - Common ImGui_ImplVulkan_XXX functions and structures are used to interface
// with imgui_impl_vulkan.cpp/.h.
//   You will use those if you want to use this rendering backend in your
//   engine/app.
// - Helper ImGui_ImplVulkanH_XXX functions and structures are only used by this
// example (main.cpp) and by
//   the backend itself (imgui_impl_vulkan.cpp), but should PROBABLY NOT be used
//   by your own engine/app code.
// Read comments in imgui_impl_vulkan.h.

#pragma execution_character_set("utf-8")
#define RELEASE
#ifdef RELEASE
#pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"")
#endif
#include <stdlib.h>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <stdexcept>
#include <thread>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "implot.h"
#include "seriallib.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "visalib.hpp"
#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to
// maximize ease of testing and compatibility with old VS compilers. To link
// with VS2010-era libraries, VS2015+ requires linking with
// legacy_stdio_definitions.lib, which we do using this pragma. Your own project
// should not be affected, as you are likely to link with a newer binary of GLFW
// that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && \
    !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

//#define IMGUI_UNLIMITED_FRAME_RATE
#ifdef _DEBUG
#define IMGUI_VULKAN_DEBUG_REPORT
#endif

static VkAllocationCallbacks* g_Allocator = NULL;
static VkInstance g_Instance = VK_NULL_HANDLE;
static VkPhysicalDevice g_PhysicalDevice = VK_NULL_HANDLE;
static VkDevice g_Device = VK_NULL_HANDLE;
static uint32_t g_QueueFamily = (uint32_t)-1;
static VkQueue g_Queue = VK_NULL_HANDLE;
static VkDebugReportCallbackEXT g_DebugReport = VK_NULL_HANDLE;
static VkPipelineCache g_PipelineCache = VK_NULL_HANDLE;
static VkDescriptorPool g_DescriptorPool = VK_NULL_HANDLE;

static ImGui_ImplVulkanH_Window g_MainWindowData;
static int g_MinImageCount = 2;
static bool g_SwapChainRebuild = false;

static void check_vk_result(VkResult err) {
  if (err == 0) return;
  fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
  if (err < 0) abort();
}

#ifdef IMGUI_VULKAN_DEBUG_REPORT
static VKAPI_ATTR VkBool32 VKAPI_CALL
debug_report(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType,
             uint64_t object, size_t location, int32_t messageCode,
             const char* pLayerPrefix, const char* pMessage, void* pUserData) {
  (void)flags;
  (void)object;
  (void)location;
  (void)messageCode;
  (void)pUserData;
  (void)pLayerPrefix;  // Unused arguments
  fprintf(stderr, "[vulkan] Debug report from ObjectType: %i\nMessage: %s\n\n",
          objectType, pMessage);
  return VK_FALSE;
}
#endif  // IMGUI_VULKAN_DEBUG_REPORT

static void SetupVulkan(const char** extensions, uint32_t extensions_count) {
  VkResult err;

  // Create Vulkan Instance
  {
    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.enabledExtensionCount = extensions_count;
    create_info.ppEnabledExtensionNames = extensions;
#ifdef IMGUI_VULKAN_DEBUG_REPORT
    // Enabling validation layers
    const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
    create_info.enabledLayerCount = 1;
    create_info.ppEnabledLayerNames = layers;

    // Enable debug report extension (we need additional storage, so we
    // duplicate the user array to add our new extension to it)
    const char** extensions_ext =
        (const char**)malloc(sizeof(const char*) * (extensions_count + 1));
    memcpy(extensions_ext, extensions, extensions_count * sizeof(const char*));
    extensions_ext[extensions_count] = "VK_EXT_debug_report";
    create_info.enabledExtensionCount = extensions_count + 1;
    create_info.ppEnabledExtensionNames = extensions_ext;

    // Create Vulkan Instance
    err = vkCreateInstance(&create_info, g_Allocator, &g_Instance);
    check_vk_result(err);
    free(extensions_ext);

    // Get the function pointer (required for any extensions)
    auto vkCreateDebugReportCallbackEXT =
        (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(
            g_Instance, "vkCreateDebugReportCallbackEXT");
    IM_ASSERT(vkCreateDebugReportCallbackEXT != NULL);

    // Setup the debug report callback
    VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
    debug_report_ci.sType =
        VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    debug_report_ci.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT |
                            VK_DEBUG_REPORT_WARNING_BIT_EXT |
                            VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
    debug_report_ci.pfnCallback = debug_report;
    debug_report_ci.pUserData = NULL;
    err = vkCreateDebugReportCallbackEXT(g_Instance, &debug_report_ci,
                                         g_Allocator, &g_DebugReport);
    check_vk_result(err);
#else
    // Create Vulkan Instance without any debug feature
    err = vkCreateInstance(&create_info, g_Allocator, &g_Instance);
    check_vk_result(err);
    IM_UNUSED(g_DebugReport);
#endif
  }

  // Select GPU
  {
    uint32_t gpu_count;
    err = vkEnumeratePhysicalDevices(g_Instance, &gpu_count, NULL);
    check_vk_result(err);
    IM_ASSERT(gpu_count > 0);

    VkPhysicalDevice* gpus =
        (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * gpu_count);
    err = vkEnumeratePhysicalDevices(g_Instance, &gpu_count, gpus);
    check_vk_result(err);

    // If a number >1 of GPUs got reported, find discrete GPU if present, or use
    // first one available. This covers most common cases
    // (multi-gpu/integrated+dedicated graphics). Handling more complicated
    // setups (multiple dedicated GPUs) is out of scope of this sample.
    int use_gpu = 0;
    for (int i = 0; i < (int)gpu_count; i++) {
      VkPhysicalDeviceProperties properties;
      vkGetPhysicalDeviceProperties(gpus[i], &properties);
      if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        use_gpu = i;
        break;
      }
    }

    g_PhysicalDevice = gpus[use_gpu];
    free(gpus);
  }

  // Select graphics queue family
  {
    uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(g_PhysicalDevice, &count, NULL);
    VkQueueFamilyProperties* queues = (VkQueueFamilyProperties*)malloc(
        sizeof(VkQueueFamilyProperties) * count);
    vkGetPhysicalDeviceQueueFamilyProperties(g_PhysicalDevice, &count, queues);
    for (uint32_t i = 0; i < count; i++)
      if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        g_QueueFamily = i;
        break;
      }
    free(queues);
    IM_ASSERT(g_QueueFamily != (uint32_t)-1);
  }

  // Create Logical Device (with 1 queue)
  {
    int device_extension_count = 1;
    const char* device_extensions[] = {"VK_KHR_swapchain"};
    const float queue_priority[] = {1.0f};
    VkDeviceQueueCreateInfo queue_info[1] = {};
    queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info[0].queueFamilyIndex = g_QueueFamily;
    queue_info[0].queueCount = 1;
    queue_info[0].pQueuePriorities = queue_priority;
    VkDeviceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount =
        sizeof(queue_info) / sizeof(queue_info[0]);
    create_info.pQueueCreateInfos = queue_info;
    create_info.enabledExtensionCount = device_extension_count;
    create_info.ppEnabledExtensionNames = device_extensions;
    err =
        vkCreateDevice(g_PhysicalDevice, &create_info, g_Allocator, &g_Device);
    check_vk_result(err);
    vkGetDeviceQueue(g_Device, g_QueueFamily, 0, &g_Queue);
  }

  // Create Descriptor Pool
  {
    VkDescriptorPoolSize pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;
    err = vkCreateDescriptorPool(g_Device, &pool_info, g_Allocator,
                                 &g_DescriptorPool);
    check_vk_result(err);
  }
}

// All the ImGui_ImplVulkanH_XXX structures/functions are optional helpers used
// by the demo. Your real engine/app may not use them.
static void SetupVulkanWindow(ImGui_ImplVulkanH_Window* wd,
                              VkSurfaceKHR surface, int width, int height) {
  wd->Surface = surface;

  // Check for WSI support
  VkBool32 res;
  vkGetPhysicalDeviceSurfaceSupportKHR(g_PhysicalDevice, g_QueueFamily,
                                       wd->Surface, &res);
  if (res != VK_TRUE) {
    fprintf(stderr, "Error no WSI support on physical device 0\n");
    exit(-1);
  }

  // Select Surface Format
  const VkFormat requestSurfaceImageFormat[] = {
      VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM,
      VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM};
  const VkColorSpaceKHR requestSurfaceColorSpace =
      VK_COLORSPACE_SRGB_NONLINEAR_KHR;
  wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(
      g_PhysicalDevice, wd->Surface, requestSurfaceImageFormat,
      (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat),
      requestSurfaceColorSpace);

  // Select Present Mode
#ifdef IMGUI_UNLIMITED_FRAME_RATE
  VkPresentModeKHR present_modes[] = {VK_PRESENT_MODE_MAILBOX_KHR,
                                      VK_PRESENT_MODE_IMMEDIATE_KHR,
                                      VK_PRESENT_MODE_FIFO_KHR};
#else
  VkPresentModeKHR present_modes[] = {VK_PRESENT_MODE_FIFO_KHR};
#endif
  wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(
      g_PhysicalDevice, wd->Surface, &present_modes[0],
      IM_ARRAYSIZE(present_modes));
  // printf("[vulkan] Selected PresentMode = %d\n", wd->PresentMode);

  // Create SwapChain, RenderPass, Framebuffer, etc.
  IM_ASSERT(g_MinImageCount >= 2);
  ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, g_PhysicalDevice, g_Device,
                                         wd, g_QueueFamily, g_Allocator, width,
                                         height, g_MinImageCount);
}

static void CleanupVulkan() {
  vkDestroyDescriptorPool(g_Device, g_DescriptorPool, g_Allocator);

#ifdef IMGUI_VULKAN_DEBUG_REPORT
  // Remove the debug report callback
  auto vkDestroyDebugReportCallbackEXT =
      (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(
          g_Instance, "vkDestroyDebugReportCallbackEXT");
  vkDestroyDebugReportCallbackEXT(g_Instance, g_DebugReport, g_Allocator);
#endif  // IMGUI_VULKAN_DEBUG_REPORT

  vkDestroyDevice(g_Device, g_Allocator);
  vkDestroyInstance(g_Instance, g_Allocator);
}

static void CleanupVulkanWindow() {
  ImGui_ImplVulkanH_DestroyWindow(g_Instance, g_Device, &g_MainWindowData,
                                  g_Allocator);
}

static void FrameRender(ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data) {
  VkResult err;

  VkSemaphore image_acquired_semaphore =
      wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
  VkSemaphore render_complete_semaphore =
      wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
  err = vkAcquireNextImageKHR(g_Device, wd->Swapchain, UINT64_MAX,
                              image_acquired_semaphore, VK_NULL_HANDLE,
                              &wd->FrameIndex);
  if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
    g_SwapChainRebuild = true;
    return;
  }
  check_vk_result(err);

  ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
  {
    err = vkWaitForFences(
        g_Device, 1, &fd->Fence, VK_TRUE,
        UINT64_MAX);  // wait indefinitely instead of periodically checking
    check_vk_result(err);

    err = vkResetFences(g_Device, 1, &fd->Fence);
    check_vk_result(err);
  }
  {
    err = vkResetCommandPool(g_Device, fd->CommandPool, 0);
    check_vk_result(err);
    VkCommandBufferBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
    check_vk_result(err);
  }
  {
    VkRenderPassBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass = wd->RenderPass;
    info.framebuffer = fd->Framebuffer;
    info.renderArea.extent.width = wd->Width;
    info.renderArea.extent.height = wd->Height;
    info.clearValueCount = 1;
    info.pClearValues = &wd->ClearValue;
    vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
  }

  // Record dear imgui primitives into command buffer
  ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);

  // Submit command buffer
  vkCmdEndRenderPass(fd->CommandBuffer);
  {
    VkPipelineStageFlags wait_stage =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &image_acquired_semaphore;
    info.pWaitDstStageMask = &wait_stage;
    info.commandBufferCount = 1;
    info.pCommandBuffers = &fd->CommandBuffer;
    info.signalSemaphoreCount = 1;
    info.pSignalSemaphores = &render_complete_semaphore;

    err = vkEndCommandBuffer(fd->CommandBuffer);
    check_vk_result(err);
    err = vkQueueSubmit(g_Queue, 1, &info, fd->Fence);
    check_vk_result(err);
  }
}

static void FramePresent(ImGui_ImplVulkanH_Window* wd) {
  if (g_SwapChainRebuild) return;
  VkSemaphore render_complete_semaphore =
      wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
  VkPresentInfoKHR info = {};
  info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  info.waitSemaphoreCount = 1;
  info.pWaitSemaphores = &render_complete_semaphore;
  info.swapchainCount = 1;
  info.pSwapchains = &wd->Swapchain;
  info.pImageIndices = &wd->FrameIndex;
  VkResult err = vkQueuePresentKHR(g_Queue, &info);
  if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
    g_SwapChainRebuild = true;
    return;
  }
  check_vk_result(err);
  wd->SemaphoreIndex =
      (wd->SemaphoreIndex + 1) %
      wd->ImageCount;  // Now we can use the next set of semaphores
}

static void glfw_error_callback(int error, const char* description) {
  fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

void sweep_ivp(seriallib* it8512, visalib* psw, float set_current,
               float set_voltage, float ocv, int step, float step_time,
               int mode, float* progress, std::vector<float>* time_ivp,
               std::vector<float>* voltage_ivp, std::vector<float>* current_ivp,
               std::vector<float>* power_ivp,
               std::vector<float>* hydrogen_ivp) {
  time_ivp->clear();
  voltage_ivp->clear();
  current_ivp->clear();
  power_ivp->clear();
  hydrogen_ivp->clear();

  FILE* fp = NULL;
  time_t now = std::time(0);
  tm* ltm = localtime(&now);
  char filename[80];
  if (mode == 0) {
    sprintf(filename, "outputs\\ivp-fc-%d-%d-%d-%d-%d-%d.csv",
            ltm->tm_year + 1900, ltm->tm_mon + 1, ltm->tm_mday, ltm->tm_hour,
            ltm->tm_min, ltm->tm_sec);
  } else {
    sprintf(filename, "outputs\\ivp-ec-%d-%d-%d-%d-%d-%d.csv",
            ltm->tm_year + 1900, ltm->tm_mon + 1, ltm->tm_mday, ltm->tm_hour,
            ltm->tm_min, ltm->tm_sec);
  }
  fp = fopen(filename, "a");
  fputs("time,voltage,current,power,hydrogen,mode\n", fp);
  // double start_time = ImGui::GetTime();
  // double now = ImGui::GetTime();
  double last_time = 0.0;
  float vcp[3];
  for (int i = 0; i < step + 1; i++) {
    if (mode == 0) {
      if (!it8512->setCurrent(set_current / step * i)) {
        std::cout << "设置负载电流失败!" << std::endl;
      }
    } else {
      if (!psw->setVoltage((set_voltage - ocv) / step * i + ocv)) {
        std::cout << "设置电源电压失败!" << std::endl;
      }
    }
    // Sleep(step_time * 1000);
    std::this_thread::sleep_for(
        std::chrono::milliseconds(int(step_time * 1000)));
    last_time = step_time * i;
    if (!it8512->readVCP(vcp)) {
      std::cout << "读取电压、电流、功率失败!" << std::endl;
    }
    if (mode == 1) {
      vcp[1] = psw->readCurrent();
      vcp[2] = vcp[1] * vcp[2];
    }

    // std::cout << last_time << ": " << *vcp << "," << *(vcp + 1) << ","
    //           << *(vcp + 2) << "," << std::endl;
    time_ivp->push_back(last_time);
    voltage_ivp->push_back(*vcp);
    current_ivp->push_back(*(vcp + 1));
    power_ivp->push_back(*(vcp + 2));
    hydrogen_ivp->push_back(*(vcp + 1) / 26.801 / 2.0 * 23.8 * 20.0);
    fprintf(fp, "%.4f,%.2f,%.3f,%.3f,%.3f,%d\n", last_time, voltage_ivp->back(),
            current_ivp->back(), power_ivp->back(), hydrogen_ivp->back(), mode);
    *progress = 1.0 / step * i;
  }
  fclose(fp);
}

int main(int, char**) {
  // Setup GLFW window
  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit()) return 1;

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
  GLFWwindow* window =
      glfwCreateWindow(1280, 720, "可逆固体氧化物电池测试平台", NULL, NULL);
  GLFWimage images[1];
  images[0].pixels =
      stbi_load("icon.png", &images[0].width, &images[0].height, 0, 4);
  glfwSetWindowIcon(window, 1, images);

  glfwMaximizeWindow(window);
  // Setup Vulkan
  if (!glfwVulkanSupported()) {
    printf("GLFW: Vulkan Not Supported\n");
    return 1;
  }
  uint32_t extensions_count = 0;
  const char** extensions =
      glfwGetRequiredInstanceExtensions(&extensions_count);
  SetupVulkan(extensions, extensions_count);

  // Create Window Surface
  VkSurfaceKHR surface;
  VkResult err =
      glfwCreateWindowSurface(g_Instance, window, g_Allocator, &surface);
  check_vk_result(err);

  // Create Framebuffers
  int w, h;
  glfwGetFramebufferSize(window, &w, &h);
  ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
  SetupVulkanWindow(wd, surface, w, h);

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImPlot::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void)io;
  // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable
  // Keyboard Controls io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; //
  // Enable Gamepad Controls

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  // ImGui::StyleColorsClassic();

  // Setup Platform/Renderer backends
  ImGui_ImplGlfw_InitForVulkan(window, true);
  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = g_Instance;
  init_info.PhysicalDevice = g_PhysicalDevice;
  init_info.Device = g_Device;
  init_info.QueueFamily = g_QueueFamily;
  init_info.Queue = g_Queue;
  init_info.PipelineCache = g_PipelineCache;
  init_info.DescriptorPool = g_DescriptorPool;
  init_info.Allocator = g_Allocator;
  init_info.MinImageCount = g_MinImageCount;
  init_info.ImageCount = wd->ImageCount;
  init_info.CheckVkResultFn = check_vk_result;
  ImGui_ImplVulkan_Init(&init_info, wd->RenderPass);

  // Load Fonts
  // - If no fonts are loaded, dear imgui will use the default font. You can
  // also load multiple fonts and use ImGui::PushFont()/PopFont() to select
  // them.
  // - AddFontFromFileTTF() will return the ImFont* so you can store it if you
  // need to select the font among multiple.
  // - If the file cannot be loaded, the function will return NULL. Please
  // handle those errors in your application (e.g. use an assertion, or display
  // an error and quit).
  // - The fonts will be rasterized at a given size (w/ oversampling) and stored
  // into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which
  // ImGui_ImplXXXX_NewFrame below will call.
  // - Read 'docs/FONTS.md' for more instructions and details.
  // - Remember that in C/C++ if you want to include a backslash \ in a string
  // literal you need to write a double backslash \\ !
  // io.Fonts->AddFontDefault();
  // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
  // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
  // io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
  // io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
  // ImFont* font =
  // io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f,
  // NULL, io.Fonts->GetGlyphRangesJapanese()); IM_ASSERT(font != NULL);

  ImFont* font =
      io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\simhei.ttf", 20.0f,
                                   NULL, io.Fonts->GetGlyphRangesChineseFull());
  IM_ASSERT(font != NULL);
  // Upload Fonts
  {
    // Use any command queue
    VkCommandPool command_pool = wd->Frames[wd->FrameIndex].CommandPool;
    VkCommandBuffer command_buffer = wd->Frames[wd->FrameIndex].CommandBuffer;

    err = vkResetCommandPool(g_Device, command_pool, 0);
    check_vk_result(err);
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err = vkBeginCommandBuffer(command_buffer, &begin_info);
    check_vk_result(err);

    ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

    VkSubmitInfo end_info = {};
    end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    end_info.commandBufferCount = 1;
    end_info.pCommandBuffers = &command_buffer;
    err = vkEndCommandBuffer(command_buffer);
    check_vk_result(err);
    err = vkQueueSubmit(g_Queue, 1, &end_info, VK_NULL_HANDLE);
    check_vk_result(err);

    err = vkDeviceWaitIdle(g_Device);
    check_vk_result(err);
    ImGui_ImplVulkan_DestroyFontUploadObjects();
  }

  // Our state
  static int mode = 0;
  static float progress = 0.0f;
  static float readFreq = 10.0f;
  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
  static bool setting_window_status = false;
  // inital serial
  seriallib it8512("COM6");
  if (!it8512.loadOn()) {
    std::cout << "打开电子负载失败!" << std::endl;
  }
  visalib psw("ASRL3::INSTR");
  if (!psw.output(false)) {
    std::cout << "关闭电源失败!" << std::endl;
  }
  float vcp[3];
  double last_time = ImGui::GetTime();

  std::vector<float> time = {};
  std::vector<float> voltage = {};
  std::vector<float> current = {};
  std::vector<float> power = {};
  std::vector<float> hydrogen = {};

  std::vector<float> time_ivp = {};
  std::vector<float> voltage_ivp = {};
  std::vector<float> current_ivp = {};
  std::vector<float> power_ivp = {};
  std::vector<float> hydrogen_ivp = {};

  static float smallest_c = 0;
  static float biggest_c = 10;
  static float smallest_v = 0;
  static float biggest_v = 30;
  static float smallest_p = 0;
  static float biggest_p = 200;
  static float smallest_h = smallest_c / 26.801 / 2.0 * 23.8 * 20.0;
  static float biggest_h = biggest_c / 26.801 / 2.0 * 23.8 * 20.0;

  ImGui::StyleColorsLight();
  ImPlot::StyleColorsLight();
  ImPlotStyle& style = ImPlot::GetStyle();
  style.AntiAliasedLines = true;
  style.LineWeight = 1.5;
  ImPlot::PushColormap("Dark");
  // system("cd %~dp0");
  system("if not exist outputs mkdir outputs");
  FILE* fp = NULL;
  time_t now = std::time(0);
  tm* ltm = localtime(&now);
  char filename[80];
  sprintf(filename, "outputs\\data%d-%d-%d-%d-%d-%d.csv", ltm->tm_year + 1900,
          ltm->tm_mon + 1, ltm->tm_mday, ltm->tm_hour, ltm->tm_min,
          ltm->tm_sec);
  fp = fopen(filename, "a");
  fputs("time,voltage,current,power,hydrogen,mode\n", fp);

  // Main loop
  while (!glfwWindowShouldClose(window)) {
    if (ImGui::GetTime() - last_time > 1.0f / readFreq) {
      if (!it8512.readVCP(vcp)) {
        std::cout << "读取电压、电流、功率失败!" << std::endl;
      }
      if (mode == 1) {
        vcp[1] = psw.readCurrent();
        vcp[2] = vcp[0] * vcp[1];
      }

      last_time = ImGui::GetTime();
      // std::cout << last_time << ": " << *vcp << "," << *(vcp + 1) << ","
      //           << *(vcp + 2) << "," << std::endl;
      time.push_back(last_time);
      voltage.push_back(*vcp);
      current.push_back(*(vcp + 1));
      power.push_back(*(vcp + 2));
      hydrogen.push_back(*(vcp + 1) / 26.801 / 2.0 * 23.8 * 20.0);
      fprintf(fp, "%.4f,%.2f,%.3f,%.3f,%.3f,%d\n", last_time, voltage.back(),
              current.back(), power.back(), hydrogen.back(), mode);
      if (current.size() > 1) {
        smallest_c = min(current.back() - 0.03, smallest_c);
        biggest_c = max(current.back() + 0.03, biggest_c);
      } else if (current.size() == 1) {
        smallest_c = current[0] - 0.03;
        biggest_c = current[0] + 0.03;
      }

      if (voltage.size() > 1) {
        smallest_v = min(voltage.back() - 0.03, smallest_v);
        biggest_v = max(voltage.back() + 0.03, biggest_v);
      } else if (voltage.size() == 1) {
        smallest_v = voltage[0] - 0.03;
        biggest_v = voltage[0] + 0.03;
      }

      if (power.size() > 1) {
        smallest_p = min(power.back() - 0.03, smallest_p);
        biggest_p = max(power.back() + 0.03, biggest_p);
      } else if (power.size() == 1) {
        smallest_p = power[0] - 0.03;
        biggest_p = power[0] + 0.03;
      }

      smallest_h = smallest_c / 26.801 / 2.0 * 23.8 * 20.0;
      biggest_h = biggest_c / 26.801 / 2.0 * 23.8 * 20.0;
    }
    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to
    // tell if dear imgui wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to
    // your main application.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input
    // data to your main application. Generally you may always pass all inputs
    // to dear imgui, and hide them from your application based on those two
    // flags.
    glfwPollEvents();

    // Resize swap chain?
    if (g_SwapChainRebuild) {
      int width, height;
      glfwGetFramebufferSize(window, &width, &height);
      if (width > 0 && height > 0) {
        ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount);
        ImGui_ImplVulkanH_CreateOrResizeWindow(
            g_Instance, g_PhysicalDevice, g_Device, &g_MainWindowData,
            g_QueueFamily, g_Allocator, width, height, g_MinImageCount);
        g_MainWindowData.FrameIndex = 0;
        g_SwapChainRebuild = false;
      }
    }

    // Start the Dear ImGui frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // 1. Show the big demo window (Most of the sample code is in
    // ImGui::ShowDemoWindow()! You can browse its code to learn more about
    // Dear ImGui!). if (show_demo_window)
    // ImGui::ShowDemoWindow(&show_demo_window);

    // 2. Show a simple window that we create ourselves. We use a Begin/End
    // pair to created a named window.
    ImGui::Begin("运行状态");  // Pass a pointer to our bool variable (the
                               // window will have a closing button that will
                               // clear the bool when clicked)
    if ((progress > 0.999f) || (progress < 0.001f)) {
      if (ImGui::RadioButton("发电模式", &mode, 0)) {
        if (!it8512.loadOn()) {
          std::cout << "打开电子负载失败!" << std::endl;
        }
        if (!psw.output(false)) {
          std::cout << "关闭电源失败!" << std::endl;
        }
      }
      ImGui::SameLine();
      if (ImGui::RadioButton("电解模式", &mode, 1)) {
        if (!it8512.loadOff()) {
          std::cout << "关闭电子负载失败!" << std::endl;
        }
        if (!psw.output(true)) {
          std::cout << "打开电源失败!" << std::endl;
        }
      }
    } else {
      if (mode == 0) {
        ImGui::Text("发电模式扫描中...");
      } else {
        ImGui::Text("电解模式扫描中...");
      }
    }

    if (voltage.size() > 0) {
      ImGui::Text("电压: %.2f  V", voltage.back());
      ImGui::Text("电流: %.3f A", current.back());
      ImGui::Text("功率: %.3f W", power.back());
    }
    if (mode == 1) {
      ImGui::Text("产氢率: %.3f NL/h", hydrogen.back());
    }
    ImGui::Text("FPS %.1f", ImGui::GetIO().Framerate);
    ImGui::Checkbox("设置", &setting_window_status);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram,
                          (ImVec4)ImColor::ImColor(26, 115, 232, 256));
    if (power.size() > 0) {
      if (mode == 0) {
        char buf[32];
        sprintf(buf, "%.1f / %.1f W", power.back(), biggest_p);
        ImGui::ProgressBar(power.back() / biggest_p, ImVec2(0.0f, 0.0f), buf);
      } else {
        char buf[32];
        sprintf(buf, "%.1f / %.1f NL/h", hydrogen.back(), biggest_h);
        ImGui::ProgressBar(hydrogen.back() / biggest_h, ImVec2(0.0f, 0.0f),
                           buf);
      }
    }
    ImGui::PopStyleColor();
    ImGui::End();

    if (setting_window_status) {
      ImGui::Begin("设置", &setting_window_status);
      ImGui::ShowStyleSelector("界面样式");
      ImPlot::ShowStyleSelector("绘图样式");
      ImPlot::ShowColormapSelector("图线颜色");
      ImGui::Checkbox("图线抗锯齿", &ImPlot::GetStyle().AntiAliasedLines);
      ImGui::DragFloat("采样频率 (Hz)", &readFreq, 1.0, 1.0, 60.0);
      ImGui::End();
    }
    static float set_current = 0.0f;
    static float ocv = 0.0f;
    static float set_voltage = 0.0f;
    static int step = 20;
    static float step_time = 1.0;

    ImGui::Begin("测试参数");
    if (mode == 0) {
      ImGui::DragFloat("负载电流 (A)", &set_current, 0.5, 0.0, 10.0);
    } else {
      ImGui::DragFloat("电源电压 (V)", &set_voltage, 0.5, 0.0, 10.0);
    }
    if (ImGui::Button("确定")) {
      if (mode == 0) {
        it8512.setCurrent(set_current);
      } else {
        psw.setVoltage(set_voltage);
      }
    }
    if (mode == 1) {
      ImGui::DragFloat("OCV (V)", &ocv, 0.5, 0.0, 35.0);
    }
    ImGui::DragInt("扫描步数 ", &step, 10, 10, 50);
    ImGui::DragFloat("扫描步长 (s)", &step_time, 0.5, 0.5, 10.0);
    if (ImGui::Button("扫描") && ((progress > 0.999f) || (progress < 0.001f))) {
      std::thread th_sweep(sweep_ivp, &it8512, &psw, set_current, set_voltage,
                           ocv, step, step_time, mode, &progress, &time_ivp,
                           &voltage_ivp, &current_ivp, &power_ivp,
                           &hydrogen_ivp);
      th_sweep.detach();
    }
    // ImGui::PushStyleColor(ImGuiCol_FrameBg,
    //                       (ImVec4)ImColor::ImColor(252, 252, 252, 256));
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram,
                          (ImVec4)ImColor::ImColor(26, 115, 232, 256));
    ImGui::ProgressBar(progress, ImVec2(0.0f, 0.0f));
    ImGui::PopStyleColor();
    ImGui::End();

    ImGui::Begin("测试结果");
    float smallest_c_ivp = 0;
    float biggest_c_ivp = 10;
    if (current_ivp.size() > 0) {
      smallest_c_ivp =
          *std::min_element(current_ivp.begin(), current_ivp.end()) - 0.03;
      biggest_c_ivp =
          *std::max_element(current_ivp.begin(), current_ivp.end()) + 0.03;
    }
    float smallest_v_ivp = 0;
    float biggest_v_ivp = 35;
    if (voltage_ivp.size() > 0) {
      smallest_v_ivp =
          *std::min_element(voltage_ivp.begin(), voltage_ivp.end()) - 0.03;
      biggest_v_ivp =
          *std::max_element(voltage_ivp.begin(), voltage_ivp.end()) + 0.03;
    }
    float smallest_p_ivp = 0;
    float biggest_p_ivp = 150;
    if (power_ivp.size() > 0) {
      smallest_p_ivp =
          *std::min_element(power_ivp.begin(), power_ivp.end()) - 0.03;
      biggest_p_ivp =
          *std::max_element(power_ivp.begin(), power_ivp.end()) + 0.03;
    }
    ImPlot::SetNextPlotLimits(smallest_c_ivp, biggest_c_ivp, smallest_v_ivp,
                              biggest_v_ivp, ImGuiCond_Always);
    ImPlot::SetNextPlotLimitsY(smallest_p_ivp, biggest_p_ivp, ImGuiCond_Always,
                               1);
    if (ImPlot::BeginPlot("IVP曲线", "电流 (A)", "电压 (V)", ImVec2(-1, -1),
                          ImPlotFlags_NoTitle | ImPlotFlags_YAxis2,
                          ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit,
                          ImPlotAxisFlags_NoGridLines,
                          ImPlotAxisFlags_NoGridLines, "功率 (W)")) {
      ImPlot::PlotLine("电压", current_ivp.data(), voltage_ivp.data(),
                       current_ivp.size());
      ImPlot::SetPlotYAxis(ImPlotYAxis_2);
      ImPlot::PlotLine("功率", current_ivp.data(), power_ivp.data(),
                       current_ivp.size());
      ImPlot::EndPlot();
    }
    ImGui::End();

    // ImGui::ShowDemoWindow();
    // ImPlot::ShowDemoWindow();
    ImGui::Begin("电压");
    ImPlot::SetNextPlotLimits(0, last_time, smallest_v, biggest_v,
                              ImGuiCond_Always);
    if (ImPlot::BeginPlot("电压", "时间 (s)", "电压 (V)", ImVec2(-1, -1),
                          ImPlotFlags_NoTitle | ImPlotFlags_NoLegend,
                          ImPlotAxisFlags_None, ImPlotAxisFlags_None)) {
      ImPlot::PushStyleColor(ImPlotCol_Line, ImPlot::GetColormapColor(0));
      ImPlot::PlotLine("电压", time.data(), voltage.data(), time.size());
      ImPlot::PopStyleColor();
      ImPlot::EndPlot();
    }
    ImGui::End();

    ImGui::Begin("电流");

    ImPlot::SetNextPlotLimits(0, last_time, smallest_c, biggest_c,
                              ImGuiCond_Always);
    if (ImPlot::BeginPlot("电流", "时间 (s)", "电流 (A)", ImVec2(-1, -1),
                          ImPlotFlags_NoTitle | ImPlotFlags_NoLegend,
                          ImPlotAxisFlags_None, ImPlotAxisFlags_None)) {
      ImPlot::PushStyleColor(ImPlotCol_Line, ImPlot::GetColormapColor(4));
      ImPlot::PlotLine("电流", time.data(), current.data(), time.size());
      ImPlot::PopStyleColor();
      ImPlot::EndPlot();
    }
    ImGui::End();

    ImGui::Begin("功率");
    ImPlot::SetNextPlotLimits(0, last_time, smallest_p, biggest_p,
                              ImGuiCond_Always);
    if (ImPlot::BeginPlot("功率", "时间 (s)", "功率 (W)", ImVec2(-1, -1),
                          ImPlotFlags_NoTitle | ImPlotFlags_NoLegend,
                          ImPlotAxisFlags_None, ImPlotAxisFlags_None)) {
      ImPlot::PushStyleColor(ImPlotCol_Line, ImPlot::GetColormapColor(1));
      ImPlot::PlotLine("功率", time.data(), power.data(), time.size());
      ImPlot::PopStyleColor();
      ImPlot::EndPlot();
    }
    ImGui::End();

    if (mode == 1) {
      ImGui::Begin("产氢率");

      ImPlot::SetNextPlotLimits(0, last_time, smallest_h, biggest_h,
                                ImGuiCond_Always);
      if (ImPlot::BeginPlot("产氢率", "时间 (s)", "产氢率 (NL/h)",
                            ImVec2(-1, -1),
                            ImPlotFlags_NoTitle | ImPlotFlags_NoLegend,
                            ImPlotAxisFlags_None, ImPlotAxisFlags_None)) {
        ImPlot::PushStyleColor(ImPlotCol_Line, ImPlot::GetColormapColor(2));
        ImPlot::PlotLine("产氢率", time.data(), hydrogen.data(), time.size());
        ImPlot::PopStyleColor();
        ImPlot::EndPlot();
      }
      ImGui::End();
    }

    // Rendering
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    const bool is_minimized =
        (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
    if (!is_minimized) {
      wd->ClearValue.color.float32[0] = clear_color.x * clear_color.w;
      wd->ClearValue.color.float32[1] = clear_color.y * clear_color.w;
      wd->ClearValue.color.float32[2] = clear_color.z * clear_color.w;
      wd->ClearValue.color.float32[3] = clear_color.w;
      FrameRender(wd, draw_data);
      FramePresent(wd);
    }
  }

  // Cleanup
  ImPlot::PopColormap();
  fclose(fp);
  err = vkDeviceWaitIdle(g_Device);
  check_vk_result(err);
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImPlot::DestroyContext();
  ImGui::DestroyContext();

  CleanupVulkanWindow();
  CleanupVulkan();
  stbi_image_free(images[0].pixels);
  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
