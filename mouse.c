/*
  mouse.c - AppleMouse II card emulation
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

#include <gtk/gtk.h>
#include <stdio.h>

#include "apple.h"
#include "cpu.h"
#include "memory.h"
#include "mouse.h"
#include "slots.h"

void  i60_RTS (void);

enum
{
  MODE_ON        = 1,
  MODE_MOVEIRQ   = 2,
  MODE_BUTTONIRQ = 4,
  MODE_TIMEDIRQ  = 8
};

enum
{
  STATUS_MOVEIRQ    = 2,
  STATUS_BUTTONIRQ  = 4,
  STATUS_TIMEDIRQ   = 8,
  STATUS_XY_CHANGED = 32,
  STATUS_OLD_BUTTON = 64,
  STATUS_BUTTON     = 128
};

#undef READ
#undef WRITE

#define READ(a)    (read_table[(a)/256]((a)))
#define WRITE(a,b) (write_table[(a)/256]((a),(b)))

static r_func_t *      read_table;
static w_func_t *      write_table;

static unsigned short  am2_slot   = 0;

static unsigned char   am2_mode   = 0;
static unsigned char   am2_status = 0;

static short           am2_xpos   = 0;
static short           am2_ypos   = 0;

static short           am2_xlow   = 0;
static short           am2_xhigh  = 1023;
static short           am2_ylow   = 0;
static short           am2_yhigh  = 1023;

static unsigned char   am2_interrupted = 0;

static cpu_state_t     cpu;

static gint  old_px = 0;
static gint  old_py = 0;

/*******************************/

static void  am2_setmouse (void)
{
  unsigned char  mode;

  mode = *(cpu.A);

  if (mode & 0xF0)
    {
      *(cpu.C) = 1;
      return;
    }

  am2_mode = mode;

  *(cpu.C) = 0;
}

static void  am2_servemouse (void)
{
  WRITE(0x778 + am2_slot, am2_status);

  if (am2_interrupted)
    *(cpu.C) = 0;
  else
    *(cpu.C) = 1;

  am2_interrupted = 0;
}

static void  am2_readmouse (void)
{
  am2_status &= 0xF1;

  /*
  printf("%d %d %d 0 0 0 0 0\n",
         am2_status & 0x80 ? 1 : 0,
         am2_status & 0x40 ? 1 : 0,
         am2_status & 0x20 ? 1 : 0);
  */

  WRITE(0x478 + am2_slot, am2_xpos & 0xFF);
  WRITE(0x4F8 + am2_slot, am2_ypos & 0xFF);
  WRITE(0x578 + am2_slot, am2_xpos / 256);
  WRITE(0x5F8 + am2_slot, am2_ypos / 256);
  WRITE(0x778 + am2_slot, am2_status);
  WRITE(0x7F8 + am2_slot, am2_mode);

  *(cpu.C) = 0;
}

static void  am2_clearmouse (void)
{
  GdkModifierType  state;

  //gdk_window_get_pointer(win_ptr, &old_px, &old_py, &state);

  am2_xpos = 0;
  am2_ypos = 0;

  WRITE(0x478 + am2_slot, 0);
  WRITE(0x4F8 + am2_slot, 0);
  WRITE(0x578 + am2_slot, 0);
  WRITE(0x5F8 + am2_slot, 0);

  *(cpu.C) = 0;
}

static void  am2_posmouse (void)
{
  am2_xpos  = READ(0x578 + am2_slot);
  am2_xpos *= 256;
  am2_xpos += READ(0x478 + am2_slot);

  am2_ypos  = READ(0x5F8 + am2_slot);
  am2_ypos *= 256;
  am2_ypos += READ(0x4F8 + am2_slot);

  /* printf("posmouse(x=%d,y=%d);\n", am2_xpos, am2_ypos); */

  *(cpu.C) = 0;
}

static void  am2_clampmouse (void)
{
  if (*(cpu.A))
    {
      am2_ylow  = READ(0x578);
      am2_ylow *= 256;
      am2_ylow += READ(0x478);

      am2_yhigh  = READ(0x5F8);
      am2_yhigh *= 256;
      am2_yhigh += READ(0x4F8);
    }
  else
    {
      am2_xlow  = READ(0x578);
      am2_xlow *= 256;
      am2_xlow += READ(0x478);

      am2_xhigh  = READ(0x5F8);
      am2_xhigh *= 256;
      am2_xhigh += READ(0x4F8);
    }
}

static void  am2_homemouse (void)
{
  am2_xpos = am2_xlow;
  am2_ypos = am2_ylow;

  *(cpu.C) = 0;
}

