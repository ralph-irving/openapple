/*
  cpu.c
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
#include <stdlib.h>
#include <string.h>

#include "apple.h"
#include "cpu.h"
#include "memory.h"
#include "slots.h"
#include "tables.h"

/* #define ZPI_BOUNDS */
/* #define TRACE_ADD */
#define ENABLE_TRACE
/* #define DECIMAL_WARN */

extern unsigned char switch_slotc3rom;
extern unsigned char switch_slotcxrom;

int trace = 0;
int param_frame_skip = 1; /* # frames to skip == param_frame_skip - 1 */

/*
void catch_write (unsigned short a, unsigned char b)
{
  if ((a & 0xFF) == 0x9D00)
    trace = 1;
  
  (w_page[(a)/256]((a),(b)));
}

#undef WRITE
#define WRITE(a,b) catch_write(a,b)
*/

/* 65c02 registers */

unsigned char  A;         /* Accumulator     */
unsigned char  X, Y;      /* Index Registers */
unsigned char  P = 0x20;  /* Status Byte     */
unsigned char  S;         /* Stack Pointer   */

unsigned char  C, Z, I, D, B, V, N;

unsigned short emPC;

cpu_state_t  cpu_ptrs;

/* variables */

static unsigned char  opcode;

unsigned char  interrupt_flags = F_RESET;

unsigned long  i_delay;
unsigned long  cycle_clock;

int raster_row = 0;
int raster_dot = 0;

void (* rasterizer) (int, int, int, int);

static volatile unsigned char  thumb;

/* Macro to implement branches */

#define BRANCH \
do { \
  unsigned short branch_dest; \
\
  branch_dest = (unsigned short)((long)(emPC + 1) + (long)(signed char)READ(emPC)); \
  emPC++; \
\
  if ((emPC & 0xFF00) == (branch_dest & 0xFF00)) \
    cycle_clock += 1; \
  else \
    cycle_clock += 2; \
\
  emPC = branch_dest; \
} while (0)

/* Macros to push bytes to and pull bytes from the 65c02 stack */

#define PUSH(b) (WRITE_STACK(S,(b)),S--)
#define PULL()  (S++,READ_STACK(S))

/* Macro to set N and Z flags */

#define SET_FLAGS_NZ(a)     \
  N = ((a) & 0x80) ? 1 : 0; \
  Z = (a) ? 0 : 1;          \

/* addressing mode functions */

/* helper functions */

/* #define USE_ARITH_TABLES */

extern unsigned short add_binary_table[0x200][0x100];
extern unsigned short subtract_binary_table[0x200][0x100];

unsigned char do_add_binary (unsigned char A, unsigned char arg, unsigned char * C);
unsigned char do_subtract_binary (unsigned char A, unsigned char arg, unsigned char * C);
unsigned char do_add_decimal (unsigned char A, unsigned char arg, unsigned char * C);
unsigned char do_subtract_decimal (unsigned char A, unsigned char arg, unsigned char * C);

#ifdef USE_ARITH_TABLES
#define add_binary(arg) \
do { \
int tmp = A; \
if (C) tmp |= 0x100; \
tmp = add_binary_table[tmp][(arg)]; \
A = (unsigned char)(tmp & 0xff); \
C = (tmp & 0x100) ? 1 : 0; \
N = (A & 0x80) ? 1 : 0; \
Z = A ? 0 : 1; \
} while(0)

#define subtract_binary(arg) \
do { \
int tmp = A; \
if (C) tmp |= 0x100; \
tmp = subtract_binary_table[tmp][(arg)]; \
A = (unsigned char)(tmp & 0xff); \
C = (tmp & 0x100) ? 1 : 0; \
N = (A & 0x80) ? 1 : 0; \
Z = A ? 0 : 1; \
} while(0)
#else
static void add_binary (unsigned char arg)
{
  A = do_add_binary(A, arg, &C);
}

static void subtract_binary (unsigned char arg)
{
  A = do_subtract_binary(A, arg, &C);
}
#endif

static void add_decimal (unsigned char arg)
{
  A = do_add_decimal(A, arg, &C);
  V = C;
  SET_FLAGS_NZ(A);
  cycle_clock++;
}

static void subtract_decimal (unsigned char arg)
{
  A = do_subtract_decimal(A, arg, &C);
  V = C;
  SET_FLAGS_NZ(A);
  cycle_clock++;
}

static inline void  compare (unsigned char  cpureg,
                             unsigned char  arg)
{
  unsigned char result;

  result = cpureg - arg;

  SET_FLAGS_NZ(result);

  C = (cpureg >= arg) ? 1 : 0;
}

static inline unsigned char  build_P (void)
{
  P = 0x20;

  if (N) P |= 0x80;
  if (V) P |= 0x40;
  if (B) P |= 0x10;
  if (D) P |= 0x08;
  if (I) P |= 0x04;
  if (Z) P |= 0x02;
  if (C) P |= 0x01;

  return P;
}

static inline void  unbuild_P (unsigned char  P)
{
  N = (P & 0x80) ? 1 : 0;
  V = (P & 0x40) ? 1 : 0;
  B = (P & 0x10) ? 1 : 0;
  D = (P & 0x08) ? 1 : 0;
  I = (P & 0x04) ? 1 : 0;
  Z = (P & 0x02) ? 1 : 0;
  C = (P & 0x01) ? 1 : 0;

  if (D)
    instruction_table = decimal_instruction_table;
  else
    instruction_table = binary_instruction_table;
}

void  init_cpu (void)
{
  cpu_ptrs.A   = &A;
  cpu_ptrs.X   = &X;
  cpu_ptrs.Y   = &Y;
  cpu_ptrs.S   = &S;
  cpu_ptrs.C   = &C;
  cpu_ptrs.Z   = &Z;
  cpu_ptrs.I   = &I;
  cpu_ptrs.D   = &D;
  cpu_ptrs.B   = &B;
  cpu_ptrs.V   = &V;
  cpu_ptrs.N   = &N;
  cpu_ptrs.irq = &interrupt_flags;
  cpu_ptrs.PC  = &emPC;
}

static inline void  interrupt (unsigned short  vector)
{
  register unsigned char  s_byte;

  s_byte = emPC / 256;
  PUSH(s_byte);
  s_byte = emPC & 0xFF;
  PUSH(s_byte);

  B = 0;
  s_byte = build_P();
  PUSH(s_byte);

  I = 1;
  D = 0;
  instruction_table = binary_instruction_table;

  emPC  = READ(vector+1);
  emPC *= 256;
  emPC += READ(vector);

  cycle_clock += 7;
}

static char            wait_disasm;
static unsigned short  wait_addr;

