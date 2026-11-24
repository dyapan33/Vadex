#define GLFW_EXPOSE_NATIVE_WIN32
#include <vulkan/vulkan.h>
#include "GLFW/Include/glfw3.h"
#include "GLFW/Include/glfw3native.h"
#include <windows.h>
#include <windowsx.h>
#include <uxtheme.h>
#include <iostream>
#include <vector>
#include <cstdlib>
#include "ImGUI/imgui.h"
#include "ImGUI/backends/imgui_impl_glfw.h"
#include "ImGUI/backends/imgui_impl_vulkan.h"
#include "Include/Editor.hpp"
#include "Include/Theme.hpp"



const int InvalidQueueFamilyIndex = -1;
const int DefaultMinImageCount = 1 + 1;

static VkAllocationCallbacks* GAllocator = nullptr;
static VkInstance GInstance = VK_NULL_HANDLE;
static VkPhysicalDevice GPhysicalDevice = VK_NULL_HANDLE;
static VkDevice GDevice = VK_NULL_HANDLE;
static uint32_t GQueueFamily = static_cast<uint32_t>(InvalidQueueFamilyIndex);
static VkQueue GQueue = VK_NULL_HANDLE;
static VkDescriptorPool GDescriptorPool = VK_NULL_HANDLE;
static VkPipelineCache GPipelineCache = VK_NULL_HANDLE;
static VkSurfaceKHR GSurface = VK_NULL_HANDLE;
static ImGui_ImplVulkanH_Window GMainWindowData;
static int GMinImageCount = DefaultMinImageCount;
static bool GSwapChainRebuild = false;

static WNDPROC GOriginalWndProc = nullptr;
static CodeEditor* GEditorInstance = nullptr;
static bool GIsMaximizeHovered = false;
static bool GIsMaximizePressed = false;

// Validates a Vulkan API result and aborts on fatal negative error codes.
static void CheckVulkanResult(VkResult Err) {
    const int SuccessCode = 0;

    if (Err == SuccessCode) {
        return;
    }

    std::cerr << "[Vulkan Error] VkResult = " << Err << std::endl;

    if (Err < SuccessCode) {
        abort();
    }
}

// Initializes the Vulkan instance, logical device, queue, and descriptor pool.
static void SetupVulkan(ImVector<const char*>& InstanceExtensions) {
    const uint32_t SingleCount = 1;
    const uint32_t ZeroIndex = 0;
    const uint32_t PoolCapacity = 15;
    const float FullPriority = 1;
    VkResult Err;

    VkInstanceCreateInfo InstanceInfo = {};
    InstanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    InstanceInfo.enabledExtensionCount = InstanceExtensions.Size;
    InstanceInfo.ppEnabledExtensionNames = InstanceExtensions.Data;
    Err = vkCreateInstance(&InstanceInfo, GAllocator, &GInstance);
    CheckVulkanResult(Err);

    GPhysicalDevice = ImGui_ImplVulkanH_SelectPhysicalDevice(GInstance);
    GQueueFamily = ImGui_ImplVulkanH_SelectQueueFamilyIndex(GPhysicalDevice);

    ImVector<const char*> DeviceExtensions;
    DeviceExtensions.push_back("VK_KHR_swapchain");

    const float QueuePriority[] = { FullPriority };
    VkDeviceQueueCreateInfo QueueInfo[SingleCount] = {};
    QueueInfo[ZeroIndex].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    QueueInfo[ZeroIndex].queueFamilyIndex = GQueueFamily;
    QueueInfo[ZeroIndex].queueCount = SingleCount;
    QueueInfo[ZeroIndex].pQueuePriorities = QueuePriority;

    VkDeviceCreateInfo DeviceInfo = {};
    DeviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    DeviceInfo.queueCreateInfoCount = SingleCount;
    DeviceInfo.pQueueCreateInfos = QueueInfo;
    DeviceInfo.enabledExtensionCount = DeviceExtensions.Size;
    DeviceInfo.ppEnabledExtensionNames = DeviceExtensions.Data;
    Err = vkCreateDevice(GPhysicalDevice, &DeviceInfo, GAllocator, &GDevice);
    CheckVulkanResult(Err);
    vkGetDeviceQueue(GDevice, GQueueFamily, ZeroIndex, &GQueue);

    VkDescriptorPoolSize PoolSizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, PoolCapacity }
    };
    VkDescriptorPoolCreateInfo PoolInfo = {};
    PoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    PoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    PoolInfo.maxSets = PoolCapacity;
    PoolInfo.poolSizeCount = static_cast<uint32_t>(IM_ARRAYSIZE(PoolSizes));
    PoolInfo.pPoolSizes = PoolSizes;
    Err = vkCreateDescriptorPool(GDevice, &PoolInfo, GAllocator, &GDescriptorPool);
    CheckVulkanResult(Err);
}

