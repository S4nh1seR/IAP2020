#include "vng.h"
#include <limits>

static inline uint8_t color_cast(int value) {
    return std::max(0, std::min(255, value));
}

VNG::VNG(const CGrayImage& grayCFAImage) :
        height(grayCFAImage.GetHeight()),
        width(grayCFAImage.GetWidth()),
        cfaBuffer(reinterpret_cast<const uint8_t*>(grayCFAImage.GetBuffer()))
{
    for (size_t longGradientIndex = 0; longGradientIndex < LGO_Count; ++longGradientIndex) {
        verticalGradient[longGradientIndex] = new uint8_t[width];
        horizontalGradient[longGradientIndex] = new uint8_t[width + 2];
        rightDiagonalLongGradient[longGradientIndex] = new uint8_t[width];
        leftDiagonalLongGradient[longGradientIndex] = new uint8_t[width];
        memset(verticalGradient[longGradientIndex], 0, width);
        memset(horizontalGradient[longGradientIndex], 0, width + 2);
        memset(rightDiagonalLongGradient[longGradientIndex], 0, width);
        memset(leftDiagonalLongGradient[longGradientIndex], 0, width);
    }
    for (size_t shortGradientIndex = 0; shortGradientIndex < SGO_Count; ++shortGradientIndex) {
        leftDiagonalShortGradient[shortGradientIndex] = new uint8_t[width];
        rightDiagonalShortGradient[shortGradientIndex] = new uint8_t[width];
        memset(leftDiagonalShortGradient[shortGradientIndex], 0, width);
        memset(rightDiagonalShortGradient[shortGradientIndex], 0, width);
    }
}

VNG::~VNG() {
    for (size_t longGradientIndex = 0; longGradientIndex < LGO_Count; ++longGradientIndex) {
        delete [] verticalGradient[longGradientIndex];
        delete [] horizontalGradient[longGradientIndex];
        delete [] rightDiagonalLongGradient[longGradientIndex];
        delete [] leftDiagonalLongGradient[longGradientIndex];
    }
    for (size_t shortGradientIndex = 0; shortGradientIndex < SGO_Count; ++shortGradientIndex) {
        delete [] leftDiagonalShortGradient[shortGradientIndex];
        delete [] rightDiagonalShortGradient[shortGradientIndex];
    }
}

