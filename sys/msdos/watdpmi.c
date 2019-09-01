/* watdpmi.h -- DPMI functions for the Open Watcom compiler */

#ifndef __WATCOMC__
# error "watdpmi.c is made for the Open Watcom compiler"
#endif

#include <string.h>
#include <i86.h>
#include "watdpmi.h"

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

int
__dpmi_physical_address_mapping(__dpmi_meminfo *info)
{
    unsigned char flags = 0;
    uint16_t retcode = 0;
    uint32_t addr, size;

    addr = info->address;
    size = info->size;
    _asm {
        mov ax,0x0800
        mov ecx,addr
        mov edi,size
        mov ebx,ecx
        mov esi,edi
        shr ebx,16
        shr esi,16
        int 0x31
        mov retcode,ax
        lahf
        mov flags,ah
        shl ebx,16
        mov bx,cx
        mov addr,ebx
    }

    if (flags & 0x1) {
        return -1;
    } else {
        info->address = addr;
        return 0;
    }
}

unsigned
__dpmi_allocate_ldt_descriptors(unsigned count)
{
    unsigned char flags = 0;
    uint16_t retcode = 0;

    _asm {
        mov eax,0x0000
        mov ecx,count
        int 0x31
        mov retcode,ax
        lahf
        mov flags,ah
    }
    if (flags & 0x1) {
        return 0;
    } else {
        return retcode;
    }
}

int
__dpmi_free_ldt_descriptor(unsigned sel)
{
    unsigned char flags = 0;
    uint16_t retcode = 0;

    _asm {
        mov eax,0x0001
        mov ebx,sel
        int 0x31
        mov retcode,ax
        lahf
        mov flags,ah
    }

    return (flags & 0x1) ? -1 : 0;
}

int
__dpmi_set_segment_base_address(unsigned sel, uint32_t addr)
{
    unsigned char flags = 0;
    uint16_t retcode = 0;

    _asm {
        mov ax,0x0007
        mov ebx,sel
        mov edx,addr
        mov ecx,edx
        shr ecx,16
        int 0x31
        mov retcode,ax
        lahf
        mov flags,ah
    }
    return (flags & 1) ? -1 : 0;
}

int
__dpmi_set_segment_limit(unsigned sel, uint32_t limit)
{
    unsigned char flags = 0;
    uint16_t retcode = 0;

    _asm {
        mov ax,0x0008
        mov ebx,sel
        mov edx,limit
        mov ecx,edx
        shr ecx,16
        int 0x31
        mov retcode,ax
        lahf
        mov flags,ah
    }
    return (flags & 1) ? -1 : 0;
}

unsigned
__dpmi_segment_to_descriptor(unsigned segment)
{
    unsigned char flags = 0;
    uint16_t retcode = 0;

    _asm {
        mov eax,0x0002
        mov ebx,segment
        int 0x31
        mov retcode,ax
        lahf
        mov flags,ah
    }
    return (flags & 1) ? 0 : retcode;
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

uint8_t
_farpeekb(uint16_t selector, unsigned offset)
{
    return *(uint8_t const __far *)MK_FP(selector, offset);
}

uint16_t
_farpeekw(uint16_t selector, unsigned offset)
{
    return *(uint16_t const __far *)MK_FP(selector, offset);
}

uint32_t
_farpeekl(uint16_t selector, unsigned offset)
{
    return *(uint32_t const __far *)MK_FP(selector, offset);
}

void
_farpokeb(uint16_t selector, unsigned offset, uint8_t num)
{
    *(uint8_t __far *)MK_FP(selector, offset) = num;
}

void
_farpokew(uint16_t selector, unsigned offset, uint16_t num)
{
    *(uint16_t __far *)MK_FP(selector, offset) = num;
}

void
_farpokel(uint16_t selector, unsigned offset, uint32_t num)
{
    *(uint32_t __far *)MK_FP(selector, offset) = num;
}
