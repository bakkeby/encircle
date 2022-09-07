/* See LICENSE file for copyright and license details. */

#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <X11/X.h>
#include <X11/extensions/XInput2.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#include "util.h"

/* macros */
#define SNAP(c,p,s)              ((c) < (p) ? (p) + snap_offset : (c) > (p) + (s) ? (p) + (s) - snap_offset : (c))
#define XINTERSECT(x,w,m)        MAX(0, MIN((x)+(w),(m)->mx+(m)->mw) - MAX((x),(m)->mx))
#define YINTERSECT(y,h,m)        MAX(0, MIN((y)+(h),(m)->my+(m)->mh) - MAX((y),(m)->my))
#define INTERSECT(x,y,w,h,m)     (XINTERSECT((x),(w),(m)) * YINTERSECT((y),(h),(m)))
#define BORDER_OVERLAPS(m, o)  ((BETWEEN((m)->mx, (o)->mx, (o)->mx + (o)->mw) \
                              || BETWEEN((m)->mx + (m)->mw - 1, (o)->mx, (o)->mx + (o)->mw)))
#define arg(A) (!strcmp(argv[i], A))

typedef struct Monitor Monitor;
struct Monitor {
	int num;
	int mx, my, mw, mh; /* screen size */
	Monitor *next;
};

static void cleanup(void);
static void cleanupmon(Monitor *mon);
static void configurenotify(XEvent *e);
static Monitor *createmon(void);
static Monitor *leftof(Monitor *m, int y);
static Monitor *above(Monitor *m, int x);
static Monitor *rightof(Monitor *m, int y);
static Monitor *below(Monitor *m, int x);
static Monitor *recttomon(int x, int y, int w, int h);
static void genericevent(XEvent *e);
#ifdef XINERAMA
static int isuniquegeom(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info);
#endif /* XINERAMA */
static void run(void);
static void quit(int unused);
static void setup(void);
static void updategeom(int width, int height);
static void usage(void);

static volatile int running = 1;
static int wrap_x = 0; /* allow monitor wrap on the x-axis */
static int wrap_y = 0; /* allow monitor wrap on the y-axis */
static int snap_only = 0;
static int snap_offset = 10; /* snap offset, the number of pixels to shift cursor when snapping */
static int px, py; /* previous cursor x and y position */
static int screen;
static int xi_opcode;
static Display *dpy;
static Window root;
static Monitor *mons;
static void (*handler[LASTEvent]) (XEvent *) = {
	[ConfigureNotify] = configurenotify,
	[GenericEvent] = genericevent,
};

void
cleanup(void)
{
	while (mons)
		cleanupmon(mons);
}

void
cleanupmon(Monitor *mon)
{
	Monitor *m;

	if (mon == mons)
		mons = mons->next;
	else {
		for (m = mons; m && m->next != mon; m = m->next);
		m->next = mon->next;
	}

	free(mon);
}

void
configurenotify(XEvent *e)
{
	XConfigureEvent *ev = &e->xconfigure;

	if (ev->window != root)
		return;

	updategeom(ev->width, ev->height);
}

void
genericevent(XEvent *e)
{
	int x, y, dx, dy, nx, ny;
	int di;
	unsigned int dui;
	Window dummy;
	Monitor *m = NULL, *o;

	if (e->xcookie.extension != xi_opcode)
		return;

	if (!XGetEventData(dpy, &e->xcookie))
		return;

	/* On each RawMotion event, retrieve the pointer location and move the pointer if necessary. */
	if (e->xcookie.evtype != XI_RawMotion)
		goto bail;

	if (!XQueryPointer(dpy, root, &dummy, &dummy, &x, &y, &di, &di, &dui))
		goto bail;

	if (!(o = recttomon(x, y, 1, 1)))
		goto bail;

	dx = x - px;
	dy = y - py;
	nx = x;
	ny = y;

	if (y == o->my && dy < 0) {
		if (wrap_y && (m = above(o, x)))
			ny = m->my + m->mh - 2;
	} else if (y == o->my + o->mh - 1 && dy > 0) {
		if (wrap_y && (m = below(o, x)))
			ny = m->my + 1;
	} else if (x == o->mx && dx < 0) {
		if (wrap_x && (m = leftof(o, y)))
			nx = m->mx + m->mw - 2;
	} else if (x == o->mx + o->mw - 1 && dx > 0) {
		if (wrap_x && (m = rightof(o, y)))
			nx = m->mx + 1;
	}

	if (nx != x || ny != y) {
		if (m != o && nx == x)
			nx = SNAP(x, m->mx, m->mw);
		if (m != o && ny == y)
			ny = SNAP(y, m->my, m->mh);
		XWarpPointer(dpy, None, root, 0, 0, 0, 0, nx, ny);
	}

	px = nx;
	py = ny;

bail:
	XFreeEventData(dpy, &e->xcookie);
}