void prodos_decode (unsigned short  addr)
{
  unsigned short  block_addr;

  wait_disasm = 1;
  wait_addr   = addr + 6;
  block_addr  = READ(addr+5) * 256 + READ(addr+4);

  printf("ProDOS ");

  switch (READ(addr+3))
    {
    case 0x40:
      {
        unsigned short  code;

        printf("ALLOC INTERRUPT\n");
        code = READ(block_addr+3) * 256 + READ(block_addr+2);
        printf("  code: $%4.4X\n", code);
      }
      break;

    case 0x41:
      printf("DEALLOC INTERRUPT\n");
      printf("  num : $%2.2X\n", READ(block_addr+1));
      break;

    case 0x80:
      {
        unsigned short  buf_addr;
        unsigned short  block_num;

        printf("READ BLOCK\n");
        buf_addr  = READ(block_addr+3) * 256 + READ(block_addr+2);
        block_num = READ(block_addr+5) * 256 + READ(block_addr+4);
        printf("  unum: %d\n", READ(block_addr+1));
        printf("  buf : $%4.4X\n", buf_addr);
        printf("  blck: $%4.4X\n", block_num);
      }
      break;

    case 0x81:
      {
        unsigned short  buf_addr;
        unsigned short  block_num;

        printf("WRITE BLOCK\n");
        buf_addr  = READ(block_addr+3) * 256 + READ(block_addr+2);
        block_num = READ(block_addr+5) * 256 + READ(block_addr+4);
        printf("  unum: %d\n", READ(block_addr+1));
        printf("  buf : $%4.4X\n", buf_addr);
        printf("  blck: $%4.4X\n", block_num);
      }
      break;

    case 0xC0:
      {
        int             idx;
        int             pathlen;
        unsigned short  path_ptr;
        unsigned short  ustmp;

        printf("CREATE\n");
        path_ptr = READ(block_addr+2) * 256 + READ(block_addr+1);

        printf("  path: $%4.4X \"", path_ptr);
        pathlen = READ(path_ptr);
        path_ptr++;

        for (idx = 0; idx < pathlen; idx++)
          {
            putchar(READ(path_ptr));
            path_ptr++;
          }

        printf("\"\n");
        printf("  accs: %2.2X\n", READ(block_addr+3));
        printf("  type: %2.2X\n", READ(block_addr+4));

        ustmp = READ(block_addr+6) * 256 + READ(block_addr+5);
        printf("  auxt: %4.4X\n", ustmp);
        printf("  stor: %2.2X\n", READ(block_addr+7));

        ustmp = READ(block_addr+9) * 256 + READ(block_addr+8);
        printf("  date: %4.4X\n", ustmp);
        ustmp = READ(block_addr+11) * 256 + READ(block_addr+10);
        printf("  time: %4.4X\n", ustmp);
      }
      break;

    case 0xC1:
      {
        int             idx;
        int             pathlen;
        unsigned short  path_ptr;

        printf("DESTROY\n");
        path_ptr = READ(block_addr+2) * 256 + READ(block_addr+1);

        printf("  path: $%4.4X \"", path_ptr);
        pathlen = READ(path_ptr);
        path_ptr++;

        for (idx = 0; idx < pathlen; idx++)
          {
            putchar(READ(path_ptr));
            path_ptr++;
          }

        printf("\"\n");
      }
      break;

    case 0xC2:
      {
        int             idx;
        int             pathlen;
        unsigned short  path_ptr;

        printf("RENAME\n");
        path_ptr = READ(block_addr+2) * 256 + READ(block_addr+1);

        printf("  onam: $%4.4X \"", path_ptr);
        pathlen = READ(path_ptr);
        path_ptr++;

        for (idx = 0; idx < pathlen; idx++)
          {
            putchar(READ(path_ptr));
            path_ptr++;
          }

        printf("\"\n");
        path_ptr = READ(block_addr+4) * 256 + READ(block_addr+3);

        printf("  nnam: $%4.4X \"", path_ptr);
        pathlen = READ(path_ptr);
        path_ptr++;

        for (idx = 0; idx < pathlen; idx++)
          {
            putchar(READ(path_ptr));
            path_ptr++;
          }

        printf("\"\n");
      }
      break;

    case 0xC3:
      {
        int             idx;
        int             pathlen;
        unsigned short  path_ptr;
        unsigned short  ustmp;

        printf("SET FILE INFO\n");
        path_ptr = READ(block_addr+2) * 256 + READ(block_addr+1);

        printf("  path: $%4.4X \"", path_ptr);
        pathlen = READ(path_ptr);
        path_ptr++;

        for (idx = 0; idx < pathlen; idx++)
          {
            putchar(READ(path_ptr));
            path_ptr++;
          }

        printf("\"\n");
        printf("  accs: %2.2X\n", READ(block_addr+3));
        printf("  type: %2.2X\n", READ(block_addr+4));

        ustmp = READ(block_addr+6) * 256 + READ(block_addr+5);
        printf("  auxt: %4.4X\n", ustmp);

        ustmp = READ(block_addr+11) * 256 + READ(block_addr+10);
        printf("  date: %4.4X\n", ustmp);
        ustmp = READ(block_addr+13) * 256 + READ(block_addr+12);
        printf("  time: %4.4X\n", ustmp);
      }
      break;

    case 0xC4:
      {
        int             idx;
        int             pathlen;
        unsigned short  path_ptr;

        printf("GET FILE INFO\n");
        path_ptr = READ(block_addr+2) * 256 + READ(block_addr+1);

        printf("  path: $%4.4X \"", path_ptr);
        pathlen = READ(path_ptr);
        path_ptr++;

        for (idx = 0; idx < pathlen; idx++)
          {
            putchar(0x7F & READ(path_ptr));
            path_ptr++;
          }

        printf("\"\n");
      }
      break;

    case 0xC5:
      {
        unsigned short  buf_addr;

        printf("ON LINE\n");
        buf_addr = READ(block_addr+3) * 256 + READ(block_addr+2);
        printf("  unum: %d\n", READ(block_addr+1));
        printf("  buf : $%4.4X\n", buf_addr);
      }
      break;

    case 0xC6:
      {
        int             idx;
        int             pathlen;
        unsigned short  path_ptr;

        printf("SET PREFIX\n");
        path_ptr = READ(block_addr+2) * 256 + READ(block_addr+1);

        printf("  path: $%4.4X \"", path_ptr);
        pathlen = READ(path_ptr);
        path_ptr++;

        for (idx = 0; idx < pathlen; idx++)
          {
            putchar(READ(path_ptr));
            path_ptr++;
          }

        printf("\"\n");
      }
      break;

    case 0xC7:
      {
        unsigned short  buf_addr;

        printf("GET PREFIX\n");
        buf_addr = READ(block_addr+2) * 256 + READ(block_addr+1);
        printf("  buf : $%4.4X\n", buf_addr);
      }
      break;

    case 0xC8:
      {
        int             idx;
        int             pathlen;
        unsigned short  path_ptr;
        unsigned short  buf_addr;

        printf("OPEN\n");
        path_ptr = READ(block_addr+2) * 256 + READ(block_addr+1);
        buf_addr = READ(block_addr+4) * 256 + READ(block_addr+3);

        printf("  path: $%4.4X \"", path_ptr);
        pathlen = READ(path_ptr);
        path_ptr++;

        for (idx = 0; idx < pathlen; idx++)
          {
            putchar(READ(path_ptr));
            path_ptr++;
          }

        printf("\"\n  buf : $%4.4X\n", buf_addr);
      }
      break;

    case 0xC9:
      printf("NEWLINE\n");
      break;

    case 0xCA:
      {
        unsigned short  buf_addr;
        unsigned short  rcnt;

        printf("READ\n");
        buf_addr = READ(block_addr+3) * 256 + READ(block_addr+2);
        rcnt     = READ(block_addr+5) * 256 + READ(block_addr+4);

        printf("  ref : %d\n", READ(block_addr+1));
        printf("  buf : $%4.4X\n", buf_addr);
        printf("  reqc: $%4.4X\n", rcnt);
      }
      wait_disasm = 0;
      break;

    case 0xCB:
      {
        unsigned short  buf_addr;
        unsigned short  rcnt;

        printf("WRITE\n");
        buf_addr = READ(block_addr+3) * 256 + READ(block_addr+2);
        rcnt     = READ(block_addr+5) * 256 + READ(block_addr+4);

        printf("  ref : %d\n", READ(block_addr+1));
        printf("  buf : $%4.4X\n", buf_addr);
        printf("  reqc: $%4.4X\n", rcnt);
      }
      break;

    case 0xCC:
      printf("CLOSE\n");
      printf("  ref : %d\n", READ(block_addr+1));
      break;

    case 0xCD:
      printf("FLUSH\n");
      printf("  ref : %d\n", READ(block_addr+1));
      break;

    case 0xCE:
      {
        unsigned long  pos;

        printf("SET MARK\n");
        printf("  ref : %d\n", READ(block_addr+1));
        pos = READ(block_addr+4) * 65536 + READ(block_addr+3) * 256 + READ(block_addr+2);
        printf("  pos : %ld\n", pos);
      }
      break;

    case 0xCF:
      printf("GET MARK\n");
      printf("  ref : %d\n", READ(block_addr+1));
      break;

    case 0xD0:
      {
        unsigned long  pos;

        printf("SET EOF\n");
        printf("  ref : %d\n", READ(block_addr+1));
        pos = READ(block_addr+4) * 65536 + READ(block_addr+3) * 256 + READ(block_addr+2);
        printf("  eof : %ld\n", pos);
      }
      break;

    case 0xD1:
      printf("GET EOF\n");
      printf("  ref : %d\n", READ(block_addr+1));
      break;

    case 0xD2:
      {
        unsigned short  buf_addr;

        printf("SET BUF\n");
        printf("  ref : %d\n", READ(block_addr+1));
        buf_addr = READ(block_addr+3) * 256 + READ(block_addr+2);
        printf("  buf : $%4.4X\n", buf_addr);
      }
      break;

    case 0xD3:
      printf("GET BUF\n");
      printf("  ref : %d\n", READ(block_addr+1));
      break;

    default:
      printf("?UNKNOWN?\n");
      break;
    }

  putchar('\n');
}

#define DLEN 100
static char  tl [DLEN];
static char  dl [DLEN];

static void  disassemble_data_byte(int             x1,
                                   int             x2,
                                   unsigned short  addr)
{
  unsigned char  mb;

  mb = READ(addr);

  sprintf(dl + x1, "%2.2X", mb);
  dl[x1+2] = ' ';

  mb &= 0x7F;

  if (isprint(mb))
    dl[x2] = mb;
  else
    dl[x2] = '.';
}

static void  disassemble_data (int             x,
                               unsigned short  addr,
                               int             min_bytes,
                               int             max_bytes)
{
  int idx, x1, x2;

  x1 = x;
  x2 = x+13;

  dl[x2++] = '[';

  /* do min_bytes bytes without checking anything */
  for(idx = 0; idx < min_bytes; idx++)
    {
      disassemble_data_byte(x1, x2++, addr++);
      x1 += 3;
    }

  /* do the rest (if any) only if the byte is not a valid opcode */
  for( ; idx < max_bytes; idx++)
    {
      if (0 == instruction_size[READ(addr)])
        {
          disassemble_data_byte(x1, x2++, addr++);
          x1 += 3;
        }
    }

  dl[x2] = ']';
}

void  disassemble_three_bytes (int             x,
                               unsigned short  addr)
{
  unsigned short  operand;

  operand = READ(addr+1) + 256 * READ(addr+2);
  sprintf(tl, instruction_strings[READ(addr)], operand);
  strncpy(dl+x, tl, strlen(tl));
}

void  disassemble_two_bytes (int             x,
                             unsigned short  addr)
{
  unsigned char  opcode;
  unsigned char  operand;

  opcode  = READ(addr);
  operand = READ(addr+1);

  if (instruction_is_branch[opcode])
    { 
      sprintf(tl, instruction_strings[opcode], 0);
      strncpy(dl+x, tl, strlen(tl));

      sprintf(dl+x+5, "$%4.4X", addr + 2 + (signed char)operand);
      dl[x+10] = ' ';
    }
  else
    {
      sprintf(tl, instruction_strings[opcode], operand);
      strncpy(dl+x, tl, strlen(tl));

      if (instruction_is_immediate[opcode])
        {
          operand &= 0x7F;
          if (isprint(operand))
            {
              sprintf(dl+x+15, "['%c']", operand);
              dl[x+20] = ' ';
            }
        }
    }
}

void  disassemble_one_byte (int             x,
                            unsigned short  addr)
{
  unsigned char  mb;

  mb = READ(addr);

  sprintf(tl, instruction_strings[mb]);
  strncpy(dl+x, tl, strlen(tl));
}

void  disassemble_line (unsigned short  addr)
{
  unsigned char  mb;
  int            idx;

  for (idx = 0; idx < DLEN; idx++)
    dl[idx] = ' ';

  sprintf(dl, "%4.4X:", addr);
  dl[5] = ' ';
  mb = READ(addr);

  if (3 == instruction_size[mb])
    {
      /* Handle three byte instructions */
      sprintf(dl+7, "%2.2X %2.2X %2.2X", mb, READ(addr+1), READ(addr+2));
      dl[15] = ' ';
      disassemble_three_bytes(20, addr);
    }
  else if (2 == instruction_size[mb])
    {
      /* Handle two byte instructions */
      sprintf(dl+7, "%2.2X %2.2X", mb, READ(addr+1));
      dl[12] = ' ';
      disassemble_two_bytes(20, addr);
    }
  else if (1 == instruction_size[mb])
    {
      /* Handle one byte instructions */
      sprintf(dl+7, "%2.2X", mb);
      dl[9] = ' ';
      disassemble_one_byte(20, addr);
    }
  else
    {
      /* treat non-opcodes as data, print up to four on a line */
      disassemble_data(7, addr, 1, 4);
    }

  sprintf(dl+45, "A:%2.2X  X:%2.2X  Y:%2.2X  S:%2.2X  NV-BDIZC:%d%d-%d%d%d%d%d\n",
          A, X, Y, S, N, V, B, D, I, Z, C);

  printf(dl);
}

void  disassemble (unsigned short  addr)
{
  unsigned char   opcode;
  unsigned short  operand;

  if (wait_disasm)
    {
      if (addr == wait_addr)
        wait_disasm = 0;
      else
        return;
    }

  opcode = READ(addr);

  /*
  if (0x91 == opcode)
    {
      unsigned short target;

      operand = READ(addr+1);
      target = READ(operand) + (256 * READ(operand+1));

      printf("STA ($%2.2x),Y {$%2.2x => $%4.4x}\n", operand, A, target + Y);
    }

  return;
  */

  if (0x6C == opcode)
    {
      unsigned short target;
      /*
      disassemble_line(addr);
      */
      target = READ(addr+1) + (256 * READ(addr+2));
      operand = READ(target) + (256 * READ(target+1));

      if (0xC65C == operand)
        {
          int sector = READ(0x3D);
          int dest = READ(0x27) * 256;

          wait_disasm = 1;
          wait_addr = 0x801;

          printf("Disk ][ Prom {Read Sector %d to $%4.4x}\n", sector, dest);
        }

      return;
    }
  else if (0x20 == opcode)
    {
      /*
      disassemble_line(addr);
      */
      operand = READ(addr+1) + (256 * READ(addr+2));

      if (0xBF00 == operand)
        {
          opcode  = READ(operand);
          operand = READ(operand+1) + (256 * READ(operand+2));

          if (0x4C == opcode && 0xBF4B == operand)
            prodos_decode(addr);
        }
      /*
      else if (0xBD00 == operand)
        {
          wait_disasm = 1;
          wait_addr   = addr + 3;

          printf("RWTS (Read/Write Track/Sector)\n");
        }
      */
      else if (0xC300 == operand)
        {
          wait_disasm = 1;
          wait_addr   = addr + 3;

          printf("Init 80-column text screen\n");
        }
      else if (0xFDED == operand)
        {
          wait_disasm = 1;
          wait_addr   = addr + 3;

          printf("COUT (output a character)\n");
        }
      else if (0xFB2F == operand)
        {
          /* wait_disasm = 1; */
          wait_addr   = addr + 3;

          printf("INIT (40 column text screen)\n");
        }
      else if (0xFB39 == operand)
        {
          wait_disasm = 1;
          wait_addr   = addr + 3;

          printf("SETTXT (switch to text mode)\n");
        }
      else if (0xFD8E == operand)
        {
          wait_disasm = 1;
          wait_addr   = addr + 3;

          printf("CROUT (output a newline)\n");
        }
      else if (0xFD6A == operand)
        {
          wait_disasm = 1;
          wait_addr   = addr + 3;

          printf("GETLN (input a string)\n");
        }
      else if (0xFC58 == operand)
        {
          wait_disasm = 1;
          wait_addr   = addr + 3;

          printf("HOME (clear the screen)\n");
        }
      else if (0xFD0C == operand)
        {
          wait_disasm = 1;
          wait_addr   = addr + 3;

          printf("RDKEY (input a character)\n");
        }
      else if (0xC311 == operand)
        {
          if (0 == switch_slotcxrom || 0 == switch_slotc3rom)
            {
              unsigned short  from;
              unsigned short  len;
              unsigned short  to;

              wait_disasm = 1;
              wait_addr   = addr + 3;

              from = READ(0x3C) + (256 * READ(0x3D));
              len  = READ(0x3E) + (256 * READ(0x3F));
              to   = READ(0x42) + (256 * READ(0x43));

              len = len - from + 1;

              if (C)
                printf("MOVE main->aux  from:%4.4X  to:%4.4X  len:%4.4X\n", from, to, len);
              else
                printf("MOVE aux->main  from:%4.4X  to:%4.4X  len:%4.4X\n", from, to, len);
            }
        }

      return;
    }
  /*
  disassemble_line(addr);
  */
}

