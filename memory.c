/*
  memory.c
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

#include "apple.h"

#include <stdio.h>   /* fopen(), fread(), printf() */
#include <stdlib.h>  /* malloc() */

#include "cpu.h"     /* raster_row, raster_dot */
#include "lc.h"      /* language_card_init() */
#include "slots.h"   /* NUM_CARDSLOTS */
#include "switches.h"
#include "memory.h"
#include "video.h"

unsigned char  r_c0 (unsigned short  r_addr);

void           w_c0 (unsigned short  w_addr,
                     unsigned char   w_byte);

/* Every pointer in memory points to a 256 byte 'page' of memory */
unsigned char * aux_memory [256];
unsigned char * std_memory [256];

unsigned char ** r_memory [256];
unsigned char ** w_memory [256];

unsigned char * aux_bank0 = 0;

r_func_t r_page [256];
w_func_t w_page [256];

unsigned char rom_memory [0x3F00];


unsigned char * text_auxmem;
unsigned char * hgr_auxmem;

/*********/

unsigned short video_table[24] =
{
  0x000, 0x080, 0x100, 0x180, 0x200, 0x280, 0x300, 0x380,
  0x028, 0x0a8, 0x128, 0x1a8, 0x228, 0x2a8, 0x328, 0x3a8,
  0x050, 0x0d0, 0x150, 0x1d0, 0x250, 0x2d0, 0x350, 0x3d0
};

/*********/

unsigned char aux_text  = 0;
unsigned char aux_hires = 0;

/*********/

unsigned char  speaker_state = 0;
unsigned short speaker_pos = 0;
unsigned char  speaker_buffer[65536];

/*********/

unsigned char r_std (unsigned short r_addr)
{
  /*
  if (r_addr >= 0x3eae && r_addr < 0x3eb2)
    printf("read from $%4.4x\n", r_addr);
  */
  return (*(r_memory[r_addr / 256]))[r_addr & 0xFF];
}

unsigned char r_rom (unsigned short r_addr)
{
  return rom_memory[r_addr - 0xC100];
}

unsigned char r_nop (unsigned short r_addr)
{
  return 0;
}

extern int trace;

void  w_std (unsigned short  w_addr,
             unsigned char   w_byte)
{
#if 0
  if (trace)
    printf("std Write $%4.4X - $%2.2X\n", w_addr, w_byte);
#endif

  (*(w_memory[w_addr / 256]))[w_addr & 0xFF] = w_byte;
}

void  w_nop (unsigned short  w_addr,
             unsigned char   w_byte)
{
}

unsigned char  r_slot_c8 (unsigned short  r_addr)
{
  int            slot_idx;
  unsigned char  result;

  result = 0;

  /* only one card should have it's c8 rom switched in, and therefore the */
  /* result should make sense... */

  for (slot_idx = 1; slot_idx < NUM_CARDSLOTS; slot_idx++)
    {
      if (slot[slot_idx].read)
        result |= slot[slot_idx].read(r_addr);
    }

  return result;
}

#if 0
unsigned char  r_cf (unsigned short  r_addr)
{
  if (0xCFFF == r_addr)
    {
      if (switch_intc8rom)
        {
          int  page_idx;

          switch_intc8rom = 0;

          if (switch_slotcxrom)
            {
              for (page_idx = 0xC8; page_idx < 0xD0; page_idx++)
                {
                  r_page[page_idx] = r_slot_c8;
                  w_page[page_idx] = w_nop;
                }

              return rom_memory[r_addr - 0xC100];
            }
        }

      /* tell all the cards to switch out their c8 roms */
    }

  if (switch_intc8rom)
    return rom_memory[r_addr - 0xC100];

  return r_slot_c8(r_addr);
}

void  w_cf (unsigned short  w_addr,
            unsigned char   w_byte)
{
  if (0xCFFF == w_addr)
    {
      if (switch_intc8rom)
        {
          int  page_idx;

          switch_intc8rom = 0;

          if (switch_slotcxrom)
            {
              for (page_idx = 0xC8; page_idx < 0xD0; page_idx++)
                {
                  r_page[page_idx] = r_slot_c8;
                  w_page[page_idx] = w_nop;
                }
            }
        }

      /* tell all the cards to switch out their c8 roms */
    }
}
#endif /* 0 */