Monitor *
above(Monitor *o, int x)
{
	Monitor *m, *r = NULL;
	int max_y = 0;

	for (m = mons; m; m = m->next)
		if (m->my + m->mh == o->my && XINTERSECT(m->mx, m->mw, o))
			return BETWEEN(x, m->mx, m->mx + m->mw) ? NULL : m;

	for (m = mons; m && !snap_only; m = m->next) {
		if (m->my >= max_y && XINTERSECT(m->mx, m->mw, o)) {
			r = m;
			max_y = r->my;
		}
	}

	return r;
}

Monitor *
below(Monitor *o, int x)
{
	Monitor *m, *r = NULL;
	int min_y = INT_MAX;

	for (m = mons; m; m = m->next)
		if (m->my == o->my + o->mh && XINTERSECT(m->mx, m->mw, o))
			return BETWEEN(x, m->mx, m->mx + m->mw) ? NULL : m;

	for (m = mons; m && !snap_only; m = m->next) {
		if (m->my <= min_y && XINTERSECT(m->mx, m->mw, o)) {
			r = m;
			min_y = r->my;
		}
	}

	return r;
}

Monitor *
leftof(Monitor *o, int y)
{
	Monitor *m, *r = NULL;
	int max_x = 0;

	for (m = mons; m; m = m->next)
		if (m->mx + m->mw == o->mx && YINTERSECT(m->my, m->mh, o))
			return BETWEEN(y, m->my, m->my + m->mh) ? NULL : m;

	for (m = mons; m && !snap_only; m = m->next) {
		if (m->mx >= max_x && YINTERSECT(m->my, m->mh, o)) {
			r = m;
			max_x = r->mx;
		}
	}

	return r;
}

Monitor *
rightof(Monitor *o, int y)
{
	Monitor *m, *r = NULL;
	int min_x = INT_MAX;

	for (m = mons; m; m = m->next)
		if (m->mx == o->mx + o->mw && YINTERSECT(m->my, m->mh, o))
			return BETWEEN(y, m->my, m->my + m->mh) ? NULL : m;

	for (m = mons; m && !snap_only; m = m->next) {
		if (m->mx <= min_x && YINTERSECT(m->my, m->mh, o)) {
			r = m;
			min_x = r->mx;
		}
	}

	return r;
}

Monitor *
createmon(void)
{
	return ecalloc(1, sizeof(Monitor));
}

#ifdef XINERAMA
int
isuniquegeom(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info)
{
	while (n--)
		if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
		&& unique[n].width == info->width && unique[n].height == info->height)
			return 0;
	return 1;
}
#endif /* XINERAMA */

void
run(void)
{
	XEvent ev;

	/* Tell XInput to send us all RawMotion events.
	 * (Normal Motion events are blocked by some windows.) */
	unsigned char mask_bytes[XIMaskLen(XI_RawMotion)];
	memset(mask_bytes, 0, sizeof(mask_bytes));
	XISetMask(mask_bytes, XI_RawMotion);

	XIEventMask mask;
	mask.deviceid = XIAllMasterDevices;
	mask.mask_len = sizeof(mask_bytes);
	mask.mask = mask_bytes;
	XISelectEvents(dpy, root, &mask, 1);

	/* main event loop */
	while (running) {
		struct pollfd pfd = {
			.fd = ConnectionNumber(dpy),
			.events = POLLIN,
		};
		int pending = XPending(dpy) > 0 || poll(&pfd, 1, -1) > 0;

		if (!running)
			break;
		if (!pending)
			continue;

		XNextEvent(dpy, &ev);
		if (handler[ev.type])
			handler[ev.type](&ev); /* call handler */
	}
}

Monitor *
recttomon(int x, int y, int w, int h)
{
	Monitor *m, *r = NULL;
	int a, area = 0;

	for (m = mons; m; m = m->next)
		if ((a = INTERSECT(x, y, w, h, m)) > area) {
			area = a;
			r = m;
		}
	return r;
}

