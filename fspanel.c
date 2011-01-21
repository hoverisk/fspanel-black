
/**********************************************************
 ** F***ing Small Panel 0.8beta1 Copyright (c) 2000-2002 **
 ** By Peter Zelezny <zed at linux dot com>              **
 ** See file COPYING for license details.                **
 **********************************************************/

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

/*
#ifdef HAVE_XPM
#include <X11/xpm.h>
#include "icon.xpm"
#endif
*/

/* you can edit these */
#define MAX_TASK_WIDTH 145
#define ICONWIDTH 16
#define ICONHEIGHT 16
#define WINHEIGHT 24
#define WINWIDTH (scr_width)
#define FONT_NAME "-cronyx-fixed-*-*-*-*-16-*-*-*-*-*-*-*"
#define XFT_FONT "cronyx-16:style=fixed"
#define PAGER		/* use a desktop pager? */
#define PAGER_DIGIT_WIDTH 6
#define PAGER_BUTTON_WIDTH 20

/* don't edit these */
#define TEXTPAD 6
#define GRILL_WIDTH 10

#ifdef XFT
#include <X11/Xft/Xft.h>
#endif

#include "fspanel.h"

Display *dd;
Window root_win;
Pixmap generic_icon;
Pixmap generic_mask;
GC fore_gc;
taskbar tb;
int scr_screen;
int scr_depth;
int scr_width;
int scr_height;
int text_y;
int pager_size;

#ifdef XFT
XftDraw *xftdraw;
XftFont *xfs;
#else
XFontStruct *xfs;
#endif

struct colors
{
	unsigned short red, green, blue;
} cols[] =
{
	{0xd75c, 0xd75c, 0xd75c},		  /* 0. light gray */
	{0xbefb, 0xbaea, 0xbefb},		  /* 1. mid gray */
	{0xaefb, 0xaaea, 0xaefb},		  /* 2. dark gray */
	{0xefbe, 0xefbe, 0xefbe},		  /* 3. white */
	{0x8617, 0x8207, 0x8617},		  /* 4. darkest gray */
	{0x0000, 0x0000, 0x0000},		  /* 5. black */
    {0x0000, 0x0000, 0xefbe},         /* 6. blue */
    {0xefbe, 0x0000, 0x0000},         /* 7. red */
};

#define PALETTE_COUNT (sizeof (cols) / sizeof (cols[0].red) / 3)

unsigned long palette[PALETTE_COUNT];

char *atom_names[] = {
	/* clients */
	"KWM_WIN_ICON",
	"WM_STATE",
	"_MOTIF_WM_HINTS",
	"_NET_WM_STATE",
	"_NET_WM_STATE_SKIP_TASKBAR",
	"_NET_WM_STATE_SHADED",
	"_NET_WM_DESKTOP",
	"_NET_WM_WINDOW_TYPE",
	"_NET_WM_WINDOW_TYPE_DOCK", /* 8 */
	"_NET_WM_STRUT",
	"_WIN_HINTS",
	/* root */
	"_NET_CLIENT_LIST",
	"_NET_NUMBER_OF_DESKTOPS",
	"_NET_CURRENT_DESKTOP",
};

#define ATOM_COUNT (sizeof (atom_names) / sizeof (atom_names[0]))

Atom atoms[ATOM_COUNT];

#define atom_KWM_WIN_ICON atoms[0]
#define atom_WM_STATE atoms[1]
#define atom__MOTIF_WM_HINTS atoms[2]
#define atom__NET_WM_STATE atoms[3]
#define atom__NET_WM_STATE_SKIP_TASKBAR atoms[4]
#define atom__NET_WM_STATE_SHADED atoms[5]
#define atom__NET_WM_DESKTOP atoms[6]
#define atom__NET_WM_WINDOW_TYPE atoms[7]
#define atom__NET_WM_WINDOW_TYPE_DOCK atoms[8]
#define atom__NET_WM_STRUT atoms[9]
#define atom__WIN_HINTS atoms[10]
#define atom__NET_CLIENT_LIST atoms[11]
#define atom__NET_NUMBER_OF_DESKTOPS atoms[12]
#define atom__NET_CURRENT_DESKTOP atoms[13]