/* Main CPU fetch/decode/execute loop */

long num_inst = 0;

void execute_one (void)
{
  int  icount;
  int  call_idx;
  int  slot_idx;

  icount = 16384;

  while (icount--)
    {
      for (slot_idx = 0; slot_idx < i_delay; slot_idx++)
        thumb = 1 - thumb;

      /* check for interrupts */

      if (interrupt_flags)
        {
          if (0 == I && interrupt_flags & F_IRQ)
            {
              interrupt(0xfffe);
              interrupt_flags &= ~F_IRQ;
            }
          else if (interrupt_flags & F_NMI)
            {
              interrupt(0xfffa);
              interrupt_flags &= ~F_NMI;
            }
          else if (interrupt_flags & F_RESET)
            {
              interrupt(0xfffc);
              interrupt_flags &= ~F_RESET;
            }
        }

#if 0
      /* debugger, if option is on */
      if (em65_debug) debugger_step();
#endif

      /* might need to call a card if the internal ROM isn't switched in */
      if (switch_slotcxrom)
        {
          /* is the PC somewhere in the slot space (0xC100-0xC7FF)? */
          if (0xC000 == (emPC & 0xF800))
            {
              /* step through 'registered' cards */
              for (call_idx = 0; call_idx < last_slot_call; call_idx++)
                {
                  slot_idx = slot_calls[call_idx];

                  /* for slot 3 have to check the internal ROM again */
                  if (3 != slot_idx || switch_slotc3rom)
                    {
                      /* is the next instruction in the card's ROM space? */
                      if ((emPC / 256) == (0xC0 + slot_idx))
                        {
                          slot[slot_idx].call();
                          goto next_instruction;
                        }
                    }
                }
            }
        }

      /* fetch, decode, and execute and instruction */
      opcode = READ(emPC);

#ifdef ENABLE_TRACE
      if (trace)
        {
          --trace;
          disassemble(emPC);
        }
#endif

      emPC++;
      instruction_table[opcode]();

      cycle_clock += instruction_cycles[opcode];

    next_instruction:
      {
        static unsigned long sample_clock = 0;
        static unsigned long old_cycle_clock = 0;

        /* update raster */
        {
          static int old_raster_row = 0;
          static int old_raster_dot = 0;

          static int raster_cycles = 0;

          static int frame_skip = 0;

          int raster_idx;
          int screen_dot;
          int screen_row;

          raster_cycles = (14 * (cycle_clock - old_cycle_clock));

          if (frame_skip) goto update_counters;

          /* beam is in the space below the screen - no chance to draw stuff */
          if (raster_row >= (SCREEN_BORDER_HEIGHT + SCREEN_HEIGHT)) goto update_counters;

          /* beam is in the space above the screen and won't wrap into the screen */
          if (raster_row < (SCREEN_BORDER_HEIGHT - SCANLINE_INCR)) goto update_counters;

          /*
           * look for cases where there won't be any screen updating
           */

          /* beam is in the line above the screen and might wrap */
          if (raster_row == (SCREEN_BORDER_HEIGHT - SCANLINE_INCR))
            {
              /* wrap and see if all the time is gone */
              raster_idx = (SCREEN_BORDER_WIDTH + SCAN_WIDTH - raster_dot);
              if (raster_idx >= raster_cycles) goto update_counters;
              screen_dot = 0;
              screen_row = 0;
            }
          /* beam is left of the screen and might reach the left border */
          else if (raster_dot < SCREEN_BORDER_WIDTH)
            {
              /* find the left border and see if all the time is gone */
              raster_idx = (SCREEN_BORDER_WIDTH - raster_dot);
              if (raster_idx >= raster_cycles) goto update_counters;
              screen_dot = 0;
              screen_row = raster_row - SCREEN_BORDER_HEIGHT;
            }
          /* beam is right of the screen and might wrap to the next line */
          else if (raster_dot >= (SCREEN_BORDER_WIDTH + SCREEN_WIDTH))
            {
              /* wrap, cross the border and see if all the time is gone */
              raster_idx = (SCREEN_BORDER_WIDTH + SCAN_WIDTH - raster_dot);
              if (raster_idx >= raster_cycles) goto update_counters;
              screen_dot = 0;
              screen_row = raster_row - SCREEN_BORDER_HEIGHT + SCANLINE_INCR;
              /* just in case the beam wrapped off the bottom of the screen */
              if (screen_row >= SCREEN_HEIGHT) goto update_counters;
            }
          /* beam is in the screen */
          else
            {
              raster_idx = 0;
              screen_dot = raster_dot - SCREEN_BORDER_WIDTH;
              screen_row = raster_row - SCREEN_BORDER_HEIGHT;
            }

          if (0 == (screen_row & 1))
            rasterizer(raster_cycles, raster_idx, screen_dot, screen_row);

        update_counters:

          raster_dot += raster_cycles;

          /* Update raster counters */
          if (raster_dot >= SCAN_WIDTH)
            {
              raster_dot -= SCAN_WIDTH;
              raster_row += SCANLINE_INCR;

              if (raster_row >= SCAN_HEIGHT)
                {
                  int slot_idx;

                  raster_row -= SCAN_HEIGHT;
                  if (++frame_skip >= param_frame_skip) frame_skip = 0;
#if 1
                  gtk_widget_draw(screen_da, &(my_GTK.screen_rect));
                  RESET_SCREEN_RECT();
#endif
                  for (slot_idx = 0; slot_idx < last_slot_time; slot_idx++)
                    slot[slot_times[slot_idx]].time();
                }
            }

          /* Generate VBL */
          if (raster_row < SCREEN_BORDER_HEIGHT || raster_row >= (SCREEN_BORDER_HEIGHT + SCREEN_HEIGHT))
            apple_vbl = 0x00;
          else
            apple_vbl = 0x80;

          old_raster_dot = raster_dot;
          old_raster_row = raster_row;
        }

        /* update speaker */
        {
          sample_clock += (100 * (cycle_clock - old_cycle_clock));
          if (sample_clock >= 4644)
            {
              sample_clock -= 4644;
              sample_buf[sample_idx++] = speaker_state;
            }
        }

        old_cycle_clock = cycle_clock;
      }
    }
}

/* 65c02 instructions */

void  i00_BRK (void)
{
  register unsigned char  s_byte;

  emPC++;

  s_byte = emPC / 256;
  PUSH(s_byte);
  s_byte = emPC & 0xFF;
  PUSH(s_byte);
    
  B = 1;
  s_byte = build_P();
  PUSH(s_byte);

  I = 1;
  D = 0;
  instruction_table = binary_instruction_table;

  emPC  = READ(0xFFFF);
  emPC *= 256;
  emPC += READ(0xFFFE);
}

void  i01_ORA (void)
{
  register unsigned char   zploc_l;
  register unsigned char   zploc_h;
  register unsigned short  ea;

  /* Indirect , X */

  zploc_l = X + READ(emPC);
  zploc_h = 1 + zploc_l;

#ifdef ZPI_BOUNDS
  if (zploc_l == 0xFF)
    printf("warning: zero page indirect crosses page boundary at $%4.4X\n", emPC-1);
#endif

  ea = READ_ZP(zploc_h);
  ea <<= 8;
  ea |= READ_ZP(zploc_l);

  A |= READ(ea);
  SET_FLAGS_NZ(A);
  emPC++;
}

void  i04_TSB (void)
{
  register unsigned char  zploc;
  register unsigned char  zpbyte;

  /* Zero Page */

  zploc  = READ(emPC);
  zpbyte = READ_ZP(zploc);

  Z = (A & zpbyte) ? 0 : 1;

  zpbyte |= A;
  WRITE_ZP(zploc, zpbyte);

  emPC++;
}

void  i05_ORA (void)
{
  register unsigned char  zploc;

  /* Zero Page */

  zploc = READ(emPC);
  A |= READ_ZP(zploc);

  SET_FLAGS_NZ(A);
  emPC++;
}

void  i06_ASL (void)
{
  register unsigned char  zploc;
  register unsigned char  zpbyte;

  /* Zero Page */

  zploc  = READ(emPC);
  zpbyte = READ_ZP(zploc);

  C = (zpbyte & 0x80) ? 1 : 0;
  zpbyte <<= 1;
    
  SET_FLAGS_NZ(zpbyte);
  WRITE_ZP(zploc, zpbyte);
  emPC++;
}

void  i08_PHP (void)
{
  register unsigned char  status;

  status = build_P();
  PUSH(status);
}

void  i09_ORA (void)
{
  /* Immediate */

  A |= READ(emPC);
  SET_FLAGS_NZ(A);
  emPC++;
}

void  i0A_ASL (void)
{
  /* Accumulator */

  C = (A & 0x80) ? 1 : 0;
  A <<= 1;
  SET_FLAGS_NZ(A);
}

void  i0C_TSB (void)
{
  register unsigned char   membyte;
  register unsigned short  ea;

  /* Absolute */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  membyte = READ(ea);

  Z = (A & membyte) ? 0 : 1;
  membyte |= A;
  WRITE(ea, membyte);

  emPC += 2;
}

void  i0D_ORA (void)
{
  register unsigned short  ea;

  /* Absolute */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  A |= READ(ea);
  SET_FLAGS_NZ(A);
  emPC += 2;
}

void  i0E_ASL (void)
{
  register unsigned char   membyte;
  register unsigned short  ea;

  /* Absolute */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  membyte = READ(ea);

  C = (membyte & 0x80) ? 1 : 0;
  membyte <<= 1;
  SET_FLAGS_NZ(membyte);
  WRITE(ea, membyte);

  emPC += 2;
}

void  i10_BPL (void)
{
  if (N)
    emPC++;
  else
    BRANCH;
}

