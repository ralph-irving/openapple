/*
  floppy.h
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

#ifndef INCLUDED_FLOPPY_H
#define INCLUDED_FLOPPY_H

#include "memory.h"
#include "slots.h"

/*********/

void  d2_new_disk (int     d_idx,
                   char *  filename);

/*********/

extern unsigned char d2_volume;

void  d2_peek (void);
void  d2_time (void);
void  d2_call (void);

void  d2_info (unsigned char *  bits);

unsigned char  d2_read (unsigned short  addr);

void  d2_write (unsigned short  addr,
                unsigned char   value);

void  d2_init (unsigned short  slot,
               cpu_state_t *   cpup,
               r_func_t *      rtab,
               w_func_t *      wtab);

#endif /* INCLUDED_FLOPPY_H */
