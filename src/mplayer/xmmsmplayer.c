#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <glib.h>

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <audacious/plugin.h>
#include <audacious/beepctrl.h>
#include <audacious/rcfile.h>
#include <audacious/util.h>
#include <unistd.h>

#include <fcntl.h> 
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include <stdio.h>
#include <sys/wait.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <assert.h> 

#include <X11/Xmd.h>


#include "xmmsmplayer.h"
static Atom  XA_WIN_LAYER;
static Atom XA_NET_WM_STATE;
static Atom XA_NET_WM_STATE_FULLSCREEN;
static Atom XA_NET_WM_STATE_ABOVE;
static Atom XA_NET_WM_STATE_STAYS_ON_TOP;
static Atom XA_NET_WM_STATE_BELOW;

static Window mplayer_video = 0;
static gchar mplayer_wid[16];

static int mplayer_pipe[2];          /* Control mplayer thru this pipe     */
static pthread_t mplayer_tid;        /* Thread id */
static gchar *mplayer_file=NULL;           /* filename */
static struct mplayer_info *mplayer_current_info=NULL;
static gint mplayer_current_time=0;   /* to be returned to xmms */

static char *mplayer_fifoname=NULL;
static InputPlugin *mplayer_ip=NULL; /* Information to be returned to xmms */
static gint mplayer_playing=0; 
static struct mplayer_cfg *mplayer_current_cfg=NULL;
static Display *dis = NULL;

InputPlugin *get_iplugin_info(void){
	if(!mplayer_ip){
		mplayer_ip=(InputPlugin *) malloc(sizeof (InputPlugin));
		memset(mplayer_ip,0,sizeof (InputPlugin));
		mplayer_ip->description   = strdup("MPlayer Plugin for Xmms");
		mplayer_ip->init          = mplayer_init;
		mplayer_ip->is_our_file   = mplayer_is_our_file;
		mplayer_ip->play_file     = mplayer_play_file;
		mplayer_ip->stop          = mplayer_stop;
		mplayer_ip->pause         = mplayer_pause;
		mplayer_ip->seek          = mplayer_seek;
		mplayer_ip->get_time      = mplayer_get_time;
		mplayer_ip->get_song_info = mplayer_get_song_info;
		mplayer_ip->set_info      = NULL;
		mplayer_ip->cleanup       = mplayer_cleanup;
		mplayer_ip->about         = mplayer_about;
		mplayer_ip->configure     = mplayer_configure;

	}
	return mplayer_ip;
}

