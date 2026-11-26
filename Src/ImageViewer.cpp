#include "Include/ImageViewer.hpp"
#include "ImGUI/backends/imgui_impl_vulkan.h"
#include <gdiplus.h>
#include <algorithm>
#include <cctype>
#include <cstring>



const int ImgZeroInt = 0;
const int ImgOneInt = 1;
const int ImgBytesPerPixel = 5 - 1;
const float ImgZeroFloat = 0;
const float ImgOneFloat = 1;
const float ImgHalfFloat = 0.5f;
const float ImgMinZoom = 0.1f;
const float ImgMaxZoom = 10;
const float ImgZoomStep = 1.25f;

// Initializes ImageViewer handles and GDI+ state.
ImageViewer::ImageViewer() {
    PhysicalDevice = VK_NULL_HANDLE;
    Device = VK_NULL_HANDLE;
    QueueFamily = static_cast<uint32_t>(ImgZeroInt);
    GraphicsQueue = VK_NULL_HANDLE;
    UploadCommandPool = VK_NULL_HANDLE;
    ImageHandle = VK_NULL_HANDLE;
    ImageMemory = VK_NULL_HANDLE;
    ImageView = VK_NULL_HANDLE;
    TextureDescriptor = VK_NULL_HANDLE;
    ImageWidth = ImgZeroInt;
    ImageHeight = ImgZeroInt;
    ZoomFactor = ImgOneFloat;
    PanOffset = ImVec2(ImgZeroFloat, ImgZeroFloat);
    GdiplusToken = static_cast<ULONG_PTR>(ImgZeroInt);
    GdiplusStarted = false;
}

// Releases GPU texture and GDI+ resources on destruction.
ImageViewer::~ImageViewer() {
    Shutdown();
}

// Returns whether FileName has a supported raster image extension.
bool ImageViewer::IsImageFile(const std::string& FileName) {
    size_t DotPos = FileName.find_last_of('.');

    if (DotPos == std::string::npos) {
        return false;
    }

    std::string Ext = FileName.substr(DotPos);
    std::transform(Ext.begin(), Ext.end(), Ext.begin(), [](unsigned char Ch) {
        return static_cast<char>(std::tolower(Ch));
    });

    return (Ext == ".png" || Ext == ".jpg" || Ext == ".jpeg" || Ext == ".bmp" || Ext == ".gif" || Ext == ".ico" || Ext == ".tif" || Ext == ".tiff");
}

// Stores Vulkan handles used to upload decoded image textures to the GPU.
void ImageViewer::SetVulkanContext(VkPhysicalDevice PhysDev, VkDevice LogDev, uint32_t QueueFam, VkQueue Queue) {
    PhysicalDevice = PhysDev;
    Device = LogDev;
    QueueFamily = QueueFam;
    GraphicsQueue = Queue;

    if (Device != VK_NULL_HANDLE && UploadCommandPool == VK_NULL_HANDLE) {
        VkCommandPoolCreateInfo PoolInfo = {};
        PoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        PoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        PoolInfo.queueFamilyIndex = QueueFamily;
        vkCreateCommandPool(Device, &PoolInfo, nullptr, &UploadCommandPool);
    }
}

// Finds a Vulkan memory type index matching TypeFilter and Properties.
uint32_t ImageViewer::FindMemoryType(uint32_t TypeFilter, VkMemoryPropertyFlags Properties) const {
    const uint32_t ZeroIdx = 0;
    const uint32_t OneBit = 1;

    VkPhysicalDeviceMemoryProperties MemProps = {};
    vkGetPhysicalDeviceMemoryProperties(PhysicalDevice, &MemProps);

    for (uint32_t Idx = ZeroIdx; Idx < MemProps.memoryTypeCount; ++Idx) {
        if ((TypeFilter & (OneBit << Idx)) != ZeroIdx && (MemProps.memoryTypes[Idx].propertyFlags & Properties) == Properties) {
            return Idx;
        }
    }

    return ZeroIdx;
}

