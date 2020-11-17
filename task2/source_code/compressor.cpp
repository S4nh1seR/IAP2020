#include "fractal.h"
#include <cassert>
#include <fstream>

namespace {
// Порядок укладки подблоков для правильной ориентации блока
enum TSubBlockOrder : unsigned char {
    SBO_TopLeft,
    SBO_TopRight,
    SBO_BotLeft,
    SBO_BotRight,

    SBO_Count
};

// Порядок укладки подблоков в разных ориентациях относительно правильной ориентации
const uint8_t orientationHashPermutations[BO_Count][SBO_Count] = {
    {0, 1, 2, 3},
    {1, 3, 0, 2},
    {3, 2, 1, 0},
    {2, 0, 3, 1},
    {1, 0, 3, 2},
    {3, 1, 2, 0},
    {2, 3, 0, 1},
    {0, 2, 1, 3}
};

// Подсчет хэша блока заданной ориентации
inline uint8_t calculateHash(const int* avgIntensities, int fullIntensity,
    TBlockOrientation orientation = BO_Rot0)
{
    const auto permutation = orientationHashPermutations[orientation];
    uint8_t hash = 0;
    for (size_t permutationIndex = 0; permutationIndex < SBO_Count; ++permutationIndex) {
        if (avgIntensities[permutation[permutationIndex]] > fullIntensity) {
            hash |= (1u << permutationIndex);
        }
    }
    return hash;
}
}

CFractalImageCompressor::CFractalImageCompressor(const CGrayImage& toCompress, int _rBlockSize,
        bool _isFastModeEnabled) :
    isFastModeEnabled(_isFastModeEnabled),
    srcBuffer(toCompress.GetBuffer()),
    rBlockSize(_rBlockSize),
    dBlockSize(2 * rBlockSize),
    rBlockArea(rBlockSize * rBlockSize),
    rBlocksPerSide(size / rBlockSize),
    rBlocksNumber(rBlocksPerSide * rBlocksPerSide),
    dBlocksNumberRoot(size - dBlockSize + 1),
    dBlocksNumber(dBlocksNumberRoot * dBlocksNumberRoot),
    downDValues(new uint8_t[rBlockArea * dBlocksNumber]),
    downDSumTable(new int32_t[dBlocksNumber]),
    downDSqSumTable(new int32_t[dBlocksNumber]),
    rBlockLines(new const uint8_t*[rBlockSize]),
    dBlockLines(new const uint8_t*[rBlockSize]),
    rBlockMappings(new RDBlockMapping[rBlocksNumber])
{
    assert(toCompress.GetWidth() == size);
    assert(toCompress.GetHeight() == size);
    assert(rBlockSize == 4 || rBlockSize == 8);
    prepareDownDValues();
    if (isFastModeEnabled) {
        precalculateDHashes();
    }
}

CFractalImageCompressor::~CFractalImageCompressor() {
    delete [] downDValues;
    delete [] downDSumTable;
    delete [] downDSqSumTable;
    delete [] rBlockLines;
    delete [] dBlockLines;
    delete [] rBlockMappings;
    if (isFastModeEnabled) {
        delete [] hashes;
    }
}