unsigned char  r_c3 (unsigned short  r_addr)
{
  if (0 == switch_intc8rom)
    {
      int  page_idx;

      for (page_idx = 0xC8; page_idx < 0xD0; page_idx++)
        {
          r_page[page_idx] = r_rom;
          w_page[page_idx] = w_nop;
        }

      switch_intc8rom = 1;
    }

  return rom_memory[r_addr - 0xC100];
}

void  w_c3 (unsigned short  w_addr,
            unsigned char   w_byte)
{
  if (0 == switch_intc8rom)
    {
      int  page_idx;

      for (page_idx = 0xC8; page_idx < 0xD0; page_idx++)
        {
          r_page[page_idx] = r_rom;
          w_page[page_idx] = w_nop;
        }

      switch_intc8rom = 1;
    }
}

unsigned char  r_c0 (unsigned short  r_addr)
{
  switch (r_addr)
    {
    case 0xC000:
    case 0xC001:
    case 0xC002:
    case 0xC003:
    case 0xC004:
    case 0xC005:
    case 0xC006:
    case 0xC007:
    case 0xC008:
    case 0xC009:
    case 0xC00A:
    case 0xC00B:
    case 0xC00C:
    case 0xC00D:
    case 0xC00E:
    case 0xC00F:
      /*
      if ((kb_byte & 0x7F) == 0x30)
        trace = 1;
      if (trace)
        printf("Read $%4.4X => $%2.2X\n", r_addr, kb_byte);
      */
      return kb_byte;

    case 0xC010:
      kb_byte &= 0x7F;
      /*
      if (trace)
        printf("Read $%4.4X => $%2.2X\n", r_addr, key_pressed);
      */
      return key_pressed;

    case 0xC011:
      if (language_card_bank_status()) return 0x80;
      return 0;

    case 0xC012:
      if (language_card_read_status()) return 0x80;
      return 0;

    case 0xC013:
      if (switch_auxread) return 0x80;
      return 0;

    case 0xC014:
      if (switch_auxwrite) return 0x80;
      return 0;

    case 0xC015:
      if (!switch_slotcxrom) return 0x80;
      return 0;

    case 0xC016:
      if (switch_auxzp) return 0x80;
      return 0;

    case 0xC017:
      if (switch_slotc3rom) return 0x80;
      return 0;

    case 0xC018:
      if (switch_80store) return 0x80;
      return 0;

    case 0xC019:
      return apple_vbl;

    case 0xC01A:
      if (!switch_graphics) return 0x80;
      return 0;

    case 0xC01B:
      if (switch_mixed) return 0x80;
      return 0;

    case 0xC01C:
      if (switch_page2)	return 0x80;
      return 0;

    case 0xC01D:
      if (switch_hires)	return 0x80;
      return 0;

    case 0xC01E:
      if (switch_alt_cset) return 0x80;
      return 0;

    case 0xC01F:
      if (switch_80col)	return 0x80;
      return 0;

    case 0xC030: case 0xC031: case 0xC032: case 0xC033:
    case 0xC034: case 0xC035: case 0xC036: case 0xC037:
    case 0xC038: case 0xC039: case 0xC03A: case 0xC03B:
    case 0xC03C: case 0xC03D: case 0xC03E: case 0xC03F:
      if (speaker_state)
        speaker_state = 0;
      else
        speaker_state = 0xFF;
      break;

    case 0xC050:
      switch_graphics = 1;
      set_video_mode();
      break;

    case 0xC051:
      switch_graphics = 0;
      set_video_mode();
      break;

    case 0xC052:
      switch_mixed = 0;
      set_video_mode();
      break;

    case 0xC053:
      switch_mixed = 1;
      set_video_mode();
      break;

    case 0xC054:
      if (switch_page2)
        {
          /* printf("switch_page2 off\n"); */
          switch_page2 = 0;
          switch_page2_off();
          if (!switch_80store)
            set_video_mode();
        }
      break;

    case 0xC055:
      if (!switch_page2)
        {
          /* printf("switch_page2 on\n"); */
          switch_page2 = 1;
          switch_page2_on();
          if (!switch_80store)
            set_video_mode();
        }
      break;

    case 0xC056:
      if (switch_hires)
        {
          switch_hires = 0;
          switch_hires_off();
          set_video_mode();
        }
      break;

    case 0xC057:
      if (!switch_hires)
        {
          switch_hires = 1;
          switch_hires_on();
          set_video_mode();
        }
      break;

    case 0xC05E:
      switch_dblhires = 1;
      set_video_mode();
      break;

    case 0xC05F:
      if (switch_ioudis)
        {
          switch_dblhires = 0;
          set_video_mode();
        }
      break;

    case 0xC061:
      // printf("0x%4.4x : O-Apple?\n", emPC);
      // if (emPC == 0x7326) trace = 1;
      return open_apple_pressed;

    case 0xC062:
      // printf("0x%4.4x : S-Apple?\n", emPC);
      return solid_apple_pressed;

#if 1
    case 0xC070:
      {
        unsigned char video_byte;
        int video_addr;
        int video_row;
        int video_dot;

        if (apple_vbl)
          {
            video_row = raster_row - SCREEN_BORDER_HEIGHT;
            if (raster_dot < SCREEN_BORDER_WIDTH || raster_dot >= (SCREEN_BORDER_WIDTH + SCREEN_WIDTH))
              video_dot = SCREEN_WIDTH - 1;
            else
              video_dot = raster_dot - SCREEN_BORDER_WIDTH;
          }
        else
          {
            video_row = SCREEN_HEIGHT - 1;
            video_dot = SCREEN_WIDTH - 1;
          }

        video_dot /= 14;

        if (!switch_graphics || !switch_hires)
          {
            video_addr = video_table[video_row / 8] + video_dot + 0x400;
            if (switch_page2 && !switch_80store) video_addr += 0x400;
          }
        else if (switch_mixed && video_row >= 192)
          {
            video_addr = video_table[video_row / 8] + video_dot + 0x400;
            if (switch_page2 && !switch_80store) video_addr += 0x400;
          }
        else
          {
            video_addr = video_table[video_row / 8] + ((video_row % 8) * 0x400) + video_dot + 0x2000;
            if (switch_page2 && !switch_80store) video_addr += 0x2000;
          }

        video_byte = READ(video_addr);
        /* printf("c070: ($%4.4x) = $%2.2x\n", video_addr, video_byte); */
        return video_byte;
      }
#endif

    case 0xC07E:
      if (!switch_ioudis) return 0x80;
      break;

    case 0xC07F:
      if (switch_dblhires) return 0x80;
      break;

    case 0xC080: case 0xC081: case 0xC082: case 0xC083:
    case 0xC084: case 0xC085: case 0xC086: case 0xC087:
    case 0xC088: case 0xC089: case 0xC08A: case 0xC08B:
    case 0xC08C: case 0xC08D: case 0xC08E: case 0xC08F:
      language_card_switch(0, r_addr & 0xF);
      break;

    default:
      if (r_addr >= 0xC090)
        {
          int  idx;

          idx = (r_addr & 0x0070) >> 4;
          if (slot[idx].read)
            return slot[idx].read(r_addr);
        }
      break;
    }

  return 0;
}

