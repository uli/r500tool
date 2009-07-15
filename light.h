/* light.h
 * Copyright (c) 2009 Ulrich Hecht <ulrich.hecht@gmail.com>
 * Licensed under the terms of the GNU Public License v2
 */

#include "smi.h"

int bqc(volatile struct SRAM *sram);
void bcm(volatile struct SRAM *sram, int brightness);
void transflective(volatile struct SRAM *sram, int flag);
void darker(volatile struct SRAM *sram);
void brighter(volatile struct SRAM *sram);
