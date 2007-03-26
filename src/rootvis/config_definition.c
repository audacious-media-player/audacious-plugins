#include <string.h>
#include <rootvis.h>
#include <config.h>

inline void add_valb(struct config_def* conf, char* name, int affects, int* var, int def);
inline void add_vali(struct config_def* conf, char* name, int affects, int* var, int def, int from, int to);
inline void add_valf(struct config_def* conf, char* name, int affects, float* var, float def, float from, float to);
inline void add_valt(struct config_def* conf, char* name, int affects, char** var, char* def, int maxlen);
inline void add_valc(struct config_def* conf, char* name, int affects, unsigned char* var, char* def);

void config_def_init(void)
{
	Cmain.count = 0;
	add_valb(&Cmain, "debug", 0, &conf.debug, 0);
	add_valb(&Cmain, "stereo", 32, &conf.stereo, 1);

	Cchannel[0].count = 0;
	add_valt(&Cchannel[0], "geometry_display", 32, &conf.geo[0].display, "", 255);
	add_vali(&Cchannel[0], "geometry_posx", 1, &conf.geo[0].posx, 520, 0, 0);
	add_vali(&Cchannel[0], "geometry_posy", 1, &conf.geo[0].posy, 1, 0, 0);
	add_vali(&Cchannel[0], "geometry_orientation", 1, &conf.geo[0].orientation, 0, 0, 3);
	add_vali(&Cchannel[0], "geometry_height", 1, &conf.geo[0].height, 50, 0, 0);
	add_vali(&Cchannel[0], "geometry_space", 1, &conf.geo[0].space, 1, 0, 0);
	add_vali(&Cchannel[0], "bar_width", 1, &conf.bar[0].width, 8, 0, 0);
	add_vali(&Cchannel[0], "bar_falloff", 0, &conf.bar[0].falloff, 5, 1, 0);
	add_vali(&Cchannel[0], "bar_shadow", 1, &conf.bar[0].shadow, 1, 0, 0);
	add_valb(&Cchannel[0], "bar_bevel", 0, &conf.bar[0].bevel, 0);
	add_valb(&Cchannel[0], "bar_gradient", 0, &conf.bar[0].gradient, 1);
	add_valc(&Cchannel[0], "bar_color_1", 2, conf.bar[0].color[0], "#e6ff64FF");
	add_valc(&Cchannel[0], "bar_color_2", 2, conf.bar[0].color[1], "#cdf62bFF");
	add_valc(&Cchannel[0], "bar_color_3", 2, conf.bar[0].color[2], "#b8dd27FF");
	add_valc(&Cchannel[0], "bar_color_4", 2, conf.bar[0].color[3], "#a3c422FF");
	add_valc(&Cchannel[0], "bar_bevel_color", 2, conf.bar[0].bevel_color, "#00FF00FF");
	add_valc(&Cchannel[0], "bar_shadow_color", 2, conf.bar[0].shadow_color, "#00000066");
	add_valb(&Cchannel[0], "peak_enabled", 1, &conf.peak[0].enabled, 1);
	add_vali(&Cchannel[0], "peak_falloff", 0, &conf.peak[0].falloff, 4, 1, 0);
	add_vali(&Cchannel[0], "peak_step", 0, &conf.peak[0].step, 5, 0, 0);
	add_valc(&Cchannel[0], "peak_color", 2, conf.peak[0].color, "#ffffffdd");
	add_valb(&Cchannel[0], "peak_shadow", 0, &conf.peak[0].shadow, 0);
	add_vali(&Cchannel[0], "data_cutoff", 1, &conf.data[0].cutoff, 180, 1, 255);
	add_vali(&Cchannel[0], "data_div", 1, &conf.data[0].div, 4, 1, 255);
	add_valf(&Cchannel[0], "data_linearity", 0, &conf.data[0].linearity, 0.33, 0.1, 0.9);
	add_vali(&Cchannel[0], "data_fps", 0, &conf.data[0].fps, 30, 1, 100);

	Cchannel[1].count = 0;
	add_valt(&Cchannel[1], "geometry_display", 32, &conf.geo[1].display, "", 255);
	add_vali(&Cchannel[1], "geometry_posx", 1, &conf.geo[1].posx, 520, 0, 0);
	add_vali(&Cchannel[1], "geometry_posy", 1, &conf.geo[1].posy, 52, 0, 0);
	add_vali(&Cchannel[1], "geometry_orientation", 1, &conf.geo[1].orientation, 1, 0, 3);
	add_vali(&Cchannel[1], "geometry_height", 1, &conf.geo[1].height, 40, 0, 0);
	add_vali(&Cchannel[1], "geometry_space", 1, &conf.geo[1].space, 2, 0, 0);
	add_vali(&Cchannel[1], "bar_width", 1, &conf.bar[1].width, 8, 0, 0);
	add_vali(&Cchannel[1], "bar_falloff", 0, &conf.bar[1].falloff, 5, 1, 0);
	add_vali(&Cchannel[1], "bar_shadow", 1, &conf.bar[1].shadow, 0, 0, 0);
	add_valb(&Cchannel[1], "bar_bevel", 0, &conf.bar[1].bevel, 0);
	add_valb(&Cchannel[1], "bar_gradient", 0, &conf.bar[1].gradient, 1);
	add_valc(&Cchannel[1], "bar_color_1", 2, conf.bar[1].color[0], "#e6ff6466");
	add_valc(&Cchannel[1], "bar_color_2", 2, conf.bar[1].color[1], "#e6ff6455");
	add_valc(&Cchannel[1], "bar_color_3", 2, conf.bar[1].color[2], "#e6ff6433");
	add_valc(&Cchannel[1], "bar_color_4", 2, conf.bar[1].color[3], "#e6ff6422");
	add_valc(&Cchannel[1], "bar_bevel_color", 2, conf.bar[1].bevel_color, "#00FF00FF");
	add_valc(&Cchannel[1], "bar_shadow_color", 2, conf.bar[1].shadow_color, "#00000066");
	add_valb(&Cchannel[1], "peak_enabled", 1, &conf.peak[1].enabled, 1);
	add_vali(&Cchannel[1], "peak_falloff", 0, &conf.peak[1].falloff, 4, 1, 0);
	add_vali(&Cchannel[1], "peak_step", 0, &conf.peak[1].step, 5, 0, 0);
	add_valc(&Cchannel[1], "peak_color", 2, conf.peak[1].color, "#ffffff88");
	add_valb(&Cchannel[1], "peak_shadow", 0, &conf.peak[1].shadow, 0);
	add_vali(&Cchannel[1], "data_cutoff", 1, &conf.data[1].cutoff, 180, 1, 255);
	add_vali(&Cchannel[1], "data_div", 1, &conf.data[1].div, 4, 1, 255);
	add_valf(&Cchannel[1], "data_linearity", 0, &conf.data[1].linearity, 0.33, 0.1, 0.9);
	add_vali(&Cchannel[1], "data_fps", 0, &conf.data[1].fps, 30, 1, 100);
}

