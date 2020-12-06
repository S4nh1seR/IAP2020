#include "binarizer.h"

#include <fstream>
#include <iostream>
#include <limits>

TBinarizationMode ChooseMode(const std::string& modeName) {
    if (modeName == "avg") {
        return BM_Avg;
    } else if (modeName == "center") {
        return BM_Center;
    } else if (modeName == "centerMinWeighted") {
        return BM_CenterMinWeighted;
    } else if (modeName == "avgCenterWeighted") {
        return BM_AvgCenterWeighted;
    } else if (modeName == "bySeparatedNoiseLevels") {
        return BM_BySeparatedNoiseLevels;
    } else {
        assert(false);
        return BM_Avg;
    }
}

namespace {
inline size_t getMaxSqueezeDegree(size_t sideSize) {
    size_t deg = 1u;
    for (size_t val = 2u; val < sideSize; val <<= 1u, ++deg) {}
    return deg - 1;
}

inline size_t getDivisibleSideSize(size_t srcSideSize, size_t depth) {
    const size_t multiplier = (1 << depth);
    const size_t remainder = srcSideSize % multiplier;
    return (remainder == 0) ? srcSideSize : srcSideSize + (multiplier - remainder);
}
}

CPyramidBinarizer::CPyramidBinarizer(const CGrayImage& grayImage, TBinarizationMode _mode,
                                     uint8_t _noiseLevel, float _noiseSigmaMultiplier) :
    mode(_mode),
    width(grayImage.GetWidth()),
    height(grayImage.GetHeight()),
    srcGrayImage(grayImage),
    depth(getMaxSqueezeDegree(std::min(height, width))),
    noiseLevel(_noiseLevel),
    noiseSigmaMultiplier(_noiseSigmaMultiplier),
    minPyramid(new CGrayImage[depth]),
    maxPyramid(new CGrayImage[depth]),
    avgPyramid(new CGrayImage[depth])
{
    for (size_t i = 0; i < binsNumber; ++i) {
        varSum[i] = 0;
        numberPerBin[i] = 0;
    }
    if (mode == BM_BySeparatedNoiseLevels) {
        prepareDeviationStats();
    }
    prepareExtended();
    preparePyramids();
    const size_t extWidth = extendedImage.IsEmpty() ? width : extendedImage.GetWidth();
    const size_t extHeight = extendedImage.IsEmpty() ? height : extendedImage.GetHeight();
    const size_t extSize = extWidth * extHeight;
    prevThresMap = new uint8_t[extSize];
    currThresMap = new uint8_t[extSize];
}

CPyramidBinarizer::~CPyramidBinarizer() {
    delete [] minPyramid;
    delete [] maxPyramid;
    delete [] avgPyramid;
    delete [] prevThresMap;
    delete [] currThresMap;
}

std::shared_ptr<CBWImage> CPyramidBinarizer::Binarize() {
    static const uint8_t blackColor = 0;
    static const uint8_t whiteColor = 1;

    std::shared_ptr<CBWImage> bwImage(new CBWImage(height, width));
    buildThresholdMap();

    const size_t extWidth = extendedImage.IsEmpty() ? width : extendedImage.GetWidth();
    const size_t extHeight = extendedImage.IsEmpty() ? height : extendedImage.GetHeight();
    const size_t xPadding = extWidth - width;
    const size_t yPadding = extHeight - height;
    const size_t topPadding = yPadding / 2;
    const size_t leftPadding = xPadding / 2;

    auto thresholdMapRowBuffer = currThresMap + topPadding * extWidth + leftPadding;
    auto srcImageRowBuffer = srcGrayImage.GetBuffer();
    auto bwImageRowBuffer = bwImage->GetBuffer();
    for (size_t rowIndex = 0; rowIndex < height; ++rowIndex) {
        for (size_t columnIndex = 0; columnIndex < width; ++columnIndex) {
            bwImageRowBuffer[columnIndex] = (srcImageRowBuffer[columnIndex] < thresholdMapRowBuffer[columnIndex])
                ? blackColor : whiteColor;
        }
        thresholdMapRowBuffer += extWidth;
        bwImageRowBuffer += width;
        srcImageRowBuffer += width;
    }
    return bwImage;
}

