#pragma once
#include <cstdint>

namespace ac {

struct Vec3 {
    float x;
    float y;
    float z;
};

struct Vec4 {
    float x;
    float y;
    float z;
    float w;
};

struct StickVector {
    float x;
    float y;
};

enum class ControllerStyle : uint32_t {
    FullKey  = 1,
    Handheld = 2,
    JoyDual  = 4
};

} // namespace ac
