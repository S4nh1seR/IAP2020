#include <iostream>
#include <time.h>
#include "image.h"
#include "fractal.h"

int main(int argc, char* argv[]) {
    if (argc < 3 || argc > 5) {
        std::cerr << "Invalid number of arguments!" << std::endl;
    }
    const std::string srcImagePath(argv[1]);
    const std::string dstBinPath(argv[2]);

    size_t rBlockSize = 4;
    if (argc >= 4) {
        try {
            rBlockSize = std::stoi(argv[3]);
        } catch(...) {
            std::cerr << "Invalid third argument! Should define R block size (4 or 8 allowed).";
        }
    }
    bool isFastModeEnabled = false;
    if (argc == 5) {
        if (std::string(argv[4]) != "FastMode") {
            std::cerr << "Invalid fourth argument! Should be \"FastMode\" for fast mode or not provided for default mode.";
        }
        isFastModeEnabled = true;
    }

    CGrayImage gray(srcImagePath);
    const time_t encodeStart = clock();
    CFractalImageCompressor encoder(gray, rBlockSize, isFastModeEnabled);
    encoder.Compress(dstBinPath);
    const time_t encodeEnd = clock();

    const auto encodeTimeDiff = static_cast<double>(encodeEnd - encodeStart);
    const auto encodeTimeInSeconds = encodeTimeDiff / CLOCKS_PER_SEC;
    const auto encodeRelativeTime = encodeTimeInSeconds / (size * size) / 1000;
    std::cout.precision(3);
    std::cout << "Encode full time: " << encodeTimeInSeconds << " seconds" << std::endl;
    std::cout << "Encode relative time: " << encodeRelativeTime << " msec/MP" << std::endl;

    return 0;
}
