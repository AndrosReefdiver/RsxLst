#include "Files11FileSystem.h"
#include "Rad50.h"
#include "DiskIO.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstring>
#include <ctime>

// Constructor
Files11FileSystem::Files11FileSystem(const std::string& path) 
    : imagePath(path), indexBitmapLBN(0), debugMode(false) {
}

// Initialize filesystem and read home block
bool Files11FileSystem::initialize() {
    std::ifstream img(imagePath, std::ios::binary);
    if (!img) {
        std::cerr << "Error: Cannot open disk image: " << imagePath << std::endl;
        return false;
    }
    
    // Read home block
    uint8_t buffer[BLOCK_SIZE];
    if (!readBlock(img, HOME_BLOCK_LBN, buffer)) {
        std::cerr << "Error: Cannot read home block" << std::endl;
        return false;
    }
    
    memcpy(&homeBlock, buffer, sizeof(homeBlock));
    indexBitmapLBN = ((uint32_t)homeBlock.hm_iblb_high << 16) | homeBlock.hm_iblb_low;
    
    // Clean volume name - handle non-printable characters
    std::string volname;
    for (int i = 0; i < 12; i++) {
        char c = homeBlock.hm_vnam[i];
        if (c == 0) break;
        if (c >= 32 && c <= 126) {
            volname += c;
        } else {
            volname += ' ';
        }
    }
    // Trim trailing spaces
    size_t end = volname.find_last_not_of(' ');
    if (end != std::string::npos) {
        volname = volname.substr(0, end + 1);
    }
    
    std::cout << "Files-11 Volume: " << volname << std::endl;
    std::cout << "Structure Level: " << homeBlock.hm_vlev;
    if (homeBlock.hm_vlev == 257 || homeBlock.hm_vlev == 0401) {
        std::cout << " (ODS-1)" << std::endl;
    } else {
        std::cout << " (Unknown)" << std::endl;
    }
    
    return true;
}

// Enable/disable debug output
void Files11FileSystem::setDebugMode(bool enable) {
    debugMode = enable;
}

// Read file header for a given file number
bool Files11FileSystem::readFileHeader(uint16_t fileNum, FileHeader& header) {
    std::ifstream img(imagePath, std::ios::binary);
    if (!img) return false;
    
    uint32_t headerLBN = indexBitmapLBN + homeBlock.hm_ibsz + fileNum - 1;
    
    if (debugMode && fileNum == 4) {
        std::cout << "Debug: Reading MFD header (file #4)" << std::endl;
        std::cout << "Debug: Index bitmap LBN: " << indexBitmapLBN << std::endl;
        std::cout << "Debug: Index bitmap size: " << homeBlock.hm_ibsz << " blocks" << std::endl;
        std::cout << "Debug: MFD header LBN: " << headerLBN << std::endl;
    }
    
    uint8_t buffer[BLOCK_SIZE];
    if (!readBlock(img, headerLBN, buffer)) {
        return false;
    }
    
    memcpy(&header, buffer, sizeof(FileHeader));
    
    if (header.h_fnum != fileNum) {
        if (debugMode && fileNum == 4) {
            std::cout << "Debug: Header file number mismatch! Expected " << fileNum 
                      << ", got " << header.h_fnum << std::endl;
        }
        return false;
    }
    
    fileHeaderCache[fileNum] = headerLBN;
    return true;
}

// Get retrieval pointers from file header
std::vector<std::pair<uint32_t, uint32_t>> Files11FileSystem::getRetrievalPointers(const FileHeader& header, bool verbose) {
    std::vector<std::pair<uint32_t, uint32_t>> pointers;
    
    int mapOffset = header.h_mpof * 2;
    bool showDebug = debugMode && verbose;
    
    if (mapOffset >= BLOCK_SIZE || mapOffset < 28) {
        if (showDebug) std::cerr << "Debug: Invalid map offset: " << mapOffset << std::endl;
        return pointers;
    }
    
    const uint8_t* mapArea = reinterpret_cast<const uint8_t*>(&header) + mapOffset;
    int mapSize = BLOCK_SIZE - mapOffset;
    
    if (mapSize < 10) {
        if (showDebug) std::cerr << "Debug: Map area too small: " << mapSize << std::endl;
        return pointers;
    }
    
    uint8_t ctsz = mapArea[6];
    uint8_t lbsz = mapArea[7];
    
    int ptrSize = ctsz + lbsz;
    if (ptrSize == 0 || ptrSize > 6 || ctsz == 0 || lbsz == 0) {
        if (showDebug) {
            std::cerr << "Debug: Invalid pointer format - CTSZ=" << (int)ctsz << " LBSZ=" << (int)lbsz << std::endl;
        }
        return pointers;
    }
    
    int offset = 10;
    
    while (offset + ptrSize <= mapSize) {
        if (ctsz == 1 && lbsz == 3) {
            // Non-standard format: [lbn_hi][count][lbn_mid][lbn_lo]
            uint8_t byte0 = mapArea[offset];
            uint8_t byte1 = mapArea[offset + 1];
            uint8_t byte2 = mapArea[offset + 2];
            uint8_t byte3 = mapArea[offset + 3];
            
            if (byte0 == 0 && byte1 == 0 && byte2 == 0 && byte3 == 0) {
                break;
            }
            
            uint32_t count = byte1 + 1;
            uint32_t lbn = ((uint32_t)byte0 << 16) | ((uint32_t)byte3 << 8) | (uint32_t)byte2;
            
            pointers.push_back(std::make_pair(lbn, count));
            offset += 4;
        }
        else {
            uint32_t count = 0;
            for (int i = 0; i < ctsz; i++) {
                count |= (mapArea[offset + i] << (i * 8));
            }
            
            uint32_t lbn = 0;
            for (int i = 0; i < lbsz; i++) {
                lbn |= (mapArea[offset + ctsz + i] << (i * 8));
            }
            
            if (count == 0 || lbn == 0) break;
            
            pointers.push_back(std::make_pair(lbn, count));
            offset += ptrSize;
        }
    }
    
    return pointers;
}

