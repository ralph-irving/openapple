/*
  video.c
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

#include "apple.h"
#include "charset.h"
#include "cpu.h"       /* rasterizer */
#include "memory.h"
#include "switches.h"  /* switch_XXX */
#include "video.h"

/*********************************************/

#define UPDATE_HERE 0

#define LINES_PER_SCREEN 24
#define LINES_PER_GROUP  3
#define BYTES_PER_LINE   40
#define TEXT_BUFFER_SIZE 0x400

#define GROUPS_PER_SCREEN (LINES_PER_SCREEN / LINES_PER_GROUP)
#define GROUP_MEMORY_SIZE (TEXT_BUFFER_SIZE / GROUPS_PER_SCREEN)

#define GROUP_NUM(a)    ((a) / GROUP_MEMORY_SIZE)
#define GROUP_OFFSET(a) ((a) % GROUP_MEMORY_SIZE)
#define GROUP_LINE(a)   (GROUP_OFFSET(a) / BYTES_PER_LINE)

#define SCREEN_LINE(a)  ((GROUP_LINE(a) >= LINES_PER_GROUP) \
                         ? LINES_PER_SCREEN \
                         : (GROUP_NUM(a) + (GROUP_LINE(a) * GROUPS_PER_SCREEN)))

#define SCREEN_COL(a)   ((GROUP_LINE(a) >= LINES_PER_GROUP) \
                         ? BYTES_PER_LINE \
                         : (GROUP_OFFSET(a) % BYTES_PER_LINE))

int video_col  [TEXT_BUFFER_SIZE];
int video_line [TEXT_BUFFER_SIZE];

/*********************************************/

#define NUM_VIDEO_COLS 560
#define NUM_VIDEO_ROWS 192
#define NUM_VIDEO_BITS (NUM_VIDEO_ROWS * NUM_VIDEO_COLS)

unsigned char text40_page1_bits [NUM_VIDEO_BITS];
unsigned char text40_page2_bits [NUM_VIDEO_BITS];

unsigned char * text40_bits = text40_page1_bits;

int text40_page1_left [NUM_VIDEO_ROWS];
int text40_page2_left [NUM_VIDEO_ROWS];
int text40_page1_right [NUM_VIDEO_ROWS];
int text40_page2_right [NUM_VIDEO_ROWS];

int * text40_left = text40_page1_left;
int * text40_right = text40_page1_right;

unsigned char text80_page1_bits [NUM_VIDEO_BITS];
unsigned char text80_page2_bits [NUM_VIDEO_BITS];

unsigned char * text80_bits = text80_page1_bits;

int text80_page1_left [NUM_VIDEO_ROWS];
int text80_page2_left [NUM_VIDEO_ROWS];
int text80_page1_right [NUM_VIDEO_ROWS];
int text80_page2_right [NUM_VIDEO_ROWS];

int * text80_left = text80_page1_left;
int * text80_right = text80_page1_right;

unsigned char gr_page1_bits [NUM_VIDEO_BITS];
unsigned char gr_page2_bits [NUM_VIDEO_BITS];

unsigned char * gr_bits = gr_page1_bits;

int gr_page1_left [NUM_VIDEO_ROWS];
int gr_page2_left [NUM_VIDEO_ROWS];
int gr_page1_right [NUM_VIDEO_ROWS];
int gr_page2_right [NUM_VIDEO_ROWS];

int * gr_left = gr_page1_left;
int * gr_right = gr_page1_right;

unsigned char dgr_page1_bits [NUM_VIDEO_BITS];
unsigned char dgr_page2_bits [NUM_VIDEO_BITS];

unsigned char * dgr_bits = dgr_page1_bits;

int dgr_page1_left [NUM_VIDEO_ROWS];
int dgr_page2_left [NUM_VIDEO_ROWS];
int dgr_page1_right [NUM_VIDEO_ROWS];
int dgr_page2_right [NUM_VIDEO_ROWS];

int * dgr_left = dgr_page1_left;
int * dgr_right = dgr_page1_right;

unsigned char hgr_page1_bits [NUM_VIDEO_BITS];
unsigned char hgr_page2_bits [NUM_VIDEO_BITS];

unsigned char * hgr_bits = hgr_page1_bits;

int hgr_page1_left [NUM_VIDEO_ROWS];
int hgr_page2_left [NUM_VIDEO_ROWS];
int hgr_page1_right [NUM_VIDEO_ROWS];
int hgr_page2_right [NUM_VIDEO_ROWS];

int * hgr_left = hgr_page1_left;
int * hgr_right = hgr_page1_right;

unsigned char dhgr_page1_bits [NUM_VIDEO_BITS];
unsigned char dhgr_page2_bits [NUM_VIDEO_BITS];

unsigned char * dhgr_bits = dhgr_page1_bits;

int dhgr_page1_left [NUM_VIDEO_ROWS];
int dhgr_page2_left [NUM_VIDEO_ROWS];
int dhgr_page1_right [NUM_VIDEO_ROWS];
int dhgr_page2_right [NUM_VIDEO_ROWS];

int * dhgr_left = dhgr_page1_left;
int * dhgr_right = dhgr_page1_right;

unsigned char video_last [NUM_VIDEO_BITS];

GdkGC * mono_half_gc;
GdkGC * mono_line1_gc;
GdkGC * mono_line2_gc;

GdkGC * color_half_gc [16];
GdkGC * color_line1_gc [16];
GdkGC * color_line2_gc [16];

/* colors from IIgs technote #63 */

unsigned short apple_colors [16][3] =
{
  { 0x0000, 0x0000, 0x0000 }, /* 0 Black */
  { 0xDDDD, 0x0000, 0x3333 }, /* 1 Magenta / Deep Red */
  { 0x0000, 0x0000, 0x9999 }, /* 2 Dark Blue */
  { 0xDDDD, 0x2222, 0xDDDD }, /* 3 Purple */
  { 0x0000, 0x7777, 0x2222 }, /* 4 Dark Green */
  { 0x5555, 0x5555, 0x5555 }, /* 5 Dark Gray */
  { 0x2222, 0x2222, 0xFFFF }, /* 6 Medium Blue */
  { 0x6666, 0xAAAA, 0xFFFF }, /* 7 Light Blue */
  { 0x8888, 0x5555, 0x0000 }, /* 8 Brown */
  { 0xFFFF, 0x6666, 0x0000 }, /* 9 Orange */
  { 0xAAAA, 0xAAAA, 0xAAAA }, /* A Light Gray */
  { 0xFFFF, 0x9999, 0x8888 }, /* B Pink */
  { 0x1111, 0xDDDD, 0x0000 }, /* C Light Green */
  { 0xFFFF, 0xFFFF, 0x0000 }, /* D Yellow */
  { 0x4444, 0xFFFF, 0x9999 }, /* E Aqua */
  { 0xFFFF, 0xFFFF, 0xFFFF }, /* F White */
};

/* bit patterns mapping GR nibbles into the above colors */

unsigned short gr_patterns[16] =
{
  0x0000, 0x1111, 0x2222, 0x3333, 0x0444, 0x1555, 0x2666, 0x3777,
  0x0888, 0x1999, 0x2aaa, 0x3bbb, 0x0ccc, 0x1ddd, 0x2eee, 0x3fff
};

/*********************************************/

static void set_full_scan (void)
{
  int idx;

  if (option_use_gtk)
    {
      mono_line2_gc = mono_line1_gc;

      for (idx = 0; idx < 16; ++idx)
        color_line2_gc[idx] = color_line1_gc[idx];
    }
}

static void set_half_scan (void)
{
  int idx;

  if (option_use_gtk)
    {
      mono_line2_gc = mono_half_gc;

      for (idx = 0; idx < 16; ++idx)
        color_line2_gc[idx] = color_half_gc[idx];
    }
}

void set_scan (void)
{
  if (option_full_scan)
    set_full_scan();
  else
    set_half_scan();
}

void rebuild_rasterizer (void);

void reset_video (void)
{
  int idx;

  set_scan();

  for (idx = 0; idx < NUM_VIDEO_BITS; ++idx)
    video_last[idx] = 0x1;

  for (idx = 0; idx < NUM_VIDEO_ROWS; ++idx)
    {
      text40_page1_left[idx]  = 0;
      text40_page1_right[idx] = NUM_VIDEO_COLS;
      text40_page2_left[idx]  = 0;
      text40_page2_right[idx] = NUM_VIDEO_COLS;

      text80_page1_left[idx]  = 0;
      text80_page1_right[idx] = NUM_VIDEO_COLS;
      text80_page2_left[idx]  = 0;
      text80_page2_right[idx] = NUM_VIDEO_COLS;

      gr_page1_left[idx]  = 0;
      gr_page1_right[idx] = NUM_VIDEO_COLS;
      gr_page2_left[idx]  = 0;
      gr_page2_right[idx] = NUM_VIDEO_COLS;

      dgr_page1_left[idx]  = 0;
      dgr_page1_right[idx] = NUM_VIDEO_COLS;
      dgr_page2_left[idx]  = 0;
      dgr_page2_right[idx] = NUM_VIDEO_COLS;

      hgr_page1_left[idx]  = 0;
      hgr_page1_right[idx] = NUM_VIDEO_COLS;
      hgr_page2_left[idx]  = 0;
      hgr_page2_right[idx] = NUM_VIDEO_COLS;

      dhgr_page1_left[idx]  = 0;
      dhgr_page1_right[idx] = NUM_VIDEO_COLS;
      dhgr_page2_left[idx]  = 0;
      dhgr_page2_right[idx] = NUM_VIDEO_COLS;
    }

  rebuild_rasterizer();

  set_video_mode();
}

void init_video (void)
{
  int idx;

  init_character_bits();

  for (idx = 0; idx < NUM_VIDEO_BITS; ++idx)
    {
      video_last[idx] = 0x1;

      text40_page1_bits[idx] = text40_page2_bits[idx] = 0x0;
      text80_page1_bits[idx] = text80_page2_bits[idx] = 0x2;

      gr_page1_bits[idx] = gr_page2_bits[idx] = 0x4;
      dgr_page1_bits[idx] = dgr_page2_bits[idx] = 0x6;

      hgr_page1_bits[idx] = hgr_page2_bits[idx] = 0x8;
      dhgr_page1_bits[idx] = dhgr_page2_bits[idx] = 0xa;
    }

  for (idx = 0; idx < TEXT_BUFFER_SIZE; ++idx)
    {
      video_col[idx]  = SCREEN_COL(idx);
      video_line[idx] = SCREEN_LINE(idx);
    }

  if (option_use_gtk)
    {
      GdkColor my_color;
      GdkColormap * my_colormap;

      my_colormap = gdk_colormap_get_system();
      mono_line1_gc = gdk_gc_new(screen_da->window);
      mono_half_gc = gdk_gc_new(screen_da->window);

      my_color.red   = 0x6000;
      my_color.green = 0xFFFF;
      my_color.blue  = 0x0000;

      gdk_colormap_alloc_color(my_colormap, &my_color, 0, 1);
      gdk_gc_set_foreground(mono_line1_gc, &my_color);

      my_color.red   /= 2;
      my_color.green /= 2;
      my_color.blue  /= 2;

      gdk_colormap_alloc_color(my_colormap, &my_color, 0, 1);
      gdk_gc_set_foreground(mono_half_gc, &my_color);

      for (idx = 0; idx < 16; ++idx)
        {
          color_line1_gc[idx] = gdk_gc_new(screen_da->window);
          color_half_gc[idx] = gdk_gc_new(screen_da->window);

          my_color.red   = apple_colors[idx][0];
          my_color.green = apple_colors[idx][1];
          my_color.blue  = apple_colors[idx][2];

          gdk_colormap_alloc_color(my_colormap, &my_color, 0, 1);
          gdk_gc_set_foreground(color_line1_gc[idx], &my_color);

          my_color.red   /= 2;
          my_color.green /= 2;
          my_color.blue  /= 2;

          gdk_colormap_alloc_color(my_colormap, &my_color, 0, 1);
          gdk_gc_set_foreground(color_half_gc[idx], &my_color);
        }
    }

  set_scan();
}

/****************************/