static void  am2_initmouse (void)
{
  am2_xlow   = 0;
  am2_xhigh  = 1023;

  am2_ylow   = 0;
  am2_yhigh  = 1023;

  am2_mode   = 0;
  am2_status = 0;

  *(cpu.C) = 0;
}

void  am2_call (void)
{
  switch (*(cpu.PC) & 0xFF)
    {
    case 0x1A:
      printf("mouse: get clamping values!\n");
      break;

    case 0x1C:
      printf("mouse: set interrupt rate!\n");
      break;

    case 0x8A:
      am2_clampmouse();
      break;

    case 0x9B:
      am2_readmouse();
      break;

    case 0xA4:
      am2_clearmouse();
      break;

    case 0xB3:
      am2_setmouse();
      break;

    case 0xBC:
      am2_initmouse();
      break;

    case 0xC0:
      am2_posmouse();
      break;

    case 0xC4:
      am2_servemouse();
      break;

    case 0xDD:
      am2_homemouse();
      break;

    default:
      break;
    }

  i60_RTS();
}

unsigned char mouse_bytes[256] =
{
  0x2C, 0x58, 0xFF, 0x70, 0x1B, 0x38, 0x90, 0x18, 0xB8, 0x50, 0x15, 0x01, 0x20, 0xF4, 0xF4, 0xF4,
  0xF4, 0x00, 0xB3, 0xC4, 0x9B, 0xA4, 0xC0, 0x8A, 0xDD, 0xBC, 0x48, 0xF0, 0x53, 0xE1, 0xE6, 0xEC,
  0x08, 0x78, 0x8D, 0xF8, 0x07, 0x48, 0x98, 0x48, 0x8A, 0x48, 0x20, 0x58, 0xFF, 0xBA, 0xBD, 0x00,
  0x01, 0xAA, 0x08, 0x0A, 0x0A, 0x0A, 0x0A, 0x28, 0xA8, 0xAD, 0xF8, 0x07, 0x8E, 0xF8, 0x07, 0x48,
  0xA9, 0x08, 0x70, 0x67, 0x90, 0x4D, 0xB0, 0x55, 0x29, 0x01, 0x09, 0xF0, 0x9D, 0x38, 0x06, 0xA9,
  0x02, 0xD0, 0x40, 0x29, 0x0F, 0x09, 0x90, 0xD0, 0x35, 0xFF, 0xFF, 0xB9, 0x83, 0xC0, 0x29, 0xFB,
  0x99, 0x83, 0xC0, 0xA9, 0x3E, 0x99, 0x82, 0xC0, 0xB9, 0x83, 0xC0, 0x09, 0x04, 0x99, 0x83, 0xC0,
  0xB9, 0x82, 0xC0, 0x29, 0xC1, 0x1D, 0xB8, 0x05, 0x99, 0x82, 0xC0, 0x68, 0xF0, 0x0A, 0x6A, 0x90,
  0x75, 0x68, 0xAA, 0x68, 0xA8, 0x68, 0x28, 0x60, 0x18, 0x60, 0x29, 0x01, 0x09, 0x60, 0x9D, 0x38,
  0x06, 0xA9, 0x0E, 0x9D, 0xB8, 0x05, 0xA9, 0x01, 0x48, 0xD0, 0xC0, 0xA9, 0x0C, 0x9D, 0xB8, 0x05,
  0xA9, 0x02, 0xD0, 0xF4, 0xA9, 0x30, 0x9D, 0x38, 0x06, 0xA9, 0x06, 0x9D, 0xB8, 0x05, 0xA9, 0x00,
  0x48, 0xF0, 0xA8, 0xC9, 0x10, 0xB0, 0xD2, 0x9D, 0x38, 0x07, 0x90, 0xEA, 0xA9, 0x04, 0xD0, 0xEB,
  0xA9, 0x40, 0xD0, 0xCA, 0xA4, 0x06, 0xA9, 0x60, 0x85, 0x06, 0x20, 0x06, 0x00, 0x84, 0x06, 0xBA,
  0xBD, 0x00, 0x01, 0xAA, 0x0A, 0x0A, 0x0A, 0x0A, 0xA8, 0xA9, 0x20, 0xD0, 0xC9, 0xA9, 0x70, 0xD0,
  0xC5, 0x48, 0xA9, 0xA0, 0xD0, 0xA8, 0x29, 0x0F, 0x09, 0xB0, 0xD0, 0xBA, 0xA9, 0xC0, 0xD0, 0xB6,
  0xA9, 0x02, 0xD0, 0xB7, 0xA2, 0x03, 0x38, 0x60, 0xFF, 0xFF, 0xFF, 0xD6, 0xFF, 0xFF, 0xFF, 0x01
};