// Get all retrieval pointers including extension headers
std::vector<std::pair<uint32_t, uint32_t>> Files11FileSystem::getAllRetrievalPointers(uint16_t fileNum, bool verbose) {
    std::vector<std::pair<uint32_t, uint32_t>> allPointers;
    
    FileHeader header;
    if (!readFileHeader(fileNum, header)) {
        return allPointers;
    }
    
    auto pointers = getRetrievalPointers(header, verbose);
    allPointers.insert(allPointers.end(), pointers.begin(), pointers.end());
    
    int mapOffset = header.h_mpof * 2;
    if (mapOffset >= BLOCK_SIZE || mapOffset < 46) {
        return allPointers;
    }
    
    const uint8_t* headerBytes = reinterpret_cast<const uint8_t*>(&header);
    const uint8_t* mapArea = headerBytes + mapOffset;
    
    uint16_t extFileNum = mapArea[2] | (mapArea[3] << 8);
    uint16_t extFileSeq = mapArea[4] | (mapArea[5] << 8);
    
    int maxExtensions = 100;
    int extensionCount = 0;
    
    while (extFileNum != 0 && extensionCount < maxExtensions) {
        FileHeader extHeader;
        if (!readFileHeader(extFileNum, extHeader)) {
            break;
        }
        
        auto extPointers = getRetrievalPointers(extHeader, verbose);
        allPointers.insert(allPointers.end(), extPointers.begin(), extPointers.end());
        
        int extMapOffset = extHeader.h_mpof * 2;
        if (extMapOffset >= BLOCK_SIZE || extMapOffset < 46) {
            break;
        }
        
        const uint8_t* extHeaderBytes = reinterpret_cast<const uint8_t*>(&extHeader);
        const uint8_t* extMapArea = extHeaderBytes + extMapOffset;
        
        extFileNum = extMapArea[2] | (extMapArea[3] << 8);
        extFileSeq = extMapArea[4] | (extMapArea[5] << 8);
        
        extensionCount++;
    }
    
    if (extensionCount >= maxExtensions) {
        std::cerr << "Warning: Maximum extension chain length exceeded for file #" << fileNum << std::endl;
    }
    
    return allPointers;
}

// Calculate total file size in blocks
uint32_t Files11FileSystem::getFileSize(uint16_t fileNum) {
    FileHeader header;
    if (!readFileHeader(fileNum, header)) {
        return 0;
    }
    
    // RSX-11M+ stores file size in blocks at UFAT[6-7] (confirmed from native file)
    uint16_t fileSizeBlocks = header.h_ufat[6] | (header.h_ufat[7] << 8);
    
    // If we have a valid block count, use it
    if (fileSizeBlocks > 0 && fileSizeBlocks < 10000) {
        return fileSizeBlocks;
    }
    
    // Fallback: Try allocated blocks from UFAT[10-11]
    uint16_t allocatedBlocks = header.h_ufat[10] | (header.h_ufat[11] << 8);
    
    if (allocatedBlocks > 0 && allocatedBlocks < 10000) {
        return allocatedBlocks;
    }
    
    // Final fallback: Calculate from retrieval pointers
    auto pointers = getAllRetrievalPointers(fileNum, false);
    uint32_t totalBlocks = 0;
    for (const auto& ptr : pointers) {
        totalBlocks += ptr.second;
    }
    return totalBlocks;
}

// Read file data
std::vector<uint8_t> Files11FileSystem::readFile(uint16_t fileNum) {
    std::vector<uint8_t> data;
    
    FileHeader header;
    if (!readFileHeader(fileNum, header)) {
        std::cerr << "Error: Cannot read file header" << std::endl;
        return data;
    }
    
    uint32_t fileSizeBlocks = getFileSize(fileNum);
    
    if (fileSizeBlocks == 0) {
        std::cerr << "Error: File size is 0 blocks" << std::endl;
        return data;
    }
    
    auto pointers = getAllRetrievalPointers(fileNum, false);
    
    if (!pointers.empty() && pointers[0].first > 20000) {
        std::cerr << "Error: Cannot read file data - retrieval pointer decoding issue" << std::endl;
        std::cerr << "       (LBN " << pointers[0].first << " is beyond disk bounds)" << std::endl;
        std::cerr << "       File size from UFAT: " << fileSizeBlocks << " blocks" << std::endl;
        return data;
    }
    
    if (pointers.empty()) {
        std::cerr << "Error: No retrieval pointers found for file #" << fileNum << std::endl;
        return data;
    }
    
    std::ifstream img(imagePath, std::ios::binary);
    if (!img) {
        std::cerr << "Error: Cannot open disk image" << std::endl;
        return data;
    }
    
    for (const auto& ptr : pointers) {
        uint32_t startLBN = ptr.first;
        uint32_t blockCount = ptr.second;
        
        if (startLBN > 20000) {
            std::cerr << "Warning: Skipping suspicious LBN " << startLBN << std::endl;
            continue;
        }
        
        for (uint32_t i = 0; i < blockCount; i++) {
            uint8_t buffer[BLOCK_SIZE];
            if (readBlock(img, startLBN + i, buffer)) {
                data.insert(data.end(), buffer, buffer + BLOCK_SIZE);
            } else {
                std::cerr << "Warning: Failed to read block at LBN " << (startLBN + i) << std::endl;
            }
        }
    }
    
    return data;
}