void  i11_ORA (void)
{
  register unsigned char   zploc_l;
  register unsigned char   zploc_h;
  register unsigned short  base;
  register unsigned short  ea;

  /* Indirect , Y */

  zploc_l = READ(emPC);
  zploc_h = 1 + zploc_l;

#ifdef ZPI_BOUNDS
  if (zploc_l == 0xFF)
    printf("warning: zero page indirect crosses page boundary at $%4.4X\n", emPC-1);
#endif

  ea = READ_ZP(zploc_h);
  ea <<= 8;
  ea |= READ_ZP(zploc_l);

  base = ea;
  ea += (unsigned short)Y;
  if ((base & 0xFF00) != (ea & 0xFF00)) cycle_clock++;

  A |= READ(ea);
  SET_FLAGS_NZ(A);
  emPC++;
}

void  i12_ORA (void)
{
  register unsigned char   zploc_l;
  register unsigned char   zploc_h;
  register unsigned short  ea;

  /* Zero Page Indirect */

  zploc_l = READ(emPC);
  zploc_h = 1 + zploc_l;

#ifdef ZPI_BOUNDS
  if (zploc_l == 0xFF)
    printf("warning: zero page indirect crosses page boundary at $%4.4X\n", emPC-1);
#endif

  ea = READ_ZP(zploc_h);
  ea <<= 8;
  ea |= READ_ZP(zploc_l);

  A |= READ(ea);
  SET_FLAGS_NZ(A);
  emPC++;
}

void  i14_TRB (void)
{
  register unsigned char  zploc;
  register unsigned char  zpbyte;

  /* Zero Page */

  zploc  = READ(emPC);
  zpbyte = READ_ZP(zploc);

  Z = (A & zpbyte) ? 0 : 1;

  zpbyte &= (~A);
  WRITE_ZP(zploc, zpbyte);
  emPC++;
}

void  i15_ORA (void)
{
  register unsigned char  zploc;

  /* Zero Page , X */

  zploc = READ(emPC) + X;
  A |= READ_ZP(zploc);

  SET_FLAGS_NZ(A);
  emPC++;
}

void  i16_ASL (void)
{
  register unsigned char  zploc;
  register unsigned char  zpbyte;

  /* Zero Page , X */

  zploc  = READ(emPC) + X;
  zpbyte = READ_ZP(zploc);

  C = (zpbyte & 0x80) ? 1 : 0;
  zpbyte <<= 1;

  SET_FLAGS_NZ(zpbyte);
  WRITE_ZP(zploc, zpbyte);
  emPC++;
}

void  i18_CLC (void)
{
  C = 0;
}

void  i19_ORA (void)
{
  register unsigned char   membyte;
  register unsigned short  base;
  register unsigned short  ea;

  /* Absolute , Y */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  base = ea;
  ea += (unsigned short)Y;
  if ((base & 0xFF00) != (ea & 0xFF00)) cycle_clock++;

  membyte = READ(ea);
  A |= membyte;
  SET_FLAGS_NZ(A);

  emPC += 2;
}

void  i1A_INC (void)
{
  /* Accumulator */

  A++;
  SET_FLAGS_NZ(A);
}

void  i1C_TRB (void)
{
  register unsigned char   membyte;
  register unsigned short  ea;

  /* Absolute */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  membyte = READ(ea);

  Z = (A & membyte) ? 0 : 1;
  membyte &= (~A);
  WRITE(ea, membyte);

  emPC += 2;
}

void  i1D_ORA (void)
{
  register unsigned char   membyte;
  register unsigned short  base;
  register unsigned short  ea;

  /* Absolute , X */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  base = ea;
  ea += (unsigned short)X;
  if ((base & 0xFF00) != (ea & 0xFF00)) cycle_clock++;

  membyte = READ(ea);
  A |= membyte;
  SET_FLAGS_NZ(A);

  emPC += 2;
}

void  i1E_ASL (void)
{
  register unsigned char   membyte;
  register unsigned short  ea;

  /* Absolute , X */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);
  ea += (unsigned short)X;

  membyte = READ(ea);

  C = (membyte & 0x80) ? 1 : 0;
  membyte <<= 1;

  SET_FLAGS_NZ(membyte);
  WRITE(ea, membyte);
  emPC += 2;
}

void  i20_JSR (void)
{
  register unsigned char   s_byte;
  register unsigned short  ea;

  s_byte = (emPC+1) / 256;
  PUSH(s_byte);
  s_byte = (emPC+1) & 0xFF;
  PUSH(s_byte);

  /* Absolute */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  emPC = ea;
}

void  i21_AND (void)
{
  register unsigned char   zploc_l;
  register unsigned char   zploc_h;
  register unsigned short  ea;

  /* Indirect , X */

  zploc_l = X + READ(emPC);
  zploc_h = 1 + zploc_l;

#ifdef ZPI_BOUNDS
  if (zploc_l == 0xFF)
    printf("warning: zero page indirect crosses page boundary at $%4.4X\n", emPC-1);
#endif

  ea = READ_ZP(zploc_h);
  ea <<= 8;
  ea |= READ_ZP(zploc_l);

  A &= READ(ea);
  SET_FLAGS_NZ(A);
  emPC++;
}

void  i24_BIT (void)
{
  register unsigned char  zploc;
  register unsigned char  zpbyte;

  /* Zero Page */

  zploc  = READ(emPC);
  zpbyte = READ_ZP(zploc);

  V = (zpbyte & 0x40) ? 1 : 0;
  N = (zpbyte & 0x80) ? 1 : 0;
  Z = (zpbyte & A)    ? 0 : 1;

  emPC++;
}

void  i25_AND (void)
{
  register unsigned char  zploc;

  /* Zero Page */

  zploc = READ(emPC);
  A &= READ_ZP(zploc);

  SET_FLAGS_NZ(A);
  emPC++;
}

void  i26_ROL (void)
{
  register unsigned char  zploc;
  register unsigned char  zpbyte;
  register unsigned char  result;

  /* Zero Page */

  zploc  = READ(emPC);
  zpbyte = READ_ZP(zploc);

  result = (zpbyte << 1) | C;
  C = (zpbyte & 0x80) ? 1 : 0;

  SET_FLAGS_NZ(result);
  WRITE_ZP(zploc, result);
  emPC++;
}

void  i28_PLP (void)
{
  register unsigned char  status;

  status = PULL();
  unbuild_P(status);
}

void  i29_AND (void)
{
  /* Immediate */

  A &= READ(emPC);
  SET_FLAGS_NZ(A);
  emPC++;
}

void  i2A_ROL (void)
{
  register unsigned char  result;

  /* Accumulator */

  result = (A << 1) | C;
  C = (A & 0x80) ? 1 : 0;
  SET_FLAGS_NZ(result);
  A = result;
}

void  i2C_BIT (void)
{
  register unsigned char   membyte;
  register unsigned short  ea;

  /* Absolute */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  membyte = READ(ea);

  V = (membyte & 0x40) ? 1 : 0;
  N = (membyte & 0x80) ? 1 : 0;
  Z = (membyte & A)    ? 0 : 1;

  emPC += 2;
}

void  i2D_AND (void)
{
  register unsigned short  ea;

  /* Absolute */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  A &= READ(ea);
  SET_FLAGS_NZ(A);

  emPC += 2;
}

void  i2E_ROL (void)
{
  register unsigned char   result;
  register unsigned char   membyte;
  register unsigned short  ea;

  /* Absolute */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  membyte = READ(ea);

  result = (membyte << 1) | C;
  C = (membyte & 0x80) ? 1 : 0;
  SET_FLAGS_NZ(result);
  WRITE(ea, result);

  emPC += 2;
}

void  i30_BMI (void)
{
  if (N)
    BRANCH;
  else
    emPC++;
}

void  i31_AND (void)
{
  register unsigned char   zploc_l;
  register unsigned char   zploc_h;
  register unsigned short  base;
  register unsigned short  ea;

  /* Indirect , Y */

  zploc_l = READ(emPC);
  zploc_h = 1 + zploc_l;

#ifdef ZPI_BOUNDS
  if (zploc_l == 0xFF)
    printf("warning: zero page indirect crosses page boundary at $%4.4X\n", emPC-1);
#endif

  ea = READ_ZP(zploc_h);
  ea <<= 8;
  ea |= READ_ZP(zploc_l);

  base = ea;
  ea += (unsigned short)Y;
  if ((base & 0xFF00) != (ea & 0xFF00)) cycle_clock++;

  A &= READ(ea);
  SET_FLAGS_NZ(A);
  emPC++;
}

void  i32_AND (void)
{
  register unsigned char   zploc_l;
  register unsigned char   zploc_h;
  register unsigned short  ea;

  /* Zero Page Indirect */

  zploc_l = READ(emPC);
  zploc_h = 1 + zploc_l;

#ifdef ZPI_BOUNDS
  if (zploc_l == 0xFF)
    printf("warning: zero page indirect crosses page boundary at $%4.4X\n", emPC-1);
#endif

  ea = READ_ZP(zploc_h);
  ea <<= 8;
  ea |= READ_ZP(zploc_l);

  A &= READ(ea);
  SET_FLAGS_NZ(A);
  emPC++;
}

void  i34_BIT (void)
{
  register unsigned char  zploc;
  register unsigned char  zpbyte;

  /* Zero Page , X */

  zploc  = READ(emPC) + X;
  zpbyte = READ_ZP(zploc);

  V = (zpbyte & 0x40) ? 1 : 0;
  N = (zpbyte & 0x80) ? 1 : 0;
  Z = (zpbyte & A)    ? 0 : 1;

  emPC++;
}

void  i35_AND (void)
{
  register unsigned char  zploc;

  /* Zero Page , X */

  zploc = READ(emPC) + X;
  A &= READ_ZP(zploc);

  SET_FLAGS_NZ(A);
  emPC++;
}

void  i36_ROL (void)
{
  register unsigned char  zploc;
  register unsigned char  zpbyte;
  register unsigned char  result;

  /* Zero Page , X */

  zploc  = READ(emPC) + X;
  zpbyte = READ_ZP(zploc);

  result = (zpbyte << 1) | C;
  C = (zpbyte & 0x80) ? 1 : 0;

  SET_FLAGS_NZ(result);
  WRITE_ZP(zploc, result);
  emPC++;
}

void  i38_SEC (void)
{
  C = 1;
}

void  i39_AND (void)
{
  register unsigned char   membyte;
  register unsigned short  base;
  register unsigned short  ea;

  /* Absolute , Y */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  base = ea;
  ea += (unsigned short)Y;
  if ((base & 0xFF00) != (ea & 0xFF00)) cycle_clock++;

  membyte = READ(ea);
  A &= membyte;
  SET_FLAGS_NZ(A);

  emPC += 2;
}

void  i3A_DEC (void)
{
  /* Accumulator */

  A--;
  SET_FLAGS_NZ(A);
}

void  i3C_BIT (void)
{
  register unsigned char   membyte;
  register unsigned short  base;
  register unsigned short  ea;

  /* Absolute , X */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  base = ea;
  ea += (unsigned short)X;
  if ((base & 0xFF00) != (ea & 0xFF00)) cycle_clock++;

  membyte = READ(ea);

  V = (membyte & 0x40) ? 1 : 0;
  N = (membyte & 0x80) ? 1 : 0;
  Z = (membyte & A)    ? 0 : 1;

  emPC += 2;
}

void  i3D_AND (void)
{
  register unsigned char   membyte;
  register unsigned short  base;
  register unsigned short  ea;

  /* Absolute , X */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  base = ea;
  ea += (unsigned short)X;
  if ((base & 0xFF00) != (ea & 0xFF00)) cycle_clock++;

  membyte = READ(ea);
  A &= membyte;
  SET_FLAGS_NZ(A);

  emPC += 2;
}