void set_video_mode (void)
{
  int idx;

  if (switch_graphics)
    {
      if (switch_hires)
        {
          if (switch_80col && switch_dblhires)
            {
              if (switch_page2)
                {
                  dhgr_bits  = dhgr_page2_bits;
                  dhgr_left  = dhgr_page2_left;
                  dhgr_right = dhgr_page2_right;

                  if (option_color_monitor)
                    {
                      if (switch_mixed)
                        rasterizer = dhgr_color_mixed_rasterizer;
                      else
                        rasterizer = dhgr_color_rasterizer;
                    }
                  else
                    {
                      if (switch_mixed)
                        rasterizer = dhgr_mono_mixed_rasterizer;
                      else
                        rasterizer = dhgr_mono_rasterizer;
                    }

                  for (idx = 0; idx < NUM_VIDEO_ROWS; ++idx)
                    {
                      dhgr_page2_left[idx]  = 0;
                      dhgr_page2_right[idx] = NUM_VIDEO_COLS;
                    }
                }
              else
                {
                  dhgr_bits  = dhgr_page1_bits;
                  dhgr_left  = dhgr_page1_left;
                  dhgr_right = dhgr_page1_right;

                  if (option_color_monitor)
                    {
                      if (switch_mixed)
                        rasterizer = dhgr_color_mixed_rasterizer;
                      else
                        rasterizer = dhgr_color_rasterizer;
                    }
                  else
                    {
                      if (switch_mixed)
                        rasterizer = dhgr_mono_mixed_rasterizer;
                      else
                        rasterizer = dhgr_mono_rasterizer;
                    }

                  for (idx = 0; idx < NUM_VIDEO_ROWS; ++idx)
                    {
                      dhgr_page1_left[idx]  = 0;
                      dhgr_page1_right[idx] = NUM_VIDEO_COLS;
                    }
                }
            }
          else
            {
              if (switch_page2)
                {
                  hgr_bits  = hgr_page2_bits;
                  hgr_left  = hgr_page2_left;
                  hgr_right = hgr_page2_right;

                  if (option_color_monitor)
                    {
                      if (switch_mixed)
                        {
                          if (switch_80col)
                            rasterizer = hgr_color_mixed80_rasterizer;
                          else
                            rasterizer = hgr_color_mixed40_rasterizer;
                        }
                      else
                        rasterizer = hgr_color_rasterizer;
                    }
                  else
                    {
                      if (switch_mixed)
                        {
                          if (switch_80col)
                            rasterizer = hgr_mono_mixed80_rasterizer;
                          else
                            rasterizer = hgr_mono_mixed40_rasterizer;
                        }
                      else
                        rasterizer = hgr_mono_rasterizer;
                    }

                  for (idx = 0; idx < NUM_VIDEO_ROWS; ++idx)
                    {
                      hgr_page2_left[idx]  = 0;
                      hgr_page2_right[idx] = NUM_VIDEO_COLS;
                    }
                }
              else
                {
                  hgr_bits  = hgr_page1_bits;
                  hgr_left  = hgr_page1_left;
                  hgr_right = hgr_page1_right;

                  if (option_color_monitor)
                    {
                      if (switch_mixed)
                        {
                          if (switch_80col)
                            rasterizer = hgr_color_mixed80_rasterizer;
                          else
                            rasterizer = hgr_color_mixed40_rasterizer;
                        }
                      else
                        rasterizer = hgr_color_rasterizer;
                    }
                  else
                    {
                      if (switch_mixed)
                        {
                          if (switch_80col)
                            rasterizer = hgr_mono_mixed80_rasterizer;
                          else
                            rasterizer = hgr_mono_mixed40_rasterizer;
                        }
                      else
                        rasterizer = hgr_mono_rasterizer;
                    }

                  for (idx = 0; idx < NUM_VIDEO_ROWS; ++idx)
                    {
                      hgr_page1_left[idx]  = 0;
                      hgr_page1_right[idx] = NUM_VIDEO_COLS;
                    }
                }
            }
        }
      else
        {
          if (switch_80col && switch_dblhires)
            {
              if (switch_page2)
                {
                  dgr_bits = dgr_page2_bits;
                  dgr_left = dgr_page2_left;
                  dgr_right = dgr_page2_right;

                  if (option_color_monitor)
                    {
                      if (switch_mixed)
                        rasterizer = dgr_color_mixed_rasterizer;
                      else
                        rasterizer = dgr_color_rasterizer;
                    }
                  else
                    {
                      if (switch_mixed)
                        rasterizer = dgr_mono_mixed_rasterizer;
                      else
                        rasterizer = dgr_mono_rasterizer;
                    }

                  for (idx = 0; idx < NUM_VIDEO_ROWS; ++idx)
                    {
                      dgr_page2_left[idx]  = 0;
                      dgr_page2_right[idx] = NUM_VIDEO_COLS;
                    }
                }
              else
                {
                  dgr_bits  = dgr_page1_bits;
                  dgr_left  = dgr_page1_left;
                  dgr_right = dgr_page1_right;

                  if (option_color_monitor)
                    {
                      if (switch_mixed)
                        rasterizer = dgr_color_mixed_rasterizer;
                      else
                        rasterizer = dgr_color_rasterizer;
                    }
                  else
                    {
                      if (switch_mixed)
                        rasterizer = dgr_mono_mixed_rasterizer;
                      else
                        rasterizer = dgr_mono_rasterizer;
                    }

                  for (idx = 0; idx < NUM_VIDEO_ROWS; ++idx)
                    {
                      dgr_page1_left[idx]  = 0;
                      dgr_page1_right[idx] = NUM_VIDEO_COLS;
                    }
                }
            }
          else
            {
              if (switch_page2)
                {
                  gr_bits  = gr_page2_bits;
                  gr_left  = gr_page2_left;
                  gr_right = gr_page2_right;

                  if (switch_mixed)
                    {
                      if (option_color_monitor)
                        {
                          if (switch_80col)
                            rasterizer = gr_color_mixed80_rasterizer;
                          else
                            rasterizer = gr_color_mixed40_rasterizer;
                        }
                      else
                        {
                          if (switch_80col)
                            rasterizer = gr_mono_mixed80_rasterizer;
                          else
                            rasterizer = gr_mono_mixed40_rasterizer;
                        }
                    }
                  else
                    {
                      if (option_color_monitor)
                        rasterizer = gr_color_rasterizer;
                      else
                        rasterizer = gr_mono_rasterizer;
                    }

                  for (idx = 0; idx < NUM_VIDEO_ROWS; ++idx)
                    {
                      gr_page2_left[idx]  = 0;
                      gr_page2_right[idx] = NUM_VIDEO_COLS;
                    }
                }
              else
                {
                  gr_bits  = gr_page1_bits;
                  gr_left  = gr_page1_left;
                  gr_right = gr_page1_right;

                  if (switch_mixed)
                    {
                      if (option_color_monitor)
                        {
                          if (switch_80col)
                            rasterizer = gr_color_mixed80_rasterizer;
                          else
                            rasterizer = gr_color_mixed40_rasterizer;
                        }
                      else
                        {
                          if (switch_80col)
                            rasterizer = gr_mono_mixed80_rasterizer;
                          else
                            rasterizer = gr_mono_mixed40_rasterizer;
                        }
                    }
                  else
                    {
                      if (option_color_monitor)
                        rasterizer = gr_color_rasterizer;
                      else
                        rasterizer = gr_mono_rasterizer;
                    }

                  for (idx = 0; idx < NUM_VIDEO_ROWS; ++idx)
                    {
                      gr_page1_left[idx]  = 0;
                      gr_page1_right[idx] = NUM_VIDEO_COLS;
                    }
                }
            }
        }
    }
  else
    {
      if (switch_80col)
        {
          if (option_color_monitor)
            rasterizer = text80_color_rasterizer;
          else
            rasterizer = text80_mono_rasterizer;

          if (switch_page2)
            {
              text80_bits = text80_page2_bits;
              text80_left = text80_page2_left;
              text80_right = text80_page2_right;

              for (idx = 0; idx < NUM_VIDEO_ROWS; ++idx)
                {
                  text80_page2_left[idx]  = 0;
                  text80_page2_right[idx] = NUM_VIDEO_COLS;
                }
            }
          else
            {
              text80_bits = text80_page1_bits;
              text80_left = text80_page1_left;
              text80_right = text80_page1_right;

              for (idx = 0; idx < NUM_VIDEO_ROWS; ++idx)
                {
                  text80_page1_left[idx]  = 0;
                  text80_page1_right[idx] = NUM_VIDEO_COLS;
                }
            }
        }
      else
        {
          if (option_color_monitor)
            rasterizer = text40_color_rasterizer;
          else
            rasterizer = text40_mono_rasterizer;

          if (switch_page2)
            {
              text40_bits  = text40_page2_bits;
              text40_left  = text40_page2_left;
              text40_right = text40_page2_right;

              for (idx = 0; idx < NUM_VIDEO_ROWS; ++idx)
                {
                  text40_page2_left[idx]  = 0;
                  text40_page2_right[idx] = NUM_VIDEO_COLS;
                }
            }
          else
            {
              text40_bits  = text40_page1_bits;
              text40_left  = text40_page1_left;
              text40_right = text40_page1_right;

              for (idx = 0; idx < NUM_VIDEO_ROWS; ++idx)
                {
                  text40_page1_left[idx]  = 0;
                  text40_page1_right[idx] = NUM_VIDEO_COLS;
                }
            }
        }
    }
}

/**************************************/

