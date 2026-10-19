#pragma once
// FeatherOS Kernel — ISR stubs (C++ only, replaces isr.asm)
// For compilation checking. Real bootable kernel needs NASM for exact iretq stack layout.

#include <cstdint>

namespace feather::arch::interrupts {

// Common handler — declared in interrupts.h
extern "C" void isr_common_handler(uint8_t interrupt_num);

// ─── Individual ISR stubs — each calls the common handler with its number ────
inline void isr_stub_0()  { isr_common_handler(0); }
inline void isr_stub_1()  { isr_common_handler(1); }
inline void isr_stub_2()  { isr_common_handler(2); }
inline void isr_stub_3()  { isr_common_handler(3); }
inline void isr_stub_4()  { isr_common_handler(4); }
inline void isr_stub_5()  { isr_common_handler(5); }
inline void isr_stub_6()  { isr_common_handler(6); }
inline void isr_stub_7()  { isr_common_handler(7); }
inline void isr_stub_8()  { isr_common_handler(8); }
inline void isr_stub_9()  { isr_common_handler(9); }
inline void isr_stub_10() { isr_common_handler(10); }
inline void isr_stub_11() { isr_common_handler(11); }
inline void isr_stub_12() { isr_common_handler(12); }
inline void isr_stub_13() { isr_common_handler(13); }
inline void isr_stub_14() { isr_common_handler(14); }
inline void isr_stub_15() { isr_common_handler(15); }
inline void isr_stub_16() { isr_common_handler(16); }
inline void isr_stub_17() { isr_common_handler(17); }
inline void isr_stub_18() { isr_common_handler(18); }
inline void isr_stub_19() { isr_common_handler(19); }
inline void isr_stub_20() { isr_common_handler(20); }
inline void isr_stub_21() { isr_common_handler(21); }
inline void isr_stub_22() { isr_common_handler(22); }
inline void isr_stub_23() { isr_common_handler(23); }
inline void isr_stub_24() { isr_common_handler(24); }
inline void isr_stub_25() { isr_common_handler(25); }
inline void isr_stub_26() { isr_common_handler(26); }
inline void isr_stub_27() { isr_common_handler(27); }
inline void isr_stub_28() { isr_common_handler(28); }
inline void isr_stub_29() { isr_common_handler(29); }
inline void isr_stub_30() { isr_common_handler(30); }
inline void isr_stub_31() { isr_common_handler(31); }

// IRQs (32-47)
inline void isr_stub_32() { isr_common_handler(32); }
inline void isr_stub_33() { isr_common_handler(33); }
inline void isr_stub_34() { isr_common_handler(34); }
inline void isr_stub_35() { isr_common_handler(35); }
inline void isr_stub_36() { isr_common_handler(36); }
inline void isr_stub_37() { isr_common_handler(37); }
inline void isr_stub_38() { isr_common_handler(38); }
inline void isr_stub_39() { isr_common_handler(39); }
inline void isr_stub_40() { isr_common_handler(40); }
inline void isr_stub_41() { isr_common_handler(41); }
inline void isr_stub_42() { isr_common_handler(42); }
inline void isr_stub_43() { isr_common_handler(43); }
inline void isr_stub_44() { isr_common_handler(44); }
inline void isr_stub_45() { isr_common_handler(45); }
inline void isr_stub_46() { isr_common_handler(46); }
inline void isr_stub_47() { isr_common_handler(47); }

// ─── Stub address table ───────────────────────────────────────
extern "C" void* isr_stub_addresses[256];

inline void init_stub_table() {
    isr_stub_addresses[0]  = (void*)isr_stub_0;
    isr_stub_addresses[1]  = (void*)isr_stub_1;
    isr_stub_addresses[2]  = (void*)isr_stub_2;
    isr_stub_addresses[3]  = (void*)isr_stub_3;
    isr_stub_addresses[4]  = (void*)isr_stub_4;
    isr_stub_addresses[5]  = (void*)isr_stub_5;
    isr_stub_addresses[6]  = (void*)isr_stub_6;
    isr_stub_addresses[7]  = (void*)isr_stub_7;
    isr_stub_addresses[8]  = (void*)isr_stub_8;
    isr_stub_addresses[9]  = (void*)isr_stub_9;
    isr_stub_addresses[10] = (void*)isr_stub_10;
    isr_stub_addresses[11] = (void*)isr_stub_11;
    isr_stub_addresses[12] = (void*)isr_stub_12;
    isr_stub_addresses[13] = (void*)isr_stub_13;
    isr_stub_addresses[14] = (void*)isr_stub_14;
    isr_stub_addresses[15] = (void*)isr_stub_15;
    isr_stub_addresses[16] = (void*)isr_stub_16;
    isr_stub_addresses[17] = (void*)isr_stub_17;
    isr_stub_addresses[18] = (void*)isr_stub_18;
    isr_stub_addresses[19] = (void*)isr_stub_19;
    isr_stub_addresses[20] = (void*)isr_stub_20;
    isr_stub_addresses[21] = (void*)isr_stub_21;
    isr_stub_addresses[22] = (void*)isr_stub_22;
    isr_stub_addresses[23] = (void*)isr_stub_23;
    isr_stub_addresses[24] = (void*)isr_stub_24;
    isr_stub_addresses[25] = (void*)isr_stub_25;
    isr_stub_addresses[26] = (void*)isr_stub_26;
    isr_stub_addresses[27] = (void*)isr_stub_27;
    isr_stub_addresses[28] = (void*)isr_stub_28;
    isr_stub_addresses[29] = (void*)isr_stub_29;
    isr_stub_addresses[30] = (void*)isr_stub_30;
    isr_stub_addresses[31] = (void*)isr_stub_31;
    isr_stub_addresses[32] = (void*)isr_stub_32;
    isr_stub_addresses[33] = (void*)isr_stub_33;
    isr_stub_addresses[34] = (void*)isr_stub_34;
    isr_stub_addresses[35] = (void*)isr_stub_35;
    isr_stub_addresses[36] = (void*)isr_stub_36;
    isr_stub_addresses[37] = (void*)isr_stub_37;
    isr_stub_addresses[38] = (void*)isr_stub_38;
    isr_stub_addresses[39] = (void*)isr_stub_39;
    isr_stub_addresses[40] = (void*)isr_stub_40;
    isr_stub_addresses[41] = (void*)isr_stub_41;
    isr_stub_addresses[42] = (void*)isr_stub_42;
    isr_stub_addresses[43] = (void*)isr_stub_43;
    isr_stub_addresses[44] = (void*)isr_stub_44;
    isr_stub_addresses[45] = (void*)isr_stub_45;
    isr_stub_addresses[46] = (void*)isr_stub_46;
    isr_stub_addresses[47] = (void*)isr_stub_47;
    for (int i = 48; i < 256; i++) isr_stub_addresses[i] = (void*)isr_stub_0;
}

} // namespace feather::arch::interrupts
