/*
  apple.c
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

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/Xxf86dga.h>

#include <sys/time.h>
#include <unistd.h>

#include "cpu.h"
#include "lc.h"
#include "memory.h"
#include "rect.h"    /* intersect_rect() */
#include "slots.h"
#include "video.h"

#include "floppy.h"
#include "hdisk.h"
#include "mouse.h"
#include "switches.h"
#include "tm2ho.h"
#include "ext80col.h"
#include "ramworks.h"
#include "xdga.h"

#ifdef USE_SDL
#include "SDL.h"
#endif

#define SEED_TIME() (srand((unsigned)time(0)))
#define RANDOM(m)   ((unsigned)((double)(m) * rand() / ((double)RAND_MAX + 1.0)))

int raster_y = 0;
int raster_x = 0;

unsigned char apple_vbl = 0;

unsigned char option_color_monitor = 1;
unsigned char option_full_scan     = 0;
unsigned char option_limit_speed   = 0;
unsigned char option_use_sdl       = 0;
unsigned char option_use_gtk       = 0;
unsigned char option_use_xdga      = 1;
unsigned int  option_request_w     = 0;
unsigned int  option_request_h     = 0;

static unsigned long old_cycle_clock;

/* display variables */

struct GTK_data my_GTK = {0,0,0,{0,0,0,0}};
xdga_info_t xdga;
int SCREEN_BORDER_WIDTH;
int SCREEN_BORDER_HEIGHT;

/*********/

static gint idlef = 0;
static gint run   = 0;

unsigned long apple_time_ticks;
#define TICKS_PER_SECOND 60

/******************/

unsigned char kb_byte             = 0;
unsigned char key_pressed         = 0;
unsigned char key_autorepeat      = 0;
unsigned char open_apple_pressed  = 0;
unsigned char solid_apple_pressed = 0;

/******************/

unsigned char  sample_buf[65536];
unsigned short sample_idx = 0;
unsigned short sample_pos = 0;

/******************/

extern int trace;

/******************/

/* */

void  empty_init (unsigned short  slot,
                  cpu_state_t *   cpup,
                  r_func_t *      rtab,
                  w_func_t *      wtab)
{
}

void  empty_info (unsigned char *  bits)
{
  *bits = 0;
}

void  empty_peek (void)
{
}

static void load_cards (void)
{
  int i_idx;

  for (i_idx = 0; i_idx < NUM_CARDSLOTS; i_idx++)
    {
      slot[i_idx].init  = empty_init;
      slot[i_idx].info  = empty_info;
      slot[i_idx].read  = r_nop;
      slot[i_idx].write = w_nop;
      slot[i_idx].peek  = empty_peek;
      slot[i_idx].call  = empty_peek;
      slot[i_idx].time  = empty_peek;
    }

  /*
  aux_slot.init  = init_extended_80col;
  aux_slot.read  = read_extended_80col;
  aux_slot.write = write_extended_80col;
  */
  aux_slot.init  = init_ramworks;
  aux_slot.read  = read_ramworks;
  aux_slot.write = write_ramworks;

  slot[4].init  = am2_init;
  slot[4].info  = am2_info;
  slot[4].read  = am2_read;
  slot[4].write = am2_write;
  slot[4].peek  = am2_peek;
  slot[4].call  = am2_call;
  slot[4].time  = am2_time;

  slot[5].init  = tm2ho_init;
  slot[5].info  = tm2ho_info;
  slot[5].read  = tm2ho_read;
  slot[5].write = tm2ho_write;
  slot[5].peek  = tm2ho_peek;
  slot[5].call  = tm2ho_call;
  slot[5].time  = tm2ho_time;

  slot[6].init  = d2_init;
  slot[6].info  = d2_info;
  slot[6].read  = d2_read;
  slot[6].write = d2_write;
  slot[6].peek  = d2_peek;
  slot[6].call  = d2_call;
  slot[6].time  = d2_time;

  /**/
  slot[7].init  = hdisk_init;
  slot[7].info  = hdisk_info;
  slot[7].read  = hdisk_read;
  slot[7].write = hdisk_write;
  slot[7].peek  = hdisk_peek;
  slot[7].call  = hdisk_call;
  slot[7].time  = hdisk_time;
  /**/
}

/* create a new backing pixmap of the appropriate size */