// List directory entries
bool Files11FileSystem::listDirectory(int group, int member, bool verbose) {
    FileHeader mfdHeader;
    if (!readFileHeader(4, mfdHeader)) {
        std::cerr << "Error: Cannot read MFD header" << std::endl;
        return false;
    }
    
    auto mfdData = readFile(4);
    if (mfdData.empty()) {
        std::cerr << "Error: Cannot read MFD data" << std::endl;
        return false;
    }
    
    char ufdName[16];
    snprintf(ufdName, sizeof(ufdName), "%03o%03o", group, member);
    std::string ufdNameStr = ufdName;
    
    uint16_t ufdFileNum = 0;
    
    for (size_t i = 0; i + sizeof(DirEntry) <= mfdData.size(); i += sizeof(DirEntry)) {
        DirEntry* entry = reinterpret_cast<DirEntry*>(&mfdData[i]);
        
        if (entry->file_id[0] == 0) continue;
        
        std::string fname = decodeRad50(entry->fname, 3);
        std::string ftype = decodeRad50(&entry->ftype, 1);
        
        if (fname == ufdNameStr && ftype == "DIR") {
            ufdFileNum = entry->file_id[0];
            break;
        }
    }
    
    if (ufdFileNum == 0) {
        std::cerr << "Error: Directory [" << std::oct << std::setfill('0') 
                  << std::setw(3) << group << "," << std::setw(3) << member 
                  << "] not found" << std::dec << std::endl;
        return false;
    }
    
    auto ufdData = readFile(ufdFileNum);
    if (ufdData.empty()) {
        std::cerr << "Error: Cannot read UFD data" << std::endl;
        return false;
    }
    
    std::cout << "\n=== Directory [" << std::oct << std::setfill('0') 
              << std::setw(3) << group << "," << std::setw(3) << member 
              << "] ===" << std::dec << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    
    std::cout << std::setfill(' ');
    std::cout << std::left 
              << std::setw(15) << "Filename" 
              << std::setw(5) << "Type" 
              << std::setw(8) << "Version"
              << std::setw(6) << "File#"
              << std::setw(10) << "Blocks"
              << std::setw(11) << "Size"
              << std::setw(15) << "Date"
              << std::setw(10) << " Owner" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    
    int fileCount = 0;
    uint32_t totalBlocks = 0;
    for (size_t i = 0; i + sizeof(DirEntry) <= ufdData.size(); i += sizeof(DirEntry)) {
        DirEntry* entry = reinterpret_cast<DirEntry*>(&ufdData[i]);
        
        if (entry->file_id[0] == 0) continue;
        
        std::string fname = decodeRad50(entry->fname, 3);
        std::string ftype = decodeRad50(&entry->ftype, 1);
        
        if (fname.empty() || fname == "      " ) continue;
        
        FileHeader fileHdr;
        std::string ownerUIC = "N/A";
        std::string dateStr = "";
        uint32_t fileBlocks = 0;
        uint32_t fileBytes = 0;
        
        if (readFileHeader(entry->file_id[0], fileHdr)) {
            int fgrp = (fileHdr.h_fown >> 8) & 0xFF;
            int fmem = fileHdr.h_fown & 0xFF;
            
            char uicBuf[16];
            snprintf(uicBuf, sizeof(uicBuf), "[%03o,%03o]", fgrp, fmem);
            ownerUIC = uicBuf;
            
            fileBlocks = getFileSize(entry->file_id[0]);
            fileBytes = fileBlocks * BLOCK_SIZE;
            totalBlocks += fileBlocks;
            
            // Read date from UFAT[24-37] (bytes 38-51 of header)
            // Format: "DDMMM<YYHHMM<SS" where YY is years since 1970
            char dateBytes[14];
            memset(dateBytes, 0, sizeof(dateBytes));
            
            // Copy date bytes (may not be null-terminated)
            for (int i = 0; i < 13; i++) {
                //dateBytes[i] = fileHdr.h_ufat[12 + i];
                dateBytes[i] = fileHdr.data[12 + i];
            }
            dateBytes[13] = '\0'; // Ensure null termination
            
            // Check if we have a valid date string
            bool hasDate = false;
            if (dateBytes[0] >= '0' && dateBytes[0] <= '9' &&
                dateBytes[1] >= '0' && dateBytes[1] <= '9') {
                hasDate = true;
            }
            
            if (hasDate) {
                // Parse the date string "DDMMM<YYHHMM<SS"
                if (strlen(dateBytes) >= 11) {
                    char day[3] = {dateBytes[0], dateBytes[1], 0};
                    char month[4] = {dateBytes[2], dateBytes[3], dateBytes[4], 0};
                    char year[3] = {dateBytes[5], dateBytes[6], 0};
                    char hour[3] = {dateBytes[7], dateBytes[8], 0};
                    char minute[3] = {dateBytes[9], dateBytes[10], 0};
                    
                    int units = year[1] - '0';
                    int tens  = year[0] - '0';
                    int offset = tens * 10 + units;
                    int iyear = 1900 + offset;
                    //int iyear = atoi(year) + 1970; // Convert from years since 1970
                    
                    char dateBuf[20];
                    snprintf(dateBuf, sizeof(dateBuf), "%s-%s-%02d %s:%s", 
                             day, month, iyear % 100, hour, minute);
                    dateStr = dateBuf;
                }
            }
        }
        
        std::string sizeStr;
        if (fileBytes < 1024) {
            sizeStr = std::to_string(fileBytes) + "B";
        } else if (fileBytes < 1024 * 1024) {
            sizeStr = std::to_string(fileBytes / 1024) + "K";
        } else {
            sizeStr = std::to_string(fileBytes / (1024 * 1024)) + "M";
        }
        
        std::cout << std::left << std::setfill(' ')
                  << std::setw(15) << fname
                  << std::setw(5) << ftype
                  << std::setw(8) << entry->version
                  << std::setw(6) << entry->file_id[0]
                  << std::setw(10) << fileBlocks
                  << std::setw(11) << sizeStr
                  << std::setw(15) << dateStr << " "
                  << std::setw(10) << ownerUIC << std::endl;
        
        fileCount++;
    }
    
    std::cout << std::string(80, '-') << std::endl;
    std::cout << "Total files: " << fileCount 
              << "  Total blocks: " << totalBlocks 
              << "  Total size: " << (totalBlocks * BLOCK_SIZE) << " bytes";
    if (totalBlocks * BLOCK_SIZE >= 1024) {
        std::cout << " (" << (totalBlocks * BLOCK_SIZE / 1024) << "K)";
    }
    std::cout << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    
    return true;
}

