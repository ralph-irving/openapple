/*
  ramworks.c
  Copyright 2000,2001 by William Sheldon Simms III

  This file is a part of open apple -- a free Apple II emulator.
 
  Open apple is free software; you can redistribute it and/or modify it under the terms
  of the GNU General Public License as published by the Free Software Foundation; either
  version 2, or (at your option) any later version.
 
  Open apple is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License along with open apple;
  see the file COPYING. If not, visit the Free Software Foundation website at http://www.fsf.org
*/

#include <stdio.h>
#include <stdlib.h>

#define MEMORY_BANKS 16  /* 1Mb */

static unsigned char *  card_memory;
static unsigned char ** auxmem;

static unsigned char current_bank = 0;

int init_ramworks (unsigned char **  apple_auxmem)
{
  unsigned int  page_idx;

  card_memory = (unsigned char *)malloc(MEMORY_BANKS * 0x10000);
  if (0 == card_memory)
    return -1;

  auxmem = apple_auxmem;

  for (page_idx = 0; page_idx < 0x100; page_idx++)
    auxmem[page_idx] = card_memory + (0x100 * page_idx);

  return 0;
}

unsigned char read_ramworks (unsigned short  r_addr)
{
  return 0;
}

void write_ramworks (unsigned short  w_addr, unsigned char w_byte)
{
  unsigned int page_idx;

  if ((w_addr & 0xFFFD) == 0xC071)
    {
      current_bank = w_byte % MEMORY_BANKS;

      /* printf("$%4.4x : %2.2x => enabling bank %d\n", w_addr, w_byte, current_bank); */

      for (page_idx = 0; page_idx < 0x100; page_idx++)
        auxmem[page_idx] = card_memory + (0x10000 * current_bank) + (0x100 * page_idx);
    }
}