static gint configure_event (GtkWidget * widget, GdkEventConfigure * event)
{
  if (my_GTK.apple_pixmap)
    gdk_pixmap_unref(my_GTK.apple_pixmap);

  my_GTK.apple_pixmap = gdk_pixmap_new(widget->window,
                                       widget->allocation.width,
                                       widget->allocation.height, -1);

  gdk_draw_rectangle(my_GTK.apple_pixmap, widget->style->black_gc,
                     1, 0, 0, widget->allocation.width, widget->allocation.height);

  return TRUE;
}

/* redraw the screen from the backing pixmap */

static gint expose_event (GtkWidget * widget, GdkEventExpose * event)
{
  GdkGC * gc = widget->style->fg_gc[GTK_WIDGET_STATE(widget)];

  if (0 == event->area.width || 0 == event->area.height)
    return FALSE;

  if (event->area.x >= WINDOW_WIDTH || event->area.y >= WINDOW_HEIGHT)
    return FALSE;

  /* do refresh stuff */
  gdk_draw_pixmap(widget->window, gc, my_GTK.apple_pixmap,
                  event->area.x, event->area.y, event->area.x, event->area.y,
                  event->area.width, event->area.height);

  /*
  gdk_draw_rectangle(widget->window, widget->style->black_gc, 1,
                     event->area.x, event->area.y, event->area.width, event->area.height);
  */

  return FALSE;
}

unsigned      count;
unsigned long key_press_time;

static gint key_press_event (GtkWidget * widget, GdkEventKey * event)
{
  if (run)
    {
      switch (event->keyval)
        {
        case GDK_Left:
          event->keyval = 0x08;
          break;

        case GDK_Up:
          event->keyval = 0x0B;
          break;

        case GDK_Right:
          event->keyval = 0x15;
          break;

        case GDK_Down:
          event->keyval = 0x0A;
          break;

        case GDK_Alt_L:
        case GDK_Page_Up:
          open_apple_pressed = 0x80;
          event->keyval = 0;
          return TRUE;

        case GDK_Alt_R:
        case GDK_Page_Down:
          solid_apple_pressed = 0x80;
          event->keyval = 0;
          return TRUE;

        case GDK_Pause:
        case GDK_Break:
          slotcxrom_on();
          interrupt_flags |= F_RESET;
          event->keyval = 0;
          return TRUE;

        default:
          event->keyval &= 0xFF;
          break;
        }

      if (event->keyval < 0x80)
        {
          if (event->state & 4)
            event->keyval &= 0x1F;

          kb_byte        = (event->keyval | 0x80);
          key_pressed    = 0x80;
          key_press_time = apple_time_ticks;

          /* printf("key down\n"); */

          gdk_key_repeat_disable();
        }

      event->keyval = 0;
    }

  return TRUE;
}

static gint key_release_event (GtkWidget * widget, GdkEventKey * event)
{
  if (run)
    {
      switch (event->keyval)
        {
        case GDK_Alt_L:
          open_apple_pressed = 0;
          break;

        case GDK_Alt_R:
          solid_apple_pressed = 0;
          break;

        default:
          if (event->state & 4)
            {
              if ((event->keyval & 0xFF) >= 0x80)
                return TRUE;
            }
          break;
        }

      key_pressed    = 0;
      key_autorepeat = 0;

      /* printf("key up\n"); */

      gdk_key_repeat_restore();
    }

  return TRUE;
}

static void x_key_press (KeySym key, unsigned int state)
{
  int keyval = 0;

  if (!run) return;

  switch (key)
    {
    case XK_Left:
      keyval = 0x08;
      break;

    case XK_Up:
      keyval = 0x0B;
      break;

    case XK_Right:
      keyval = 0x15;
      break;

    case XK_Down:
      keyval = 0x0A;
      break;

    case XK_Alt_L:
    case XK_Meta_L:
    case XK_Super_L:
    case XK_Hyper_L:
      open_apple_pressed = 0x80;
      return;

    case XK_Alt_R:
    case XK_Meta_R:
    case XK_Super_R:
    case XK_Hyper_R:
      solid_apple_pressed = 0x80;
      return;

    case XK_Pause:
    case XK_Break:
      slotcxrom_on();
      interrupt_flags |= F_RESET;
      return;

    case XK_Page_Down:
      {
        char * foo;
        foo = malloc(80);
        strcpy(foo, "./ppb.dsk");
        d2_new_disk(0, foo);
      }
      return;

    case XK_Page_Up:
      {
        char * foo;
        foo = malloc(80);
        strcpy(foo, "./pp0.dsk");
        d2_new_disk(0, foo);
      }
      return;

    default:
      keyval = key & 0xFF;
      break;
    }

  if (keyval < 0x80)
    {
      if (state & ControlMask)
        keyval &= 0x1F;

      kb_byte        = 0x80 | keyval;
      key_pressed    = 0x80;
      key_press_time = apple_time_ticks;

      //printf("keyval:%x  kb_byte:%x  kb_strobe:%x\n", keyval, kb_byte, key_pressed);

      //gdk_key_repeat_disable();
    }
}

