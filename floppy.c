/*
  floppy.c - based on the file controller.c by Peter Koch, 10.2.93
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

/*
#define VERBOSE
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpu.h"
#include "floppy.h"
#include "slots.h"

#define CYCLES_PER_SECOND       1024000
#define ROTATIONS_PER_SECOND    5
#define CYCLES_PER_BIT          4

#define BITS_PER_ROTATION       (CYCLES_PER_SECOND / ROTATIONS_PER_SECOND / CYCLES_PER_BIT)
#define BITS_PER_TRACK          BITS_PER_ROTATION

#define DOS_TRACKS_PER_DISK     35                               /*     #   #   #     */
#define DRIVE_TRACKS_PER_DISK   (2 * DOS_TRACKS_PER_DISK + 1)    /*   : # : # : # :   */
#define QUARTER_TRACKS_PER_DISK (2 * DRIVE_TRACKS_PER_DISK + 3)  /* _.:.#.:.#.:.#.:._ */

#define MINIMUM_TRACK           2
#define MAXIMUM_TRACK           (QUARTER_TRACKS_PER_DISK - 3)

/*
 * Track
 */

typedef struct raw_track
{
  unsigned char bits [BITS_PER_TRACK / 8];
}
raw_track;

static void set_track_bit (raw_track * track, int bit_idx)
{
  int byte_idx;
  int byte_offset;

  bit_idx %= BITS_PER_TRACK;

  byte_idx = bit_idx / 8;
  byte_offset = bit_idx % 8;

  switch (byte_offset)
    {
    case 0: track->bits[byte_idx] |= 0x01; break;
    case 1: track->bits[byte_idx] |= 0x02; break;
    case 2: track->bits[byte_idx] |= 0x04; break;
    case 3: track->bits[byte_idx] |= 0x08; break;
    case 4: track->bits[byte_idx] |= 0x10; break;
    case 5: track->bits[byte_idx] |= 0x20; break;
    case 6: track->bits[byte_idx] |= 0x40; break;
    case 7: track->bits[byte_idx] |= 0x80; break;
    }
}

static void clear_track_bit (raw_track * track, int bit_idx)
{
  int byte_idx;
  int byte_offset;

  bit_idx %= BITS_PER_TRACK;

  byte_idx = bit_idx / 8;
  byte_offset = bit_idx % 8;

  switch (byte_offset)
    {
    case 0: track->bits[byte_idx] &= 0xfe; break;
    case 1: track->bits[byte_idx] &= 0xfd; break;
    case 2: track->bits[byte_idx] &= 0xfb; break;
    case 3: track->bits[byte_idx] &= 0xf7; break;
    case 4: track->bits[byte_idx] &= 0xef; break;
    case 5: track->bits[byte_idx] &= 0xdf; break;
    case 6: track->bits[byte_idx] &= 0xbf; break;
    case 7: track->bits[byte_idx] &= 0x7f; break;
    }
}

static unsigned char get_track_bit (raw_track * track, int bit_idx)
{
  int byte_idx;
  int byte_offset;

  bit_idx %= BITS_PER_TRACK;

  byte_idx = bit_idx / 8;
  byte_offset = bit_idx % 8;

  switch (byte_offset)
    {
    case 0: return track->bits[byte_idx] & 0x01;
    case 1: return track->bits[byte_idx] & 0x02;
    case 2: return track->bits[byte_idx] & 0x04;
    case 3: return track->bits[byte_idx] & 0x08;
    case 4: return track->bits[byte_idx] & 0x10;
    case 5: return track->bits[byte_idx] & 0x20;
    case 6: return track->bits[byte_idx] & 0x40;
    case 7: return track->bits[byte_idx] & 0x80;
    }

  return 0;
}

/*
 * Disk
 */

typedef struct raw_disk
{
  raw_track tracks[QUARTER_TRACKS_PER_DISK];
}
raw_disk;

static void set_disk_bit (raw_disk * disk, int track_idx, int bit_idx)
{
  raw_track * track_ptr = 0;

  /* catch fatal errors - shouldn't happen but... */
  if (track_idx < MINIMUM_TRACK || track_idx > MAXIMUM_TRACK)
    return;

  track_ptr = disk->tracks + track_idx;

  /* set the actual track */
  set_track_bit(track_ptr, bit_idx);

  /* blow away the quarter tracks on either side */
  set_track_bit(track_ptr - 1, bit_idx);
  set_track_bit(track_ptr + 1, bit_idx);

  /* affect the half tracks on either side */
  if (rand() > (RAND_MAX / 2))
    set_track_bit(track_ptr - 2, bit_idx);

  if (rand() > (RAND_MAX / 2))
    set_track_bit(track_ptr + 2, bit_idx);
}