void *
get_prop_data (Window win, Atom prop, Atom type, int *items)
{
	Atom type_ret;
	int format_ret;
	unsigned long items_ret;
	unsigned long after_ret;
	unsigned char *prop_data;

	prop_data = 0;

	XGetWindowProperty (dd, win, prop, 0, 0x7fffffff, False,
							  type, &type_ret, &format_ret, &items_ret,
							  &after_ret, &prop_data);
	if (items)
		*items = items_ret;

	return prop_data;
}

void
set_foreground (int index)
{
	XSetForeground (dd, fore_gc, palette[index]);
}

void
draw_line (int x, int y, int a, int b)
{
	XDrawLine (dd, tb.win, fore_gc, x, y, a, b);
}

void
fill_rect (int x, int y, int a, int b)
{
	XFillRectangle (dd, tb.win, fore_gc, x, y, a, b);
}

void
scale_icon (task *tk)
{
	int xx, yy, x, y, w, h, d, bw;
	Pixmap pix, mk = None;
	XGCValues gcv;
	GC mgc;

	XGetGeometry (dd, tk->icon, &pix, &x, &y, &w, &h, &bw, &d);
	pix = XCreatePixmap (dd, tk->win, ICONWIDTH, ICONHEIGHT, scr_depth);

	if (tk->mask != None)
	{
		mk = XCreatePixmap (dd, tk->win, ICONWIDTH, ICONHEIGHT, 1);
		gcv.subwindow_mode = IncludeInferiors;
		gcv.graphics_exposures = False;
		mgc = XCreateGC (dd, mk, GCGraphicsExposures | GCSubwindowMode, &gcv);
	}

	set_foreground (5);

	/* this is my simple & dirty scaling routine */
	for (y = ICONHEIGHT - 1; y >= 0; y--)
	{
		yy = (y * h) / ICONHEIGHT;
		for (x = ICONWIDTH - 1; x >= 0; x--)
		{
			xx = (x * w) / ICONWIDTH;
			if (d != scr_depth)
				XCopyPlane (dd, tk->icon, pix, fore_gc, xx, yy, 1, 1, x, y, 1);
			else
				XCopyArea (dd, tk->icon, pix, fore_gc, xx, yy, 1, 1, x, y);
			if (mk != None)
				XCopyArea (dd, tk->mask, mk, mgc, xx, yy, 1, 1, x, y);
		}
	}

	if (mk != None)
	{
		XFreeGC (dd, mgc);
		tk->mask = mk;
	}

	tk->icon = pix;
}

void
get_task_hinticon (task *tk)
{
	XWMHints *hin;

	tk->icon = None;
	tk->mask = None;

	hin = (XWMHints *) get_prop_data (tk->win, XA_WM_HINTS, XA_WM_HINTS, 0);
	if (hin)
	{
		if ((hin->flags & IconPixmapHint))
		{
			if ((hin->flags & IconMaskHint))
			{
				tk->mask = hin->icon_mask;
			}

			tk->icon = hin->icon_pixmap;
			tk->icon_copied = 1;
			scale_icon (tk);
		}
		XFree (hin);
	}

	if (tk->icon == None)
	{
		tk->icon = generic_icon;
		tk->mask = generic_mask;
	}
}

void
get_task_kdeicon (task *tk)
{
	unsigned long *data;

	data = get_prop_data (tk->win, atom_KWM_WIN_ICON, atom_KWM_WIN_ICON, 0);
	if (data)
	{
		tk->icon = data[0];
		tk->mask = data[1];
		XFree (data);
	}
}

int
generic_get_int (Window win, Atom at)
{
	int num = 0;
	unsigned long *data;

	data = get_prop_data (win, at, XA_CARDINAL, 0);
	if (data)
	{
		num = *data;
		XFree (data);
	}
	return num;
}

int
find_desktop (Window win)
{
	return generic_get_int (win, atom__NET_WM_DESKTOP);
}

int
is_hidden (Window win)
{
	unsigned long *data;
	int ret = 0;
	int num;

	data = get_prop_data (win, atom__NET_WM_STATE, XA_ATOM, &num);
	if (data)
	{
		while (num)
		{
			num--;
			if ((data[num]) == atom__NET_WM_STATE_SKIP_TASKBAR)
				ret = 1;
		}
		XFree (data);
	}

	return ret;
}

int
is_iconified (Window win)
{
	unsigned long *data;
	int ret = 0;

	data = get_prop_data (win, atom_WM_STATE, atom_WM_STATE, 0);
	if (data)
	{
		if (data[0] == IconicState)
			ret = 1;
		XFree (data);
	}
	return ret;
}

