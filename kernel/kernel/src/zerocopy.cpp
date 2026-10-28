// FeatherOS Kernel — Zero-copy IPC Implementation
// Shared memory ring buffers for high-performance IPC between
// kernel services and userspace agents.

#include "zerocopy.h"
#include "../../lib/include/printf.h"
#include "../../lib/include/string.h"
#include "../../mm/include/heap.h"
#include "../../mm/include/paging.h"

namespace feather::kernel::zerocopy {
namespace {

// Maximum channels
static constexpr uint32_t MAX_CHANNELS = 64;

// Channel table
static channel g_channels[MAX_CHANNELS];
static uint32_t g_channel_count = 0;

// Doorbell callbacks
static doorbell_cb_t g_doorbell_cb[MAX_CHANNELS] = {nullptr};
static void* g_doorbell_arg[MAX_CHANNELS] = {nullptr};

// Allocate a new channel ID
static int alloc_channel_id() {
    for (uint32_t i = 0; i < MAX_CHANNELS; i++) {
        if (g_channels[i].state == channel_state::CHAN_CLOSED) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// Initialize ring header
static void init_ring(ring_header* ring, uint32_t slot_count, uint32_t slot_size) {
    ring->version = 1;
    ring->slot_count = slot_count;
    ring->slot_size = slot_size;
    ring->producer_idx = 0;
    ring->consumer_idx = 0;
    ring->flags = 0;
    ring->seq_counter = 0;
    for (int i = 0; i < 7; i++) ring->reserved[i] = 0;
}

// Calculate total ring buffer size
static uint32_t calc_ring_size(uint32_t slot_count, uint32_t slot_size) {
    // Round slot_size up to 8 bytes for alignment
    uint32_t aligned_slot = (slot_size + 7) & ~7;
    return sizeof(ring_header) + aligned_slot * slot_count;
}

} // anonymous namespace

void zc_init() {
    g_channel_count = 0;
    for (uint32_t i = 0; i < MAX_CHANNELS; i++) {
        g_channels[i].state = channel_state::CHAN_CLOSED;
        g_channels[i].chan_id = static_cast<uint32_t>(i);
        g_channels[i].ring = nullptr;
        g_channels[i].ring_data = nullptr;
        g_doorbell_cb[i] = nullptr;
        g_doorbell_arg[i] = nullptr;
    }

    lib::kputs("[zerocopy] zero-copy IPC subsystem initialized\n");
}

void zc_shutdown() {
    for (uint32_t i = 0; i < MAX_CHANNELS; i++) {
        if (g_channels[i].state != channel_state::CHAN_CLOSED) {
            zc_destroy_channel(static_cast<int>(i));
        }
    }
    g_channel_count = 0;
}

int zc_create_channel(channel_dir dir, uint32_t slot_size, uint32_t slot_count) {
    if (slot_size == 0 || slot_count == 0 || slot_count > 4096) return -1;

    int id = alloc_channel_id();
    if (id < 0) return -1;

    channel* ch = &g_channels[id];

    // Calculate sizes
    uint32_t aligned_slot = (slot_size + 7) & ~7;
    uint32_t ring_size = calc_ring_size(slot_count, slot_size);

    // Allocate physically contiguous memory (simplified: use heap)
    // In a real implementation, this would use physically contiguous pages
    uint8_t* mem = static_cast<uint8_t*>(mm::kmalloc(ring_size));
    if (!mem) return -1;

    ring_header* ring = reinterpret_cast<ring_header*>(mem);
    uint8_t* data_area = mem + sizeof(ring_header);

    // Initialize ring
    init_ring(ring, slot_count, aligned_slot);

    // Fill channel descriptor
    ch->chan_id = static_cast<uint32_t>(id);
    ch->state = channel_state::CHAN_OPEN;
    ch->direction = dir;
    ch->ring = ring;
    ch->ring_data = data_area;
    ch->slot_size = aligned_slot;
    ch->slot_count = slot_count;
    ch->user_ptr = nullptr;
    ch->phys_addr = reinterpret_cast<uint64_t>(mem);  // Simplified: use VA as PA
    ch->seq = 0;

    g_channel_count++;

    lib::kprintf("[zerocopy] channel %d created: dir=%d, slots=%u, slot_size=%u\n",
                 id, static_cast<int>(dir), slot_count, aligned_slot);

    return id;
}

int zc_destroy_channel(int chan_id) {
    if (chan_id < 0 || chan_id >= static_cast<int>(MAX_CHANNELS)) return -1;

    channel* ch = &g_channels[chan_id];
    if (ch->state == channel_state::CHAN_CLOSED) return -1;

    g_doorbell_cb[chan_id] = nullptr;
    g_doorbell_arg[chan_id] = nullptr;

    if (ch->ring) {
        mm::kfree(reinterpret_cast<void*>(ch->ring));
        ch->ring = nullptr;
        ch->ring_data = nullptr;
    }

    ch->state = channel_state::CHAN_CLOSED;
    g_channel_count--;

    return 0;
}

int zc_send(int chan_id, const void* data, uint32_t len) {
    if (chan_id < 0 || chan_id >= static_cast<int>(MAX_CHANNELS)) return -1;

    channel* ch = &g_channels[chan_id];
    if (!ch || ch->state != channel_state::CHAN_OPEN) return -1;
    if (ch->direction != channel_dir::CHAN_TX && ch->direction != channel_dir::CHAN_BIDIR)
        return -1;

    ring_header* ring = ch->ring;
    if (!ring) return -1;

    uint32_t slot_size = ch->slot_size;
    if (len > slot_size) len = slot_size;

    // Check if there's space
    uint32_t prod = ring->producer_idx;
    uint32_t cons = ring->consumer_idx;
    uint32_t used = (prod + ring->slot_count - cons) % ring->slot_count;
    if (used >= ring->slot_count - 1) return -1;  // Full

    // Write data to slot
    uint8_t* slot = ch->ring_data + prod * ch->slot_size;
    lib::memcpy(slot, data, len);

    // Update producer index
    ring->producer_idx = (prod + 1) % ring->slot_count;
    ring->seq_counter++;

    // Notify doorbell
    if (g_doorbell_cb[chan_id]) {
        g_doorbell_cb[chan_id](chan_id, g_doorbell_arg[chan_id]);
    }

    return static_cast<int>(len);
}

int zc_recv(int chan_id, void* data, uint32_t max_len) {
    if (chan_id < 0 || chan_id >= static_cast<int>(MAX_CHANNELS)) return -1;

    channel* ch = &g_channels[chan_id];
    if (!ch || ch->state != channel_state::CHAN_OPEN) return -1;
    if (ch->direction != channel_dir::CHAN_RX && ch->direction != channel_dir::CHAN_BIDIR)
        return -1;

    ring_header* ring = ch->ring;
    if (!ring) return -1;

    uint32_t slot_size = ch->slot_size;

    // Check if there's data
    uint32_t prod = ring->producer_idx;
    uint32_t cons = ring->consumer_idx;
    uint32_t used = (prod + ring->slot_count - cons) % ring->slot_count;
    if (used == 0) return 0;  // Empty

    // Read from slot
    uint8_t* slot = ch->ring_data + cons * ch->slot_size;
    uint32_t copy_len = (max_len < slot_size) ? max_len : slot_size;
    lib::memcpy(data, slot, copy_len);

    // Update consumer index
    ring->consumer_idx = (cons + 1) % ring->slot_count;

    return static_cast<int>(copy_len);
}

channel_state zc_channel_state(int chan_id) {
    if (chan_id < 0 || chan_id >= static_cast<int>(MAX_CHANNELS))
        return channel_state::CHAN_CLOSED;

    channel* ch = &g_channels[chan_id];
    return ch->state;
}

uint32_t zc_readable(int chan_id) {
    if (chan_id < 0 || chan_id >= static_cast<int>(MAX_CHANNELS)) return 0;

    channel* ch = &g_channels[chan_id];
    if (!ch || ch->state != channel_state::CHAN_OPEN) return 0;

    ring_header* ring = ch->ring;
    if (!ring) return 0;

    uint32_t prod = ring->producer_idx;
    uint32_t cons = ring->consumer_idx;
    return (prod + ring->slot_count - cons) % ring->slot_count;
}

uint32_t zc_writable(int chan_id) {
    if (chan_id < 0 || chan_id >= static_cast<int>(MAX_CHANNELS)) return 0;

    channel* ch = &g_channels[chan_id];
    if (!ch || ch->state != channel_state::CHAN_OPEN) return 0;

    ring_header* ring = ch->ring;
    if (!ring) return 0;

    uint32_t prod = ring->producer_idx;
    uint32_t cons = ring->consumer_idx;
    uint32_t used = (prod + ring->slot_count - cons) % ring->slot_count;
    return (ring->slot_count - 1 - used);
}

void zc_register_doorbell(int chan_id, doorbell_cb_t cb, void* arg) {
    if (chan_id < 0 || chan_id >= static_cast<int>(MAX_CHANNELS)) return;

    channel* ch = &g_channels[chan_id];
    if (!ch || ch->state == channel_state::CHAN_CLOSED) return;

    g_doorbell_cb[chan_id] = cb;
    g_doorbell_arg[chan_id] = arg;
}

void zc_get_mapping(int chan_id, uint64_t* phys_addr, uint32_t* slot_size,
                    uint32_t* slot_count, uint32_t* ring_size) {
    if (chan_id < 0 || chan_id >= static_cast<int>(MAX_CHANNELS)) return;

    channel* ch = &g_channels[chan_id];
    if (!ch || ch->state != channel_state::CHAN_OPEN) return;

    if (phys_addr) *phys_addr = ch->phys_addr;
    if (slot_size) *slot_size = ch->slot_size;
    if (slot_count) *slot_count = ch->slot_count;
    if (ring_size) *ring_size = sizeof(ring_header) + ch->slot_size * ch->slot_count;
}

} // namespace feather::kernel::zerocopy
