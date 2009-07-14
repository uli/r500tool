#include "smbios.h"

/* brightness query */
int bqc(volatile struct SRAM *sram)
{
  int brightness;
  smbr(sram, HCI_GET, HCI_LCD_BRIGHTNESS, 0, 0, 0xb2);
  if (sram->oeax == 0) {
    int brtv[] = {0x0a, 0x0f, 0x1e, 0x28, 0x37, 0x41, 0x50, 0x64};
    brightness = brtv[sram->oecx >> 13];
  }
  else brightness = 7;
  return brightness;
}

/* set brightness */
void bcm(volatile struct SRAM *sram, int brightness)
{
  smbr(sram, HCI_SET, HCI_LCD_BRIGHTNESS, brightness * 0xffff / 0x64, 0, 0xb2);
}

/* brightness levels from DSDT */
uint8_t bcl[] = {0x64, 0x0a, 0x0a, 0x0f, 0x1e, 0x28, 0x37, 0x41, 0x50, 0x64};

void transflective(volatile struct SRAM *sram, int flag)
{
  smbr(sram, HCI_SET, HCI_TR_BACKLIGHT, flag, 0, 0xb2);
}

void darker(volatile struct SRAM *sram)
{
  int i, newbright;
  int curbright = bqc(sram);
  for (newbright = i = 0; i < sizeof(bcl); i++) {
    if (bcl[i] < curbright && bcl[i] > newbright)
      newbright = bcl[i];
  }
  bcm(sram, newbright);
}

void brighter(volatile struct SRAM *sram)
{
  int i;
  int newbright = 100;
  int curbright = bqc(sram);
  for (i = 0; i < sizeof(bcl); i++) {
    if (bcl[i] > curbright && bcl[i] < newbright)
      newbright = bcl[i];
  }
  bcm(sram, newbright);
}
