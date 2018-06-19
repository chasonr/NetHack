/* watdpmi.h -- DPMI functions for the Open Watcom compiler */

#ifndef WATDPMI_H
#define WATDPMI_H

#include <stdint.h>

/* Watcom does not provide DPMI support functions. Here we provide the needed
   functions under the same names that DJGPP uses. */
#pragma pack(__push, 1)
typedef union nh_dpmi_regs {
    struct {
        uint32_t edi;
        uint32_t esi;
        uint32_t ebp;
        uint32_t res;
        uint32_t ebx;
        uint32_t edx;
        uint32_t ecx;
        uint32_t eax;
    } d;
    struct {
        uint16_t di, di_hi;
        uint16_t si, si_hi;
        uint16_t bp, bp_hi;
        uint16_t res, res_hi;
        uint16_t bx, bx_hi;
        uint16_t dx, dx_hi;
        uint16_t cx, cx_hi;
        uint16_t ax, ax_hi;
        uint16_t flags;
        uint16_t es;
        uint16_t ds;
        uint16_t fs;
        uint16_t gs;
        uint16_t ip;
        uint16_t cs;
        uint16_t sp;
        uint16_t ss;
    } x;
    struct {
        uint8_t edi[4];
        uint8_t esi[4];
        uint8_t ebp[4];
        uint8_t res[4];
        uint8_t bl, bh, ebx_b2, ebx_b3;
        uint8_t dl, dh, edx_b2, edx_b3;
        uint8_t cl, ch, ecx_b2, ecx_b3;
        uint8_t al, ah, eax_b2, eax_b3;
    } h;
} __dpmi_regs;
#pragma pack(__pop)

int __dpmi_int(int vector, __dpmi_regs *regs);
int __dpmi_allocate_dos_memory(int paragraphs, int *selector_max);
int __dpmi_free_dos_memory(int selector);

/* Not actually DPMI, but useful just the same */
void dosmemget(unsigned long offset, size_t length, void *buffer);
void dosmemput(const void *buffer, size_t length, unsigned long offset);

#endif
