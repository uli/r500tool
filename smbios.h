/* smbios.h
 * Copyright (c) 2009 Ulrich Hecht <ulrich.hecht@gmail.com>
 * Licensed under the terms of the GNU Public License v2
 */

#ifndef __SMBIOS_H
#define __SMBIOS_H

#include <stdint.h>

/* constants taken from toshset */
#define HCI_SET 0xff00 /* this is actually 0xffc0 in toshset, but it's 0xff00
                          in the DSDT, so I'm using that one */
#define HCI_GET 0xfe00 /* same here */
#define HCI_TR_BACKLIGHT 0x5
#define HCI_HOTKEY_EVENT 0x1e
#define HCI_LCD_BRIGHTNESS 0x2a

/* constants from DSDT */
#define HKCD 0x500

/* observed constants */
#define HKCD_F6_BRIGHTDOWN 0x40
#define HKCD_F7_BRIGHTUP 0x41

struct SRAM {
  uint32_t ieax;
  uint32_t iebx;
  uint32_t iecx;
  uint32_t iedx;
  uint32_t iesi;
  uint32_t iebp;
  uint32_t __pad1[2];
  uint32_t oeax;
  uint32_t oebx;
  uint32_t oecx;
  uint32_t oedx;
  uint32_t oesi;
  uint32_t oedi;
  uint32_t oebp;
};

void smbr(volatile struct SRAM *sram, uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx, uint8_t p);

#endif