void draw_text (unsigned short  w_addr,
                unsigned char   w_byte,
                unsigned char * text40_page_bits,
                unsigned char * text80_page_bits,
                unsigned char * gr_page_bits,
                unsigned char * dgr_page_bits)
{
  int line, col;
  int y_idx, x_idx;

  unsigned char gr_color;

  unsigned short gr_pattern;
  unsigned short gr_pattern_1;
  unsigned short gr_pattern_2;

  unsigned short dgr_pattern;
  unsigned short dgr_pattern_1;
  unsigned short dgr_pattern_2;

  unsigned char * gr_ptr;
  unsigned char * dgr_ptr;
  unsigned char * bits_ptr;
  unsigned char * text80_ptr;
  unsigned short * text40_ptr;

  /* writes to screen holes don't draw anything */
  if ((w_addr & 0x0078) == 0x0078) return;

  w_addr &= 0x3ff;

  col  = video_col[w_addr];
  line = CHAR_HEIGHT * video_line[w_addr];

  bits_ptr = &(character_bits[w_byte * CHAR_HEIGHT]);

  gr_pattern_1 = gr_patterns[w_byte & 0xF];
  gr_pattern_2 = gr_patterns[(w_byte >> 4) & 0xF];

  dgr_pattern_1 = gr_patterns[w_byte & 0xF];
  dgr_pattern_2 = gr_patterns[(w_byte >> 4) & 0xF];

  text40_ptr = ((unsigned short *)text40_page_bits) + (280 * line) + (7 * col);
  text80_ptr = text80_page_bits + (560 * line) + (14 * col);

  gr_ptr = gr_page_bits + (560 * line) + (14 * col);
  dgr_ptr = dgr_page_bits + (560 * line) + (14 * col);

  if (!aux_text)
    {
      text80_ptr += 7;
      dgr_ptr += 7;
    }

  x_idx = 14 * col;
  if (aux_text)
    {
      if (text80_left[line + 0] > x_idx) text80_left[line + 0] = x_idx;
      if (text80_left[line + 1] > x_idx) text80_left[line + 1] = x_idx;
      if (text80_left[line + 2] > x_idx) text80_left[line + 2] = x_idx;
      if (text80_left[line + 3] > x_idx) text80_left[line + 3] = x_idx;
      if (text80_left[line + 4] > x_idx) text80_left[line + 4] = x_idx;
      if (text80_left[line + 5] > x_idx) text80_left[line + 5] = x_idx;
      if (text80_left[line + 6] > x_idx) text80_left[line + 6] = x_idx;
      if (text80_left[line + 7] > x_idx) text80_left[line + 7] = x_idx;

      if (dgr_left[line + 0] > x_idx) dgr_left[line + 0] = x_idx;
      if (dgr_left[line + 1] > x_idx) dgr_left[line + 1] = x_idx;
      if (dgr_left[line + 2] > x_idx) dgr_left[line + 2] = x_idx;
      if (dgr_left[line + 3] > x_idx) dgr_left[line + 3] = x_idx;
      if (dgr_left[line + 4] > x_idx) dgr_left[line + 4] = x_idx;
      if (dgr_left[line + 5] > x_idx) dgr_left[line + 5] = x_idx;
      if (dgr_left[line + 6] > x_idx) dgr_left[line + 6] = x_idx;
      if (dgr_left[line + 7] > x_idx) dgr_left[line + 7] = x_idx;
    }

  if (text40_left[line + 0] > x_idx) text40_left[line + 0] = x_idx;
  if (text40_left[line + 1] > x_idx) text40_left[line + 1] = x_idx;
  if (text40_left[line + 2] > x_idx) text40_left[line + 2] = x_idx;
  if (text40_left[line + 3] > x_idx) text40_left[line + 3] = x_idx;
  if (text40_left[line + 4] > x_idx) text40_left[line + 4] = x_idx;
  if (text40_left[line + 5] > x_idx) text40_left[line + 5] = x_idx;
  if (text40_left[line + 6] > x_idx) text40_left[line + 6] = x_idx;
  if (text40_left[line + 7] > x_idx) text40_left[line + 7] = x_idx;

  if (gr_left[line + 0] > x_idx) gr_left[line + 0] = x_idx;
  if (gr_left[line + 1] > x_idx) gr_left[line + 1] = x_idx;
  if (gr_left[line + 2] > x_idx) gr_left[line + 2] = x_idx;
  if (gr_left[line + 3] > x_idx) gr_left[line + 3] = x_idx;
  if (gr_left[line + 4] > x_idx) gr_left[line + 4] = x_idx;
  if (gr_left[line + 5] > x_idx) gr_left[line + 5] = x_idx;
  if (gr_left[line + 6] > x_idx) gr_left[line + 6] = x_idx;
  if (gr_left[line + 7] > x_idx) gr_left[line + 7] = x_idx;

  x_idx += 7;
  if (aux_text)
    {
      if (text80_right[line + 0] < x_idx) text80_right[line + 0] = x_idx;
      if (text80_right[line + 1] < x_idx) text80_right[line + 1] = x_idx;
      if (text80_right[line + 2] < x_idx) text80_right[line + 2] = x_idx;
      if (text80_right[line + 3] < x_idx) text80_right[line + 3] = x_idx;
      if (text80_right[line + 4] < x_idx) text80_right[line + 4] = x_idx;
      if (text80_right[line + 5] < x_idx) text80_right[line + 5] = x_idx;
      if (text80_right[line + 6] < x_idx) text80_right[line + 6] = x_idx;
      if (text80_right[line + 7] < x_idx) text80_right[line + 7] = x_idx;

      if (dgr_right[line + 0] < x_idx) dgr_right[line + 0] = x_idx;
      if (dgr_right[line + 1] < x_idx) dgr_right[line + 1] = x_idx;
      if (dgr_right[line + 2] < x_idx) dgr_right[line + 2] = x_idx;
      if (dgr_right[line + 3] < x_idx) dgr_right[line + 3] = x_idx;
      if (dgr_right[line + 4] < x_idx) dgr_right[line + 4] = x_idx;
      if (dgr_right[line + 5] < x_idx) dgr_right[line + 5] = x_idx;
      if (dgr_right[line + 6] < x_idx) dgr_right[line + 6] = x_idx;
      if (dgr_right[line + 7] < x_idx) dgr_right[line + 7] = x_idx;
    }
  else
    {
      if (text80_left[line + 0] > x_idx) text80_left[line + 0] = x_idx;
      if (text80_left[line + 1] > x_idx) text80_left[line + 1] = x_idx;
      if (text80_left[line + 2] > x_idx) text80_left[line + 2] = x_idx;
      if (text80_left[line + 3] > x_idx) text80_left[line + 3] = x_idx;
      if (text80_left[line + 4] > x_idx) text80_left[line + 4] = x_idx;
      if (text80_left[line + 5] > x_idx) text80_left[line + 5] = x_idx;
      if (text80_left[line + 6] > x_idx) text80_left[line + 6] = x_idx;
      if (text80_left[line + 7] > x_idx) text80_left[line + 7] = x_idx;

      if (dgr_left[line + 0] > x_idx) dgr_left[line + 0] = x_idx;
      if (dgr_left[line + 1] > x_idx) dgr_left[line + 1] = x_idx;
      if (dgr_left[line + 2] > x_idx) dgr_left[line + 2] = x_idx;
      if (dgr_left[line + 3] > x_idx) dgr_left[line + 3] = x_idx;
      if (dgr_left[line + 4] > x_idx) dgr_left[line + 4] = x_idx;
      if (dgr_left[line + 5] > x_idx) dgr_left[line + 5] = x_idx;
      if (dgr_left[line + 6] > x_idx) dgr_left[line + 6] = x_idx;
      if (dgr_left[line + 7] > x_idx) dgr_left[line + 7] = x_idx;
    }

  x_idx += 7;
  if (!aux_text)
    {
      if (text80_right[line + 0] < x_idx) text80_right[line + 0] = x_idx;
      if (text80_right[line + 1] < x_idx) text80_right[line + 1] = x_idx;
      if (text80_right[line + 2] < x_idx) text80_right[line + 2] = x_idx;
      if (text80_right[line + 3] < x_idx) text80_right[line + 3] = x_idx;
      if (text80_right[line + 4] < x_idx) text80_right[line + 4] = x_idx;
      if (text80_right[line + 5] < x_idx) text80_right[line + 5] = x_idx;
      if (text80_right[line + 6] < x_idx) text80_right[line + 6] = x_idx;
      if (text80_right[line + 7] < x_idx) text80_right[line + 7] = x_idx;

      if (dgr_right[line + 0] < x_idx) dgr_right[line + 0] = x_idx;
      if (dgr_right[line + 1] < x_idx) dgr_right[line + 1] = x_idx;
      if (dgr_right[line + 2] < x_idx) dgr_right[line + 2] = x_idx;
      if (dgr_right[line + 3] < x_idx) dgr_right[line + 3] = x_idx;
      if (dgr_right[line + 4] < x_idx) dgr_right[line + 4] = x_idx;
      if (dgr_right[line + 5] < x_idx) dgr_right[line + 5] = x_idx;
      if (dgr_right[line + 6] < x_idx) dgr_right[line + 6] = x_idx;
      if (dgr_right[line + 7] < x_idx) dgr_right[line + 7] = x_idx;
    }

  if (text40_right[line + 0] < x_idx) text40_right[line + 0] = x_idx;
  if (text40_right[line + 1] < x_idx) text40_right[line + 1] = x_idx;
  if (text40_right[line + 2] < x_idx) text40_right[line + 2] = x_idx;
  if (text40_right[line + 3] < x_idx) text40_right[line + 3] = x_idx;
  if (text40_right[line + 4] < x_idx) text40_right[line + 4] = x_idx;
  if (text40_right[line + 5] < x_idx) text40_right[line + 5] = x_idx;
  if (text40_right[line + 6] < x_idx) text40_right[line + 6] = x_idx;
  if (text40_right[line + 7] < x_idx) text40_right[line + 7] = x_idx;

  if (gr_right[line + 0] < x_idx) gr_right[line + 0] = x_idx;
  if (gr_right[line + 1] < x_idx) gr_right[line + 1] = x_idx;
  if (gr_right[line + 2] < x_idx) gr_right[line + 2] = x_idx;
  if (gr_right[line + 3] < x_idx) gr_right[line + 3] = x_idx;
  if (gr_right[line + 4] < x_idx) gr_right[line + 4] = x_idx;
  if (gr_right[line + 5] < x_idx) gr_right[line + 5] = x_idx;
  if (gr_right[line + 6] < x_idx) gr_right[line + 6] = x_idx;
  if (gr_right[line + 7] < x_idx) gr_right[line + 7] = x_idx;

  for (y_idx = 0; y_idx < 8; ++y_idx)
    {
      unsigned char bits = *bits_ptr++;

      if (y_idx < 4)
        {
          gr_color = (w_byte << 4) & 0xF0;
          gr_pattern = gr_pattern_1;
          dgr_pattern = dgr_pattern_1;
        }
      else
        {
          gr_color = w_byte & 0xF0;
          gr_pattern = gr_pattern_2;
          dgr_pattern = dgr_pattern_2;
        }

      for (x_idx = 0; x_idx < 7; ++x_idx)
        {
#if 0
          if (switch_alt_cset)
            {
#endif
              if (bits & 1)
                {
                  *text40_ptr++ = 0x0101;
                  *text80_ptr++ = 0x03;
                }
              else
                {
                  *text40_ptr++ = 0x0000;
                  *text80_ptr++ = 0x02;
                }
#if 0
            }
          else
            {
              if (bits & 1)
                {
                  *text40_ptr++ = 0x1111;
                  *text80_ptr++ = 0x13;
                }
              else
                {
                  *text40_ptr++ = 0x1010;
                  *text80_ptr++ = 0x12;
                }
            }
#endif
          bits >>= 1;

          if (gr_pattern & 1)
            *gr_ptr++ = gr_color | 0x5;
          else
            *gr_ptr++ = gr_color | 0x4;

          gr_pattern >>= 1;

          if (gr_pattern & 1)
            *gr_ptr++ = gr_color | 0x5;
          else
            *gr_ptr++ = gr_color | 0x4;

          gr_pattern >>= 1;

          if (dgr_pattern & 1)
            *dgr_ptr++ = gr_color | 0x7;
          else
            *dgr_ptr++ = gr_color | 0x6;

          dgr_pattern >>= 1;

        }

      text40_ptr += (280 - 7);
      text80_ptr += (560 - 7);

      gr_ptr += (560 - 14);
      dgr_ptr += (560 - 7);
    }
}

void draw_hires (unsigned short  w_addr,
                 unsigned char   w_byte,
                 unsigned char * hgr_page_bits,
                 unsigned char * dhgr_page_bits,
                 int *           hgr_page_left,
                 int *           hgr_page_right,
                 int *           dhgr_page_left,
                 int *           dhgr_page_right)
{
  int line, col;
  int x_idx;

  unsigned char bits;
  unsigned char * dhgr_ptr;
  unsigned char * hgr_ptr;

  /* writes to screen holes don't draw anything */
  if ((w_addr & 0x0078) == 0x0078)
    return;

#if 0
  if (w_addr >= 0x3eae && w_addr < 0x3eb2)
    {
      printf("$%4.4x : $%2.2x\n", w_addr, w_byte);
      /* return; */
    }
#endif

  col  = video_col[w_addr & 0x3ff];
  line = (CHAR_HEIGHT * video_line[w_addr & 0x3ff]) + ((w_addr & 0x1fff) / 0x400);

  hgr_ptr = hgr_page_bits + (560 * line) + (14 * col);
  dhgr_ptr = dhgr_page_bits + (560 * line) + (14 * col);

  if (!aux_hires) dhgr_ptr += 7;

  x_idx = 14 * col;
  if (hgr_page_left[line] > (x_idx - 4)) hgr_page_left[line] = x_idx - 4;
  if (aux_hires)
    {
      if (dhgr_page_left[line] > (x_idx - 4)) dhgr_page_left[line] = (x_idx - 4);
    }
  else
    {
      if (dhgr_page_left[line] > (x_idx + 3)) dhgr_page_left[line] = (x_idx + 3);
    }

  x_idx += 7;
  if (aux_hires)
    {
      if (dhgr_page_right[line] < (x_idx + 4)) dhgr_page_right[line] = (x_idx + 4);
    }
  else
    {
      if (dhgr_page_right[line] < (x_idx + 11)) dhgr_page_right[line] = (x_idx + 11);
    }

  x_idx += 12;
  if (hgr_page_right[line] < x_idx) hgr_page_right[line] = x_idx;

  /* handle the HGR half-dot shift */

  if (w_byte & 0x80)
    {
      if (0 == col)
        *hgr_ptr = 0x08;
      else
        {
        *hgr_ptr = *(hgr_ptr-1);
        }

      ++hgr_ptr;
    }
  else if (col < 39)
    {
      if (0x0c == (*(hgr_ptr+15) & 0x0c))
        {
          if (w_byte & 0x40)
            *(hgr_ptr+14) = 0x09;
          else
            *(hgr_ptr+14) = 0x08;
        }
      else
        {
          *(hgr_ptr+14) = *(hgr_ptr+15);
        }
    }

  bits = w_byte;

  /* plot the first 6 bits */

  if (w_byte & 0x80)
    {
      for (x_idx = 0; x_idx < 6; ++x_idx)
        {
          if (bits & 1)
            {
              *dhgr_ptr++ = 0x0b;
              *hgr_ptr++  = 0x0d;
              *hgr_ptr++  = 0x0d;
            }
          else
            {
              *dhgr_ptr++ = 0x0a;
              *hgr_ptr++  = 0x0c;
              *hgr_ptr++  = 0x0c;
            }

          bits >>= 1;
        }
    }
  else
    {
      for (x_idx = 0; x_idx < 6; ++x_idx)
        {
          if (bits & 1)
            {
              *dhgr_ptr++ = 0x0b;
              *hgr_ptr++  = 0x09;
              *hgr_ptr++  = 0x09;
            }
          else
            {
              *dhgr_ptr++ = 0x0a;
              *hgr_ptr++  = 0x08;
              *hgr_ptr++  = 0x08;
            }

          bits >>= 1;
        }
    }

  /* plot the 7th bit */

  if (w_byte & 0x80)
    {
      if (bits & 1)
        {
          *dhgr_ptr  = 0x0b;
          *hgr_ptr++ = 0x0d;

          if (col < 39)
            {
              if ((*(hgr_ptr+1) & 0x0c) == 0x0c)
                *hgr_ptr = 0x0d;
            }
        }
      else
        {
          *dhgr_ptr  = 0x0a;
          *hgr_ptr++ = 0x0c;

          if (col < 39)
            {
              if ((*(hgr_ptr+1) & 0x0c) == 0x0c)
                *hgr_ptr = 0x0c;
            }
        }
    }
  else
    {
      if (bits & 1)
        {
          *dhgr_ptr  = 0x0b;
          *hgr_ptr++ = 0x09;
          *hgr_ptr   = 0x09;
        }
      else
        {
          *dhgr_ptr  = 0x0a;
          *hgr_ptr++ = 0x08;
          *hgr_ptr   = 0x08;
        }
    }
}

/*
** write_func_t functions used to write to video memory
*/

void text_p1_write (unsigned short w_addr, unsigned char w_byte)
{
  (*(w_memory[w_addr / 256]))[w_addr & 0xFF] = w_byte;

  draw_text(w_addr,
            w_byte,
            text40_page1_bits,
            text80_page1_bits,
            gr_page1_bits,
            dgr_page1_bits);
}

void text_p2_write (unsigned short w_addr, unsigned char w_byte)
{
  (*(w_memory[w_addr / 256]))[w_addr & 0xFF] = w_byte;

  draw_text(w_addr,
            w_byte,
            text40_page2_bits,
            text80_page2_bits,
            gr_page2_bits,
            dgr_page2_bits);
}

void hgr_p1_write (unsigned short w_addr, unsigned char w_byte)
{
  (*(w_memory[w_addr / 256]))[w_addr & 0xFF] = w_byte;

  draw_hires(w_addr,
             w_byte,
             hgr_page1_bits,
             dhgr_page1_bits,
             hgr_page1_left,
             hgr_page1_right,
             dhgr_page1_left,
             dhgr_page1_right);
}

void hgr_p2_write (unsigned short w_addr, unsigned char w_byte)
{
  (*(w_memory[w_addr / 256]))[w_addr & 0xFF] = w_byte;

  draw_hires(w_addr,
             w_byte,
             hgr_page2_bits,
             dhgr_page2_bits,
             hgr_page2_left,
             hgr_page2_right,
             dhgr_page2_left,
             dhgr_page2_right);
}

/************************************/

