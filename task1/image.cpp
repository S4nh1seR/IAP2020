#include "image.h"

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
    const uint8_t Y = ((redWeight * R + greenWeight * G + blueWeight * B) >> denominatorBitsNumber);
    return CGrayValue{Y};
}

std::shared_ptr<CGrayImage> ConvertRGBImageToGray(const CRGBImage& colorImage) {
    const size_t width = colorImage.GetWidth();
    const size_t height = colorImage.GetHeight();
    std::shared_ptr<CGrayImage> grayImage(new CGrayImage(height, width));
    auto toFillBuffer = grayImage->GetBuffer();
    auto toReadBuffer = colorImage.GetBuffer();
    for (size_t pixelNumber = 0; pixelNumber < width * height; ++pixelNumber) {
        toFillBuffer[pixelNumber] = colorTransform(toReadBuffer[pixelNumber]);
    }
    return grayImage;
}