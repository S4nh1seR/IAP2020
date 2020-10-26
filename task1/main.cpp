#include <iostream>
#include "image.h"
#include "vng.h"
#include <time.h>

int main() {
    CGrayImage cfaGray("./source_images/CFA.bmp");
    VNG vng(cfaGray);

    time_t start = clock();
    std::shared_ptr<CRGBImage> recovered = vng.RecoverImage();
    time_t end = clock();

    recovered->SaveToFile("./recovered.bmp");

    CRGBImage reference("./source_images/Original.bmp");
    const CMetrics& metrics = CalculateMetrics(*recovered, reference);

    const double timeDiff = static_cast<double>( end - start );
    const double timeInSeconds = timeDiff / CLOCKS_PER_SEC;
    const size_t width = reference.GetWidth();
    const size_t height = reference.GetHeight();
    const double relativeTime = timeInSeconds / (width * height) / 1000;
    std::cout.precision(3);
    std::cout << "Full time: " << timeInSeconds << " seconds" << std::endl;
    std::cout << "Relative time: " << relativeTime << " msec/MP" << std::endl;
    std::cout << "MSE: " << metrics.MSE << std::endl;
    std::cout << "PSNR: " << metrics.PSNR << std::endl;

    return 0;
}
