#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	int		numOfSongs;
	int		defSong; 
	char	*commentBuffer;
	int		currentTimeIn50Sec;
} sapMUSICstrc;

sapMUSICstrc *sapLoadMusicFile( char *fname );
void sapPlaySong( int numOfSong );
void sapRenderBuffer( short int *buffer, int number_of_samples );

#ifdef __cplusplus
}
#endif

