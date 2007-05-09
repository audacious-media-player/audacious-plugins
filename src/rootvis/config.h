#ifndef _RV_CONFIG_H
#define _RV_CONFIG_H

enum valtype {
	BOOLN,
	INT,
	FLOAT,
	TEXT,
	COLOR
};

void config_init(void);
void config_revert(int);
void config_save(int);

void config_show(int);

struct config_value_int {
	int* var;
	int def_value;
	int range[2];
};

struct config_value_float {
	float* var;
	float def_value;
	float range[2];
};

struct config_value_text {
	char** var;
	char* def_value;
	int maxlength;
};

struct config_value_color {
	unsigned char* var;
	char* def_value;
	void* frontend;
};

struct config_value {
	enum valtype type;
	char* name;
	int affects;
	union {
		struct config_value_int vali;
		struct config_value_float valf;
		struct config_value_text valt;
		struct config_value_color valc;
	};
};

struct config_def {
	int count;
	struct config_value* def;
};

struct config_def Cmain;
struct config_def Cchannel[2];

#endif
