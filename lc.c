/*
  lc.c - 'language card' bank switching
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
#include "memory.h"

static unsigned char  switch_lc_bank  = 1;
static unsigned char  switch_lc_read  = 1;
static unsigned char  switch_lc_write = 1;

static unsigned char  r_bank_2 (unsigned short  r_addr)
{
  r_addr -= 0x1000;
  return (*(r_memory[r_addr / 256]))[r_addr & 0xFF];
}

static void  w_bank_2 (unsigned short  w_addr,
                       unsigned char   w_byte)
{
  w_addr -= 0x1000;
  (*(w_memory[w_addr / 256]))[w_addr & 0xFF] = w_byte;
}

unsigned char  language_card_bank_status (void)
{
  return switch_lc_bank;
}

unsigned char  language_card_read_status (void)
{
  return switch_lc_read;
}

unsigned char  language_card_write_status (void)
{
  return switch_lc_write;
}

/* #define  PSW */

void  language_card_switch (int  wr_byte,
                            int  lc_byte)
{
  static int  write_enabled = 0;

  int  page_idx;

#ifdef PSW
  static long  lc_count = 0;

  if (wr_byte)
    printf("[%ld]  w: C08%X", lc_count, lc_byte);
  else
    printf("[%ld]  r: C08%X", lc_count, lc_byte);

  lc_count++;
#endif

  if (lc_byte & 0x08)
    switch_lc_bank = 0;
  else
    switch_lc_bank = 1;

  if (lc_byte & 0x01)
    {
      if (lc_byte & 0x02)
        switch_lc_read = 1;
      else
        switch_lc_read = 0;

      if (0 == wr_byte)
        {
          if (write_enabled)
            switch_lc_write = 1;
          else
            switch_lc_write = 0;

          write_enabled = 1;
        }
#if 0
      else
        {
          write_enabled = 0;
        }
#endif
    }
  else
    {
      if (lc_byte & 0x02)
        switch_lc_read = 0;
      else
        switch_lc_read = 1;

      if (0 == wr_byte)
        {
          switch_lc_write = 0;
          write_enabled = 0;
        }
#if 0
      else
        {
          write_enabled = 0;
        }
#endif
    }

#ifdef PSW
  printf("  bank:%d read:%d write:%d\n", switch_lc_bank, switch_lc_read, switch_lc_write);
  fflush(stdout);
#endif

  if (0 == switch_lc_write)
    {
      for (page_idx = 0xD0; page_idx < 0x100; page_idx++)
        w_page[page_idx] = w_nop;
    }
  else
    {
      if (0 == switch_lc_bank)
        {
          for (page_idx = 0xD0; page_idx < 0xE0; page_idx++)
            w_page[page_idx] = w_std;
        }
      else
        {
          for (page_idx = 0xD0; page_idx < 0xE0; page_idx++)
            w_page[page_idx] = w_bank_2;
        }

      for (page_idx = 0xE0; page_idx < 0x100; page_idx++)
        w_page[page_idx] = w_std;
    }

  if (0 == switch_lc_read)
    {
      for (page_idx = 0xD0; page_idx < 0x100; page_idx++)
        r_page[page_idx] = r_rom;
    }
  else
    {
      if (0 == switch_lc_bank)
        {
          for (page_idx = 0xD0; page_idx < 0xE0; page_idx++)
            r_page[page_idx] = r_std;
        }
      else
        {
          for (page_idx = 0xD0; page_idx < 0xE0; page_idx++)
            r_page[page_idx] = r_bank_2;
        }

      for (page_idx = 0xE0; page_idx < 0x100; page_idx++)
        r_page[page_idx] = r_std;
    }
}

void  language_card_init (void)
{
  int  page_idx;

  switch_lc_bank  = 0;
  switch_lc_read  = 0;
  switch_lc_write = 0;

  for (page_idx = 0xD0; page_idx < 0x100; page_idx++)
    {
      r_page[page_idx] = r_rom;
      w_page[page_idx] = w_nop;
    }
}