void w_c0 (unsigned short w_addr,
           unsigned char  w_byte)
{
  switch (w_addr)
    {
    case 0xC000:
      if (switch_80store)
        {
          /* printf("switch_80store off\n"); */
          switch_80store = 0;
          switch_80store_off();
        }
      break;

    case 0xC001:
      if (!switch_80store)
        {
          /* printf("switch_80store on\n"); */
          switch_80store = 1;
          switch_80store_on();
        }
      break;

    case 0xC002:
      if (switch_auxread)
        {
          /* printf("switch_auxread off\n"); */
          switch_auxread = 0;
          auxread_off();
        }
      break;

    case 0xC003:
      if (!switch_auxread)
        {
          /* printf("switch_auxread on\n"); */
          switch_auxread = 1;
          auxread_on();
        }
      break;

    case 0xC004:
      if (switch_auxwrite)
        {
          /* printf("switch_auxwrite off\n"); */
          switch_auxwrite = 0;
          auxwrite_off();
        }
      break;

    case 0xC005:
      if (!switch_auxwrite)
        {
          /* printf("switch_auxwrite on\n"); */
          switch_auxwrite = 1;
          auxwrite_on();
        }
      break;

    case 0xC006:
      slotcxrom_on();
      break;

    case 0xC007:
      slotcxrom_off();
      break;

    case 0xC008:
      auxzp_off();
      break;

    case 0xC009:
      auxzp_on();
      break;

    case 0xC00A:
      slotc3rom_off();
      break;

    case 0xC00B:
      slotc3rom_on();
      break;

    case 0xC00C:
      /* printf("switch_80col off\n"); */
      switch_80col = 0;
      set_video_mode();
      break;

    case 0xC00D:
      /* printf("switch_80col on\n"); */
      switch_80col = 1;
      set_video_mode();
      break;

    case 0xC00E:
      switch_alt_cset = 0;
      break;

    case 0xC00F:
      switch_alt_cset = 1;
      break;

    case 0xC010:
      kb_byte &= 0x7F;
      break;

    case 0xC030: case 0xC031: case 0xC032: case 0xC033:
    case 0xC034: case 0xC035: case 0xC036: case 0xC037:
    case 0xC038: case 0xC039: case 0xC03A: case 0xC03B:
    case 0xC03C: case 0xC03D: case 0xC03E: case 0xC03F:
      /* printf("Write to $%4.4X\n", w_addr); */

    case 0xC050:
      switch_graphics = 1;
      set_video_mode();
      break;

    case 0xC051:
      switch_graphics = 0;
      set_video_mode();
      break;

    case 0xC052:
      switch_mixed = 0;
      set_video_mode();
      break;

    case 0xC053:
      switch_mixed = 1;
      set_video_mode();
      break;

    case 0xC054:
      if (switch_page2)
        {
          /* printf("switch_page2 off\n"); */
          switch_page2 = 0;
          switch_page2_off();
          if (!switch_80store)
            set_video_mode();
        }
      break;

    case 0xC055:
      if (!switch_page2)
        {
          /* printf("switch_page2 on\n"); */
          switch_page2 = 1;
          switch_page2_on();
          if (!switch_80store)
            set_video_mode();
        }
      break;

    case 0xC056:
      if (switch_hires)
        {
          switch_hires = 0;
          switch_hires_off();
          set_video_mode();
        }
      break;

    case 0xC057:
      if (!switch_hires)
        {
          switch_hires = 1;
          switch_hires_on();
          set_video_mode();
        }
      break;

    case 0xC05E:
      switch_dblhires = 1;
      set_video_mode();
      break;

    case 0xC05F:
      if (switch_ioudis)
        {
          switch_dblhires = 0;
          set_video_mode();
        }
      break;

    case 0xC07E:
      switch_ioudis = 1;
      break;

    case 0xC07F:
      switch_ioudis = 0;
      break;

    case 0xC080: case 0xC081: case 0xC082: case 0xC083:
    case 0xC084: case 0xC085: case 0xC086: case 0xC087:
    case 0xC088: case 0xC089: case 0xC08A: case 0xC08B:
    case 0xC08C: case 0xC08D: case 0xC08E: case 0xC08F:
      language_card_switch(1, w_addr & 0xF);
      break;

    default:
      /* if (w_addr >= 0xC090) */
        {
          int  idx;

          idx = (w_addr & 0x0070) >> 4;
          if (slot[idx].write)
            slot[idx].write(w_addr, w_byte);
          if (aux_slot.write)
            aux_slot.write(w_addr, w_byte);
        }
      break;
    }
}