void text40_color_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row)
{
  unsigned short * bits;
  unsigned short * last;

  /*
   * look for cases where the beam is over "clean" (unchanged) pixels
   */

  /* beam is currently left of the "dirty" area */
  if (screen_dot < text40_left[screen_row / 2])
    {
      /* advance to changed dots, and see if all the time is gone */
      raster_idx += text40_left[screen_row / 2] - screen_dot;
      if (raster_idx >= raster_cycles) return;
      screen_dot = text40_left[screen_row / 2];
    }
  /* beam is to the right of the "dirty" area */
  else if (screen_dot >= text40_right[screen_row / 2])
    {
      /* advance to end of row and see if all the time is gone */
      raster_idx += SCREEN_WIDTH - screen_dot;
      if (raster_idx >= raster_cycles) return;
      screen_dot = SCREEN_WIDTH;
    }

  /*
   * try to mark parts of the current row "clean"
   */

  /* advance the left side of the "dirty" area, if possible */
  if (screen_dot == text40_left[screen_row / 2])
    {
      text40_left[screen_row / 2] += (raster_cycles - raster_idx);

      /* mark the whole row as "clean" if possible */
      if (text40_left[screen_row / 2] >= text40_right[screen_row / 2])
        {
          text40_right[screen_row / 2] = 0;
        }
    }
  else if (screen_dot < text40_right[screen_row / 2])
    {
      /* mark the end of the "dirty" area as now being "clean" */
      if ((screen_dot + (raster_cycles - raster_idx)) >= text40_right[screen_row / 2])
        {
          text40_right[screen_row / 2] = screen_dot;
        }
    }

  /*
   * finally update when necessary
   */

  /* 40 char * 7 dots * screen_row / SCANLINE_INCR => (140 * screen_row) */

  last  = (unsigned short *)video_last;
  last += ((140 * screen_row) + (screen_dot / 2));
  bits  = (unsigned short *)text40_bits;
  bits += ((140 * screen_row) + (screen_dot / 2));

  for ( ; raster_idx < raster_cycles; raster_idx += 2)
    {
      /* beam is now below the screen -- no more work to do */
      if (screen_row >= SCREEN_HEIGHT) goto rasterize_done;

      /* beam is now to the right of the screen -- check for a wrap */
      if (screen_dot >= SCREEN_WIDTH)
        {
          /* wrap, cross the border and see if all the time is gone */
          raster_idx += (SCAN_WIDTH - screen_dot);
          if (raster_idx >= raster_cycles) goto rasterize_done;
          screen_dot = 0;
          screen_row += SCANLINE_INCR;
          /* just in case the beam wrapped off the bottom of the screen */
          if (screen_row >= SCREEN_HEIGHT) goto rasterize_done;

          /* really should repeat the "dirty" region checks here */
        }

      if (*bits != *last)
        {
          if (*bits & 1)
            {
              gdk_draw_point(my_GTK.apple_pixmap, color_line1_gc[15],
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row);
              gdk_draw_point(my_GTK.apple_pixmap, color_line1_gc[15],
                             SCREEN_BORDER_WIDTH + screen_dot + 1, SCREEN_BORDER_HEIGHT + screen_row);
              gdk_draw_point(my_GTK.apple_pixmap, color_line2_gc[15],
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row + 1);
              gdk_draw_point(my_GTK.apple_pixmap, color_line2_gc[15],
                             SCREEN_BORDER_WIDTH + screen_dot + 1, SCREEN_BORDER_HEIGHT + screen_row + 1);
            }
          else
            {
              gdk_draw_point(my_GTK.apple_pixmap, screen_da->style->black_gc,
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row);
              gdk_draw_point(my_GTK.apple_pixmap, screen_da->style->black_gc,
                             SCREEN_BORDER_WIDTH + screen_dot + 1, SCREEN_BORDER_HEIGHT + screen_row);
              gdk_draw_point(my_GTK.apple_pixmap, screen_da->style->black_gc,
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row + 1);
              gdk_draw_point(my_GTK.apple_pixmap, screen_da->style->black_gc,
                             SCREEN_BORDER_WIDTH + screen_dot + 1, SCREEN_BORDER_HEIGHT + screen_row + 1);
            }

          if ((SCREEN_BORDER_WIDTH + screen_dot) < my_GTK.screen_rect.x)
            {
              my_GTK.screen_rect.width += (my_GTK.screen_rect.x - (SCREEN_BORDER_WIDTH + screen_dot));
              my_GTK.screen_rect.x = (SCREEN_BORDER_WIDTH + screen_dot);
            }
          else if ((SCREEN_BORDER_WIDTH + screen_dot + 2) > (my_GTK.screen_rect.x + my_GTK.screen_rect.width))
            {
              my_GTK.screen_rect.width = (SCREEN_BORDER_WIDTH + screen_dot + 2) - my_GTK.screen_rect.x;
            }

          if ((SCREEN_BORDER_HEIGHT + screen_row) < my_GTK.screen_rect.y)
            {
              my_GTK.screen_rect.height += (my_GTK.screen_rect.y - (SCREEN_BORDER_HEIGHT + screen_row));
              my_GTK.screen_rect.y = (SCREEN_BORDER_HEIGHT + screen_row);
            }
          else if ((SCREEN_BORDER_HEIGHT + screen_row + 2) > (my_GTK.screen_rect.y + my_GTK.screen_rect.height))
            my_GTK.screen_rect.height = (SCREEN_BORDER_HEIGHT + screen_row + 2) - my_GTK.screen_rect.y;
        }

      *last++ = *bits++;
      screen_dot += 2;
    }

 rasterize_done:
#if UPDATE_HERE
  gtk_widget_draw(screen_da, &(my_GTK.screen_rect));
  RESET_SCREEN_RECT();
#endif
}

/************************************/

void text40_mono_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row)
{
  unsigned short * bits;
  unsigned short * last;

  /*
   * look for cases where the beam is over "clean" (unchanged) pixels
   */

  /* beam is currently left of the "dirty" area */
  if (screen_dot < text40_left[screen_row / 2])
    {
      /* advance to changed dots, and see if all the time is gone */
      raster_idx += text40_left[screen_row / 2] - screen_dot;
      if (raster_idx >= raster_cycles) return;
      screen_dot = text40_left[screen_row / 2];
    }
  /* beam is to the right of the "dirty" area */
  else if (screen_dot >= text40_right[screen_row / 2])
    {
      /* advance to end of row and see if all the time is gone */
      raster_idx += SCREEN_WIDTH - screen_dot;
      if (raster_idx >= raster_cycles) return;
      screen_dot = SCREEN_WIDTH;
    }

  /*
   * try to mark parts of the current row "clean"
   */

  /* advance the left side of the "dirty" area, if possible */
  if (screen_dot == text40_left[screen_row / 2])
    {
      text40_left[screen_row / 2] += (raster_cycles - raster_idx);

      /* mark the whole row as "clean" if possible */
      if (text40_left[screen_row / 2] >= text40_right[screen_row / 2])
        {
          text40_right[screen_row / 2] = 0;
        }
    }
  else if (screen_dot < text40_right[screen_row / 2])
    {
      /* mark the end of the "dirty" area as now being "clean" */
      if ((screen_dot + (raster_cycles - raster_idx)) >= text40_right[screen_row / 2])
        {
          text40_right[screen_row / 2] = screen_dot;
        }
    }

  /*
   * finally update when necessary
   */

  /* 40 char * 7 dots * screen_row / SCANLINE_INCR => (140 * screen_row) */

  last  = (unsigned short *)video_last;
  last += ((140 * screen_row) + (screen_dot / 2));
  bits  = (unsigned short *)text40_bits;
  bits += ((140 * screen_row) + (screen_dot / 2));

  for ( ; raster_idx < raster_cycles; raster_idx += 2)
    {
      /* beam is now below the screen -- no more work to do */
      if (screen_row >= SCREEN_HEIGHT) goto rasterize_done;

      /* beam is now to the right of the screen -- check for a wrap */
      if (screen_dot >= SCREEN_WIDTH)
        {
          /* wrap, cross the border and see if all the time is gone */
          raster_idx += (SCAN_WIDTH - screen_dot);
          if (raster_idx >= raster_cycles) goto rasterize_done;
          screen_dot = 0;
          screen_row += SCANLINE_INCR;
          /* just in case the beam wrapped off the bottom of the screen */
          if (screen_row >= SCREEN_HEIGHT) goto rasterize_done;

          /* really should repeat the "dirty" region checks here */
        }

      if (*bits != *last)
        {
          if (*bits & 1)
            {
              gdk_draw_point(my_GTK.apple_pixmap, mono_line1_gc,
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row);
              gdk_draw_point(my_GTK.apple_pixmap, mono_line1_gc,
                             SCREEN_BORDER_WIDTH + screen_dot + 1, SCREEN_BORDER_HEIGHT + screen_row);
              gdk_draw_point(my_GTK.apple_pixmap, mono_line2_gc,
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row + 1);
              gdk_draw_point(my_GTK.apple_pixmap, mono_line2_gc,
                             SCREEN_BORDER_WIDTH + screen_dot + 1, SCREEN_BORDER_HEIGHT + screen_row + 1);
            }
          else
            {
              gdk_draw_point(my_GTK.apple_pixmap, screen_da->style->black_gc,
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row);
              gdk_draw_point(my_GTK.apple_pixmap, screen_da->style->black_gc,
                             SCREEN_BORDER_WIDTH + screen_dot + 1, SCREEN_BORDER_HEIGHT + screen_row);
              gdk_draw_point(my_GTK.apple_pixmap, screen_da->style->black_gc,
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row + 1);
              gdk_draw_point(my_GTK.apple_pixmap, screen_da->style->black_gc,
                             SCREEN_BORDER_WIDTH + screen_dot + 1, SCREEN_BORDER_HEIGHT + screen_row + 1);
            }

          if ((SCREEN_BORDER_WIDTH + screen_dot) < my_GTK.screen_rect.x)
            {
              my_GTK.screen_rect.width += (my_GTK.screen_rect.x - (SCREEN_BORDER_WIDTH + screen_dot));
              my_GTK.screen_rect.x = (SCREEN_BORDER_WIDTH + screen_dot);
            }
          else if ((SCREEN_BORDER_WIDTH + screen_dot + 2) > (my_GTK.screen_rect.x + my_GTK.screen_rect.width))
            {
              my_GTK.screen_rect.width = (SCREEN_BORDER_WIDTH + screen_dot + 2) - my_GTK.screen_rect.x;
            }

          if ((SCREEN_BORDER_HEIGHT + screen_row) < my_GTK.screen_rect.y)
            {
              my_GTK.screen_rect.height += (my_GTK.screen_rect.y - (SCREEN_BORDER_HEIGHT + screen_row));
              my_GTK.screen_rect.y = (SCREEN_BORDER_HEIGHT + screen_row);
            }
          else if ((SCREEN_BORDER_HEIGHT + screen_row + 2) > (my_GTK.screen_rect.y + my_GTK.screen_rect.height))
            my_GTK.screen_rect.height = (SCREEN_BORDER_HEIGHT + screen_row + 2) - my_GTK.screen_rect.y;
        }

      *last++ = *bits++;
      screen_dot += 2;
    }

 rasterize_done:
#if UPDATE_HERE
  gtk_widget_draw(screen_da, &(my_GTK.screen_rect));
  RESET_SCREEN_RECT();
#endif
}

/************************************/

void text80_color_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row)
{
  unsigned char * bits;
  unsigned char * last;

  /*
   * look for cases where the beam is over "clean" (unchanged) pixels
   */

  /* beam is currently left of the "dirty" area */
  if (screen_dot < text80_left[screen_row / 2])
    {
      /* advance to changed dots, and see if all the time is gone */
      raster_idx += text80_left[screen_row / 2] - screen_dot;
      if (raster_idx >= raster_cycles) return;
      screen_dot = text80_left[screen_row / 2];
    }
  /* beam is to the right of the "dirty" area */
  else if (screen_dot >= text80_right[screen_row / 2])
    {
      /* advance to end of row and see if all the time is gone */
      raster_idx += SCREEN_WIDTH - screen_dot;
      if (raster_idx >= raster_cycles) return;
      screen_dot = SCREEN_WIDTH;
    }

  /*
   * try to mark parts of the current row "clean"
   */

  /* advance the left side of the "dirty" area, if possible */
  if (screen_dot == text80_left[screen_row / 2])
    {
      text80_left[screen_row / 2] += (raster_cycles - raster_idx);

      /* mark the whole row as "clean" if possible */
      if (text80_left[screen_row / 2] >= text80_right[screen_row / 2])
        {
          text80_right[screen_row / 2] = 0;
        }
    }
  else if (screen_dot < text80_right[screen_row / 2])
    {
      /* mark the end of the "dirty" area as now being "clean" */
      if ((screen_dot + (raster_cycles - raster_idx)) >= text80_right[screen_row / 2])
        {
          text80_right[screen_row / 2] = screen_dot;
        }
    }

  /*
   * finally update when necessary
   */

  /* 80 char * 7 dots * screen_row / SCANLINE_INCR => (280 * screen_row) */

  last = video_last + (280 * screen_row) + screen_dot;
  bits = text80_bits + (280 * screen_row) + screen_dot;

  for ( ; raster_idx < raster_cycles; ++raster_idx)
    {
      /* beam is now below the screen -- no more work to do */
      if (screen_row >= SCREEN_HEIGHT) goto rasterize_done;

      /* beam is now to the right of the screen -- check for a wrap */
      if (screen_dot >= SCREEN_WIDTH)
        {
          /* wrap, cross the border and see if all the time is gone */
          raster_idx += (SCAN_WIDTH - screen_dot);
          if (raster_idx >= raster_cycles) goto rasterize_done;
          screen_dot = 0;
          screen_row += SCANLINE_INCR;
          /* just in case the beam wrapped off the bottom of the screen */
          if (screen_row >= SCREEN_HEIGHT) goto rasterize_done;
        }

      if (*bits != *last)
        {
          if (*bits & 1)
            {
              gdk_draw_point(my_GTK.apple_pixmap, color_line1_gc[15],
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row);
              gdk_draw_point(my_GTK.apple_pixmap, color_line2_gc[15],
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row + 1);
            }
          else
            {
              gdk_draw_point(my_GTK.apple_pixmap, screen_da->style->black_gc,
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row);
              gdk_draw_point(my_GTK.apple_pixmap, screen_da->style->black_gc,
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row + 1);
            }

          if ((SCREEN_BORDER_WIDTH + screen_dot) < my_GTK.screen_rect.x)
            {
              my_GTK.screen_rect.width += (my_GTK.screen_rect.x - (SCREEN_BORDER_WIDTH + screen_dot));
              my_GTK.screen_rect.x = (SCREEN_BORDER_WIDTH + screen_dot);
            }
          else if ((SCREEN_BORDER_WIDTH + screen_dot + 1) > (my_GTK.screen_rect.x + my_GTK.screen_rect.width))
            {
              my_GTK.screen_rect.width = (SCREEN_BORDER_WIDTH + screen_dot + 1) - my_GTK.screen_rect.x;
            }

          if ((SCREEN_BORDER_HEIGHT + screen_row) < my_GTK.screen_rect.y)
            {
              my_GTK.screen_rect.height += (my_GTK.screen_rect.y - (SCREEN_BORDER_HEIGHT + screen_row));
              my_GTK.screen_rect.y = (SCREEN_BORDER_HEIGHT + screen_row);
            }
          else if ((SCREEN_BORDER_HEIGHT + screen_row + 2) > (my_GTK.screen_rect.y + my_GTK.screen_rect.height))
            my_GTK.screen_rect.height = (SCREEN_BORDER_HEIGHT + screen_row + 2) - my_GTK.screen_rect.y;
        }

      *last++ = *bits++;
      ++screen_dot;
    }

 rasterize_done:
#if UPDATE_HERE
  gtk_widget_draw(screen_da, &(my_GTK.screen_rect));
  RESET_SCREEN_RECT();
#endif
}

