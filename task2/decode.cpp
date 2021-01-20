#include <iostream>
#include <time.h>
#include <fstream>

#include "image.h"
#include "fractal.h"

int main(int argc, char* argv[]) {
    if (argc < 3 || argc > 6) {
        std::cerr << "Invalid number of arguments!" << std::endl;
    }
    const std::string encodedBinaryPath(argv[1]);
    const std::string resultImagePath = std::string(argv[2]) + ".bmp";
    const std::string resultMetricPath = std::string(argv[2]) + ".txt";

    CGrayImage gray;
    if (argc >= 4) {
        gray.LoadFromFile(argv[3]);
    }

    std::string resultsFolder;
    if (argc >= 5) {
        resultsFolder = std::string(argv[4]);
    }

    size_t decodeIterationsNumber = 8;
    if (argc == 6) {
        try {
            decodeIterationsNumber = std::stoi(argv[5]);
        } catch(...) {
            std::cerr << "Invalid fourth argument! Should define iterations number (8 by default).";
        }
    }

    const time_t decodeStart = clock();
    CFractalImageDecompressor decoder(encodedBinaryPath);
    std::shared_ptr<CGrayImage> retrieved = decoder.Decompress(decodeIterationsNumber, resultsFolder, gray);
    const time_t decodeEnd = clock();

    const auto decodeTimeDiff = static_cast<double>(decodeEnd - decodeStart);
    const auto decodeTimeInSeconds = decodeTimeDiff / CLOCKS_PER_SEC;
    const auto decodeRelativeTime = decodeTimeInSeconds / (size * size) / 1000;
    const auto decodeTimePerIteration = decodeTimeInSeconds / decodeIterationsNumber;
    const auto decodeRelativeTimePerIteration = decodeRelativeTime / decodeIterationsNumber;

    std::cout.precision(3);
    std::cout << "Decode full time: " << decodeTimeInSeconds << " seconds" << std::endl;
    std::cout << "Decode relative time: " << decodeRelativeTime << " msec/MP" << std::endl;
    std::cout << "Decode one iteration time: " << decodeTimePerIteration << " seconds" << std::endl;
    std::cout << "Decode one iteration relative time: " << decodeRelativeTimePerIteration << " msec/MP" << std::endl;

    if (!gray.IsEmpty()) {
        const CMetrics& resultMetrics = CalculateMetrics(*retrieved, gray);
        std::cout << "MSE: " << resultMetrics.MSE << std::endl;
        std::cout << "PSNR: " << resultMetrics.PSNR << std::endl;
        std::ofstream out;
        out.open(resultMetricPath);
        out << resultMetrics.PSNR << std::endl;
        out.close();
    }
    retrieved->SaveToFile(resultImagePath);

    return 0;
}
