#pragma once

#include <string>

// Цвет изображения
enum TImageColor : unsigned char {
    // Полутоновое "серое" изображение
    IC_Gray = 0,
    // Изображение цветового пространства RGB
    IC_RGB,
    // Черно-белое изображение
    IC_BW,
    // Количество поддерживаемых типов изображения
    IC_Count
};

// Свойства конкретного типа
template<TImageColor TColor>
struct CColorProperties {
    static constexpr size_t ComponentsNumber = 1;
};

// Специализация для RGB
template<>
struct CColorProperties<IC_RGB> {
    static constexpr size_t ComponentsNumber = 3;
};

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

// В случае одной компоненты удобнее работать с uint8_t, чтобы не делать лишние reinterpret_cast-ы
template<TImageColor TColor>
using TImageBufferType = std::conditional_t<ComponentsNumber<TColor> == 1, uint8_t, CColorValue<TColor>>;

// Изображение произвольного цвета
template<TImageColor TColor>
class CImage {
public:
    typedef TImageBufferType<TColor> TColorValue;
    static constexpr auto ComponentsNumber = ::ComponentsNumber<TColor>;
    CImage() = default;
    // Создать изображение, считав его из файла на диске
    CImage(const std::string& sourceFilePath);
    // Создать пустое изображение нужного размера
    CImage(size_t height, size_t width);
    ~CImage();

    // Получение размеров изображения
    size_t GetWidth() const { return width; }
    size_t GetHeight() const { return height; }
    bool IsEmpty() const { return dataBuffer == nullptr; }
    // Получение/Установка значения конкретного пикселя
    TColorValue GetValue(size_t y, size_t x) const;
    void SetValue(size_t y, size_t x, TColorValue value);
    // Получение буфера изображения для заполнения
    TColorValue* GetBuffer() { return dataBuffer; }
    const TColorValue* GetBuffer() const { return dataBuffer; }
    // Сериализация изображения в/из файла
    void LoadFromFile(const std::string& sourceFilePath);
    void SaveToFile(const std::string& targetFilePath) const;

private:
    size_t height{0};
    size_t width{0};
    TColorValue* dataBuffer{nullptr};
};

// Для ЧБ-изображения сериализация реализована с помощью
// низкоуровневой C-библиотеки libtiff (OpenCV не умеет сохранять ЧБ в 1bit-depth формат)
template<> void CImage<IC_BW>::LoadFromFile(const std::string& sourceFilePath);
template<> void CImage<IC_BW>::SaveToFile(const std::string& sourceFilePath) const;

// Инстанцировать будем в cpp, это позволит отделить специализации,
// использующие разные библиотеки с пересечением имен сущностей
extern template class CImage<IC_Gray>;
extern template class CImage<IC_RGB>;
extern template class CImage<IC_BW>;
// alias-ы для типов изображений
typedef CImage<IC_Gray> CGrayImage;
typedef CImage<IC_RGB> CRGBImage;
typedef CImage<IC_BW> CBWImage;
// alias-ы для значений цвета изображения конкретного типа
typedef CGrayImage::TColorValue CGrayValue;
typedef CRGBImage::TColorValue CRGBValue;
typedef CBWImage::TColorValue CBWValue;

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
void ConvertRGBImageToGray(const CRGBImage& colorImage, CGrayImage& grayImage);

// Приведение значения в правильный диапазон
template<typename TReturnType = uint8_t>
inline TReturnType color_cast(int value, int min = 0, int max = 255) {
    return std::max(min, std::min(max, value));
}

//////////////////////////////////////////////////////////////////////////

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
    if (dataBuffer != nullptr) {
        delete[] dataBuffer;
    }
}

template<TImageColor TColor>
inline typename CImage<TColor>::TColorValue CImage<TColor>::GetValue(size_t y, size_t x) const {
    assert(dataBuffer != nullptr);
    return dataBuffer[y * width + x];
}

template<TImageColor TColor>
inline void CImage<TColor>::SetValue(size_t y, size_t x, TColorValue value) {
    assert(dataBuffer != nullptr);
    dataBuffer[y * width + x] = value;
}