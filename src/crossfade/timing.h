/* quantaudio parameters */
typedef struct
{
	int RMS;
	float mix_in;
	float mix_out;
	float length;
}
quantaudio_t;

/* id3 stuff is taken from the id3 GPL program */
typedef struct
{
	char comment[1024];
	int track;
}
id3_t;

int get_timing_comment(char *filename, quantaudio_t *qa);
int get_id3(char *filename, id3_t *id3);
