/* smi.c
 * Copyright (c) 2009 Ulrich Hecht <ulrich.hecht@gmail.com>
 * Licensed under the terms of the GNU Public License v2
 */

#include "smi.h"
#include "darwinio.h"

void smbr(volatile struct SRAM* sram, uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx, uint8_t p)
{
  sram->ieax = eax;
  sram->iebx = ebx;
  sram->iecx = ecx;
  sram->iedx = edx;
  outb(p, 0xb2);
}