// Configures the Vulkan swapchain and render pass for the main window.
static void SetupVulkanWindow(ImGui_ImplVulkanH_Window* Wd, VkSurfaceKHR Surface, int Width, int Height) {
    const int ExitFailureCode = -1;
    const VkImageUsageFlags DefaultUsageFlags = 0;

    Wd->Surface = Surface;

    VkBool32 Res;
    vkGetPhysicalDeviceSurfaceSupportKHR(GPhysicalDevice, GQueueFamily, Wd->Surface, &Res);

    if (Res != VK_TRUE) {
        std::cerr << "Error: no WSI support on physical device" << std::endl;
        exit(ExitFailureCode);
    }

    const VkFormat RequestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
    const VkColorSpaceKHR RequestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    Wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(GPhysicalDevice, Wd->Surface, RequestSurfaceImageFormat, static_cast<int>(IM_ARRAYSIZE(RequestSurfaceImageFormat)), RequestSurfaceColorSpace);

    VkPresentModeKHR PresentModes[] = { VK_PRESENT_MODE_FIFO_KHR };
    Wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(GPhysicalDevice, Wd->Surface, PresentModes, IM_ARRAYSIZE(PresentModes));

    ImGui_ImplVulkanH_CreateOrResizeWindow(GInstance, GPhysicalDevice, GDevice, Wd, GQueueFamily, GAllocator, Width, Height, GMinImageCount, DefaultUsageFlags);
}

// Destroys Vulkan descriptor pool, surface, device, and instance resources.
static void CleanupVulkan() {
    vkDestroyDescriptorPool(GDevice, GDescriptorPool, GAllocator);

    if (GSurface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(GInstance, GSurface, GAllocator);
        GSurface = VK_NULL_HANDLE;
    }

    vkDestroyDevice(GDevice, GAllocator);
    vkDestroyInstance(GInstance, GAllocator);
}

// Destroys the Vulkan window swapchain resources.
static void CleanupVulkanWindow() {
    ImGui_ImplVulkanH_DestroyWindow(GInstance, GDevice, &GMainWindowData, GAllocator);
}