void CPyramidBinarizer::buildThresholdMap() {
    const size_t maxSqueezedWidth = avgPyramid[depth - 1].GetWidth();
    const size_t maxSqueezedHeight = avgPyramid[depth - 1].GetHeight();
    const size_t maxSqueezedSize = maxSqueezedWidth * maxSqueezedHeight;
    std::copy_n(avgPyramid[depth - 1].GetBuffer(), maxSqueezedSize, currThresMap);

    for (size_t level = depth - 1; level != static_cast<size_t>(-1); --level) {
        const size_t currMapWidth = avgPyramid[level].GetWidth();
        const size_t currMapHeight = avgPyramid[level].GetHeight();
        if (level != depth - 1) {
            const auto minPyramidBuffer = minPyramid[level].GetBuffer();
            const auto maxPyramidBuffer = maxPyramid[level].GetBuffer();
            const auto avgPyramidBuffer = avgPyramid[level].GetBuffer();
            size_t mapIndex = 0;
            switch(mode) {
                case BM_Avg:
                    for (size_t rowIndex = 0; rowIndex < currMapHeight; ++rowIndex) {
                        for (size_t columnIndex = 0; columnIndex < currMapWidth; ++columnIndex, ++mapIndex) {
                            const auto maxValue = maxPyramidBuffer[mapIndex];
                            const auto minValue = minPyramidBuffer[mapIndex];
                            if (maxValue - minValue > noiseLevel) {
                                currThresMap[mapIndex] = avgPyramidBuffer[mapIndex];
                            }
                        }
                    }
                    break;
                case BM_Center:
                    for (size_t rowIndex = 0; rowIndex < currMapHeight; ++rowIndex) {
                        for (size_t columnIndex = 0; columnIndex < currMapWidth; ++columnIndex, ++mapIndex) {
                            const auto maxValue = maxPyramidBuffer[mapIndex];
                            const auto minValue = minPyramidBuffer[mapIndex];
                            if (maxValue - minValue > noiseLevel) {
                                currThresMap[mapIndex] = (maxPyramidBuffer[mapIndex] + minPyramidBuffer[mapIndex] + 1) / 2;
                            }
                        }
                    }
                    break;
                case BM_CenterMinWeighted:
                    for (size_t rowIndex = 0; rowIndex < currMapHeight; ++rowIndex) {
                        for (size_t columnIndex = 0; columnIndex < currMapWidth; ++columnIndex, ++mapIndex) {
                            const auto maxValue = maxPyramidBuffer[mapIndex];
                            const auto minValue = minPyramidBuffer[mapIndex];
                            if (maxValue - minValue > noiseLevel) {
                                const auto medValue = (minValue + maxValue) / 2;
                                currThresMap[mapIndex] = (minValue + medValue * 2 + 1) / 3;
                            }
                        }
                    }
                    break;
                case BM_AvgCenterWeighted:
                    for (size_t rowIndex = 0; rowIndex < currMapHeight; ++rowIndex) {
                        for (size_t columnIndex = 0; columnIndex < currMapWidth; ++columnIndex, ++mapIndex) {
                            const auto maxValue = maxPyramidBuffer[mapIndex];
                            const auto minValue = minPyramidBuffer[mapIndex];
                            if (maxValue - minValue > noiseLevel) {
                                const auto medValue = (minValue + maxValue) / 2;
                                const auto avgValue = avgPyramidBuffer[mapIndex];
                                currThresMap[mapIndex] = (medValue + avgValue + 1) / 2;
                            }
                        }
                    }
                    break;
                case BM_BySeparatedNoiseLevels:
                    for (size_t rowIndex = 0; rowIndex < currMapHeight; ++rowIndex) {
                        for (size_t columnIndex = 0; columnIndex < currMapWidth; ++columnIndex, ++mapIndex) {
                            const auto maxValue = maxPyramidBuffer[mapIndex];
                            const auto minValue = minPyramidBuffer[mapIndex];
                            const auto avgValue = avgPyramidBuffer[mapIndex];
                            if (maxValue - minValue > static_cast<int>(noiseSigmaMultiplier * varSum[avgValue / valuesPerBin])) {
                                currThresMap[mapIndex] = (minValue + maxValue) / 2;
                            }
                        }
                    }
                    break;
                default:
                    assert(false);
            }
        }
        std::swap(prevThresMap, currThresMap);
        upsampleThresholdMap(currMapWidth, currMapHeight);
    }
}

