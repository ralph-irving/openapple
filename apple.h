/*
  apple.h
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

#ifndef INCLUDED_APPLE_H
#define INCLUDED_APPLE_H

#include "xdga.h"
#include <gtk/gtk.h>

/* allow for border */

#define RASTER_TEST

#if 1
#define WINDOW_WIDTH  640
#define WINDOW_HEIGHT 480
#else
#define WINDOW_WIDTH  560
#define WINDOW_HEIGHT 384
#endif

#define SCREEN_WIDTH  560
#define SCREEN_HEIGHT 384

#define SCAN_WIDTH  910
#define SCAN_HEIGHT 524

#define SCANLINE_INCR 2

extern int SCREEN_BORDER_WIDTH;
extern int SCREEN_BORDER_HEIGHT;

#define RESET_SCREEN_RECT() \
  my_GTK.screen_rect.x      = WINDOW_WIDTH; \
  my_GTK.screen_rect.y      = WINDOW_HEIGHT; \
  my_GTK.screen_rect.width  = 0; \
  my_GTK.screen_rect.height = 0;

#define SET_SCREEN_RECT() \
  my_GTK.screen_rect.x      = 0; \
  my_GTK.screen_rect.y      = 0; \
  my_GTK.screen_rect.width  = WINDOW_WIDTH; \
  my_GTK.screen_rect.height = WINDOW_HEIGHT;

/* GTK+/Gdk display stuff */

struct GTK_data 
{
  GdkPixmap * apple_pixmap;
  GtkWidget * apple_window;
  GtkWidget * screen_da;
  GdkRectangle screen_rect;
};

extern struct GTK_data my_GTK;

/* I use this for both GTK+ and SDL */

extern GdkRectangle screen_rect;
extern xdga_info_t xdga;

/* Apple II internal state */

extern unsigned char apple_vbl;
extern unsigned long apple_vbl_time;

/* keyboard stuff */

extern unsigned char kb_byte;
extern unsigned char key_pressed;
extern unsigned char key_autorepeat;
extern unsigned char open_apple_pressed;
extern unsigned char solid_apple_pressed;

/* speaker stuff */

extern unsigned char  sample_buf[65536];
extern unsigned short sample_idx;

/* user-selectable items */

extern unsigned char option_color_monitor;
extern unsigned char option_full_scan;
extern unsigned char option_limit_speed;
extern unsigned char option_use_sdl;
extern unsigned char option_use_gtk;
extern unsigned char option_use_xdga;

/* temporary names */

#define screen_pixmap  my_GTK.screen_pixmap
#define hgr_p1_pixmap  my_GTK.hgr_p1_pixmap
#define hgr_p2_pixmap  my_GTK.hgr_p2_pixmap
#define text_p1_pixmap my_GTK.text_p1_pixmap
#define text_p2_pixmap my_GTK.text_p2_pixmap
#define apple_window   my_GTK.apple_window
#define screen_da      my_GTK.screen_da

#endif /* INCLUDED_APPLE_H */