/************************************/

void text80_mono_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row)
{
  unsigned char * bits;
  unsigned char * last;

  /*
   * look for cases where the beam is over "clean" (unchanged) pixels
   */

  /* beam is currently left of the "dirty" area */
  if (screen_dot < text80_left[screen_row / 2])
    {
      /* advance to changed dots, and see if all the time is gone */
      raster_idx += text80_left[screen_row / 2] - screen_dot;
      if (raster_idx >= raster_cycles) return;
      screen_dot = text80_left[screen_row / 2];
    }
  /* beam is to the right of the "dirty" area */
  else if (screen_dot >= text80_right[screen_row / 2])
    {
      /* advance to end of row and see if all the time is gone */
      raster_idx += SCREEN_WIDTH - screen_dot;
      if (raster_idx >= raster_cycles) return;
      screen_dot = SCREEN_WIDTH;
    }

  /*
   * try to mark parts of the current row "clean"
   */

  /* advance the left side of the "dirty" area, if possible */
  if (screen_dot == text80_left[screen_row / 2])
    {
      text80_left[screen_row / 2] += (raster_cycles - raster_idx);

      /* mark the whole row as "clean" if possible */
      if (text80_left[screen_row / 2] >= text80_right[screen_row / 2])
        {
          text80_right[screen_row / 2] = 0;
        }
    }
  else if (screen_dot < text80_right[screen_row / 2])
    {
      /* mark the end of the "dirty" area as now being "clean" */
      if ((screen_dot + (raster_cycles - raster_idx)) >= text80_right[screen_row / 2])
        {
          text80_right[screen_row / 2] = screen_dot;
        }
    }

  /*
   * finally update when necessary
   */

  /* 80 char * 7 dots * screen_row / SCANLINE_INCR => (280 * screen_row) */

  last = video_last + (280 * screen_row) + screen_dot;
  bits = text80_bits + (280 * screen_row) + screen_dot;

  for ( ; raster_idx < raster_cycles; ++raster_idx)
    {
      /* beam is now below the screen -- no more work to do */
      if (screen_row >= SCREEN_HEIGHT) goto rasterize_done;

      /* beam is now to the right of the screen -- check for a wrap */
      if (screen_dot >= SCREEN_WIDTH)
        {
          /* wrap, cross the border and see if all the time is gone */
          raster_idx += (SCAN_WIDTH - screen_dot);
          if (raster_idx >= raster_cycles) goto rasterize_done;
          screen_dot = 0;
          screen_row += SCANLINE_INCR;
          /* just in case the beam wrapped off the bottom of the screen */
          if (screen_row >= SCREEN_HEIGHT) goto rasterize_done;
        }

      if (*bits != *last)
        {
          if (*bits & 1)
            {
              gdk_draw_point(my_GTK.apple_pixmap, mono_line1_gc,
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row);
              gdk_draw_point(my_GTK.apple_pixmap, mono_line2_gc,
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row + 1);
            }
          else
            {
              gdk_draw_point(my_GTK.apple_pixmap, screen_da->style->black_gc,
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row);
              gdk_draw_point(my_GTK.apple_pixmap, screen_da->style->black_gc,
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row + 1);
            }

          if ((SCREEN_BORDER_WIDTH + screen_dot) < my_GTK.screen_rect.x)
            {
              my_GTK.screen_rect.width += (my_GTK.screen_rect.x - (SCREEN_BORDER_WIDTH + screen_dot));
              my_GTK.screen_rect.x = (SCREEN_BORDER_WIDTH + screen_dot);
            }
          else if ((SCREEN_BORDER_WIDTH + screen_dot + 1) > (my_GTK.screen_rect.x + my_GTK.screen_rect.width))
            {
              my_GTK.screen_rect.width = (SCREEN_BORDER_WIDTH + screen_dot + 1) - my_GTK.screen_rect.x;
            }

          if ((SCREEN_BORDER_HEIGHT + screen_row) < my_GTK.screen_rect.y)
            {
              my_GTK.screen_rect.height += (my_GTK.screen_rect.y - (SCREEN_BORDER_HEIGHT + screen_row));
              my_GTK.screen_rect.y = (SCREEN_BORDER_HEIGHT + screen_row);
            }
          else if ((SCREEN_BORDER_HEIGHT + screen_row + 2) > (my_GTK.screen_rect.y + my_GTK.screen_rect.height))
            my_GTK.screen_rect.height = (SCREEN_BORDER_HEIGHT + screen_row + 2) - my_GTK.screen_rect.y;
        }

      *last++ = *bits++;
      ++screen_dot;
    }

 rasterize_done:
#if UPDATE_HERE
  gtk_widget_draw(screen_da, &(my_GTK.screen_rect));
  RESET_SCREEN_RECT();
#endif
}

/************************************/

void gr_color_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row)
{
  unsigned char * bits;
  unsigned char * last;

  /*
   * look for cases where the beam is over "clean" (unchanged) pixels
   */

  /* beam is currently left of the "dirty" area */
  if (screen_dot < gr_left[screen_row / 2])
    {
      /* advance to changed dots, and see if all the time is gone */
      raster_idx += gr_left[screen_row / 2] - screen_dot;
      if (raster_idx >= raster_cycles) return;
      screen_dot = gr_left[screen_row / 2];
    }
  /* beam is to the right of the "dirty" area */
  else if (screen_dot >= gr_right[screen_row / 2])
    {
      /* advance to end of row and see if all the time is gone */
      raster_idx += SCREEN_WIDTH - screen_dot;
      if (raster_idx >= raster_cycles) return;
      screen_dot = SCREEN_WIDTH;
    }

  /*
   * try to mark parts of the current row "clean"
   */

  /* advance the left side of the "dirty" area, if possible */
  if (screen_dot == gr_left[screen_row / 2])
    {
      gr_left[screen_row / 2] += (raster_cycles - raster_idx);

      /* mark the whole row as "clean" if possible */
      if (gr_left[screen_row / 2] >= gr_right[screen_row / 2])
        {
          gr_right[screen_row / 2] = 0;
        }
    }
  else if (screen_dot < gr_right[screen_row / 2])
    {
      /* mark the end of the "dirty" area as now being "clean" */
      if ((screen_dot + (raster_cycles - raster_idx)) >= gr_right[screen_row / 2])
        {
          gr_right[screen_row / 2] = screen_dot;
        }
    }

  /*
   * finally update when necessary
   */

  /* 80 char * 7 dots * screen_row / SCANLINE_INCR => (280 * screen_row) */

  last = video_last + (280 * screen_row) + screen_dot;
  bits = gr_bits + (280 * screen_row) + screen_dot;

  for ( ; raster_idx < raster_cycles; ++raster_idx)
    {
      /* beam is now below the screen -- no more work to do */
      if (screen_row >= SCREEN_HEIGHT) goto rasterize_done;

      /* beam is now to the right of the screen -- check for a wrap */
      if (screen_dot >= SCREEN_WIDTH)
        {
          /* wrap, cross the border and see if all the time is gone */
          raster_idx += (SCAN_WIDTH - screen_dot);
          if (raster_idx >= raster_cycles) goto rasterize_done;
          screen_dot = 0;
          screen_row += SCANLINE_INCR;
          /* just in case the beam wrapped off the bottom of the screen */
          if (screen_row >= SCREEN_HEIGHT) goto rasterize_done;
        }

      if (*bits != *last)
        {
          gdk_draw_point(my_GTK.apple_pixmap, color_line1_gc[(*bits >> 4) & 0xF],
                         SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row);
          gdk_draw_point(my_GTK.apple_pixmap, color_line2_gc[(*bits >> 4) & 0xF],
                         SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row + 1);

          if ((SCREEN_BORDER_WIDTH + screen_dot) < my_GTK.screen_rect.x)
            {
              my_GTK.screen_rect.width += (my_GTK.screen_rect.x - (SCREEN_BORDER_WIDTH + screen_dot));
              my_GTK.screen_rect.x = (SCREEN_BORDER_WIDTH + screen_dot);
            }
          else if ((SCREEN_BORDER_WIDTH + screen_dot + 1) > (my_GTK.screen_rect.x + my_GTK.screen_rect.width))
            {
              my_GTK.screen_rect.width = (SCREEN_BORDER_WIDTH + screen_dot + 1) - my_GTK.screen_rect.x;
            }

          if ((SCREEN_BORDER_HEIGHT + screen_row) < my_GTK.screen_rect.y)
            {
              my_GTK.screen_rect.height += (my_GTK.screen_rect.y - (SCREEN_BORDER_HEIGHT + screen_row));
              my_GTK.screen_rect.y = (SCREEN_BORDER_HEIGHT + screen_row);
            }
          else if ((SCREEN_BORDER_HEIGHT + screen_row + 2) > (my_GTK.screen_rect.y + my_GTK.screen_rect.height))
            my_GTK.screen_rect.height = (SCREEN_BORDER_HEIGHT + screen_row + 2) - my_GTK.screen_rect.y;
        }

      *last++ = *bits++;
      ++screen_dot;
    }

 rasterize_done:
#if UPDATE_HERE
  gtk_widget_draw(screen_da, &(my_GTK.screen_rect));
  RESET_SCREEN_RECT();
#endif
}

/************************************/

void gr_color_mixed40_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row)
{
  /*
   * theoretically there could be an error here if, for example, screen_row == 319 and so
   * gr_rasterizer() is called, but then in that function the beam wraps to the next line and
   * draws something. Then we might get a small segment of GR in the first line of the text
   * area. This is only possible, however, when raster_cycles is quite large. I'm not sure
   * that it is ever so large.
   */

  if (screen_row < 320) 
    gr_color_rasterizer(raster_cycles, raster_idx, screen_dot, screen_row);
  else
    text40_color_rasterizer(raster_cycles, raster_idx, screen_dot, screen_row);
}

/************************************/

void gr_color_mixed80_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row)
{
  /*
   * theoretically there could be an error here if, for example, screen_row == 319 and so
   * gr_rasterizer() is called, but then in that function the beam wraps to the next line and
   * draws something. Then we might get a small segment of GR in the first line of the text
   * area. This is only possible, however, when raster_cycles is quite large. I'm not sure
   * that it is ever so large.
   */

  if (screen_row < 320) 
    gr_color_rasterizer(raster_cycles, raster_idx, screen_dot, screen_row);
  else
    text80_color_rasterizer(raster_cycles, raster_idx, screen_dot, screen_row);
}

/************************************/