int
get_current_desktop (void)
{
	return generic_get_int (root_win, atom__NET_CURRENT_DESKTOP);
}

int
get_number_of_desktops (void)
{
	return generic_get_int (root_win, atom__NET_NUMBER_OF_DESKTOPS);
}

void
add_task (Window win, int focus)
{
	task *tk, *list;

	if (win == tb.win)
		return;

	/* is this window on a different desktop? */
	if (tb.my_desktop != find_desktop (win) || is_hidden (win))
		return;

	tk = calloc (1, sizeof (task));
	tk->win = win;
	tk->focused = focus;
	tk->name = get_prop_data (win, XA_WM_NAME, XA_STRING, 0);
	tk->iconified = is_iconified (win);

	get_task_kdeicon (tk);
	if (tk->icon == None)
		get_task_hinticon (tk);

	XSelectInput (dd, win, PropertyChangeMask | FocusChangeMask |
					  StructureNotifyMask);

	/* now append it to our linked list */
	tb.num_tasks++;

	list = tb.task_list;
	if (!list)
	{
		tb.task_list = tk;
		return;
	}
	while (1)
	{
		if (!list->next)
		{
			list->next = tk;
			return;
		}
		list = list->next;
	}
}

void
gui_sync (void)
{
	XSync (dd, False);
}

void
set_prop (Window win, Atom at, Atom type, long val)
{
	XChangeProperty (dd, win, at, type, 32,
						  PropModeReplace, (unsigned char *) &val, 1);
}

Window
gui_create_taskbar (void)
{
	Window win;
	MWMHints mwm;
	XSizeHints size_hints;
	XWMHints wmhints;
	XSetWindowAttributes att;
	unsigned long strut[4];

	att.background_pixel = palette[5];
	att.event_mask = ButtonPressMask | ExposureMask;

	win = XCreateWindow (  /* display */ dd,
								  /* parent  */ root_win,
								  /* x       */ 0,
								  /* y       */ scr_height - WINHEIGHT,
								  /* width   */ WINWIDTH,
								  /* height  */ WINHEIGHT,
								  /* border  */ 0,
								  /* depth   */ CopyFromParent,
								  /* class   */ InputOutput,
								  /* visual  */ CopyFromParent,
								  /*value mask*/ CWBackPixel | CWEventMask,
								  /* attribs */ &att);

    /* reserve "WINHEIGHT" pixels at the bottom of the screen */
	strut[0] = 0;
	strut[1] = 0;
	strut[2] = 0;
	strut[3] = WINHEIGHT;
	XChangeProperty (dd, win, atom__NET_WM_STRUT, XA_CARDINAL, 32,
						  PropModeReplace, (unsigned char *) strut, 4);

	/* reside on ALL desktops */
	set_prop (win, atom__NET_WM_DESKTOP, XA_CARDINAL, 0xFFFFFFFF);
	set_prop (win, atom__NET_WM_WINDOW_TYPE, XA_ATOM,
				 atom__NET_WM_WINDOW_TYPE_DOCK);
	/* use old gnome hint since sawfish doesn't support _NET_WM_STRUT */
	set_prop (win, atom__WIN_HINTS, XA_CARDINAL,
				 WIN_HINTS_SKIP_FOCUS | WIN_HINTS_SKIP_WINLIST |
				 WIN_HINTS_SKIP_TASKBAR | WIN_HINTS_DO_NOT_COVER);

	/* borderless motif hint */
	bzero (&mwm, sizeof (mwm));
	mwm.flags = MWM_HINTS_DECORATIONS;
	XChangeProperty (dd, win, atom__MOTIF_WM_HINTS, atom__MOTIF_WM_HINTS, 32,
						  PropModeReplace, (unsigned char *) &mwm,
						  sizeof (MWMHints) / 4);

	/* make sure the WM obays our window position */
	size_hints.flags = PPosition;
	/*XSetWMNormalHints (dd, win, &size_hints);*/
	XChangeProperty (dd, win, XA_WM_NORMAL_HINTS, XA_WM_SIZE_HINTS, 32,
						  PropModeReplace, (unsigned char *) &size_hints,
						  sizeof (XSizeHints) / 4);

	/* make our window unfocusable */
	wmhints.flags = InputHint;
	wmhints.input = False;
	/*XSetWMHints (dd, win, &wmhints);*/
	XChangeProperty (dd, win, XA_WM_HINTS, XA_WM_HINTS, 32, PropModeReplace,
							(unsigned char *) &wmhints, sizeof (XWMHints) / 4);

	XMapWindow (dd, win);

#ifdef XFT
	xftdraw = XftDrawCreate (dd, win, DefaultVisual (dd, scr_screen), 
									 DefaultColormap (dd, scr_screen));
#endif


	XMoveWindow (dd, win, 0, scr_height - WINHEIGHT);

	return win;
}

