/*
  ext80col.c - extended 80 column (auxiliary slot) card
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

static unsigned char *   card_memory;
static unsigned char **  auxmem;

int init_extended_80col (unsigned char **  apple_auxmem)
{
  unsigned int  page_idx;

  card_memory = (unsigned char *)malloc(0x10000);
  if (0 == card_memory)
    return -1;

  auxmem = apple_auxmem;

  for (page_idx = 0; page_idx < 0x100; page_idx++)
    auxmem[page_idx] = card_memory + (256 * page_idx);

  return 0;
}

unsigned char read_extended_80col (unsigned short  r_addr)
{
  return 0;
}

void write_extended_80col (unsigned short  w_addr, unsigned char w_byte)
{
}
