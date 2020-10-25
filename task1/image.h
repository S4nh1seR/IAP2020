// (Серое/Цветное) изображение в памяти
#pragma once

#include <string>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <cassert>
#include <algorithm>

// Цвет изображения
enum TImageColor : unsigned char {
    // Полутоновое "серое" изображение
    IC_Gray = 0,
    // Изображение цветового пространства RGB
    IC_RGB,
    // Количество поддерживаемых типов изображения
    IC_Count
};

// Свойства конкретного типа
template<TImageColor TColor>
struct CColorProperties;

// Специализация для серого
template<>
struct CColorProperties<IC_Gray> {
    static constexpr size_t ComponentsNumber = 1;
};

// Специализация для RGB
template<>
struct CColorProperties<IC_RGB> {
    static constexpr size_t ComponentsNumber = 3;
};
static_assert(IC_Count == 2);

// Шаблонная переменная для удобства
template<TImageColor TColor>
constexpr auto ComponentsNumber = CColorProperties<TColor>::ComponentsNumber;

// Значение конкретного цвета
template<TImageColor TColor>
struct CColorValue {
    uint8_t Components[ComponentsNumber<TColor>];
    uint8_t& operator[](size_t index) { return Components[index]; }
    const uint8_t& operator[](size_t index) const { return Components[index]; }
};

// Изображение произвольного цвета
template<TImageColor TColor>
class CImage {
public:
    typedef CColorValue<TColor> TColorValue;
    static constexpr std::underlying_type_t<TImageColor> Color = TColor;
    static constexpr auto ComponentsNumber = ::ComponentsNumber<TColor>;

    // Создать изображение, считав его из файла на диске
    CImage(const std::string& sourceFilePath);
    // Создать пустое изображение нужного размера
    CImage(size_t height, size_t width);
    ~CImage();

    // Получение размеров изображения
    size_t GetWidth() const { return width; }
    size_t GetHeight() const { return height; }
    // Получение/Установка значения конкретного пикселя
    TColorValue GetValue(size_t y, size_t x) const { return dataBuffer[y * width + x]; }
    void SetValue(size_t y, size_t x, TColorValue value) { dataBuffer[y * width + x] = value; }
    // Получение буфера изображения для заполнения
    TColorValue* GetBuffer() { return dataBuffer; }
    const TColorValue* GetBuffer() const { return dataBuffer; }
    // Сериализация изображения в/из файла
    bool LoadFromFile(const std::string& sourceFilePath);
    bool SaveToFile(const std::string& targetFilePath) const;

private:
    size_t height{0};
    size_t width{0};
    TColorValue* dataBuffer{nullptr};
};

// alias-ы для типов изображений
typedef CImage<IC_Gray> CGrayImage;
typedef CImage<IC_RGB> CRGBImage;
// alias-ы для значений цвета изображения конкретного типа
typedef CGrayImage::TColorValue CGrayValue;
typedef CRGBImage::TColorValue CRGBValue;

// Цветовые компоненты RGB пространства
// Чтобы не заморачиваться с перестановкой компонент, будем хранить в BGR порядке так же как в OpenCV
enum TRGBComponent : unsigned char {
    RGBC_Blue = 0,
    RGBC_Green,
    RGBC_Red,
    RGBC_Count
};

// Создание серого изображения по цветному
std::shared_ptr<CGrayImage> ConvertRGBImageToGray(const CRGBImage& colorImage);

////////////////////////////////////////////////////////////////////////////////////////////
// Реализация - запись(чтение) изображений в(из) файл(а) осуществляется с помощью OpenCV

template<TImageColor TColor>
CImage<TColor>::CImage(size_t _height, size_t _width) :
    height(_height),
    width(_width)
{
    dataBuffer = new TColorValue[width * height];
}

template<TImageColor TColor>
CImage<TColor>::CImage(const std::string& sourceFilePath) {
    LoadFromFile(sourceFilePath);
}

template<TImageColor TColor>
CImage<TColor>::~CImage() {
    delete [] dataBuffer;
}

template<TImageColor TColor>
bool CImage<TColor>::LoadFromFile(const std::string& sourceFilePath) {
    static_assert(IC_Count == 2);
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
bool CImage<TColor>::SaveToFile(const std::string& targetFilePath) const {
    static_assert(IC_Count == 2);
    constexpr auto cvImageType = (TColor == IC_Gray) ? CV_8UC1 : CV_8UC3;
    const cv::Mat cvImage(height, width, cvImageType, dataBuffer);
    imwrite(targetFilePath, cvImage);
}