void CPyramidBinarizer::upsampleThresholdMap(size_t currMapWidth, size_t currMapHeight) {
    static const uint8_t centerWeight = 9;
    static const uint8_t ortoWeight = 3;
    static const uint8_t diagWeight = 1;
    static const uint8_t sumWeight = centerWeight + 2 * ortoWeight + diagWeight;
    static const uint8_t sumWeightHalf = sumWeight / 2;

    const size_t upMapWidth = 2 * currMapWidth;
    for (size_t rowIndex = 0; rowIndex < currMapHeight; ++rowIndex) {
        auto currRow = prevThresMap + currMapWidth * rowIndex;
        auto prevRow = std::max(currRow - currMapWidth, prevThresMap);
        auto nextRow = std::min(currRow + currMapWidth, prevThresMap + currMapHeight * currMapWidth);
        auto upTopRow = currThresMap + 2 * rowIndex * upMapWidth;
        auto upBotRow = upTopRow + upMapWidth;
        for (size_t columnIndex = 0; columnIndex < currMapWidth; ++columnIndex) {
            const size_t prevIndex = std::max<size_t>(columnIndex, 1) - 1;
            const size_t nextIndex = std::min<size_t>(columnIndex + 1, currMapWidth - 1);

            const uint8_t curr = currRow[columnIndex];
            const uint8_t north = prevRow[columnIndex];
            const uint8_t south = nextRow[columnIndex];
            const uint8_t west = currRow[prevIndex];
            const uint8_t east = currRow[nextIndex];
            const uint8_t northWest = prevRow[prevIndex];
            const uint8_t northEast = prevRow[nextIndex];
            const uint8_t southWest = nextRow[prevIndex];
            const uint8_t southEast = nextRow[nextIndex];

            const uint16_t centerWeighted = centerWeight * curr;
            const uint16_t northWeighted = north * ortoWeight;
            const uint16_t southWeighted = south * ortoWeight;
            const uint16_t westWeighted = west * ortoWeight;
            const uint16_t eastWeighted = east * ortoWeight;

            const uint8_t topLeft = (centerWeighted + northWeighted + westWeighted + diagWeight * northWest + sumWeightHalf) / sumWeight;
            const uint8_t topRight = (centerWeighted + northWeighted + eastWeighted + diagWeight * northEast + sumWeightHalf) / sumWeight;
            const uint8_t botLeft = (centerWeighted + southWeighted + westWeighted + diagWeight * southWest + sumWeightHalf) / sumWeight;
            const uint8_t botRight = (centerWeighted + southWeighted + eastWeighted + diagWeight * southEast + sumWeightHalf) / sumWeight;

            const size_t posIndex = 2 * columnIndex;
            upTopRow[posIndex] = topLeft;
            upBotRow[posIndex] = botLeft;
            upTopRow[posIndex + 1] = topRight;
            upBotRow[posIndex + 1] = botRight;
        }
    }
}

void CPyramidBinarizer::prepareExtended() {
    const size_t extHeight = getDivisibleSideSize(height, depth);
    const size_t extWidth = getDivisibleSideSize(width, depth);
    assert(extWidth >= width && extHeight >= height);
    assert(depth >= 1);
    if (height != extHeight || width != extWidth) {
        new(&extendedImage) CGrayImage(extHeight, extWidth);
        const size_t yPadding = extHeight - height;
        const size_t xPadding = extWidth - width;
        const size_t leftPadding = xPadding / 2;
        const size_t topPadding = yPadding / 2;

        auto srcBuffer = srcGrayImage.GetBuffer();
        auto extendedRowBuffer = extendedImage.GetBuffer();
        for (size_t rowIndex = 0; rowIndex < extHeight; ++rowIndex) {
            const size_t rowOffset = std::min(std::max(rowIndex, topPadding) - topPadding, height - 1);
            const uint8_t* rowBuffer = srcBuffer + width * rowOffset;
            for (size_t columnIndex = 0; columnIndex < extWidth; ++columnIndex) {
                const size_t columnOffset = std::min(std::max(columnIndex, leftPadding) - leftPadding, width - 1);
                extendedRowBuffer[columnIndex] = rowBuffer[columnOffset];
            }
            extendedRowBuffer += extWidth;
        }
    }
}