void mplayer_init(){
	/*Add read configure!! */

	/*mplayer_playing=0;*/
}
int mplayer_is_our_file(char *filename){
	gchar *ext;
	ext = strrchr(filename, '.');
	if (ext){
		if((!strcasecmp(ext, ".mpg")) ||
				(!strcasecmp(ext, ".mpeg")) ||
				(!strcasecmp(ext, ".divx")) ||
				(!strcasecmp(ext, ".qt")) ||
				(!strcasecmp(ext, ".mov")) ||
				(!strcasecmp(ext, ".mp2")) ||
				(!strcasecmp(ext, ".mpa")) ||
				(!strcasecmp(ext, ".dat")) ||
				(!strcasecmp(ext, ".rm")) ||
				(!strcasecmp(ext, ".swf")) ||
				(!strcasecmp(ext, ".wma")) ||
				(!strcasecmp(ext, ".wmv")) ||
				(!strcasecmp(ext, ".wmp")) ||
				(!strcasecmp(ext, ".asf")) ||
				(!strcasecmp(ext, ".avi"))
		  ) return TRUE;
	}
	return FALSE; 
}
void mplayer_play_file(char *filename){
	pthread_attr_t tattr;
	void *arg;
	int ret,pipe_ret;
	gchar temp[10];
	gchar *username;

	mplayer_debugf("debug-play-in\n");
	mplayer_file=filename;
	pipe_ret=pipe(mplayer_pipe);

	if(mplayer_current_info)g_free(mplayer_current_info);
	mplayer_current_info = mplayer_read_file_info(filename);
	mplayer_debugf("Setting info:\n%sTerm\n%i\n%i\n%i\n%i\n\n",
			mplayer_current_info->caption,
			mplayer_current_info->length * 1000,
			mplayer_current_info->abr,
			mplayer_current_info->rate,
			mplayer_current_info->nch);
	mplayer_debugf("debug-play-fileinfo-done\n");
	/*  usleep(100000);*/
	mplayer_ip->set_info(mplayer_current_info->caption,
			mplayer_current_info->length * 1000,
			mplayer_current_info->abr,
			mplayer_current_info->rate,
			mplayer_current_info->nch); 
	mplayer_debugf("debug-play-sent-info\n");
	mplayer_debugf("debug- reading configfile\n");
	if (mplayer_current_cfg){
		g_free(mplayer_current_cfg->extra);
		g_free(mplayer_current_cfg);
	}
	mplayer_current_cfg=mplayer_read_cfg();
	if(mplayer_current_cfg->onewin &&(!mplayer_video)){
/*		mplayer_video=gtk_window_new(GTK_WINDOW_DIALOG);
		gtk_widget_set_usize(GTK_WIDGET(mplayer_video), 320, 240);
		gtk_window_set_title(GTK_WINDOW(mplayer_video), "XMMS - MPlayer");
		gtk_signal_connect(GTK_OBJECT(mplayer_video),
				"destroy",
				GTK_SIGNAL_FUNC(mplayer_quitting_video),
				NULL);
		gtk_widget_show(mplayer_video);
		sprintf(mplayer_wid,"%i",GDK_WINDOW_XWINDOW(mplayer_video->window));
*/
		if(!dis) dis = XOpenDisplay(NULL);
		assert(dis);
		XEvent report;
		int blackColor = BlackPixel(dis, DefaultScreen(dis));
		int whiteColor = WhitePixel(dis, DefaultScreen(dis));
		mplayer_video = XCreateSimpleWindow(dis, DefaultRootWindow(dis), 0, 0, 
				320, 240, 0, blackColor, blackColor);
		XMapWindow(dis,mplayer_video);
		XFlush(dis);
		XSelectInput (dis, mplayer_video,  KeyPressMask);
		sprintf(mplayer_wid,"%i",mplayer_video);
	}
	if((!mplayer_current_cfg->onewin) &&(mplayer_video)){
		XDestroyWindow(dis,mplayer_video);
		mplayer_video=0;
		mplayer_wid[0]='\0';
	}
	if(mplayer_current_cfg->xmmsaudio){
		if(!mplayer_fifoname){
			username = getenv("LOGNAME");
			if(!username)username = getenv("USERNAME");
			if(!username)username = getenv("USER");
			sprintf(temp,"%i",getpid());
			if (!username)mplayer_fifoname=g_strconcat("/tmp/xmmsmplayer-",temp,(char*)0);
			else mplayer_fifoname=g_strconcat("/tmp/xmmsmplayer-",username,"-",temp,(char*)0);
			mkfifo(mplayer_fifoname,0600);
		}
		mplayer_ip->output->open_audio(FMT_S16_LE,
				mplayer_current_info->rate,
				mplayer_current_info->nch);
	}
	mplayer_debugf("debug- read configfile\n");  
	if (mplayer_playing==1) mplayer_debugf("Duplicate call!! -- Panic \n");
	else mplayer_playing=1;
	mplayer_debugf("debug-play-creating-thread\n");
	ret = pthread_create(&mplayer_tid, NULL, mplayer_play_loop, arg);
	mplayer_debugf("debug-play-out\n\n");
}

void mplayer_stop(){
	int ret;
	if (mplayer_playing==0) mplayer_debugf("Confusion: Restopped!\n");
	else {
		mplayer_playing=0;
		if(mplayer_current_cfg->xmmsaudio)mplayer_ip->output->close_audio();
		if (write(mplayer_pipe[1],"quit\n",5)==5){
			mplayer_debugf("debug-stop-if\n");
		};
		mplayer_debugf("debug-stop\n");
		ret = pthread_join(mplayer_tid, NULL);
		mplayer_debugf("debug-stop\n\n");
	}
}

