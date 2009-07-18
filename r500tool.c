/* r500tool.c
 * Tool to control LCD backlight brightness on the Toshiba Portege R500 under
 * OSX. Also switches sound output between speakers and headphones based on
 * the state of the headphone jack.
 * Copyright (c) 2009 Ulrich Hecht <ulrich.hecht@gmail.com>
 * Licensed under the terms of the GNU Public License v2
 */

#include "darwinio.h"
#include "hpjack.h"
#include "light.h"
#include "smi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void watcher(volatile struct SRAM* sram, volatile unsigned char* sram_c)
{
    uint8_t oldhotkey = 0;
    int count = 50;
    init_watch_headphone_jack();
    for(;;) {
      watch_headphone_jack();
      uint8_t hotkey = sram_c[HKCD];
      if (!(hotkey & 0x80) && hotkey && hotkey != oldhotkey) {
        switch (hotkey) {
        case HKCD_F6_BRIGHTDOWN:
          darker(sram);
          break;
        case HKCD_F7_BRIGHTUP:
          brighter(sram);
          break;
        default:
          fprintf(stderr, "unknown hotkey code 0x%x\n", hotkey);
          break;
        }
        oldhotkey = hotkey;
      }
      if (hotkey == oldhotkey) {
        count--;
        if (!count) {
          oldhotkey = 0;
          count = 8;
        }
      }
      if (!hotkey || (hotkey & 0x80)) {
          oldhotkey = 0;
          count = 30;
      }
      if (hotkey) usleep(10000);
      else usleep(200000);
    }
}

int main(int argc, char **argv)
{
  int i;
  if (argc < 2) {
usage:
    fprintf(stderr,"Usage: %s [+|-|q|ton|toff|dump|daemon|hpjack|<brightness (percent)>]\n", argv[0]);
    exit(1);
  }
  char* parm = argv[1];
  if (strcmp(parm,"-") && strcmp(parm,"+") && strcmp(parm, "q") 
      && strcmp(parm,"ton") && strcmp(parm, "toff") && strcmp(parm, "dump")
      && strcmp(parm, "daemon") && strcmp(parm, "watch")
      && strcmp(parm, "smb") && strcmp(parm, "hpjack") && strcmp(parm, "killdvd")
      && (parm[0] < '0' || parm[0] > '9' || atoi(parm) < -1 || atoi(parm) > 100)) {
      goto usage;
  }
  if (iopl(3) < 0) {
    perror("iopl");
    fprintf(stderr, "Maybe you forgot to install the DirectIO driver?\n");
    exit(1);
  }
  /* map the magic memory area defined in DSDT */
  volatile unsigned char* sram_c = map_physical(0xee800, 0x1800);
  volatile struct SRAM* sram = (volatile struct SRAM*)sram_c;
  
  if (!strcmp(parm, "dump")) {
    for (i = 0; i < 0x1800; i++) {
      if (i%16==0) {
        printf("0x%08x: ", i);
      }
      printf("%02x ", sram_c[i]);
      if (i%16==15)
        printf("\n");
    }
    return 0;
  }

  if (!strcmp(parm, "watch")) {
    unsigned char mem[0x1800];
    int change = 1;
    memcpy(mem, (void*)sram_c, 0x1800);
    for(;;) {
      for (i = 0; i < 0x1800; i++) {
        if (mem[i] != sram_c[i]) {
          printf("0x%04x: 0x%02x -> 0x%02x\n", i, mem[i], sram_c[i]);
          change = 1;
        }
      }
      if (change) memcpy(mem, (void*)sram_c, 0x1800);
      usleep(10000);
    }
  }
  
  if (!strcmp(parm, "smb")) {
#define NARG(x) (strtol(argv[x], NULL, 0))
    smbr(sram, NARG(2), NARG(3), NARG(4), NARG(5), NARG(6));
    printf("eax 0x%x ebx 0x%x ecx 0x%x edx 0x%x esi 0x%x edi 0x%x\n",
           sram->oeax, sram->oebx, sram->oecx, sram->oedx, sram->oesi, sram->oedi);
    return 0;
  }
  
  if (!strcmp(parm, "daemon")) {
    watcher(sram, sram_c);
  }
  else if (!strcmp(parm, "hpjack")) {
    init_watch_headphone_jack();
    printf("%d\n", read_headphone_jack());
  }
  else if (!strcmp(parm, "killdvd")) {
    smbr(sram, 0xfa00, 0x3100, 0, 0, 0xb2); /* from DSDT */
  }
  else if (parm[0] == 'q') {
    printf("%d\n", bqc(sram));
  }
  else if (parm[0] == '-') {
    darker(sram);
  }
  else if (parm[0] == '+') {
    brighter(sram);
  }
  else if (!strcmp(parm, "ton")) {
    transflective(sram, 0);
  }
  else if (!strcmp(parm, "toff")) {
    transflective(sram, 1);
  }
  else {
    bcm(sram, atoi(parm));
  }
  return 0;
}