void gr_mono_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row)
{
  unsigned char * bits;
  unsigned char * last;

  /*
   * look for cases where the beam is over "clean" (unchanged) pixels
   */

  /* beam is currently left of the "dirty" area */
  if (screen_dot < gr_left[screen_row / 2])
    {
      /* advance to changed dots, and see if all the time is gone */
      raster_idx += gr_left[screen_row / 2] - screen_dot;
      if (raster_idx >= raster_cycles) return;
      screen_dot = gr_left[screen_row / 2];
    }
  /* beam is to the right of the "dirty" area */
  else if (screen_dot >= gr_right[screen_row / 2])
    {
      /* advance to end of row and see if all the time is gone */
      raster_idx += SCREEN_WIDTH - screen_dot;
      if (raster_idx >= raster_cycles) return;
      screen_dot = SCREEN_WIDTH;
    }

  /*
   * try to mark parts of the current row "clean"
   */

  /* advance the left side of the "dirty" area, if possible */
  if (screen_dot == gr_left[screen_row / 2])
    {
      gr_left[screen_row / 2] += (raster_cycles - raster_idx);

      /* mark the whole row as "clean" if possible */
      if (gr_left[screen_row / 2] >= gr_right[screen_row / 2])
        {
          gr_right[screen_row / 2] = 0;
        }
    }
  else if (screen_dot < gr_right[screen_row / 2])
    {
      /* mark the end of the "dirty" area as now being "clean" */
      if ((screen_dot + (raster_cycles - raster_idx)) >= gr_right[screen_row / 2])
        {
          gr_right[screen_row / 2] = screen_dot;
        }
    }

  /*
   * finally update when necessary
   */

  /* 80 char * 7 dots * screen_row / SCANLINE_INCR => (280 * screen_row) */

  last = video_last + (280 * screen_row) + screen_dot;
  bits = gr_bits + (280 * screen_row) + screen_dot;

  for ( ; raster_idx < raster_cycles; ++raster_idx)
    {
      /* beam is now below the screen -- no more work to do */
      if (screen_row >= SCREEN_HEIGHT) goto rasterize_done;

      /* beam is now to the right of the screen -- check for a wrap */
      if (screen_dot >= SCREEN_WIDTH)
        {
          /* wrap, cross the border and see if all the time is gone */
          raster_idx += (SCAN_WIDTH - screen_dot);
          if (raster_idx >= raster_cycles) goto rasterize_done;
          screen_dot = 0;
          screen_row += SCANLINE_INCR;
          /* just in case the beam wrapped off the bottom of the screen */
          if (screen_row >= SCREEN_HEIGHT) goto rasterize_done;
        }

      if (*bits != *last)
        {
          if (*bits & 1)
            {
              gdk_draw_point(my_GTK.apple_pixmap, mono_line1_gc,
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row);
              gdk_draw_point(my_GTK.apple_pixmap, mono_line2_gc,
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row + 1);
            }
          else
            {
              gdk_draw_point(my_GTK.apple_pixmap, screen_da->style->black_gc,
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row);
              gdk_draw_point(my_GTK.apple_pixmap, screen_da->style->black_gc,
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row + 1);
            }

          if ((SCREEN_BORDER_WIDTH + screen_dot) < my_GTK.screen_rect.x)
            {
              my_GTK.screen_rect.width += (my_GTK.screen_rect.x - (SCREEN_BORDER_WIDTH + screen_dot));
              my_GTK.screen_rect.x = (SCREEN_BORDER_WIDTH + screen_dot);
            }
          else if ((SCREEN_BORDER_WIDTH + screen_dot + 1) > (my_GTK.screen_rect.x + my_GTK.screen_rect.width))
            {
              my_GTK.screen_rect.width = (SCREEN_BORDER_WIDTH + screen_dot + 1) - my_GTK.screen_rect.x;
            }

          if ((SCREEN_BORDER_HEIGHT + screen_row) < my_GTK.screen_rect.y)
            {
              my_GTK.screen_rect.height += (my_GTK.screen_rect.y - (SCREEN_BORDER_HEIGHT + screen_row));
              my_GTK.screen_rect.y = (SCREEN_BORDER_HEIGHT + screen_row);
            }
          else if ((SCREEN_BORDER_HEIGHT + screen_row + 2) > (my_GTK.screen_rect.y + my_GTK.screen_rect.height))
            my_GTK.screen_rect.height = (SCREEN_BORDER_HEIGHT + screen_row + 2) - my_GTK.screen_rect.y;
        }

      *last++ = *bits++;
      ++screen_dot;
    }

 rasterize_done:
#if UPDATE_HERE
  gtk_widget_draw(screen_da, &(my_GTK.screen_rect));
  RESET_SCREEN_RECT();
#endif
}

/************************************/

void gr_mono_mixed40_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row)
{
  /*
   * theoretically there could be an error here if, for example, screen_row == 319 and so
   * gr_rasterizer() is called, but then in that function the beam wraps to the next line and
   * draws something. Then we might get a small segment of GR in the first line of the text
   * area. This is only possible, however, when raster_cycles is quite large. I'm not sure
   * that it is ever so large.
   */

  if (screen_row < 320) 
    gr_mono_rasterizer(raster_cycles, raster_idx, screen_dot, screen_row);
  else
    text40_mono_rasterizer(raster_cycles, raster_idx, screen_dot, screen_row);
}

/************************************/

void gr_mono_mixed80_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row)
{
  /*
   * theoretically there could be an error here if, for example, screen_row == 319 and so
   * gr_rasterizer() is called, but then in that function the beam wraps to the next line and
   * draws something. Then we might get a small segment of GR in the first line of the text
   * area. This is only possible, however, when raster_cycles is quite large. I'm not sure
   * that it is ever so large.
   */

  if (screen_row < 320) 
    gr_mono_rasterizer(raster_cycles, raster_idx, screen_dot, screen_row);
  else
    text80_mono_rasterizer(raster_cycles, raster_idx, screen_dot, screen_row);
}

/************************************/

void dgr_color_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row)
{
  unsigned char * bits;
  unsigned char * last;

  /*
   * look for cases where the beam is over "clean" (unchanged) pixels
   */

  /* beam is currently left of the "dirty" area */
  if (screen_dot < dgr_left[screen_row / 2])
    {
      /* advance to changed dots, and see if all the time is gone */
      raster_idx += dgr_left[screen_row / 2] - screen_dot;
      if (raster_idx >= raster_cycles) return;
      screen_dot = dgr_left[screen_row / 2];
    }
  /* beam is to the right of the "dirty" area */
  else if (screen_dot >= dgr_right[screen_row / 2])
    {
      /* advance to end of row and see if all the time is gone */
      raster_idx += SCREEN_WIDTH - screen_dot;
      if (raster_idx >= raster_cycles) return;
      screen_dot = SCREEN_WIDTH;
    }

  /*
   * try to mark parts of the current row "clean"
   */

  /* advance the left side of the "dirty" area, if possible */
  if (screen_dot == dgr_left[screen_row / 2])
    {
      dgr_left[screen_row / 2] += (raster_cycles - raster_idx);

      /* mark the whole row as "clean" if possible */
      if (dgr_left[screen_row / 2] >= dgr_right[screen_row / 2])
        {
          dgr_right[screen_row / 2] = 0;
        }
    }
  else if (screen_dot < dgr_right[screen_row / 2])
    {
      /* mark the end of the "dirty" area as now being "clean" */
      if ((screen_dot + (raster_cycles - raster_idx)) >= dgr_right[screen_row / 2])
        {
          dgr_right[screen_row / 2] = screen_dot;
        }
    }

  /*
   * finally update when necessary
   */

  /* 80 char * 7 dots * screen_row / SCANLINE_INCR => (280 * screen_row) */

  last = video_last + (280 * screen_row) + screen_dot;
  bits = dgr_bits + (280 * screen_row) + screen_dot;

  for ( ; raster_idx < raster_cycles; ++raster_idx)
    {
      /* beam is now below the screen -- no more work to do */
      if (screen_row >= SCREEN_HEIGHT) goto rasterize_done;

      /* beam is now to the right of the screen -- check for a wrap */
      if (screen_dot >= SCREEN_WIDTH)
        {
          /* wrap, cross the border and see if all the time is gone */
          raster_idx += (SCAN_WIDTH - screen_dot);
          if (raster_idx >= raster_cycles) goto rasterize_done;
          screen_dot = 0;
          screen_row += SCANLINE_INCR;
          /* just in case the beam wrapped off the bottom of the screen */
          if (screen_row >= SCREEN_HEIGHT) goto rasterize_done;
        }

      if (*bits != *last)
        {
          gdk_draw_point(my_GTK.apple_pixmap, color_line1_gc[(*bits >> 4) & 0xF],
                         SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row);
          gdk_draw_point(my_GTK.apple_pixmap, color_line2_gc[(*bits >> 4) & 0xF],
                         SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row + 1);

          if ((SCREEN_BORDER_WIDTH + screen_dot) < my_GTK.screen_rect.x)
            {
              my_GTK.screen_rect.width += (my_GTK.screen_rect.x - (SCREEN_BORDER_WIDTH + screen_dot));
              my_GTK.screen_rect.x = (SCREEN_BORDER_WIDTH + screen_dot);
            }
          else if ((SCREEN_BORDER_WIDTH + screen_dot + 1) > (my_GTK.screen_rect.x + my_GTK.screen_rect.width))
            {
              my_GTK.screen_rect.width = (SCREEN_BORDER_WIDTH + screen_dot + 1) - my_GTK.screen_rect.x;
            }

          if ((SCREEN_BORDER_HEIGHT + screen_row) < my_GTK.screen_rect.y)
            {
              my_GTK.screen_rect.height += (my_GTK.screen_rect.y - (SCREEN_BORDER_HEIGHT + screen_row));
              my_GTK.screen_rect.y = (SCREEN_BORDER_HEIGHT + screen_row);
            }
          else if ((SCREEN_BORDER_HEIGHT + screen_row + 2) > (my_GTK.screen_rect.y + my_GTK.screen_rect.height))
            my_GTK.screen_rect.height = (SCREEN_BORDER_HEIGHT + screen_row + 2) - my_GTK.screen_rect.y;
        }

      *last++ = *bits++;
      ++screen_dot;
    }

 rasterize_done:
#if UPDATE_HERE
  gtk_widget_draw(screen_da, &(my_GTK.screen_rect));
  RESET_SCREEN_RECT();
#endif
}

/************************************/

void dgr_color_mixed_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row)
{
  /*
   * theoretically there could be an error here if, for example, screen_row == 319 and so
   * dgr_rasterizer() is called, but then in that function the beam wraps to the next line and
   * draws something. Then we might get a small segment of DGR in the first line of the text
   * area. This is only possible, however, when raster_cycles is quite large. I'm not sure
   * that it is ever so large.
   */

  if (screen_row < 320) 
    dgr_color_rasterizer(raster_cycles, raster_idx, screen_dot, screen_row);
  else
    text80_color_rasterizer(raster_cycles, raster_idx, screen_dot, screen_row);
}

/************************************/

void dgr_mono_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row)
{
  unsigned char * bits;
  unsigned char * last;

  /*
   * look for cases where the beam is over "clean" (unchanged) pixels
   */

  /* beam is currently left of the "dirty" area */
  if (screen_dot < dgr_left[screen_row / 2])
    {
      /* advance to changed dots, and see if all the time is gone */
      raster_idx += dgr_left[screen_row / 2] - screen_dot;
      if (raster_idx >= raster_cycles) return;
      screen_dot = dgr_left[screen_row / 2];
    }
  /* beam is to the right of the "dirty" area */
  else if (screen_dot >= dgr_right[screen_row / 2])
    {
      /* advance to end of row and see if all the time is gone */
      raster_idx += SCREEN_WIDTH - screen_dot;
      if (raster_idx >= raster_cycles) return;
      screen_dot = SCREEN_WIDTH;
    }

  /*
   * try to mark parts of the current row "clean"
   */

  /* advance the left side of the "dirty" area, if possible */
  if (screen_dot == dgr_left[screen_row / 2])
    {
      dgr_left[screen_row / 2] += (raster_cycles - raster_idx);

      /* mark the whole row as "clean" if possible */
      if (dgr_left[screen_row / 2] >= dgr_right[screen_row / 2])
        {
          dgr_right[screen_row / 2] = 0;
        }
    }
  else if (screen_dot < dgr_right[screen_row / 2])
    {
      /* mark the end of the "dirty" area as now being "clean" */
      if ((screen_dot + (raster_cycles - raster_idx)) >= dgr_right[screen_row / 2])
        {
          dgr_right[screen_row / 2] = screen_dot;
        }
    }

  /*
   * finally update when necessary
   */

  /* 80 char * 7 dots * screen_row / SCANLINE_INCR => (280 * screen_row) */

  last = video_last + (280 * screen_row) + screen_dot;
  bits = dgr_bits + (280 * screen_row) + screen_dot;

  for ( ; raster_idx < raster_cycles; ++raster_idx)
    {
      /* beam is now below the screen -- no more work to do */
      if (screen_row >= SCREEN_HEIGHT) goto rasterize_done;

      /* beam is now to the right of the screen -- check for a wrap */
      if (screen_dot >= SCREEN_WIDTH)
        {
          /* wrap, cross the border and see if all the time is gone */
          raster_idx += (SCAN_WIDTH - screen_dot);
          if (raster_idx >= raster_cycles) goto rasterize_done;
          screen_dot = 0;
          screen_row += SCANLINE_INCR;
          /* just in case the beam wrapped off the bottom of the screen */
          if (screen_row >= SCREEN_HEIGHT) goto rasterize_done;
        }

      if (*bits != *last)
        {
          if (*bits & 1)
            {
              gdk_draw_point(my_GTK.apple_pixmap, mono_line1_gc,
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row);
              gdk_draw_point(my_GTK.apple_pixmap, mono_line2_gc,
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row + 1);
            }
          else
            {
              gdk_draw_point(my_GTK.apple_pixmap, screen_da->style->black_gc,
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row);
              gdk_draw_point(my_GTK.apple_pixmap, screen_da->style->black_gc,
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row + 1);
            }

          if ((SCREEN_BORDER_WIDTH + screen_dot) < my_GTK.screen_rect.x)
            {
              my_GTK.screen_rect.width += (my_GTK.screen_rect.x - (SCREEN_BORDER_WIDTH + screen_dot));
              my_GTK.screen_rect.x = (SCREEN_BORDER_WIDTH + screen_dot);
            }
          else if ((SCREEN_BORDER_WIDTH + screen_dot + 1) > (my_GTK.screen_rect.x + my_GTK.screen_rect.width))
            {
              my_GTK.screen_rect.width = (SCREEN_BORDER_WIDTH + screen_dot + 1) - my_GTK.screen_rect.x;
            }

          if ((SCREEN_BORDER_HEIGHT + screen_row) < my_GTK.screen_rect.y)
            {
              my_GTK.screen_rect.height += (my_GTK.screen_rect.y - (SCREEN_BORDER_HEIGHT + screen_row));
              my_GTK.screen_rect.y = (SCREEN_BORDER_HEIGHT + screen_row);
            }
          else if ((SCREEN_BORDER_HEIGHT + screen_row + 2) > (my_GTK.screen_rect.y + my_GTK.screen_rect.height))
            my_GTK.screen_rect.height = (SCREEN_BORDER_HEIGHT + screen_row + 2) - my_GTK.screen_rect.y;
        }

      *last++ = *bits++;
      ++screen_dot;
    }

 rasterize_done:
#if UPDATE_HERE
  gtk_widget_draw(screen_da, &(my_GTK.screen_rect));
  RESET_SCREEN_RECT();
#endif
}