void slotcxrom_on (void)
{
  int page_idx;

  if (switch_slotcxrom)
    return;

  switch_slotcxrom = 1;

  r_page[0xC1] = slot[1].read;
  w_page[0xC1] = slot[1].write;

  r_page[0xC2] = slot[2].read;
  w_page[0xC2] = slot[2].write;

  if (switch_slotc3rom)
    {
      r_page[0xC3] = slot[3].read;
      w_page[0xC3] = slot[3].write;
    }
  else
    {
      r_page[0xC3] = r_c3;
      w_page[0xC3] = w_c3;
    }

  r_page[0xC4] = slot[4].read;
  w_page[0xC4] = slot[4].write;

  r_page[0xC5] = slot[5].read;
  w_page[0xC5] = slot[5].write;

  r_page[0xC6] = slot[6].read;
  w_page[0xC6] = slot[6].write;

  r_page[0xC7] = slot[7].read;
  w_page[0xC7] = slot[7].write;

  if (switch_intc8rom)
    {
      for (page_idx = 0xC8; page_idx < 0xD0; page_idx++)
        {
          r_page[page_idx] = r_rom;
          w_page[page_idx] = w_nop;
        }
    }
  else
    {
      for (page_idx = 0xC8; page_idx < 0xD0; page_idx++)
        {
          r_page[page_idx] = r_slot_c8;
          w_page[page_idx] = w_nop;
        }
    }
}