std::shared_ptr<CRGBImage> VNG::RecoverImage() {
    std::shared_ptr<CRGBImage> recoveredImage(new CRGBImage(height, width));
    CRGBValue* recoveredBuffer = recoveredImage->GetBuffer();

    for (size_t lineIndex = 0; lineIndex < LO_Count; ++lineIndex) {
        cfaLines[lineIndex] = cfaBuffer + lineIndex * width;
    }

    calcVerticalGradient(cfaLines[LO_BeforePrev], cfaLines[LO_Curr], LGO_Top);
    calcVerticalGradient(cfaLines[LO_Prev], cfaLines[LO_Next], LGO_Mid);
    calcHorizontalGradient(cfaLines[LO_Prev], LGO_Top);
    calcHorizontalGradient(cfaLines[LO_Curr], LGO_Mid);

    const bool isShort = true;
    const bool isLeft = true;

    calcDiagonalGradient(cfaLines[LO_BeforePrev], cfaLines[LO_Curr], !isShort, !isLeft, LGO_Top);
    calcDiagonalGradient(cfaLines[LO_BeforePrev], cfaLines[LO_Curr], !isShort, isLeft, LGO_Top);
    calcDiagonalGradient(cfaLines[LO_Prev], cfaLines[LO_Next], !isShort, !isLeft, LGO_Mid);
    calcDiagonalGradient(cfaLines[LO_Prev], cfaLines[LO_Next], !isShort, isLeft, LGO_Mid);

    calcDiagonalGradient(cfaLines[LO_BeforePrev], cfaLines[LO_Prev], isShort, isLeft, SGO_Top);
    calcDiagonalGradient(cfaLines[LO_Prev], cfaLines[LO_Curr], isShort, isLeft, SGO_MidTop);
    calcDiagonalGradient(cfaLines[LO_Curr], cfaLines[LO_Next], isShort, isLeft, SGO_MidBot);
    calcDiagonalGradient(cfaLines[LO_BeforePrev], cfaLines[LO_Prev], isShort, !isLeft, SGO_Top);
    calcDiagonalGradient(cfaLines[LO_Prev], cfaLines[LO_Curr], isShort, !isLeft, SGO_MidTop);
    calcDiagonalGradient(cfaLines[LO_Curr], cfaLines[LO_Next], isShort, !isLeft, SGO_MidBot);

    currRecoveredLine = recoveredBuffer + width * 2;

    for (size_t rowIndex = 2; rowIndex < height - 2; ++rowIndex, currRecoveredLine += width) {
        const bool isRedGreenLine = (rowIndex % 2 == 0);
        updateGradients();

        const size_t greenOffset = isRedGreenLine ? 1 : 0;
        const size_t otherOffset = 1 - greenOffset;
        const TRGBComponent horizontalOtherColor = isRedGreenLine ? RGBC_Red : RGBC_Blue;
        const TRGBComponent verticalOtherColor = isRedGreenLine ? RGBC_Blue : RGBC_Red;

        for (size_t columnIndex = 2 + greenOffset; columnIndex < width - 2; columnIndex += 2) {
            calcDirectionGradientsForGreen(columnIndex);
            const uint32_t gradientThreshold = getGradientThreshold();
            interpolateColorsForGreen(columnIndex, gradientThreshold, horizontalOtherColor, verticalOtherColor);
        }
        for (size_t columnIndex = 2 + otherOffset; columnIndex < width - 2; columnIndex += 2) {
            calcDirectionGradientsForNotGreen(columnIndex);
            const uint32_t gradientThreshold = getGradientThreshold();
            interpolateColorsForNotGreen(columnIndex, gradientThreshold, horizontalOtherColor, verticalOtherColor);
        }
        moveCache();
    }
    return recoveredImage;
}

// Подсчитываем новые градиенты
void VNG::updateGradients() {
    calcVerticalGradient(cfaLines[LO_Curr], cfaLines[LO_AfterNext], LGO_Bot);
    calcHorizontalGradient(cfaLines[LO_Next], LGO_Bot);
    const bool isShort = true;
    const bool isLeft = true;
    calcDiagonalGradient(cfaLines[LO_Curr], cfaLines[LO_AfterNext], !isShort, isLeft, LGO_Bot);
    calcDiagonalGradient(cfaLines[LO_Curr], cfaLines[LO_AfterNext], !isShort, !isLeft, LGO_Bot);
    calcDiagonalGradient(cfaLines[LO_Next], cfaLines[LO_AfterNext], isShort, isLeft, SGO_Bot);
    calcDiagonalGradient(cfaLines[LO_Next], cfaLines[LO_AfterNext], isShort, !isLeft, SGO_Bot);
}

// Сдвиг кэша градиентов и строк на единицу
void VNG::moveCache() {
    for (size_t gradIndex = 0; gradIndex < LGO_Count - 1; ++gradIndex) {
        std::swap(verticalGradient[gradIndex], verticalGradient[gradIndex + 1]);
        std::swap(horizontalGradient[gradIndex], horizontalGradient[gradIndex + 1]);
        std::swap(rightDiagonalLongGradient[gradIndex], rightDiagonalLongGradient[gradIndex + 1]);
        std::swap(leftDiagonalLongGradient[gradIndex], leftDiagonalLongGradient[gradIndex + 1]);
    }

    for (size_t gradIndex = 0; gradIndex < SGO_Count - 1; ++gradIndex) {
        std::swap(leftDiagonalShortGradient[gradIndex], leftDiagonalShortGradient[gradIndex + 1]);
        std::swap(rightDiagonalShortGradient[gradIndex], rightDiagonalShortGradient[gradIndex + 1]);
    }

    for (size_t lineIndex = 0; lineIndex < LO_Count - 1; ++lineIndex) {
        std::swap(cfaLines[lineIndex], cfaLines[lineIndex + 1]);
    }
    cfaLines[LO_Count - 1] = cfaLines[LO_Count - 2] + width;
}