static void x_key_release (KeySym key, unsigned int state)
{
  if (!run) return;

  switch (key)
    {
    case XK_Alt_L:
    case XK_Meta_L:
    case XK_Super_L:
    case XK_Hyper_L:
      open_apple_pressed = 0;
      break;

    case XK_Alt_R:
    case XK_Meta_R:
    case XK_Super_R:
    case XK_Hyper_R:
      solid_apple_pressed = 0;
      break;

    default:
      if ((state & ControlMask) && ((key & 0xFF) >= 0x80))
        return;
    }

  key_pressed    = 0;
  key_autorepeat = 0;

  //gdk_key_repeat_restore();
}

void toggle_scan (GtkWidget * widget, gpointer data)
{
  option_full_scan = option_full_scan ? 0 : 1;
  reset_video();
}

void toggle_color (GtkWidget * widget, gpointer data)
{
  option_color_monitor = option_color_monitor ? 0 : 1;
  reset_video();
}

void toggle_limit_speed (GtkWidget *  widget,
                         gpointer     data)
{
  option_limit_speed = 1 - option_limit_speed;

  if (option_limit_speed)
    i_delay = 1;
  else
    i_delay = 0;

  old_cycle_clock = cycle_clock;
}


extern unsigned char current_image_type;

void set_disk_volume (GtkWidget *  widget,
                      gpointer     data)
{
  if (d2_volume == 254)
    d2_volume = 1;
  else
    d2_volume = 254;

  printf("volume : %d\n", d2_volume);
}

static GtkWidget *  file_select = 0;
static char *       fs_filename = 0;
static int          fs_drive    = 0;

static void fs_ok (GtkWidget *         w,
                   GtkFileSelection *  fs)
{
  //fs_filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(fs));
  fs_filename = (char *)malloc(1 + strlen(fs_filename));

  if (fs_filename)
    {
      strcpy(fs_filename, gtk_file_selection_get_filename(GTK_FILE_SELECTION(fs)));
      d2_new_disk(fs_drive, fs_filename);
    }

  gtk_widget_destroy(file_select);
  file_select = 0;
}

static void fs_cancel (GtkWidget *         w,
                       GtkFileSelection *  fs)
{
  fs_filename = 0;
  gtk_widget_destroy(file_select);
  file_select = 0;
}

static void select_image (void)
{
  if (!file_select)
    {
      file_select = gtk_file_selection_new("Select a disk image");

      if (!file_select)
        {
          g_print("Failed to open file selection dialog\n");
          fs_filename = 0;
        }

      gtk_signal_connect(GTK_OBJECT(file_select), "destroy",
                         (GtkSignalFunc)fs_cancel, &file_select);

      gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(file_select)->ok_button),
                         "clicked", (GtkSignalFunc)fs_ok, file_select);
      gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(file_select)->cancel_button),
                         "clicked", (GtkSignalFunc)fs_cancel, file_select);
    }

  gtk_widget_show(file_select);
}

void select_drive1_disk (GtkWidget * widget, gpointer data)
{
  fs_drive = 0;
  select_image();
}

void select_drive2_disk (GtkWidget * widget, gpointer data)
{
  fs_drive = 1;
  select_image();
}