// Records and submits the Vulkan draw commands for a single frame.
static void FrameRender(ImGui_ImplVulkanH_Window* Wd, ImDrawData* DrawData) {
    const uint32_t SingleCount = 1;
    const VkCommandPoolResetFlags ZeroResetFlags = 0;
    VkResult Err;

    VkSemaphore ImageAcquiredSemaphore = Wd->FrameSemaphores[Wd->SemaphoreIndex].ImageAcquiredSemaphore;
    VkSemaphore RenderCompleteSemaphore = Wd->FrameSemaphores[Wd->SemaphoreIndex].RenderCompleteSemaphore;
    Err = vkAcquireNextImageKHR(GDevice, Wd->Swapchain, UINT64_MAX, ImageAcquiredSemaphore, VK_NULL_HANDLE, &Wd->FrameIndex);

    if (Err == VK_ERROR_OUT_OF_DATE_KHR || Err == VK_SUBOPTIMAL_KHR) {
        GSwapChainRebuild = true;

        return;
    }

    CheckVulkanResult(Err);

    ImGui_ImplVulkanH_Frame* Fd = &Wd->Frames[Wd->FrameIndex];

    {
        Err = vkWaitForFences(GDevice, SingleCount, &Fd->Fence, VK_TRUE, UINT64_MAX);
        CheckVulkanResult(Err);
        Err = vkResetFences(GDevice, SingleCount, &Fd->Fence);
        CheckVulkanResult(Err);
    }

    {
        Err = vkResetCommandPool(GDevice, Fd->CommandPool, ZeroResetFlags);
        CheckVulkanResult(Err);
        VkCommandBufferBeginInfo Info = {};
        Info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        Info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        Err = vkBeginCommandBuffer(Fd->CommandBuffer, &Info);
        CheckVulkanResult(Err);
    }

    {
        VkRenderPassBeginInfo Info = {};
        Info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        Info.renderPass = Wd->RenderPass;
        Info.framebuffer = Fd->Framebuffer;
        Info.renderArea.extent.width = Wd->Width;
        Info.renderArea.extent.height = Wd->Height;
        Info.clearValueCount = SingleCount;
        Info.pClearValues = &Wd->ClearValue;
        vkCmdBeginRenderPass(Fd->CommandBuffer, &Info, VK_SUBPASS_CONTENTS_INLINE);
    }

    ImGui_ImplVulkan_RenderDrawData(DrawData, Fd->CommandBuffer);
    vkCmdEndRenderPass(Fd->CommandBuffer);

    {
        VkPipelineStageFlags WaitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo Info = {};
        Info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        Info.waitSemaphoreCount = SingleCount;
        Info.pWaitSemaphores = &ImageAcquiredSemaphore;
        Info.pWaitDstStageMask = &WaitStage;
        Info.commandBufferCount = SingleCount;
        Info.pCommandBuffers = &Fd->CommandBuffer;
        Info.signalSemaphoreCount = SingleCount;
        Info.pSignalSemaphores = &RenderCompleteSemaphore;
        Err = vkEndCommandBuffer(Fd->CommandBuffer);
        CheckVulkanResult(Err);
        Err = vkQueueSubmit(GQueue, SingleCount, &Info, Fd->Fence);
        CheckVulkanResult(Err);
    }
}

// Presents the rendered swapchain image to the window surface.
static void FramePresent(ImGui_ImplVulkanH_Window* Wd) {
    const uint32_t SingleCount = 1;
    const uint32_t IndexIncrement = 1;

    if (GSwapChainRebuild) {
        return;
    }

    VkSemaphore RenderCompleteSemaphore = Wd->FrameSemaphores[Wd->SemaphoreIndex].RenderCompleteSemaphore;
    VkPresentInfoKHR Info = {};
    Info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    Info.waitSemaphoreCount = SingleCount;
    Info.pWaitSemaphores = &RenderCompleteSemaphore;
    Info.swapchainCount = SingleCount;
    Info.pSwapchains = &Wd->Swapchain;
    Info.pImageIndices = &Wd->FrameIndex;

    VkResult Err = vkQueuePresentKHR(GQueue, &Info);

    if (Err == VK_ERROR_OUT_OF_DATE_KHR || Err == VK_SUBOPTIMAL_KHR) {
        GSwapChainRebuild = true;

        return;
    }

    CheckVulkanResult(Err);
    Wd->SemaphoreIndex = (Wd->SemaphoreIndex + IndexIncrement) % Wd->SemaphoreCount;
}

// Checks whether a client-space point falls within a titlebar rectangle.
static bool IsPointInTitleBarRect(const POINT& Point, const TitleBarRect& Rect) {
    float PointX = static_cast<float>(Point.x);
    float PointY = static_cast<float>(Point.y);

    return (PointX >= Rect.Left && PointX < Rect.Right && PointY >= Rect.Top && PointY < Rect.Bottom);
}

