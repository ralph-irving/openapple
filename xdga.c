/*
  xdga.c
  Copyright 2000,2001,2002 by William Sheldon Simms III

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
#include "xdga.h"

#include <limits.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/xf86dga.h>

/* need DGA version 2.0 or better */
#define REQUIRED_MAJOR 2
#define REQUIRED_MINOR 0

static int xdga_event_base;
static int xdga_error_base;

static unsigned short apple_red [16] =
  {
    0x0000,
    0xdddd,
    0x0000,
    0xdddd,
    0x0000,
    0x5555,
    0x2222,
    0x6666,
    0x8888,
    0xffff,
    0xaaaa,
    0xffff,
    0x1111,
    0xffff,
    0x4444,
    0xffff
  };

static unsigned short apple_green [16] =
  {
    0x0000,
    0x0000,
    0x0000,
    0x2222,
    0x7777,
    0x5555,
    0x2222,
    0xaaaa,
    0x5555,
    0x6666,
    0xaaaa,
    0x9999,
    0xdddd,
    0xffff,
    0xffff,
    0xffff
  };

static unsigned short apple_blue [16] =
  {
    0x0000,
    0x3333,
    0x9999,
    0xdddd,
    0x2222,
    0x5555,
    0xffff,
    0xffff,
    0x0000,
    0x0000,
    0xaaaa,
    0x8888,
    0x0000,
    0x0000,
    0x9999,
    0xffff
  };

static int xdga_exists (Display * xdisp, char ** why)
{
  int xdga_major, xdga_minor;

  if (!XDGAQueryExtension(xdisp, &xdga_event_base, &xdga_error_base))
    {
      *why = "DGA not supported on this server.";
      return 0;
    }

  if (!XDGAQueryVersion(xdisp, &xdga_major, &xdga_minor))
    {
      *why = "Encountered problem trying to query DGA version.";
      return 0;
    }

  if (xdga_major < REQUIRED_MAJOR || (xdga_major == REQUIRED_MAJOR && xdga_minor < REQUIRED_MINOR))
    {
      *why = "DGA version on this server is too old. Need DGA version 2.0 or better.";
      return 0;
    }

  return 1;
}

static void rgb_bits (XDGAMode * mode, int * r_bit, int * g_bit, int * b_bit)
{
  int idx;
  int mask;

  for (idx = 0, mask = 1; idx < 32; ++idx, mask <<= 1)
    {
      if (mode->redMask & mask)
        break;
    }
  *r_bit = idx;

  for (idx = 0, mask = 1; idx < 32; ++idx, mask <<= 1)
    {
      if (mode->greenMask & mask)
        break;
    }
  *g_bit = idx;

  for (idx = 0, mask = 1; idx < 32; ++idx, mask <<= 1)
    {
      if (mode->blueMask & mask)
        break;
    }
  *b_bit = idx;
}

static int set_8_bit_pixels (xdga_info_t * info, XDGAMode * mode, char ** why)
{
  int idx;
  int x, y;
  char * line_ptr;
  char * pixel_ptr;
  XColor color;

  info->cmap =
    XDGACreateColormap(info->xdisp, DefaultScreen(info->xdisp), info->device, AllocAll);

  if (0 == info->cmap)
    {
      *why = "Couldn't create colormap for PseudoColor DGA mode.";
      return 1;
    }

  for (idx = 0; idx < 16; ++idx)
    {
      color.pixel = idx;
      color.red = apple_red[idx];
      color.green = apple_green[idx];
      color.blue = apple_blue[idx];
      color.flags = DoRed | DoGreen | DoBlue;

      XStoreColor(info->xdisp, info->cmap, &color);
    }

  XDGAInstallColormap(info->xdisp, DefaultScreen(info->xdisp), info->cmap);

  line_ptr = info->base;
  for (y = 0; y < info->height; ++y)
    {
      pixel_ptr = line_ptr;
      for (x = 0; x < info->width; ++x)
        *pixel_ptr++ = 0;
      line_ptr += info->bytes_per_line;
    }

  return 1;
}

static int set_15_bit_pixels (xdga_info_t * info, XDGAMode * mode, char ** why)
{
  int idx;
  int x, y;
  char * line_ptr;
  short * pixel_ptr;
  int r_bit, g_bit, b_bit;

  rgb_bits(mode, &r_bit, &g_bit, &b_bit);

  for (idx = 0; idx < 16; ++idx)
    {
      info->pixel[idx].halfword[HALFWORD_IDX] = (((apple_red[idx] >> 11) << r_bit) +
                                                 ((apple_green[idx] >> 11) << g_bit) +
                                                 ((apple_blue[idx] >> 11) << b_bit));
    }

  line_ptr = info->base;
  for (y = 0; y < info->height; ++y)
    {
      pixel_ptr = (short *)line_ptr;
      for (x = 0; x < info->width; ++x)
        *pixel_ptr++ = 0;
      line_ptr += info->bytes_per_line;
    }

  *why = 0;
  return 1;
}

