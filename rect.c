/*
  rect.c
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

#include <gtk/gtk.h> /* GdkRectangle */

GdkRectangle union_rect (GdkRectangle r1, GdkRectangle r2)
{
  GdkRectangle result;

  if (r1.x < r2.x) result.x = r1.x;
  else             result.x = r2.x;

  if (r1.y < r2.y) result.y = r1.y;
  else             result.y = r2.y;

  if ((r1.x + r1.width) > (r2.x + r2.width)) result.width = r1.x + r1.width - result.x;
  else                                       result.width = r2.x + r2.width - result.x;

  if ((r1.y + r1.height) > (r2.y + r2.height)) result.height = r1.y + r1.height - result.y;
  else                                         result.height = r2.y + r2.height - result.y;

  return result;
}

GdkRectangle intersect_rect (GdkRectangle r1, GdkRectangle r2)
{
  GdkRectangle result;

  if (r1.x > r2.x) result.x = r1.x;
  else             result.x = r2.x;

  if (r1.y > r2.y) result.y = r1.y;
  else             result.y = r2.y;

  if ((r1.x + r1.width) < (r2.x + r2.width)) result.width = r1.x + r1.width - result.x;
  else                                       result.width = r2.x + r2.width - result.x;

  if ((r1.y + r1.height) < (r2.y + r2.height)) result.height = r1.y + r1.height - result.y;
  else                                         result.height = r2.y + r2.height - result.y;

  return result;
}