void slotcxrom_off (void)
{
  int page_idx;

  if (0 == switch_slotcxrom)
    return;

  switch_slotcxrom = 0;

  for (page_idx = 0xC1; page_idx < 0xD0; page_idx++)
    {
      r_page[page_idx] = r_rom;
      w_page[page_idx] = w_nop;
    }
}

void slotc3rom_on (void)
{
  if (switch_slotc3rom)
    return;

  switch_slotc3rom = 1;

  if (switch_slotcxrom)
    {
      r_page[0xC3] = slot[SLOT_3].read;
      w_page[0xC3] = slot[SLOT_3].write;
    }
}

void slotc3rom_off (void)
{
  if (0 == switch_slotc3rom)
    return;

  switch_slotc3rom = 0;

  if (switch_slotcxrom)
    {
      r_page[0xC3] = r_c3;
      w_page[0xC3] = w_c3;
    }
}

void auxzp_on (void)
{
  int page_idx;

  if (switch_auxzp)
    return;

  switch_auxzp = 1;

  r_memory[0] = aux_memory;
  w_memory[0] = aux_memory;

  r_memory[1] = aux_memory + 1;
  w_memory[1] = aux_memory + 1;

  for (page_idx = 0xC0; page_idx < 0x100; page_idx++)
    {
      r_memory[page_idx] = aux_memory + page_idx;
      w_memory[page_idx] = aux_memory + page_idx;
    }
}

void auxzp_off (void)
{
  int page_idx;

  if (!switch_auxzp)
    return;

  switch_auxzp = 0;

  r_memory[0] = std_memory;
  w_memory[0] = std_memory;

  r_memory[1] = std_memory + 1;
  w_memory[1] = std_memory + 1;

  for (page_idx = 0xC0; page_idx < 0x100; page_idx++)
    {
      r_memory[page_idx] = std_memory + page_idx;
      w_memory[page_idx] = std_memory + page_idx;
    }
}

void auxread_on (void)
{
  int page_idx;

  r_memory[2] = aux_memory + 2;
  r_memory[3] = aux_memory + 3;

  for (page_idx = 0x8; page_idx < 0x20; page_idx++)
    r_memory[page_idx] = aux_memory + page_idx;

  for (page_idx = 0x40; page_idx < 0xC0; page_idx++)
    r_memory[page_idx] = aux_memory + page_idx;

  if (switch_80store)
    {
      if (switch_hires)
        return;
    }
  else
    {
      r_memory[4] = aux_memory + 4;
      r_memory[5] = aux_memory + 5;
      r_memory[6] = aux_memory + 6;
      r_memory[7] = aux_memory + 7;
    }

  for (page_idx = 0x20; page_idx < 0x40; page_idx++)
    r_memory[page_idx] = aux_memory + page_idx;
}