// Copy file from Files-11 to local filesystem
bool Files11FileSystem::copyFromFiles11(int group, int member, const std::string& filename, 
                     const std::string& type, int version, const std::string& outputPath) {
    FileHeader mfdHeader;
    if (!readFileHeader(4, mfdHeader)) {
        std::cerr << "Error: Cannot read MFD" << std::endl;
        return false;
    }
    
    auto mfdData = readFile(4);
    if (mfdData.empty()) {
        std::cerr << "Error: Cannot read MFD" << std::endl;
        return false;
    }
    
    char ufdName[16];
    snprintf(ufdName, sizeof(ufdName), "%03o%03o", group, member);
    std::string ufdNameStr = ufdName;
    
    uint16_t ufdFileNum = 0;
    for (size_t i = 0; i + sizeof(DirEntry) <= mfdData.size(); i += sizeof(DirEntry)) {
        DirEntry* entry = reinterpret_cast<DirEntry*>(&mfdData[i]);
        if (entry->file_id[0] == 0) continue;
        
        std::string fname = decodeRad50(entry->fname, 3);
        std::string ftype = decodeRad50(&entry->ftype, 1);
        
        if (fname == ufdNameStr && ftype == "DIR") {
            ufdFileNum = entry->file_id[0];
            break;
        }
    }
    
    if (ufdFileNum == 0) {
        std::cerr << "Error: Directory [" << std::oct << std::setfill('0') 
                  << std::setw(3) << group << "," << std::setw(3) << member 
                  << "] not found" << std::dec << std::endl;
        return false;
    }
    
    auto ufdData = readFile(ufdFileNum);
    if (ufdData.empty()) {
        std::cerr << "Error: Cannot read UFD" << std::endl;
        return false;
    }
    
    uint16_t targetFileNum = 0;
    int actualVersion = version;
    
    if (version == -1) {
        int maxVer = 0;
        for (size_t i = 0; i + sizeof(DirEntry) <= ufdData.size(); i += sizeof(DirEntry)) {
            DirEntry* entry = reinterpret_cast<DirEntry*>(&ufdData[i]);
            if (entry->file_id[0] == 0) continue;
            
            std::string fname = decodeRad50(entry->fname, 3);
            std::string ftype = decodeRad50(&entry->ftype, 1);
            
            if (fname == filename && ftype == type && entry->version > maxVer) {
                maxVer = entry->version;
                targetFileNum = entry->file_id[0];
            }
        }
        actualVersion = maxVer;
    } else {
        for (size_t i = 0; i + sizeof(DirEntry) <= ufdData.size(); i += sizeof(DirEntry)) {
            DirEntry* entry = reinterpret_cast<DirEntry*>(&ufdData[i]);
            if (entry->file_id[0] == 0) continue;
            
            std::string fname = decodeRad50(entry->fname, 3);
            std::string ftype = decodeRad50(&entry->ftype, 1);
            
            if (fname == filename && ftype == type && entry->version == version) {
                targetFileNum = entry->file_id[0];
                break;
            }
        }
    }
    
    if (targetFileNum == 0) {
        std::cerr << "Error: File not found: [" << std::oct << std::setfill('0') 
                  << std::setw(3) << group << "," << std::setw(3) << member 
                  << "]" << std::dec << filename << "." << type;
        if (version != -1) std::cerr << ";" << version;
        std::cerr << std::endl;
        return false;
    }
    
    std::cout << "Reading [" << std::oct << std::setfill('0') 
              << std::setw(3) << group << "," << std::setw(3) << member 
              << "]" << std::dec << filename << "." << type << ";" << actualVersion << "..." << std::endl;
    
    auto fileData = readFile(targetFileNum);
    if (fileData.empty()) {
        std::cerr << "Error: Cannot read file data" << std::endl;
        return false;
    }
    
    std::ofstream out(outputPath, std::ios::binary);
    if (!out) {
        std::cerr << "Error: Cannot create output file: " << outputPath << std::endl;
        return false;
    }
    
    out.write(reinterpret_cast<const char*>(fileData.data()), fileData.size());
    out.close();
    
    std::cout << "Copied to: " << outputPath << " (" << fileData.size() << " bytes)" << std::endl;
    
    return true;
}

// Copy file from Files-11 to host filesystem (legacy extract command)
bool Files11FileSystem::extractFile(int group, int member, const std::string& filename, 
                 const std::string& type, int version, const std::string& outputPath) {
    return copyFromFiles11(group, member, filename, type, version, outputPath);
}

// Copy file from local filesystem to Files-11
bool Files11FileSystem::copyToFiles11(int group, int member, const std::string& localPath, const std::string& filename) {
    // Read the local file
    std::ifstream localFile(localPath, std::ios::binary | std::ios::ate);
    if (!localFile) {
        std::cerr << "Error: Cannot open local file: " << localPath << std::endl;
        return false;
    }
    
    // Get file size
    std::streamsize fileSize = localFile.tellg();
    localFile.seekg(0, std::ios::beg);
    
    if (fileSize <= 0) {
        std::cerr << "Error: Local file is empty or invalid" << std::endl;
        return false;
    }
    
    // Calculate blocks needed (round up)
    uint32_t blocksNeeded = static_cast<uint32_t>((fileSize + BLOCK_SIZE - 1) / BLOCK_SIZE);
    
    std::cout << "Copying " << localPath << " to Files-11 disk..." << std::endl;
    std::cout << "File size: " << fileSize << " bytes (" << blocksNeeded << " blocks)" << std::endl;
    
    // Read file data
    std::vector<uint8_t> fileData(fileSize);
    if (!localFile.read(reinterpret_cast<char*>(fileData.data()), fileSize)) {
        std::cerr << "Error: Failed to read local file data" << std::endl;
        return false;
    }
    localFile.close();
    
    // Parse destination filename (NAME.TYPE or NAME.TYPE;version)
    std::string name, type;
    int version = 1; // Default version
    
    size_t dot = filename.find('.');
    size_t semi = filename.find(';');
    
    if (dot != std::string::npos) {
        name = filename.substr(0, dot);
        if (semi != std::string::npos) {
            type = filename.substr(dot + 1, semi - dot - 1);
            version = std::stoi(filename.substr(semi + 1));
        } else {
            type = filename.substr(dot + 1);
        }
    } else {
        name = filename;
        type = "";
    }
    
    // Convert to uppercase and validate
    for (auto& c : name) c = toupper(c);
    for (auto& c : type) c = toupper(c);
    
    if (name.length() > 9) {
        std::cerr << "Error: Filename too long (max 9 characters): " << name << std::endl;
        return false;
    }
    if (type.length() > 3) {
        std::cerr << "Error: File type too long (max 3 characters): " << type << std::endl;
        return false;
    }
    
    // Find UFD file number
    FileHeader mfdHeader;
    if (!readFileHeader(4, mfdHeader)) {
        std::cerr << "Error: Cannot read MFD" << std::endl;
        return false;
    }
    
    auto mfdData = readFile(4);
    if (mfdData.empty()) {
        std::cerr << "Error: Cannot read MFD" << std::endl;
        return false;
    }
    
    char ufdName[16];
    snprintf(ufdName, sizeof(ufdName), "%03o%03o", group, member);
    std::string ufdNameStr = ufdName;
    
    uint16_t ufdFileNum = 0;
    for (size_t i = 0; i + sizeof(DirEntry) <= mfdData.size(); i += sizeof(DirEntry)) {
        DirEntry* entry = reinterpret_cast<DirEntry*>(&mfdData[i]);
        if (entry->file_id[0] == 0) continue;
        
        std::string fname = decodeRad50(entry->fname, 3);
        std::string ftype = decodeRad50(&entry->ftype, 1);
        
        if (fname == ufdNameStr && ftype == "DIR") {
            ufdFileNum = entry->file_id[0];
            break;
        }
    }
    
    if (ufdFileNum == 0) {
        std::cerr << "Error: Directory [" << std::oct << std::setfill('0') 
                  << std::setw(3) << group << "," << std::setw(3) << member 
                  << "] not found" << std::dec << std::endl;
        return false;
    }
    
    std::cout << "Target directory: [" << std::oct << std::setfill('0') 
              << std::setw(3) << group << "," << std::setw(3) << member 
              << "]" << std::dec << name << "." << type << ";" << version << std::endl;
    
    // Allocate disk blocks
    std::cout << "Allocating " << blocksNeeded << " blocks..." << std::endl;
    std::vector<uint32_t> allocatedBlocks = allocateBlocks(blocksNeeded);
    if (allocatedBlocks.size() != blocksNeeded) {
        std::cerr << "Error: Could not allocate enough blocks (got " << allocatedBlocks.size() 
                  << ", needed " << blocksNeeded << ")" << std::endl;
        return false;
    }
    
    // Allocate file number
    uint16_t newFileNum = allocateFileNumber();
    if (newFileNum == 0) {
        std::cerr << "Error: Could not allocate file number" << std::endl;
        return false;
    }
    std::cout << "Allocated file number: " << newFileNum << std::endl;
    
    // Write file data to allocated blocks
    std::cout << "Writing file data..." << std::endl;
    if (!writeFileData(allocatedBlocks, fileData)) {
        std::cerr << "Error: Failed to write file data" << std::endl;
        return false;
    }
    
    // Create file header
    std::cout << "Creating file header..." << std::endl;
    uint16_t ownerUIC = (group << 8) | member;
    if (!createFileHeader(newFileNum, ownerUIC, static_cast<uint32_t>(fileSize), allocatedBlocks)) {
        std::cerr << "Error: Failed to create file header" << std::endl;
        return false;
    }
    
    // Add directory entry
    std::cout << "Adding directory entry..." << std::endl;
    if (!addDirectoryEntry(ufdFileNum, name, type, version, newFileNum)) {
        std::cerr << "Error: Failed to add directory entry" << std::endl;
        return false;
    }
    
    std::cout << "Successfully copied file to Files-11 disk!" << std::endl;
    std::cout << "File: [" << std::oct << std::setfill('0') 
              << std::setw(3) << group << "," << std::setw(3) << member 
              << "]" << std::dec << name << "." << type << ";" << version 
              << " (#" << newFileNum << ")" << std::endl;
    
    return true;
}

