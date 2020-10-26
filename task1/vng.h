#pragma once

#include "image.h"
#include <iostream>

// Направления градиента
enum TBestGradientDirection : unsigned char {
    BGD_North = 0,
    BGD_South,
    BGD_West,
    BGD_East,
    BGD_NorthWest,
    BGD_NorthEast,
    BGD_SouthWest,
    BGD_SouthEast,

    BGD_Count
};

// Алгоритм demosaicing-а "Variable Number of gradients"
class VNG {
public:
    explicit VNG(const CGrayImage& grayCFAImage);
    ~VNG();

    // Восстановление цветного изображения по CFA
    std::shared_ptr<CRGBImage> RecoverImage();

private:
    // Размер изображения
    const size_t height;
    const size_t width;
    // Буффер с данными серого CFA изображения
    const uint8_t* cfaBuffer;

    uint8_t* cfaExpanded;

    // Для удобной индексации кэшированных строк
    enum TLineOrder : unsigned char {
        LO_BeforePrev,
        LO_Prev,
        LO_Curr,
        LO_Next,
        LO_AfterNext,
        LO_Count
    };
    // Буфер-кэш "актуальных" строк изображения
    const uint8_t* cfaLines[LO_Count];
    // Текущая заполняемая линия восстанавливаемого изображения
    CRGBValue* currRecoveredLine{nullptr};

    // Для индексации градиентов через строку
    enum TLongGradientsOrder : unsigned char {
        LGO_Top = 0,
        LGO_Mid,
        LGO_Bot,
        LGO_Count
    };
    // Вертикальные градиенты (через строку, для текущих 5-ти рассматриваемых строк)
    uint8_t* verticalGradient[LGO_Count];
    // Горизонтальные градиенты (через пиксель)
    uint8_t* horizontalGradient[LGO_Count];
    // Диагональные градиенты через строку - направление от левого нижнего угла к правому верхнему (условно "правые-длинные")
    uint8_t* rightDiagonalLongGradient[LGO_Count];
    // Диагональные градиенты через строку - направление от правого нижнего угла к левому верхнему (условно "левые-длинные")
    uint8_t* leftDiagonalLongGradient[LGO_Count];

    // Для индексации градиентов подряд идущих строк
    enum TShortGradientsOrder : unsigned char {
        SGO_Top = 0,
        SGO_MidTop,
        SGO_MidBot,
        SGO_Bot,
        SGO_Count
    };
    // Диагональные градиенты соседних строк для зеленого цвета (условно "правые/левые-короткие")
    uint8_t* leftDiagonalShortGradient[SGO_Count];
    uint8_t* rightDiagonalShortGradient[SGO_Count];

    // Градиенты по всем направлениям в рассматриваемой точке
    uint16_t directionGradients[BGD_Count];

    void prepareExpandedImage();
    void calcVerticalGradient(const uint8_t* firstLine, const uint8_t* secondLine, TLongGradientsOrder gradType);
    void calcHorizontalGradient(const uint8_t* line, TLongGradientsOrder gradType);
    void calcDiagonalGradient(const uint8_t* firstLine, const uint8_t* secondLine, bool isShort, bool isLeft,
        size_t gradIndex);
    void calcDirectionGradientsForGreen(size_t columnIndex);
    void calcDirectionGradientsForNotGreen(size_t columnIndex);
    void calcNonDiagonalDirectionGradients(size_t columnIndex);
    uint32_t getGradientThreshold() const;
    void interpolateColorsForGreen(size_t columnIndex, uint32_t gradientThreshold, TRGBComponent horizontalOtherColor,
        TRGBComponent verticalOtherColor);
    void interpolateColorsForNotGreen(size_t columnIndex, uint32_t gradientThreshold, TRGBComponent centralColor,
        TRGBComponent otherNotGreenColor);
    void updateGradients();
    void moveCache();
};

// Метрики качества восстановленного изображения
struct CMetrics {
    // Среднеквадратичная ошибка
    const double MSE;
    // Peak signal-to-noize ratio
    const double PSNR;
    CMetrics(float mse, float psnr) : MSE(mse), PSNR(psnr) {}
};

// Подсчет метрик
CMetrics CalculateMetrics(const CRGBImage& recoveredImage, const CRGBImage& referenceImage);

CMetrics CalculateCuttedMetrics(const CRGBImage& recoveredImage, const CRGBImage& referenceImage);