static void clear_disk_bit (raw_disk * disk, int track_idx, int bit_idx)
{
  raw_track * track_ptr = 0;

  /* catch fatal errors - shouldn't happen but... */
  if (track_idx < MINIMUM_TRACK || track_idx > MAXIMUM_TRACK)
    return;

  track_ptr = disk->tracks + track_idx;

  /* clear the actual track */
  clear_track_bit(track_ptr, bit_idx);

  /* blow away the quarter tracks on either side */
  clear_track_bit(track_ptr - 1, bit_idx);
  clear_track_bit(track_ptr + 1, bit_idx);

  /* affect the half tracks on either side */
  if (rand() > (RAND_MAX / 2))
    clear_track_bit(track_ptr - 2, bit_idx);

  if (rand() > (RAND_MAX / 2))
    clear_track_bit(track_ptr + 2, bit_idx);
}

static unsigned char get_disk_bit (raw_disk * disk, int track_idx, int bit_idx)
{
  /* catch fatal errors - shouldn't happen but... */
  if (track_idx < MINIMUM_TRACK || track_idx > MAXIMUM_TRACK)
    return 0;

  return get_track_bit(disk->tracks + track_idx, bit_idx);
}

static unsigned long last_cycle_clock = 0;

typedef struct disk_ii_drive
{
  raw_disk * disk;
  unsigned char shift_buffer;
  unsigned long current_bit;

  int current_track;
}
disk_ii_drive;

static void init_disk_drive (disk_ii_drive * drive)
{
  drive->disk = 0;
  drive->current_track = MINIMUM_TRACK;
}

static void increment_track (disk_ii_drive * drive)
{
  if (drive->current_track < MAXIMUM_TRACK)
    --(drive->current_track);
}

static void decrement_track (disk_ii_drive * drive)
{
  if (drive->current_track > MINIMUM_TRACK)
    --(drive->current_track);
}

static unsigned char read_disk_byte (disk_ii_drive * drive)
{
  int idx;
  int shift;
  unsigned long updated_bit = drive->current_bit;

  /* see how many bits the disk rotated */
  updated_bit += (cycle_clock - last_cycle_clock) / 4;

  /* cap that at 8, to see how many bits need to be shifted into the shift_buffer */
  shift = (updated_bit - drive->current_bit) % 8;

  /* use updated_bit to index the bits that need to be shifted in */
  updated_bit = updated_bit % BITS_PER_TRACK - shift;

  /* start shifting */
  for (idx = 0; idx < shift; ++idx)
    {
      /* shift ... */
      drive->shift_buffer = drive->shift_buffer << 1;

      /* ... in the appropriate bit */
      if (get_disk_bit(drive->disk, drive->current_track, updated_bit))
        drive->shift_buffer |= 0x01;
      else
        drive->shift_buffer &= 0xfe;

      /* advance the bit index */
      ++updated_bit;
      updated_bit %= BITS_PER_TRACK;
    }

  /* update current_bit and return what's now in the shift buffer */
  drive->current_bit = updated_bit;
  return drive->shift_buffer;
}

static void write_disk_byte (disk_ii_drive * drive, unsigned char byte)
{
  int idx;
  unsigned long updated_bit = drive->current_bit;

  /* get new position after rotation */
  updated_bit += (cycle_clock - last_cycle_clock) / 4;
  updated_bit %= BITS_PER_TRACK;

  /* remember the new position */
  drive->current_bit = updated_bit;

  /* prepare buffer for writing */
  drive->shift_buffer = byte;

  /* write out all 8 bits (if there is another write before all 8 bits would have really */
  /* been written out, it will just write over what we're writing now) */
  for (idx = 0; idx < 8; ++idx)
    {
      /* write a bit */
      if (byte & 0x80)
        set_disk_bit(drive->disk, drive->current_track, updated_bit);
      else
        clear_disk_bit(drive->disk, drive->current_track, updated_bit);

      /* shift the shift_buffer */
      byte <<= 1;

      /* increment the bit index */
      ++updated_bit;
      updated_bit %= BITS_PER_TRACK;
    }
}

