#pragma once

#include "image.h"
#include <memory>
#include <string>
#include <limits>

// Возможные ориентации блока
// 1. Поворот задается по часовой стрелке
// 2. Отражение обозначает обход в противоположном направлении
// (справа налево, если правильно ориентировать блок по повороту)
enum TBlockOrientation : unsigned char {
    BO_Rot0,
    BO_Rot90,
    BO_Rot180,
    BO_Rot270,
    BO_MirroredRot0,
    BO_MirroredRot90,
    BO_MirroredRot180,
    BO_MirroredRot270,

    BO_Count
};

// "Единица" сжимающего фрактального отображения - задается на одном блоке. Занимает 4 байта
struct RDBlockMapping {
    typedef uint8_t pos_type;

    // Координаты найденного прообраза - блока D
    pos_type TopLeftX;
    pos_type TopLeftY;
    // Ориентация блока
    uint8_t Orientation : 3;
    // Параметр масштаба яркостного преобразования (Храним дискрет. значение по основанию 32).
    // берем Scale < 1, иначе преобразование может быть не сжимающим
    uint8_t Scale : 5;
    // Параметр сдвига яркостного преобразования, дискретизован в диапазоне [-128, 127]
    int8_t Bias;

    // Основание, по которому храним параметр масштаба
    static constexpr int ScaleBase = 32;
};
static_assert(sizeof(RDBlockMapping) == 4);

// Механизмы работают на фиксированном размере
// Код легко обобщить, но в рамках данной реализации мы этого делать не будем
static constexpr int size = 256;
static_assert(std::numeric_limits<RDBlockMapping::pos_type>::max() + 1 >= size);

//////////////////////////////////////////////////////////////////////////////////////////////////

// Энкодер полутонового изображения во фрактальное представление
class CFractalImageCompressor {
public:
    // Размер блока = 4 или 8 (assert).
    // Быстрый режим - ускоренный поиск блока-прообраза D только по блокам с таким же хэшом.
    explicit CFractalImageCompressor(const CGrayImage& toCompress, int rBlockSize = 4, bool isFastModeEnabled = false);
    ~CFractalImageCompressor();

    // Основной метод фрактального сжатия - сохраняет бинарный файл на диск по переданному пути
    void Compress(const std::string& pathToSave);

private:
    // Включен ли "быстрый" режим
    bool isFastModeEnabled;
    // Буфер обрабатываемого изображения
    const uint8_t* srcBuffer;
    // Размер блока R (по одной стороне)
    const int rBlockSize;
    // Размер блока D (не сжатого)
    const int dBlockSize;
    // Площадь блока R
    const int rBlockArea;
    // Количество столбцов(строк) блоков R вдоль одной из сторон изображения
    const int rBlocksPerSide;
    // Общее число блоков R
    const int rBlocksNumber;
    // Количество блоков D, приходящееся на одну сторону изображения
    const int dBlocksNumberRoot;
    // Общее количество блоков D
    const int dBlocksNumber;

    // Предпосчитанные сжатые блоки D
    uint8_t* downDValues;
    // Предпосчитанные суммы по сжатым блокам D
    int* downDSumTable;
    // Предподсчитанные суммы квадратов в сжатых блоках D
    int* downDSqSumTable;
    // Хэши блоков D, для всех ориентаций
    uint8_t* hashes{nullptr};

    // Указатели на строки текущих рассматриваемых блоков R и D (сжатого)
    const uint8_t** rBlockLines;
    const uint8_t** dBlockLines;
    // Выстраеваемые для блоков R прообразы
    RDBlockMapping* rBlockMappings;

    void prepareRBlockStructs(size_t rBlockRow, size_t rBlockColumn, int& rBlockSum, int& rBlockSquaresSum, uint8_t& hash);
    void prepareDownDValues();
    int calculateIntensities(int* subBlockIntensities, const uint8_t* buffer, size_t fullBlockSize) const;
    void precalculateDHashes();
    int getBlocksConvolution(TBlockOrientation orientation) const;
    void saveToBinaryFile(const std::string& pathToSave) const;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

// Декодер полутонового изображения из фрактального представления
class CFractalImageDecompressor {
public:
    explicit CFractalImageDecompressor(const std::string& pathToCompressed);
    ~CFractalImageDecompressor();

    // Восстановление изображения заданным количеством итераций
    // Осуществляет дополнительный дамп промежуточных изображений и метрик на диск (опционально)
    std::shared_ptr<CGrayImage> Decompress(size_t iterationsNumber, const std::string& folderPathToSaveResults = "",
        const CGrayImage& reference = CGrayImage());

private:
    // Размер блока R (считывается первым из файла)
    int rBlockSize;
    // Число блоков на одну сторону изображения
    int rBlocksPerSide;
    // Общее число блоков
    int rBlocksNumber;
    // Отображения блоков, считанные из файла
    RDBlockMapping* rBlockMappings;
    // Строки заполняемого блока R
    uint8_t** rBlockLines;

    static void randomInitialize(CGrayImage& toInitialize);
    void prepareRBlockLines(CGrayImage& dstImage, size_t rBlockRow, size_t rBlockColumn);
    void applyMapping(const CGrayImage& sourceImage, size_t rBlockIndex);
    static void onIterationEnd(size_t iteration, const std::string& pathToResultsFolder,
        const CGrayImage& reference, const CGrayImage& currentRetrieved);
    void loadFromBinaryFile(const std::string& pathToBinary);
    const uint8_t* getTopLeftBlockPtr(const uint8_t* buffer, size_t rowIndex, size_t columnIndex,
        TBlockOrientation orientation) const;
};