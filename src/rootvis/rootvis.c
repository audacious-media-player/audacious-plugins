#include <string.h>
#include <math.h>
#include <pthread.h>
#include <time.h>

#include "rootvis.h"
// as imlib2 uses X definitions, it has to be included after the X includes, which are done in rootvis.h
#include <Imlib2.h>
#include "config.h"

extern Window ToonGetRootWindow(Display*, int, Window*);

// Forward declarations
static void rootvis_init(void);
static void rootvis_cleanup(void);
static void rootvis_about(void);
static void rootvis_configure(void);
static void rootvis_playback_start(void);
static void rootvis_playback_stop(void);
static void rootvis_render_freq(gint16 freq_data[2][256]);

static gboolean plugin_is_initted = FALSE;

// Callback functions
VisPlugin rootvis_vtable = {
	.description = "Root Spectrum Analyzer 0.0.8",
	.num_pcm_chs_wanted = 0,
	.num_freq_chs_wanted = 2,
	.init = rootvis_init,
	.cleanup = rootvis_cleanup,
	.configure = rootvis_configure,
	.playback_start = rootvis_playback_start,
	.playback_stop = rootvis_playback_stop,
	.render_freq = rootvis_render_freq
};

VisPlugin *rootvis_vplist[] = { &rootvis_vtable, NULL };

SIMPLE_VISUAL_PLUGIN(rootvis, rootvis_vplist);

// X related
struct rootvis_x {
	int screen;
	Display *display;
	Window rootWin, Parent;
	Pixmap rootBg;
	GC gc;

	Visual *vis;
	Colormap cm;
	Imlib_Image background;
	Imlib_Image buffer;
};

// thread talk

struct rootvis_threads {
	gint16 freq_data[2][256];
	pthread_t worker[2];
	pthread_mutex_t mutex1;
	enum {GO, STOP} control;
	char dirty;
	/*** dirty flaglist ***
	  1: channel 1 geometry change
	  2: channel 1 color change
	  4: channel 2 geometry change
	  8: channel 2 color change
	 16: no data yet (don't do anything)
	 32: switch mono/stereo
	*/
} threads;

// For use in config_backend:

void threads_lock(void) {
	print_status("Locking");
	pthread_mutex_lock(&threads.mutex1);
}

void threads_unlock(char dirty) {
	print_status("Unlocking");
	threads.dirty = threads.dirty & dirty;
	pthread_mutex_unlock(&threads.mutex1);
}

// Some helper stuff

void clean_data(void) {
	pthread_mutex_lock(&threads.mutex1);
	memset(threads.freq_data, 0, sizeof(gint16) * 2 * 256);
	pthread_mutex_unlock(&threads.mutex1);
}

void print_status(char msg[]) {
	if (conf.debug == 1) printf(">> rootvis >> %s\n", msg); // for debug purposes, but doesn't tell much anyway
}

void error_exit(char msg[]) {
	printf("*** ERROR (rootvis): %s\n", msg);
	rootvis_vtable.disable_plugin(&rootvis_vtable);
}

void initialize_X(struct rootvis_x* drw, char* display) {
	print_status("Opening X Display");
	drw->display = XOpenDisplay(display);
	if (drw->display == NULL) {
		fprintf(stderr, "cannot connect to X server %s\n",
			getenv("DISPLAY") ? getenv("DISPLAY") : "(default)");
		error_exit("Connecting to X server failed");
		pthread_exit(NULL);
	}
	print_status("Getting screen and window");
	drw->screen = DefaultScreen(drw->display);
	drw->rootWin = ToonGetRootWindow(drw->display, drw->screen, &drw->Parent);

	print_status("Initializing Imlib2");

	drw->vis   = DefaultVisual(drw->display, drw->screen);
	drw->cm    = DefaultColormap(drw->display, drw->screen);

	imlib_context_set_display(drw->display);
	imlib_context_set_visual(drw->vis);
	imlib_context_set_colormap(drw->cm);

	imlib_context_set_dither(0);
	imlib_context_set_blend(1);
}

void draw_init(struct rootvis_x* drw, unsigned short damage_coords[4])
{
	Atom tmp_rootmapid, tmp_type;
    int tmp_format;
    unsigned long tmp_length, tmp_after;
    unsigned char *data = NULL;

	if ((tmp_rootmapid = XInternAtom(drw->display, "_XROOTPMAP_ID", True)) != None)
	{
		int ret = XGetWindowProperty(drw->display, drw->rootWin, tmp_rootmapid, 0L, 1L, False, AnyPropertyType,
										&tmp_type, &tmp_format, &tmp_length, &tmp_after,&data);
		if ((ret == Success)&&(tmp_type == XA_PIXMAP)&&((drw->rootBg = *((Pixmap *)data)) != None)) {
			pthread_mutex_lock(&threads.mutex1);
			imlib_context_set_drawable(drw->rootBg);
			drw->background = imlib_create_image_from_drawable(0, damage_coords[0], damage_coords[1], damage_coords[2], damage_coords[3], 1);
			pthread_mutex_unlock(&threads.mutex1);
		}
		if (drw->background == NULL)
			error_exit("Initial image could not be created");
	}
}