void mplayer_cleanup(){
	if (mplayer_playing) mplayer_stop();
	if (mplayer_fifoname){
		remove(mplayer_fifoname);
		g_free(mplayer_fifoname);
		mplayer_fifoname=NULL;
	}

}

void mplayer_pause(short p){
	write(mplayer_pipe[1],"pause\n",6);
}

void mplayer_seek(int t){
	char buff[16];
	if(mplayer_playing){
		sprintf(buff,"seek %i\n",(t - mplayer_current_time));
		write(mplayer_pipe[1],buff,strlen(buff));
	}
}

gint mplayer_get_time(){
	return (mplayer_current_time*1000) ;
}
void mplayer_get_song_info(char * filename, char ** title, int * length){
	struct mplayer_info *info;
	char *temp,*name;
	info = mplayer_read_file_info(filename);
	*title  = g_strdup(info->caption);
	*length = (info->length * 1000);
	g_free(info);
}

void *mplayer_play_loop(void *arg){ 
	char buff[35];
	int mplayer_slave_pid;
	int i,read_length,playtime,one_read;
	int mplayer_status_pipe[2];
	int mplayer_error_stream;
	int mplayer_fifo_fd;
	int pipe_ret;
	char **params;
	char audio_buff[MPLAYER_AUDIO_SIZE];
	int audio_buff_read;
	gboolean audio_flag, status_flag;
	struct timespec nanotime;
	nanotime.tv_sec=0;
	nanotime.tv_nsec=1;


	pipe_ret=pipe(mplayer_status_pipe);

	if ((mplayer_slave_pid=vfork())==0){
		//close(mplayer_pipe[1]);
		mplayer_debugf("debug Pre-exec!\n");
		mplayer_error_stream=open("/dev/null",0);
		mplayer_debugf("debug Making vector\n");    
		params=mplayer_make_vector();
		mplayer_debugf("debug Made vector\n");    
		close(0);
		close(1);
		close(2);
		if(dup2(mplayer_pipe[0],0)!=0) mplayer_debugf("duplication error\n");
		if(dup2(mplayer_status_pipe[1],1)!=0) mplayer_debugf("duplication error\n");
		if(dup2(mplayer_error_stream,2)!=0) mplayer_debugf("duplication error\n");
		/*i=execlp("mplayer","mplayer","-vo","x11","-zoom","-sws","2","-framedrop","-slave","-wid",mplayer_wid,mplayer_file,(char *)0);*/
		i=execvp("mplayer",params);
		mplayer_debugf("Fatal Error: Couldnt start MPlayer! exec returned %i\n",i);
		_exit(-1);
	}
	else{
		close(mplayer_status_pipe[1]);    /* EXTREMELY IMPORTANT, a little tricky! */
		close(mplayer_pipe[0]);
		if(mplayer_current_cfg->xmmsaudio){
			mplayer_debugf("debug - Opening fifo...\n");
			mplayer_fifo_fd=open(mplayer_fifoname,O_RDONLY);
			mplayer_debugf("debug - Opened fifo\n");
			fcntl(mplayer_status_pipe[0],F_SETFL,O_NDELAY);
		}
		audio_buff_read=-1;
		read_length=-1;
		one_read=-1;
		while((wait3((union wait *)0,WNOHANG,(struct rusage *)0)!=mplayer_slave_pid)&&(mplayer_playing==1)){
			audio_flag=FALSE;
			status_flag=FALSE;
			/* Window event code here */
			if(mplayer_current_cfg->xmmsaudio){
				//	  do{

				/* mplayer_debugf("debug - reading fifo...\n");*/
				audio_buff_read=read(mplayer_fifo_fd,audio_buff,MPLAYER_AUDIO_SIZE);
				if (audio_buff_read==0) break;
				if (audio_buff_read>0){
					mplayer_ip->output->write_audio(audio_buff,audio_buff_read);
					/*mplayer_ip->add_vis_pcm();*/
					audio_flag=TRUE;
					mplayer_debugf(" %i ",audio_buff_read);
				}
				//}
				//while(audio_buff_read>0);
			}
			do{
				if(mplayer_current_cfg->onewin ){
					XEvent report;
					if(XCheckWindowEvent(dis,mplayer_video,KeyPressMask, &report)){
						if (XLookupKeysym(&report.xkey, 0) == XK_f){ 
							fprintf (stdout, "The f was pressed.\n");
							XMoveResizeWindow(dis,mplayer_video,0,0,1280,1024);
							XMapRaised(dis,mplayer_video);
							XRaiseWindow(dis,mplayer_video);
							XSetTransientForHint(dis,mplayer_video, RootWindow(dis,0));
							
   XClientMessageEvent  xev;
   char *state;

   memset( &xev,0,sizeof( xev ) );
   xev.type=ClientMessage;
   xev.message_type=XA_WIN_LAYER;
   xev.display=dis;
   xev.window=mplayer_video;
   xev.format=32;
   xev.data.l[0]=10;
     xev.data.l[1]=CurrentTime;
   XSendEvent( dis,RootWindow( dis,0),False,SubstructureRedirectMask,(XEvent*)&xev );

 
fprintf(stdout,"Cleared 1\n");
XSetWindowAttributes attr;
							attr.override_redirect = True;
							XChangeWindowAttributes(dis,mplayer_video, CWOverrideRedirect, &attr);

							XFlush(dis);

						}
					}
				}
				one_read= read(mplayer_status_pipe[0],buff+read_length,1);
				if (one_read==0) break;
				if (one_read > 0) {
					status_flag=TRUE;
					if ((buff[read_length]==(char)13)||(buff[read_length]==(char)10)) read_length=0;
					if (read_length < 32 ) read_length++;
					if (read_length >= 16){
						sscanf(buff+3,"%i",&playtime);
						mplayer_current_time=playtime;
					}
				}
			}
			while (one_read>0);

			/*
			   audio_buff_read=0;
			   if(mplayer_current_cfg->xmmsaudio){
			   mplayer_debugf("debug - reading fifo...\n");

			   audio_buff_read=read(mplayer_fifo_fd,audio_buff,MPLAYER_AUDIO_SIZE);
			   if (audio_buff_read>0)mplayer_ip->output->write_audio(audio_buff,audio_buff_read);
			   }
			   read_length=read(mplayer_status_pipe[0],buff,1);
			   if(read_length>0){ 
			   if ((buff[0]==(char)13)||(buff[0]==(char)10) ) {
			   one_read=1;
			   read_length=0;
			   while((read_length<16)&&(one_read>0)) {
			   audio_buff_read=0;
			   if(mplayer_current_cfg->xmmsaudio){
			   mplayer_debugf("debug -  readin fifo...\n");
			   audio_buff_read=read(mplayer_fifo_fd,audio_buff,MPLAYER_AUDIO_SIZE);
			   mplayer_debugf("debug -  read fifo...%i\n", audio_buff_read);
			   if (audio_buff_read>0)mplayer_ip->output->write_audio(audio_buff,audio_buff_read);
			   }
			   mplayer_debugf("debug -  readin status...\n");
			   one_read= read(mplayer_status_pipe[0],buff+read_length,16-read_length);
			   read_length += one_read;
			   if ((audio_buff_read <= 0)&&(one_read <= 0)) usleep(5);
			   }
			   sscanf(buff+3,"%i",&playtime);
			   mplayer_current_time=playtime;}

			   }
			   else if ((audio_buff_read <= 0)&&(read_length <= 0)) usleep(5);
			 */

			//else if (read_length == -1) break;
		}
		if (mplayer_playing==1){
			if(mplayer_current_cfg->xmmsaudio)mplayer_ip->output->close_audio();
			mplayer_playing=0;
			xmms_remote_playlist_next(ctrlsocket_get_session_id());
		}
		//  if (mplayer_dont_spawn) mplayer_dont_spawn=0;
		//  else xmms_remote_playlist_next(ctrlsocket_get_session_id());
		pthread_exit(NULL);
	}
}


