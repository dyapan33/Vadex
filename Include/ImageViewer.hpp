#ifndef IMAGE_VIEWER_HPP
#define IMAGE_VIEWER_HPP
#include <windows.h>
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include "ImGUI/imgui.h"



// Displays raster images inside an editor tab using GDI+ decoding and a GPU Vulkan texture.
class ImageViewer {
private:
    VkPhysicalDevice PhysicalDevice;
    VkDevice Device;
    uint32_t QueueFamily;
    VkQueue GraphicsQueue;
    VkCommandPool UploadCommandPool;
    VkImage ImageHandle;
    VkDeviceMemory ImageMemory;
    VkImageView ImageView;
    VkDescriptorSet TextureDescriptor;
    int ImageWidth;
    int ImageHeight;
    float ZoomFactor;
    ImVec2 PanOffset;
    std::string LoadedPath;
    std::string ErrorMessage;
    ULONG_PTR GdiplusToken;
    bool GdiplusStarted;

    uint32_t FindMemoryType(uint32_t TypeFilter, VkMemoryPropertyFlags Properties) const;
    bool UploadRgbaToVulkan(const std::vector<unsigned char>& Pixels, int Width, int Height);

public:
    ImageViewer();
    ~ImageViewer();

    static bool IsImageFile(const std::string& FileName);
    void SetVulkanContext(VkPhysicalDevice PhysDev, VkDevice LogDev, uint32_t QueueFam, VkQueue Queue);
    bool LoadImage(const std::string& FilePath);
    void ReleaseTexture();
    void Shutdown();
    const std::string& GetLoadedPath() const { return LoadedPath; }
    void Render(float Width, float Height);
};

#endif
