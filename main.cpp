#include <vector>
#include <iostream>
#include <stdexcept>
#include <budImage.hpp>
#include <budOpenCL.hpp>
#include <budOpenGL.hpp>
#include <budVulkan.hpp>

int main()
{
    std::vector<bud::Imagef*> images;

    bud::cl::ImageCL imageCL(8, 8, 4);
    bud::gl::ImageGL imageGL(8, 8, 4);
    bud::vk::ImageVK imageVK(8, 8, 4);

    images.push_back(static_cast<bud::Imagef*>(&imageCL));
    images.push_back(static_cast<bud::Imagef*>(&imageGL));
    images.push_back(static_cast<bud::Imagef*>(&imageVK));

    try {
        for (const auto image : images) image->compute();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
}