void auxread_off (void)
{
  int page_idx;

  r_memory[2] = std_memory + 2;
  r_memory[3] = std_memory + 3;

  for (page_idx = 0x8; page_idx < 0x20; page_idx++)
    r_memory[page_idx] = std_memory + page_idx;

  for (page_idx = 0x40; page_idx < 0xC0; page_idx++)
    r_memory[page_idx] = std_memory + page_idx;

  if (switch_80store)
    {
      if (switch_hires)
        return;
    }
  else
    {
      r_memory[4] = std_memory + 4;
      r_memory[5] = std_memory + 5;
      r_memory[6] = std_memory + 6;
      r_memory[7] = std_memory + 7;
    }

  for (page_idx = 0x20; page_idx < 0x40; page_idx++)
    r_memory[page_idx] = std_memory + page_idx;
}

void auxwrite_on (void)
{
  int page_idx;

  w_memory[2] = aux_memory + 2;
  w_memory[3] = aux_memory + 3;

  for (page_idx = 0x8; page_idx < 0x20; page_idx++)
    w_memory[page_idx] = aux_memory + page_idx;

  for (page_idx = 0x40; page_idx < 0xC0; page_idx++)
    w_memory[page_idx] = aux_memory + page_idx;

  if (switch_80store)
    {
      if (switch_hires)
        return;
    }
  else
    {
      aux_text = 1;
      w_memory[4] = aux_memory + 4;
      w_memory[5] = aux_memory + 5;
      w_memory[6] = aux_memory + 6;
      w_memory[7] = aux_memory + 7;
    }

  aux_hires = 1;
  for (page_idx = 0x20; page_idx < 0x40; page_idx++)
    w_memory[page_idx] = aux_memory + page_idx;
}

void auxwrite_off (void)
{
  int page_idx;

  w_memory[2] = std_memory + 2;
  w_memory[3] = std_memory + 3;

  for (page_idx = 0x8; page_idx < 0x20; page_idx++)
    w_memory[page_idx] = std_memory + page_idx;

  for (page_idx = 0x40; page_idx < 0xC0; page_idx++)
    w_memory[page_idx] = std_memory + page_idx;

  if (switch_80store)
    {
      if (switch_hires)
        return;
    }
  else
    {
      aux_text = 0;
      w_memory[4] = std_memory + 4;
      w_memory[5] = std_memory + 5;
      w_memory[6] = std_memory + 6;
      w_memory[7] = std_memory + 7;
    }

  aux_hires = 0;
  for (page_idx = 0x20; page_idx < 0x40; page_idx++)
    w_memory[page_idx] = std_memory + page_idx;
}

void switch_80store_on (void)
{
  int page_idx;

  if (switch_page2)
    {
      r_memory[4] = aux_memory + 4;
      r_memory[5] = aux_memory + 5;
      r_memory[6] = aux_memory + 6;
      r_memory[7] = aux_memory + 7;

      aux_text = 1;
      w_memory[4] = aux_memory + 4;
      w_memory[5] = aux_memory + 5;
      w_memory[6] = aux_memory + 6;
      w_memory[7] = aux_memory + 7;

      if (switch_hires)
        {
          aux_hires = 1;
          for (page_idx = 0x20; page_idx < 0x40; page_idx++)
            {
              r_memory[page_idx] = aux_memory + page_idx;
              w_memory[page_idx] = aux_memory + page_idx;
            }
        }
    }
  else
    {
      r_memory[4] = std_memory + 4;
      r_memory[5] = std_memory + 5;
      r_memory[6] = std_memory + 6;
      r_memory[7] = std_memory + 7;

      aux_text = 0;
      w_memory[4] = std_memory + 4;
      w_memory[5] = std_memory + 5;
      w_memory[6] = std_memory + 6;
      w_memory[7] = std_memory + 7;

      if (switch_hires)
        {
          aux_hires = 0;
          for (page_idx = 0x20; page_idx < 0x40; page_idx++)
            {
              r_memory[page_idx] = std_memory + page_idx;
              w_memory[page_idx] = std_memory + page_idx;
            }
        }
    }
}

