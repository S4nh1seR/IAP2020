#include "image.h"
#include <cassert>
#include <tiffio.h>
#include <iostream>

template<>
void CImage<IC_BW>::LoadFromFile(const std::string& sourceFilePath) {
    assert(false);
}

template<>
void CImage<IC_BW>::SaveToFile(const std::string& sourceFilePath) const {
    const char ext[] = ".tiff";
    assert(sourceFilePath.substr(sourceFilePath.size() - sizeof(ext) + 1, sizeof(ext)) == ext);
    const size_t packedBytesPerRow = (width + 7) / 8;
    const size_t packedBufferSize = packedBytesPerRow * height;
    uint8_t* packedBuffer = new uint8_t[packedBufferSize];
    for (size_t byteIndex = 0; byteIndex < packedBufferSize; ++byteIndex) {
        packedBuffer[byteIndex] = 0;
    }
    size_t srcBufferIndex = 0;
    for (size_t rowIndex = 0; rowIndex < height; ++rowIndex) {
        auto packedRowBuffer = packedBuffer + rowIndex * packedBytesPerRow;
        for (size_t columnIndex = 0; columnIndex < width; ++columnIndex, ++srcBufferIndex) {
            if (dataBuffer[srcBufferIndex] != 0) {
                packedRowBuffer[columnIndex / 8] |= (1u << (7 - (columnIndex % 8)));
            }
        }
    }
    TIFF* tiffFile = TIFFOpen(sourceFilePath.c_str(), "w");
    TIFFSetField(tiffFile, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(tiffFile, TIFFTAG_IMAGELENGTH, height);
    TIFFSetField(tiffFile, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(tiffFile, TIFFTAG_BITSPERSAMPLE, 1);
    TIFFSetField(tiffFile, TIFFTAG_COMPRESSION, COMPRESSION_CCITTFAX3);
    TIFFSetField(tiffFile, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
    TIFFSetField(tiffFile, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB);
    TIFFSetField(tiffFile, TIFFTAG_ROWSPERSTRIP, 1);
    TIFFSetField(tiffFile, TIFFTAG_XRESOLUTION, 300.0f);
    TIFFSetField(tiffFile, TIFFTAG_YRESOLUTION, 300.0f);
    for (size_t rowIndex = 0; rowIndex < height; ++rowIndex) {
        TIFFWriteScanline(tiffFile, (void*)(packedBuffer + packedBytesPerRow * rowIndex), rowIndex);
    }
    TIFFClose(tiffFile);
    delete [] packedBuffer;
}

template class CImage<IC_BW>;
