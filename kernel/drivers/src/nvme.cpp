// FeatherOS Kernel — NVMe implementation
// Sets up admin + I/O queues, identifies controller/namespace, supports read/write
// via submission/completion queues and doorbell registers.

#include "nvme.h"
#include "../../arch/x86_64/include/io.h"

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace feather::drivers::nvme {

namespace {

// Simple memory copy (avoids <cstring> MSVC header issues)
static void mem_copy(void* dst, const void* src, size_t n) {
    uint8_t* d = static_cast<uint8_t*>(dst);
    const uint8_t* s = static_cast<const uint8_t*>(src);
    for (size_t i = 0; i < n; i++) d[i] = s[i];
}

// MMIO base address (set during init)
static volatile uint32_t* g_regs = nullptr;

// Doorbell stride (in 4-byte units, from CAP register)
static uint32_t g_doorbell_stride = 1;

// Queue storage (statically allocated, page-aligned)
static alignas(4096) uint8_t g_admin_sq[4096];   // 4 entries x 64 bytes
static alignas(4096) uint8_t g_admin_cq[4096];   // 4 entries x 16 bytes
static alignas(4096) uint8_t g_io_sq[4096];      // 4 entries x 64 bytes
static alignas(4096) uint8_t g_io_cq[4096];      // 4 entries x 16 bytes

// I/O data buffer (physically contiguous for PRP)
static alignas(4096) uint8_t g_io_buffer[4096 * 16];  // 64 KiB buffer

// Queue state
static uint16_t g_admin_sq_tail = 0;
static uint16_t g_admin_cq_tail = 0;
static uint8_t  g_admin_phase   = 1;

static uint16_t g_io_sq_tail = 0;
static uint16_t g_io_cq_tail = 0;
static uint8_t  g_io_phase   = 1;

static uint16_t g_cid = 0;

// Identify data
static alignas(4096) uint8_t g_identify_ctrl[4096];
static alignas(4096) uint8_t g_identify_ns[4096];

static uint32_t g_ns_count = 0;
static uint32_t g_ns_block_size = 512;
static uint64_t g_ns_size_blocks = 0;

// Read a 32-bit register
inline uint32_t reg_read(uint32_t offset) {
    return g_regs[offset / 4];
}

// Write a 32-bit register
inline void reg_write(uint32_t offset, uint32_t value) {
    g_regs[offset / 4] = value;
}

// Doorbell register offset for a queue
inline uint32_t doorbell_offset(uint32_t queue_id, bool is_sq) {
    // Doorbell offset = (queue_id * 2 + (is_sq ? 0 : 1)) * doorbell_stride * 4
    return (queue_id * 2 + (is_sq ? 0 : 1)) * g_doorbell_stride * 4;
}

// Wait for controller to be ready
bool wait_ready(uint32_t timeout_us) {
    for (uint32_t i = 0; i < timeout_us; i++) {
        if (reg_read(REG_CSTS) & CSTS_READY) return true;
        for (volatile int j = 0; j < 10; j++) {}
    }
    return false;
}

// Submit an admin command and wait for completion
bool admin_cmd(nvme_sqe& sqe, nvme_cqe& cqe, uint32_t timeout_us) {
    nvme_sqe* entry = reinterpret_cast<nvme_sqe*>(g_admin_sq + g_admin_sq_tail * 64);
    *entry = sqe;
    entry->cid = g_cid++;
    entry->flags = g_admin_phase & 0x01;

    uint16_t next_tail = static_cast<uint16_t>((g_admin_sq_tail + 1) & 0x3);
    reg_write(doorbell_offset(0, true), g_admin_sq_tail);
    g_admin_sq_tail = next_tail;

    for (uint32_t i = 0; i < timeout_us; i++) {
        nvme_cqe* cq = reinterpret_cast<nvme_cqe*>(g_admin_cq + g_admin_cq_tail * 16);
        if ((cq->status & 0x1) == (g_admin_phase & 0x1)) {
            cqe = *cq;
            g_admin_cq_tail = static_cast<uint16_t>((g_admin_cq_tail + 1) & 0x3);
            reg_write(doorbell_offset(0, false), g_admin_cq_tail);
            return true;
        }
        for (volatile int j = 0; j < 10; j++) {}
    }
    return false;
}

// Submit an I/O command and wait for completion
bool io_cmd(nvme_sqe& sqe, nvme_cqe& cqe, uint32_t timeout_us) {
    nvme_sqe* entry = reinterpret_cast<nvme_sqe*>(g_io_sq + g_io_sq_tail * 64);
    *entry = sqe;
    entry->cid = g_cid++;
    entry->flags = g_io_phase & 0x01;

    uint16_t next_tail = static_cast<uint16_t>((g_io_sq_tail + 1) & 0x3);
    reg_write(doorbell_offset(1, true), g_io_sq_tail);  // queue ID 1 for I/O
    g_io_sq_tail = next_tail;

    for (uint32_t i = 0; i < timeout_us; i++) {
        nvme_cqe* cq = reinterpret_cast<nvme_cqe*>(g_io_cq + g_io_cq_tail * 16);
        if ((cq->status & 0x1) == (g_io_phase & 0x1)) {
            cqe = *cq;
            g_io_cq_tail = static_cast<uint16_t>((g_io_cq_tail + 1) & 0x3);
            reg_write(doorbell_offset(1, false), g_io_cq_tail);
            return true;
        }
        for (volatile int j = 0; j < 10; j++) {}
    }
    return false;
}

} // anonymous namespace

bool init(uintptr_t mmio_base) {
    g_regs = reinterpret_cast<volatile uint32_t*>(mmio_base);

    // Read doorbell stride from CAP
    uint32_t cap = reg_read(REG_CAP);
    g_doorbell_stride = (cap & 0xF) * 4;  // doorbell stride in bytes (divide by 4 for offset)

    // Wait for controller ready
    if (!wait_ready(1000000)) {
        return false;
    }

    // Disable controller first
    uint32_t cc = reg_read(REG_CC);
    cc &= ~CC_ENABLE;
    reg_write(REG_CC, cc);
    for (volatile int i = 0; i < 100000; i++) {}

    // Set up Admin Queue Attributes (AQA)
    reg_write(REG_AQA, (3 << 16) | 3);  // 4-entry SQ, 4-entry CQ

    // Admin SQ base
    reg_write(REG_ASQ,  reinterpret_cast<uintptr_t>(g_admin_sq) & 0xFFFFFFFF);
    reg_write(REG_ASQ + 4, (reinterpret_cast<uintptr_t>(g_admin_sq) >> 32) & 0xFFFFFFFF);
    // Admin CQ base
    reg_write(REG_ACQ,  reinterpret_cast<uintptr_t>(g_admin_cq) & 0xFFFFFFFF);
    reg_write(REG_ACQ + 4, (reinterpret_cast<uintptr_t>(g_admin_cq) >> 32) & 0xFFFFFFFF);

    // Configure Controller (CC)
    cc = CC_ENABLE
       | (0 << CC_MPS_SHIFT)
       | (0 << CC_AMS_SHIFT)
       | (6 << CC_IOSQES_SHIFT)
       | (4 << CC_IOCQES_SHIFT);
    reg_write(REG_CC, cc);

    if (!wait_ready(1000000)) {
        return false;
    }

    // Identify controller (CNS=1)
    nvme_sqe identify_ctrl_sqe{};
    identify_ctrl_sqe.opcode = NVME_ADMIN_IDENTIFY;
    identify_ctrl_sqe.nsid   = 0xFFFFFFFF;
    identify_ctrl_sqe.prp1   = reinterpret_cast<uintptr_t>(g_identify_ctrl);
    identify_ctrl_sqe.cdw2   = 1;

    nvme_cqe cqe{};
    if (!admin_cmd(identify_ctrl_sqe, cqe, 1000000)) {
        return false;
    }
    if (((cqe.status >> 1) & 0x7FF) != NVME_ST_SUCCESS) {
        return false;
    }

    // Identify namespace 1 (CNS=0)
    nvme_sqe identify_ns_sqe{};
    identify_ns_sqe.opcode = NVME_ADMIN_IDENTIFY;
    identify_ns_sqe.nsid   = 1;
    identify_ns_sqe.prp1   = reinterpret_cast<uintptr_t>(g_identify_ns);
    identify_ns_sqe.cdw2   = 0;

    if (!admin_cmd(identify_ns_sqe, cqe, 1000000)) {
        return false;
    }
    if (((cqe.status >> 1) & 0x7FF) != NVME_ST_SUCCESS) {
        return false;
    }

    // Parse namespace identify data
    g_ns_size_blocks = *reinterpret_cast<uint64_t*>(g_identify_ns + 0);
    uint8_t lbaf = g_identify_ns[40];
    g_ns_block_size = 1 << (lbaf & 0x0F);
    if (g_ns_block_size < 512) g_ns_block_size = 512;
    g_ns_count = 1;

    return true;
}

void shutdown() {
    uint32_t cc = reg_read(REG_CC);
    cc &= ~CC_ENABLE;
    reg_write(REG_CC, cc);
    g_regs = nullptr;
    g_ns_count = 0;
}

uint32_t namespace_count() {
    return g_ns_count;
}

uint64_t namespace_size(uint32_t nsid) {
    (void)nsid;
    return g_ns_size_blocks;
}

uint32_t namespace_block_size(uint32_t nsid) {
    (void)nsid;
    return g_ns_block_size;
}

bool read(uint32_t nsid, uint64_t lba, void* buffer, uint32_t num_blocks) {
    if (!g_regs || num_blocks == 0 || num_blocks > 16) return false;

    nvme_sqe sqe{};
    sqe.opcode = NVME_NVM_READ;
    sqe.nsid   = nsid;
    sqe.prp1   = reinterpret_cast<uintptr_t>(buffer);
    sqe.cdw2   = static_cast<uint32_t>(lba & 0xFFFFFFFF);
    sqe.cdw3   = static_cast<uint32_t>(lba >> 32);
    sqe.cdw4   = static_cast<uint32_t>(num_blocks - 1);
    sqe.cdw5   = 0;

    nvme_cqe cqe{};
    if (!io_cmd(sqe, cqe, 5000000)) {
        return false;
    }
    return ((cqe.status >> 1) & 0x7FF) == NVME_ST_SUCCESS;
}

bool write(uint32_t nsid, uint64_t lba, const void* buffer, uint32_t num_blocks) {
    if (!g_regs || num_blocks == 0 || num_blocks > 16) return false;

    nvme_sqe sqe{};
    sqe.opcode = NVME_NVM_WRITE;
    sqe.nsid   = nsid;
    sqe.prp1   = reinterpret_cast<uintptr_t>(const_cast<void*>(buffer));
    sqe.cdw2   = static_cast<uint32_t>(lba & 0xFFFFFFFF);
    sqe.cdw3   = static_cast<uint32_t>(lba >> 32);
    sqe.cdw4   = static_cast<uint32_t>(num_blocks - 1);
    sqe.cdw5   = 0;

    nvme_cqe cqe{};
    if (!io_cmd(sqe, cqe, 5000000)) {
        return false;
    }
    return ((cqe.status >> 1) & 0x7FF) == NVME_ST_SUCCESS;
}

bool block_read(uint64_t sector, void* buffer, uint32_t count) {
    if (!g_regs || g_ns_block_size == 0) return false;
    uint32_t blocks_per_sector = g_ns_block_size / 512;
    uint64_t lba = sector / blocks_per_sector;
    uint32_t num_blocks = count * blocks_per_sector;
    if (num_blocks > 16) num_blocks = 16;
    return read(1, lba, buffer, num_blocks);
}

bool block_write(uint64_t sector, const void* buffer, uint32_t count) {
    if (!g_regs || g_ns_block_size == 0) return false;
    uint32_t blocks_per_sector = g_ns_block_size / 512;
    uint64_t lba = sector / blocks_per_sector;
    uint32_t num_blocks = count * blocks_per_sector;
    if (num_blocks > 16) num_blocks = 16;
    return write(1, lba, buffer, num_blocks);
}

} // namespace feather::drivers::nvme