gint apple_timed (gpointer data)
{
  apple_time_ticks++;

  if (key_pressed)
    {
      if (key_autorepeat)
        {
          if ((apple_time_ticks - key_press_time) >= (TICKS_PER_SECOND / 20))
            {
              kb_byte |= 0x80;
              key_press_time = apple_time_ticks;
            }
        }
      else if ((apple_time_ticks - key_press_time) > (TICKS_PER_SECOND / 3))
        {
          key_press_time = apple_time_ticks;
          key_autorepeat = 1;
        }
    }

  if (1 || option_limit_speed)
    {
      double speed;
      double real_elapsed;
      double virtual_elapsed;

      struct timeval tv;
      static struct timeval last_tv;

      static int count = 0;

      gettimeofday(&tv, 0);

      if ((cycle_clock - old_cycle_clock) > 64000)
        {
          old_cycle_clock = cycle_clock - old_cycle_clock;

          real_elapsed = ((1000000.0 * tv.tv_sec + tv.tv_usec) -
                          (1000000.0 * last_tv.tv_sec + last_tv.tv_usec));

          virtual_elapsed = (double)(old_cycle_clock) / 1.024;

          /*
          printf("Speed = %gMhz  virtual/real elapsed: %g/%g s\n",
          (double)old_cycle_clock / 1.024 / real_elapsed,
          virtual_elapsed / 1000000.0, real_elapsed / 1000000.0);
          */

          /**/
          i_delay = (long)((double)i_delay * (virtual_elapsed / real_elapsed));

          if (0 == i_delay)
            i_delay = 1;
          /**/

          if (!option_limit_speed)
            i_delay = 1;

          speed = (double)old_cycle_clock / 1.024 / real_elapsed;

          if (speed <= 0.9)
            {
              if (param_frame_skip < 7)
                {
                  ++param_frame_skip;
                  /* printf("frame_skip = %d\n", param_frame_skip - 1); */
                }
            }
          else if (speed >= 1.1)
            {
              if (param_frame_skip > 1)
                {
                  --param_frame_skip;
                  /* printf("frame_skip = %d\n", param_frame_skip - 1); */
                }
            }

          if (++count == 64)
            {
              count = 0;
              printf("Speed = %gMhz\n", speed);
              fflush(stdout);
            }

          last_tv = tv;
          old_cycle_clock = cycle_clock;
        }
    }

  return TRUE;
}

gint execute (gpointer data)
{
  execute_one();
#if 0
  gtk_widget_draw(screen_da, &(my_GTK.screen_rect));
  RESET_SCREEN_RECT();
#endif
  return TRUE;
}

void startstop (GtkWidget * widget, gpointer data)
{
  if (run)
    {
      run = 0;
      gtk_idle_remove(idlef);
    }
  else
    {
      run = 1;
      idlef = gtk_idle_add(execute, 0);
    }
}

void quit (GtkWidget * widget, gpointer data)
{
  gtk_main_quit();
}

void init_cards (void)
{
  unsigned int page_idx;
  unsigned char info_bits;

  for (page_idx = 1; page_idx < 8; page_idx++)
    {
      if (slot[page_idx].init)
        {
          slot[page_idx].init(page_idx, &cpu_ptrs, r_page, w_page);

          slot[page_idx].info(&info_bits);

          if (info_bits & SLOT_PEEK)
            slot_peeks[last_slot_peek++] = page_idx;

          if (info_bits & SLOT_CALL)
            slot_calls[last_slot_call++] = page_idx;

          if (info_bits & SLOT_TIME)
            slot_times[last_slot_time++] = page_idx;
        }
    }
}