/************************************/

void dgr_mono_mixed_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row)
{
  /*
   * theoretically there could be an error here if, for example, screen_row == 319 and so
   * dgr_rasterizer() is called, but then in that function the beam wraps to the next line and
   * draws something. Then we might get a small segment of DGR in the first line of the text
   * area. This is only possible, however, when raster_cycles is quite large. I'm not sure
   * that it is ever so large.
   */

  if (screen_row < 320) 
    dgr_mono_rasterizer(raster_cycles, raster_idx, screen_dot, screen_row);
  else
    text80_mono_rasterizer(raster_cycles, raster_idx, screen_dot, screen_row);
}

/************************************/

void hgr_mono_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row)
{
  unsigned char * bits;
  unsigned char * last;

  /*
   * look for cases where the beam is over "clean" (unchanged) pixels
   */

  /* beam is currently left of the "dirty" area */
  if (screen_dot < hgr_left[screen_row / 2])
    {
      /* advance to changed dots, and see if all the time is gone */
      raster_idx += hgr_left[screen_row / 2] - screen_dot;
      if (raster_idx >= raster_cycles) return;
      screen_dot = hgr_left[screen_row / 2];
    }
  /* beam is to the right of the "dirty" area */
  else if (screen_dot >= hgr_right[screen_row / 2])
    {
      /* advance to end of row and see if all the time is gone */
      raster_idx += SCREEN_WIDTH - screen_dot;
      if (raster_idx >= raster_cycles) return;
      screen_dot = SCREEN_WIDTH;
    }

  /*
   * try to mark parts of the current row "clean"
   */

  /* advance the left side of the "dirty" area, if possible */
  if (screen_dot == hgr_left[screen_row / 2])
    {
      hgr_left[screen_row / 2] += (raster_cycles - raster_idx);

      /* mark the whole row as "clean" if possible */
      if (hgr_left[screen_row / 2] >= hgr_right[screen_row / 2])
        {
          hgr_right[screen_row / 2] = 0;
        }
    }
  else if (screen_dot < hgr_right[screen_row / 2])
    {
      /* mark the end of the "dirty" area as now being "clean" */
      if ((screen_dot + (raster_cycles - raster_idx)) >= hgr_right[screen_row / 2])
        {
          hgr_right[screen_row / 2] = screen_dot;
        }
    }

  /*
   * finally update when necessary
   */

  /* 80 char * 7 dots * screen_row / SCANLINE_INCR => (280 * screen_row) */

  last = video_last + (280 * screen_row) + screen_dot;
  bits = hgr_bits + (280 * screen_row) + screen_dot;

  for ( ; raster_idx < raster_cycles; ++raster_idx)
    {
      /* beam is now below the screen -- no more work to do */
      if (screen_row >= SCREEN_HEIGHT) goto rasterize_done;

      /* beam is now to the right of the screen -- check for a wrap */
      if (screen_dot >= SCREEN_WIDTH)
        {
          /* wrap, cross the border and see if all the time is gone */
          raster_idx += (SCAN_WIDTH - screen_dot);
          if (raster_idx >= raster_cycles) goto rasterize_done;
          screen_dot = 0;
          screen_row += SCANLINE_INCR;
          /* just in case the beam wrapped off the bottom of the screen */
          if (screen_row >= SCREEN_HEIGHT) goto rasterize_done;
        }

      if (*bits != *last)
        {
          if (*bits & 1)
            {
              gdk_draw_point(my_GTK.apple_pixmap, mono_line1_gc,
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row);
              gdk_draw_point(my_GTK.apple_pixmap, mono_line2_gc,
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row + 1);
            }
          else
            {
              gdk_draw_point(my_GTK.apple_pixmap, screen_da->style->black_gc,
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row);
              gdk_draw_point(my_GTK.apple_pixmap, screen_da->style->black_gc,
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row + 1);
            }

          if ((SCREEN_BORDER_WIDTH + screen_dot) < my_GTK.screen_rect.x)
            {
              my_GTK.screen_rect.width += (my_GTK.screen_rect.x - (SCREEN_BORDER_WIDTH + screen_dot));
              my_GTK.screen_rect.x = (SCREEN_BORDER_WIDTH + screen_dot);
            }
          else if ((SCREEN_BORDER_WIDTH + screen_dot + 1) > (my_GTK.screen_rect.x + my_GTK.screen_rect.width))
            {
              my_GTK.screen_rect.width = (SCREEN_BORDER_WIDTH + screen_dot + 1) - my_GTK.screen_rect.x;
            }

          if ((SCREEN_BORDER_HEIGHT + screen_row) < my_GTK.screen_rect.y)
            {
              my_GTK.screen_rect.height += (my_GTK.screen_rect.y - (SCREEN_BORDER_HEIGHT + screen_row));
              my_GTK.screen_rect.y = (SCREEN_BORDER_HEIGHT + screen_row);
            }
          else if ((SCREEN_BORDER_HEIGHT + screen_row + 2) > (my_GTK.screen_rect.y + my_GTK.screen_rect.height))
            my_GTK.screen_rect.height = (SCREEN_BORDER_HEIGHT + screen_row + 2) - my_GTK.screen_rect.y;
        }

      *last++ = *bits++;
      ++screen_dot;
    }

 rasterize_done:
#if UPDATE_HERE
  gtk_widget_draw(screen_da, &(my_GTK.screen_rect));
  RESET_SCREEN_RECT();
#endif
}

/************************************/

void hgr_mono_mixed40_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row)
{
  /*
   * theoretically there could be an error here if, for example, screen_row == 319 and so
   * hgr_rasterizer() is called, but then in that function the beam wraps to the next line and
   * draws something. Then we might get a small segment of HGR in the first line of the text
   * area. This is only possible, however, when raster_cycles is quite large. I'm not sure
   * that it is ever so large.
   */

  if (screen_row < 320) 
    hgr_mono_rasterizer(raster_cycles, raster_idx, screen_dot, screen_row);
  else
    text40_mono_rasterizer(raster_cycles, raster_idx, screen_dot, screen_row);
}

/************************************/

void hgr_mono_mixed80_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row)
{
  /*
   * theoretically there could be an error here if, for example, screen_row == 319 and so
   * hgr_rasterizer() is called, but then in that function the beam wraps to the next line and
   * draws something. Then we might get a small segment of HGR in the first line of the text
   * area. This is only possible, however, when raster_cycles is quite large. I'm not sure
   * that it is ever so large.
   */

  if (screen_row < 320) 
    hgr_mono_rasterizer(raster_cycles, raster_idx, screen_dot, screen_row);
  else
    text80_mono_rasterizer(raster_cycles, raster_idx, screen_dot, screen_row);
}

/************************************/

void hgr_color_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row)
{
  unsigned char * bits;
  unsigned char * last;

  /*
   * look for cases where the beam is over "clean" (unchanged) pixels
   */

  /* beam is currently left of the "dirty" area */
  if (screen_dot < hgr_left[screen_row / 2])
    {
      /* advance to changed dots, and see if all the time is gone */
      raster_idx += hgr_left[screen_row / 2] - screen_dot;
      if (raster_idx >= raster_cycles) return;
      screen_dot = hgr_left[screen_row / 2];
    }
  /* beam is to the right of the "dirty" area */
  else if (screen_dot >= hgr_right[screen_row / 2])
    {
      /* advance to end of row and see if all the time is gone */
      raster_idx += SCREEN_WIDTH - screen_dot;
      if (raster_idx >= raster_cycles) return;
      screen_dot = SCREEN_WIDTH;
    }

  /*
   * try to mark parts of the current row "clean"
   */

  /* advance the left side of the "dirty" area, if possible */
  if (screen_dot == hgr_left[screen_row / 2])
    {
      hgr_left[screen_row / 2] += (raster_cycles - raster_idx);

      /* mark the whole row as "clean" if possible */
      if (hgr_left[screen_row / 2] >= hgr_right[screen_row / 2])
        {
          hgr_right[screen_row / 2] = 0;
        }
    }
  else if (screen_dot < hgr_right[screen_row / 2])
    {
      /* mark the end of the "dirty" area as now being "clean" */
      if ((screen_dot + (raster_cycles - raster_idx)) >= hgr_right[screen_row / 2])
        {
          hgr_right[screen_row / 2] = screen_dot;
        }
    }

  /*
   * finally update when necessary
   */

  /* 80 char * 7 dots * screen_row / SCANLINE_INCR => (280 * screen_row) */

  last = video_last + (280 * screen_row) + screen_dot;
  bits = hgr_bits + (280 * screen_row) + screen_dot;

  for ( ; raster_idx < raster_cycles; ++raster_idx)
    {
      int hgrpix;

      /* beam is now below the screen -- no more work to do */
      if (screen_row >= SCREEN_HEIGHT) goto rasterize_done;

      /* beam is now to the right of the screen -- check for a wrap */
      if (screen_dot >= SCREEN_WIDTH)
        {
          /* wrap, cross the border and see if all the time is gone */
          raster_idx += (SCAN_WIDTH - screen_dot);
          if (raster_idx >= raster_cycles) goto rasterize_done;
          screen_dot = 0;
          screen_row += SCANLINE_INCR;
          /* just in case the beam wrapped off the bottom of the screen */
          if (screen_row >= SCREEN_HEIGHT) goto rasterize_done;
        }

      hgrpix = 0;
      if (*bits != *last) hgrpix |= 1;
      if ((screen_dot < (SCREEN_WIDTH - 1)) && *(bits+1) != *(last+1)) hgrpix |= 2;
      if ((screen_dot < (SCREEN_WIDTH - 2)) && *(bits+2) != *(last+2)) hgrpix |= 4;
      if ((screen_dot < (SCREEN_WIDTH - 3)) && *(bits+3) != *(last+3)) hgrpix |= 8;

      if (hgrpix)
        {
          if (*bits & 1)
            {
              hgrpix = 0x11;

              if (screen_dot < (SCREEN_WIDTH - 1))
                {
                  if (*(bits+1) & 1)
                    hgrpix |= 0x22;
                }

              if (screen_dot < (SCREEN_WIDTH - 2))
                {
                  if (*(bits+2) & 1)
                    hgrpix |= 0x44;
                }

              if (screen_dot < (SCREEN_WIDTH - 3))
                {
                  if (*(bits+3) & 1)
                    hgrpix |= 0x88;
                }

              hgrpix = 0xF & (hgrpix >> (4 - (screen_dot & 0x3)));

              gdk_draw_point(my_GTK.apple_pixmap, color_line1_gc[hgrpix],
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row);
              gdk_draw_point(my_GTK.apple_pixmap, color_line2_gc[hgrpix],
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row + 1);
            }
          else
            {
              gdk_draw_point(my_GTK.apple_pixmap, screen_da->style->black_gc,
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row);
              gdk_draw_point(my_GTK.apple_pixmap, screen_da->style->black_gc,
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row + 1);
            }

          if ((SCREEN_BORDER_WIDTH + screen_dot) < my_GTK.screen_rect.x)
            {
              my_GTK.screen_rect.width += (my_GTK.screen_rect.x - (SCREEN_BORDER_WIDTH + screen_dot));
              my_GTK.screen_rect.x = (SCREEN_BORDER_WIDTH + screen_dot);
            }
          else if ((SCREEN_BORDER_WIDTH + screen_dot + 1) > (my_GTK.screen_rect.x + my_GTK.screen_rect.width))
            {
              my_GTK.screen_rect.width = (SCREEN_BORDER_WIDTH + screen_dot + 1) - my_GTK.screen_rect.x;
            }

          if ((SCREEN_BORDER_HEIGHT + screen_row) < my_GTK.screen_rect.y)
            {
              my_GTK.screen_rect.height += (my_GTK.screen_rect.y - (SCREEN_BORDER_HEIGHT + screen_row));
              my_GTK.screen_rect.y = (SCREEN_BORDER_HEIGHT + screen_row);
            }
          else if ((SCREEN_BORDER_HEIGHT + screen_row + 2) > (my_GTK.screen_rect.y + my_GTK.screen_rect.height))
            my_GTK.screen_rect.height = (SCREEN_BORDER_HEIGHT + screen_row + 2) - my_GTK.screen_rect.y;
        }

      *last++ = *bits++;
      ++screen_dot;
    }

 rasterize_done:
#if UPDATE_HERE
  gtk_widget_draw(screen_da, &(my_GTK.screen_rect));
  RESET_SCREEN_RECT();
#endif
}

/************************************/

void hgr_color_mixed40_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row)
{
  /*
   * theoretically there could be an error here if, for example, screen_row == 319 and so
   * hgr_rasterizer() is called, but then in that function the beam wraps to the next line and
   * draws something. Then we might get a small segment of HGR in the first line of the text
   * area. This is only possible, however, when raster_cycles is quite large. I'm not sure
   * that it is ever so large.
   */

  if (screen_row < 320) 
    hgr_color_rasterizer(raster_cycles, raster_idx, screen_dot, screen_row);
  else
    text40_color_rasterizer(raster_cycles, raster_idx, screen_dot, screen_row);
}

