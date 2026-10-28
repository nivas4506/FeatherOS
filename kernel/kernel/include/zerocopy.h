#pragma once
// FeatherOS Kernel — Zero-copy IPC Subsystem
// Shared memory ring buffers for high-performance IPC between
// kernel services and userspace agents (featherd daemon, cognitive agents).

#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <cstdint>
#include <cstddef>

namespace feather::kernel::zerocopy {

// ──────────────────────────────────────────────────────────────
// Zero-copy Channel
// ──────────────────────────────────────────────────────────────

// Channel direction
enum class channel_dir : uint8_t {
    CHAN_RX = 0,  // Kernel -> Userspace
    CHAN_TX = 1,  // Userspace -> Kernel
    CHAN_BIDIR = 2, // Bidirectional
};

// Channel state
enum class channel_state : uint8_t {
    CHAN_CLOSED = 0,
    CHAN_OPEN   = 1,
    CHAN_CLOSING = 2,
};

// Ring buffer header (shared between kernel and userspace)
struct alignas(64) ring_header {
    uint32_t    version;        // Ring buffer version
    uint32_t    slot_count;     // Number of slots
    uint32_t    slot_size;      // Size of each slot (bytes)
    uint32_t    producer_idx;   // Producer write index
    uint32_t    consumer_idx;   // Consumer read index
    uint32_t    flags;          // Channel flags
    uint64_t    seq_counter;    // Monotonic sequence counter
    uint64_t    reserved[7];    // Padding for future use
};

// Channel descriptor
struct channel {
    uint32_t            chan_id;      // Channel ID
    channel_state       state;        // Channel state
    channel_dir         direction;    // Direction
    ring_header*        ring;         // Shared ring buffer
    uint8_t*            ring_data;    // Slot data area
    uint32_t            slot_size;    // Slot size in bytes
    uint32_t            slot_count;   // Number of slots
    void*               user_ptr;     // Userspace mapping pointer
    uint64_t            phys_addr;    // Physical address of ring
    uint64_t            seq;          // Last sequence number used
};

// ──────────────────────────────────────────────────────────────
// Public API
// ──────────────────────────────────────────────────────────────

// Initialize zero-copy IPC subsystem
void zc_init();

// Shutdown zero-copy IPC subsystem
void zc_shutdown();

// Create a zero-copy channel
// size_hint: approximate size in bytes (will be rounded to page)
// Returns channel ID on success, -1 on failure
int zc_create_channel(channel_dir dir, uint32_t slot_size, uint32_t slot_count);

// Destroy a channel
int zc_destroy_channel(int chan_id);

// Send data through channel (zero-copy)
// Returns bytes written, or -1 on failure
int zc_send(int chan_id, const void* data, uint32_t len);

// Receive data from channel (zero-copy)
// Returns bytes read, or -1 on failure
int zc_recv(int chan_id, void* data, uint32_t max_len);

// Get channel state
channel_state zc_channel_state(int chan_id);

// Get number of available slots for reading
uint32_t zc_readable(int chan_id);

// Get number of available slots for writing
uint32_t zc_writable(int chan_id);

// Register a doorbell callback (called when data is available)
typedef void (*doorbell_cb_t)(int chan_id, void* arg);
void zc_register_doorbell(int chan_id, doorbell_cb_t cb, void* arg);

// Get shared memory mapping info for userspace
void zc_get_mapping(int chan_id, uint64_t* phys_addr, uint32_t* slot_size,
                    uint32_t* slot_count, uint32_t* ring_size);

} // namespace feather::kernel::zerocopy