unsigned char  am2_read (unsigned short  r_addr)
{
  /*
  printf("$%4.4X\n", r_addr);
  trace = 4;
  */

  if (r_addr >= 0xC100 && r_addr < 0xC800)
    {
      return mouse_bytes[r_addr & 0xFF];
#if 0
      switch(r_addr & 0xFF)
        {
        case 0x05: return 0x38;
        case 0x07: return 0x18;
        case 0x0B: return 0x01;
        case 0x0C: return 0x20;
        case 0x12: return 0xB3;
        case 0x13: return 0xC4;
        case 0x14: return 0x9B;
        case 0x15: return 0xA4;
        case 0x16: return 0xC0;
        case 0x17: return 0x8A;
        case 0x18: return 0xDD;
        case 0x19: return 0xBC;
        case 0xFB: return 0xD6;

        default:
          break;
        }
#endif
    }

  return 0;
}

void  am2_write (unsigned short  w_addr,
                 unsigned char   w_byte)
{
}

void  am2_peek (void)
{
}

void  am2_time (void)
{
  static char  old_button = 0;

  gint  px;
  gint  py;
  char  xy_changed;

  long  xpos;
  long  ypos;

  GdkModifierType  state;

  /*
    if (!(am2_mode & MODE_ON))
    return;
  */

  //gdk_window_get_pointer(win_ptr, &px, &py, &state);

  xy_changed = 0;

  /* if (px != old_px && px > SCREEN_BORDER_WIDTH && px < (SCREEN_WIDTH - SCREEN_BORDER_WIDTH)) */
  if (px != old_px)
    {
      xy_changed = 1;

      /* xpos = px - SCREEN_BORDER_WIDTH; */
      xpos = am2_xpos + (px - old_px);
      old_px = px;

      if (xpos > am2_xhigh)
        xpos = am2_xhigh;
      else if (xpos < am2_xlow)
        xpos = am2_xlow;

      am2_xpos = (short)xpos;

      if (am2_mode & MODE_MOVEIRQ)
        {
          am2_interrupted = 1;
          am2_status |= STATUS_MOVEIRQ;
        }
    }

  /* if (py != old_py && py > SCREEN_BORDER_HEIGHT && py < (SCREEN_HEIGHT - SCREEN_BORDER_HEIGHT)) */
  if (py != old_py)
    {
      xy_changed = 1;

      /* ypos = py - SCREEN_BORDER_HEIGHT; */
      ypos = am2_ypos + (py - old_py);
      old_py = py;

      if (ypos > am2_yhigh)
        ypos = am2_yhigh;
      else if (ypos < am2_ylow)
        ypos = am2_ylow;

      am2_ypos = (short)ypos;

      if (am2_mode & MODE_MOVEIRQ)
        {
          am2_interrupted = 1;
          am2_status |= STATUS_MOVEIRQ;
        }
    }

  if (px >= 0 && px <= WINDOW_WIDTH && py >= 0 && py <= WINDOW_HEIGHT)
    {
      if (old_button)
        am2_status |= STATUS_OLD_BUTTON;
      else
        am2_status &= ~STATUS_OLD_BUTTON;

      if (state & GDK_BUTTON1_MASK)
        {
          am2_status |= STATUS_BUTTON;
          old_button = 1;

          if (am2_mode & MODE_BUTTONIRQ)
            {
              am2_interrupted = 1;
              am2_status |= STATUS_BUTTONIRQ;
            }
        }
      else
        {
          old_button = 0;
          am2_status &= ~STATUS_BUTTON;
        }
    }

  if (xy_changed)
    {
      /*
      printf("%d %d : [%d %d] [%d %d] : %d %d\n",
             px, py, am2_xlow, am2_xhigh, am2_ylow, am2_yhigh, am2_xpos, am2_ypos);
      */

      am2_status |= STATUS_XY_CHANGED;
    }
  else
    {
      am2_status &= ~STATUS_XY_CHANGED;
    }

  if (am2_mode & MODE_TIMEDIRQ)
    {
      am2_interrupted = 1;
      am2_status |= STATUS_TIMEDIRQ;
    }

  if (am2_interrupted)
    *(cpu.irq) |= F_IRQ;
}

void  am2_info (unsigned char *  bits)
{
  *bits = SLOT_CALL | SLOT_TIME;
}

void  am2_init (unsigned short  slot,
                cpu_state_t *   cpup,
                r_func_t *      rtab,
                w_func_t *      wtab)
{
  am2_slot    = slot;
  cpu         = *cpup;
  read_table  = rtab;
  write_table = wtab;
}
