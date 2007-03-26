#include <string.h>
#include <audacious/configdb.h>

#include <rootvis.h>
#include <config.h>

extern void config_def_init(void);
extern void config_frontend_init(void);

void color_quad2arr(unsigned char* res, char* quad) {
	if (sscanf(quad, "#%2hhx%2hhx%2hhx%2hhx", &res[0], &res[1], &res[2], &res[3]) != 4)
		fprintf(stderr, "Color value %s could not be recognized as #rrggbbaa, ranging from #00000000 to #FFFFFFFF\n", quad);
}

char* color_arr2quad(unsigned char* src, char* quad) {
	sprintf(quad, "#%2.2hhx%2.2hhx%2.2hhx%2.2hhx", src[0], src[1], src[2], src[3]);
	return quad;
}

void cval_setdefault(struct config_value val)
{
	switch (val.type)
	{
		case BOOLN:
		case INT:
			*val.vali.var = val.vali.def_value;
		break;
		case FLOAT:
			*val.valf.var = val.valf.def_value;
		break;
		case TEXT:
			strcpy(*val.valt.var, val.valt.def_value);
		break;
		case COLOR:
			color_quad2arr(val.valc.var, val.valc.def_value);
		break;
	}
}

void cval_writefile(struct config_value val, ConfigDb *fp, char* sect)
{
	switch (val.type)
	{
		case BOOLN:
		case INT:
			bmp_cfg_db_set_int(fp, sect, val.name, *val.vali.var);
		break;
		case FLOAT:
			bmp_cfg_db_set_float(fp, sect, val.name, *val.valf.var);
		break;
		case TEXT:
			bmp_cfg_db_set_string(fp, sect, val.name, *val.valt.var);
		break;
		case COLOR:
		{
			char colortmp[10];
			bmp_cfg_db_set_string(fp, sect, val.name, color_arr2quad(val.valc.var, colortmp));
		}
		break;
	}
}

void cval_readfile(struct config_value val, ConfigDb *fp, char* sect)
{
	switch (val.type)
	{
		case BOOLN:
		case INT:
			if (!(bmp_cfg_db_get_int(fp, sect, val.name, val.vali.var)))
				cval_writefile(val, fp, sect);
		break;
		case FLOAT:
			if (!(bmp_cfg_db_get_float(fp, sect, val.name, val.valf.var)))
				cval_writefile(val, fp, sect);
		break;
		case TEXT:
			if (!(bmp_cfg_db_get_string(fp, sect, val.name, val.valt.var)))
				cval_writefile(val, fp, sect);
		break;
		case COLOR:
		{
			char* colortmp = NULL;
			if (!(bmp_cfg_db_get_string(fp, sect, val.name, &colortmp)))
				cval_writefile(val, fp, sect);
			else	color_quad2arr(val.valc.var, colortmp);
		}
		break;
	}
}

// this parses ~/.xmms/config
// if a setting is not found, it is created to make it possible to edit the default settings
// after the configuration dialogue is finished, this won't be necessary any more
void config_read(int number) {
	int i, j;
	ConfigDb *fp;

	fp = bmp_cfg_db_open();

	print_status("Reading configuration");

	if (number == 2)
		for (i = 0; i < Cmain.count; ++i)
		{
			cval_setdefault(Cmain.def[i]);
			cval_readfile(Cmain.def[i], fp, "rootvis");
		}

	for (j = 0; j < 2; ++j)
		if ((number == j)||(number == 2))
			for (i = 0; i < Cchannel[j].count; ++i)
			{
				cval_setdefault(Cchannel[j].def[i]);
				cval_readfile(Cchannel[j].def[i], fp, (j == 0 ? "rootvis" : "rootvis2"));
			}

	bmp_cfg_db_close(fp);
	print_status("Configuration finished");
}

void config_write(int number) {
	int i, j;
	ConfigDb *fp;

	print_status("Writing configuration");
	fp = bmp_cfg_db_open();

	if (number == 2)
		for (i = 0; i < Cmain.count; ++i)
			cval_writefile(Cmain.def[i], fp, "rootvis");

	for (j = 0; j < 2; ++j)
		if ((number == j)||(number == 2))
			for (i = 0; i < Cchannel[j].count; ++i)
				cval_writefile(Cchannel[j].def[i], fp, (j == 0 ? "rootvis" : "rootvis2"));

	bmp_cfg_db_close(fp);
}

void config_revert(int number) {
	/* as the configs aren't saved in a thread save way, we have to lock while we read them */
	threads_lock();
	config_read(number);

	// set the right change bits, according to wether we change channel 0, 1 or both (2)
	if (number == 2) number = 15;
	 else	number = 3 + number*9;
	threads_unlock(number);
}

void config_save(int number) {
	threads_lock();
	config_write(number);
	threads_unlock(0);
}

void config_init(void) {
	static int initialized = 0;
	if (initialized == 0) {
		print_status("First initialization");

		conf.geo[0].display = malloc(256);
		conf.geo[1].display = malloc(256);

		config_def_init();
		config_frontend_init();

		config_read(2);

		initialized++;
	}
}
