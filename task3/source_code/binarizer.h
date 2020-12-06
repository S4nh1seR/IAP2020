#pragma once

#include "image.h"

enum TBinarizationMode {
    BM_Avg,
    BM_Center,
    BM_CenterMinWeighted,
    BM_AvgCenterWeighted,
    BM_BySeparatedNoiseLevels
};

TBinarizationMode ChooseMode(const std::string& modeName);

class CPyramidBinarizer {
public:
    static constexpr uint8_t NoiseLevel = 40;
    static constexpr float SigmaMultiplier = 3.0f;
    static constexpr TBinarizationMode DefaultMode = BM_Center;

    CPyramidBinarizer(const CGrayImage& grayImage, TBinarizationMode mode = DefaultMode,
                      uint8_t noiseLevel = NoiseLevel, float noiseSigmaMultiplier = SigmaMultiplier);
    ~CPyramidBinarizer();

    std::shared_ptr<CBWImage> Binarize();

private:
    // Режим работы механизма
    TBinarizationMode mode;
    // Размеры исходного изображения
    const size_t width;
    const size_t height;
    // Исходное серое изображение
    const CGrayImage& srcGrayImage;
    // Глубина выстраевамой пирамиды
    const size_t depth;
    // Шумовой порог
    const uint8_t noiseLevel;
    // Для режима BM_BySeparatedNoiseLevels - коэф-т для шумового порога
    const float noiseSigmaMultiplier;
    // Расширенное изображение
    CGrayImage extendedImage;
    // Пирамиды минимумов, максимумов, средних значений
    CGrayImage* minPyramid;
    CGrayImage* maxPyramid;
    CGrayImage* avgPyramid;
    // Карты порогов (текущий и предыдущий шаг построения)
    uint8_t* prevThresMap{nullptr};
    uint8_t* currThresMap{nullptr};

    static constexpr size_t binsNumber = 16;
    static constexpr size_t valuesPerBin = 256 / binsNumber;

    uint64_t numberPerBin[binsNumber];
    uint64_t varSum[binsNumber];

    void prepareDeviationStats();
    void prepareExtended();
    void preparePyramids();
    void buildThresholdMap();
    void upsampleThresholdMap(size_t currMapWidth, size_t currMapHeight);
};