// Интерполяция не зеленых точек
void VNG::interpolateColorsForNotGreen(size_t columnIndex, uint32_t gradientThreshold, TRGBComponent centralColor,
    TRGBComponent otherNotGreenColor)
{
    size_t gradientsNumber = 0;
    uint16_t colorSum[RGBC_Count] = {0, 0, 0};

    if (directionGradients[BGD_NorthWest] <= gradientThreshold) {
        ++gradientsNumber;
        colorSum[otherNotGreenColor] += cfaLines[LO_Prev][columnIndex - 1];
        colorSum[centralColor] += (cfaLines[LO_Curr][columnIndex] + cfaLines[LO_BeforePrev][columnIndex - 2]) / 2;
        colorSum[RGBC_Green] += (cfaLines[LO_Prev][columnIndex - 2] + cfaLines[LO_Prev][columnIndex] +
                cfaLines[LO_Curr][columnIndex - 1] + cfaLines[LO_BeforePrev][columnIndex - 1]) / 4;
    }
    if (directionGradients[BGD_NorthEast] <= gradientThreshold) {
        ++gradientsNumber;
        colorSum[otherNotGreenColor] += cfaLines[LO_Prev][columnIndex + 1];
        colorSum[centralColor] += (cfaLines[LO_Curr][columnIndex] + cfaLines[LO_BeforePrev][columnIndex + 2]) / 2;
        colorSum[RGBC_Green] += (cfaLines[LO_Prev][columnIndex + 2] + cfaLines[LO_Prev][columnIndex] +
                cfaLines[LO_Curr][columnIndex + 1] + cfaLines[LO_BeforePrev][columnIndex + 1]) / 4;
    }
    if (directionGradients[BGD_SouthWest] <= gradientThreshold) {
        ++gradientsNumber;
        colorSum[otherNotGreenColor] += cfaLines[LO_Next][columnIndex - 1];
        colorSum[centralColor] += (cfaLines[LO_Curr][columnIndex] + cfaLines[LO_AfterNext][columnIndex - 2]) / 2;
        colorSum[RGBC_Green] += (cfaLines[LO_Next][columnIndex - 2] + cfaLines[LO_Next][columnIndex] +
                cfaLines[LO_Curr][columnIndex - 1] + cfaLines[LO_AfterNext][columnIndex - 1]) / 4;
    }
    if (directionGradients[BGD_SouthEast] <= gradientThreshold) {
        ++gradientsNumber;
        colorSum[otherNotGreenColor] += cfaLines[LO_Next][columnIndex + 1];
        colorSum[centralColor] += (cfaLines[LO_Curr][columnIndex] + cfaLines[LO_AfterNext][columnIndex + 2]) / 2;
        colorSum[RGBC_Green] += (cfaLines[LO_Next][columnIndex + 2] + cfaLines[LO_Next][columnIndex] +
                cfaLines[LO_Curr][columnIndex + 1] + cfaLines[LO_AfterNext][columnIndex + 1]) / 4;
    }
    if (directionGradients[BGD_North] <= gradientThreshold) {
        ++gradientsNumber;
        colorSum[RGBC_Green] += cfaLines[LO_Prev][columnIndex];
        colorSum[centralColor] += (cfaLines[LO_Curr][columnIndex] + cfaLines[LO_BeforePrev][columnIndex]) / 2;
        colorSum[otherNotGreenColor] += (cfaLines[LO_Prev][columnIndex - 1] + cfaLines[LO_Prev][columnIndex + 1]) / 2;
    }
    if (directionGradients[BGD_South] <= gradientThreshold) {
        ++gradientsNumber;
        colorSum[RGBC_Green] += cfaLines[LO_Next][columnIndex];
        colorSum[centralColor] += (cfaLines[LO_Curr][columnIndex] + cfaLines[LO_AfterNext][columnIndex]) / 2;
        colorSum[otherNotGreenColor] += (cfaLines[LO_Next][columnIndex - 1] + cfaLines[LO_Next][columnIndex + 1]) / 2;
    }
    if (directionGradients[BGD_West] <= gradientThreshold) {
        ++gradientsNumber;
        colorSum[RGBC_Green] += cfaLines[LO_Curr][columnIndex - 1];
        colorSum[centralColor] += (cfaLines[LO_Curr][columnIndex - 2] + cfaLines[LO_Curr][columnIndex]) / 2;
        colorSum[otherNotGreenColor] += (cfaLines[LO_Prev][columnIndex - 1] + cfaLines[LO_Next][columnIndex - 1]) / 2;
    }
    if (directionGradients[BGD_East] <= gradientThreshold) {
        ++gradientsNumber;
        colorSum[RGBC_Green] += cfaLines[LO_Curr][columnIndex + 1];
        colorSum[centralColor] += (cfaLines[LO_Curr][columnIndex + 2] + cfaLines[LO_Curr][columnIndex]) / 2;
        colorSum[otherNotGreenColor] += (cfaLines[LO_Prev][columnIndex + 1] + cfaLines[LO_Next][columnIndex + 1]) / 2;
    }

    currRecoveredLine[columnIndex][centralColor] = {cfaLines[LO_Curr][columnIndex]};
    currRecoveredLine[columnIndex][otherNotGreenColor] = {color_cast(cfaLines[LO_Curr][columnIndex] + (colorSum[otherNotGreenColor] - colorSum[centralColor]) / gradientsNumber)};
    currRecoveredLine[columnIndex][RGBC_Green] = {color_cast(cfaLines[LO_Curr][columnIndex] + (colorSum[RGBC_Green] - colorSum[centralColor]) / gradientsNumber)};
}