void switch_80store_off (void)
{
  int page_idx;

  if (switch_auxread)
    {
      r_memory[4] = aux_memory + 4;
      r_memory[5] = aux_memory + 5;
      r_memory[6] = aux_memory + 6;
      r_memory[7] = aux_memory + 7;

      if (switch_hires)
        {
          for (page_idx = 0x20; page_idx < 0x40; page_idx++)
            r_memory[page_idx] = aux_memory + page_idx;
        }
    }
  else
    {
      r_memory[4] = std_memory + 4;
      r_memory[5] = std_memory + 5;
      r_memory[6] = std_memory + 6;
      r_memory[7] = std_memory + 7;

      if (switch_hires)
        {
          for (page_idx = 0x20; page_idx < 0x40; page_idx++)
            r_memory[page_idx] = std_memory + page_idx;
        }
    }

  if (switch_auxwrite)
    {
      aux_text = 1;
      w_memory[4] = aux_memory + 4;
      w_memory[5] = aux_memory + 5;
      w_memory[6] = aux_memory + 6;
      w_memory[7] = aux_memory + 7;

      if (switch_hires)
        {
          aux_hires = 1;
          for (page_idx = 0x20; page_idx < 0x40; page_idx++)
            w_memory[page_idx] = aux_memory + page_idx;
        }
    }
  else
    {
      aux_text = 0;
      w_memory[4] = std_memory + 4;
      w_memory[5] = std_memory + 5;
      w_memory[6] = std_memory + 6;
      w_memory[7] = std_memory + 7;

      if (switch_hires)
        {
          aux_hires = 0;
          for (page_idx = 0x20; page_idx < 0x40; page_idx++)
            w_memory[page_idx] = std_memory + page_idx;
        }
    }
}

void switch_page2_on (void)
{
  int page_idx;

  if (switch_80store)
    {
      r_memory[4] = aux_memory + 4;
      r_memory[5] = aux_memory + 5;
      r_memory[6] = aux_memory + 6;
      r_memory[7] = aux_memory + 7;

      aux_text = 1;
      w_memory[4] = aux_memory + 4;
      w_memory[5] = aux_memory + 5;
      w_memory[6] = aux_memory + 6;
      w_memory[7] = aux_memory + 7;

      if (switch_hires)
        {
          aux_hires = 1;
          for (page_idx = 0x20; page_idx < 0x40; page_idx++)
            {
              r_memory[page_idx] = aux_memory + page_idx;
              w_memory[page_idx] = aux_memory + page_idx;
            }
        }
    }
}

void switch_page2_off (void)
{
  int page_idx;

  if (switch_80store)
    {
      r_memory[4] = std_memory + 4;
      r_memory[5] = std_memory + 5;
      r_memory[6] = std_memory + 6;
      r_memory[7] = std_memory + 7;

      aux_text = 0;
      w_memory[4] = std_memory + 4;
      w_memory[5] = std_memory + 5;
      w_memory[6] = std_memory + 6;
      w_memory[7] = std_memory + 7;

      if (switch_hires)
        {
          aux_hires = 0;
          for (page_idx = 0x20; page_idx < 0x40; page_idx++)
            {
              r_memory[page_idx] = std_memory + page_idx;
              w_memory[page_idx] = std_memory + page_idx;
            }
        }
    }
}

void switch_hires_on (void)
{
  int page_idx;

  if (switch_80store)
    {
      if (switch_page2)
        {
          aux_hires = 1;
          for (page_idx = 0x20; page_idx < 0x40; page_idx++)
            {
              r_memory[page_idx] = aux_memory + page_idx;
              w_memory[page_idx] = aux_memory + page_idx;
            }
        }
      else
        {
          aux_hires = 0;
          for (page_idx = 0x20; page_idx < 0x40; page_idx++)
            {
              r_memory[page_idx] = std_memory + page_idx;
              w_memory[page_idx] = std_memory + page_idx;
            }
        }
    }
}