void
gui_init (void)
{
	XGCValues gcv;
	XColor xcl;
	unsigned int i;
#ifndef XFT
	char *fontname;
#endif

	i = 0;
	do
	{
		xcl.red = cols[i].red;
		xcl.green = cols[i].green;
		xcl.blue = cols[i].blue;
		XAllocColor (dd, DefaultColormap (dd, scr_screen), &xcl);
		palette[i] = xcl.pixel;
		i++;
	}
	while (i < PALETTE_COUNT);

#ifdef XFT
	xfs = XftFontOpenName (dd, scr_screen, XFT_FONT);
#else
	fontname = FONT_NAME;
	do
	{
		xfs = XLoadQueryFont (dd, fontname);
		fontname = "fixed";
	}
	while (!xfs);
#endif

	gcv.graphics_exposures = False;
#ifdef XFT
	text_y = xfs->ascent + ((WINHEIGHT - (xfs->ascent + xfs->descent)) / 2);
	fore_gc = XCreateGC (dd, root_win, GCGraphicsExposures, &gcv);
#else
	text_y = xfs->ascent + ((WINHEIGHT - xfs->ascent) / 2);
	gcv.font = xfs->fid;
	fore_gc = XCreateGC (dd, root_win, GCFont | GCGraphicsExposures, &gcv);
#endif

#ifdef HAVE_XPM
	XpmCreatePixmapFromData (dd, root_win, icon_xpm, &generic_icon,
									 &generic_mask, NULL);
#else
	generic_icon = 0;
#endif
}

void
gui_draw_vline (int x)
{
	set_foreground (5);
	draw_line (x, 0, x, WINHEIGHT);
	set_foreground (4);
	draw_line (x + 1, 0, x + 1, WINHEIGHT);
}

void
draw_box (int x, int width)
{
    set_foreground (5);		  /* mid gray */
	fill_rect (x + 3, 2, width - 2, WINHEIGHT - 4);

	set_foreground (5);		  /* white */
	draw_line (x + 3, WINHEIGHT - 2, x + width - 1, WINHEIGHT - 2);
	draw_line (x + width - 1, 1, x + width - 1, WINHEIGHT - 2);

	set_foreground (5);		  /* darkest gray */
	draw_line (x + 3, 1, x + width - 1, 1);
	draw_line (x + 3, 2, x + 3, WINHEIGHT - 3);
}

