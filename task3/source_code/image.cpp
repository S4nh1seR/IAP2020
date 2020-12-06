#include "image.h"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <algorithm>

////////////////////////////////////////////////////////////////////////////////////////////

template<TImageColor TColor>
void CImage<TColor>::LoadFromFile(const std::string& sourceFilePath) {
    static_assert(TColor == IC_Gray || TColor == IC_RGB);
    constexpr auto readMode = (TColor == IC_Gray) ? cv::IMREAD_GRAYSCALE : cv::IMREAD_COLOR;
    const cv::Mat cvImage = imread(sourceFilePath, readMode);
    if (dataBuffer != nullptr) {
        delete [] dataBuffer;
    }
    width = cvImage.cols;
    height = cvImage.rows;
    const size_t fullSize = width * height;
    dataBuffer = new TColorValue[fullSize];
    std::copy_n(cvImage.data, fullSize * ComponentsNumber, reinterpret_cast<decltype(cvImage.data)>(dataBuffer));
}

template<TImageColor TColor>
void CImage<TColor>::SaveToFile(const std::string& targetFilePath) const {
    static_assert(TColor == IC_Gray || TColor == IC_RGB);
    constexpr auto cvImageType = (TColor == IC_Gray) ? CV_8UC1 : CV_8UC3;
    const cv::Mat cvImage(height, width, cvImageType, dataBuffer);
    imwrite(targetFilePath, cvImage);
}

// Обозначение {a} - в системе счисления по основанию a
// RedWeight   = 0.299{10} ~ 0.010011001000110{2} с точностью 10^(-5) (1)
// GreenWeight = 0.587{10} ~ 0.100101100100011{2} с точностью 10^(-5) (2)
// BlueWeight  = 0.114{10} ~ 0.000111010010111{2} с точностью 10^(-5) (3)

// Будем вычислять значения серых пикселей в целочисленной арифметике
static constexpr size_t denominatorBitsNumber = 15;
static constexpr size_t denominator = (1 << denominatorBitsNumber);
// Целочисленные значения весов RGB-компонент при расчете Y-компоненты серого изображения
static constexpr size_t redWeight = 9798;    // denominator * 0.299{10 -> 2}
static constexpr size_t greenWeight = 19235; // denominator * 0.587{10 -> 2}
static constexpr size_t blueWeight = 3735;   // denominator * 0.114{10 -> 2}
// (Двоичные представления коэффициентов взяты в точности такие, как из комментариев (1-3) выше)

static inline CGrayValue colorTransform(const CRGBValue& colorValue) {
    const uint8_t R = colorValue.Components[RGBC_Red];
    const uint8_t G = colorValue.Components[RGBC_Green];
    const uint8_t B = colorValue.Components[RGBC_Blue];
    const uint8_t Y = ((redWeight * R + greenWeight * G + blueWeight * B + denominatorBitsNumber / 2) >> denominatorBitsNumber);
    return CGrayValue{Y};
}

void ConvertRGBImageToGray(const CRGBImage& colorImage, CGrayImage& grayImage) {
    const size_t width = colorImage.GetWidth();
    const size_t height = colorImage.GetHeight();
    assert(grayImage.GetHeight() == height);
    assert(grayImage.GetWidth() == width);
    auto toFillBuffer = grayImage.GetBuffer();
    auto toReadBuffer = colorImage.GetBuffer();
    for (size_t pixelNumber = 0; pixelNumber < width * height; ++pixelNumber) {
        toFillBuffer[pixelNumber] = colorTransform(toReadBuffer[pixelNumber]);
    }
}

std::shared_ptr<CGrayImage> ConvertRGBImageToGray(const CRGBImage& colorImage) {
    const size_t width = colorImage.GetWidth();
    const size_t height = colorImage.GetHeight();
    std::shared_ptr<CGrayImage> grayImage(new CGrayImage(height, width));
    ConvertRGBImageToGray(colorImage, *grayImage);
    return grayImage;
}

template class CImage<IC_Gray>;
template class CImage<IC_RGB>;