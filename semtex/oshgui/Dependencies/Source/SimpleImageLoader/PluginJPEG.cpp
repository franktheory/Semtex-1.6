#include "SimpleImageLoader.hpp"
#include "PluginJPEG.hpp"

namespace SimpleImageLoader
{
    bool PluginJPEG::IsValidFormat(const RawData &data)
    {
        if (data.size() < 3) return false;
        return data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF;
    }

    ImageData PluginJPEG::Load(Stream &data)
    {
        return ImageData();
    }
}