struct mplayer_info  *mplayer_read_file_info(char *filename){
	struct mplayer_info *info;
	int datalength,done=0;
	FILE *inquiry, *size_query;
	char mplayer_command[256];
	char buff[4096];
	char *temp;
	info=(struct mplayer_info *)malloc(sizeof(struct mplayer_info)); 
	memset((char *)info,0,sizeof(struct mplayer_info));
	sprintf(mplayer_command,"mplayer -slave -identify -vo null -ao null -frames 0 \"%s\" 2> /dev/null",filename);
	inquiry=popen(mplayer_command,"r");
	done=0;
	while((!feof(inquiry)) && (done <4000) ){
		fscanf(inquiry,"%c",(buff+done));
		done++;
	}
	buff[done]=(char)0;
	pclose(inquiry);
	mplayer_debugf("debug-id\n");
	temp=strstr(buff,"Name:");
	if (temp) mplayer_read_to_eol(info->title,temp+5);
	mplayer_debugf("debug-id\n");
	temp=strstr(buff,"Artist:");
	if (temp) mplayer_read_to_eol(info->artist,temp+7);
	mplayer_debugf("debug-id\n");
	temp=strstr(buff,"ID_VIDEO_BITRATE=");
	if (temp) sscanf(temp+strlen("ID_VIDEO_BITRATE=") ,"%i",&(info->vbr));
	temp=strstr(buff,"ID_VIDEO_WIDTH=");
	if (temp) sscanf(temp+strlen("ID_VIDEO_WIDTH=") ,"%i",&(info->x));
	temp=strstr(buff,"ID_VIDEO_HEIGHT=");
	if (temp) sscanf(temp+strlen("ID_VIDEO_HEIGHT=") ,"%i",&(info->y));
	temp=strstr(buff,"ID_AUDIO_BITRATE=");
	if (temp) sscanf(temp+strlen("ID_AUDIO_BITRATE="),"%i",&(info->abr));
	info->br = info->abr + info->vbr;
	temp=strstr(buff,"ID_AUDIO_RATE=");
	if (temp) sscanf(temp+strlen("ID_AUDIO_RATE="),"%i",&(info->rate));
	temp=strstr(buff,"ID_AUDIO_NCH=");
	if (temp) sscanf(temp+strlen("ID_AUDIO_NCH="),"%i",&(info->nch));
	temp=strstr(buff,"ID_LENGTH=");
	if (temp) sscanf(temp+strlen("ID_LENGTH="),"%i",&(info->length));
	else{
		sprintf(mplayer_command,"du -b \"%s\" ",filename);
		size_query=popen(mplayer_command,"r");
		fscanf(size_query,"%i",&(info->filesize));
		pclose(size_query);
		if (info->br > 0) info->length=((info->filesize * 8)/(info->br));}
		mplayer_debugf("debug-id\n");
		info->filename=filename;
		if((strlen(info->artist)+strlen(info->title)>0)){
			sprintf(info->caption,"%s - %s",info->artist,info->title);}
		else{
			temp = g_strdup(g_basename(filename));
			strcpy(info->caption,temp);
			free(temp);
			if ((temp = strrchr(info->caption, '.')) != NULL)
				*temp = '\0';
		}
		mplayer_debugf("debug-id\n\n");
		return info;
}