// Read storage bitmap
bool Files11FileSystem::readStorageBitmap(std::vector<uint8_t>& bitmap) {
    std::ifstream img(imagePath, std::ios::binary);
    if (!img) return false;
    
    // Storage bitmap location: According to home block structure,
    // there's no explicit storage bitmap pointer in ODS-1.
    // The bitmap is typically managed differently.
    // For safety, we'll scan the disk to find free blocks by checking file headers
    
    // Create a pseudo-bitmap based on what we can determine
    bitmap.resize(16 * BLOCK_SIZE);
    memset(bitmap.data(), 0, bitmap.size());
    
    // Mark system areas as allocated (blocks 0-99 are typically reserved)
    for (int lbn = 0; lbn < 100; lbn++) {
        int byteIndex = lbn / 8;
        int bitIndex = lbn % 8;
        if (byteIndex < bitmap.size()) {
            bitmap[byteIndex] |= (1 << bitIndex);
        }
    }
    
    // Mark blocks used by existing files by reading all file headers
    for (uint16_t fileNum = 1; fileNum <= homeBlock.hm_fmax; fileNum++) {
        FileHeader header;
        if (readFileHeader(fileNum, header) && header.h_fnum == fileNum) {
            // Mark blocks used by this file
            auto pointers = getAllRetrievalPointers(fileNum, false);
            for (const auto& ptr : pointers) {
                uint32_t lbn = ptr.first;
                uint32_t count = ptr.second;
                for (uint32_t i = 0; i < count; i++) {
                    uint32_t blockLBN = lbn + i;
                    uint32_t byteIndex = blockLBN / 8;
                    uint32_t bitIndex = blockLBN % 8;
                    if (byteIndex < bitmap.size()) {
                        bitmap[byteIndex] |= (1 << bitIndex);
                    }
                }
            }
            
            // Also mark the file header block itself
            uint32_t headerLBN = indexBitmapLBN + homeBlock.hm_ibsz + fileNum - 1;
            uint32_t byteIndex = headerLBN / 8;
            uint32_t bitIndex = headerLBN % 8;
            if (byteIndex < bitmap.size()) {
                bitmap[byteIndex] |= (1 << bitIndex);
            }
        }
    }
    
    // Mark index file bitmap area
    for (uint32_t lbn = indexBitmapLBN; lbn < indexBitmapLBN + homeBlock.hm_ibsz; lbn++) {
        uint32_t byteIndex = lbn / 8;
        uint32_t bitIndex = lbn % 8;
        if (byteIndex < bitmap.size()) {
            bitmap[byteIndex] |= (1 << bitIndex);
        }
    }
    
    // Mark file header area
    uint32_t fileHeaderStart = indexBitmapLBN + homeBlock.hm_ibsz;
    uint32_t fileHeaderEnd = fileHeaderStart + homeBlock.hm_fmax;
    for (uint32_t lbn = fileHeaderStart; lbn < fileHeaderEnd; lbn++) {
        uint32_t byteIndex = lbn / 8;
        uint32_t bitIndex = lbn % 8;
        if (byteIndex < bitmap.size()) {
            bitmap[byteIndex] |= (1 << bitIndex);
        }
    }
    
    return true;
}

// Write storage bitmap - NOT IMPLEMENTED for safety
// In RSX-11M+, the storage bitmap is managed by the OS
// We don't write it back; instead we just use our allocation map
bool Files11FileSystem::writeStorageBitmap(const std::vector<uint8_t>& bitmap) {
    // Do nothing - we're not actually modifying the OS-managed bitmap
    // Our allocation is determined by scanning existing files
    if (debugMode) {
        std::cout << "Debug: Storage bitmap write skipped (managed by OS)" << std::endl;
    }
    return true;
}