void
gui_draw_task (task *tk)
{
	int len;
	int x = tk->pos_x;
	int taskw = tk->width;
#ifdef XFT
	XGlyphInfo ext;
	XftColor col;
#endif

	if (!tk->name)
		return;

	gui_draw_vline (x);

	if (tk->focused)
	{
		draw_box (x, taskw);
	} else
	{
		set_foreground (5);		  /* mid gray */
		fill_rect (x + 2, 0, taskw - 1, WINHEIGHT);
	}

	{
		register int text_x = x + TEXTPAD + TEXTPAD + ICONWIDTH;
#ifdef XFT

		/* check how many chars can fit */
		len = strlen (tk->name);
		while (len > 0)
		{
			XftTextExtents8 (dd, xfs, tk->name, len, &ext);
			if (ext.width < taskw - (text_x - x) - 2)
				break;
			len--;
		}

		col.color.alpha = 0xffff;

		if (tk->iconified)
		{
			/* draw task's name dark (iconified) */
			col.color.red = cols[3].red;
			col.color.green = cols[3].green;
			col.color.blue = cols[3].blue;
			XftDrawString8 (xftdraw, &col, xfs, text_x, text_y + 1, tk->name, len);
			col.color.red = cols[3].red;
			col.color.green = cols[3].green;
			col.color.blue = cols[3].blue;
		} else
		{
			col.color.red = cols[3].red;
			col.color.green = cols[3].green;
			col.color.blue = cols[3].blue;
		}

		/* draw task's name here */
		XftDrawString8 (xftdraw, &col, xfs, text_x, text_y, tk->name, len);

#else

		/* check how many chars can fit */
		len = strlen (tk->name);

		while (XTextWidth (xfs, tk->name, len) >= taskw - (text_x - x) - 2
				 && len > 0)
			len--;

		if (tk->iconified)
		{
			/* draw task's name dark (iconified) */
		    set_foreground (4);
			XDrawString (dd, tb.win, fore_gc, text_x, text_y + 1, tk->name, len);
			set_foreground (3);
		} else if (tk->focused)
		{
        	set_foreground (7);
		}
        else
        {
            set_foreground (4);
        }

		/* draw task's name here */
		XDrawString (dd, tb.win, fore_gc, text_x, text_y, tk->name, len);
#endif
	}

#ifndef HAVE_XPM
	if (!tk->icon)
		return;
#endif

	/* draw the task's icon */
	XSetClipMask (dd, fore_gc, tk->mask);
	XSetClipOrigin (dd, fore_gc, x + TEXTPAD, (WINHEIGHT - ICONHEIGHT) / 2);
	XCopyArea (dd, tk->icon, tb.win, fore_gc, 0, 0, ICONWIDTH, ICONHEIGHT,
				  x + TEXTPAD, (WINHEIGHT - ICONHEIGHT) / 2);
	XSetClipMask (dd, fore_gc, None);
}

void
draw_dot (int x, int y)
{
    set_foreground (5);
	XDrawPoint (dd, tb.win, fore_gc, x, y);
    set_foreground (4);
	XDrawPoint (dd, tb.win, fore_gc, x + 1, y + 1);
}

void
draw_grill (int x)
{
	int y = 0;
	while (y < WINHEIGHT - 4)
	{
		y += 3;
		draw_dot (x + 3, y);
		draw_dot (x, y);
	}
}

void
toggle_shade (Window win)
{
	XClientMessageEvent xev;

	xev.type = ClientMessage;
	xev.window = win;
	xev.message_type = atom__NET_WM_STATE;
	xev.format = 32;
	xev.data.l[0] = 2;	/* toggle */
	xev.data.l[1] = atom__NET_WM_STATE_SHADED;
	xev.data.l[2] = 0;
	XSendEvent (dd, root_win, False, SubstructureNotifyMask, (XEvent *) &xev);
}

#ifdef PAGER

void
switch_desk (int new_desk)
{
	XClientMessageEvent xev;

	if (get_number_of_desktops () <= new_desk)
		return;

	xev.type = ClientMessage;
	xev.window = root_win;
	xev.message_type = atom__NET_CURRENT_DESKTOP;
	xev.format = 32;
	xev.data.l[0] = new_desk;
	xev.data.l[1] = 0;
	xev.data.l[2] = 0;
	xev.data.l[3] = 0;
	xev.data.l[4] = 0;
	XSendEvent (dd, root_win, False, SubstructureRedirectMask | SubstructureNotifyMask, (XEvent *) &xev);
}

void
pager_draw_button (int x, int num)
{
	char label;
#ifdef XFT
	XftColor col;
#endif

	if (num == tb.my_desktop)
	{
		/* current desktop */
		draw_box (x, PAGER_BUTTON_WIDTH);
	} else
	{
		set_foreground (5);
		fill_rect (x, 1, PAGER_BUTTON_WIDTH+1, WINHEIGHT - 2);
	}

	label = '1' + num;

#ifdef XFT
	col.color.alpha = 0xffff;
	col.color.red = cols[5].red;
	col.color.green = cols[5].green;
	col.color.blue = cols[5].blue;
	XftDrawString8 (xftdraw, &col, xfs, x + ((PAGER_BUTTON_WIDTH - PAGER_DIGIT_WIDTH) / 2), text_y, &label, 1);
#else
    if (num == tb.my_desktop)
    {
        set_foreground (6);
    }
    else
    {
        set_foreground (4);
    }
	XDrawString (dd, tb.win, fore_gc,
	x + ((PAGER_BUTTON_WIDTH - PAGER_DIGIT_WIDTH) / 2) - 1, text_y, &label, 1);
#endif
}

