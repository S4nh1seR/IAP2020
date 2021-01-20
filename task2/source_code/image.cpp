#include "image.h"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <cassert>
#include <algorithm>
#include <fstream>

////////////////////////////////////////////////////////////////////////////////////////////
// Реализация - запись(чтение) изображений в(из) файл(а) осуществляется с помощью OpenCV

CGrayImage::CGrayImage(size_t _height, size_t _width) :
    height(_height),
    width(_width)
{
    if (width * height != 0) {
        dataBuffer = new uint8_t[width * height];
    }
}

CGrayImage::CGrayImage(const std::string& sourceFilePath) {
    LoadFromFile(sourceFilePath);
}

CGrayImage::~CGrayImage() {
    delete [] dataBuffer;
}

void CGrayImage::LoadFromFile(const std::string& sourceFilePath) {
    const cv::Mat cvImage = imread(sourceFilePath, cv::IMREAD_GRAYSCALE);
    if (dataBuffer != nullptr) {
        delete [] dataBuffer;
    }
    width = cvImage.cols;
    height = cvImage.rows;
    const size_t fullSize = width * height;
    dataBuffer = new uint8_t[fullSize];
    std::copy_n(cvImage.data, fullSize, reinterpret_cast<decltype(cvImage.data)>(dataBuffer));
}

void CGrayImage::SaveToFile(const std::string& targetFilePath) const {
    const cv::Mat cvImage(height, width, CV_8UC1, dataBuffer);
    imwrite(targetFilePath, cvImage);
}

void CGrayImage::SwapImage(CGrayImage& other) {
    std::swap(height, other.height);
    std::swap(width, other.width);
    std::swap(dataBuffer, other.dataBuffer);
}

CMetrics CalculateMetrics(const CGrayImage& recoveredImage, const CGrayImage& referenceImage) {
    const size_t width = recoveredImage.GetWidth();
    const size_t height = recoveredImage.GetHeight();
    assert(width == referenceImage.GetWidth());
    assert(height == referenceImage.GetHeight());
    const size_t imageSize = width * height;
    double mse = 0.0;
    const auto recoveredBuffer = recoveredImage.GetBuffer();
    const auto referenceBuffer = referenceImage.GetBuffer();

    for (size_t pixelIndex = 0; pixelIndex < imageSize; ++pixelIndex) {
        mse += std::pow(recoveredBuffer[pixelIndex] - referenceBuffer[pixelIndex], 2) / imageSize;
    }
    const double psnr = 10 * std::log10(pow(255, 2) / mse);
    return CMetrics(mse, psnr);
}

void CMetrics::SaveToFile(const std::string& pathToSave) const {
    std::ofstream out;
    out.open(pathToSave);
    out << "MSE = " << MSE << std::endl;
    out << "PSNR = " << PSNR << std::endl;
    out.close();
}