void CFractalImageCompressor::Compress(const std::string& pathToSave) {
    size_t rBlockIndex = 0;
    for (size_t rBlockRow = 0; rBlockRow < rBlocksPerSide; ++rBlockRow) {
        for (size_t rBlockColumn = 0; rBlockColumn < rBlocksPerSide; ++rBlockColumn, ++rBlockIndex) {
            int rBlockSum = 0, rBlockSquaresSum = 0;
            uint8_t hash = 0;
            prepareRBlockStructs(rBlockRow, rBlockColumn, rBlockSum, rBlockSquaresSum, hash);
            const int rBlockSumSquare = rBlockSum * rBlockSum;
            const bool isRBlockVarSmall = (rBlockSquaresSum - rBlockSumSquare / rBlockArea) / rBlockArea < 10;

            auto hashPtr = hashes;
            int minLossValue = std::numeric_limits<int>::max();
            size_t dBlockIndex = 0;
            for (size_t dBlockRow = 0; dBlockRow < dBlocksNumberRoot; ++dBlockRow) {
                for (size_t dBlockColumn = 0; dBlockColumn < dBlocksNumberRoot; ++dBlockColumn, ++dBlockIndex, hashPtr += BO_Count) {
                    dBlockLines[0] = downDValues + dBlockIndex * rBlockArea;
                    for (size_t rowIndex = 1; rowIndex < rBlockSize; ++rowIndex) {
                        dBlockLines[rowIndex] = dBlockLines[rowIndex - 1] + rBlockSize;
                    }
                    const auto dBlockSum = downDSumTable[dBlockIndex];
                    const auto dBlockSquaresSum = downDSqSumTable[dBlockIndex];
                    const int scaleDenominator = rBlockArea * dBlockSquaresSum - dBlockSum * dBlockSum;
                    if (scaleDenominator == 0) {
                        const int currLoss = rBlockSquaresSum - rBlockSumSquare / rBlockArea;
                        if (currLoss < minLossValue) {
                            rBlockMappings[rBlockIndex].Scale = 0;
                            rBlockMappings[rBlockIndex].Bias = rBlockSum / rBlockArea;
                            rBlockMappings[rBlockIndex].Orientation = BO_Rot0;
                            rBlockMappings[rBlockIndex].TopLeftX = dBlockColumn;
                            rBlockMappings[rBlockIndex].TopLeftY = dBlockRow;
                            minLossValue = currLoss;
                        }
                        continue;
                    }
                    const int sumsMultiplied = dBlockSum * rBlockSum;
                    for (size_t dBlockOrientation = 0; dBlockOrientation < BO_Count; ++dBlockOrientation) {
                        if (isFastModeEnabled && hashPtr[dBlockOrientation] != hash && !isRBlockVarSmall) {
                            continue;
                        }
                        const auto orientation = static_cast<TBlockOrientation>(dBlockOrientation);
                        const int blocksConv = getBlocksConvolution(orientation);
                        const int scaleNumerator = rBlockArea * blocksConv - sumsMultiplied;
                        const double scale = static_cast<double>(scaleNumerator) / scaleDenominator;
                        if (scale >= 1.0 || scale < 0.0) {
                            continue;
                        }
                        const int discretizedScale = static_cast<int>(scale * RDBlockMapping::ScaleBase);
                        const int scaledDBlockSum = (dBlockSum * discretizedScale) / RDBlockMapping::ScaleBase;
                        const int biasDiscretized = color_cast<int>((rBlockSum - scaledDBlockSum) / rBlockArea,
                            std::numeric_limits<int8_t>::min(), std::numeric_limits<int8_t>::max());
                        const int loss = rBlockSquaresSum + (dBlockSquaresSum * discretizedScale / RDBlockMapping::ScaleBase -
                            2 * blocksConv + 2 * biasDiscretized * dBlockSum) * discretizedScale / RDBlockMapping::ScaleBase +
                            biasDiscretized * (biasDiscretized * rBlockArea - 2 * rBlockSum);
                        if (loss < minLossValue) {
                            rBlockMappings[rBlockIndex].Scale = discretizedScale;
                            rBlockMappings[rBlockIndex].Bias = biasDiscretized;
                            rBlockMappings[rBlockIndex].Orientation = orientation;
                            rBlockMappings[rBlockIndex].TopLeftX = dBlockColumn;
                            rBlockMappings[rBlockIndex].TopLeftY = dBlockRow;
                            minLossValue = loss;
                        }
                    }
                }
            }
        }
    }
    saveToBinaryFile(pathToSave);
}

// Подготовка необходимых структур по текущему блоку R
inline void CFractalImageCompressor::prepareRBlockStructs(size_t rBlockRow, size_t rBlockColumn, int& rBlockSum,
    int& rBlockSquaresSum, uint8_t& hash)
{
    rBlockLines[0] = srcBuffer + rBlockSize * (rBlockRow * size + rBlockColumn);
    for (size_t lineIndex = 1; lineIndex < rBlockSize; ++lineIndex) {
        rBlockLines[lineIndex] = rBlockLines[lineIndex - 1] + size;
    }
    for (size_t lineIndex = 0; lineIndex < rBlockSize; ++lineIndex) {
        for (size_t columnIndex = 0; columnIndex < rBlockSize; ++columnIndex) {
            const uint8_t value = rBlockLines[lineIndex][columnIndex];
            rBlockSum += value;
            rBlockSquaresSum += value * value;
        }
    }
    if (isFastModeEnabled) {
        int avgIntensities[4] = { 0, 0, 0, 0 };
        const int fullIntensity = calculateIntensities(avgIntensities, rBlockLines[0], rBlockSize);
        hash = calculateHash(avgIntensities, fullIntensity);
    }
}