void
pager_draw (void)
{
	int desks, i, x = GRILL_WIDTH;

	desks = get_number_of_desktops ();

	for (i = 0; i < desks; i++)
	{
		pager_draw_button (x, i);
		if (i > 8)
			break;
		x += PAGER_BUTTON_WIDTH;
	}

	pager_size = x;
}

#endif

void
gui_draw_taskbar (void)
{
	task *tk;
	int x, width, taskw;

#ifdef PAGER
	pager_draw ();
#else
	pager_size = GRILL_WIDTH;
#endif

	width = WINWIDTH - (pager_size + GRILL_WIDTH + GRILL_WIDTH);
	x = pager_size + 2;

	if (tb.num_tasks == 0)
		goto clear;

	taskw = width / tb.num_tasks;
	if (taskw > MAX_TASK_WIDTH)
		taskw = MAX_TASK_WIDTH;

	tk = tb.task_list;
	while (tk)
	{
		tk->pos_x = x;
		tk->width = taskw - 1;
		gui_draw_task (tk);
		x += taskw;
		tk = tk->next;
	}

	if (x < (width + pager_size + 2))
	{
clear:
		gui_draw_vline (x);
		set_foreground (5);
		fill_rect (x + 2, 0, WINWIDTH, WINHEIGHT);
	}

	gui_draw_vline (8);
	gui_draw_vline (WINWIDTH - 8);

	draw_grill (2);
	draw_grill (WINWIDTH - 6);
}

task *
find_task (Window win)
{
	task *list = tb.task_list;
	while (list)
	{
		if (list->win == win)
			return list;
		list = list->next;
	}
	return 0;
}

void
del_task (Window win)
{
	task *next, *prev = 0, *list = tb.task_list;

	while (list)
	{
		next = list->next;
		if (list->win == win)
		{
			/* unlink and free this task */
			tb.num_tasks--;
			if (list->icon_copied)
			{
				XFreePixmap (dd, list->icon);
				if (list->mask != None)
					XFreePixmap (dd, list->mask);
			}
			if (list->name)
				XFree (list->name);
			free (list);
			if (prev == 0)
				tb.task_list = next;
			else
				prev->next = next;
			return;
		}
		prev = list;
		list = next;
	}
}

void
taskbar_read_clientlist (void)
{
	Window *win, focus_win;
	int num, i, rev, desk, new_desk = 0;
	task *list, *next;

	desk = get_current_desktop ();
	if (desk != tb.my_desktop)
	{
		new_desk = 1;
		tb.my_desktop = desk;
	}

	XGetInputFocus (dd, &focus_win, &rev);

	win = get_prop_data (root_win, atom__NET_CLIENT_LIST, XA_WINDOW, &num);
	if (!win)
		return;

	/* remove windows that arn't in the _NET_CLIENT_LIST anymore */
	list = tb.task_list;
	while (list)
	{
		list->focused = (focus_win == list->win);
		next = list->next;

		if (!new_desk)
			for (i = num - 1; i >= 0; i--)
				if (list->win == win[i])
					goto dontdel;
		del_task (list->win);
dontdel:

		list = next;
	}

	/* add any new windows */
	for (i = 0; i < num; i++)
	{
		if (!find_task (win[i]))
			add_task (win[i], (win[i] == focus_win));
	}

	XFree (win);
}

void
move_taskbar (void)
{
	int x, y;

	x = y = 0;

	if (tb.hidden)
		x = WINWIDTH - TEXTPAD;

	if (!tb.at_top)
		y = scr_height - WINHEIGHT;

	XMoveWindow (dd, tb.win, x, y);
}