void  i3E_ROL (void)
{
  register unsigned char   result;
  register unsigned char   membyte;
  register unsigned short  ea;

  /* Absolute , X */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);
  ea += (unsigned short)X;

  membyte = READ(ea);

  result = (membyte << 1) | C;
  C = (membyte & 0x80) ? 1 : 0;
    
  SET_FLAGS_NZ(result);
  WRITE(ea, result);
  emPC += 2;
}

void  i40_RTI (void)
{
  register unsigned char  status;

  status = PULL();
  unbuild_P(status);

  emPC  = PULL();
  emPC += (256 * PULL());
}

void  i41_EOR (void)
{
  register unsigned char   zploc_l;
  register unsigned char   zploc_h;
  register unsigned short  ea;

  /* Indirect , X */

  zploc_l = X + READ(emPC);
  zploc_h = 1 + zploc_l;

#ifdef ZPI_BOUNDS
  if (zploc_l == 0xFF)
    printf("warning: zero page indirect crosses page boundary at $%4.4X\n", emPC-1);
#endif

  ea = READ_ZP(zploc_h);
  ea <<= 8;
  ea |= READ_ZP(zploc_l);

  A ^= READ(ea);
  SET_FLAGS_NZ(A);
  emPC++;
}

void  i45_EOR (void)
{
  register unsigned char  zploc;

  /* Zero Page */

  zploc = READ(emPC);
  A ^= READ_ZP(zploc);

  SET_FLAGS_NZ(A);
  emPC++;
}

void  i46_LSR (void)
{
  register unsigned char  zploc;
  register unsigned char  zpbyte;

  /* Zero Page */

  zploc  = READ(emPC);
  zpbyte = READ_ZP(zploc);

  C = zpbyte & 0x01;
  zpbyte >>= 1;

  N = 0;
  Z = zpbyte ? 0 : 1;

  WRITE_ZP(zploc, zpbyte);
  emPC++;
}

void  i48_PHA (void)
{
  PUSH(A);
}

void  i49_EOR (void)
{
  /* Immediate */

  A ^= READ(emPC);
  SET_FLAGS_NZ(A);
  emPC++;
}

void  i4A_LSR (void)
{
  /* Accumulator */

  C = A & 0x01;
  A >>= 1;

  N = 0;
  Z = A ? 0 : 1;
}

void  i4C_JMP (void)
{
  register unsigned short  ea;

  /* Absolute */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  emPC = ea;
}

void  i4D_EOR (void)
{
  register unsigned short  ea;

  /* Absolute */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  A ^= READ(ea);
  SET_FLAGS_NZ(A);

  emPC += 2;
}

void  i4E_LSR (void)
{
  register unsigned char   membyte;
  register unsigned short  ea;

  /* Absolute */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  membyte = READ(ea);

  C = membyte & 0x01;
  membyte >>= 1;

  N = 0;
  Z = membyte ? 0 : 1;

  WRITE(ea, membyte);
  emPC += 2;
}

void  i50_BVC (void)
{
  if (V)
    emPC++;
  else
    BRANCH;
}

void  i51_EOR (void)
{
  register unsigned char   zploc_l;
  register unsigned char   zploc_h;
  register unsigned short  base;
  register unsigned short  ea;

  /* Indirect , Y */

  zploc_l = READ(emPC);
  zploc_h = 1 + zploc_l;

#ifdef ZPI_BOUNDS
  if (zploc_l == 0xFF)
    printf("warning: zero page indirect crosses page boundary at $%4.4X\n", emPC-1);
#endif

  ea = READ_ZP(zploc_h);
  ea <<= 8;
  ea |= READ_ZP(zploc_l);

  base = ea;
  ea += (unsigned short)Y;
  if ((base & 0xFF00) != (ea & 0xFF00)) cycle_clock++;

  A ^= READ(ea);
  SET_FLAGS_NZ(A);
  emPC++;
}

void  i52_EOR (void)
{
  register unsigned char   zploc_l;
  register unsigned char   zploc_h;
  register unsigned short  ea;

  /* Zero Page Indirect */

  zploc_l = READ(emPC);
  zploc_h = 1 + zploc_l;

#ifdef ZPI_BOUNDS
  if (zploc_l == 0xFF)
    printf("warning: zero page indirect crosses page boundary at $%4.4X\n", emPC-1);
#endif

  ea = READ_ZP(zploc_h);
  ea <<= 8;
  ea |= READ_ZP(zploc_l);

  A ^= READ(ea);
  SET_FLAGS_NZ(A);
  emPC++;
}

void  i55_EOR (void)
{
  register unsigned char  zploc;

  /* Zero Page , X */

  zploc = READ(emPC) + X;
  A ^= READ_ZP(zploc);

  SET_FLAGS_NZ(A);
  emPC++;
}

void  i56_LSR (void)
{
  register unsigned char  zploc;
  register unsigned char  zpbyte;

  /* Zero Page , X */

  zploc  = READ(emPC) + X;
  zpbyte = READ_ZP(zploc);

  C = zpbyte & 0x01;
  zpbyte >>= 1;

  N = 0;
  Z = zpbyte ? 0 : 1;

  WRITE_ZP(zploc, zpbyte);
  emPC++;
}

void  i58_CLI (void)
{
  I = 0;
}

void  i59_EOR (void)
{
  register unsigned char   membyte;
  register unsigned short  base;
  register unsigned short  ea;

  /* Absolute , Y */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  base = ea;
  ea += (unsigned short)Y;
  if ((base & 0xFF00) != (ea & 0xFF00)) cycle_clock++;

  membyte = READ(ea);
  A ^= membyte;
  SET_FLAGS_NZ(A);

  emPC += 2;
}

void  i5A_PHY (void)
{
  PUSH(Y);
}

void  i5D_EOR (void)
{
  register unsigned char   membyte;
  register unsigned short  base;
  register unsigned short  ea;

  /* Absolute , X */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  base = ea;
  ea += (unsigned short)X;
  if ((base & 0xFF00) != (ea & 0xFF00)) cycle_clock++;

  membyte = READ(ea);
  A ^= membyte;
  SET_FLAGS_NZ(A);

  emPC += 2;
}

void  i5E_LSR (void)
{
  register unsigned char   membyte;
  register unsigned short  ea;

  /* Absolute , X */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);
  ea += (unsigned short)X;

  membyte = READ(ea);

  C = membyte & 0x01;
  membyte >>= 1;

  N = 0;
  Z = membyte ? 0 : 1;

  WRITE(ea, membyte);
  emPC += 2;
}

void  i60_RTS (void)
{
  emPC  = PULL();
  emPC += (256 * PULL());
  emPC++;
}

void  i61_ADC_binary (void)
{
  register unsigned char   zploc_l;
  register unsigned char   zploc_h;
  register unsigned char   membyte;
  register unsigned short  ea;

  /* Indirect , X */

  zploc_l = X + READ(emPC);
  zploc_h = 1 + zploc_l;

#ifdef ZPI_BOUNDS
  if (zploc_l == 0xFF)
    printf("warning: zero page indirect crosses page boundary at $%4.4X\n", emPC-1);
#endif

  ea = READ_ZP(zploc_h);
  ea <<= 8;
  ea |= READ_ZP(zploc_l);

  membyte = READ(ea);
  emPC++;
  add_binary(membyte);
}

void  i61_ADC_decimal (void)
{
  register unsigned char   zploc_l;
  register unsigned char   zploc_h;
  register unsigned char   membyte;
  register unsigned short  ea;

  /* Indirect , X */

  zploc_l = X + READ(emPC);
  zploc_h = 1 + zploc_l;

#ifdef ZPI_BOUNDS
  if (zploc_l == 0xFF)
    printf("warning: zero page indirect crosses page boundary at $%4.4X\n", emPC-1);
#endif

  ea = READ_ZP(zploc_h);
  ea <<= 8;
  ea |= READ_ZP(zploc_l);

  membyte = READ(ea);
  emPC++;
  add_decimal(membyte);
}

void  i64_STZ (void)
{
  register unsigned char  zploc;

  /* Zero Page */

  zploc = READ(emPC);
  WRITE_ZP(zploc, 0);
  emPC++;
}

void  i65_ADC_binary (void)
{
  register unsigned char  zploc;
  register unsigned char  zpbyte;

  /* Zero Page */

  zploc  = READ(emPC);
  zpbyte = READ_ZP(zploc);

  emPC++;

  add_binary(zpbyte);
}

void  i65_ADC_decimal (void)
{
  register unsigned char  zploc;
  register unsigned char  zpbyte;

  /* Zero Page */

  zploc  = READ(emPC);
  zpbyte = READ_ZP(zploc);

  emPC++;

  add_decimal(zpbyte);
}

void  i66_ROR (void)
{
  register unsigned char  zploc;
  register unsigned char  zpbyte;
  register unsigned char  result;

  /* Zero Page */

  zploc  = READ(emPC);
  zpbyte = READ_ZP(zploc);

  result = zpbyte >> 1;
  if (C) result |= 0x80;

  C = zpbyte & 0x01;

  SET_FLAGS_NZ(result);
  WRITE_ZP(zploc, result);

  emPC++;
}

void  i68_PLA (void)
{
  A = PULL();
  SET_FLAGS_NZ(A);
}

void  i69_ADC_binary (void)
{
  register unsigned char  membyte;

  /* Immediate */

  membyte = READ(emPC);
  emPC++;
  add_binary(membyte);
}

void  i69_ADC_decimal (void)
{
  register unsigned char  membyte;

  /* Immediate */

  membyte = READ(emPC);
  emPC++;
  add_decimal(membyte);
}

void  i6A_ROR (void)
{
  register unsigned char result;

  /* Accumulator */

  result = A >> 1;
  if (C) result |= 0x80;

  C = A & 0x01;
  SET_FLAGS_NZ(result);
  A = result;
}

void  i6C_JMP (void)
{
  register unsigned short  ea;

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  emPC  = READ(ea + 1);
  emPC *= 256;
  emPC += READ(ea);
}

void  i6D_ADC_binary (void)
{
  register unsigned char   membyte;
  register unsigned short  ea;

  /* Absolute */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  membyte = READ(ea);
  emPC += 2;

  add_binary(membyte);
}

void  i6D_ADC_decimal (void)
{
  register unsigned char   membyte;
  register unsigned short  ea;

  /* Absolute */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  membyte = READ(ea);
  emPC += 2;

  add_decimal(membyte);
}

void  i6E_ROR (void)
{
  register unsigned char   result;
  register unsigned char   membyte;
  register unsigned short  ea;

  /* Absolute */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  membyte = READ(ea);

  result = membyte >> 1;
  if (C) result |= 0x80;

  C = membyte & 0x01;
  SET_FLAGS_NZ(result);
  WRITE(ea, result);

  emPC += 2;
}

void  i70_BVS (void)
{
  if (V)
    BRANCH;
  else
    emPC++;
}

void  i71_ADC_binary (void)
{
  register unsigned char   zploc_l;
  register unsigned char   zploc_h;
  register unsigned char   membyte;
  register unsigned short  base;
  register unsigned short  ea;

  /* Indirect , Y */

  zploc_l = READ(emPC);
  zploc_h = 1 + zploc_l;

#ifdef ZPI_BOUNDS
  if (zploc_l == 0xFF)
    printf("warning: zero page indirect crosses page boundary at $%4.4X\n", emPC-1);
#endif

  ea = READ_ZP(zploc_h);
  ea <<= 8;
  ea |= READ_ZP(zploc_l);

  base = ea;
  ea += (unsigned short)Y;
  if ((base & 0xFF00) != (ea & 0xFF00)) cycle_clock++;

  membyte = READ(ea);
  emPC++;
  add_binary(membyte);
}

