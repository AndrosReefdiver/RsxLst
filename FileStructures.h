#pragma once

#include <cstdint>

// Block size constant for Files-11 ODS-1
const int BLOCK_SIZE = 512;
const int HOME_BLOCK_LBN = 1;

#pragma pack(push, 1)

// ===== HOME BLOCK STRUCTURE =====
struct RSX11M_HomeBlock {
    uint16_t hm_ibsz;          // 0-1: Index file bitmap size (blocks)
    uint16_t hm_iblb_high;     // 2-3: Index bitmap LBN (high word)
    uint16_t hm_iblb_low;      // 4-5: Index bitmap LBN (low word)
    uint16_t hm_fmax;          // 6-7: Maximum number of files
    uint16_t hm_sbcl;          // 8-9: Storage bitmap cluster factor
    uint16_t hm_dvty;          // 10-11: Device type code
    uint16_t hm_vlev;          // 12-13: Volume structure level (401 octal)
    char hm_vnam[12];          // 14-25: Volume name
    uint8_t hm_unused1[4];     // 26-29: Not used
    uint16_t hm_vown;          // 30-31: Volume owner UIC
    uint16_t hm_vpro;          // 32-33: Volume protection code
    uint16_t hm_vcha;          // 34-35: Volume characteristics
    uint16_t hm_fpro;          // 36-37: Default file protection
    uint8_t hm_unused2[6];     // 38-43: Not used
    uint8_t hm_wisz;           // 44: Default window size
    uint8_t hm_fiex;           // 45: Default file extend
    uint8_t hm_lruc;           // 46: Directory LRU limit
    uint8_t hm_unused3[11];    // 47-57: Not used
    uint16_t hm_chk1;          // 58-59: First checksum
    char hm_vdat[14];          // 60-73: Volume creation date
    uint8_t hm_unused4[398];   // 74-471: Reserved
    char hm_indn[12];          // 472-483: Volume name copy
    char hm_indo[12];          // 484-495: Volume owner
    char hm_indf[12];          // 496-507: Format type
    uint8_t hm_unused5[2];     // 508-509: Not used
    uint16_t hm_chk2;          // 510-511: Second checksum
};

// ===== FILE HEADER STRUCTURE =====
struct FileHeader {
    // Header Area (28 bytes)
    uint8_t h_idof;            // 0: Ident area offset
    uint8_t h_mpof;            // 1: Map area offset (in words)
    uint16_t h_fnum;           // 2-3: File number
    uint16_t h_fseq;           // 4-5: File sequence number
    uint16_t h_flev;           // 6-7: File structure level (should be 401 octal = 0x101 = 257 decimal)
    uint16_t h_fown;           // 8-9: File owner UIC
    uint16_t h_fpro;          // 10-11: File protection
    uint8_t h_ucha;            // 12: User characteristics
    uint8_t h_scha;            // 13: System characteristics
    uint8_t h_ufat[32];        // 14-45: User file attribute area
    uint8_t data[BLOCK_SIZE - 46]; // Rest of header (ident + map areas start at h_idof*2)
};

struct IdentArea {   // begins at the word indicated by h.idof (so header addr + h_idof*2)
    uint16_t i_fnam[3];       // 0-5: Filename (RAD-50)
    uint16_t i_ftyp;          // 6-7: File type (RAD-50)
    uint16_t i_fver;          // 8-9: File version
    uint16_t i_rvno;          // 10-11: revision number
    uint8_t  i_rvdt[7];       // 12-18: revision date (ASCII)
    uint8_t  i_rvti[6];       // 19-25: revision time hhmmss
    uint8_t  i_crdt[7];       // 26-33: creation date (ASCII)
    uint8_t  i_crti[6];       // 34-39: creation time hhmmss
    uint8_t  i_exdt[7];       // 40-47: expiration date (ASCII)
    uint8_t  i_unused;        // 48: unused for word alignment
};

struct MapArea {    // starts at the word indicated by h.mpof (so header addr + h_mpof *2)
    uint8_t m_esqn;        // 0: Extension segment number (start with 0)
    uint8_t m_ervn;        // 1: Extensionrelative volume # (usually 0?)
    uint16_t m_efnu;       // 2-3: Extension file number (0 = no extension)
    uint16_t m_efsq;       // 4-5: Extension file sequence number
    uint8_t m_ctsz;        // 6: Count field size (in bytes)
    //uint8_t m_lbsz;        // 7: LBN field size (in bytes)
    //uint16_t m_use;        // 8-9: Map words in use
};

// ===== DIRECTORY ENTRY (16 bytes) =====
struct DirEntry {
    uint16_t file_id[3];       // 0-5: File ID (num, seq, rvn)
    uint16_t fname[3];         // 6-11: Filename (RAD-50)
    uint16_t ftype;            // 12-13: File type (RAD-50)
    uint16_t version;          // 14-15: Version number
};

#pragma pack(pop)