void
handle_press (int x, int y, int button)
{
	task *tk;

#ifdef PAGER
	if (y > 2 && y < WINHEIGHT - 2)
		switch_desk ((x - GRILL_WIDTH) / PAGER_BUTTON_WIDTH);
#endif

	/* clicked left grill */
	if (x < 6)
	{
		if (tb.hidden)
			tb.hidden = 0;
		else
			tb.at_top = !tb.at_top;
		move_taskbar ();
		return;
	}

	/* clicked right grill */
	if (x + TEXTPAD > WINWIDTH)
	{
		tb.hidden = !tb.hidden;
		move_taskbar ();
		return;
	}

	tk = tb.task_list;
	while (tk)
	{
		if (x > tk->pos_x && x < tk->pos_x + tk->width)
		{
			if (button == 3)	/* right-click */
			{
				toggle_shade (tk->win);
				return;
			}

			if (tk->iconified)
			{
				tk->iconified = 0;
				tk->focused = 1;
				XMapWindow (dd, tk->win);
			} else
			{
				if (tk->focused)
				{
					tk->iconified = 1;
					tk->focused = 0;
					XIconifyWindow (dd, tk->win, scr_screen);
				} else
				{
					tk->focused = 1;
					XRaiseWindow (dd, tk->win);
					XSetInputFocus (dd, tk->win, RevertToNone, CurrentTime);
				}
			}
			gui_sync ();
			gui_draw_task (tk);
		} else
		{
			if (button == 1 && tk->focused)
			{
				tk->focused = 0;
				gui_draw_task (tk);
			}
		}

		tk = tk->next;
	}
}

void
handle_focusin (Window win)
{
	task *tk;

	tk = tb.task_list;
	while (tk)
	{
		if (tk->focused)
		{
			if (tk->win != win)
			{
				tk->focused = 0;
				gui_draw_task (tk);
			}
		} else
		{
			if (tk->win == win)
			{
				tk->focused = 1;
				gui_draw_task (tk);
			}
		}
		tk = tk->next;
	}
}

void
handle_propertynotify (Window win, Atom at)
{
	task *tk;

	if (win == root_win)
	{
		if (at == atom__NET_CLIENT_LIST || at == atom__NET_CURRENT_DESKTOP)
		{
			taskbar_read_clientlist ();
			gui_draw_taskbar ();
		}
		return;
	}

	tk = find_task (win);
	if (!tk)
		return;

	if (at == XA_WM_NAME)
	{
		/* window's title changed */
		if (tk->name)
			XFree (tk->name);
		tk->name = get_prop_data (tk->win, XA_WM_NAME, XA_STRING, 0);
		gui_draw_task (tk);
	} else if (at == atom_WM_STATE)
	{
		/* iconified state changed? */
		if (is_iconified (tk->win) != tk->iconified)
		{
			tk->iconified = !tk->iconified;
			gui_draw_task (tk);
		}
	} else if (at == XA_WM_HINTS)
	{
		/* some windows set their WM_HINTS icon after mapping */
		if (tk->icon == generic_icon)
		{
			get_task_hinticon (tk);
			gui_draw_task (tk);
		}
	}
}

void
handle_error (Display *d, XErrorEvent *ev)
{
}

int
#ifdef NOSTDLIB
_start (void)
#else
main (int argc, char *argv[])
#endif
{
	XEvent ev;
	fd_set fd;
	int xfd;

	dd = XOpenDisplay (NULL);
	if (!dd)
		return 0;
	scr_screen = DefaultScreen (dd);
	scr_depth = DefaultDepth (dd, scr_screen);
	scr_height = DisplayHeight (dd, scr_screen);
	scr_width = DisplayWidth (dd, scr_screen);
	root_win = RootWindow (dd, scr_screen);

	/* helps us catch windows closing/opening */
	XSelectInput (dd, root_win, PropertyChangeMask);

	XSetErrorHandler ((XErrorHandler) handle_error);

	XInternAtoms (dd, atom_names, ATOM_COUNT, False, atoms);

	gui_init ();
	bzero (&tb, sizeof (struct taskbar));
	tb.win = gui_create_taskbar ();
	xfd = ConnectionNumber (dd);
	gui_sync ();

	while (1)
	{
		FD_ZERO (&fd);
		FD_SET (xfd, &fd);
		select (xfd + 1, &fd, 0, 0, 0);

		while (XPending (dd))
		{
			XNextEvent (dd, &ev);
			switch (ev.type)
			{
			case ButtonPress:
				handle_press (ev.xbutton.x, ev.xbutton.y, ev.xbutton.button);
				break;
			case DestroyNotify:
				del_task (ev.xdestroywindow.window);
				/* fall through */
			case Expose:
				gui_draw_taskbar ();
				break;
			case PropertyNotify:
				handle_propertynotify (ev.xproperty.window, ev.xproperty.atom);
				break;
			case FocusIn:
				handle_focusin (ev.xfocus.window);
				break;
			/*default:
				   printf ("unknown evt type: %d\n", ev.type);*/
			}
		}
	}

	/*XCloseDisplay (dd);

   return 0;*/
}