void  i71_ADC_decimal (void)
{
  register unsigned char   zploc_l;
  register unsigned char   zploc_h;
  register unsigned char   membyte;
  register unsigned short  base;
  register unsigned short  ea;

  /* Indirect , Y */

  zploc_l = READ(emPC);
  zploc_h = 1 + zploc_l;

#ifdef ZPI_BOUNDS
  if (zploc_l == 0xFF)
    printf("warning: zero page indirect crosses page boundary at $%4.4X\n", emPC-1);
#endif

  ea = READ_ZP(zploc_h);
  ea <<= 8;
  ea |= READ_ZP(zploc_l);

  base = ea;
  ea += (unsigned short)Y;
  if ((base & 0xFF00) != (ea & 0xFF00)) cycle_clock++;

  membyte = READ(ea);
  emPC++;
  add_decimal(membyte);
}

void  i72_ADC_binary (void)
{
  register unsigned char   zploc_l;
  register unsigned char   zploc_h;
  register unsigned char   membyte;
  register unsigned short  ea;

  /* Zero Page Indirect */

  zploc_l = READ(emPC);
  zploc_h = 1 + zploc_l;

#ifdef ZPI_BOUNDS
  if (zploc_l == 0xFF)
    printf("warning: zero page indirect crosses page boundary at $%4.4X\n", emPC-1);
#endif

  ea = READ_ZP(zploc_h);
  ea <<= 8;
  ea |= READ_ZP(zploc_l);

  membyte = READ(ea);
  emPC++;
  add_binary(membyte);
}

void  i72_ADC_decimal (void)
{
  register unsigned char   zploc_l;
  register unsigned char   zploc_h;
  register unsigned char   membyte;
  register unsigned short  ea;

  /* Zero Page Indirect */

  zploc_l = READ(emPC);
  zploc_h = 1 + zploc_l;

#ifdef ZPI_BOUNDS
  if (zploc_l == 0xFF)
    printf("warning: zero page indirect crosses page boundary at $%4.4X\n", emPC-1);
#endif

  ea = READ_ZP(zploc_h);
  ea <<= 8;
  ea |= READ_ZP(zploc_l);

  membyte = READ(ea);
  emPC++;
  add_decimal(membyte);
}

void  i74_STZ (void)
{
  register unsigned char  zploc;

  /* Zero Page , X */

  zploc = READ(emPC) + X;
  WRITE_ZP(zploc, 0);
  emPC++;
}

void  i75_ADC_binary (void)
{
  register unsigned char  zploc;
  register unsigned char  zpbyte;

  /* Zero Page , X */

  zploc  = READ(emPC) + X;
  zpbyte = READ_ZP(zploc);

  emPC++;

  add_binary(zpbyte);
}

void  i75_ADC_decimal (void)
{
  register unsigned char  zploc;
  register unsigned char  zpbyte;

  /* Zero Page , X */

  zploc  = READ(emPC) + X;
  zpbyte = READ_ZP(zploc);

  emPC++;

  add_decimal(zpbyte);
}

void  i76_ROR (void)
{
  register unsigned char  zploc;
  register unsigned char  zpbyte;
  register unsigned char  result;

  /* Zero Page , X */

  zploc  = READ(emPC) + X;
  zpbyte = READ_ZP(zploc);

  result = zpbyte >> 1;
  if (C) result |= 0x80;

  C = zpbyte & 0x01;

  SET_FLAGS_NZ(result);
  WRITE_ZP(zploc, result);

  emPC++;
}

void  i78_SEI (void)
{
  I = 1;
}

void  i79_ADC_binary (void)
{
  register unsigned char   membyte;
  register unsigned short  base;
  register unsigned short  ea;

  /* Absolute , Y */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  base = ea;
  ea += (unsigned short)Y;
  if ((base & 0xFF00) != (ea & 0xFF00)) cycle_clock++;

  membyte = READ(ea);
  emPC += 2;

  add_binary(membyte);
}

void  i79_ADC_decimal (void)
{
  register unsigned char   membyte;
  register unsigned short  base;
  register unsigned short  ea;

  /* Absolute , Y */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  base = ea;
  ea += (unsigned short)Y;
  if ((base & 0xFF00) != (ea & 0xFF00)) cycle_clock++;

  membyte = READ(ea);
  emPC += 2;

  add_decimal(membyte);
}

void  i7A_PLY (void)
{
  Y = PULL();
  SET_FLAGS_NZ(Y);
}

void  i7C_JMP (void)
{
  register unsigned short  ea;

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);
  ea += (unsigned short)X;

  emPC  = READ(ea + 1);
  emPC *= 256;
  emPC += READ(ea);
}

void  i7D_ADC_binary (void)
{
  register unsigned char   membyte;
  register unsigned short  base;
  register unsigned short  ea;

  /* Absolute , X */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  base = ea;
  ea += (unsigned short)X;
  if ((base & 0xFF00) != (ea & 0xFF00)) cycle_clock++;

  membyte = READ(ea);
  emPC += 2;

  add_binary(membyte);
}

void  i7D_ADC_decimal (void)
{
  register unsigned char   membyte;
  register unsigned short  base;
  register unsigned short  ea;

  /* Absolute , X */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  base = ea;
  ea += (unsigned short)X;
  if ((base & 0xFF00) != (ea & 0xFF00)) cycle_clock++;

  membyte = READ(ea);
  emPC += 2;

  add_decimal(membyte);
}

void  i7E_ROR (void)
{
  register unsigned char   result;
  register unsigned char   membyte;
  register unsigned short  ea;

  /* Absolute , X */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);
  ea += (unsigned short)X;

  membyte = READ(ea);

  result = membyte >> 1;
  if (C) result |= 0x80;
  C = membyte & 0x01;
    
  SET_FLAGS_NZ(result);
  WRITE(ea, result);
  emPC += 2;
}

void  i80_BRA (void)
{
  BRANCH;
}

void  i81_STA (void)
{
  register unsigned char   zploc_l;
  register unsigned char   zploc_h;
  register unsigned short  ea;

  /* Indirect , X */

  zploc_l = X + READ(emPC);
  zploc_h = 1 + zploc_l;

#ifdef ZPI_BOUNDS
  if (zploc_l == 0xFF)
    printf("warning: zero page indirect crosses page boundary at $%4.4X\n", emPC-1);
#endif

  ea = READ_ZP(zploc_h);
  ea <<= 8;
  ea |= READ_ZP(zploc_l);

  WRITE(ea, A);
  emPC++;
}

void  i84_STY (void)
{
  register unsigned char  zploc;

  /* Zero Page */

  zploc = READ(emPC);
  WRITE_ZP(zploc, Y);
  emPC++;
}

void  i85_STA (void)
{
  register unsigned char  zploc;

  /* Zero Page */

  zploc = READ(emPC);
  WRITE_ZP(zploc, A);
  emPC++;
}

void  i86_STX (void)
{
  register unsigned char  zploc;

  /* Zero Page */

  zploc = READ(emPC);
  WRITE_ZP(zploc, X);
  emPC++;
}

void  i88_DEY (void)
{
  Y--;
  SET_FLAGS_NZ(Y);
}

void  i89_BIT (void)
{
  register unsigned char  membyte;

  /* Immediate */

  membyte = READ(emPC);
  Z = (membyte & A) ? 0 : 1;
  emPC++;
}

void  i8A_TXA (void)
{
  A = X;
  SET_FLAGS_NZ(A);
}

void  i8C_STY (void)
{
  register unsigned short  ea;

  /* Absolute */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  WRITE(ea, Y);
  emPC += 2;
}

void  i8D_STA (void)
{
  register unsigned short  ea;

  /* Absolute */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  WRITE(ea, A);
  emPC += 2;
}

void  i8E_STX (void)
{
  register unsigned short  ea;

  /* Absolute */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  WRITE(ea, X);
  emPC += 2;
}

void  i90_BCC (void)
{
  if (C)
    emPC++;
  else
    BRANCH;
}

void  i91_STA (void)
{
  register unsigned char   zploc_l;
  register unsigned char   zploc_h;
  register unsigned short  base;
  register unsigned short  ea;

  /* Indirect , Y */

  zploc_l = READ(emPC);
  zploc_h = 1 + zploc_l;

#ifdef ZPI_BOUNDS
  if (zploc_l == 0xFF)
    printf("warning: zero page indirect crosses page boundary at $%4.4X\n", emPC-1);
#endif

  ea = READ_ZP(zploc_h);
  ea <<= 8;
  ea |= READ_ZP(zploc_l);

  base = ea;
  ea += (unsigned short)Y;
  if ((base & 0xFF00) != (ea & 0xFF00)) cycle_clock++;

#if 0
  if (trace == 1)
    printf("sta ($%2.2x),y : $%2.2x => ($%4.4x + $%2.2x = $%4.4x)\n", zploc_l, A, base, Y, ea);
#endif

  WRITE(ea, A);
  emPC++;
}

void  i92_STA (void)
{
  register unsigned char   zploc_l;
  register unsigned char   zploc_h;
  register unsigned short  ea;

  /* Zero Page Indirect */

  zploc_l = READ(emPC);
  zploc_h = 1 + zploc_l;

#ifdef ZPI_BOUNDS
  if (zploc_l == 0xFF)
    printf("warning: zero page indirect crosses page boundary at $%4.4X\n", emPC-1);
#endif

  ea = READ_ZP(zploc_h);
  ea <<= 8;
  ea |= READ_ZP(zploc_l);

  WRITE(ea, A);
  emPC++;
}

void  i94_STY (void)
{
  register unsigned char  zploc;

  /* Zero Page , X */

  zploc = READ(emPC) + X;
  WRITE_ZP(zploc, Y);
  emPC++;
}

void  i95_STA (void)
{
  register unsigned char  zploc;

  /* Zero Page , X */

  zploc = READ(emPC) + X;
  WRITE_ZP(zploc, A);
  emPC++;
}

void  i96_STX (void)
{
  register unsigned char  zploc;

  /* Zero Page , Y */

  zploc = READ(emPC) + Y;
  WRITE_ZP(zploc, X);
  emPC++;
}

void  i98_TYA (void)
{
  A = Y;
  SET_FLAGS_NZ(A);
}

void  i99_STA (void)
{
  register unsigned short  base;
  register unsigned short  ea;

  /* Absolute , Y */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  base = ea;
  ea += (unsigned short)Y;
  if ((base & 0xFF00) != (ea & 0xFF00)) cycle_clock++;

  WRITE(ea, A);
  emPC += 2;
}

void  i9A_TXS (void)
{
  S = X;
}

void  i9C_STZ (void)
{
  register unsigned short  ea;

  /* Absolute */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  WRITE(ea, 0);
  emPC += 2;
}

void  i9D_STA (void)
{
  register unsigned short  base;
  register unsigned short  ea;

  /* Absolute , X */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  base = ea;
  ea += (unsigned short)X;
  if ((base & 0xFF00) != (ea & 0xFF00)) cycle_clock++;

  WRITE(ea, A);
  emPC += 2;
}

void  i9E_STZ (void)
{
  register unsigned short  base;
  register unsigned short  ea;

  /* Absolute , X */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  base = ea;
  ea += (unsigned short)X;
  if ((base & 0xFF00) != (ea & 0xFF00)) cycle_clock++;

  WRITE(ea, 0);
  emPC += 2;
}