// Интерполяция зеленых точек
void VNG::interpolateColorsForGreen(size_t columnIndex, uint32_t gradientThreshold, TRGBComponent horizontalOtherColor,
    TRGBComponent verticalOtherColor)
{
    size_t gradientsNumber = 0;
    uint16_t colorSum[RGBC_Count] = {0, 0, 0};

    if (directionGradients[BGD_NorthWest] <= gradientThreshold) {
        ++gradientsNumber;
        colorSum[RGBC_Green] += cfaLines[LO_Prev][columnIndex - 1];
        colorSum[horizontalOtherColor] += (cfaLines[LO_Prev][columnIndex - 2] + cfaLines[LO_Prev][columnIndex]) / 2;
        colorSum[verticalOtherColor] += (cfaLines[LO_Curr][columnIndex - 1] + cfaLines[LO_BeforePrev][columnIndex - 1]) / 2;
    }
    if (directionGradients[BGD_NorthEast] <= gradientThreshold) {
        ++gradientsNumber;
        colorSum[RGBC_Green] += cfaLines[LO_Prev][columnIndex + 1];
        colorSum[horizontalOtherColor] += (cfaLines[LO_Prev][columnIndex + 2] + cfaLines[LO_Prev][columnIndex]) / 2;
        colorSum[verticalOtherColor] += (cfaLines[LO_Curr][columnIndex + 1] + cfaLines[LO_BeforePrev][columnIndex + 1]) / 2;
    }
    if (directionGradients[BGD_SouthWest] <= gradientThreshold) {
        ++gradientsNumber;
        colorSum[RGBC_Green] += cfaLines[LO_Next][columnIndex - 1];
        colorSum[horizontalOtherColor] += (cfaLines[LO_Next][columnIndex - 2] + cfaLines[LO_Next][columnIndex]) / 2;
        colorSum[verticalOtherColor] += (cfaLines[LO_Curr][columnIndex - 1] + cfaLines[LO_AfterNext][columnIndex - 1]) / 2;
    }
    if (directionGradients[BGD_SouthEast] <= gradientThreshold) {
        ++gradientsNumber;
        colorSum[RGBC_Green] += cfaLines[LO_Next][columnIndex + 1];
        colorSum[horizontalOtherColor] += (cfaLines[LO_Next][columnIndex + 2] + cfaLines[LO_Next][columnIndex]) / 2;
        colorSum[verticalOtherColor] += (cfaLines[LO_Curr][columnIndex + 1] + cfaLines[LO_AfterNext][columnIndex + 1]) / 2;
    }
    if (directionGradients[BGD_North] <= gradientThreshold) {
        ++gradientsNumber;
        colorSum[verticalOtherColor] += cfaLines[LO_Prev][columnIndex];
        colorSum[horizontalOtherColor] += (cfaLines[LO_Curr][columnIndex - 1] + cfaLines[LO_Curr][columnIndex + 1] +
                cfaLines[LO_BeforePrev][columnIndex - 1] + cfaLines[LO_BeforePrev][columnIndex + 1]) / 4;
        colorSum[RGBC_Green] += (cfaLines[LO_Curr][columnIndex] + cfaLines[LO_BeforePrev][columnIndex]) / 2;
    }
    if (directionGradients[BGD_South] <= gradientThreshold) {
        ++gradientsNumber;
        colorSum[verticalOtherColor] += cfaLines[LO_Next][columnIndex - 1];
        colorSum[horizontalOtherColor] += (cfaLines[LO_Curr][columnIndex - 1] + cfaLines[LO_Curr][columnIndex + 1] +
                cfaLines[LO_AfterNext][columnIndex - 1] + cfaLines[LO_AfterNext][columnIndex + 1]) / 4;
        colorSum[RGBC_Green] += (cfaLines[LO_Curr][columnIndex] + cfaLines[LO_AfterNext][columnIndex]) / 2;
    }
    if (directionGradients[BGD_West] <= gradientThreshold) {
        ++gradientsNumber;
        colorSum[horizontalOtherColor] += cfaLines[LO_Curr][columnIndex - 1];
        colorSum[verticalOtherColor] += (cfaLines[LO_Prev][columnIndex] + cfaLines[LO_Prev][columnIndex - 2] +
                cfaLines[LO_Next][columnIndex] + cfaLines[LO_Next][columnIndex - 2]) / 4;
        colorSum[RGBC_Green] += (cfaLines[LO_Curr][columnIndex] + cfaLines[LO_Curr][columnIndex - 2]) / 2;
    }
    if (directionGradients[BGD_East] <= gradientThreshold) {
        ++gradientsNumber;
        colorSum[horizontalOtherColor] += cfaLines[LO_Curr][columnIndex + 1];
        colorSum[verticalOtherColor] += (cfaLines[LO_Prev][columnIndex] + cfaLines[LO_Prev][columnIndex + 2] +
                cfaLines[LO_Next][columnIndex] + cfaLines[LO_Next][columnIndex + 2]) / 4;
        colorSum[RGBC_Green] += (cfaLines[LO_Curr][columnIndex] + cfaLines[LO_Curr][columnIndex + 2]) / 2;
    }

    currRecoveredLine[columnIndex][RGBC_Green] = {cfaLines[LO_Curr][columnIndex]};
    currRecoveredLine[columnIndex][RGBC_Red] = {color_cast(cfaLines[LO_Curr][columnIndex] + (colorSum[RGBC_Red] - colorSum[RGBC_Green]) / gradientsNumber)};
    currRecoveredLine[columnIndex][RGBC_Blue] = {color_cast(cfaLines[LO_Curr][columnIndex] + (colorSum[RGBC_Blue] - colorSum[RGBC_Green]) / gradientsNumber)};
}

