/*
  cpu.h
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

#ifndef INCLUDED_CPU_H
#define INCLUDED_CPU_H

extern unsigned char  A, X, Y, P, S;
extern unsigned short emPC;

extern unsigned char C, Z, I, D, B, V, N;

extern unsigned char interrupt_flags;

extern unsigned long i_delay;
extern unsigned long cycle_clock;

extern int raster_row;
extern int raster_dot;

extern int param_frame_skip; /* # frames to skip == param_frame_skip - 1 */

extern void (* rasterizer) (int, int, int, int);

extern int trace;

#define F_RESET  0x1
#define F_IRQ    0x2
#define F_NMI    0x4

struct cpu_state
{
  unsigned char * A;
  unsigned char * X;
  unsigned char * Y;
  unsigned char * S;
  unsigned char * C;
  unsigned char * Z;
  unsigned char * I;
  unsigned char * D;
  unsigned char * B;
  unsigned char * V;
  unsigned char * N;
  unsigned char * irq;
  unsigned short * PC;
};

typedef struct cpu_state cpu_state_t;

extern cpu_state_t cpu_ptrs;

void init_cpu    (void);
void execute_one (void);

#endif /* #ifndef INCLUDED_CPU_H */
