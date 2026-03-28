#include "make_cubemap.hpp"
#include <chrono> 

int main(int argc, char** argv) {
    std::string src;
    std::string out;
    get_images(argc, argv, &src, &out);

    auto start = std::chrono::high_resolution_clock::now();

    output_lambertian_map(src, out);

    auto end = std::chrono::high_resolution_clock::now();;

    auto time_spent = std::chrono::duration_cast<std::chrono::seconds>(end - start);

    std::cout << "The cube utility took: " << time_spent << std::endl;

    return 1;
}