void init_gtk_video (int argc, char * argv[])
{
  GtkWidget * hbox;
  GtkWidget * vbox;
  GtkWidget * button;

  gtk_init(&argc, &argv);

  /* create the Apple window */

  apple_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_events(apple_window, GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);
  gtk_widget_set_name(apple_window, "Apple //e");

  vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(apple_window), vbox);
  gtk_widget_show(vbox);

  gtk_signal_connect(GTK_OBJECT(apple_window), "destroy", GTK_SIGNAL_FUNC(quit), NULL);

  /* create the drawing area */

  screen_da = gtk_drawing_area_new();
  gtk_widget_set_events(screen_da, GDK_EXPOSURE_MASK);
  gtk_drawing_area_size(GTK_DRAWING_AREA(screen_da), WINDOW_WIDTH, WINDOW_HEIGHT);
  gtk_box_pack_start(GTK_BOX(vbox), screen_da, TRUE, TRUE, 0);
  gtk_widget_show(screen_da);

  /* create the button area */

  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
  gtk_widget_show(hbox);

  button = gtk_button_new_with_label("Start/Stop");
  gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(startstop), 0);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, FALSE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label("Mono/Color");
  gtk_signal_connect(GTK_OBJECT(button), "clicked",
                     GTK_SIGNAL_FUNC(toggle_color), 0);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, FALSE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label("Full/Half");
  gtk_signal_connect(GTK_OBJECT(button), "clicked",
                     GTK_SIGNAL_FUNC(toggle_scan), 0);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, FALSE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label("Limit Speed");
  gtk_signal_connect(GTK_OBJECT(button), "clicked",
                     GTK_SIGNAL_FUNC(toggle_limit_speed), 0);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, FALSE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label("Drive 1");
  gtk_signal_connect(GTK_OBJECT(button), "clicked",
                     GTK_SIGNAL_FUNC(select_drive1_disk), 0);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, FALSE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label("Drive 2");
  gtk_signal_connect(GTK_OBJECT(button), "clicked",
                     GTK_SIGNAL_FUNC(select_drive2_disk), 0);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, FALSE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label("Volume");
  gtk_signal_connect(GTK_OBJECT(button), "clicked",
                     GTK_SIGNAL_FUNC(set_disk_volume), 0);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, FALSE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label("Quit");
  gtk_signal_connect_object(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy),
                            GTK_OBJECT(apple_window));
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, FALSE, 0);
  gtk_widget_show(button);

  /* signals used to handle backing pixmap */

  gtk_signal_connect(GTK_OBJECT(screen_da), "expose_event",
                     (GtkSignalFunc)expose_event, NULL);
  gtk_signal_connect(GTK_OBJECT(screen_da), "configure_event",
                     (GtkSignalFunc)configure_event, NULL);

  /* keyboard signals */

  gtk_signal_connect(GTK_OBJECT(apple_window), "key_press_event",
                     (GtkSignalFunc)key_press_event, NULL);
  gtk_signal_connect(GTK_OBJECT(apple_window), "key_release_event",
                     (GtkSignalFunc)key_release_event, NULL);

  gtk_widget_show(apple_window);
}

int init_sdl_video ()
{
  return 0;
}

#ifdef USE_SDL
void fill_apple_audio (void * udata, Uint8 * stream, int buffer_len)
{
  Uint8 * stream_ptr;
  unsigned short idx = 0;
  unsigned short available_len = 0;

  stream_ptr = stream;
  available_len = sample_idx - sample_pos;

  if (0 == available_len)
    return;

  if (available_len < buffer_len)
    {
      for (idx = 0; idx < available_len; ++idx)
        *stream_ptr++ = sample_buf[sample_pos++];
      while (idx++ < buffer_len)
        *stream_ptr++ = sample_buf[sample_pos-1];
    }
  else
    {
      int flag = 0;
      int this_byte = 0;
      int last_byte = 0;

more_than_enough:
      flag = 0;

      last_byte = *stream_ptr++ = sample_buf[sample_pos++];
      for (idx = 1; idx < buffer_len; ++idx)
        {
          this_byte = *stream_ptr++ = sample_buf[sample_pos++];
          if (this_byte != last_byte) flag = 1;
          last_byte = this_byte;
        }

      if (0 == flag)
        {
          stream_ptr = stream;
          available_len = sample_idx - sample_pos;

          if (available_len > buffer_len)
            goto more_than_enough;

          sample_pos = sample_idx - buffer_len;
          for (idx = 0; idx < buffer_len; ++idx)
            *stream_ptr++ = sample_buf[sample_pos++];
        }
    }

  /*
  SDL_MixAudio(stream, sample_pos, buffer_len, SDL_MIX_MAXVOLUME);
  sample_pos += copy_len;
  */
}
#endif

