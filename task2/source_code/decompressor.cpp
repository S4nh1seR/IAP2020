#include "fractal.h"
#include <cassert>
#include <fstream>
#include <random>
#include <iostream>

CFractalImageDecompressor::CFractalImageDecompressor(const std::string& pathToCompressed) {
    loadFromBinaryFile(pathToCompressed);
    rBlockLines = new uint8_t*[rBlockSize];
}

CFractalImageDecompressor::~CFractalImageDecompressor() {
    delete [] rBlockMappings;
    delete [] rBlockLines;
}

std::shared_ptr<CGrayImage> CFractalImageDecompressor::Decompress(size_t iterationsNumber,
    const std::string& folderPathToSaveIntermediate, const CGrayImage& reference)
{
    std::shared_ptr<CGrayImage> prevImage(new CGrayImage(size, size));
    std::shared_ptr<CGrayImage> currImage(new CGrayImage(size, size));
    randomInitialize(*currImage);

    for (size_t iteration = 0; iteration < iterationsNumber; ++iteration) {
        prevImage->SwapImage(*currImage);
        size_t rBlockIndex = 0;
        for (size_t rBlockRow = 0; rBlockRow < rBlocksPerSide; ++rBlockRow) {
            for (size_t rBlockColumn = 0; rBlockColumn < rBlocksPerSide; ++rBlockColumn, ++rBlockIndex) {
                prepareRBlockLines(*currImage, rBlockRow, rBlockColumn);
                applyMapping(*prevImage, rBlockIndex);
            }
        }
        onIterationEnd(iteration, folderPathToSaveIntermediate, reference, *currImage);
    }
    return currImage;
}

// Подготовка строк текущего R блока
void CFractalImageDecompressor::prepareRBlockLines(CGrayImage& dstImage, size_t rBlockRow, size_t rBlockColumn) {
    rBlockLines[0] = dstImage.GetBuffer() + rBlockSize * (rBlockRow * size + rBlockColumn);
    for (size_t lineIndex = 1; lineIndex < rBlockSize; ++lineIndex) {
        rBlockLines[lineIndex] = rBlockLines[lineIndex - 1] + size;
    }
}

// Применение отображения к одному блоку
void CFractalImageDecompressor::applyMapping(const CGrayImage& sourceImage, size_t rBlockIndex) {
    const RDBlockMapping& mapping = rBlockMappings[rBlockIndex];
    const auto scale = mapping.Scale;
    const auto bias = mapping.Bias;
    const auto orientation = static_cast<TBlockOrientation>(mapping.Orientation);
    const auto buffer = sourceImage.GetBuffer() + rBlockMappings[rBlockIndex].TopLeftY * size +
        rBlockMappings[rBlockIndex].TopLeftX;;
    for (size_t rowIndex = 0; rowIndex < rBlockSize; ++rowIndex) {
        for (size_t columnIndex = 0; columnIndex < rBlockSize; ++columnIndex) {
            auto topLeft = getTopLeftBlockPtr(buffer, rowIndex, columnIndex, orientation);
            auto topRight = topLeft + 1;
            auto botLeft = topLeft + size;
            auto botRight = botLeft + 1;
            rBlockLines[rowIndex][columnIndex] = color_cast((((*topLeft + *topRight + *botLeft + *botRight + 2) / 4) *
                scale + RDBlockMapping::ScaleBase / 2) / RDBlockMapping::ScaleBase + bias);
        }
    }
}

// Случайная инициализация начального изображения
void CFractalImageDecompressor::randomInitialize(CGrayImage& toInitialize) {
    std::random_device device;
    std::mt19937 random(device());
    std::uniform_int_distribution<int> generator(std::numeric_limits<uint8_t>::min(), std::numeric_limits<uint8_t>::max());
    auto currImageBuffer = toInitialize.GetBuffer();
    for (size_t pixelIndex = 0; pixelIndex < size * size; ++pixelIndex) {
        currImageBuffer[pixelIndex] = generator(random);
    }
}

// Коллбэк на конец итерации - логирует результаты
void CFractalImageDecompressor::onIterationEnd(size_t iteration, const std::string& pathToResultsFolder,
    const CGrayImage& reference, const CGrayImage& currentRetrieved)
{
    if (!pathToResultsFolder.empty()) {
        const std::string& pathToSaveImage = pathToResultsFolder + "/result_" + std::to_string(iteration) + ".bmp";
        currentRetrieved.SaveToFile(pathToSaveImage);
        if (!reference.IsEmpty()) {
            const std::string& pathToSaveMetrics = pathToResultsFolder + "/metrics_" + std::to_string(iteration) + ".txt";
            const CMetrics& metrics = CalculateMetrics(currentRetrieved, reference);
            metrics.SaveToFile(pathToSaveMetrics);
        }
    }
}

// Сериализация фрактального представления изображения из файла на диске
void CFractalImageDecompressor::loadFromBinaryFile(const std::string& pathToBinary) {
    std::ifstream in;
    in.open(pathToBinary, std::ios::binary);
    in.read(reinterpret_cast<char*>(&rBlockSize), sizeof(rBlockSize));
    rBlocksPerSide = size / rBlockSize;
    rBlocksNumber = rBlocksPerSide * rBlocksPerSide;
    rBlockMappings = new RDBlockMapping[rBlocksNumber];
    in.read(reinterpret_cast<char*>(rBlockMappings), rBlocksNumber * sizeof(RDBlockMapping));
    in.close();
}

// Получить указатель на верхний левый угол подблока 2x2 блока D
inline const uint8_t* CFractalImageDecompressor::getTopLeftBlockPtr(const uint8_t* buffer,
    size_t rowIndex, size_t columnIndex, TBlockOrientation orientation) const
{
    switch(orientation) {
        case BO_Rot0:
            return buffer + 2 * size * rowIndex + 2 * columnIndex;
        case BO_Rot90:
            return buffer + 2 * size * columnIndex + 2 * (rBlockSize - 1 - rowIndex);
        case BO_Rot180:
            return buffer + 2 * size * (rBlockSize - 1 - rowIndex) + 2 * (rBlockSize - 1 - columnIndex);
        case BO_Rot270:
            return buffer + 2 * size * (rBlockSize - 1 - columnIndex) + 2 * rowIndex;
        case BO_MirroredRot0:
            return buffer + 2 * size * rowIndex + 2 * (rBlockSize - 1 - columnIndex);
        case BO_MirroredRot90:
            return buffer + 2 * size * (rBlockSize - 1 - columnIndex) + 2 * (rBlockSize - 1 - rowIndex);;
        case BO_MirroredRot180:
            return buffer + 2 * size * (rBlockSize - 1 - rowIndex) + 2 * columnIndex;
        case BO_MirroredRot270:
            return buffer + 2 * size * columnIndex + 2 * rowIndex;
        default:
            assert(false);
            return nullptr;
    }
}
