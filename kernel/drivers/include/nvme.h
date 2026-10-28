#pragma once
// FeatherOS Kernel — NVMe (NVM Express) driver
// Identifies NVMe controllers and namespaces, submits admin and I/O commands
// via submission/completion queues.

#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <cstddef>
#include <cstdint>

namespace feather::drivers::nvme {

// NVMe register offsets (from BAR0 memory-mapped base)
enum nvme_reg : uint32_t {
    REG_CAP           = 0x0000,  // Capabilities
    REG_VS            = 0x0008,  // Version
    REG_INTMS         = 0x000C,  // Interrupt Mask Set
    REG_INTMC         = 0x0010,  // Interrupt Mask Clear
    REG_CC            = 0x0014,  // Controller Configuration
    REG_CSTS          = 0x001C,  // Controller Status
    REG_NSSR          = 0x0020,  // NVM Subsystem Reset
    REG_AQA           = 0x0024,  // Admin Queue Attributes
    REG_ASQ           = 0x0028,  // Admin Submission Queue Base
    REG_ACQ           = 0x0030,  // Admin Completion Queue Base
    REG_CMBLOC        = 0x0038,  // Controller Memory Buffer Location
    REG_CMBSZ         = 0x003C,  // Controller Memory Buffer Size
    REG_BPINFO        = 0x0044,  // Boot Partition Information
    REG_BPRSEL        = 0x0048,  // Boot Partition Read/Write Select
    REG_BPMBL         = 0x004C,  // Boot Partition Memory Buffer Length
    REG_CMBMSC        = 0x0050,  // CMB Memory Size and Capabilities
    REG_SQ0           = 0x1000,  // Submission Queue 0 (I/O)
    REG_CQ0           = 0x1004,  // Completion Queue 0 (I/O)
};

// Controller Configuration (CC) register bits
enum cc_bits : uint32_t {
    CC_ENABLE         = (1u << 0),
    CC_CSS_SHIFT      = 4,
    CC_CSS_MASK       = (7u << CC_CSS_SHIFT),
    CC_MPS_SHIFT      = 7,
    CC_MPS_MASK       = (7u << CC_MPS_SHIFT),
    CC_AMS_SHIFT      = 11,
    CC_AMS_MASK       = (3u << CC_AMS_SHIFT),
    CC_SHN_SHIFT      = 14,
    CC_SHN_MASK       = (3u << CC_SHN_SHIFT),
    CC_IOCQES_SHIFT   = 16,
    CC_IOCQES_MASK    = (0xFu << CC_IOCQES_SHIFT),
    CC_IOSQES_SHIFT   = 20,
    CC_IOSQES_MASK    = (0xFu << CC_IOSQES_SHIFT),
};

// Controller Status (CSTS) register bits
enum csts_bits : uint32_t {
    CSTS_READY   = (1u << 0),
    CFS_SHST_SHIFT = 14,
    CFS_SHST_MASK  = (3u << CFS_SHST_SHIFT),
};

// Command opcodes (Admin)
enum nvme_opcode_admin : uint8_t {
    NVME_ADMIN_DELETE_SQ        = 0x00,
    NVME_ADMIN_CREATE_SQ        = 0x01,
    NVME_ADMIN_GET_LOG_PAGE     = 0x02,
    NVME_ADMIN_DELETE_CQ        = 0x04,
    NVME_ADMIN_CREATE_CQ        = 0x05,
    NVME_ADMIN_IDENTIFY         = 0x06,
    NVME_ADMIN_ABORT            = 0x08,
    NVME_ADMIN_SET_FEATURES     = 0x09,
    NVME_ADMIN_GET_FEATURES     = 0x0A,
    NVME_ADMIN_ASYNC_EVENT      = 0x0C,
};

// Command opcodes (NVM I/O)
enum nvme_opcode_nvm : uint8_t {
    NVME_NVM_FLUSH       = 0x00,
    NVME_NVM_WRITE       = 0x01,
    NVME_NVM_READ        = 0x02,
    NVME_NVM_WRITE_UNCOR = 0x04,
    NVME_NVM_COMPARE     = 0x05,
    NVME_NVM_WRITE_ZEROES= 0x08,
    NVME_NVM_DSM         = 0x09,
};

// Identify CNS values
enum nvme_identify_cns : uint8_t {
    IDENTIFY_CNS_NS       = 0x00,  // Namespace
    IDENTIFY_CNS_CTRL     = 0x01,  // Controller
    IDENTIFY_CNS_NS_ACTIVE= 0x02,  // Namespace List (Active)
};

// Completion queue entry status type
enum nvme_status_type : uint8_t {
    NVME_ST_SUCCESS             = 0x00,
    NVME_ST_INVALID_OPCODE      = 0x01,
    NVME_ST_INVALID_FIELD       = 0x02,
    NVME_ST_ID_NOT_FOUND        = 0x05,
    NVME_ST_ABORTED_COMMAND     = 0x06,
};

// Completion queue entry
struct alignas(16) nvme_cqe {
    uint32_t result0;   // DW0 - command specific
    uint32_t result1;   // DW1 - command specific
    uint32_t sq_head;   // DW2 - sq head pointer
    uint16_t sq_id;     // DW3 - submission queue id
    uint16_t cid;       // DW3 - command id
    uint16_t status;    // DW3 - status (phase in bit 0, status type/code in upper bits)
};

// Submission queue entry (64 bytes)
struct alignas(64) nvme_sqe {
    uint8_t  opcode;        // DW0
    uint8_t  flags;         // DW0
    uint16_t cid;           // DW0
    uint32_t nsid;          // DW1
    uint64_t reserved2;     // DW2-DW3
    uint64_t mptr;          // DW4-DW5 - metadata pointer
    uint64_t prp1;          // DW6-DW7 - PRP entry 1
    uint64_t prp2;          // DW8-DW9 - PRP entry 2 (optional)
    uint64_t cdw2;          // DW10
    uint64_t cdw3;          // DW12
    uint64_t cdw4;          // DW14
    uint64_t cdw5;          // DW15
};

// Initialize NVMe driver for a PCI device (bar0 = MMIO base)
bool init(uintptr_t mmio_base);

// Shutdown NVMe controller
void shutdown();

// Get number of namespaces
uint32_t namespace_count();

// Read from an NVMe namespace
// lba: logical block address (512-byte or 4KiB sectors)
// buffer: destination buffer (must be physically contiguous or PRP-mapped)
// num_blocks: number of blocks to read
bool read(uint32_t nsid, uint64_t lba, void* buffer, uint32_t num_blocks);

// Write to an NVMe namespace
bool write(uint32_t nsid, uint64_t lba, const void* buffer, uint32_t num_blocks);

// Get namespace size in blocks
uint64_t namespace_size(uint32_t nsid);

// Get namespace block size (bytes per logical block)
uint32_t namespace_block_size(uint32_t nsid);

// Block device interface (512-byte sectors) for filesystem drivers
// sector: 512-byte sector address
// buffer: destination buffer (must be at least count * 512 bytes)
// count: number of 512-byte sectors to read
bool block_read(uint64_t sector, void* buffer, uint32_t count);

// Block device interface (512-byte sectors) for writing
bool block_write(uint64_t sector, const void* buffer, uint32_t count);

} // namespace feather::drivers::nvme
