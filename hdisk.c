/*
  hdisk.c
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

#include <gtk/gtk.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include "cpu.h"
#include "memory.h"
#include "slots.h"

/* ProDOS boot block */

unsigned char boot_block [512] =
{
  0x01, 0x38, 0xB0, 0x03, 0x4C, 0x32, 0xA1, 0x86,
  0x43, 0xC9, 0x03, 0x08, 0x8A, 0x29, 0x70, 0x4A,
  0x4A, 0x4A, 0x4A, 0x09, 0xC0, 0x85, 0x49, 0xA0,
  0xFF, 0x84, 0x48, 0x28, 0xC8, 0xB1, 0x48, 0xD0,
  0x3A, 0xB0, 0x0E, 0xA9, 0x03, 0x8D, 0x00, 0x08,
  0xE6, 0x3D, 0xA5, 0x49, 0x48, 0xA9, 0x5B, 0x48,
  0x60, 0x85, 0x40, 0x85, 0x48, 0xA0, 0x63, 0xB1,
  0x48, 0x99, 0x94, 0x09, 0xC8, 0xC0, 0xEB, 0xD0,
  0xF6, 0xA2, 0x06, 0xBC, 0x1D, 0x09, 0xBD, 0x24,
  0x09, 0x99, 0xF2, 0x09, 0xBD, 0x2B, 0x09, 0x9D,
  0x7F, 0x0A, 0xCA, 0x10, 0xEE, 0xA9, 0x09, 0x85,
  0x49, 0xA9, 0x86, 0xA0, 0x00, 0xC9, 0xF9, 0xB0,
  0x2F, 0x85, 0x48, 0x84, 0x60, 0x84, 0x4A, 0x84,
  0x4C, 0x84, 0x4E, 0x84, 0x47, 0xC8, 0x84, 0x42,
  0xC8, 0x84, 0x46, 0xA9, 0x0C, 0x85, 0x61, 0x85,
  0x4B, 0x20, 0x12, 0x09, 0xB0, 0x68, 0xE6, 0x61,
  0xE6, 0x61, 0xE6, 0x46, 0xA5, 0x46, 0xC9, 0x06,
  0x90, 0xEF, 0xAD, 0x00, 0x0C, 0x0D, 0x01, 0x0C,
  0xD0, 0x6D, 0xA9, 0x04, 0xD0, 0x02, 0xA5, 0x4A,
  0x18, 0x6D, 0x23, 0x0C, 0xA8, 0x90, 0x0D, 0xE6,
  0x4B, 0xA5, 0x4B, 0x4A, 0xB0, 0x06, 0xC9, 0x0A,
  0xF0, 0x55, 0xA0, 0x04, 0x84, 0x4A, 0xAD, 0x02,
  0x09, 0x29, 0x0F, 0xA8, 0xB1, 0x4A, 0xD9, 0x02,
  0x09, 0xD0, 0xDB, 0x88, 0x10, 0xF6, 0x29, 0xF0,
  0xC9, 0x20, 0xD0, 0x3B, 0xA0, 0x10, 0xB1, 0x4A,
  0xC9, 0xFF, 0xD0, 0x33, 0xC8, 0xB1, 0x4A, 0x85,
  0x46, 0xC8, 0xB1, 0x4A, 0x85, 0x47, 0xA9, 0x00,
  0x85, 0x4A, 0xA0, 0x1E, 0x84, 0x4B, 0x84, 0x61,
  0xC8, 0x84, 0x4D, 0x20, 0x12, 0x09, 0xB0, 0x17,
  0xE6, 0x61, 0xE6, 0x61, 0xA4, 0x4E, 0xE6, 0x4E,
  0xB1, 0x4A, 0x85, 0x46, 0xB1, 0x4C, 0x85, 0x47,
  0x11, 0x4A, 0xD0, 0xE7, 0x4C, 0x00, 0x20, 0x4C,
  0x3F, 0x09, 0x26, 0x50, 0x52, 0x4F, 0x44, 0x4F,
  0x53, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0xA5, 0x60, 0x85, 0x44, 0xA5, 0x61,
  0x85, 0x45, 0x6C, 0x48, 0x00, 0x08, 0x1E, 0x24,
  0x3F, 0x45, 0x47, 0x76, 0xF4, 0xD7, 0xD1, 0xB6,
  0x4B, 0xB4, 0xAC, 0xA6, 0x2B, 0x18, 0x60, 0x4C,
  0xBC, 0x09, 0xA9, 0x9F, 0x48, 0xA9, 0xFF, 0x48,
  0xA9, 0x01, 0xA2, 0x00, 0x4C, 0x79, 0xF4, 0x20,
  0x58, 0xFC, 0xA0, 0x1C, 0xB9, 0x50, 0x09, 0x99,
  0xAE, 0x05, 0x88, 0x10, 0xF7, 0x4C, 0x4D, 0x09,
  0xAA, 0xAA, 0xAA, 0xA0, 0xD5, 0xCE, 0xC1, 0xC2,
  0xCC, 0xC5, 0xA0, 0xD4, 0xCF, 0xA0, 0xCC, 0xCF,
  0xC1, 0xC4, 0xA0, 0xD0, 0xD2, 0xCF, 0xC4, 0xCF,
  0xD3, 0xA0, 0xAA, 0xAA, 0xAA, 0xA5, 0x53, 0x29,
  0x03, 0x2A, 0x05, 0x2B, 0xAA, 0xBD, 0x80, 0xC0,
  0xA9, 0x2C, 0xA2, 0x11, 0xCA, 0xD0, 0xFD, 0xE9,
  0x01, 0xD0, 0xF7, 0xA6, 0x2B, 0x60, 0xA5, 0x46,
  0x29, 0x07, 0xC9, 0x04, 0x29, 0x03, 0x08, 0x0A,
  0x28, 0x2A, 0x85, 0x3D, 0xA5, 0x47, 0x4A, 0xA5,
  0x46, 0x6A, 0x4A, 0x4A, 0x85, 0x41, 0x0A, 0x85,
  0x51, 0xA5, 0x45, 0x85, 0x27, 0xA6, 0x2B, 0xBD,
  0x89, 0xC0, 0x20, 0xBC, 0x09, 0xE6, 0x27, 0xE6,
  0x3D, 0xE6, 0x3D, 0xB0, 0x03, 0x20, 0xBC, 0x09,
  0xBC, 0x88, 0xC0, 0x60, 0xA5, 0x40, 0x0A, 0x85,
  0x53, 0xA9, 0x00, 0x85, 0x54, 0xA5, 0x53, 0x85,
  0x50, 0x38, 0xE5, 0x51, 0xF0, 0x14, 0xB0, 0x04,
  0xE6, 0x53, 0x90, 0x02, 0xC6, 0x53, 0x38, 0x20,
  0x6D, 0x09, 0xA5, 0x50, 0x18, 0x20, 0x6F, 0x09,
  0xD0, 0xE3, 0xA0, 0x7F, 0x84, 0x52, 0x08, 0x28,
  0x38, 0xC6, 0x52, 0xF0, 0xCE, 0x18, 0x08, 0x88,
  0xF0, 0xF5, 0xBD, 0x8C, 0xC0, 0x10, 0xFB, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

void  i60_RTS (void);

/* "SmartPort" call parameter memory locations */

#define COMMAND           0x42
#define UNIT_NUMBER       0x43
#define BUFFER_PTR_LOW    0x44
#define BUFFER_PTR_HIGH   0x45
#define BLOCK_NUMBER_LOW  0x46
#define BLOCK_NUMBER_HIGH 0x47

/* "SmartPort" error codes */
#define IO_ERROR             0x27
#define NO_DEVICE_CONNECTED  0x28
#define WRITE_PROTECTED      0x2B

#undef READ
#undef WRITE

#define READ(a)    (r_table[(a)/256]((a)))
#define WRITE(a,b) (w_table[(a)/256]((a),(b)))

static r_func_t *  r_table;
static w_func_t *  w_table;

static unsigned short  hdisk_slot = 0;
static cpu_state_t     cpu;

static char *  drive_name[2] =
{
  "hdd0",
  "hdd1"
};

static unsigned int    current_drive;
static int             drive[2];
static unsigned short  block_count[2];

/*
static unsigned char   drive_filler[512];
*/

static void  hdisk_status (void)
{
  *(cpu.C) = 0;
  *(cpu.A) = 0x00;
  *(cpu.X) = block_count[current_drive] & 255;
  *(cpu.Y) = block_count[current_drive] / 256;
}

extern int  trace;

static void  hdisk_read_block (void)
{
  int   idx;
  int   buffer_addr;
  long  block_number;

  char block_buf [512];
	
  buffer_addr  = (256 * READ(BUFFER_PTR_HIGH)) + READ(BUFFER_PTR_LOW);
  block_number = (256 * READ(BLOCK_NUMBER_HIGH)) + READ(BLOCK_NUMBER_LOW);

  if (trace)
    {
      printf("%ld HD: read block %d to 0x%4.4X\n", cycle_clock,
             (unsigned short)block_number, buffer_addr);
    }
	
  if (-1 == drive[current_drive])
    {
      *(cpu.A) = NO_DEVICE_CONNECTED;
      return;
    }
	
  if (-1 == lseek(drive[current_drive], 512L * block_number ,SEEK_SET))
    {
      *(cpu.C) = 1;
      *(cpu.A) = IO_ERROR;
      return;
    }

  if (512 != read(drive[current_drive], block_buf, 512))
    {
      *(cpu.C) = 1;
      *(cpu.A) = IO_ERROR;
      return;
    }

  for (idx = buffer_addr; idx < (buffer_addr + 512); idx++)
    WRITE(idx, block_buf[idx - buffer_addr]);

  *(cpu.C) = 0;
}

static void  hdisk_write_block (void)
{
  int   idx;
  int   buffer_addr;
  long  block_number;
	
  char block_buf [512];

  buffer_addr  = (256 * READ(BUFFER_PTR_HIGH)) + READ(BUFFER_PTR_LOW);
  block_number = (256 * READ(BLOCK_NUMBER_HIGH)) + READ(BLOCK_NUMBER_LOW);
	
  if (-1 == drive[current_drive])
    {
      *(cpu.A) = NO_DEVICE_CONNECTED;
      return;
    }
	
  if (-1 == lseek(drive[current_drive], 512L * block_number ,SEEK_SET))
    {
      *(cpu.C) = 1;
      *(cpu.A) = IO_ERROR;
      return;
    }

  for (idx = buffer_addr; idx < (buffer_addr + 512); idx++)
    block_buf[idx - buffer_addr] = READ(idx);

  if (512 != (idx = write(drive[current_drive], block_buf, 512)))
    {
      *(cpu.C) = 1;

      if (0 == idx)
        *(cpu.A) = WRITE_PROTECTED;
      else
        *(cpu.A) = IO_ERROR;

      return;
    }

  *(cpu.C) = 0;
}

static void  hdisk_format (void)
{
  /* Just say we formatted it */
  *(cpu.C) = 0;
}

static void  hdisk_command (void)
{
  if (0x80 & READ(UNIT_NUMBER))
    current_drive = 1;
  else
    current_drive = 0;

  switch(READ(COMMAND))
    {
    case 0x00:
      hdisk_status();
      break;

    case 0x01:
      hdisk_read_block();
      break;

    case 0x02:
      hdisk_write_block();
      break;

    case 0x03:
      hdisk_format();
      break;

    default:
      break;
    }
}

void  hdisk_peek (void)
{
}

void  hdisk_time (void)
{
}

void  hdisk_call (void)
{
  switch (*(cpu.PC) & 0xFF)
    {
    case 0x00:
      WRITE(0x42, 0x01);
      WRITE(0x43, 0x70);
      WRITE(0x44, 0x00);
      WRITE(0x45, 0x08);
      WRITE(0x46, 0x00);
      WRITE(0x47, 0x00);

      hdisk_command();

      *(cpu.X)  = 0x70;
      *(cpu.PC) = 0x801;
      break;

    case 0x10:
      hdisk_command();
      i60_RTS();
      break;

    default:
      break;
    }
}

unsigned char  hdisk_read (unsigned short  r_addr)
{
  if (r_addr >= 0xC100 && r_addr < 0xC800)
    {
      if (drive[0] || drive[1])
        {
          switch(r_addr & 0xFF)
            {
            case 0x01: return 0x20;
            case 0x05: return 0x03;
            case 0x07: return 0x02;
            case 0xFE: return 0x1F;
            case 0xFF: return 0x10;

            default:
              break;
            }
        }
    }

  return 0;
}

void  hdisk_write (unsigned short  w_addr,
                   unsigned char   w_byte)
{
}

#if 0
static unsigned short  get_drive_size (void)
{
  return 4096;
}

#if 0
struct volume_directory_header
{
  char	stnl;         /* Storage Type & Name Length */
  char	volname[15];
  char	reserved[8];
  long	cdt;          /* 12:00am, 1/1/94 */
  char	vers;
  char	minvers;
  char	access;
  char	entlen;
  char	entperblk;
  short	filecount;
  short	bmapptr;      /* bitmap starts at block 6 */
  short	totalblks;
};
#endif

static char  boot_name[] = "boot";

void dump_boot_blocks (void)
{
  int y, x;
  FILE * bbf;
  static unsigned char  bb[512];

  fseek(drive[0], 0, SEEK_SET);
  fread(bb, 1, 512, drive[0]);

  bbf = fopen("bb.c", "w");
  fprintf(bbf, "\n/* ProDOS boot blocks */\n\nunsigned char boot_blocks [512] =\n{\n");

  for (y = 0; y < 64; y++)
    {
      fprintf(bbf, "  ");

      for (x = 0; x < 8; x++)
        {
          fprintf(bbf, "0x%2.2X", bb[y*8+x]);
          if (x < 7)
            fprintf(bbf, ", ");
          else if (y < 63)
            fprintf(bbf, ",\n");
          else
            fprintf(bbf, "\n");
        }
    }

  fprintf(bbf, "};\n\n");
  fclose(bbf);
}

static FILE *  hdisk_new (void)
{
  unsigned char   drive_byte;
  unsigned short  new_block_count;
  unsigned short  bitmap_blocks;
  long            idx;
  FILE *          new_drive;

  /* Ask user what size drive and create it */
  new_block_count = get_drive_size();
  bitmap_blocks   = new_block_count / 4096;

  if (new_block_count % 4096)
    bitmap_blocks++;

  /* Zero all blocks */
  new_drive = fopen(drive_name[0], "wb");
  for(idx = 0; idx < new_block_count; idx++)
    fwrite(&drive_filler, 1, 512, new_drive);

  /* Copy boot block onto "drive" */
  fseek(new_drive, 0L, SEEK_SET);
  fwrite(&boot_block, 1, 512, new_drive);
	
  /* Write initial volume bitmap (all free) */
  fseek(new_drive, 3072L, SEEK_SET);
  fputc(0x01, new_drive);

  drive_byte = 0xFF;
  for(idx = 1; idx < (new_block_count / 8); idx++)
    fputc(drive_byte, new_drive);

  /* new_block_count will be a multiple of 8, so don't worry about */
  /* a possible partially filled last byte in the bitmap           */

  /* Write volume directory */
  fseek(new_drive, 1026L, SEEK_SET);    /* block 2 ptr */
  fputc(0x03, new_drive);
	
  /* Write Volume Directory Header */

  drive_byte  = strlen(boot_name);
  drive_byte &= 0x0F;

  fputc(0xF0 | drive_byte, new_drive);

  for(idx = 0; idx < 15; idx++)
    {
      if (idx < drive_byte)
        fputc(boot_name[idx], new_drive);
      else
        fputc(0, new_drive);
    }

  for(idx = 0; idx < 8; idx++)
    fputc(0x00, new_drive);

  {
    time_t          tv;
    struct tm *     tp;

    unsigned short  prodos_date;
    unsigned short  prodos_time;

    prodos_time = 0;
    prodos_date = 0;

    tv = time(0);
    tp = localtime(&tv);

    if (tp)
      {
        prodos_date   = (tp->tm_year % 100);
        prodos_date <<= 4;
        prodos_date  |= (1 + tp->tm_mon);
        prodos_date <<= 5;
        prodos_date  |= tp->tm_mday;

        prodos_time   = (tp->tm_hour);
        prodos_time <<= 8;
        prodos_time  |= (tp->tm_min);
      }

    fputc(prodos_date & 255, new_drive);
    fputc(prodos_date / 256, new_drive);
    fputc(prodos_time & 255, new_drive);
    fputc(prodos_time / 256, new_drive);
  }

  fputc(0x00, new_drive);
  fputc(0x00, new_drive);

  fputc(0xC3, new_drive); /* access */
  fputc(0x27, new_drive); /* directory entry length */
  fputc(0x0D, new_drive); /* # entries per block */

  fputc(0x00, new_drive); /* number entries in volume directory */
  fputc(0x00, new_drive);

  fputc(0x06, new_drive); /* pointer to volume bitmap (block 6) */
  fputc(0x00, new_drive);

  fputc(new_block_count & 255, new_drive); /* total block count */
  fputc(new_block_count / 256, new_drive);

  /* write the rest of the block ptrs */

  fseek(new_drive, 1536L, SEEK_SET);    /* block 3 ptrs */
  fputc(0x02, new_drive);
  fputc(0x00, new_drive);
  fputc(0x04, new_drive);
  fputc(0x00, new_drive);
	
  fseek(new_drive, 2048L, SEEK_SET);    /* block 4 ptrs */
  fputc(0x03, new_drive);
  fputc(0x00, new_drive);
  fputc(0x05, new_drive);
  fputc(0x00, new_drive);
	
  fseek(new_drive, 2560L, SEEK_SET);    /* block 5 ptrs */
  fputc(0x04, new_drive);

  fclose(new_drive);
  new_drive = fopen(drive_name[0], "r+b");

  return new_drive;
}
#endif

void  hdisk_info (unsigned char *  bits)
{
  *bits = SLOT_CALL;
}

void  hdisk_init (unsigned short  slot,
                  cpu_state_t *   cpup,
                  r_func_t *      rtab,
                  w_func_t *      wtab)
{
  long  drive_size;
	
  hdisk_slot  = slot;
  cpu         = *cpup;
  r_table     = rtab;
  w_table     = wtab;

  /*********/

  drive[0] = open(drive_name[0], O_RDWR);
  if (-1 == drive[0])
    {
#if 0
      drive[0] = hdisk_new();
      fseek(drive[0], 0L, SEEK_END);
      drive_size = ftell(drive[0]);
      fclose(drive[0]);
      drive[0] = 0;
      if (drive_size % 512)
        {
          remove(drive_name[0]);
          printf("hdisk_init(): Couldn't create a new drive image\n");
        }
#endif
      return;
    }

  drive_size = lseek(drive[0], 0L, SEEK_END);
  /* drive_size = ftell(drive[0]); */
  if (drive_size % 512)
    printf("hdisk_init(): Warning. Drive 0 image size is not a multiple of 512!\n");
  block_count[0] = drive_size / 512;	
  printf("hdisk_init(): Drive 0 size = %hu blocks\n", block_count[0]); 

  drive[1] = open(drive_name[1], O_RDWR);
  if (-1 != drive[1])
    {
      drive_size = lseek(drive[1], 0L, SEEK_END);
      if (0 == drive_size)
        {
          /* do it the hard way */
          char buf[512];
          while (1)
            {
              if (512 != read(drive[1], buf, 512))
                break;
              drive_size += 512;
            }

          /* assume (hope) we can at least do this */
          if (0 != lseek(drive[1], 0, SEEK_SET))
            {
              close(drive[1]);
              drive[1] = -1;
              return;
            }
        }

      /* drive_size = ftell(drive[1]); */
      if (drive_size % 512)
        printf("hdisk_init(): Warning. Drive 1 image size is not a multiple of 512!\n");
      block_count[1] = drive_size / 512;	
      printf("hdisk_init(): Drive 1 size = %hu blocks\n", block_count[1]);
    }
}
