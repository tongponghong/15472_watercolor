#include <array>
#include <set>

// #include "imgui/imgui.h"
// #include "imgui/backends/imgui_impl_glfw.h"
// #include "imgui/backends/imgui_impl_vulkan.h"

template <typename T> 
concept arithmetic = std::integral<T> or std::floating_point<T>;

// ensures that the ranges are ranges of numbers and are compatible with sets, vectors, etc. 
template<std::ranges::input_range R>
    requires arithmetic<std::ranges::range_value_t<R>>
std::string range_to_string(const R &vals) {
    std::string new_string;
    for (auto v : vals) {
        if (!new_string.empty()) new_string += ", ";
        new_string += std::to_string(v);
    }
    return new_string;
}