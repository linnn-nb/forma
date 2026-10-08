#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
struct TrackPresentation
{
    struct Height
    {
        const char* name;
        int pixels;
    };
    static const std::array<Height, 7>& heights()
    {
        static const std::array<Height, 7> values{{{"Micro", 32},
                                                   {"Mini", 64},
                                                   {"Small", 96},
                                                   {"Medium", 144},
                                                   {"Large", 224},
                                                   {"Jumbo", 384},
                                                   {"Extreme", 640}}};
        return values;
    }
    struct Colour
    {
        const char* name;
        const char* value;
    };
    static const std::array<Colour, 9>& colours()
    {
        static const std::array<Colour, 9> values{{{"默认", "default"},
                                                   {"青绿", "#4EBDB2"},
                                                   {"蓝", "#568FDB"},
                                                   {"紫", "#A77BD7"},
                                                   {"粉", "#D97CA0"},
                                                   {"红", "#D66C68"},
                                                   {"橙", "#D59957"},
                                                   {"黄", "#C7B85B"},
                                                   {"灰", "#84909F"}}};
        return values;
    }
    static int height(const Json& view, const std::string& id)
    {
        const auto& values = view.value("track_heights", Json::object());
        return values.value(id, view.value("row_height", 144));
    }
};
} // namespace ndaw::desktop