// Releases the currently uploaded Vulkan texture resources safely.
void ImageViewer::ReleaseTexture() {
    if (Device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(Device);
    }

    if (TextureDescriptor != VK_NULL_HANDLE) {
        ImGui_ImplVulkan_RemoveTexture(TextureDescriptor);
        TextureDescriptor = VK_NULL_HANDLE;
    }

    if (ImageView != VK_NULL_HANDLE && Device != VK_NULL_HANDLE) {
        vkDestroyImageView(Device, ImageView, nullptr);
        ImageView = VK_NULL_HANDLE;
    }

    if (ImageHandle != VK_NULL_HANDLE && Device != VK_NULL_HANDLE) {
        vkDestroyImage(Device, ImageHandle, nullptr);
        ImageHandle = VK_NULL_HANDLE;
    }

    if (ImageMemory != VK_NULL_HANDLE && Device != VK_NULL_HANDLE) {
        vkFreeMemory(Device, ImageMemory, nullptr);
        ImageMemory = VK_NULL_HANDLE;
    }

    ImageWidth = ImgZeroInt;
    ImageHeight = ImgZeroInt;
    LoadedPath.clear();
    ErrorMessage.clear();
}

// Releases all Vulkan and GDI+ resources.
void ImageViewer::Shutdown() {
    ReleaseTexture();

    if (UploadCommandPool != VK_NULL_HANDLE && Device != VK_NULL_HANDLE) {
        vkDestroyCommandPool(Device, UploadCommandPool, nullptr);
        UploadCommandPool = VK_NULL_HANDLE;
    }

    if (GdiplusStarted) {
        Gdiplus::GdiplusShutdown(GdiplusToken);
        GdiplusStarted = false;
    }
}