void  iA0_LDY (void)
{
  /* Immediate */

  Y = READ(emPC);
  SET_FLAGS_NZ(Y);
  emPC++;
}

void  iA1_LDA (void)
{
  register unsigned char   zploc_l;
  register unsigned char   zploc_h;
  register unsigned short  ea;

  /* Indirect , X */

  zploc_l = X + READ(emPC);
  zploc_h = 1 + zploc_l;

#ifdef ZPI_BOUNDS
  if (zploc_l == 0xFF)
    printf("warning: zero page indirect crosses page boundary at $%4.4X\n", emPC-1);
#endif

  ea = READ_ZP(zploc_h);
  ea <<= 8;
  ea |= READ_ZP(zploc_l);

  A = READ(ea);
  SET_FLAGS_NZ(A);
  emPC++;
}

void  iA2_LDX (void)
{
  /* Immediate */

  X = READ(emPC);
  SET_FLAGS_NZ(X);
  emPC++;
}

void  iA4_LDY (void)
{
  register unsigned char  zploc;

  /* Zero Page */

  zploc = READ(emPC);
  Y = READ_ZP(zploc);
  SET_FLAGS_NZ(Y);
  emPC++;
}

void  iA5_LDA (void)
{
  register unsigned char  zploc;

  /* Zero Page */

  zploc = READ(emPC);
  A = READ_ZP(zploc);
  SET_FLAGS_NZ(A);
  emPC++;
}

void  iA6_LDX (void)
{
  register unsigned char  zploc;

  /* Zero Page */

  zploc = READ(emPC);
  X = READ_ZP(zploc);
  SET_FLAGS_NZ(X);
  emPC++;
}

void  iA8_TAY (void)
{
  Y = A;
  SET_FLAGS_NZ(Y);
}

void  iA9_LDA (void)
{
  /* Immediate */

  A = READ(emPC);
  SET_FLAGS_NZ(A);
  emPC++;
}

void  iAA_TAX (void)
{
  X = A;
  SET_FLAGS_NZ(X);
}

void  iAC_LDY (void)
{
  register unsigned short  ea;

  /* Absolute */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  Y = READ(ea);
  SET_FLAGS_NZ(Y);
  emPC += 2;
}

void  iAD_LDA (void)
{
  register unsigned short  ea;

  /* Absolute */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  A = READ(ea);
  SET_FLAGS_NZ(A);
  emPC += 2;
}

void  iAE_LDX (void)
{
  register unsigned short  ea;

  /* Absolute */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  X = READ(ea);
  SET_FLAGS_NZ(X);
  emPC += 2;
}

void  iB0_BCS (void)
{
  if (C)
    BRANCH;
  else
    emPC++;
}

void  iB1_LDA (void)
{
  register unsigned char   zploc_l;
  register unsigned char   zploc_h;
  register unsigned short  base;
  register unsigned short  ea;

  /* Indirect , Y */

  zploc_l = READ(emPC);
  zploc_h = 1 + zploc_l;

#ifdef ZPI_BOUNDS
  if (zploc_l == 0xFF)
    printf("warning: zero page indirect crosses page boundary at $%4.4X\n", emPC-1);
#endif

  ea = READ_ZP(zploc_h);
  ea <<= 8;
  ea |= READ_ZP(zploc_l);

  base = ea;
  ea += (unsigned short)Y;
  if ((base & 0xFF00) != (ea & 0xFF00)) cycle_clock++;

  A = READ(ea);
  SET_FLAGS_NZ(A);
  emPC++;
}

void  iB2_LDA (void)
{
  register unsigned char   zploc_l;
  register unsigned char   zploc_h;
  register unsigned short  ea;

  /* Zero Page Indirect */

  zploc_l = READ(emPC);
  zploc_h = 1 + zploc_l;

#ifdef ZPI_BOUNDS
  if (zploc_l == 0xFF)
    printf("warning: zero page indirect crosses page boundary at $%4.4X\n", emPC-1);
#endif

  ea = READ_ZP(zploc_h);
  ea <<= 8;
  ea |= READ_ZP(zploc_l);

  A = READ(ea);
  SET_FLAGS_NZ(A);
  emPC++;
}

void  iB4_LDY (void)
{
  register unsigned char  zploc;

  /* Zero Page , X */

  zploc = READ(emPC) + X;
  Y = READ_ZP(zploc);
  SET_FLAGS_NZ(Y);
  emPC++;
}

void  iB5_LDA (void)
{
  register unsigned char  zploc;

  /* Zero Page , X */

  zploc = READ(emPC) + X;
  A = READ_ZP(zploc);
  SET_FLAGS_NZ(A);
  emPC++;
}

void  iB6_LDX (void)
{
  register unsigned char  zploc;

  /* Zero Page , Y */

  zploc = READ(emPC) + Y;
  X = READ_ZP(zploc);
  SET_FLAGS_NZ(X);
  emPC++;
}

void  iB8_CLV (void)
{
  V = 0;
}

void  iB9_LDA (void)
{
  register unsigned short  base;
  register unsigned short  ea;

  /* Absolute , Y */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  base = ea;
  ea += (unsigned short)Y;
  if ((base & 0xFF00) != (ea & 0xFF00)) cycle_clock++;

  A = READ(ea);
  SET_FLAGS_NZ(A);

  emPC += 2;
}

void  iBA_TSX (void)
{
  X = S;
  SET_FLAGS_NZ(X);
}

void  iBC_LDY (void)
{
  register unsigned short  base;
  register unsigned short  ea;

  /* Absolute , X */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  base = ea;
  ea += (unsigned short)X;
  if ((base & 0xFF00) != (ea & 0xFF00)) cycle_clock++;

  Y = READ(ea);
  SET_FLAGS_NZ(Y);

  emPC += 2;
}

void  iBD_LDA (void)
{
  register unsigned short  base;
  register unsigned short  ea;

  /* Absolute , X */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  base = ea;
  ea += (unsigned short)X;
  if ((base & 0xFF00) != (ea & 0xFF00)) cycle_clock++;

  A = READ(ea);
  SET_FLAGS_NZ(A);

  emPC += 2;
}

void  iBE_LDX (void)
{
  register unsigned short  base;
  register unsigned short  ea;

  /* Absolute , Y */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  base = ea;
  ea += (unsigned short)Y;
  if ((base & 0xFF00) != (ea & 0xFF00)) cycle_clock++;

  X = READ(ea);
  SET_FLAGS_NZ(X);

  emPC += 2;
}

void  iC0_CPY (void)
{
  register unsigned char  membyte;

  /* Immediate */

  membyte = READ(emPC);
  emPC++;
  compare(Y, membyte);
}

void  iC1_CMP (void)
{
  register unsigned char   zploc_l;
  register unsigned char   zploc_h;
  register unsigned char   membyte;
  register unsigned short  ea;

  /* Indirect , X */

  zploc_l = X + READ(emPC);
  zploc_h = 1 + zploc_l;

#ifdef ZPI_BOUNDS
  if (zploc_l == 0xFF)
    printf("warning: zero page indirect crosses page boundary at $%4.4X\n", emPC-1);
#endif

  ea = READ_ZP(zploc_h);
  ea <<= 8;
  ea |= READ_ZP(zploc_l);

  membyte = READ(ea);
  emPC++;
  compare(A, membyte);
}

void  iC4_CPY (void)
{
  register unsigned char  zploc;
  register unsigned char  zpbyte;

  /* Zero Page */

  zploc  = READ(emPC);
  zpbyte = READ_ZP(zploc);

  emPC++;

  compare(Y, zpbyte);
}

void  iC5_CMP (void)
{
  register unsigned char  zploc;
  register unsigned char  zpbyte;

  /* Zero Page */

  zploc  = READ(emPC);
  zpbyte = READ_ZP(zploc);

  emPC++;

  compare(A, zpbyte);
}

void  iC6_DEC (void)
{
  register unsigned char  zploc;
  register unsigned char  zpbyte;

  /* Zero Page */

  zploc  = READ(emPC);
  zpbyte = READ_ZP(zploc);

  zpbyte--;

  SET_FLAGS_NZ(zpbyte);
  WRITE_ZP(zploc, zpbyte);
  emPC++;
}

void  iC8_INY (void)
{
  Y++;
  SET_FLAGS_NZ(Y);
}

void  iC9_CMP (void)
{
  register unsigned char  membyte;

  /* Immediate */

  membyte = READ(emPC);
  emPC++;
  compare(A, membyte);
}

void  iCA_DEX (void)
{
  X--;
  SET_FLAGS_NZ(X);
}

void  iCC_CPY (void)
{
  register unsigned char   membyte;
  register unsigned short  ea;

  /* Absolute */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  membyte = READ(ea);
  emPC += 2;

  compare(Y, membyte);
}

void  iCD_CMP (void)
{
  register unsigned char   membyte;
  register unsigned short  ea;

  /* Absolute */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  membyte = READ(ea);
  emPC += 2;

  compare(A, membyte);
}

void  iCE_DEC (void)
{
  register unsigned char   membyte;
  register unsigned short  ea;

  /* Absolute */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  membyte = READ(ea);

  membyte--;

#if 0
  if (trace == 1)
    printf("decr $%4.4x => $%2.2x\n", ea, membyte);
#endif

  SET_FLAGS_NZ(membyte);
  WRITE(ea, membyte);

  emPC += 2;
}

void  iD0_BNE (void)
{
  if (Z)
    emPC++;
  else
    BRANCH;
}

void  iD1_CMP (void)
{
  register unsigned char   zploc_l;
  register unsigned char   zploc_h;
  register unsigned char   membyte;
  register unsigned short  base;
  register unsigned short  ea;

  /* Indirect , Y */

  zploc_l = READ(emPC);
  zploc_h = 1 + zploc_l;

#ifdef ZPI_BOUNDS
  if (zploc_l == 0xFF)
    printf("warning: zero page indirect crosses page boundary at $%4.4X\n", emPC-1);
#endif

  ea = READ_ZP(zploc_h);
  ea <<= 8;
  ea |= READ_ZP(zploc_l);

  base = ea;
  ea += (unsigned short)Y;
  if ((base & 0xFF00) != (ea & 0xFF00)) cycle_clock++;

  membyte = READ(ea);
  emPC++;
  compare(A, membyte);
}

void  iD2_CMP (void)
{
  register unsigned char   zploc_l;
  register unsigned char   zploc_h;
  register unsigned char   membyte;
  register unsigned short  ea;

  /* Zero Page Indirect */

  zploc_l = READ(emPC);
  zploc_h = 1 + zploc_l;

#ifdef ZPI_BOUNDS
  if (zploc_l == 0xFF)
    printf("warning: zero page indirect crosses page boundary at $%4.4X\n", emPC-1);
#endif

  ea = READ_ZP(zploc_h);
  ea <<= 8;
  ea |= READ_ZP(zploc_l);

  membyte = READ(ea);
  emPC++;
  compare(A, membyte);
}

void  iD5_CMP (void)
{
  register unsigned char  zploc;
  register unsigned char  zpbyte;

  /* Zero Page , X */

  zploc  = READ(emPC) + X;
  zpbyte = READ_ZP(zploc);

  emPC++;

  compare(A, zpbyte);
}

void  iD6_DEC (void)
{
  register unsigned char  zploc;
  register unsigned char  zpbyte;

  /* Zero Page , X */

  zploc  = READ(emPC) + X;
  zpbyte = READ_ZP(zploc);

  zpbyte--;

  SET_FLAGS_NZ(zpbyte);
  WRITE_ZP(zploc, zpbyte);
  emPC++;
}