void draw_close(struct rootvis_x* drw, unsigned short damage_coords[4]) {
	pthread_mutex_lock(&threads.mutex1);
	imlib_context_set_image(drw->background);
	imlib_render_image_on_drawable(damage_coords[0], damage_coords[1]);
	XClearArea(drw->display, drw->rootWin, damage_coords[0], damage_coords[1], damage_coords[2], damage_coords[3], True);
	imlib_free_image();
	pthread_mutex_unlock(&threads.mutex1);
}

void draw_start(struct rootvis_x* drw, unsigned short damage_coords[4]) {
	imlib_context_set_image(drw->background);
	drw->buffer = imlib_clone_image();
	imlib_context_set_image(drw->buffer);
}

void draw_end(struct rootvis_x* drw, unsigned short damage_coords[4]) {
	imlib_context_set_drawable(drw->rootWin);
	imlib_render_image_on_drawable(damage_coords[0], damage_coords[1]);
	imlib_free_image();
}

void draw_bar(struct rootvis_x* drw, int t, int i, unsigned short level, unsigned short oldlevel, unsigned short peak, unsigned short oldpeak) {

	/* to make following cleaner, we work with redundant helper variables
	   this also avoids some calculations */
	register int a, b, c, d;
	float angle;
	Imlib_Color_Range range = imlib_create_color_range();

	if (conf.geo[t].orientation < 2) {
		a = i*(conf.bar[t].width + conf.bar[t].shadow + conf.geo[t].space);
		c = conf.bar[t].width;
		b = d = 0;
	} else {
		b = (conf.data[t].cutoff/conf.data[t].div - i - 1)
			*(conf.bar[t].width + conf.bar[t].shadow + conf.geo[t].space);
		d = conf.bar[t].width;
		a = c = 0;
	}

	if (conf.geo[t].orientation == 0) {	b = conf.geo[t].height - level; d = level; }
	else if (conf.geo[t].orientation == 1) { b = 0; d = level; }
	else if (conf.geo[t].orientation == 2) { a = 0; c = level; }
	else	{ a = conf.geo[t].height - level; c = level; }

	if (conf.bar[t].shadow > 0) {
		imlib_context_set_color(conf.bar[t].shadow_color[0], conf.bar[t].shadow_color[1],
								conf.bar[t].shadow_color[2], conf.bar[t].shadow_color[3]);
		if (conf.bar[t].gradient)
			imlib_image_fill_rectangle(a + conf.bar[t].shadow, b + conf.bar[t].shadow, c, d);
		else if (conf.bar[t].bevel)
			imlib_image_draw_rectangle(a + conf.bar[t].shadow, b + conf.bar[t].shadow, c, d);

		if (conf.peak[t].shadow > 0)
		{
			int aa = a, bb = b, cc = c, dd = d;
			if (conf.geo[t].orientation == 0) {	bb = conf.geo[t].height - peak; dd = 1; }
			else if (conf.geo[t].orientation == 1) { bb = peak - 1; dd = 1; }
			else if (conf.geo[t].orientation == 2) { aa = peak - 1; cc = 1; }
			else	{ aa = conf.geo[t].height - peak; cc = 1; }
			imlib_image_fill_rectangle(aa + conf.bar[t].shadow, bb + conf.bar[t].shadow, cc, dd);
		}
	}

	if (conf.bar[t].gradient)
	{
		switch (conf.geo[t].orientation) {
			case 0:	angle = 0.0; break;
			case 1:	angle = 180.0; break;
			case 2:	angle = 90.0; break;
			case 3:	default:
					angle = -90.0;
		}

		imlib_context_set_color_range(range);
		imlib_context_set_color(conf.bar[t].color[3][0], conf.bar[t].color[3][1], conf.bar[t].color[3][2], conf.bar[t].color[3][3]);
		imlib_add_color_to_color_range(0);
		imlib_context_set_color(conf.bar[t].color[2][0], conf.bar[t].color[2][1], conf.bar[t].color[2][2], conf.bar[t].color[2][3]);
		imlib_add_color_to_color_range(level * 2 / 5);
		imlib_context_set_color(conf.bar[t].color[1][0], conf.bar[t].color[1][1], conf.bar[t].color[1][2], conf.bar[t].color[1][3]);
		imlib_add_color_to_color_range(level * 4 / 5);
		imlib_context_set_color(conf.bar[t].color[0][0], conf.bar[t].color[0][1], conf.bar[t].color[0][2], conf.bar[t].color[0][3]);
		imlib_add_color_to_color_range(level);
		imlib_image_fill_color_range_rectangle(a, b, c, d, angle);
		imlib_free_color_range();
	}

	if (conf.bar[t].bevel)
	{
		imlib_context_set_color(conf.bar[t].bevel_color[0], conf.bar[t].bevel_color[1],
								conf.bar[t].bevel_color[2], conf.bar[t].bevel_color[3]);
		imlib_image_draw_rectangle(a, b, c, d);
	}

	if (peak > 0) {
		if (conf.geo[t].orientation == 0) {	b = conf.geo[t].height - peak; d = 1; }
		else if (conf.geo[t].orientation == 1) { b = peak - 1; d = 1; }
		else if (conf.geo[t].orientation == 2) { a = peak - 1; c = 1; }
		else	{ a = conf.geo[t].height - peak; c = 1; }
		imlib_context_set_color(conf.peak[t].color[0], conf.peak[t].color[1], conf.peak[t].color[2], conf.peak[t].color[3]);
		imlib_image_fill_rectangle(a, b, c, d);
	}
}