// Подсчет порога на градиенты в текущей точке
uint32_t VNG::getGradientThreshold() const {
    uint16_t minGradient = std::numeric_limits<uint16_t>::max();
    uint16_t maxGradient = std::numeric_limits<uint16_t>::min();
    for (size_t directionIndex = 0; directionIndex < BGD_Count; ++directionIndex) {
        if (directionGradients[directionIndex] < minGradient) {
            minGradient = directionGradients[directionIndex];
        }
        if (directionGradients[directionIndex] > maxGradient) {
            maxGradient = directionGradients[directionIndex];
        }
    }
    //return (minGradient + maxGradient / 2u);
   // return ((4u * minGradient + maxGradient) >> 1u);
    //return minGradient * 300; //+ (maxGradient) / 15;

    return std::numeric_limits<uint32_t>::max();
   // return ((4u * minGradient + maxGradient) >> 1u);
}

// Подсчет вертикального градиента
void VNG::calcVerticalGradient(const uint8_t* firstLine, const uint8_t* secondLine, TLongGradientsOrder gradType) {
    for (size_t columnIndex = 0; columnIndex < width; ++columnIndex) {
        verticalGradient[gradType][columnIndex] = std::abs(secondLine[columnIndex] - firstLine[columnIndex]);
    }
}