// Свертка блоков D и R
int CFractalImageCompressor::getBlocksConvolution(TBlockOrientation orientation) const {
    int acc = 0;
    switch (orientation) {
        case BO_Rot0:
            for (size_t rowIndex = 0; rowIndex < rBlockSize; ++rowIndex) {
                for (size_t columnIndex = 0; columnIndex < rBlockSize; ++columnIndex) {
                    acc += rBlockLines[rowIndex][columnIndex] * dBlockLines[rowIndex][columnIndex];
                }
            }
            break;
        case BO_Rot90:
            for (size_t rowIndex = 0; rowIndex < rBlockSize; ++rowIndex) {
                for (size_t columnIndex = 0; columnIndex < rBlockSize; ++columnIndex) {
                    acc += rBlockLines[rowIndex][columnIndex] * dBlockLines[columnIndex][rBlockSize - 1 - rowIndex];
                }
            }
            break;
        case BO_Rot180:
            for (size_t rowIndex = 0; rowIndex < rBlockSize; ++rowIndex) {
                for (size_t columnIndex = 0; columnIndex < rBlockSize; ++columnIndex) {
                    acc += rBlockLines[rowIndex][columnIndex] * dBlockLines[rBlockSize - 1 - rowIndex][rBlockSize - 1 - columnIndex];
                }
            }
            break;
        case BO_Rot270:
            for (size_t rowIndex = 0; rowIndex < rBlockSize; ++rowIndex) {
                for (size_t columnIndex = 0; columnIndex < rBlockSize; ++columnIndex) {
                    acc += rBlockLines[rowIndex][columnIndex] * dBlockLines[rBlockSize - 1 - columnIndex][rowIndex];
                }
            }
            break;
        case BO_MirroredRot0:
            for (size_t rowIndex = 0; rowIndex < rBlockSize; ++rowIndex) {
                for (size_t columnIndex = 0; columnIndex < rBlockSize; ++columnIndex) {
                    acc += rBlockLines[rowIndex][columnIndex] * dBlockLines[rowIndex][rBlockSize - 1 - columnIndex];
                }
            }
            break;
        case BO_MirroredRot90:
            for (size_t rowIndex = 0; rowIndex < rBlockSize; ++rowIndex) {
                for (size_t columnIndex = 0; columnIndex < rBlockSize; ++columnIndex) {
                    acc += rBlockLines[rowIndex][columnIndex] * dBlockLines[rBlockSize - 1 - columnIndex][rBlockSize - 1 - rowIndex];
                }
            }
            break;
        case BO_MirroredRot180:
            for (size_t rowIndex = 0; rowIndex < rBlockSize; ++rowIndex) {
                for (size_t columnIndex = 0; columnIndex < rBlockSize; ++columnIndex) {
                    acc += rBlockLines[rowIndex][columnIndex] * dBlockLines[rBlockSize - 1 - rowIndex][columnIndex];
                }
            }
            break;
        case BO_MirroredRot270:
            for (size_t rowIndex = 0; rowIndex < rBlockSize; ++rowIndex) {
                for (size_t columnIndex = 0; columnIndex < rBlockSize; ++columnIndex) {
                    acc += rBlockLines[rowIndex][columnIndex] * dBlockLines[columnIndex][rowIndex];
                }
            }
            break;
        default:
            assert(false);
    }
    return acc;
}

// Предпосчет сжатых блоков D
void CFractalImageCompressor::prepareDownDValues() {
    auto topLeftDBlockPtr = srcBuffer;
    auto downDBuffer = downDValues;
    size_t dBlockIndex = 0;
    for (size_t rowIndex = 0; rowIndex < dBlocksNumberRoot; ++rowIndex) {
        for (size_t columnIndex = 0; columnIndex < dBlocksNumberRoot; ++columnIndex, ++dBlockIndex) {
            size_t index = 0;
            for (size_t dBlockRow = 0; dBlockRow < rBlockSize; ++dBlockRow) {
                for (size_t dBlockColumn = 0; dBlockColumn < rBlockSize; ++dBlockColumn, ++index) {
                    auto topLeft = topLeftDBlockPtr + 2 * dBlockRow * size + 2 * dBlockColumn;
                    auto topRight = topLeft + 1;
                    auto botLeft = topLeft + size;
                    auto botRight = botLeft + 1;
                    downDBuffer[index] = (*topLeft + *topRight + *botLeft + *botRight + 2) / 4;
                    downDSumTable[dBlockIndex] += downDBuffer[index];
                    downDSqSumTable[dBlockIndex] += downDBuffer[index] * downDBuffer[index];
                }
            }
            downDBuffer += rBlockArea;
            ++topLeftDBlockPtr;
        }
        topLeftDBlockPtr += (dBlockSize - 1);
    }
}