#define SECTORS_PER_TRACK        16
#define RAW_BITS_PER_BYTE        8
#define BYTES_PER_SECTOR         256
#define RAW_BYTES_PER_SECTOR     374
#define TRACKS_PER_DISK          35
#define RAW_TRACK_SIZE           6200
#define MICROSECONDS_PER_BIT     4.0
#define CYCLES_PER_RAW_BIT       (CYCLES_PER_SECOND / 1000000.0 * MICROSECONDS_PER_BIT)

enum { DOS_ORDER, PRODOS_ORDER };
unsigned char current_image_type = DOS_ORDER;

struct disk_ii
{
  unsigned char   dirty;
  unsigned char   new_disk;
  unsigned char   protected;
  unsigned char   image_type;

  unsigned short  byte;
  unsigned short  half_track;

  char *          filename;
  FILE *          file;

  unsigned char   image[TRACKS_PER_DISK][RAW_TRACK_SIZE];
};

struct disk_ii    disk_drive[2];
struct disk_ii *  current_drive;

static unsigned char  sector_buf[256];

/* Disk-Parameter */
unsigned char d2_volume;

static unsigned char  d2_motor_on;
static unsigned char  d2_select;
static unsigned char  d2_write_ready;

static int            d2_slot;

static unsigned long  d2_write_time;
static unsigned long  d2_motor_off_time;

/* Skewing-Tables */
static int  w_skewing[16] = { 0, 13, 11, 9,  7, 5,  3, 1, 14, 12, 10, 8, 6, 4, 2, 15 };
static int  r_skewing[16] = { 0,  7, 14, 6, 13, 5, 12, 4, 11,  3, 10, 2, 9, 1, 8, 15 };