void  iD8_CLD (void)
{
  D = 0;
  instruction_table = binary_instruction_table;
}

void  iD9_CMP (void)
{
  register unsigned char   membyte;
  register unsigned short  base;
  register unsigned short  ea;

  /* Absolute , Y */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  base = ea;
  ea += (unsigned short)Y;
  if ((base & 0xFF00) != (ea & 0xFF00)) cycle_clock++;

  membyte = READ(ea);
  emPC += 2;

  compare(A, membyte);
}

void  iDA_PHX (void)
{
  PUSH(X);
}

void  iDD_CMP (void)
{
  register unsigned char   membyte;
  register unsigned short  base;
  register unsigned short  ea;

  /* Absolute , X */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  base = ea;
  ea += (unsigned short)X;
  if ((base & 0xFF00) != (ea & 0xFF00)) cycle_clock++;

  membyte = READ(ea);
  emPC += 2;

  compare(A, membyte);
}

void  iDE_DEC (void)
{
  register unsigned char   membyte;
  register unsigned short  ea;

  /* Absolute , X */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);
  ea += (unsigned short)X;

  membyte = READ(ea);
  membyte--;

  SET_FLAGS_NZ(membyte);
  WRITE(ea, membyte);
  emPC += 2;
}

void  iE0_CPX (void)
{
  register unsigned char  membyte;

  /* Immediate */

  membyte = READ(emPC);
  emPC++;
  compare(X, membyte);
}

void  iE1_SBC_binary (void)
{
  register unsigned char   zploc_l;
  register unsigned char   zploc_h;
  register unsigned char   membyte;
  register unsigned short  ea;

  /* Indirect , X */

  zploc_l = X + READ(emPC);
  zploc_h = 1 + zploc_l;

#ifdef ZPI_BOUNDS
  if (zploc_l == 0xFF)
    printf("warning: zero page indirect crosses page boundary at $%4.4X\n", emPC-1);
#endif

  ea = READ_ZP(zploc_h);
  ea <<= 8;
  ea |= READ_ZP(zploc_l);

  membyte = READ(ea);
  emPC++;
  subtract_binary(membyte);
}

void  iE1_SBC_decimal (void)
{
  register unsigned char   zploc_l;
  register unsigned char   zploc_h;
  register unsigned char   membyte;
  register unsigned short  ea;

  /* Indirect , X */

  zploc_l = X + READ(emPC);
  zploc_h = 1 + zploc_l;

#ifdef ZPI_BOUNDS
  if (zploc_l == 0xFF)
    printf("warning: zero page indirect crosses page boundary at $%4.4X\n", emPC-1);
#endif

  ea = READ_ZP(zploc_h);
  ea <<= 8;
  ea |= READ_ZP(zploc_l);

  membyte = READ(ea);
  emPC++;
  subtract_decimal(membyte);
}

void  iE4_CPX (void)
{
  register unsigned char  zploc;
  register unsigned char  zpbyte;

  /* Zero Page */

  zploc  = READ(emPC);
  zpbyte = READ_ZP(zploc);

  emPC++;

  compare(X, zpbyte);
}

void  iE5_SBC_binary (void)
{
  register unsigned char  zploc;
  register unsigned char  zpbyte;

  /* Zero Page */

  zploc  = READ(emPC);
  zpbyte = READ_ZP(zploc);

  emPC++;

  subtract_binary(zpbyte);
}

void  iE5_SBC_decimal (void)
{
  register unsigned char  zploc;
  register unsigned char  zpbyte;

  /* Zero Page */

  zploc  = READ(emPC);
  zpbyte = READ_ZP(zploc);

  emPC++;

  subtract_decimal(zpbyte);
}

void  iE6_INC (void)
{
  register unsigned char  zploc;
  register unsigned char  zpbyte;

  /* Zero Page */

  zploc  = READ(emPC);
  zpbyte = READ_ZP(zploc);

  zpbyte++;

  SET_FLAGS_NZ(zpbyte);
  WRITE_ZP(zploc, zpbyte);
  emPC++;
}

void  iE8_INX (void)
{
  X++;
  SET_FLAGS_NZ(X);
}

void  iE9_SBC_binary (void)
{
  register unsigned char  membyte;

  /* Immediate */

  membyte = READ(emPC);
  emPC++;
  subtract_binary(membyte);
}

void  iE9_SBC_decimal (void)
{
  register unsigned char  membyte;

  /* Immediate */

  membyte = READ(emPC);
  emPC++;
  subtract_decimal(membyte);
}

void  iEA_NOP (void)
{
  /* Do nothing! */
}

void  iEC_CPX (void)
{
  register unsigned char   membyte;
  register unsigned short  ea;

  /* Absolute */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  membyte = READ(ea);
  emPC += 2;

  compare(X, membyte);
}

void  iED_SBC_binary (void)
{
  register unsigned char   membyte;
  register unsigned short  ea;

  /* Absolute */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  membyte = READ(ea);
  emPC += 2;

  subtract_binary(membyte);
}

void  iED_SBC_decimal (void)
{
  register unsigned char   membyte;
  register unsigned short  ea;

  /* Absolute */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  membyte = READ(ea);
  emPC += 2;

  subtract_decimal(membyte);
}

void  iEE_INC (void)
{
  register unsigned char   membyte;
  register unsigned short  ea;

  /* Absolute */

  ea  = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  membyte = READ(ea);

  membyte++;
  SET_FLAGS_NZ(membyte);
  WRITE(ea, membyte);
  emPC += 2;
}

void iF0_BEQ (void)
{
  if (Z)
    BRANCH;
  else
    emPC++;
}

void iF1_SBC_binary (void)
{
  unsigned char zploc_l;
  unsigned char zploc_h;
  unsigned char membyte;
  unsigned short base;
  unsigned short ea;

  /* Indirect , Y */

  zploc_l = READ(emPC);
  zploc_h = 1 + zploc_l;

#ifdef ZPI_BOUNDS
  if (zploc_l == 0xFF)
    printf("warning: zero page indirect crosses page boundary at $%4.4X\n", emPC-1);
#endif

  ea = READ_ZP(zploc_h);
  ea <<= 8;
  ea |= READ_ZP(zploc_l);

  base = ea;
  ea += (unsigned short)Y;
  if ((base & 0xFF00) != (ea & 0xFF00)) cycle_clock++;

  membyte = READ(ea);
  emPC++;
  subtract_binary(membyte);
}

void iF1_SBC_decimal (void)
{
  unsigned char zploc_l;
  unsigned char zploc_h;
  unsigned char membyte;
  unsigned short base;
  unsigned short ea;

  /* Indirect , Y */

  zploc_l = READ(emPC);
  zploc_h = 1 + zploc_l;

#ifdef ZPI_BOUNDS
  if (zploc_l == 0xFF)
    printf("warning: zero page indirect crosses page boundary at $%4.4X\n", emPC-1);
#endif

  ea = READ_ZP(zploc_h);
  ea <<= 8;
  ea |= READ_ZP(zploc_l);

  base = ea;
  ea += (unsigned short)Y;
  if ((base & 0xFF00) != (ea & 0xFF00)) cycle_clock++;

  membyte = READ(ea);
  emPC++;
  subtract_decimal(membyte);
}

void iF2_SBC_binary (void)
{
  unsigned char zploc_l;
  unsigned char zploc_h;
  unsigned char membyte;
  unsigned short ea;

  /* Zero Page Indirect */

  zploc_l = READ(emPC);
  zploc_h = 1 + zploc_l;

#ifdef ZPI_BOUNDS
  if (zploc_l == 0xFF)
    printf("warning: zero page indirect crosses page boundary at $%4.4X\n", emPC-1);
#endif

  ea = READ_ZP(zploc_h);
  ea <<= 8;
  ea |= READ_ZP(zploc_l);

  membyte = READ(ea);
  emPC++;
  subtract_binary(membyte);
}

void iF2_SBC_decimal (void)
{
  unsigned char zploc_l;
  unsigned char zploc_h;
  unsigned char membyte;
  unsigned short ea;

  /* Zero Page Indirect */

  zploc_l = READ(emPC);
  zploc_h = 1 + zploc_l;

#ifdef ZPI_BOUNDS
  if (zploc_l == 0xFF)
    printf("warning: zero page indirect crosses page boundary at $%4.4X\n", emPC-1);
#endif

  ea = READ_ZP(zploc_h);
  ea <<= 8;
  ea |= READ_ZP(zploc_l);

  membyte = READ(ea);
  emPC++;
  subtract_decimal(membyte);
}

void iF5_SBC_binary (void)
{
  unsigned char zploc;
  unsigned char zpbyte;

  /* Zero Page , X */

  zploc = READ(emPC) + X;
  zpbyte = READ_ZP(zploc);

  emPC++;

  subtract_binary(zpbyte);
}

void iF5_SBC_decimal (void)
{
  unsigned char zploc;
  unsigned char zpbyte;

  /* Zero Page , X */

  zploc = READ(emPC) + X;
  zpbyte = READ_ZP(zploc);

  emPC++;

  subtract_decimal(zpbyte);
}

void iF6_INC (void)
{
  unsigned char zploc;
  unsigned char zpbyte;

  /* Zero Page , X */

  zploc = READ(emPC) + X;
  zpbyte = READ_ZP(zploc);

  zpbyte++;

  SET_FLAGS_NZ(zpbyte);
  WRITE_ZP(zploc, zpbyte);
  emPC++;
}

void iF8_SED (void)
{
  D = 1;
  instruction_table = decimal_instruction_table;
}

void iF9_SBC_binary (void)
{
  unsigned char membyte;
  unsigned short base;
  unsigned short ea;

  /* Absolute , Y */

  ea = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  base = ea;
  ea += (unsigned short)Y;
  if ((base & 0xFF00) != (ea & 0xFF00)) cycle_clock++;

  membyte = READ(ea);
  emPC += 2;

  subtract_binary(membyte);
}

void iF9_SBC_decimal (void)
{
  unsigned char membyte;
  unsigned short base;
  unsigned short ea;

  /* Absolute , Y */

  ea = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  base = ea;
  ea += (unsigned short)Y;
  if ((base & 0xFF00) != (ea & 0xFF00)) cycle_clock++;

  membyte = READ(ea);
  emPC += 2;

  subtract_decimal(membyte);
}

void iFA_PLX (void)
{
  X = PULL();
  SET_FLAGS_NZ(X);
}

void iFD_SBC_binary (void)
{
  unsigned char membyte;
  unsigned short base;
  unsigned short ea;

  /* Absolute , X */

  ea = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  base = ea;
  ea += (unsigned short)X;
  if ((base & 0xFF00) != (ea & 0xFF00)) cycle_clock++;

  membyte = READ(ea);
  emPC += 2;

  subtract_binary(membyte);
}

void iFD_SBC_decimal (void)
{
  unsigned char membyte;
  unsigned short base;
  unsigned short ea;

  /* Absolute , X */

  ea = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);

  base = ea;
  ea += (unsigned short)X;
  if ((base & 0xFF00) != (ea & 0xFF00)) cycle_clock++;

  membyte = READ(ea);
  emPC += 2;

  subtract_decimal(membyte);
}

void iFE_INC (void)
{
  unsigned char membyte;
  unsigned short ea;

  /* Absolute , X */

  ea = READ(emPC + 1);
  ea *= 256;
  ea += READ(emPC);
  ea += (unsigned short)X;

  membyte = READ(ea);
  membyte++;

  SET_FLAGS_NZ(membyte);
  WRITE(ea, membyte);
  emPC += 2;
}