// Подсчет горизонтального градиента
void VNG::calcHorizontalGradient(const uint8_t* line, TLongGradientsOrder gradType) {
    for (size_t columnIndex = 2; columnIndex < width; ++columnIndex) {
        horizontalGradient[gradType][columnIndex] = std::abs(line[columnIndex] - line[columnIndex - 2]);
    }
}

// Подсчет диагонального градиента
void VNG::calcDiagonalGradient(const uint8_t* firstLine, const uint8_t* secondLine, bool isShort, bool isLeft,
    size_t gradIndex)
{
    if (!isShort && isLeft) {
        for (size_t columnIndex = 2; columnIndex < width; ++columnIndex) {
            leftDiagonalLongGradient[gradIndex][columnIndex] = std::abs(secondLine[columnIndex] - firstLine[columnIndex - 2]);
        }
        leftDiagonalLongGradient[gradIndex][0] = std::abs(secondLine[0] - firstLine[0]);
        leftDiagonalLongGradient[gradIndex][1] = std::abs(secondLine[1] - firstLine[1]);
    }
    if (!isShort && !isLeft) {
        for (size_t columnIndex = 0; columnIndex < width - 2; ++columnIndex) {
            rightDiagonalLongGradient[gradIndex][columnIndex] = std::abs(secondLine[columnIndex] - firstLine[columnIndex + 2]);
        }
        rightDiagonalLongGradient[gradIndex][width - 1] = std::abs(secondLine[width - 1] - firstLine[width - 1]);
        rightDiagonalLongGradient[gradIndex][width - 2] = std::abs(secondLine[width - 2] - firstLine[width - 2]);
    }
    if (isShort && isLeft) {
        for (size_t columnIndex = 1; columnIndex < width; ++columnIndex) {
            leftDiagonalShortGradient[gradIndex][columnIndex] = std::abs(secondLine[columnIndex] - firstLine[columnIndex - 1]);
        }
        leftDiagonalShortGradient[gradIndex][0] = std::abs(secondLine[0] - firstLine[1]);
    }
    if (isShort && !isLeft) {
        for (size_t columnIndex = 0; columnIndex < width - 1; ++columnIndex) {
            rightDiagonalShortGradient[gradIndex][columnIndex] = std::abs(secondLine[columnIndex] - firstLine[columnIndex + 1]);
        }
        rightDiagonalShortGradient[gradIndex][width - 1] = std::abs(secondLine[width - 1] - firstLine[width - 2]);
    }
}

