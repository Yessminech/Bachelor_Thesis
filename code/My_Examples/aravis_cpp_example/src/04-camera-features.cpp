/* SPDX-License-Identifier: Unlicense */

/* Aravis header */
#include <arv.h>

/* Standard headers */
#include <iostream>
#include <stdexcept>

int main(int argc, char **argv)
{
    // Initialize Aravis
    g_type_init();

    // Initialize variables
    ArvCamera *camera = nullptr;
    GError *error = nullptr;

    try {
        // Connect to the first available camera
        camera = arv_camera_new(nullptr, &error);

        if (ARV_IS_CAMERA(camera)) {
            int width = 0;
            int height = 0;
            const char *pixel_format = nullptr;

            std::cout << "Found camera: " << arv_camera_get_model_name(camera, nullptr) << std::endl;

            // Retrieve generally mandatory features for transmitters
            if (!error) {
                width = arv_camera_get_integer(camera, "Width", &error);
            }
            if (!error) {
                height = arv_camera_get_integer(camera, "Height", &error);
            }
            if (!error) {
                pixel_format = arv_camera_get_string(camera, "PixelFormat", &error);
            }

            // Check for errors and print the details
            if (error == nullptr) {
                std::cout << "Width = " << width << std::endl;
                std::cout << "Height = " << height << std::endl;
                std::cout << "Pixel format = " << (pixel_format ? pixel_format : "Unknown") << std::endl;
            } else {
                throw std::runtime_error(error->message);
            }

            // Clean up the camera object
            g_clear_object(&camera);
        } else {
            throw std::runtime_error("Failed to connect to camera.");
        }
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