// Handles custom Win32 borderless frame messages for native Windows 11 snap preview and snap layouts.
static LRESULT CALLBACK CustomWindowProc(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam) {
    const LRESULT ZeroResult = 0;
    const LRESULT HandledErase = 1;
    const LONG ZeroCoord = 0;
    const LONG SideResizeBorder = 7;
    const LONG TopResizeBorder = 5;
    const LONG MinWindowWidth = 400;
    const LONG MinWindowHeight = 300;
    const int FirstRectIndex = 0;

    if (Msg == WM_NCCALCSIZE && WParam == TRUE) {
        NCCALCSIZE_PARAMS* Params = reinterpret_cast<NCCALCSIZE_PARAMS*>(LParam);

        if (IsZoomed(Hwnd)) {
            int FrameX = GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
            int FrameY = GetSystemMetrics(SM_CYSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);

            Params->rgrc[FirstRectIndex].left += FrameX;
            Params->rgrc[FirstRectIndex].top += FrameY;
            Params->rgrc[FirstRectIndex].right -= FrameX;
            Params->rgrc[FirstRectIndex].bottom -= FrameY;

            HMONITOR ActiveMonitor = MonitorFromWindow(Hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO MonInfo = {};
            MonInfo.cbSize = sizeof(MONITORINFO);

            if (GetMonitorInfoW(ActiveMonitor, &MonInfo)) {
                if (Params->rgrc[FirstRectIndex].bottom > MonInfo.rcWork.bottom) {
                    Params->rgrc[FirstRectIndex].bottom = MonInfo.rcWork.bottom;
                }
                if (Params->rgrc[FirstRectIndex].right > MonInfo.rcWork.right) {
                    Params->rgrc[FirstRectIndex].right = MonInfo.rcWork.right;
                }
                if (Params->rgrc[FirstRectIndex].left < MonInfo.rcWork.left) {
                    Params->rgrc[FirstRectIndex].left = MonInfo.rcWork.left;
                }
                if (Params->rgrc[FirstRectIndex].top < MonInfo.rcWork.top) {
                    Params->rgrc[FirstRectIndex].top = MonInfo.rcWork.top;
                }
            }
        }

        return ZeroResult;
    }

    if (Msg == WM_NCHITTEST) {
        POINT ClientPoint = { GET_X_LPARAM(LParam), GET_Y_LPARAM(LParam) };
        ScreenToClient(Hwnd, &ClientPoint);

        RECT ClientRect = {};
        GetClientRect(Hwnd, &ClientRect);

        // Checks resize borders when the window is not maximized.
        if (!IsZoomed(Hwnd)) {
            bool OnLeft = (ClientPoint.x < SideResizeBorder);
            bool OnRight = (ClientPoint.x >= ClientRect.right - SideResizeBorder);
            bool OnTop = (ClientPoint.y < TopResizeBorder);
            bool OnBottom = (ClientPoint.y >= ClientRect.bottom - SideResizeBorder);

            if (OnTop && OnLeft) {
                return HTTOPLEFT;
            }
            if (OnTop && OnRight) {
                return HTTOPRIGHT;
            }
            if (OnBottom && OnLeft) {
                return HTBOTTOMLEFT;
            }
            if (OnBottom && OnRight) {
                return HTBOTTOMRIGHT;
            }
            if (OnLeft) {
                return HTLEFT;
            }
            if (OnRight) {
                return HTRIGHT;
            }
            if (OnTop) {
                return HTTOP;
            }
            if (OnBottom) {
                return HTBOTTOM;
            }
        }

        // Evaluates custom titlebar buttons and draggable caption area.
        if (GEditorInstance != nullptr) {
            const TitleBarLayout& Layout = GEditorInstance->GetTitleBarLayout();

            if (ClientPoint.y >= ZeroCoord && ClientPoint.y < static_cast<LONG>(Layout.TitleBarHeight)) {
                if (!Layout.IsPopupOpen && IsPointInTitleBarRect(ClientPoint, Layout.MaximizeRect)) {
                    return HTMAXBUTTON;
                }

                if (IsPointInTitleBarRect(ClientPoint, Layout.MinimizeRect) || IsPointInTitleBarRect(ClientPoint, Layout.CloseRect)) {
                    return HTCLIENT;
                }

                if (ClientPoint.x < static_cast<LONG>(Layout.MenuItemsEndX) || Layout.IsPopupOpen) {
                    return HTCLIENT;
                }

                return HTCAPTION;
            }
        }

        return HTCLIENT;
    }

    if (Msg == WM_NCMOUSEMOVE) {
        if (WParam == HTMAXBUTTON) {
            GIsMaximizeHovered = true;

            TRACKMOUSEEVENT TrackEvent = {};
            TrackEvent.cbSize = sizeof(TRACKMOUSEEVENT);
            TrackEvent.dwFlags = TME_LEAVE | TME_NONCLIENT;
            TrackEvent.hwndTrack = Hwnd;
            TrackMouseEvent(&TrackEvent);

            return ZeroResult;
        }

        GIsMaximizeHovered = false;
    }

    if (Msg == WM_NCMOUSELEAVE) {
        GIsMaximizeHovered = false;
        GIsMaximizePressed = false;
    }

    if (Msg == WM_MOUSEMOVE) {
        GIsMaximizeHovered = false;
    }

    if (Msg == WM_NCLBUTTONDOWN && WParam == HTMAXBUTTON) {
        GIsMaximizePressed = true;

        return ZeroResult;
    }

    if (Msg == WM_NCLBUTTONUP && WParam == HTMAXBUTTON) {
        if (GIsMaximizePressed) {
            GIsMaximizePressed = false;
            ShowWindow(Hwnd, IsZoomed(Hwnd) ? SW_RESTORE : SW_MAXIMIZE);
        }

        return ZeroResult;
    }

    if (Msg == WM_LBUTTONDOWN || Msg == WM_RBUTTONDOWN || Msg == WM_MBUTTONDOWN) {
        if (GetFocus() != Hwnd) {
            SetFocus(Hwnd);
        }
    }

    if (Msg == WM_NOTIFY && GEditorInstance != nullptr) {
        GEditorInstance->HandleNativeNotify(WParam, LParam);

        return ZeroResult;
    }

    if (Msg == WM_GETMINMAXINFO) {
        MINMAXINFO* MinMaxInfo = reinterpret_cast<MINMAXINFO*>(LParam);
        MinMaxInfo->ptMinTrackSize.x = MinWindowWidth;
        MinMaxInfo->ptMinTrackSize.y = MinWindowHeight;

        return ZeroResult;
    }

    if (Msg == WM_ERASEBKGND) {
        return HandledErase;
    }

    return CallWindowProcW(GOriginalWndProc, Hwnd, Msg, WParam, LParam);
}

// Configures native Win32 frame styles, DWM shadow, corner rounding, and WndProc subclassing.
static void ConfigureCustomFrame(HWND Hwnd) {
    const int ZeroVal = 0;
    const int TopFrameExtend = 1;
    const DWORD DarkModeAttr = 20;
    const BOOL DarkModeEnabled = TRUE;
    const DWORD CornerAttr = 33;
    const DWORD RoundPreference = 1 + 1;

    LONG_PTR WindowStyle = GetWindowLongPtrW(Hwnd, GWL_STYLE);
    WindowStyle |= WS_CAPTION | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU | WS_CLIPCHILDREN;
    WindowStyle &= ~WS_POPUP;
    SetWindowLongPtrW(Hwnd, GWL_STYLE, WindowStyle);

    HMODULE DwmModule = LoadLibraryA("dwmapi.dll");

    if (DwmModule != nullptr) {
        typedef HRESULT(WINAPI* PfnDwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);
        typedef HRESULT(WINAPI* PfnDwmExtendFrameIntoClientArea)(HWND, const MARGINS*);

        PfnDwmSetWindowAttribute SetAttr = reinterpret_cast<PfnDwmSetWindowAttribute>(GetProcAddress(DwmModule, "DwmSetWindowAttribute"));

        if (SetAttr != nullptr) {
            SetAttr(Hwnd, DarkModeAttr, &DarkModeEnabled, sizeof(DarkModeEnabled));
            SetAttr(Hwnd, CornerAttr, &RoundPreference, sizeof(RoundPreference));
        }

        PfnDwmExtendFrameIntoClientArea ExtendFrame = reinterpret_cast<PfnDwmExtendFrameIntoClientArea>(GetProcAddress(DwmModule, "DwmExtendFrameIntoClientArea"));

        if (ExtendFrame != nullptr) {
            MARGINS FrameMargins = { ZeroVal, ZeroVal, TopFrameExtend, ZeroVal };
            ExtendFrame(Hwnd, &FrameMargins);
        }

        FreeLibrary(DwmModule);
    }

    GOriginalWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(Hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(CustomWindowProc)));
    SetWindowPos(Hwnd, nullptr, ZeroVal, ZeroVal, ZeroVal, ZeroVal, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// Logs GLFW error messages to standard error.
static void GlfwErrorCallback(int Error, const char* Description) {
    std::cerr << "GLFW Error " << Error << ": " << Description << std::endl;
}

// Application entry point.
int main(int Argc, char** Argv) {
    const int ExitSuccessCode = 0;
    const int ExitFailureCode = 1;
    const int InitialWindowWidth = 1300;
    const int InitialWindowHeight = 730;
    const uint32_t ZeroUint = 0;
    const int ZeroInt = 0;
    const int ChannelRedIndex = 0;
    const int ChannelGreenIndex = 1;
    const int ChannelBlueIndex = 1 + 1;
    const int ChannelAlphaIndex = 3;
    const float ZeroFloat = 0;
    const float ClearRedGreen = 0.1f;
    const float ClearBlue = 0.11f;
    const float FullAlpha = 1;

    (void)Argc;
    (void)Argv;

    glfwSetErrorCallback(GlfwErrorCallback);

    if (!glfwInit()) {
        return ExitFailureCode;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    GLFWwindow* Window = glfwCreateWindow(InitialWindowWidth, InitialWindowHeight, "Vadex", nullptr, nullptr);

    if (Window == nullptr) {
        std::cerr << "GLFW: Failed to create window" << std::endl;
        glfwTerminate();

        return ExitFailureCode;
    }

    if (!glfwVulkanSupported()) {
        std::cerr << "GLFW: Vulkan Not Supported" << std::endl;
        glfwDestroyWindow(Window);
        glfwTerminate();

        return ExitFailureCode;
    }

    ImVector<const char*> Extensions;
    uint32_t ExtensionsCount = ZeroUint;
    const char** GlfwExtensions = glfwGetRequiredInstanceExtensions(&ExtensionsCount);

    for (uint32_t Index = ZeroUint; Index < ExtensionsCount; Index++) {
        Extensions.push_back(GlfwExtensions[Index]);
    }

    SetupVulkan(Extensions);

    VkResult Err = glfwCreateWindowSurface(GInstance, Window, GAllocator, &GSurface);
    CheckVulkanResult(Err);

    int Width = ZeroInt;
    int Height = ZeroInt;
    glfwGetFramebufferSize(Window, &Width, &Height);
    ImGui_ImplVulkanH_Window* Wd = &GMainWindowData;
    SetupVulkanWindow(Wd, GSurface, Width, Height);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // Pins imgui.ini inside the executable directory so opened workspaces stay clean.
    const DWORD ZeroDword = 0;
    char ExePathBuf[MaxFileSize] = {};
    static std::string GImGuiIniPath = "imgui.ini";

    if (GetModuleFileNameA(nullptr, ExePathBuf, MaxFileSize) > ZeroDword) {
        std::string ExeStr(ExePathBuf);
        size_t LastSlash = ExeStr.find_last_of("\\/");

        if (LastSlash != std::string::npos) {
            GImGuiIniPath = ExeStr.substr(ZeroInt, LastSlash) + "\\imgui.ini";
        }
    }

    ImGui::GetIO().IniFilename = GImGuiIniPath.c_str();
    EditorTheme::ApplyDarkTheme();

    ImGui_ImplGlfw_InitForVulkan(Window, true);
    ImGui_ImplVulkan_InitInfo InitInfo = {};
    InitInfo.Instance = GInstance;
    InitInfo.PhysicalDevice = GPhysicalDevice;
    InitInfo.Device = GDevice;
    InitInfo.QueueFamily = GQueueFamily;
    InitInfo.Queue = GQueue;
    InitInfo.PipelineCache = GPipelineCache;
    InitInfo.DescriptorPool = GDescriptorPool;
    InitInfo.MinImageCount = GMinImageCount;
    InitInfo.ImageCount = Wd->ImageCount;
    InitInfo.Allocator = GAllocator;
    InitInfo.PipelineInfoMain.RenderPass = Wd->RenderPass;
    InitInfo.PipelineInfoMain.Subpass = ZeroUint;
    InitInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    ImGui_ImplVulkan_Init(&InitInfo);

    CodeEditor EditorApp;
    EditorApp.Initialize(Window);
    GEditorInstance = &EditorApp;

    HWND Hwnd = glfwGetWin32Window(Window);

    if (Hwnd != nullptr) {
        ConfigureCustomFrame(Hwnd);
    }

    ImVec4 ClearColor = ImVec4(ClearRedGreen, ClearRedGreen, ClearBlue, FullAlpha);
    bool TileKeyPressed = false;

    const double IdleWaitSeconds = 0.05;

    while (!EditorApp.GetShouldExit()) {
        glfwWaitEventsTimeout(IdleWaitSeconds);

        if (glfwWindowShouldClose(Window)) {
            if (EditorApp.HasUnsavedChanges()) {
                glfwSetWindowShouldClose(Window, GLFW_FALSE);
                EditorApp.RequestExit();
            } else {
                break;
            }
        }

        EditorApp.SetMaximizeButtonState(GIsMaximizeHovered, GIsMaximizePressed);

        // Handles Ctrl+Alt+Arrow shortcuts for custom window tiling.
        bool IsCtrlAltHeld = (glfwGetKey(Window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS && glfwGetKey(Window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS);
        bool TileKeyLeft = (glfwGetKey(Window, GLFW_KEY_LEFT) == GLFW_PRESS);
        bool TileKeyRight = (glfwGetKey(Window, GLFW_KEY_RIGHT) == GLFW_PRESS);
        bool TileKeyUp = (glfwGetKey(Window, GLFW_KEY_UP) == GLFW_PRESS);
        bool TileKeyDown = (glfwGetKey(Window, GLFW_KEY_DOWN) == GLFW_PRESS);

        if (IsCtrlAltHeld && (TileKeyLeft || TileKeyRight || TileKeyUp || TileKeyDown)) {
            if (!TileKeyPressed) {
                TileKeyPressed = true;

                if (TileKeyUp) {
                    glfwMaximizeWindow(Window);
                } else if (TileKeyDown) {
                    glfwRestoreWindow(Window);
                } else if (TileKeyLeft) {
                    EditorApp.TileWindow(TileLeftHalf);
                } else if (TileKeyRight) {
                    EditorApp.TileWindow(TileRightHalf);
                }
            }
        } else {
            TileKeyPressed = false;
        }

        int FrameWidth = ZeroInt;
        int FrameHeight = ZeroInt;
        glfwGetFramebufferSize(Window, &FrameWidth, &FrameHeight);

        if (FrameWidth > ZeroInt && FrameHeight > ZeroInt && (GSwapChainRebuild || Wd->Width != FrameWidth || Wd->Height != FrameHeight)) {
            ImGui_ImplVulkan_SetMinImageCount(GMinImageCount);
            ImGui_ImplVulkanH_CreateOrResizeWindow(GInstance, GPhysicalDevice, GDevice, &GMainWindowData, GQueueFamily, GAllocator, FrameWidth, FrameHeight, GMinImageCount, ZeroUint);
            GMainWindowData.FrameIndex = ZeroUint;
            GSwapChainRebuild = false;
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        EditorApp.Render();

        ImGui::Render();
        ImDrawData* DrawData = ImGui::GetDrawData();
        const bool IsMinimized = (DrawData->DisplaySize.x <= ZeroFloat || DrawData->DisplaySize.y <= ZeroFloat);

        if (!IsMinimized) {
            Wd->ClearValue.color.float32[ChannelRedIndex] = ClearColor.x * ClearColor.w;
            Wd->ClearValue.color.float32[ChannelGreenIndex] = ClearColor.y * ClearColor.w;
            Wd->ClearValue.color.float32[ChannelBlueIndex] = ClearColor.z * ClearColor.w;
            Wd->ClearValue.color.float32[ChannelAlphaIndex] = ClearColor.w;
            FrameRender(Wd, DrawData);
            FramePresent(Wd);
        }
    }

    GEditorInstance = nullptr;

    Err = vkDeviceWaitIdle(GDevice);
    CheckVulkanResult(Err);

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    CleanupVulkanWindow();
    CleanupVulkan();

    glfwDestroyWindow(Window);
    glfwTerminate();

    return ExitSuccessCode;
}