char **mplayer_make_vector(){ /*To be passed to exec*/
	char** vector;
	char** temp;
	int i=0;
	vector =(char **)malloc(sizeof(char*)*MPLAYER_MAX_VECTOR);
	memset(vector,0,sizeof(char*)*MPLAYER_MAX_VECTOR);
	mplayer_vector_append(vector,"mplayer");
	mplayer_vector_append(vector,"-slave");
	if(mplayer_current_cfg->vo){
		mplayer_vector_append(vector,"-vo");
		switch(mplayer_current_cfg->vo){
			case MPLAYER_VO_XV:
				mplayer_vector_append(vector,"xv");
				break;
			case MPLAYER_VO_X11:
				mplayer_vector_append(vector,"x11");
				break;
			case MPLAYER_VO_GL:
				mplayer_vector_append(vector,"gl");
				break;
			case MPLAYER_VO_SDL:
				mplayer_vector_append(vector,"sdl");
				break;      
		}
	}
	if(mplayer_current_cfg->ao){
		mplayer_vector_append(vector,"-ao");
		switch(  mplayer_current_cfg->ao){
			case MPLAYER_AO_OSS:
				mplayer_vector_append(vector,"oss");
				break;
			case MPLAYER_AO_ARTS:
				mplayer_vector_append(vector,"arts");
				break;
			case MPLAYER_AO_ESD:
				mplayer_vector_append(vector,"esd");
				break;
			case MPLAYER_AO_ALSA:
				mplayer_vector_append(vector,"alsa");
				break;
			case MPLAYER_AO_SDL:
				mplayer_vector_append(vector,"sdl");
				break;
		}
	}
	if(mplayer_current_cfg->zoom) mplayer_vector_append(vector,"-zoom");
	if(mplayer_current_cfg->framedrop)mplayer_vector_append(vector,"-framedrop");
	if(mplayer_current_cfg->idx) mplayer_vector_append(vector,"-idx");
	if(mplayer_current_cfg->onewin){ 
		mplayer_vector_append(vector,"-wid");
		mplayer_vector_append(vector,mplayer_wid);}
		if(mplayer_current_cfg->xmmsaudio){
			mplayer_vector_append(vector,"-ao");
			mplayer_vector_append(vector,"pcm");
			mplayer_vector_append(vector,"-aofile");
			mplayer_vector_append(vector,mplayer_fifoname);
			mplayer_vector_append(vector,"-autosync");
			mplayer_vector_append(vector,"10000");
			mplayer_vector_append(vector,"-nowaveheader");
			mplayer_vector_append(vector,"-format");
			mplayer_vector_append(vector,"128");
		}
		if(mplayer_current_cfg->extra){
			mplayer_debugf("debug - adding extra options\n");
			temp= g_strsplit(mplayer_current_cfg->extra," ",0);
			while(temp[i]){
				mplayer_vector_append(vector,temp[i]);
				i++;
			}
			g_strfreev(temp);
		}

		mplayer_vector_append(vector,mplayer_file);
		return vector;
}
void mplayer_vector_append(char**vector,char*param){
	int i=0;
	while(vector[i])i++;
	if(i >= (MPLAYER_MAX_VECTOR -1)) {
		mplayer_debugf("Too many arguments to mplayer!!\n");
		mplayer_debugf("Ignoring parameter: %s\n",param);
		return;
	}
	mplayer_debugf("Adding parameter: %s\n",param);
	vector[i]=strdup(param);
}