// Allocate blocks from storage bitmap
std::vector<uint32_t> Files11FileSystem::allocateBlocks(uint32_t count) {
    std::vector<uint32_t> allocated;
    std::vector<uint8_t> bitmap;
    
    if (!readStorageBitmap(bitmap)) {
        std::cerr << "Error: Cannot read storage bitmap" << std::endl;
        return allocated;
    }
    
    // Search for free blocks in bitmap
    // Bit set = block in use, bit clear = block free
    uint32_t totalBits = static_cast<uint32_t>(bitmap.size() * 8);
    
    for (uint32_t lbn = 100; lbn < totalBits && allocated.size() < count; lbn++) {
        uint32_t byteIndex = lbn / 8;
        uint32_t bitIndex = lbn % 8;
        
        if (byteIndex >= bitmap.size()) break;
        
        // Check if block is free
        if ((bitmap[byteIndex] & (1 << bitIndex)) == 0) {
            allocated.push_back(lbn);
            // Mark as allocated
            bitmap[byteIndex] |= (1 << bitIndex);
        }
    }
    
    if (allocated.size() == count) {
        // Write updated bitmap
        if (!writeStorageBitmap(bitmap)) {
            std::cerr << "Error: Failed to update storage bitmap" << std::endl;
            return std::vector<uint32_t>();
        }
    }
    
    return allocated;
}

// Write file data to allocated blocks
bool Files11FileSystem::writeFileData(const std::vector<uint32_t>& lbns, const std::vector<uint8_t>& data) {
    std::fstream img(imagePath, std::ios::binary | std::ios::in | std::ios::out);
    if (!img) return false;
    
    size_t dataOffset = 0;
    for (uint32_t lbn : lbns) {
        uint8_t buffer[BLOCK_SIZE];
        memset(buffer, 0, BLOCK_SIZE);
        
        size_t bytesToCopy = std::min(static_cast<size_t>(BLOCK_SIZE), data.size() - dataOffset);
        memcpy(buffer, &data[dataOffset], bytesToCopy);
        
        if (!writeBlock(img, lbn, buffer)) {
            return false;
        }
        
        dataOffset += bytesToCopy;
        if (dataOffset >= data.size()) break;
    }
    
    return true;
}

