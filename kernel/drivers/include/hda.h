#pragma once
// FeatherOS Kernel — Intel High Definition Audio (HDA) Driver
// HDA controller for audio playback and capture.

#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <cstddef>
#include <cstdint>

namespace feather::drivers::audio {

// HDA memory-mapped registers
struct hda_regs {
    uint32_t gcap;       // Global capabilities
    uint32_t vmin;       // Minor version
    uint32_t vmaj;       // Major version
    uint32_t outpay;     // Output payload capability
    uint32_t inpay;      // Input payload capability
    uint32_t gctl;       // Global control
    uint32_t wakeen;     // Wake enable
    uint32_t statsts;    // Status state change
    uint32_t reserved[2];
    uint32_t gcap2;      // Global capabilities 2
    uint32_t outpay2;    // Output payload 2
    uint32_t inpay2;     // Input payload 2
};

// HDA CORB (Command Output Ring Buffer) / RIRB (Response Input Ring Buffer)
struct hda_corb {
    uint32_t corbla;     // CORB lower base address
    uint32_t corbh;      // CORB upper base address
    uint32_t corbwp;     // CORB write pointer
    uint32_t corbrp;     // CORB read pointer
};

struct hda_rirb {
    uint32_t ribba;      // RIRB lower base address
    uint32_t ribbh;      // RIRB upper base address
    uint32_t ripwp;      // RIRB write pointer
    uint32_t riprp;      // RIRB read pointer
    uint32_t rirbcnt;    // RIRB count
};

// Audio codec widget types
enum hda_widget_type : uint8_t {
    HDA_WIDGET_CONVERTER   = 0x00,
    HDA_WIDGET_PIN         = 0x01,
    HDA_WIDGET_SUMMING     = 0x02,
    HDA_WIDGET_SELECTOR    = 0x03,
    HDA_WIDGET_MUX         = 0x04,
    HDA_WIDGET_DEMUX       = 0x05,
    HDA_WIDGET_DIGITAL_CONV= 0x06,
    HDA_WIDGET_OTHER       = 0x07,
};

// Initialize HDA controller
bool hda_init(uintptr_t mmio_base);

// Shutdown HDA controller
void hda_shutdown();

// Play audio (simplified)
int hda_play(const uint8_t* data, uint32_t len);

// Stop audio
void hda_stop();

// Set volume
void hda_set_volume(uint8_t percent);

// HDA controller instance
struct hda_controller {
    volatile hda_regs* regs;
    volatile hda_corb* corb;
    volatile hda_rirb* rirb;
    bool     initialized;
    uint8_t  volume;
};

extern hda_controller g_hda;

} // namespace feather::drivers::audio