// Подчет градиентов по направлениям в зеленой точке
void VNG::calcDirectionGradientsForGreen(size_t columnIndex) {
    calcNonDiagonalDirectionGradients(columnIndex);
    directionGradients[BGD_NorthWest] = leftDiagonalLongGradient[LGO_Top][columnIndex] + leftDiagonalLongGradient[LGO_Top][columnIndex + 1] +
            leftDiagonalLongGradient[LGO_Mid][columnIndex + 1] + leftDiagonalLongGradient[LGO_Mid][columnIndex];
    directionGradients[BGD_NorthEast] = rightDiagonalLongGradient[LGO_Mid][columnIndex - 1] + rightDiagonalLongGradient[LGO_Top][columnIndex] +
            rightDiagonalLongGradient[LGO_Top][columnIndex - 1] + rightDiagonalLongGradient[LGO_Mid][columnIndex];
    directionGradients[BGD_SouthWest] = rightDiagonalLongGradient[LGO_Mid][columnIndex - 1] + rightDiagonalLongGradient[LGO_Bot][columnIndex - 2] +
            rightDiagonalLongGradient[LGO_Bot][columnIndex - 1] + rightDiagonalLongGradient[LGO_Mid][columnIndex - 2];
    directionGradients[BGD_SouthEast] = leftDiagonalLongGradient[LGO_Mid][columnIndex + 1] + leftDiagonalLongGradient[LGO_Mid][columnIndex + 2] +
            leftDiagonalLongGradient[LGO_Bot][columnIndex + 1] + leftDiagonalLongGradient[LGO_Bot][columnIndex + 2];
}

// Подсчет градиентов по направлениям в незеленой точке
void VNG::calcDirectionGradientsForNotGreen(size_t columnIndex) {
    calcNonDiagonalDirectionGradients(columnIndex);
    directionGradients[BGD_NorthWest] = leftDiagonalLongGradient[LGO_Mid][columnIndex + 1] + leftDiagonalLongGradient[LGO_Top][columnIndex] +
            (leftDiagonalShortGradient[SGO_Top][columnIndex] + leftDiagonalShortGradient[SGO_MidTop][columnIndex - 1] +
            leftDiagonalShortGradient[SGO_MidTop][columnIndex + 1] + leftDiagonalShortGradient[SGO_MidBot][columnIndex]) / 2;
    directionGradients[BGD_NorthEast] = rightDiagonalLongGradient[LGO_Mid][columnIndex - 1] + rightDiagonalLongGradient[LGO_Top][columnIndex] +
            (rightDiagonalShortGradient[SGO_MidTop][columnIndex - 1] + rightDiagonalShortGradient[SGO_MidTop][columnIndex + 1] +
            rightDiagonalShortGradient[SGO_MidBot][columnIndex] + rightDiagonalShortGradient[SGO_Top][columnIndex]) / 2;
    directionGradients[BGD_SouthWest] = rightDiagonalLongGradient[LGO_Mid][columnIndex - 1] + rightDiagonalLongGradient[LGO_Bot][columnIndex + 2] +
            (rightDiagonalShortGradient[SGO_MidTop][columnIndex - 1] + rightDiagonalShortGradient[SGO_MidBot][columnIndex - 2] +
            rightDiagonalShortGradient[SGO_MidBot][columnIndex] + rightDiagonalShortGradient[SGO_Bot][columnIndex - 1]) / 2;
    directionGradients[BGD_SouthEast] = leftDiagonalLongGradient[LGO_Mid][columnIndex + 1] + leftDiagonalLongGradient[LGO_Bot][columnIndex + 2] +
            (leftDiagonalShortGradient[SGO_MidTop][columnIndex + 1] + leftDiagonalShortGradient[SGO_MidBot][columnIndex] +
            leftDiagonalShortGradient[SGO_MidBot][columnIndex + 2] + leftDiagonalShortGradient[SGO_Bot][columnIndex + 1]) / 2;
}

