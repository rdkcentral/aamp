#pragma once

#include <string>

namespace device {

class PixelResolution {
public:
    static const int k720x480 = 0;
    static const int k720x576 = 1;
    static const int k1280x720 = 2;
    static const int k1920x1080 = 3;
    static const int k3840x2160 = 4;
    static const int k4096x2160 = 5;
    
    int getId() const { return 0; }
};

class VideoResolution {
public:
    std::string getName() const { return "1920x1080"; }
    PixelResolution getPixelResolution() const { return PixelResolution(); }
};

}
