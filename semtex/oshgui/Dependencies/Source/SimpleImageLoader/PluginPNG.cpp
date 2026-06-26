#include "SimpleImageLoader.hpp"
#include "PluginPNG.hpp"

namespace SimpleImageLoader
{
    bool PluginPNG::IsValidFormat(const RawData &data)
    {
        if (data.size() < 8) return false;
        return data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47;
    }

    ImageData PluginPNG::Load(Stream &data)
    {
        return ImageData();
    }
}