void CPyramidBinarizer::prepareDeviationStats() {
    const CGrayImage& srcImage = extendedImage.IsEmpty() ? srcGrayImage : extendedImage;
    const size_t srcHeight = srcImage.GetHeight();
    const size_t srcWidth = srcImage.GetWidth();

    uint64_t* sumSqTable = new uint64_t[(srcHeight + 1) * (srcWidth + 1)];
    uint64_t* sumTable = new uint64_t[(srcHeight + 1) * (srcWidth + 1)];
    for (size_t i = 0; i < srcWidth + 1; ++i) {
        sumSqTable[i] = 0;
        sumTable[i] = 0;
    }
    for (size_t i = 1; i < srcHeight + 1; ++i) {
        const auto currRowOffset = (srcWidth + 1) * i;
        const auto prevRowOffset = (srcWidth + 1) * (i - 1);
        sumSqTable[currRowOffset] = 0;
        sumTable[currRowOffset] = 0;
        for (size_t j = 1; j < srcWidth + 1; ++j) {
            const auto value = srcImage.GetValue(i - 1, j - 1);
            sumSqTable[currRowOffset + j] = sumSqTable[prevRowOffset + j] + value * value;
            sumTable[currRowOffset + j] = sumTable[prevRowOffset + j] + value;
        }
    }
    for (size_t i = 1; i < srcHeight + 1; ++i) {
        const auto rowOffset = i * (srcWidth + 1);
        for (size_t j = 1; j < srcWidth + 1; ++j) {
            sumSqTable[rowOffset + j] += sumSqTable[rowOffset + (j - 1)];
            sumTable[rowOffset + j] += sumTable[rowOffset + (j - 1)];
        }
    }
    uint64_t* sqAreaTable = sumSqTable + (srcWidth + 1) + 1;
    uint64_t* areaTable = sumTable + (srcWidth + 1) + 1;

    const size_t radius = 16;
    for (size_t i = 0; i < srcHeight; ++i) {
        const size_t top = (i >= radius) ? (i - radius) : 0;
        const size_t bot = (i + radius > srcHeight - 1) ? srcHeight - 1 : i + radius;
        const size_t vertNumber = bot - top + 1;
        const auto botOffset = bot * (srcWidth + 1);
        const auto topOffset = (top - 1) * (srcWidth + 1);
        for (size_t j = 0; j < srcWidth; ++j) {
            const uint64_t left = (j >= radius) ? (j - radius) : 0;
            const uint64_t right = (j + radius > srcWidth - 1) ? srcWidth - 1 : j + radius;
            const uint64_t horNumber = right - left + 1;
            const uint64_t pixelsNumber = horNumber * vertNumber;
            const uint64_t pixelsHalf = pixelsNumber / 2;

            const auto cSqVal = sqAreaTable[botOffset + right];
            const auto aSqVal = sqAreaTable[topOffset + left - 1];
            const auto bSqVal = sqAreaTable[topOffset + right];
            const auto dSqVal = sqAreaTable[botOffset + left - 1];
            const auto secondMoment = (cSqVal + aSqVal - bSqVal - dSqVal + pixelsHalf) / pixelsNumber;

            const auto cVal = areaTable[botOffset + right];
            const auto aVal = areaTable[topOffset + left - 1];
            const auto bVal = areaTable[topOffset + right];
            const auto dVal = areaTable[botOffset + left - 1];
            const auto firstMoment = (cVal + aVal - bVal - dVal + pixelsHalf) / pixelsNumber;

            const auto firstSquared = firstMoment * firstMoment;
            const auto var = secondMoment >= firstSquared ? secondMoment - firstSquared : 0;

            if (var != 0) {
                auto varBin = firstMoment / 16;
                ++numberPerBin[varBin];
                varSum[varBin] += var;
            }
        }
    }
    for (size_t i = 0; i < binsNumber; ++i) {
        if (numberPerBin[i] != 0) {
            varSum[i] /= numberPerBin[i];
            varSum[i] = round(sqrt(varSum[i]));
        }
    }

    delete [] sumSqTable;
    delete [] sumTable;
}

