/*
  switches.c
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

/* Apple II soft switches */

unsigned char  switch_alt_cset = 0;
unsigned char  switch_graphics = 0;
unsigned char  switch_mixed    = 0;
unsigned char  switch_hires    = 0;
unsigned char  switch_page2    = 0;

/* Apple IIe soft switches */

unsigned char  switch_auxzp           = 0;
unsigned char  switch_auxread         = 0;
unsigned char  switch_auxwrite        = 0;
unsigned char  switch_page2_aux_text  = 0;
unsigned char  switch_page2_aux_hires = 0;
unsigned char  switch_intc8rom        = 0;
unsigned char  switch_slotc3rom       = 0;
unsigned char  switch_slotcxrom       = 0;
unsigned char  switch_80col           = 0;
unsigned char  switch_80store         = 0;
unsigned char  switch_dblhires        = 0;
unsigned char  switch_ioudis          = 1;

/* Apple IIc soft switches */
