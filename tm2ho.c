/*
  tm2ho.c - A.E. Timemaster II H.O. emulation
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

#include <ctype.h>
#include <stdio.h>
#include <time.h>
#include <gtk/gtk.h>

#include "memory.h"
#include "slots.h"

void  i60_RTS (void);

#undef READ
#undef WRITE

/* clock "modes" */
enum { MODE_APPLECLOCK = 1, MODE_TIMEMASTER = 3 };

/* time formats */
enum { APPLECLOCK, TIMEMASTER, THUNDER_1, THUNDER_2, THUNDER_3, THUNDER_4, THUNDER_5 };

static char *  time_format[] =
{
  "\"%m/%d %H:%M:%S.%w%y\r",
  "\"%w %m/%d/%y %H:%M:%S\r",
  "\"%a %b %d %I:%M:%S %p\r",
  "\"%a %b %d %H:%M:%S\r",
  "%m,%2w,%d,%H,%M,%S\r",
  " %a %b %d %I:%M:%S %p\r",
  " %a %b %d %H:%M:%S\r",
};

#define READ(a)    (read_table[(a)/256]((a)))
#define WRITE(a,b) (write_table[(a)/256]((a),(b)))

static cpu_state_t     cpu;

static r_func_t *      read_table;
static w_func_t *      write_table;

static unsigned short  tm2ho_address = 0;

static unsigned char   tm2ho_mode          = MODE_TIMEMASTER;
static unsigned char   tm2ho_format        = TIMEMASTER;
static unsigned char   tm2ho_interrupts_on = 0;

static char            time_buf[256];

void  tm2ho_peek (void)
{
}

static void  tm2ho_read_time (void)
{
  char         tc;
  int          idx;
  int          tlen;
  time_t       simple_time;
  struct tm *  formatted_time;

  simple_time = time(0);
  if ((time_t)-1 == simple_time)
    simple_time = 0;

  formatted_time = localtime(&simple_time);
  tlen = 0x200 + strftime(time_buf, 256, time_format[tm2ho_format], formatted_time);

  for (idx = 0x200; idx < tlen; idx++)
    {
      tc = 0x80 | toupper(time_buf[idx - 0x200]);
      WRITE(idx, tc);

#if 0
      printf("%c", tc & 0x7f); fflush(stdout);
      if (tc == (char)0x8d) putchar('\n');
#endif
    }
}

static void  tm2ho_select_format (void)
{
  switch (*(cpu.A))
    {
    case 0x80 + ' ': tm2ho_format = APPLECLOCK; break;
    case 0x80 + ':': tm2ho_format = TIMEMASTER; break;
    case 0x80 + '%': tm2ho_format = THUNDER_1;  break;
    case 0x80 + '&': tm2ho_format = THUNDER_2;  break;
    case 0x80 + '#': tm2ho_format = THUNDER_3;  break;
    case 0x80 + '>': tm2ho_format = THUNDER_4;  break;
    case 0x80 + '<': tm2ho_format = THUNDER_5;  break;
    case 0x80 + '.': tm2ho_interrupts_on = 1;   break;

    default:
      break;
    }
}

static void  tm2ho_appleclock (void)
{
  int          time_len;
  int          link_addr;
  int          input_link;
  int          output_link;
  time_t       simple_time;
  struct tm *  formatted_time;

  static int   tbuf_idx      = 0;
  static int   time_char     = 0;
  static int   echo_toggle   = 1;
  static int   need_new_time = 1;

#if 0
  printf("=>[%2.2X]", *(cpu.A));
  if (*(cpu.A) > 0xA0)
    printf("    %c", *(cpu.A) & 0x7F);
  putchar('\n');
  fflush(stdout);
#endif

  link_addr   = 256 * READ(0x37);
  link_addr  += READ(0x36);
  output_link = (tm2ho_address == link_addr);

  link_addr  = 256 * READ(0x39);
  link_addr += READ(0x38);
  input_link = (tm2ho_address == link_addr);

  if (!output_link && !input_link)
    return;

  echo_toggle = 1 - echo_toggle;

  if (output_link && echo_toggle)
    {
      return;
    }

  if (need_new_time)
    {
      if (output_link && 0x80 != *(cpu.A))
	{
	  tm2ho_select_format();
	}
      else
	{
	  simple_time    = time(0);
	  formatted_time = localtime(&simple_time);
	  time_len       = strftime(time_buf, 256, time_format[tm2ho_format], formatted_time);
	  need_new_time  = 0;
	}

      echo_toggle = 1;
      return;
    }

  time_char = 0x80 | toupper(time_buf[tbuf_idx++]);
  *(cpu.A)  = time_char;

  if (0x8D == time_char)
    {
      tbuf_idx      = 0;
      need_new_time = 1;
    }

#if 0
  printf("<=[%2.2X]", *(cpu.A));
  if (*(cpu.A) > 0xA0)
    printf(" %c", *(cpu.A) & 0x7F);
  putchar('\n');
  fflush(stdout);
#endif
}

void  tm2ho_call (void)
{
  switch (*(cpu.PC) & 0xFF)
    {
    case 0x00:
      tm2ho_appleclock();
      break;

    case 0x08:
      tm2ho_read_time();
      break;

    case 0x0B:
      tm2ho_select_format();
      break;

    default:
      break;
    }

  i60_RTS();
}

void  tm2ho_time (void)
{
}

unsigned char  tm2ho_read (unsigned short  r_addr)
{
  if (r_addr >= 0xC100 && r_addr < 0xC800)
    {
      switch(r_addr & 0xFF)
	{
	case 0x00: return 0x08;
	case 0x01: return 0x78;
	case 0x02: return 0x28;
	case 0x04: return 0x58;
	case 0x05: return 0xFF;
	case 0x06: return 0x70;
	case 0xFE: return 0xB2;
	case 0xFF: return tm2ho_mode;

	default:
	  break;
	}
    }

  return 0;
}

void  tm2ho_write (unsigned short  w_addr,
		   unsigned char   w_byte)
{
}

void  tm2ho_info (unsigned char *  bits)
{
  *bits = SLOT_CALL;
}

void  tm2ho_init (unsigned short  slot,
                  cpu_state_t *   cpup,
                  r_func_t *      rtab,
                  w_func_t *      wtab)
{
  cpu         = *cpup;
  read_table  = rtab;
  write_table = wtab;

  tm2ho_address  = 0xC000 + (0x100 * slot);
}
