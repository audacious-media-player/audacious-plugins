#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xproto.h>
#include <audacious/plugin.h>


/* following values are used if there is no user configuration */
#define DEFAULT_stereo 0 // therefore we don't initialize the second channel with different settings

#define DEFAULT_geometry_posx 520
#define DEFAULT_geometry_posy 1
#define DEFAULT_geometry_orientation 0 // 0 = bottom->up, 1 = top->down, 2 = left->right, 3 = right->left
#define DEFAULT_geometry_height 50 // maximum height/amplitude of a bar
#define DEFAULT_geometry_space 1 // space between bars
#define DEFAULT_bar_width 8 // width of a bar
// set the following to 0 to disable shadows
#define DEFAULT_bar_shadow 1 // offset of shadow in pixels
// set the following to HEIGHT to disable falloff
#define DEFAULT_bar_falloff 5 // how many pixels the bars should falloff every frame
#define DEFAULT_bar_color_1 "#a3c422FF"
#define DEFAULT_bar_color_2 "#b8dd27FF"
#define DEFAULT_bar_color_3 "#cdf62bFF"
#define DEFAULT_bar_color_4 "#e6ff64FF"
#define DEFAULT_bar_shadow_color "#00285088"

// set the following to 0 to disable peaks
#define DEFAULT_peak_enabled	1
#define DEFAULT_peak_falloff	4 // how many pixels the peaks should falloff every frame
#define DEFAULT_peak_step	5 // how many frames should the peak resist the falloff
#define DEFAULT_peak_color "#ffffffdd"

// we're cutting off high frequencies by only showing 0 to CUTOFF
#define DEFAULT_cutoff 180 // frequencies are represented by 256 values
#define DEFAULT_div 4 // we have CUTOFF sources, every bar represents DIV sources

/* Linearity of the amplitude scale (0.5 for linear, keep in [0.1, 0.9]) */
#define DEFAULT_linearity 0.33
#define DEFAULT_fps	30 // how many frames per second should be drawn

// print out debug messages
#define DEFAULT_debug 0

// this is for color[] indexing

#define RED 0
#define GREEN 1
#define BLUE 2
#define ALPHA 3
#define COLORSIZE 4


void print_status(char msg[]);
void error_exit(char msg[]);

void threads_lock(void);
void threads_unlock(char);

struct rootvis_geometry {
	char* display;
	int posx;
	int posy;
	int orientation;
	int height;
	int space;
};

struct rootvis_bar {
	int width;
	int shadow;
	int falloff;
	int bevel;
	int gradient;
	unsigned char color[4][4];
	unsigned char bevel_color[4];
	unsigned char shadow_color[4];
};

struct rootvis_peak {
	int enabled;
	int falloff;
	int step;
	int shadow;
	unsigned char color[4];
};

struct rootvis_data {
	int cutoff;
	int div;
	int fps;
	float linearity;
};

struct rootvis_config {
	int stereo;
	struct rootvis_geometry geo[2];
	struct rootvis_bar bar[2];
	struct rootvis_peak peak[2];
	struct rootvis_data data[2];
	int debug;
} conf;
