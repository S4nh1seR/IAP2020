#pragma once

#include <string>

// Полутоновое изображение в памяти
class CGrayImage {
public:
    // Создать изображение, считав его из файла на диске
    CGrayImage(const std::string& sourceFilePath);
    // Создать пустое изображение нужного размера
    CGrayImage(size_t height = 0, size_t width = 0);
    ~CGrayImage();

    bool IsEmpty() const { return dataBuffer == nullptr; }
    // Получение размеров изображения
    size_t GetWidth() const { return width; }
    size_t GetHeight() const { return height; }
    // Получение/Установка значения конкретного пикселя
    uint8_t GetValue(size_t y, size_t x) const { return dataBuffer[y * width + x]; }
    void SetValue(size_t y, size_t x, uint8_t value) { dataBuffer[y * width + x] = value; }
    // Получение буфера изображения для заполнения
    uint8_t* GetBuffer() { return dataBuffer; }
    const uint8_t* GetBuffer() const { return dataBuffer; }
    // Сериализация изображения в/из файла
    bool LoadFromFile(const std::string& sourceFilePath);
    bool SaveToFile(const std::string& targetFilePath) const;

    void SwapImage(CGrayImage& other);

private:
    size_t height{0};
    size_t width{0};
    uint8_t* dataBuffer{nullptr};
};

//////////////////////////////////////////////////////////////////////////////////////////////////

// Метрики качества восстановленного изображения
struct CMetrics {
    // Среднеквадратичная ошибка
    const double MSE;
    // Peak signal-to-noize ratio
    const double PSNR;

    CMetrics(float mse, float psnr) : MSE(mse), PSNR(psnr) {}
    void SaveToFile(const std::string& pathToSave) const;
};

// Подсчет метрик
CMetrics CalculateMetrics(const CGrayImage& recoveredImage, const CGrayImage& referenceImage);

//////////////////////////////////////////////////////////////////////////////////////////////////

// Приведение значения в правильный диапазон
template<typename TReturnType = uint8_t>
inline TReturnType color_cast(int value, int min = 0, int max = 255) {
    return std::max(min, std::min(max, value));
}