void mplayer_read_to_eol(char *str1,char *str2){  
	/* copy str2 to str1 till \n is reached in str2 */
	int i=0,j=0;
	while(!((str2[j]=='\n')||(str2[j]==(char)0)||(str2[j]==(char)10)||(j>32) )) 
		str1[i++]=str2[j++];
	str1[i]=(char)0;
}


static void mplayer_about(void)
{
	static GtkWidget *window = NULL;
	if (window)
		return;

	window = xmms_show_message(
			"About " PACKAGE,
			PACKAGE " " VERSION "\n"
			"Author: " " Nandan Dixit " " <nandan@cse.iitb.ac.in>" "\n"
			"http://xmmsmplayer.sourceforge.net/" "\n"
			"Ported to Audacious: " " Aaron Sheldon " "\n"
			"http://audacious-media-player.org/" "\n"
			"This library is free software; you can redistribute it and/or\n"
			"modify it under the terms of the GNU Library General Public\n",
			"Ok", FALSE, NULL, NULL);

	gtk_signal_connect(GTK_OBJECT(window), "destroy",
			GTK_SIGNAL_FUNC(gtk_widget_destroyed), &window);

	gtk_widget_show(window);

}
/*
void mplayer_quitting_video(GtkWidget *widget, gpointer data) {
	mplayer_debugf("debug - entered quitting_video\n");
	if(widget==mplayer_video){
		mplayer_debugf("debug - correct widget\n");
		mplayer_video=NULL;
		mplayer_wid[0]='\0';
	}
}
*/