int main (int argc, char *argv[])
{
  SCREEN_BORDER_WIDTH = ((WINDOW_WIDTH - SCREEN_WIDTH) / 2);
  SCREEN_BORDER_HEIGHT = ((WINDOW_HEIGHT - SCREEN_HEIGHT) / 2);

  SEED_TIME();

  if (option_use_sdl)
    option_use_sdl = init_sdl_video();

  if (option_use_xdga)
    {
      if (geteuid())
        {
          fprintf(stderr, "Must be suid root to use DGA\n");
          option_use_xdga = 0;
          option_use_gtk = 1;
        }
    }

  if (option_use_gtk)
    init_gtk_video(argc, argv);

#ifdef USE_SDL
  {
    SDL_AudioSpec wanted;
    if (SDL_Init(SDL_INIT_AUDIO) < 0)
      {
        fprintf(stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
        exit(1);
      }

    wanted.freq     = 22050;
    wanted.format   = AUDIO_U8;
    wanted.channels = 1;
    wanted.samples  = 512;
    wanted.callback = fill_apple_audio;
    wanted.userdata = 0;

    if (SDL_OpenAudio(&wanted, 0) < 0)
      fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
  }
#endif

  /* memory allocation and pointer initialization */

  init_cpu();
  load_cards();
  init_cards();
  init_memory();

  init_video();

  /* BIG NASTY HACK */
  {
#define MINMAJOR 2
#define MINMINOR 0
    Display * xdisp;
    XEvent event;
    XKeyEvent x_key;
    XDGAKeyEvent * xdga_key;
    char * fail_reason;

    if (option_use_xdga)
      {
        if (0 == (xdisp = XOpenDisplay(0)))
          {
            printf("Couldn't open default display\n");
            exit(1);
          }

        if (option_request_w == 0) option_request_w = WINDOW_WIDTH;
        if (option_request_h == 0) option_request_h = WINDOW_HEIGHT;

        if (xdga_mode(xdisp, option_request_w, option_request_h, &xdga, &fail_reason))
          {
            long total;
            struct timeval before;
            struct timeval after;
            struct timezone garbage;

            SCREEN_BORDER_WIDTH = ((xdga.width - SCREEN_WIDTH) / 2);
            SCREEN_BORDER_HEIGHT = ((xdga.height - SCREEN_HEIGHT) / 2);

            printf("Using DGA video mode %dx%dx%d\n", xdga.width, xdga.height, xdga.depth);

            XDGASelectInput(xdga.xdisp, DefaultScreen(xdga.xdisp),
                            KeyPressMask | KeyReleaseMask | ButtonReleaseMask);
            run = 1;
            gettimeofday(&before, &garbage);
            while (1)
              {
                while (XEventsQueued(xdga.xdisp, QueuedAfterFlush))
                  {
                    XNextEvent(xdga.xdisp, &event);
                    switch (event.type)
                      {
                      case KeyPress:
                      case KeyRelease:
                      case ButtonRelease:
                        goto end_dga;

                      default:
                        {
                          int dga_type = event.type - xdga.event_base;
                          switch (dga_type)
                            {
                            case KeyPress:
                            case KeyRelease:
                              {
                                int index;
                                KeySym key;

                                xdga_key = (XDGAKeyEvent *)&event;
                                XDGAKeyEventToXKeyEvent(xdga_key, &x_key);
                                index = (x_key.state & (ShiftMask | LockMask)) ? 1 : 0;
                                key = XLookupKeysym(&x_key, index);

                                // printf("key:%x  state:%x\n", key, x_key.state);

                                if (dga_type == KeyPress)
                                  x_key_press(key, x_key.state);
                                else
                                  x_key_release(key, x_key.state);
                              }
                              break;

                            case ButtonRelease:
                              goto end_dga;
                            }
                        }
                      }
                  }

                execute_one();
                if ((cycle_clock - old_cycle_clock) > (1024 * 1024))
                  {
                    gettimeofday(&after, &garbage);

                    if (after.tv_sec == before.tv_sec)
                      total = after.tv_usec - before.tv_usec;
                    else
                      total = 1000000 * (after.tv_sec - before.tv_sec) + after.tv_usec - before.tv_usec;

                    before = after;

                    printf("%g Mhz\n", (double)(cycle_clock - old_cycle_clock) / 1.024 / (double)total);

                    old_cycle_clock = cycle_clock;
                  }
              }

          end_dga:
            XDGASetMode(xdga.xdisp, DefaultScreen(xdga.xdisp), 0);
            XFree(xdga.device);
            XDGACloseFramebuffer(xdga.xdisp, DefaultScreen(xdga.xdisp));
            exit(0);
          }

        printf("Can't use DGA: %s\n", fail_reason);
        exit(1);
      }
  }

  option_use_xdga = 0;
  set_video_mode();

  SET_SCREEN_RECT();
  gtk_widget_draw(screen_da, &(my_GTK.screen_rect));
  RESET_SCREEN_RECT();

  gtk_timeout_add(1000 / TICKS_PER_SECOND, apple_timed, 0);

#ifdef USE_SDL
  SDL_PauseAudio(0);
#endif

  gtk_main();

#ifdef USE_SDL
  SDL_CloseAudio();
#endif

  return 0;
}