// Our worker thread

void* worker_func(void* threadnump) {
	struct rootvis_x draw;
	gint16 freq_data[256];
	double scale = 0.0, x00 = 0.0, y00 = 0.0;
	unsigned int threadnum, i, j, level;
	unsigned short damage_coords[4];
	unsigned short *level1 = NULL, *level2 = NULL, *levelsw, *peak1 = NULL, *peak2 = NULL, *peakstep;
	int barcount = 0;

	if (threadnump == NULL) threadnum = 0; else threadnum = 1;

	print_status("Memory allocations");
	level1 = (unsigned short*)calloc(256, sizeof(short)); // need to be zeroed out
	level2 = (unsigned short*)malloc(256*sizeof(short));
	peak1 = (unsigned short*)calloc(256, sizeof(short)); // need to be zeroed out
	peak2 = (unsigned short*)calloc(256, sizeof(short)); // need to be zeroed out for disabled peaks
	peakstep = (unsigned short*)calloc(256, sizeof(short)); // need to be zeroed out
	if ((level1 == NULL)||(level2 == NULL)||(peak1 == NULL)||(peak2 == NULL)||(peakstep == NULL)) {
		error_exit("Allocation of memory failed");
		pthread_exit(NULL);
	}
	print_status("Allocations done");

	draw.display = NULL;

	while (threads.control != STOP) {

		{
			//print_status("start sleep");
			struct timespec sleeptime;
			sleeptime.tv_sec = 0;
			sleeptime.tv_nsec = 999999999 / conf.data[threadnum].fps;
			while (nanosleep(&sleeptime, &sleeptime) == -1) {}; //print_status("INTR");
			//print_status("end sleep");
		}

		/* we will unset our own dirty flags after receiving them */
		pthread_mutex_lock(&threads.mutex1);
		memcpy(&freq_data, &threads.freq_data[threadnum], sizeof(gint16)*256);
		i = threads.dirty;
		if ((i & 16) == 0) threads.dirty = i & (~(3 + threadnum*9));
		pthread_mutex_unlock(&threads.mutex1);

		if ((i & 16) == 0) { // we've gotten data
			if (draw.display == NULL)	initialize_X(&draw, conf.geo[threadnum].display);
			else if (i & (1 + threadnum*3)) draw_close(&draw, damage_coords);

			if (i & (1 + threadnum*3)) {	// geometry has changed
				damage_coords[0] = conf.geo[threadnum].posx;
				damage_coords[1] = conf.geo[threadnum].posy;
				if (conf.geo[threadnum].orientation < 2) {
					damage_coords[2] = conf.data[threadnum].cutoff/conf.data[threadnum].div
						*(conf.bar[threadnum].width + conf.bar[threadnum].shadow + conf.geo[threadnum].space);
					damage_coords[3] = conf.geo[threadnum].height + conf.bar[threadnum].shadow;
				} else {
					damage_coords[2] = conf.geo[threadnum].height + conf.bar[threadnum].shadow;
					damage_coords[3] = conf.data[threadnum].cutoff/conf.data[threadnum].div
						*(conf.bar[threadnum].width + conf.bar[threadnum].shadow + conf.geo[threadnum].space);
				}
				print_status("Geometry recalculations");
				scale = conf.geo[threadnum].height /
					(log((1 - conf.data[threadnum].linearity) / conf.data[threadnum].linearity) * 4);
				x00 = conf.data[threadnum].linearity*conf.data[threadnum].linearity*32768.0 /
					(2*conf.data[threadnum].linearity - 1);
				y00 = -log(-x00) * scale;
				barcount = conf.data[threadnum].cutoff/conf.data[threadnum].div;
				memset(level1, 0, 256*sizeof(short));
				memset(peak1, 0, 256*sizeof(short));
				memset(peak2, 0, 256*sizeof(short));

				draw_init(&draw, damage_coords);
			}
			/*if (i & (2 + threadnum*6)) {	// colors have changed
			}*/

			/* instead of copying the old level array to the second array,
				we just tell the first is now the second one */
			levelsw = level1;
			level1 = level2;
			level2 = levelsw;
			levelsw = peak1;
			peak1 = peak2;
			peak2 = levelsw;

			for (i = 0; i < barcount; i++) {
				level = 0;
				for (j = i*conf.data[threadnum].div; j < (i+1)*conf.data[threadnum].div; j++)
					if (level < freq_data[j])
						level = freq_data[j];
				level = level * (i*conf.data[threadnum].div + 1);
				level = floor(abs(log(level - x00)*scale + y00));
				if (level < conf.geo[threadnum].height) {
					if ((level2[i] > conf.bar[threadnum].falloff)&&(level < level2[i] - conf.bar[threadnum].falloff))
						level1[i] = level2[i] - conf.bar[threadnum].falloff;
					else	level1[i] = level;
				} else level1[i] = conf.geo[threadnum].height;
				if (conf.peak[threadnum].enabled) {
					if (level1[i] > peak2[i] - conf.peak[threadnum].falloff) {
						peak1[i] = level1[i];
						peakstep[i] = 0;
					} else if (peakstep[i] == conf.peak[threadnum].step)
						if (peak2[i] > conf.peak[threadnum].falloff)
							peak1[i] = peak2[i] - conf.peak[threadnum].falloff;
						else peak1[i] = 0;
					else {
						peak1[i] = peak2[i];
						peakstep[i]++;
					}
				}
			}

			pthread_mutex_lock(&threads.mutex1);
			draw_start(&draw, damage_coords);
			for (i = 0; i < barcount; i++)
				draw_bar(&draw, threadnum, i, level1[i], level2[i], peak1[i], peak2[i]);
			draw_end(&draw, damage_coords);
			pthread_mutex_unlock(&threads.mutex1);
		}
	}
	print_status("Worker thread: Exiting");
	if (draw.display != NULL) {
		draw_close(&draw, damage_coords);
		XCloseDisplay(draw.display);
	}
	free(level1);	free(level2);	free(peak1);	free(peak2);	free(peakstep);
	return NULL;
}


