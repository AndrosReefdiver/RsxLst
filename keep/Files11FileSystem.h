#pragma once

#include "FileStructures.h"
#include <string>
#include <vector>
#include <map>
#include <cstdint>

// RSX-11M Files-11 ODS-1 File System class

class Files11FileSystem {
private:
    std::string imagePath;
    RSX11M_HomeBlock homeBlock;
    uint32_t indexBitmapLBN;
    std::map<uint16_t, uint32_t> fileHeaderCache; // file_num -> LBN
    bool debugMode;
    
public:
    Files11FileSystem(const std::string& path);
    
    // Initialize filesystem and read home block
    bool initialize();
    
    // Enable/disable debug output
    void setDebugMode(bool enable);
    
    // Read file header for a given file number
    bool readFileHeader(uint16_t fileNum, FileHeader& header);
    
    // Get retrieval pointers from file header
    std::vector<std::pair<uint32_t, uint32_t>> getRetrievalPointers(const FileHeader& header, bool verbose = false);
    
    // Get all retrieval pointers including extension headers
    std::vector<std::pair<uint32_t, uint32_t>> getAllRetrievalPointers(uint16_t fileNum, bool verbose = false);
    
    // Calculate total file size in blocks
    uint32_t getFileSize(uint16_t fileNum);
    
    // Read file data
    std::vector<uint8_t> readFile(uint16_t fileNum);
    
    // List directory entries
    bool listDirectory(int group, int member, bool verbose = false);
    
    // Copy file from Files-11 to local filesystem
    bool copyFromFiles11(int group, int member, const std::string& filename, 
                         const std::string& type, int version, const std::string& outputPath);
    
    // Copy file from local filesystem to Files-11
    bool copyToFiles11(int group, int member, const std::string& localPath, const std::string& filename);
    
    // Copy file from Files-11 to host filesystem (legacy extract command)
    bool extractFile(int group, int member, const std::string& filename, 
                     const std::string& type, int version, const std::string& outputPath);
    
    // Display home block information
    void displayHomeBlock();
    
    // Display file header details
    void displayFileHeader(uint16_t fileNum);
    
    // Display directory entry details
    void displayDirectoryEntry(int group, int member, const std::string& filename, const std::string& type, int version);

private:
    // Helper methods for copyto functionality
    std::vector<uint32_t> allocateBlocks(uint32_t count);
    bool writeFileData(const std::vector<uint32_t>& lbns, const std::vector<uint8_t>& data);
    bool createFileHeader(uint16_t fileNum, uint16_t ownerUIC, uint32_t fileSize, const std::vector<uint32_t>& lbns);
    bool addDirectoryEntry(uint16_t ufdFileNum, const std::string& filename, const std::string& type, int version, uint16_t fileNum);
    uint16_t allocateFileNumber();
    bool readStorageBitmap(std::vector<uint8_t>& bitmap);
    bool writeStorageBitmap(const std::vector<uint8_t>& bitmap);
};