static int set_24_bit_pixels (xdga_info_t * info, XDGAMode * mode, char ** why)
{
  int idx;
  int x, y;
  char * line_ptr;
  long * pixel_ptr;
  int r_bit, g_bit, b_bit;

  rgb_bits(mode, &r_bit, &g_bit, &b_bit);

  for (idx = 0; idx < 16; ++idx)
    {
      info->pixel[idx].word = (((apple_red[idx] >> 8) << r_bit) +
                               ((apple_green[idx] >> 8) << g_bit) +
                               ((apple_blue[idx] >> 8) << b_bit));
    }

  line_ptr = info->base;
  for (y = 0; y < info->height; ++y)
    {
      pixel_ptr = (long *)line_ptr;
      for (x = 0; x < info->width; ++x)
        *pixel_ptr++ = 0;
      line_ptr += info->bytes_per_line;
    }

  *why = 0;
  return 1;
}

int xdga_mode (Display * xdisp, int request_w, int request_h, xdga_info_t * info, char ** why)
{
  int idx = 0;
  int num_modes = 0;

  int score = 0;
  int best_idx = -1;
  int best_score = INT_MAX;

  XDGAMode mode;
  XDGAMode * xdgaidx = 0;
  XDGAMode * xdgainfo = 0;

  if (!xdga_exists(xdisp, why))
    return 0;

  if (request_w < 600) request_w = 600;
  if (request_h < 400) request_h = 400;

  xdgaidx = xdgainfo = XDGAQueryModes(xdisp, DefaultScreen(xdisp), &num_modes);
  if (0 == xdgainfo)
    {
      *why = "Encountered problem trying to query DGA video modes.";
      return 0;
    }

  /*
    Look for 8, 16, or 32 bit modes. With (at least) the requested width & height.
    Reject interlaced and doublescan modes.
    Reject 16 bit modes that have an extra bit for green.
    Reject modes less than 600x400 regardless of the requested size.
    Prefer smaller bpp modes. Prefer smaller dimensions.
   */

  for (idx = 0; idx < num_modes; ++idx, ++xdgaidx)
    {
      if (xdgaidx->viewportWidth < request_w || xdgaidx->viewportHeight < request_h)
        continue;

      if (xdgaidx->depth != 8 && xdgaidx->depth != 15 && xdgaidx->depth != 24)
        continue;

      if (xdgaidx->depth == 8 && xdgaidx->visualClass != PseudoColor)
        continue;

      if (xdgaidx->depth == 24 && xdgaidx->bitsPerPixel == 24)
        continue;

      if (xdgaidx->flags & (XDGAInterlaced | XDGADoublescan))
        continue;

      score = 0;

      if (xdgaidx->bitsPerPixel < 32) ++score;
      if (xdgaidx->bitsPerPixel < 16) ++score;

      score += (xdgaidx->viewportWidth - request_w);
      score += (xdgaidx->viewportHeight - request_h);

      if (xdgaidx->visualClass == DirectColor)
        ++score;

      if (score < best_score)
        {
          best_score = score;
          best_idx = idx;
        }
    }

  if (best_idx < 0)
    {
      XFree(xdgainfo);
      *why = "Couldn't find a DGA video mode that I can support.";
      return 0;
    }

  mode = xdgainfo[best_idx];
  XFree(xdgainfo);

  /*
  printf("Using %dx%dx%d class %d\n",
         mode.viewportWidth, mode.viewportHeight, mode.depth, mode.visualClass);
  */

  /* go ahead and open the chosen video mode */

  if (!XDGAOpenFramebuffer(xdisp, DefaultScreen(xdisp)))
    {
      *why = "Couldn't map the DGA frame buffer into my address space";
      return 0;
    }

  if (0 == (info->device = XDGASetMode(xdisp, DefaultScreen(xdisp), mode.num)))
    {
      *why = "Couldn't set DGA video mode.";
      return 0;
    }

  info->xdisp = xdisp;
  info->event_base = xdga_event_base;
  info->error_base = xdga_error_base;
  info->depth = mode.depth;
  info->width = mode.viewportWidth;
  info->height = mode.viewportHeight;
  info->base = info->device->data;
  info->bytes_per_line = mode.bytesPerScanline;

  switch (mode.depth)
    {
    case 8:
      info->bytes_per_pixel = 1;
      if (!set_8_bit_pixels(info, &mode, why))
        goto error;
      break;

    case 15:
      info->bytes_per_pixel = 2;
      if (!set_15_bit_pixels(info, &mode, why))
        goto error;
      break;

    case 24:
      info->bytes_per_pixel = 4;
      if (!set_24_bit_pixels(info, &mode, why))
        goto error;
      break;
    }

  return 1;

 error:
  XDGASetMode(info->xdisp, DefaultScreen(info->xdisp), 0);
  XFree(info->device);
  XDGACloseFramebuffer(info->xdisp, DefaultScreen(info->xdisp));
  return 0;
}
