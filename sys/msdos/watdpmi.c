/* watdpmi.h -- DPMI functions for the Open Watcom compiler */

#include <string.h>
#include "watdpmi.h"

#ifndef __WATCOMC__
# error "watdpmi.c is made for the Open Watcom compiler"
#endif

/* Watcom does not provide DPMI support functions. Here we provide the needed
   functions under the same names that DJGPP uses. */
int
__dpmi_int(int vector, __dpmi_regs *regs)
{
    unsigned char flags = 0;
	int rc;

    _asm {
        mov ax,ds
        mov es,ax
        mov eax,0x0300
        mov ebx,vector
        and ebx,0xFF
        xor ecx,ecx
        mov edi,regs
        int 0x31
        lahf
        mov flags,ah
    }
    if ((flags & 0x1) == 0) {
        rc = -1;
    } else {
        rc = 0;
    }
	return rc;
}

int
__dpmi_allocate_dos_memory(int paragraphs, int *selector_max)
{
    unsigned char flags = 0;
    int dos_seg = 0;
    int dos_sel = 0;
    int dos_max = 0;

    _asm {
        mov eax,0x0100
        mov ebx,paragraphs
        int 0x31
        mov dos_seg,eax
        mov dos_sel,edx
        mov dos_max,ebx
        lahf
        mov flags,ah
    }
    if ((flags & 0x1) == 0) {
        /* carry clear; return is OK */
        *selector_max = dos_sel;
    } else {
        *selector_max = dos_max;
        dos_seg = -1;
    }
	return dos_seg;
}

int
__dpmi_free_dos_memory(int selector)
{
    unsigned char flags = 0;
	int rc;

    _asm {
        mov eax,0x0101
        mov edx,selector
        int 0x31
        lahf
        mov flags,ah
    }
    if ((flags & 0x1) == 0) {
        rc = -1;
    } else {
        rc = 0;
    }
	return rc;
}

/* Not actually DPMI, but useful just the same */
void
dosmemget(unsigned long offset, size_t length, void *buffer)
{
    memcpy(buffer, (void *)offset, length);
}

void
dosmemput(const void *buffer, size_t length, unsigned long offset)
{
    memcpy((void *)offset, buffer, length);
}