/************************************/

void hgr_color_mixed80_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row)
{
  /*
   * theoretically there could be an error here if, for example, screen_row == 319 and so
   * hgr_rasterizer() is called, but then in that function the beam wraps to the next line and
   * draws something. Then we might get a small segment of HGR in the first line of the text
   * area. This is only possible, however, when raster_cycles is quite large. I'm not sure
   * that it is ever so large.
   */

  if (screen_row < 320) 
    hgr_color_rasterizer(raster_cycles, raster_idx, screen_dot, screen_row);
  else
    text80_color_rasterizer(raster_cycles, raster_idx, screen_dot, screen_row);
}

/************************************/

void dhgr_color_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row)
{
  unsigned char * bits;
  unsigned char * last;

  /*
   * look for cases where the beam is over "clean" (unchanged) pixels
   */

  /* beam is currently left of the "dirty" area */
  if (screen_dot < dhgr_left[screen_row / 2])
    {
      /* advance to changed dots, and see if all the time is gone */
      raster_idx += dhgr_left[screen_row / 2] - screen_dot;
      if (raster_idx >= raster_cycles) return;
      screen_dot = dhgr_left[screen_row / 2];
    }
  /* beam is to the right of the "dirty" area */
  else if (screen_dot >= dhgr_right[screen_row / 2])
    {
      /* advance to end of row and see if all the time is gone */
      raster_idx += SCREEN_WIDTH - screen_dot;
      if (raster_idx >= raster_cycles) return;
      screen_dot = SCREEN_WIDTH;
    }

  /*
   * try to mark parts of the current row "clean"
   */

  /* advance the left side of the "dirty" area, if possible */
  if (screen_dot == dhgr_left[screen_row / 2])
    {
      dhgr_left[screen_row / 2] += (raster_cycles - raster_idx);

      /* mark the whole row as "clean" if possible */
      if (dhgr_left[screen_row / 2] >= dhgr_right[screen_row / 2])
        {
          dhgr_right[screen_row / 2] = 0;
        }
    }
  else if (screen_dot < dhgr_right[screen_row / 2])
    {
      /* mark the end of the "dirty" area as now being "clean" */
      if ((screen_dot + (raster_cycles - raster_idx)) >= dhgr_right[screen_row / 2])
        {
          dhgr_right[screen_row / 2] = screen_dot;
        }
    }

  /*
   * finally update when necessary
   */

  /* 80 char * 7 dots * screen_row / SCANLINE_INCR => (280 * screen_row) */

  last = video_last + (280 * screen_row) + screen_dot;
  bits = dhgr_bits + (280 * screen_row) + screen_dot;

  for ( ; raster_idx < raster_cycles; ++raster_idx)
    {
      int dhgrpix;

      /* beam is now below the screen -- no more work to do */
      if (screen_row >= SCREEN_HEIGHT) goto rasterize_done;

      /* beam is now to the right of the screen -- check for a wrap */
      if (screen_dot >= SCREEN_WIDTH)
        {
          /* wrap, cross the border and see if all the time is gone */
          raster_idx += (SCAN_WIDTH - screen_dot);
          if (raster_idx >= raster_cycles) goto rasterize_done;
          screen_dot = 0;
          screen_row += SCANLINE_INCR;
          /* just in case the beam wrapped off the bottom of the screen */
          if (screen_row >= SCREEN_HEIGHT) goto rasterize_done;
        }

      dhgrpix = 0;
      if (*bits != *last) dhgrpix |= 1;
      if ((screen_dot < (SCREEN_WIDTH - 1)) && *(bits+1) != *(last+1)) dhgrpix |= 2;
      if ((screen_dot < (SCREEN_WIDTH - 2)) && *(bits+2) != *(last+2)) dhgrpix |= 4;
      if ((screen_dot < (SCREEN_WIDTH - 3)) && *(bits+3) != *(last+3)) dhgrpix |= 8;

      if (dhgrpix)
        {
          if (*bits & 1)
            {
              dhgrpix = 0x11;

              if ((screen_dot < (SCREEN_WIDTH - 1)) && (*(bits+1) & 1)) dhgrpix |= 0x22;
              if ((screen_dot < (SCREEN_WIDTH - 2)) && (*(bits+2) & 1)) dhgrpix |= 0x44;
              if ((screen_dot < (SCREEN_WIDTH - 3)) && (*(bits+3) & 1)) dhgrpix |= 0x88;

              dhgrpix <<= 1;
              dhgrpix = 0xF & (dhgrpix >> (4 - (screen_dot & 0x3)));

              gdk_draw_point(my_GTK.apple_pixmap, color_line1_gc[dhgrpix],
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row);
              gdk_draw_point(my_GTK.apple_pixmap, color_line2_gc[dhgrpix],
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row + 1);
            }
          else
            {
              gdk_draw_point(my_GTK.apple_pixmap, screen_da->style->black_gc,
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row);
              gdk_draw_point(my_GTK.apple_pixmap, screen_da->style->black_gc,
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row + 1);
            }

          if ((SCREEN_BORDER_WIDTH + screen_dot) < my_GTK.screen_rect.x)
            {
              my_GTK.screen_rect.width += (my_GTK.screen_rect.x - (SCREEN_BORDER_WIDTH + screen_dot));
              my_GTK.screen_rect.x = (SCREEN_BORDER_WIDTH + screen_dot);
            }
          else if ((SCREEN_BORDER_WIDTH + screen_dot + 1) > (my_GTK.screen_rect.x + my_GTK.screen_rect.width))
            {
              my_GTK.screen_rect.width = (SCREEN_BORDER_WIDTH + screen_dot + 1) - my_GTK.screen_rect.x;
            }

          if ((SCREEN_BORDER_HEIGHT + screen_row) < my_GTK.screen_rect.y)
            {
              my_GTK.screen_rect.height += (my_GTK.screen_rect.y - (SCREEN_BORDER_HEIGHT + screen_row));
              my_GTK.screen_rect.y = (SCREEN_BORDER_HEIGHT + screen_row);
            }
          else if ((SCREEN_BORDER_HEIGHT + screen_row + 2) > (my_GTK.screen_rect.y + my_GTK.screen_rect.height))
            my_GTK.screen_rect.height = (SCREEN_BORDER_HEIGHT + screen_row + 2) - my_GTK.screen_rect.y;
        }

      *last++ = *bits++;
      ++screen_dot;
    }

 rasterize_done:
#if UPDATE_HERE
  gtk_widget_draw(screen_da, &(my_GTK.screen_rect));
  RESET_SCREEN_RECT();
#endif
}

/************************************/

void dhgr_color_mixed_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row)
{
  /*
   * theoretically there could be an error here if, for example, screen_row == 319 and so
   * dhgr_rasterizer() is called, but then in that function the beam wraps to the next line and
   * draws something. Then we might get a small segment of DHGR in the first line of the text
   * area. This is only possible, however, when raster_cycles is quite large. I'm not sure
   * that it is ever so large.
   */

  if (screen_row < 320) 
    dhgr_color_rasterizer(raster_cycles, raster_idx, screen_dot, screen_row);
  else
    text80_color_rasterizer(raster_cycles, raster_idx, screen_dot, screen_row);
}

/************************************/

void dhgr_mono_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row)
{
  unsigned char * bits;
  unsigned char * last;

  /*
   * look for cases where the beam is over "clean" (unchanged) pixels
   */

  /* beam is currently left of the "dirty" area */
  if (screen_dot < dhgr_left[screen_row / 2])
    {
      /* advance to changed dots, and see if all the time is gone */
      raster_idx += dhgr_left[screen_row / 2] - screen_dot;
      if (raster_idx >= raster_cycles) return;
      screen_dot = dhgr_left[screen_row / 2];
    }
  /* beam is to the right of the "dirty" area */
  else if (screen_dot >= dhgr_right[screen_row / 2])
    {
      /* advance to end of row and see if all the time is gone */
      raster_idx += SCREEN_WIDTH - screen_dot;
      if (raster_idx >= raster_cycles) return;
      screen_dot = SCREEN_WIDTH;
    }

  /*
   * try to mark parts of the current row "clean"
   */

  /* advance the left side of the "dirty" area, if possible */
  if (screen_dot == dhgr_left[screen_row / 2])
    {
      dhgr_left[screen_row / 2] += (raster_cycles - raster_idx);

      /* mark the whole row as "clean" if possible */
      if (dhgr_left[screen_row / 2] >= dhgr_right[screen_row / 2])
        {
          dhgr_right[screen_row / 2] = 0;
        }
    }
  else if (screen_dot < dhgr_right[screen_row / 2])
    {
      /* mark the end of the "dirty" area as now being "clean" */
      if ((screen_dot + (raster_cycles - raster_idx)) >= dhgr_right[screen_row / 2])
        {
          dhgr_right[screen_row / 2] = screen_dot;
        }
    }

  /*
   * finally update when necessary
   */

  /* 80 char * 7 dots * screen_row / SCANLINE_INCR => (280 * screen_row) */

  last = video_last + (280 * screen_row) + screen_dot;
  bits = dhgr_bits + (280 * screen_row) + screen_dot;

  for ( ; raster_idx < raster_cycles; ++raster_idx)
    {
      /* beam is now below the screen -- no more work to do */
      if (screen_row >= SCREEN_HEIGHT) goto rasterize_done;

      /* beam is now to the right of the screen -- check for a wrap */
      if (screen_dot >= SCREEN_WIDTH)
        {
          /* wrap, cross the border and see if all the time is gone */
          raster_idx += (SCAN_WIDTH - screen_dot);
          if (raster_idx >= raster_cycles) goto rasterize_done;
          screen_dot = 0;
          screen_row += SCANLINE_INCR;
          /* just in case the beam wrapped off the bottom of the screen */
          if (screen_row >= SCREEN_HEIGHT) goto rasterize_done;
        }

      if (*bits != *last)
        {
          if (*bits & 1)
            {
              gdk_draw_point(my_GTK.apple_pixmap, mono_line1_gc,
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row);
              gdk_draw_point(my_GTK.apple_pixmap, mono_line2_gc,
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row + 1);
            }
          else
            {
              gdk_draw_point(my_GTK.apple_pixmap, screen_da->style->black_gc,
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row);
              gdk_draw_point(my_GTK.apple_pixmap, screen_da->style->black_gc,
                             SCREEN_BORDER_WIDTH + screen_dot, SCREEN_BORDER_HEIGHT + screen_row + 1);
            }

          if ((SCREEN_BORDER_WIDTH + screen_dot) < my_GTK.screen_rect.x)
            {
              my_GTK.screen_rect.width += (my_GTK.screen_rect.x - (SCREEN_BORDER_WIDTH + screen_dot));
              my_GTK.screen_rect.x = (SCREEN_BORDER_WIDTH + screen_dot);
            }
          else if ((SCREEN_BORDER_WIDTH + screen_dot + 1) > (my_GTK.screen_rect.x + my_GTK.screen_rect.width))
            {
              my_GTK.screen_rect.width = (SCREEN_BORDER_WIDTH + screen_dot + 1) - my_GTK.screen_rect.x;
            }

          if ((SCREEN_BORDER_HEIGHT + screen_row) < my_GTK.screen_rect.y)
            {
              my_GTK.screen_rect.height += (my_GTK.screen_rect.y - (SCREEN_BORDER_HEIGHT + screen_row));
              my_GTK.screen_rect.y = (SCREEN_BORDER_HEIGHT + screen_row);
            }
          else if ((SCREEN_BORDER_HEIGHT + screen_row + 2) > (my_GTK.screen_rect.y + my_GTK.screen_rect.height))
            my_GTK.screen_rect.height = (SCREEN_BORDER_HEIGHT + screen_row + 2) - my_GTK.screen_rect.y;
        }

      *last++ = *bits++;
      ++screen_dot;
    }

 rasterize_done:
#if UPDATE_HERE
  gtk_widget_draw(screen_da, &(my_GTK.screen_rect));
  RESET_SCREEN_RECT();
#endif
}

/************************************/

void dhgr_mono_mixed_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row)
{
  /*
   * theoretically there could be an error here if, for example, screen_row == 319 and so
   * dhgr_rasterizer() is called, but then in that function the beam wraps to the next line and
   * draws something. Then we might get a small segment of DHGR in the first line of the text
   * area. This is only possible, however, when raster_cycles is quite large. I'm not sure
   * that it is ever so large.
   */

  if (screen_row < 320) 
    dhgr_mono_rasterizer(raster_cycles, raster_idx, screen_dot, screen_row);
  else
    text80_mono_rasterizer(raster_cycles, raster_idx, screen_dot, screen_row);
}

/************************************/

void rebuild_rasterizer (void)
{
  int screen_dot;
  int screen_row;

  unsigned char * last;

  last = video_last;

  for (screen_row = 0; screen_row < SCREEN_HEIGHT; screen_row += 2)
    for (screen_dot = 0; screen_dot < SCREEN_WIDTH; ++screen_dot)
      {
        switch (*last & 0xE)
          {
          case 0x0:
            if (option_color_monitor)
              text40_color_rasterizer(1, 0, screen_dot, screen_row);
            else
              text40_mono_rasterizer(1, 0, screen_dot, screen_row);
            break;

          case 0x2: break;
          case 0x4: break;
          case 0x6: break;
          case 0x8: break;
          case 0xa: break;
          case 0xc: break;
          }
      }
}
