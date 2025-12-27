#include "DiskIO.h"
#include "FileStructures.h"

// Read a block from disk image
bool readBlock(std::ifstream& img, uint32_t lbn, uint8_t* buffer) {
    img.seekg(static_cast<std::streamoff>(lbn) * BLOCK_SIZE, std::ios::beg);
    if (!img) return false;
    img.read(reinterpret_cast<char*>(buffer), BLOCK_SIZE);
    return img.gcount() == BLOCK_SIZE;
}

// Write a block to disk image
bool writeBlock(std::ofstream& img, uint32_t lbn, const uint8_t* buffer) {
    img.seekp(static_cast<std::streamoff>(lbn) * BLOCK_SIZE, std::ios::beg);
    if (!img) return false;
    img.write(reinterpret_cast<const char*>(buffer), BLOCK_SIZE);
    return img.good();
}

// Write a block to disk image (fstream overload)
bool writeBlock(std::fstream& img, uint32_t lbn, const uint8_t* buffer) {
    img.seekp(static_cast<std::streamoff>(lbn) * BLOCK_SIZE, std::ios::beg);
    if (!img) return false;
    img.write(reinterpret_cast<const char*>(buffer), BLOCK_SIZE);
    return img.good();
}

// Get word from buffer (little-endian)
uint16_t getWord(const uint8_t* buf, int offset) {
    return buf[offset] | (buf[offset + 1] << 8);
}

// Set word in buffer (little-endian)
void setWord(uint8_t* buf, int offset, uint16_t value) {
    buf[offset] = value & 0xFF;
    buf[offset + 1] = (value >> 8) & 0xFF;
}