inline void add_begin(struct config_def* conf, char* name, int affects)
{
	conf->def = realloc(conf->def, (conf->count+1)*sizeof(struct config_value));
	conf->def[conf->count].name = (char*)malloc(strlen(name) + 1);
	strcpy(conf->def[conf->count].name, name);
	conf->def[conf->count].affects = affects;
}

inline void add_end(struct config_def* conf)
{
	conf->count++;
}

inline void add_valb(struct config_def* conf, char* name, int affects, int* var, int def)
{
	add_begin(conf, name, affects);

	conf->def[conf->count].type = BOOLN;

	conf->def[conf->count].vali.var = var;
	conf->def[conf->count].vali.def_value = def;

	add_end(conf);
}

inline void add_vali(struct config_def* conf, char* name, int affects, int* var, int def, int from, int to)
{
	add_begin(conf, name, affects);

	conf->def[conf->count].type = INT;

	conf->def[conf->count].vali.var = var;
	conf->def[conf->count].vali.def_value = def;
	conf->def[conf->count].vali.range[0] = from;
	conf->def[conf->count].vali.range[1] = to;

	add_end(conf);
}

inline void add_valf(struct config_def* conf, char* name, int affects, float* var, float def, float from, float to)
{
	add_begin(conf, name, affects);

	conf->def[conf->count].type = FLOAT;

	conf->def[conf->count].valf.var = var;
	conf->def[conf->count].valf.def_value = def;
	conf->def[conf->count].valf.range[0] = from;
	conf->def[conf->count].valf.range[1] = to;

	add_end(conf);
}

inline void add_valt(struct config_def* conf, char* name, int affects, char** var, char* def, int maxlen)
{
	add_begin(conf, name, affects);

	conf->def[conf->count].type = TEXT;

	conf->def[conf->count].valt.var = var;
	conf->def[conf->count].valt.def_value = (char*)malloc(strlen(def) + 1);
	strcpy(conf->def[conf->count].valt.def_value, def);
	conf->def[conf->count].valt.maxlength = maxlen;

	add_end(conf);
}

inline void add_valc(struct config_def* conf, char* name, int affects, unsigned char* var, char* def)
{
	add_begin(conf, name, affects);

	conf->def[conf->count].type = COLOR;

	conf->def[conf->count].valc.var = var;
	conf->def[conf->count].valc.def_value = (char*)malloc(strlen(def) + 1);
	strcpy(conf->def[conf->count].valc.def_value, def);

	add_end(conf);
}