void
setup(void)
{
	XSetWindowAttributes wa;

	signal(SIGINT, quit);
	signal(SIGTERM, quit);
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);

	wa.event_mask = StructureNotifyMask;
	XChangeWindowAttributes(dpy, root, CWEventMask, &wa);

	updategeom(DisplayWidth(dpy, screen), DisplayHeight(dpy, screen));
}

void
quit(int unused)
{
	running = 0;
}

void
usage(void)
{
	fprintf(stdout, "usage: encircle [-hvfxy]\n\n" );
	fprintf(stdout, "encircle is a window manager agnostic tool that wraps the X cursor ");
	fprintf(stdout, "around the edges of the screen and is specifically designed to work with ");
	fprintf(stdout, "asymmetric multi-monitor Xinerama setups.\n\n");

	char ofmt[] = "   %-10s%s\n";
	fprintf(stdout, "Options:\n");
	fprintf(stdout, ofmt, "-h", "print this help section");
	fprintf(stdout, ofmt, "-v", "print version information and exit");
	fprintf(stdout, ofmt, "-f", "fork the process (i.e run in the background)");
	fprintf(stdout, ofmt, "-x", "enable cursor wrapping on the x-axis");
	fprintf(stdout, ofmt, "-y", "enable cursor wrapping on the y-axis");
	fprintf(stdout, ofmt, "-s", "snap only, disables wrapping across outer screen edges");


	fprintf(stdout, "\nBy default cursor wrapping is enabled on both x and y axes.\n");
	fprintf(stdout, "\nSee the man page for more details.\n\n");
	exit(0);
}

void
updategeom(int width, int height)
{
#ifdef XINERAMA
	if (XineramaIsActive(dpy)) {
		int i, j, n, nn;
		Monitor *m;
		XineramaScreenInfo *info = XineramaQueryScreens(dpy, &nn);
		XineramaScreenInfo *unique = NULL;

		for (n = 0, m = mons; m; m = m->next, n++);
		/* only consider unique geometries as separate screens */
		unique = ecalloc(nn, sizeof(XineramaScreenInfo));
		for (i = 0, j = 0; i < nn; i++)
			if (isuniquegeom(unique, j, &info[i]))
				memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
		XFree(info);
		nn = j;

		/* new monitors if nn > n */
		for (i = n; i < nn; i++) {
			for (m = mons; m && m->next; m = m->next);
			if (m)
				m->next = createmon();
			else
				mons = createmon();
		}
		for (i = 0, m = mons; i < nn && m; m = m->next, i++)
			if (i >= n
			|| unique[i].x_org != m->mx || unique[i].y_org != m->my
			|| unique[i].width != m->mw || unique[i].height != m->mh)
			{
				m->num = i;
				m->mx = unique[i].x_org;
				m->my = unique[i].y_org;
				m->mw = unique[i].width;
				m->mh = unique[i].height;
			}
		/* removed monitors if n > nn */
		for (i = nn; i < n; i++) {
			for (m = mons; m && m->next; m = m->next);
			cleanupmon(m);
		}
		free(unique);
	} else
#endif /* XINERAMA */
	{ /* default monitor setup */
		if (!mons)
			mons = createmon();
		mons->mw = width;
		mons->mh = height;
	}
}

int
main(int argc, char *argv[])
{
	int junk, i;

	for (i = 1; i < argc; i++) {
		if (arg("-v") || arg("--version")) { /* prints version information */
			puts("encircle-"VERSION);
			exit(0);
		} else if arg("-x") { /* wrap on x-axis only */
			wrap_x = 1;
		} else if arg("-y") { /* wrap on y-axis only */
			wrap_y = 1;
		} else if (arg("-s") || arg("--snap-only")) {
			snap_only = 1;
		} else if (arg("-f") || arg("--fork")) {
			if (fork() != 0)
				exit(EXIT_SUCCESS);
		} else if (arg("-h") || arg("--help")) {
			usage();
		} else {
			fprintf(stderr, "Unknown argument: %s\n", argv[i]);
			usage();
		}
	}

	if (!wrap_x && !wrap_y) {
		wrap_x = 1;
		wrap_y = 1;
	}

	if (!(dpy = XOpenDisplay(NULL)))
		die("encircle: cannot open display");
	if (!XQueryExtension(dpy, "XInputExtension", &xi_opcode, &junk, &junk))
		die("XInput is not available.");
	setup();
	run();
	cleanup();
	XCloseDisplay(dpy);
	return EXIT_SUCCESS;
}
