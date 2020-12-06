#include "image.h"
#include "binarizer.h"
#include <iostream>
#include <time.h>

int main(int argc, char* argv[]) {
    uint8_t noiseLevel = CPyramidBinarizer::NoiseLevel;
    float sigmaMultiplier = CPyramidBinarizer::SigmaMultiplier;
    TBinarizationMode mode = CPyramidBinarizer::DefaultMode;

    if (argc < 3 || argc > 5) {
        std::cerr << "Invalid number of arguments!" << std::endl;
    }
    std::string srcPath = argv[1];
    std::string resPath = argv[2];
    if (argc >= 4) {
        mode = ChooseMode(argv[3]);
    }
    if (argc == 5) {
        if (mode != BM_BySeparatedNoiseLevels) {
            try {
                noiseLevel = std::stoi(argv[4]);
            } catch(...) {
                std::cerr << "Invalid noiseLevel argument! Should be int." << std::endl;
            }
        } else {
            try {
                sigmaMultiplier = std::stof(argv[4]);
            } catch(...) {
                std::cerr << "Invalid sigma multiplier argument! Should be float." << std::endl;
            }
        }
    }

    const CRGBImage srcColorImage(srcPath);
    const size_t srcHeight = srcColorImage.GetHeight();
    const size_t srcWidth = srcColorImage.GetWidth();
    const size_t srcImageSize = srcWidth * srcHeight;

    CGrayImage srcGrayImage(srcHeight, srcWidth);
    ConvertRGBImageToGray(srcColorImage, srcGrayImage);

    const time_t binarizeTimeStart = clock();
    CPyramidBinarizer binarizer(srcGrayImage, mode, noiseLevel, sigmaMultiplier);
    std::shared_ptr<CBWImage> binarized = binarizer.Binarize();
    const time_t binarizeTimeEnd = clock();

    const auto binarizeTimeDiff = static_cast<double>(binarizeTimeEnd - binarizeTimeStart);
    const auto binarizeTimeInSeconds = binarizeTimeDiff / CLOCKS_PER_SEC;
    const auto binarizeRelativeTime = binarizeTimeInSeconds / srcImageSize / 1000;

    std::cout.precision(3);
    std::cout << "Binarize full time: " << binarizeTimeInSeconds << " seconds" << std::endl;
    std::cout << "Binarize relative time: " << binarizeRelativeTime << " msec/MP" << std::endl;
    binarized->SaveToFile(resPath);

    return 0;
}
