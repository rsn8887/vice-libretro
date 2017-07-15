/*
 * vsyncarch.c - End-of-frame handling for Unix
 *
 * Written by
 *  Dag Lem <resid@nimrod.no>
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */


#include "vice.h"

#include "kbdbuf.h"
#include "ui.h"
#include "uistatusbar.h"
#include "vsyncapi.h"
#include "videoarch.h"
#include "video.h"
#include "resources.h"

#include "libretro-core.h"

#if defined(__X64SC__)
//XS:0   YS:16 XI:0 YI:0 W:384 H:272 XSC64
#define XS 0
#define YS 16
#elif defined(__PLUS4__)
#define XS 28
#define YS 19
#elif defined(__VIC20__)
//XS:576 YS:28 XI:0 YI:0 W:448 H:284
#define XS 588
#define YS 28
#elif defined(__CBM2__)
//XS:0 YS:8 XI:0 YI:0 W:704 H:266
#define XS 0
#define YS 8
#else
//CBM5x
//XS:104 YS:16 XI:0 YI:0 W:384 H:272 X64
#define XS 104
#define YS 16
#endif

extern void retro_poll_event(int joyon);
extern void app_vkb_handle();

extern struct video_canvas_s *RCANVAS;

#include <time.h>

#define TICKSPERSECOND  1000000L     /* Microseconds resolution. */

/* hook to ui event dispatcher */
static void_hook_t ui_dispatch_hook;

/* ------------------------------------------------------------------------- */

/* Number of timer units per second. */
signed long vsyncarch_frequency(void)
{
    /* Microseconds resolution. */
    return TICKSPERSECOND;
}

/* Get time in timer units. */
unsigned long vsyncarch_gettime(void)
{
    /* Microseconds resolution. */
    return GetTicks();
}

void vsyncarch_init(void)
{
    (void)vsync_set_event_dispatcher(ui_dispatch_events);
}

/* Display speed (percentage) and frame rate (frames per second). */
void vsyncarch_display_speed(double speed, double frame_rate, int warp_enabled)
{
    ui_display_speed((float)speed, (float)frame_rate, warp_enabled);
}

/* Sleep a number of timer units. */
void vsyncarch_sleep(signed long delay)
{//printf("delay:%d \n",delay);
	usleep(delay);
}

void vsyncarch_presync(void)
{
	int v;
	resources_get_int("RetroJoy",&v);

        kbdbuf_flush();
	
	retro_poll_event(v);

#if defined(__VIC20__)
        RCANVAS->videoconfig->rendermode = VIDEO_RENDER_RGB_1X1;
#endif
	video_canvas_render(RCANVAS,(BYTE *)bmp/*Retro_Screen*/,
						retrow,retroh,//384, 272,
                        XS,YS,//xs, ys,
                        0,0,//xi, yi,
                        retrow*PITCH,8*PITCH);//384*4, 32);

	//app_vkb_handle();

    if (uistatusbar_state & UISTATUSBAR_ACTIVE && vice_statusbar==1) {
		
        uistatusbar_draw();
    }

    cpuloop=0;

}

void_hook_t vsync_set_event_dispatcher(void_hook_t hook)
{
    void_hook_t t = ui_dispatch_hook;
    ui_dispatch_hook = hook;
    return t;
}

void vsyncarch_postsync(void)
{
    /* Dispatch all the pending UI events.  */
    ui_dispatch_events();
}