// Uploads RGBA pixel data into a GPU-resident Vulkan VkImage and registers it with ImGui.
bool ImageViewer::UploadRgbaToVulkan(const std::vector<unsigned char>& Pixels, int Width, int Height) {
    const uint32_t SingleCount = 1;
    const uint32_t ZeroIndex = 0;
    const VkDeviceSize ZeroOffset = 0;

    if (Device == VK_NULL_HANDLE || PhysicalDevice == VK_NULL_HANDLE || GraphicsQueue == VK_NULL_HANDLE || UploadCommandPool == VK_NULL_HANDLE) {
        return false;
    }

    VkDeviceSize ImageSize = static_cast<VkDeviceSize>(Width) * static_cast<VkDeviceSize>(Height) * static_cast<VkDeviceSize>(ImgBytesPerPixel);

    // Creates host-visible staging buffer and copies RGBA pixels into it.
    VkBuffer StagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory StagingMemory = VK_NULL_HANDLE;
    VkBufferCreateInfo BufInfo = {};
    BufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    BufInfo.size = ImageSize;
    BufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    BufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(Device, &BufInfo, nullptr, &StagingBuffer) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements BufReq = {};
    vkGetBufferMemoryRequirements(Device, StagingBuffer, &BufReq);

    VkMemoryAllocateInfo BufAlloc = {};
    BufAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    BufAlloc.allocationSize = BufReq.size;
    BufAlloc.memoryTypeIndex = FindMemoryType(BufReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(Device, &BufAlloc, nullptr, &StagingMemory) != VK_SUCCESS) {
        vkDestroyBuffer(Device, StagingBuffer, nullptr);

        return false;
    }

    vkBindBufferMemory(Device, StagingBuffer, StagingMemory, ZeroOffset);

    void* MappedPtr = nullptr;
    vkMapMemory(Device, StagingMemory, ZeroOffset, ImageSize, ZeroIndex, &MappedPtr);
    memcpy(MappedPtr, Pixels.data(), static_cast<size_t>(ImageSize));
    vkUnmapMemory(Device, StagingMemory);

    // Creates device-local GPU VkImage.
    VkImageCreateInfo ImgInfo = {};
    ImgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ImgInfo.imageType = VK_IMAGE_TYPE_2D;
    ImgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    ImgInfo.extent.width = static_cast<uint32_t>(Width);
    ImgInfo.extent.height = static_cast<uint32_t>(Height);
    ImgInfo.extent.depth = SingleCount;
    ImgInfo.mipLevels = SingleCount;
    ImgInfo.arrayLayers = SingleCount;
    ImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    ImgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    ImgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ImgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(Device, &ImgInfo, nullptr, &ImageHandle) != VK_SUCCESS) {
        vkDestroyBuffer(Device, StagingBuffer, nullptr);
        vkFreeMemory(Device, StagingMemory, nullptr);

        return false;
    }

    VkMemoryRequirements ImgReq = {};
    vkGetImageMemoryRequirements(Device, ImageHandle, &ImgReq);

    VkMemoryAllocateInfo ImgAlloc = {};
    ImgAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ImgAlloc.allocationSize = ImgReq.size;
    ImgAlloc.memoryTypeIndex = FindMemoryType(ImgReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(Device, &ImgAlloc, nullptr, &ImageMemory) != VK_SUCCESS) {
        vkDestroyImage(Device, ImageHandle, nullptr);
        ImageHandle = VK_NULL_HANDLE;
        vkDestroyBuffer(Device, StagingBuffer, nullptr);
        vkFreeMemory(Device, StagingMemory, nullptr);

        return false;
    }

    vkBindImageMemory(Device, ImageHandle, ImageMemory, ZeroOffset);

    // Records and submits one-shot layout transition + staging copy command buffer.
    VkCommandBufferAllocateInfo CmdAlloc = {};
    CmdAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    CmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    CmdAlloc.commandPool = UploadCommandPool;
    CmdAlloc.commandBufferCount = SingleCount;

    VkCommandBuffer CmdBuf = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(Device, &CmdAlloc, &CmdBuf);

    VkCommandBufferBeginInfo BeginInfo = {};
    BeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    BeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(CmdBuf, &BeginInfo);

    VkImageMemoryBarrier ToTransfer = {};
    ToTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    ToTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ToTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    ToTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    ToTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    ToTransfer.image = ImageHandle;
    ToTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ToTransfer.subresourceRange.baseMipLevel = ZeroIndex;
    ToTransfer.subresourceRange.levelCount = SingleCount;
    ToTransfer.subresourceRange.baseArrayLayer = ZeroIndex;
    ToTransfer.subresourceRange.layerCount = SingleCount;
    ToTransfer.srcAccessMask = ZeroIndex;
    ToTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(CmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, ZeroIndex, ZeroIndex, nullptr, ZeroIndex, nullptr, SingleCount, &ToTransfer);

    VkBufferImageCopy CopyRegion = {};
    CopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    CopyRegion.imageSubresource.mipLevel = ZeroIndex;
    CopyRegion.imageSubresource.baseArrayLayer = ZeroIndex;
    CopyRegion.imageSubresource.layerCount = SingleCount;
    CopyRegion.imageExtent = { static_cast<uint32_t>(Width), static_cast<uint32_t>(Height), SingleCount };

    vkCmdCopyBufferToImage(CmdBuf, StagingBuffer, ImageHandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, SingleCount, &CopyRegion);

    VkImageMemoryBarrier ToShader = {};
    ToShader.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    ToShader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    ToShader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    ToShader.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    ToShader.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    ToShader.image = ImageHandle;
    ToShader.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ToShader.subresourceRange.baseMipLevel = ZeroIndex;
    ToShader.subresourceRange.levelCount = SingleCount;
    ToShader.subresourceRange.baseArrayLayer = ZeroIndex;
    ToShader.subresourceRange.layerCount = SingleCount;
    ToShader.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    ToShader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(CmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, ZeroIndex, ZeroIndex, nullptr, ZeroIndex, nullptr, SingleCount, &ToShader);
    vkEndCommandBuffer(CmdBuf);

    VkSubmitInfo SubmitInfo = {};
    SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfo.commandBufferCount = SingleCount;
    SubmitInfo.pCommandBuffers = &CmdBuf;
    vkQueueSubmit(GraphicsQueue, SingleCount, &SubmitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(GraphicsQueue);

    vkFreeCommandBuffers(Device, UploadCommandPool, SingleCount, &CmdBuf);
    vkDestroyBuffer(Device, StagingBuffer, nullptr);
    vkFreeMemory(Device, StagingMemory, nullptr);

    // Creates VkImageView and registers it with ImGui's Vulkan backend.
    VkImageViewCreateInfo ViewInfo = {};
    ViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ViewInfo.image = ImageHandle;
    ViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ViewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    ViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ViewInfo.subresourceRange.baseMipLevel = ZeroIndex;
    ViewInfo.subresourceRange.levelCount = SingleCount;
    ViewInfo.subresourceRange.baseArrayLayer = ZeroIndex;
    ViewInfo.subresourceRange.layerCount = SingleCount;

    if (vkCreateImageView(Device, &ViewInfo, nullptr, &ImageView) != VK_SUCCESS) {
        ReleaseTexture();

        return false;
    }

    TextureDescriptor = ImGui_ImplVulkan_AddTexture(ImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    ImageWidth = Width;
    ImageHeight = Height;

    return (TextureDescriptor != VK_NULL_HANDLE);
}

// Decodes an image file on disk using GDI+ and uploads it as a GPU Vulkan texture.
bool ImageViewer::LoadImage(const std::string& FilePath) {
    const int UnknownLength = -1;
    const int RedOffset = 1 + 1;
    const int GreenOffset = 1;
    const int BlueOffset = 0;
    const int AlphaOffset = 3;

    if (FilePath == LoadedPath && TextureDescriptor != VK_NULL_HANDLE) {
        return true;
    }

    ReleaseTexture();

    if (!GdiplusStarted) {
        Gdiplus::GdiplusStartupInput StartupInput;

        if (Gdiplus::GdiplusStartup(&GdiplusToken, &StartupInput, nullptr) == Gdiplus::Ok) {
            GdiplusStarted = true;
        }
    }

    if (!GdiplusStarted) {
        ErrorMessage = "Failed to initialize GDI+ image decoder.";

        return false;
    }

    int WideLen = MultiByteToWideChar(CP_UTF8, ImgZeroInt, FilePath.c_str(), UnknownLength, nullptr, ImgZeroInt);

    if (WideLen <= ImgOneInt) {
        ErrorMessage = "Invalid image path.";

        return false;
    }

    std::wstring WidePath(static_cast<size_t>(WideLen), L'\0');
    MultiByteToWideChar(CP_UTF8, ImgZeroInt, FilePath.c_str(), UnknownLength, &WidePath[static_cast<size_t>(ImgZeroInt)], WideLen);

    Gdiplus::Bitmap SourceBmp(WidePath.c_str());

    if (SourceBmp.GetLastStatus() != Gdiplus::Ok) {
        ErrorMessage = "Unable to decode image file.";

        return false;
    }

    int Width = static_cast<int>(SourceBmp.GetWidth());
    int Height = static_cast<int>(SourceBmp.GetHeight());

    if (Width <= ImgZeroInt || Height <= ImgZeroInt) {
        ErrorMessage = "Image has empty dimensions.";

        return false;
    }

    Gdiplus::Rect LockRect(ImgZeroInt, ImgZeroInt, Width, Height);
    Gdiplus::BitmapData BmpData = {};

    if (SourceBmp.LockBits(&LockRect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &BmpData) != Gdiplus::Ok) {
        ErrorMessage = "Failed to read image pixel data.";

        return false;
    }

    std::vector<unsigned char> RgbaPixels(static_cast<size_t>(Width) * static_cast<size_t>(Height) * static_cast<size_t>(ImgBytesPerPixel));
    const unsigned char* SrcBase = static_cast<const unsigned char*>(BmpData.Scan0);

    for (int Row = ImgZeroInt; Row < Height; ++Row) {
        const unsigned char* SrcRow = SrcBase + (Row * BmpData.Stride);
        unsigned char* DstRow = RgbaPixels.data() + (static_cast<size_t>(Row) * static_cast<size_t>(Width) * static_cast<size_t>(ImgBytesPerPixel));

        for (int Col = ImgZeroInt; Col < Width; ++Col) {
            int PixelOffset = Col * ImgBytesPerPixel;
            DstRow[PixelOffset + BlueOffset] = SrcRow[PixelOffset + RedOffset];
            DstRow[PixelOffset + GreenOffset] = SrcRow[PixelOffset + GreenOffset];
            DstRow[PixelOffset + RedOffset] = SrcRow[PixelOffset + BlueOffset];
            DstRow[PixelOffset + AlphaOffset] = SrcRow[PixelOffset + AlphaOffset];
        }
    }

    SourceBmp.UnlockBits(&BmpData);

    if (!UploadRgbaToVulkan(RgbaPixels, Width, Height)) {
        ErrorMessage = "Failed to upload image texture to GPU.";

        return false;
    }

    LoadedPath = FilePath;
    ZoomFactor = ImgOneFloat;
    PanOffset = ImVec2(ImgZeroFloat, ImgZeroFloat);

    return true;
}

// Renders the interactive image viewer with toolbar, checkerboard transparency, zoom, and pan.
void ImageViewer::Render(float Width, float Height) {
    const float ToolbarPadding = 9;
    const float CheckerSize = 15;
    const float PercentScale = 100;
    const float CanvasMargin = 15;
    const int CheckerLight = 55;
    const int CheckerDark = 35;
    const int FullByte = 255;
    const int EvenMod = 1 + 1;

    ImGui::BeginChild("ImageViewerRoot", ImVec2(Width, Height), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    if (ImGui::Button("Zoom In")) {
        ZoomFactor *= ImgZoomStep;

        if (ZoomFactor > ImgMaxZoom) {
            ZoomFactor = ImgMaxZoom;
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Zoom Out")) {
        ZoomFactor /= ImgZoomStep;

        if (ZoomFactor < ImgMinZoom) {
            ZoomFactor = ImgMinZoom;
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("100%")) {
        ZoomFactor = ImgOneFloat;
        PanOffset = ImVec2(ImgZeroFloat, ImgZeroFloat);
    }

    ImGui::SameLine();

    if (ImGui::Button("Fit")) {
        ImVec2 Avail = ImGui::GetContentRegionAvail();

        if (ImageWidth > ImgZeroInt && ImageHeight > ImgZeroInt && Avail.x > CanvasMargin && Avail.y > CanvasMargin) {
            float FitX = (Avail.x - CanvasMargin) / static_cast<float>(ImageWidth);
            float FitY = (Avail.y - CanvasMargin) / static_cast<float>(ImageHeight);
            ZoomFactor = (FitX < FitY) ? FitX : FitY;

            if (ZoomFactor > ImgOneFloat) {
                ZoomFactor = ImgOneFloat;
            }
            if (ZoomFactor < ImgMinZoom) {
                ZoomFactor = ImgMinZoom;
            }

            PanOffset = ImVec2(ImgZeroFloat, ImgZeroFloat);
        }
    }

    ImGui::SameLine();
    ImGui::Text("  %d x %d px  |  %d%%", ImageWidth, ImageHeight, static_cast<int>(ZoomFactor * PercentScale));
    ImGui::Separator();

    ImVec2 CanvasSize = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("ImageCanvas", CanvasSize, false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    if (!ErrorMessage.empty()) {
        ImGui::SetCursorPos(ImVec2(ToolbarPadding, ToolbarPadding));
        ImGui::TextWrapped("%s", ErrorMessage.c_str());
    } else if (TextureDescriptor != VK_NULL_HANDLE && ImageWidth > ImgZeroInt && ImageHeight > ImgZeroInt) {
        if (ImGui::IsWindowHovered()) {
            float Wheel = ImGui::GetIO().MouseWheel;

            if (Wheel > ImgZeroFloat) {
                ZoomFactor *= ImgZoomStep;
            } else if (Wheel < ImgZeroFloat) {
                ZoomFactor /= ImgZoomStep;
            }

            if (ZoomFactor < ImgMinZoom) {
                ZoomFactor = ImgMinZoom;
            }
            if (ZoomFactor > ImgMaxZoom) {
                ZoomFactor = ImgMaxZoom;
            }

            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                ImVec2 Delta = ImGui::GetIO().MouseDelta;
                PanOffset.x += Delta.x;
                PanOffset.y += Delta.y;
            }
        }

        float DrawW = static_cast<float>(ImageWidth) * ZoomFactor;
        float DrawH = static_cast<float>(ImageHeight) * ZoomFactor;
        ImVec2 Origin = ImGui::GetCursorScreenPos();
        float ImgX = Origin.x + ((CanvasSize.x - DrawW) * ImgHalfFloat) + PanOffset.x;
        float ImgY = Origin.y + ((CanvasSize.y - DrawH) * ImgHalfFloat) + PanOffset.y;

        ImDrawList* DrawList = ImGui::GetWindowDrawList();
        ImU32 LightCol = IM_COL32(CheckerLight, CheckerLight, CheckerLight, FullByte);
        ImU32 DarkCol = IM_COL32(CheckerDark, CheckerDark, CheckerDark, FullByte);

        // Draws checkerboard backdrop behind the image to visualize alpha transparency.
        for (float Py = ImgZeroFloat; Py < DrawH; Py += CheckerSize) {
            for (float Px = ImgZeroFloat; Px < DrawW; Px += CheckerSize) {
                int CellX = static_cast<int>(Px / CheckerSize);
                int CellY = static_cast<int>(Py / CheckerSize);
                bool IsEven = (((CellX + CellY) % EvenMod) == ImgZeroInt);
                float CellRight = (Px + CheckerSize < DrawW) ? (ImgX + Px + CheckerSize) : (ImgX + DrawW);
                float CellBottom = (Py + CheckerSize < DrawH) ? (ImgY + Py + CheckerSize) : (ImgY + DrawH);
                DrawList->AddRectFilled(ImVec2(ImgX + Px, ImgY + Py), ImVec2(CellRight, CellBottom), IsEven ? LightCol : DarkCol);
            }
        }

        DrawList->AddImage(reinterpret_cast<ImTextureID>(TextureDescriptor), ImVec2(ImgX, ImgY), ImVec2(ImgX + DrawW, ImgY + DrawH));
    }

    ImGui::EndChild();
    ImGui::EndChild();
}