// da xmms functions

static void rootvis_init(void) {
	int rc1;
	print_status("Initializing");
	pthread_mutex_init(&threads.mutex1, NULL);
	threads.control = GO;
	clean_data();
	config_init();
	threads.dirty = 31;	// this means simply everything has changed and there was no data
	if ((rc1 = pthread_create(&threads.worker[0], NULL, worker_func, NULL))) {
		fprintf(stderr, "Thread creation failed: %d\n", rc1);
		error_exit("Thread creation failed");
	}
	if ((conf.stereo)&&(rc1 = pthread_create(&threads.worker[1], NULL, worker_func, &rc1))) {
		fprintf(stderr, "Thread creation failed: %d\n", rc1);
		error_exit("Thread creation failed");
	}
  plugin_is_initted = TRUE;
}

static void rootvis_cleanup(void) {
  if ( plugin_is_initted )
  {
	  print_status("Cleanup... ");
	  threads.control = STOP;
	  pthread_join(threads.worker[0], NULL);
	  if (conf.stereo)	pthread_join(threads.worker[1], NULL);
	  print_status("Clean Exit");
    plugin_is_initted = FALSE;
  }
}

static void rootvis_about(void)
{
	print_status("About");
}

static void rootvis_configure(void)
{
	print_status("Configuration trigger");
	config_init();
	config_show(2);
}

static void rootvis_playback_start(void)
{
	print_status("Playback starting");
}

static void rootvis_playback_stop(void)
{
	clean_data();
}

static void rootvis_render_freq(gint16 freq_data[2][256]) {
	int channel, bucket;
	pthread_mutex_lock(&threads.mutex1);
	threads.dirty = threads.dirty & (~(16)); // unset no data yet flag
	for (channel = 0; channel < 2; channel++) {
	 for (bucket = 0; bucket < 256; bucket++) {
		if (conf.stereo) threads.freq_data[channel][bucket] = freq_data[channel][bucket];
		else if (channel == 0) threads.freq_data[0][bucket] = freq_data[channel][bucket] / 2;
			else	threads.freq_data[0][bucket] += freq_data[channel][bucket] / 2;
	 }
	}
	pthread_mutex_unlock(&threads.mutex1);
}