void switch_hires_off (void)
{
  int page_idx;

  if (switch_80store)
    {
      if (switch_auxread)
        {
          for (page_idx = 0x20; page_idx < 0x40; page_idx++)
            r_memory[page_idx] = aux_memory + page_idx;
        }
      else
        {
          for (page_idx = 0x20; page_idx < 0x40; page_idx++)
            r_memory[page_idx] = std_memory + page_idx;
        }

      if (switch_auxwrite)
        {
          aux_hires = 1;
          for (page_idx = 0x20; page_idx < 0x40; page_idx++)
            w_memory[page_idx] = aux_memory + page_idx;
        }
      else
        {
          aux_hires = 0;
          for (page_idx = 0x20; page_idx < 0x40; page_idx++)
            w_memory[page_idx] = std_memory + page_idx;
        }
    }
}

static void  init_rom (void)
{
  FILE *  romf;

  romf = fopen("cxrom.rom", "rb");
  if (0 == romf)
    {
      printf("Failed to open file 'cxrom.rom'\n");
      exit(1);
    }

  if (0x0F00 != fread(rom_memory, 1, 0x0F00, romf))
    {
      printf("Failed to read file 'cxrom.rom'\n");
      exit(1);
    }

  fclose(romf);

  romf = fopen("applesoft.rom", "rb");
  if (0 == romf)
    {
      printf("Failed to open file 'applesoft.rom'\n");
      exit(1);
    }

  if (0x3000 != fread(rom_memory + 0xF00, 1, 0x3000, romf))
    {
      printf("Failed to read file 'applesoft.rom'\n");
      exit(1);
    }

  fclose(romf);
}

/*********************************/

void set_text (void)
{
  w_page[4]  = text_p1_write;
  w_page[5]  = text_p1_write;
  w_page[6]  = text_p1_write;
  w_page[7]  = text_p1_write;
  w_page[8]  = text_p2_write;
  w_page[9]  = text_p2_write;
  w_page[10] = text_p2_write;
  w_page[11] = text_p2_write;
}

void set_hgr (void)
{
  int page_idx;

  for (page_idx = 0x20; page_idx < 0x40; ++page_idx)
    w_page[page_idx] = hgr_p1_write;

  for (page_idx = 0x40; page_idx < 0x60; ++page_idx)
    w_page[page_idx] = hgr_p2_write;
}

/*********************************/

void init_memory (void)
{
  unsigned int  page_idx;

  /* allocate rom space and read in rom files */
  init_rom();

  /* allocate base memory space */
  std_memory[0] = (unsigned char *)malloc(0x10000);
  if (0 == std_memory[0])
    {
      printf("Failed to allocate 64k main memory\n");
      exit(1);
    }

  for (page_idx = 0; page_idx < 0x100; page_idx++)
    {
      std_memory[page_idx] = std_memory[0] + (256 * page_idx);

      r_memory[page_idx] = std_memory + page_idx;
      w_memory[page_idx] = std_memory + page_idx;
    }

  /* init auxiliary slot card here to get auxiliary memory */
  if (aux_slot.init)
    {
      aux_slot.init(&(aux_memory[0]));
      aux_bank0 = aux_memory[0];
    }

  for (page_idx = 0; page_idx < 0xC0; page_idx++)
    {
      r_page[page_idx] = r_std;
      w_page[page_idx] = w_std;
    }

  r_page[0xC0] = r_c0;
  w_page[0xC0] = w_c0;

  /* 0xC1 to 0xCF starts out as slots */
  slotcxrom_on();

  /* this handles the space from 0xD0 to 0xDF */
  language_card_init();

  set_hgr();
  set_text();

  /*************/

  aux_text  = 0;
  aux_hires = 0;
}