// Вычисление интенсивностей подблоков и общей интенсивности
int CFractalImageCompressor::calculateIntensities(int* subBlockIntensities, const uint8_t* buffer,
    size_t fullBlockSize) const
{
    const size_t subBlockSize = fullBlockSize / 2;
    subBlockIntensities[SBO_TopLeft] = 0;
    for (size_t dBlockRow = 0; dBlockRow < subBlockSize; ++dBlockRow) {
        for (size_t dBlockColumn = 0; dBlockColumn < subBlockSize; ++dBlockColumn) {
            subBlockIntensities[SBO_TopLeft] += *(buffer + 2 * dBlockRow * size + 2 * dBlockColumn);
        }
    }
    subBlockIntensities[SBO_TopRight] = 0;
    for (size_t dBlockRow = 0; dBlockRow < subBlockSize; ++dBlockRow) {
        for (size_t dBlockColumn = subBlockSize; dBlockColumn < fullBlockSize; ++dBlockColumn) {
            subBlockIntensities[SBO_TopRight] += *(buffer + 2 * dBlockRow * size + 2 * dBlockColumn);
        }
    }
    subBlockIntensities[SBO_BotLeft] = 0;
    for (size_t dBlockRow = subBlockSize; dBlockRow < fullBlockSize; ++dBlockRow) {
        for (size_t dBlockColumn = 0; dBlockColumn < subBlockSize; ++dBlockColumn) {
            subBlockIntensities[SBO_BotLeft] += *(buffer + 2 * dBlockRow * size + 2 * dBlockColumn);
        }
    }
    subBlockIntensities[SBO_BotRight] = 0;
    for (size_t dBlockRow = subBlockSize; dBlockRow < fullBlockSize; ++dBlockRow) {
        for (size_t dBlockColumn = subBlockSize; dBlockColumn < fullBlockSize; ++dBlockColumn) {
            subBlockIntensities[SBO_BotRight] += *(buffer + 2 * dBlockRow * size + 2 * dBlockColumn);
        }
    }
    const int blockArea = fullBlockSize * fullBlockSize;
    const int subBlockArea = subBlockSize * subBlockSize;
    int fullIntensity = 0;
    for (size_t subBlockIndex = 0; subBlockIndex < SBO_Count; ++subBlockIndex) {
        fullIntensity += subBlockIntensities[subBlockIndex];
        subBlockIntensities[subBlockIndex] = (subBlockIntensities[subBlockIndex] + subBlockArea / 2) / subBlockArea;
    }
    fullIntensity = (fullIntensity + blockArea / 2) / blockArea;
    return fullIntensity;
}

// Предпосчет хэшей
void CFractalImageCompressor::precalculateDHashes() {
    assert(isFastModeEnabled);
    hashes = new uint8_t[BO_Count * dBlocksNumber];
    auto blockHashesPtr = hashes;
    auto topLeftDBlockPtr = srcBuffer;
    size_t dBlockIndex = 0;
    for (size_t rowIndex = 0; rowIndex < dBlocksNumberRoot; ++rowIndex) {
        for (size_t columnIndex = 0; columnIndex < dBlocksNumberRoot; ++columnIndex, ++dBlockIndex) {
            int avgIntensities[4] = { 0, 0, 0, 0 };
            const int fullIntensity = calculateIntensities(avgIntensities, topLeftDBlockPtr, dBlockSize);
            for (size_t orientationIndex = 0; orientationIndex < BO_Count; ++orientationIndex) {
                blockHashesPtr[orientationIndex] = calculateHash(avgIntensities, fullIntensity,
                    static_cast<TBlockOrientation>(orientationIndex));
            }
            blockHashesPtr += BO_Count;
            ++topLeftDBlockPtr;
        }
        topLeftDBlockPtr += (dBlockSize - 1);
    }
}

// Сериализация сжатого представления
void CFractalImageCompressor::saveToBinaryFile(const std::string& pathToSave) const {
    std::ofstream out;
    out.open(pathToSave, std::ios::binary);
    out.write(reinterpret_cast<const char*>(&rBlockSize), sizeof(rBlockSize));
    out.write(reinterpret_cast<const char*>(rBlockMappings), sizeof(RDBlockMapping) * rBlocksNumber);
    out.close();
}