// Create file header with retrieval pointers
bool Files11FileSystem::createFileHeader(uint16_t fileNum, uint16_t ownerUIC, uint32_t fileSize, const std::vector<uint32_t>& lbns) {
    FileHeader header;
    memset(&header, 0, sizeof(FileHeader));
    
    // Fill in header fields
    header.h_idof = 23; // 0x17, Ident area offset in words (matches native files - 46 bytes)
    header.h_mpof = 46; // 0x2E, Map area offset in words (92 bytes) - match native files
    header.h_fnum = fileNum;
    header.h_fseq = 1;
    header.h_flev = 257; // ODS-1 structure level (401 octal)
    header.h_fown = ownerUIC;
    header.h_fpro = 0xFF00; // Default protection
    header.h_ucha = 0;
    header.h_scha = 0;
    
    // Get current date/time
    time_t now = time(nullptr);
    struct tm timeinfo;
    
#ifdef _WIN32
    localtime_s(&timeinfo, &now);
#else
    struct tm* tmp = localtime(&now);
    if (tmp) timeinfo = *tmp;
#endif
    
    // UFAT area setup
    uint32_t fileSizeBlocks = (fileSize + BLOCK_SIZE - 1) / BLOCK_SIZE;
    
    // UFAT[0-1]: Set to 1 (matching native files)
    header.h_ufat[0] = 0x01;
    header.h_ufat[1] = 0x00;
    
    // UFAT[2-3]: Store exact EOF byte position
    header.h_ufat[2] = fileSize & 0xFF;
    header.h_ufat[3] = (fileSize >> 8) & 0xFF;
    
    // UFAT[4-5]: Reserved (0)
    header.h_ufat[4] = 0;
    header.h_ufat[5] = 0;
    
    // UFAT[6-7]: File size in BLOCKS - RSX-11M+ reads THIS for file size!
    header.h_ufat[6] = fileSizeBlocks & 0xFF;
    header.h_ufat[7] = (fileSizeBlocks >> 8) & 0xFF;
    
    // UFAT[8-9]: Reserved (0)
    header.h_ufat[8] = 0;
    header.h_ufat[9] = 0;
    
    // UFAT[10-11]: Allocated blocks (add 1 for safety)
    uint32_t allocatedBlocks = fileSizeBlocks + 1;
    header.h_ufat[10] = allocatedBlocks & 0xFF;
    header.h_ufat[11] = (allocatedBlocks >> 8) & 0xFF;
    
    // Clear rest of UFAT (but leave room for date at UFAT[24-37])
    for (int i = 12; i < 24; i++) {
        header.h_ufat[i] = 0;
    }
    
    // Date/time storage - RSX-11M+ stores ONE timestamp at bytes 38-51 of header (14 bytes)
    // Format: "DDMMM<YHHMMSS" (13 bytes) + 1 byte padding
    // Where: DD=day, MMM=month, <Y=year with overflow encoding, HHMMSS=time
    const char* months[] = {"", "JAN", "FEB", "MAR", "APR", "MAY", "JUN", 
                           "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    int month = timeinfo.tm_mon + 1;
    int year_offset = timeinfo.tm_year; // Years since 1900 (e.g., 125 for 2025)
    
    // Encode year as two "digit" characters (may overflow into punctuation for post-1999)
    char year_tens = (year_offset / 10) + '0';   // For 2025: 125/10=12 → 12+48=60 → '<'
    char year_ones = (year_offset % 10) + '0';   // For 2025: 125%10=5 → 5+48=53 → '5'
    
    // Generate 13-byte timestamp: DDMMM<YHHMMSS
    char dateStr[27];
    snprintf(dateStr, sizeof(dateStr), "%02d%s%c%c%02d%02d%02d%02d%s%c%c%02d%02d%02d",
             timeinfo.tm_mday,
             (month >= 1 && month <= 12) ? months[month] : "???",
             year_tens,
             year_ones,
             timeinfo.tm_hour,
             timeinfo.tm_min,
             timeinfo.tm_sec,
              timeinfo.tm_mday,
             (month >= 1 && month <= 12) ? months[month] : "???",
             year_tens,
             year_ones,
             timeinfo.tm_hour,
             timeinfo.tm_min,
             timeinfo.tm_sec
           
             );
    
    //dateStr[13] = '2';  // Padding byte (seems to be '2' in native files - possibly seconds tens digit again?)
    //dateStr[14] = '\0';
    
    // Debug output
    std::cout << "Debug: Date string generated: \"" << dateStr << "\" (length=" << strlen(dateStr) << ")" << std::endl;
    std::cout << "Debug: Date components: day=" << timeinfo.tm_mday 
              << " month=" << months[month]
              << " year_offset=" << year_offset
              << " encoded_year=" << year_tens << year_ones 
              << " time=" << timeinfo.tm_hour << ":" << timeinfo.tm_min << ":" << timeinfo.tm_sec << std::endl;
    
    // Write 14 bytes of date/time to header bytes 38-51
    // First 8 bytes go to UFAT[24-31] (header bytes 38-45)
    for (int i = 0; i < 26; i++) {
        //header.h_ufat[44 + i] = (uint8_t)dateStr[i];
        header.data[12 + i] = (uint8_t)dateStr[i];
    }
    // Last 6 bytes go to data[0-5] (header bytes 46-51)
    //for (int i = 0; i < 6; i++) {
    //    header.data[i] = (uint8_t)dateStr[8 + i];
    //}
    
    // Identification area starts at h_idof * 2 = 46 bytes
    // Clear it (4 characters starting from byte 6 since we used data[0-5] for date)
    for (int i = 6; i < 10; i++) {
        header.data[i] = 0;
    }
    
    // Build retrieval pointers in map area
    uint8_t* mapArea = &header.data[0] + (header.h_mpof * 2 - 46);
    std::cout << "Debug: Map area of header at offset: " << (header.h_mpof * 2 - 46) << std::endl;
    // Map area control fields
    mapArea[0] = 0; // M.ESQN
    mapArea[1] = 0; // M.ERVN
    mapArea[2] = 0; // M.EFNU (no extension)
    mapArea[3] = 0;
    mapArea[4] = 0; // M.EFSQ
    mapArea[5] = 0;
    mapArea[6] = 1; // M.CTSZ (count field size)
    mapArea[7] = 3; // M.LBSZ (LBN field size)
    mapArea[8] = static_cast<uint8_t>(lbns.size() * 2); // M.USE (map words in use)
    mapArea[9] = 0;
    
    // Write retrieval pointers (non-standard format: [lbn_hi][count][lbn_mid][lbn_lo])
    int ptrOffset = 10;
    for (size_t i = 0; i < lbns.size(); i++) {
        uint32_t lbn = lbns[i];
        uint8_t count = 0; // Count-1, so 0 means 1 block
        
        // Check if we can combine consecutive blocks
        while (i + 1 < lbns.size() && lbns[i + 1] == lbns[i] + 1 && count < 255) {
            count++;
            i++;
        }
        
        mapArea[ptrOffset++] = (lbn >> 16) & 0xFF; // LBN high byte
        mapArea[ptrOffset++] = count;               // Count
        mapArea[ptrOffset++] = (lbn >> 8) & 0xFF;  // LBN mid byte
        mapArea[ptrOffset++] = lbn & 0xFF;         // LBN low byte
    }
    
    // Calculate and store checksum
    // Files-11 uses a simple additive checksum of all words in the header
    // The checksum is stored so that the sum of all words (including checksum) = 0
    uint8_t buffer[BLOCK_SIZE];
    memcpy(buffer, &header, sizeof(FileHeader));
    
    // Calculate checksum over all 16-bit words in the block
    uint16_t checksum = 0;
    for (int i = 0; i < BLOCK_SIZE; i += 2) {
        uint16_t word = buffer[i] | (buffer[i + 1] << 8);
        checksum += word;
    }
    
    // Negate to get the value that makes sum = 0
    checksum = ~checksum + 1;
    
    // Store checksum at the end of the ident area (just before map area)
    // Checksum location is at h_mpof*2 - 2 (last word of ident area)
    int checksumOffset = header.h_mpof * 2 - 2;
    if (checksumOffset > 0 && checksumOffset < BLOCK_SIZE - 2) {
        buffer[checksumOffset] = checksum & 0xFF;
        buffer[checksumOffset + 1] = (checksum >> 8) & 0xFF;
        
        // Recalculate to verify
        uint16_t verify = 0;
        for (int i = 0; i < BLOCK_SIZE; i += 2) {
            uint16_t word = buffer[i] | (buffer[i + 1] << 8);
            verify += word;
        }
        
        if (debugMode) {
            std::cout << "Debug: File header checksum = 0x" << std::hex << checksum 
                      << ", verification sum = 0x" << verify << std::dec << std::endl;
            std::cout << "Debug: File size = " << fileSize << " bytes, " 
                      << fileSizeBlocks << " blocks allocated" << std::endl;
        }
    }
    
    // Write file header to disk
    std::fstream img(imagePath, std::ios::binary | std::ios::in | std::ios::out);
    if (!img) return false;
    
    uint32_t headerLBN = indexBitmapLBN + homeBlock.hm_ibsz + fileNum - 1;
    
    return writeBlock(img, headerLBN, buffer);
}

// Add directory entry to UFD
bool Files11FileSystem::addDirectoryEntry(uint16_t ufdFileNum, const std::string& filename, 
                                          const std::string& type, int version, uint16_t fileNum) {
    // Read UFD data
    auto ufdData = readFile(ufdFileNum);
    if (ufdData.empty()) {
        return false;
    }
    
    // Find empty slot or end of directory
    size_t emptySlot = 0;
    bool foundSlot = false;
    
    for (size_t i = 0; i + sizeof(DirEntry) <= ufdData.size(); i += sizeof(DirEntry)) {
        DirEntry* entry = reinterpret_cast<DirEntry*>(&ufdData[i]);
        if (entry->file_id[0] == 0) {
            emptySlot = i;
            foundSlot = true;
            break;
        }
    }
    
    if (!foundSlot) {
        std::cerr << "Error: No empty directory slots available" << std::endl;
        return false;
    }
    
    // Create new directory entry
    DirEntry newEntry;
    memset(&newEntry, 0, sizeof(DirEntry));
    
    newEntry.file_id[0] = fileNum;
    newEntry.file_id[1] = 1; // Sequence number
    newEntry.file_id[2] = 0; // RVN
    
    // Encode filename and type in RAD-50
    uint16_t fnameWords[3] = {0, 0, 0};
    uint16_t ftypeWord = 0;
    
    encodeRad50(filename, fnameWords, 3);
    encodeRad50(type, &ftypeWord, 1);
    
    memcpy(newEntry.fname, fnameWords, 6);
    newEntry.ftype = ftypeWord;
    newEntry.version = version;
    
    // Write entry to UFD data
    memcpy(&ufdData[emptySlot], &newEntry, sizeof(DirEntry));
    
    // Write UFD back to disk
    auto pointers = getAllRetrievalPointers(ufdFileNum, false);
    if (pointers.empty()) {
        return false;
    }
    
    std::fstream img(imagePath, std::ios::binary | std::ios::in | std::ios::out);
    if (!img) return false;
    
    size_t dataOffset = 0;
    for (const auto& ptr : pointers) {
        uint32_t startLBN = ptr.first;
        uint32_t blockCount = ptr.second;
        
        for (uint32_t i = 0; i < blockCount; i++) {
            uint8_t buffer[BLOCK_SIZE];
            memset(buffer, 0, BLOCK_SIZE);
            
            size_t bytesToCopy = std::min(static_cast<size_t>(BLOCK_SIZE), ufdData.size() - dataOffset);
            if (bytesToCopy > 0) {
                memcpy(buffer, &ufdData[dataOffset], bytesToCopy);
            }
            
            if (!writeBlock(img, startLBN + i, buffer)) {
                return false;
            }
            
            dataOffset += BLOCK_SIZE;
            if (dataOffset >= ufdData.size()) break;
        }
        
        if (dataOffset >= ufdData.size()) break;
    }
    
    return true;
}

// Allocate a free file number
uint16_t Files11FileSystem::allocateFileNumber() {
    // Start searching from file 10 (skip system files)
    for (uint16_t fileNum = 10; fileNum < homeBlock.hm_fmax; fileNum++) {
        FileHeader header;
        if (!readFileHeader(fileNum, header)) {
            // If can't read header, it's likely free
            return fileNum;
        }
        
        // Check if file number matches (valid file)
        if (header.h_fnum != fileNum) {
            return fileNum;
        }
    }
    
    return 0; // No free file numbers
}

// Display home block information
void Files11FileSystem::displayHomeBlock() {
    std::cout << "\n=== Volume Information ===" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    
    std::cout << "\nVolume Name: " << std::string(homeBlock.hm_vnam, 12) << std::endl;
    std::cout << "Structure Level: " << homeBlock.hm_vlev;
    if (homeBlock.hm_vlev == 257 || homeBlock.hm_vlev == 0401) {
        std::cout << " (ODS-1)" << std::endl;
    } else {
        std::cout << std::endl;
    }
    std::cout << "Max Files: " << homeBlock.hm_fmax << std::endl;
    std::cout << "Index Bitmap LBN: " << indexBitmapLBN << std::endl;
    std::cout << "Index Bitmap Size: " << homeBlock.hm_ibsz << " blocks" << std::endl;
    
    std::cout << std::string(80, '=') << std::endl;
}

// Display file header details
void Files11FileSystem::displayFileHeader(uint16_t fileNum) {
    FileHeader header;
    if (!readFileHeader(fileNum, header)) {
        std::cerr << "Error: Cannot read file header for file #" << fileNum << std::endl;
        return;
    }
    
    std::cout << "\n=== File Header for File #" << fileNum << " ===" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    std::cout << "File Number: " << header.h_fnum << std::endl;
    std::cout << "File Sequence: " << header.h_fseq << std::endl;
    std::cout << "Structure Level: " << header.h_flev << std::endl;
    
    int file_grp = (header.h_fown >> 8) & 0xFF;
    int file_mem = header.h_fown & 0xFF;
    std::cout << "Owner UIC: [" << std::oct << std::setfill('0') 
              << std::setw(3) << file_grp << "," << std::setw(3) << file_mem 
              << "]" << std::dec << std::endl;
    
    uint32_t fileSize = getFileSize(fileNum);
    std::cout << "File Size: " << fileSize << " blocks (" << (fileSize * BLOCK_SIZE) << " bytes)" << std::endl;
    
    std::cout << std::string(80, '=') << std::endl;
}

// Display directory entry details
void Files11FileSystem::displayDirectoryEntry(int group, int member, const std::string& filename, const std::string& type, int version) {
    FileHeader mfdHeader;
    if (!readFileHeader(4, mfdHeader)) {
        std::cerr << "Error: Cannot read MFD" << std::endl;
        return;
    }
    
    auto mfdData = readFile(4);
    if (mfdData.empty()) return;
    
    char ufdName[16];
    snprintf(ufdName, sizeof(ufdName), "%03o%03o", group, member);
    std::string ufdNameStr = ufdName;
    
    uint16_t ufdFileNum = 0;
    for (size_t i = 0; i + sizeof(DirEntry) <= mfdData.size(); i += sizeof(DirEntry)) {
        DirEntry* entry = reinterpret_cast<DirEntry*>(&mfdData[i]);
        if (entry->file_id[0] == 0) continue;
        
        std::string fname = decodeRad50(entry->fname, 3);
        std::string ftype = decodeRad50(&entry->ftype, 1);
        
        if (fname == ufdNameStr && ftype == "DIR") {
            ufdFileNum = entry->file_id[0];
            break;
        }
    }
    
    if (ufdFileNum == 0) {
        std::cerr << "Error: Directory not found" << std::endl;
        return;
    }
    
    auto ufdData = readFile(ufdFileNum);
    if (ufdData.empty()) return;
    
    for (size_t i = 0; i + sizeof(DirEntry) <= ufdData.size(); i += sizeof(DirEntry)) {
        DirEntry* entry = reinterpret_cast<DirEntry*>(&ufdData[i]);
        if (entry->file_id[0] == 0) continue;
        
        std::string fname = decodeRad50(entry->fname, 3);
        std::string ftype = decodeRad50(&entry->ftype, 1);
        
        if (fname == filename && ftype == type && entry->version == version) {
            std::cout << "\n=== Directory Entry Details ===" << std::endl;
            std::cout << std::string(80, '=') << std::endl;
            std::cout << "Filename: " << fname << "." << ftype << ";" << entry->version << std::endl;
            std::cout << "File Number: " << entry->file_id[0] << std::endl;
            std::cout << "File Sequence: " << entry->file_id[1] << std::endl;
            std::cout << "RVN: " << entry->file_id[2] << std::endl;
            std::cout << std::string(80, '=') << std::endl;
            
            displayFileHeader(entry->file_id[0]);
            return;
        }
    }
    
    std::cerr << "Error: File not found: " << filename << "." << type << ";" << version << std::endl;
}