void CPyramidBinarizer::preparePyramids() {
    const CGrayImage* prevMinPyramid = extendedImage.IsEmpty() ? &srcGrayImage : &extendedImage;
    const CGrayImage* prevMaxPyramid = prevMinPyramid;
    const CGrayImage* prevAvgPyramid = prevMinPyramid;

    for (size_t pyramidIndex = 0; pyramidIndex < depth; ++pyramidIndex) {
        const size_t prevPyramidHeight = prevMinPyramid->GetHeight();
        const size_t prevPyramidWidth = prevMinPyramid->GetWidth();
        const size_t currPyramidHeight = prevPyramidHeight / 2;
        const size_t currPyramidWidth = prevPyramidWidth / 2;

        new(&minPyramid[pyramidIndex]) CGrayImage(currPyramidHeight, currPyramidWidth);
        new(&maxPyramid[pyramidIndex]) CGrayImage(currPyramidHeight, currPyramidWidth);
        new(&avgPyramid[pyramidIndex]) CGrayImage(currPyramidHeight, currPyramidWidth);

        auto prevMinPyramidTopRowBuffer = prevMinPyramid->GetBuffer();
        auto prevMaxPyramidTopRowBuffer = prevMaxPyramid->GetBuffer();
        auto prevAvgPyramidTopRowBuffer = prevAvgPyramid->GetBuffer();
        auto prevMinPyramidBotRowBuffer = prevMinPyramidTopRowBuffer + prevPyramidWidth;
        auto prevMaxPyramidBotRowBuffer = prevMaxPyramidTopRowBuffer + prevPyramidWidth;
        auto prevAvgPyramidBotRowBuffer = prevAvgPyramidTopRowBuffer + prevPyramidWidth;

        auto currMinPyramidBuffer = minPyramid[pyramidIndex].GetBuffer();
        auto currMaxPyramidBuffer = maxPyramid[pyramidIndex].GetBuffer();
        auto currAvgPyramidBuffer = avgPyramid[pyramidIndex].GetBuffer();

        const size_t pyramidRowStepOffset = 2 * prevPyramidWidth;
        for (size_t rowIndex = 0; rowIndex < currPyramidHeight; ++rowIndex) {
            for (size_t columnIndex = 0; columnIndex < currPyramidWidth; ++columnIndex) {
                currMinPyramidBuffer[columnIndex] = std::min({
                    prevMinPyramidTopRowBuffer[2 * columnIndex], prevMinPyramidTopRowBuffer[2 * columnIndex + 1],
                    prevMinPyramidBotRowBuffer[2 * columnIndex], prevMinPyramidBotRowBuffer[2 * columnIndex + 1]});
                currMaxPyramidBuffer[columnIndex] = std::max({
                    prevMaxPyramidTopRowBuffer[2 * columnIndex], prevMaxPyramidTopRowBuffer[2 * columnIndex + 1],
                    prevMaxPyramidBotRowBuffer[2 * columnIndex], prevMaxPyramidBotRowBuffer[2 * columnIndex + 1]});
                currAvgPyramidBuffer[columnIndex] =
                    (prevAvgPyramidTopRowBuffer[2 * columnIndex] + prevAvgPyramidTopRowBuffer[2 * columnIndex + 1] +
                    prevAvgPyramidBotRowBuffer[2 * columnIndex] + prevAvgPyramidBotRowBuffer[2 * columnIndex + 1] + 2) / 4;
            }

            prevMinPyramidTopRowBuffer += pyramidRowStepOffset;
            prevMaxPyramidTopRowBuffer += pyramidRowStepOffset;
            prevAvgPyramidTopRowBuffer += pyramidRowStepOffset;
            prevMinPyramidBotRowBuffer += pyramidRowStepOffset;
            prevMaxPyramidBotRowBuffer += pyramidRowStepOffset;
            prevAvgPyramidBotRowBuffer += pyramidRowStepOffset;

            currMinPyramidBuffer += currPyramidWidth;
            currMaxPyramidBuffer += currPyramidWidth;
            currAvgPyramidBuffer += currPyramidWidth;
        }
        prevMinPyramid = &minPyramid[pyramidIndex];
        prevMaxPyramid = &maxPyramid[pyramidIndex];
        prevAvgPyramid = &avgPyramid[pyramidIndex];
    }
}