static unsigned char  d2_rom[256] =
{
  0xa2, 0x20, 0xa0, 0x00, 0xa2, 0x03, 0x86, 0x3c,
  0x8a, 0x0a, 0x24, 0x3c, 0xf0, 0x10, 0x05, 0x3c,
  0x49, 0xff, 0x29, 0x7e, 0xb0, 0x08, 0x4a, 0xd0,
  0xfb, 0x98, 0x9d, 0x56, 0x03, 0xc8, 0xe8, 0x10,
  0xe5, 0x20, 0x58, 0xff, 0xba, 0xbd, 0x00, 0x01,
  0x0a, 0x0a, 0x0a, 0x0a, 0x85, 0x2b, 0xaa, 0xbd,
  0x8e, 0xc0, 0xbd, 0x8c, 0xc0, 0xbd, 0x8a, 0xc0,
  0xbd, 0x89, 0xc0, 0xa0, 0x50, 0xbd, 0x80, 0xc0,
  0x98, 0x29, 0x03, 0x0a, 0x05, 0x2b, 0xaa, 0xbd,
  0x81, 0xc0, 0xa9, 0x56, 0x20, 0xa8, 0xfc, 0x88,
  0x10, 0xeb, 0x85, 0x26, 0x85, 0x3d, 0x85, 0x41,
  0xa9, 0x08, 0x85, 0x27, 0x18, 0x08, 0xbd, 0x8c,
  0xc0, 0x10, 0xfb, 0x49, 0xd5, 0xd0, 0xf7, 0xbd,
  0x8c, 0xc0, 0x10, 0xfb, 0xc9, 0xaa, 0xd0, 0xf3,
  0xea, 0xbd, 0x8c, 0xc0, 0x10, 0xfb, 0xc9, 0x96,
  0xf0, 0x09, 0x28, 0x90, 0xdf, 0x49, 0xad, 0xf0,
  0x25, 0xd0, 0xd9, 0xa0, 0x03, 0x85, 0x40, 0xbd,
  0x8c, 0xc0, 0x10, 0xfb, 0x2a, 0x85, 0x3c, 0xbd,
  0x8c, 0xc0, 0x10, 0xfb, 0x25, 0x3c, 0x88, 0xd0,
  0xec, 0x28, 0xc5, 0x3d, 0xd0, 0xbe, 0xa5, 0x40,
  0xc5, 0x41, 0xd0, 0xb8, 0xb0, 0xb7, 0xa0, 0x56,
  0x84, 0x3c, 0xbc, 0x8c, 0xc0, 0x10, 0xfb, 0x59,
  0xd6, 0x02, 0xa4, 0x3c, 0x88, 0x99, 0x00, 0x03,
  0xd0, 0xee, 0x84, 0x3c, 0xbc, 0x8c, 0xc0, 0x10,
  0xfb, 0x59, 0xd6, 0x02, 0xa4, 0x3c, 0x91, 0x26,
  0xc8, 0xd0, 0xef, 0xbc, 0x8c, 0xc0, 0x10, 0xfb,
  0x59, 0xd6, 0x02, 0xd0, 0x87, 0xa0, 0x00, 0xa2,
  0x56, 0xca, 0x30, 0xfb, 0xb1, 0x26, 0x5e, 0x00,
  0x03, 0x2a, 0x5e, 0x00, 0x03, 0x2a, 0x91, 0x26,
  0xc8, 0xd0, 0xee, 0xe6, 0x27, 0xe6, 0x3d, 0xa5,
  0x3d, 0xcd, 0x00, 0x08, 0xa6, 0x2b, 0x90, 0xdb,
  0x4c, 0x01, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* Translation Table */
static int  translate[256] =
{
  0x96, 0x97, 0x9a, 0x9b, 0x9d, 0x9e, 0x9f, 0xa6,
  0xa7, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb2, 0xb3,
  0xb4, 0xb5, 0xb6, 0xb7, 0xb9, 0xba, 0xbb, 0xbc,
  0xbd, 0xbe, 0xbf, 0xcb, 0xcd, 0xce, 0xcf, 0xd3,
  0xd6, 0xd7, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde,
  0xdf, 0xe5, 0xe6, 0xe7, 0xe9, 0xea, 0xeb, 0xec,
  0xed, 0xee, 0xef, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6,
  0xf7, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x01,
  0x80, 0x80, 0x02, 0x03, 0x80, 0x04, 0x05, 0x06,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x07, 0x08,
  0x80, 0x80, 0x80, 0x09, 0x0a, 0x0b, 0x0c, 0x0d,
  0x80, 0x80, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13,
  0x80, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x1b, 0x80, 0x1c, 0x1d, 0x1e,
  0x80, 0x80, 0x80, 0x1f, 0x80, 0x80, 0x20, 0x21,
  0x80, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x29, 0x2a, 0x2b,
  0x80, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32,
  0x80, 0x80, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
  0x80, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f
};

static unsigned char  which_half[16] =    { 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1 };
static unsigned char  sector_offset[16] = { 0, 7, 6, 6, 5, 5, 4, 4, 3, 3, 2, 2, 1, 1, 0, 7 };

static void  d2_write_image (struct disk_ii *  d_ptr);

void  d2_update_position (void)
{
  if (d2_motor_on)
    {
      current_drive->byte = (1 + current_drive->byte) % RAW_TRACK_SIZE;
      /*
        printf("\rTrack = %d.%d  Byte = %d", current_drive->half_track/2,
        (current_drive->half_track & 1) ? 5 : 0, current_drive->byte);
        fflush(stdout);
      */
    }
}

static void  diskread (FILE *           df,
                       int              track,
                       int              sector,
                       unsigned char *  dbuf)
{
  unsigned short  block;
  unsigned short  start;
  int             read_err;

  if (df)
    {
      block    = 8 * track + sector_offset[sector];
      start    = (which_half[sector] ? 256 : 0);
      read_err = fseek(df, 512L * (long)block + start, SEEK_SET);
      read_err = fread(dbuf, 256, 1, df);
    }
}

static void  diskwrite (FILE *           df,
                        int              track,
                        int              sector,
                        unsigned char *  dbuf)
{
  unsigned short  block;
  unsigned short  start;
  int             write_err;
	
  if (df)
    {
      block     = 8 * track + sector_offset[sector];
      start     = (which_half[sector] ? 256 : 0);
      write_err = fseek(df, 512L * (long)block + start, SEEK_SET);
      write_err = fwrite(dbuf, 256, 1, df);
      fflush(df);
    }
}

static unsigned char  d2_image_raw_byte (struct disk_ii *  d_ptr,
                                         unsigned short    t_idx,
                                         unsigned short    s_idx,
                                         unsigned short    b_idx)
{
  long  f_pos;

  static unsigned char  old;
  static unsigned char  xor;
  static unsigned char  raw;
  static unsigned char  check;

  raw = 0xFF;

  switch (b_idx)
    {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 20:
    case 21:
    case 22:
    case 23:
    case 24:
      /* sync bytes */
      return 0xFF;

    case 6:
    case 25:
      /* sector header byte #1 */
      return 0xD5;

    case 7:
    case 26:
      /* sector header byte #2 */
      return 0xAA;

    case 8:
      /* address header byte */
      return 0x96;

    case 9:
      /* volume byte #1 */
      check = d2_volume;
      return 0xAA | (d2_volume >> 1);

    case 10:
      /* volume */
      return 0xAA | d2_volume;

    case 11:
      /* track byte #1 */
      check ^= t_idx;
      return 0xAA | (t_idx >> 1);

    case 12:
      /* track byte #2 */
      return 0xAA | t_idx;

    case 13:
      /* sector byte #1 */
      check ^= s_idx;
      return 0xAA | (s_idx >> 1);

    case 14:
      /* sector byte #2 */
      return 0xAA | s_idx;

    case 15:
      /* checksum byte #1 */
      return 0xAA | (check >> 1);

    case 16:
      /* checksum byte #2 */
      return 0xAA | check;

    case 17:
    case 371:
      /* address/data trailer byte #1 */
      return 0xDE;

    case 18:
    case 372:
      /* address/data trailer byte #2 */
      return 0xAA;

    case 19:
    case 373:
      /* address/data trailer byte #3 */
      return 0xEB;

    case 27:
      /* data header */
      xor = 0;

      if (DOS_ORDER == d_ptr->image_type)
        {
          /* Read the coming sector */
          f_pos = (256 * 16 * t_idx) + (256 * r_skewing[s_idx]);

          fseek(d_ptr->file, f_pos, SEEK_SET);
          fread(sector_buf, 1, 256, d_ptr->file);
        }
      else if (PRODOS_ORDER == d_ptr->image_type)
        {
          diskread(d_ptr->file, t_idx, r_skewing[s_idx], sector_buf);
        }

      return 0xAD;

    case 370:
      /* checksum */
      return translate[xor & 0x3F];

    default:
      b_idx -= 28;

      if (b_idx >= 0x56)
        {
          /* 6 Bit */
          old  = sector_buf[b_idx - 0x56];
          old  = old >> 2;
          xor ^= old;
          raw  = translate[xor & 0x3F];
          xor  = old;
        }
      else
        {
          /* 3 * 2 Bit */
          old  = (sector_buf[b_idx] & 0x01) << 1;
          old |= (sector_buf[b_idx] & 0x02) >> 1;
          old |= (sector_buf[b_idx + 0x56] & 0x01) << 3;
          old |= (sector_buf[b_idx + 0x56] & 0x02) << 1;
          old |= (sector_buf[b_idx + 0xAC] & 0x01) << 5;
          old |= (sector_buf[b_idx + 0xAC] & 0x02) << 3;
          xor ^= old;
          raw  = translate[xor & 0x3F];
          xor  = old;
        }
      break;
    }

  return raw;
}

static void  d2_load_images (void)
{
  int  d_idx;

  for (d_idx = 0; d_idx < 2; d_idx++)
    {
      if (disk_drive[d_idx].new_disk)
        {
          disk_drive[d_idx].new_disk = 0;

          /* write out and close the old disk image, if any */
          if (disk_drive[d_idx].file)
            {
              d2_write_image(disk_drive + d_idx);
              fclose(disk_drive[d_idx].file);
            }

          if (0 == disk_drive[d_idx].filename)
            continue;

          /* Try to open for read/write */
          disk_drive[d_idx].file = fopen(disk_drive[d_idx].filename, "r+b");

          if (disk_drive[d_idx].file)
            disk_drive[d_idx].protected = 0;
          else
            {
              /* Try to open read-only and treat it like a notched disk */
              disk_drive[d_idx].file = fopen(disk_drive[d_idx].filename, "rb");

              if (disk_drive[d_idx].file)
                disk_drive[d_idx].protected = 1;
              else
                {
                  /* Try to create the file */
                  disk_drive[d_idx].file = fopen(disk_drive[d_idx].filename, "wb");
                  if (disk_drive[d_idx].file)
                    {
                      fclose(disk_drive[d_idx].file);
                      disk_drive[d_idx].file = fopen(disk_drive[d_idx].filename, "r+b");

                      if (disk_drive[d_idx].file)
                        disk_drive[d_idx].protected = 0;
                    }
                }
            }

          if (disk_drive[d_idx].file)
            {
              long  size;

              fseek(disk_drive[d_idx].file, 0, SEEK_END);
              size = ftell(disk_drive[d_idx].file);

              if (0 == size)
                {
                  /* new file... make a blank disk */

                  long  b_idx;

                  fseek(disk_drive[d_idx].file, 0, SEEK_SET);
                  for (b_idx = 0; b_idx < 143360; b_idx++)
                    fputc(0, disk_drive[d_idx].file);

                  fseek(disk_drive[d_idx].file, 0, SEEK_END);
                  size = ftell(disk_drive[d_idx].file);

                  if (143360 != size)
                    {
                      fclose(disk_drive[d_idx].file);
                      disk_drive[d_idx].file = 0;
                      remove(disk_drive[d_idx].filename);
                      size = 0;
                    }
                }

              if (143360 == size)
                {
                  unsigned short  t_idx;
                  unsigned short  s_idx;
                  unsigned short  sb_idx;
                  unsigned short  tb_idx;

                  /* ### */
                  disk_drive[d_idx].image_type = current_image_type;

                  for (t_idx = 0; t_idx < TRACKS_PER_DISK; t_idx++)
                    {
                      tb_idx = 0;

                      for (s_idx = 0; s_idx < SECTORS_PER_TRACK; s_idx++)
                        {
                          for (sb_idx = 0; sb_idx < RAW_BYTES_PER_SECTOR; sb_idx++, tb_idx++)
                            {
                              disk_drive[d_idx].image[t_idx][tb_idx] =
                                d2_image_raw_byte(disk_drive + d_idx, t_idx, s_idx, sb_idx);
                            }
                        }

                      while (tb_idx < RAW_TRACK_SIZE)
                        {
                          disk_drive[d_idx].image[t_idx][tb_idx] = 0xFF;
                          tb_idx++;
                        }
                    }
                }
              else if (disk_drive[d_idx].file)
                {
                  fclose(disk_drive[d_idx].file);
                  disk_drive[d_idx].file = 0;
                }
            }

          disk_drive[d_idx].dirty      = 0;
          disk_drive[d_idx].byte       = 0;
          disk_drive[d_idx].half_track = 0;
        }
    }
}

/*
** Returns the next byte from the 'disk'
*/

static unsigned char  d2_readbyte (void)
{
  d2_update_position();
  d2_load_images();

  /* make sure there's a 'disk in the drive' */
  if (0 == current_drive->file)
    return 0xFF;

#ifdef VERBOSE
  {
    unsigned char  rb;

    rb = current_drive->image[current_drive->half_track / 2][current_drive->byte];
    printf("%ld Read [%2.2x] from T:%2.2x B:%d\n",
           cycle_clock, rb, current_drive->half_track / 2, current_drive->byte);
  }
#endif

  return current_drive->image[current_drive->half_track / 2][current_drive->byte];
}

/*
** Returns the appropriate disk controller (IWM) register based on the
** state of the Q6 and Q7 select bits.
*/

static unsigned char  d2_read_selected (void)
{
  if (!d2_write_ready)
    {
      if ((cycle_clock - d2_write_time) > (RAW_BITS_PER_BYTE * CYCLES_PER_RAW_BIT))
        d2_write_ready = 0x80;
    }

  switch (d2_select)
    {
    case 0:
      return d2_readbyte();

    case 1:
      return current_drive->protected | d2_motor_on;

    case 2:
      return d2_write_ready;
    }

  return 0;
}

static void  d2_write_image (struct disk_ii *  d_ptr)
{
  unsigned char   xor;
  unsigned char   s_idx;

  int  sector_search_count;

  int  t_idx;
  int  b_idx;
  int  tb_idx;
  int  sector_count;

  unsigned char   sector_buf[BYTES_PER_SECTOR + 2];

  if (0 == d_ptr->dirty)
    return;

  /*
    printf("%ld Writing disk II image #%d\n", cycle_clock, d_ptr == disk_drive ? 0 : 1);
  */

  tb_idx = 0;
  for (t_idx = 0; t_idx < TRACKS_PER_DISK; t_idx++)
    {
      sector_count = 16;

    find_sector:

      /* look for the sector header */

      sector_search_count = 0;
      while (0xD5 != d_ptr->image[t_idx][tb_idx])
        {
          tb_idx = (tb_idx + 1) % RAW_TRACK_SIZE;
          if (++sector_search_count > RAW_TRACK_SIZE) return;
        }

      tb_idx = (tb_idx + 1) % RAW_TRACK_SIZE;
      if (0xAA != d_ptr->image[t_idx][tb_idx])
        {
          tb_idx = (tb_idx + 1) % RAW_TRACK_SIZE;
          goto find_sector;
        }

      tb_idx = (tb_idx + 1) % RAW_TRACK_SIZE;
      if (0x96 != d_ptr->image[t_idx][tb_idx])
        {
          tb_idx = (tb_idx + 1) % RAW_TRACK_SIZE;
          goto find_sector;
        }

      /* found the sector header */

      /* skip 'volume' and 'track' */
      tb_idx = (tb_idx + 5) % RAW_TRACK_SIZE;

      /* sector byte #1 */
      s_idx  = 0x55 | (d_ptr->image[t_idx][tb_idx] << 1);
      tb_idx = (tb_idx + 1) % RAW_TRACK_SIZE;
      s_idx &= d_ptr->image[t_idx][tb_idx];
      tb_idx = (tb_idx + 1) % RAW_TRACK_SIZE;

      /* look for the sector data */

      while (0xD5 != d_ptr->image[t_idx][tb_idx])
        tb_idx = (tb_idx + 1) % RAW_TRACK_SIZE;

      tb_idx = (tb_idx + 1) % RAW_TRACK_SIZE;
      if (0xAA != d_ptr->image[t_idx][tb_idx])
        {
          tb_idx = (tb_idx + 1) % RAW_TRACK_SIZE;
          goto find_sector;
        }

      tb_idx = (tb_idx + 1) % RAW_TRACK_SIZE;
      if (0xAD != d_ptr->image[t_idx][tb_idx])
        {
          tb_idx = (tb_idx + 1) % RAW_TRACK_SIZE;
          goto find_sector;
        }

      /* found the sector data */

      tb_idx = (tb_idx + 1) % RAW_TRACK_SIZE;
      xor = 0;

      for (b_idx = 0; b_idx < 342; b_idx++)
        {
          xor ^= translate[d_ptr->image[t_idx][tb_idx]];
          tb_idx = (tb_idx + 1) % RAW_TRACK_SIZE;

          if (b_idx >= 0x56)
            {
              /* 6 Bit */
              sector_buf[b_idx - 0x56] |= (xor << 2);
            }
          else
            {
              /* 3 * 2 Bit */
              sector_buf[b_idx]         = (xor & 0x01) << 1;
              sector_buf[b_idx]        |= (xor & 0x02) >> 1;
              sector_buf[b_idx + 0x56]  = (xor & 0x04) >> 1;
              sector_buf[b_idx + 0x56] |= (xor & 0x08) >> 3;
              sector_buf[b_idx + 0xAC]  = (xor & 0x10) >> 3;
              sector_buf[b_idx + 0xAC] |= (xor & 0x20) >> 5;
            }
        }

      /* write the sector */

      if (DOS_ORDER == d_ptr->image_type)
        {
          fseek(d_ptr->file,
                (BYTES_PER_SECTOR * SECTORS_PER_TRACK * t_idx) +
                (BYTES_PER_SECTOR * r_skewing[s_idx]), SEEK_SET);

          fwrite(sector_buf, 1, BYTES_PER_SECTOR, d_ptr->file);
        }
      else if (PRODOS_ORDER == d_ptr->image_type)
        {
          diskwrite(d_ptr->file, t_idx, w_skewing[s_idx], sector_buf);
        }

      sector_count--;

      if (sector_count)
        goto find_sector;
    }

  d_ptr->dirty = 0;
}

/*
** Writes a byte to the next position on the 'disk'
*/

static void  d2_writebyte (unsigned char  w_byte)
{
  d2_update_position();
  d2_load_images();

  /* make sure there's a 'disk in the drive' */
  if (0 == current_drive->file)
    return;

  if (current_drive->protected)
    return;

  /* don't allow impossible bytes */
  if (w_byte < 0x96)
    return;

  current_drive->dirty = 1;
  current_drive->image[current_drive->half_track / 2][current_drive->byte] = w_byte;
}

/*
** Writes to the appropriate disk controller (IWM) register based on the
** state of the Q6 and Q7 select bits.
*/

static void  d2_write_selected (unsigned char  w_byte)
{
  if (3 == d2_select)
    {
      if (d2_motor_on)
        {
#if 0
          if (d2_write_ready)
#else
            if (1)
#endif
              {
                d2_writebyte(w_byte);
                d2_write_time  = cycle_clock;
                d2_write_ready = 0;
              }
        }
    }
}

static void  d2_switch (unsigned short  addr)
{
  switch (addr & 0xF)
    {
    case 0x08:
      d2_motor_on       = 0;
      d2_motor_off_time = cycle_clock;
      d2_write_image(current_drive);
      break;

    case 0x09:
      d2_motor_on = 0x20;
      break;

    case 0x0A:
      /* select drive 0 */
      if (current_drive != disk_drive)
        {
          d2_write_image(current_drive);
          current_drive = disk_drive;
        }
      break;

    case 0x0B:
      /* select drive 1 */
      if (current_drive == disk_drive)
        {
          d2_write_image(current_drive);
          current_drive = disk_drive + 1;
        }
      break;

    case 0x0C:
      d2_select &= 0xFE;
      break;

    case 0x0D:
      d2_select |= 0x01;
      break;

    case 0x0E:
      d2_select &= 0xFD;
      break;

    case 0x0F:
      d2_select |= 0x02;
      break;

    default:
      {
        static int last_count = 0;

        int phase = (addr & 0x06) >> 1;

        /* move the head in and out by stepping motor */
        if (addr & 0x01)
          {
            // printf("phase %d on  : delay: %.9ld", phase, cycle_clock - last_count);
            last_count = cycle_clock;

            phase += 4;
            phase -= (current_drive->half_track % 4);
            phase %= 4;

            if (1 == phase)
              {
                if (current_drive->half_track < (2 * TRACKS_PER_DISK - 2))
                  {
                    current_drive->half_track++;
                    /* printf("++track => %d\n", current_drive->half_track); */
                  }
              }
            else if (3 == phase)
              {
                if (current_drive->half_track > 0)
                  {
                    current_drive->half_track--;
                    /* printf("--track => %d", current_drive->half_track); */
                  }
              }
          }
        else
          {
            // printf("phase %d off : delay: %.9ld", phase, cycle_clock - last_count);
            last_count = cycle_clock;
          }

        // printf("   now on track %2.2d\n", current_drive->half_track / 2);
      }
      break;
    }
}

unsigned char  d2_read (unsigned short  r_addr)
{
  if (d2_slot == ((r_addr & 0x0F00) >> 8))
    {
      return d2_rom[r_addr & 0xFF];
    }

  d2_switch(r_addr);

  /* odd addresses don't produce anything */
  if (r_addr & 1)
    return 0;

  return d2_read_selected();
}

void  d2_write (unsigned short  w_addr,
                unsigned char   w_byte)
{
  d2_switch(w_addr);

#ifdef VERBOSE
  printf("%ld Write [%2.2x]", cycle_clock, w_byte);
#endif

  /* even addresses don't write anything */
  if (w_addr & 1)
    d2_write_selected(w_byte);
}

void  d2_new_disk (int     d_idx,
                   char *  filename)
{
  char * end_ptr = 0;

  if (disk_drive[d_idx].filename)
    free(disk_drive[d_idx].filename);

  disk_drive[d_idx].new_disk = 1;
  disk_drive[d_idx].filename = filename;

  end_ptr = strrchr(filename, '.');

  if (end_ptr)
    {
      if (0 == strcmp(end_ptr+1, "do"))
        {
          current_image_type = DOS_ORDER;
          printf("DOS order\n");
        }
      else if (0 == strcmp(end_ptr+1, "po"))
        {
          current_image_type = PRODOS_ORDER;
          printf("ProDOS order\n");
        }
    }
  else if (current_image_type == DOS_ORDER)
    printf("assuming DOS order\n");
  else
    printf("assuming ProDOS order\n");
}

void  d2_peek (void)
{
}

void  d2_time (void)
{
}

void  d2_call (void)
{
}

void  d2_info (unsigned char *  bits)
{
  *bits = 0;
}

void  d2_init (void *          pwin,
               unsigned short  slot,
               cpu_state_t *   cpup,
               r_func_t *      rtab,
               w_func_t *      wtab)
{
  d2_slot = slot;

  disk_drive[0].new_disk   = 0;
  disk_drive[0].filename   = 0;
  disk_drive[0].file       = 0;

  disk_drive[1].new_disk   = 0;
  disk_drive[1].filename   = 0;
  disk_drive[1].file       = 0;

  current_drive = disk_drive;

#if 0
  /* set filenames */
  disk_drive[0].filename = "s6d1";
  disk_drive[1].filename = "s6d2";
#endif

  /* set variables */
  d2_write_ready = 0x80;
  d2_volume      = 254;
  d2_select      = 0;
  d2_motor_on    = 0;
}