// Подсчет недиагональных градиентов по направлению
void VNG::calcNonDiagonalDirectionGradients(size_t columnIndex) {
    directionGradients[BGD_North] = verticalGradient[LGO_Top][columnIndex] + verticalGradient[LGO_Mid][columnIndex] +
                                    (verticalGradient[LGO_Top][columnIndex - 1] + verticalGradient[LGO_Mid][columnIndex - 1] +
                                    verticalGradient[LGO_Top][columnIndex + 1] + verticalGradient[LGO_Mid][columnIndex + 1]) / 2;
    directionGradients[BGD_South] = verticalGradient[LGO_Bot][columnIndex] + verticalGradient[LGO_Mid][columnIndex] +
                                    (verticalGradient[LGO_Bot][columnIndex - 1] + verticalGradient[LGO_Mid][columnIndex - 1] +
                                    verticalGradient[LGO_Bot][columnIndex + 1] + verticalGradient[LGO_Mid][columnIndex + 1]) / 2;
    directionGradients[BGD_West] = horizontalGradient[LGO_Mid][columnIndex] + horizontalGradient[LGO_Mid][columnIndex + 1] +
                                   (horizontalGradient[LGO_Top][columnIndex] + horizontalGradient[LGO_Top][columnIndex + 1] +
                                   horizontalGradient[LGO_Bot][columnIndex] + horizontalGradient[LGO_Bot][columnIndex + 1]) / 2;
    directionGradients[BGD_East] = horizontalGradient[LGO_Mid][columnIndex + 1] + horizontalGradient[LGO_Mid][columnIndex + 2] +
                                   (horizontalGradient[LGO_Top][columnIndex + 1] + horizontalGradient[LGO_Top][columnIndex + 2] +
                                   horizontalGradient[LGO_Bot][columnIndex + 1] + horizontalGradient[LGO_Bot][columnIndex + 2]) / 2;
}

CMetrics CalculateMetrics(const CRGBImage& recoveredImage, const CRGBImage& referenceImage) {
    std::shared_ptr<CGrayImage> grayRecovered = ConvertRGBImageToGray(recoveredImage);
    std::shared_ptr<CGrayImage> grayReference = ConvertRGBImageToGray(referenceImage);
    const size_t width = grayRecovered->GetWidth();
    const size_t height = grayRecovered->GetHeight();
    assert(width == grayReference->GetWidth());
    assert(height == grayReference->GetHeight());
    const size_t imageSize = width * height;
    double mse = 0.0;
    const CGrayValue* recoveredBuffer = grayRecovered->GetBuffer();
    const CGrayValue* referenceBuffer = grayReference->GetBuffer();
    for (size_t pixelIndex = 0; pixelIndex < imageSize; ++pixelIndex) {
        mse += std::pow(recoveredBuffer[pixelIndex].Components[0] - referenceBuffer[pixelIndex].Components[0], 2);
    }
    mse /= imageSize;
    const double psnr = 10 * std::log10(pow(255, 2) / mse);
    return CMetrics(mse, psnr);
}

CMetrics CalculateCuttedMetrics(const CRGBImage& recoveredImage, const CRGBImage& referenceImage) {
    std::shared_ptr<CGrayImage> grayRecovered = ConvertRGBImageToGray(recoveredImage);
    std::shared_ptr<CGrayImage> grayReference = ConvertRGBImageToGray(referenceImage);
    const size_t width = grayRecovered->GetWidth();
    const size_t height = grayRecovered->GetHeight();
    assert(width == grayReference->GetWidth());
    assert(height == grayReference->GetHeight());
    const size_t imageSize = width * height;
    double mse = 0.0;
    const CGrayValue* recoveredBuffer = grayRecovered->GetBuffer();
    const CGrayValue* referenceBuffer = grayReference->GetBuffer();
    recoveredBuffer += 2 * width;
    referenceBuffer += 2 * width;
    for (size_t rowIndex = 2; rowIndex < height - 2; ++rowIndex) {
        for (size_t columnIndex = 2; columnIndex < width - 2; ++columnIndex) {
            mse += std::pow(recoveredBuffer[columnIndex].Components[0] - referenceBuffer[columnIndex].Components[0], 2);
        }
        recoveredBuffer += width;
        referenceBuffer += width;
    }

    mse /= ((height - 2) * (width - 2));
    const double psnr = 10 * std::log10(pow(255, 2) / mse);
    return CMetrics(mse, psnr);
}