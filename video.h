/*
  video.h
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

#ifndef INCLUDED_VIDEO_H
#define INCLUDED_VIDEO_H

/* intialization */
void init_video (void);
void reset_video (void);
void set_scan (void);

/* writes to video memory */
void hgr_p1_write (unsigned short w_addr, unsigned char w_byte);
void hgr_p2_write (unsigned short w_addr, unsigned char w_byte);

void text_p1_write (unsigned short w_addr, unsigned char w_byte);
void text_p2_write (unsigned short w_addr, unsigned char w_byte);

/* set the current video mode based on the video switches */
void set_video_mode (void);

/* rasterizers */
void text40_mono_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row);
void text40_color_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row);
void text40_color_dga_rasterizer (int raster_cycles,
                                  int raster_idx,
                                  int screen_dot,
                                  int screen_row);

void text80_mono_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row);
void text80_color_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row);
void text80_color_dga_rasterizer (int raster_cycles,
                                  int raster_idx,
                                  int screen_dot,
                                  int screen_row);

void gr_mono_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row);
void gr_mono_mixed40_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row);
void gr_mono_mixed80_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row);

void gr_color_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row);
void gr_color_mixed40_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row);
void gr_color_mixed80_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row);
void gr_color_dga_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row);
void gr_color_mixed40_dga_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row);
void gr_color_mixed80_dga_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row);

void dgr_color_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row);
void dgr_color_mixed_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row);
void dgr_color_dga_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row);
void dgr_color_mixed_dga_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row);
void dgr_mono_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row);
void dgr_mono_mixed_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row);

void hgr_mono_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row);
void hgr_mono_mixed40_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row);
void hgr_mono_mixed80_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row);

void hgr_color_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row);
void hgr_color_mixed40_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row);
void hgr_color_mixed80_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row);
void hgr_color_dga_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row);
void hgr_color_mixed40_dga_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row);
void hgr_color_mixed80_dga_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row);

void dhgr_mono_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row);
void dhgr_mono_mixed_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row);

void dhgr_color_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row);
void dhgr_color_mixed_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row);
void dhgr_color_dga_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row);
void dhgr_color_mixed_dga_rasterizer (int raster_cycles, int raster_idx, int screen_dot, int screen_row);

#endif /* INCLUDED_VIDEO_H */
