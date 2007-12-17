/*
 * main.cxx: plugin glue to libprojectm
 * Copyright (c) 2008 William Pitcock <nenolod@sacredspiral.co.uk>
 * Portions copyright (c) 2004-2006 Peter Sperl
 *
 * This program is free software; you may distribute it under the terms
 * of the GNU General Public License; version 2.
 */

#include <stdio.h>
#include <string.h>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>

extern "C"
{
#include <audacious/util.h>
#include <audacious/plugin.h>
#include <audacious/playlist.h>
#include <audacious/auddrct.h>
}

#include <math.h>
#include "ConfigFile.h"

#include <libprojectM/projectM.hpp>

#include "sdltoprojectM.h"

#include <GL/gl.h>
#define CONFIG_FILE "/share/projectM/config.inp"

// Forward declarations 
extern "C" void projectM_xmms_init(void);
extern "C" void projectM_cleanup(void);
extern "C" void projectM_render_pcm(gint16 pcm_data[2][512]);
extern "C" int worker_func(void *);
std::string read_config();

extern "C" VisPlugin projectM_vtable;

int SDLThreadWrapper(void *);
void handle_playback_trigger(void *, void *);

class projectMPlugin
{
  private:
    projectM *pm;

    gint wvw, wvh, fvw, fvh;
    gboolean fullscreen;
    gboolean error;

    SDL_Event event;
    SDL_Surface *screen;
    SDL_Thread *worker_thread;
    SDL_sem *sem;

  public:
    projectMPlugin()
    {
        std::string configFile = read_config();
        ConfigFile config(configFile);

        this->wvw = config.read<int>("Window Width", 512);
        this->wvh = config.read<int>("Window Height", 512);

        this->fullscreen = FALSE;
        if (config.read("Fullscreen", true))
              this->fullscreen = TRUE;

        /* initialise SDL */
        if (SDL_Init(SDL_INIT_VIDEO) < 0)
        {
            this->error++;
            return;
        }

        SDL_WM_SetCaption("projectM", "audacious");
        SDL_EnableUNICODE(1);

        this->sem = SDL_CreateSemaphore(0);

        /* XXX */
        aud_hook_associate("playback begin", handle_playback_trigger, NULL);
    }

    ~projectMPlugin()
    {
        if (!this->sem)
            return;

        SDL_SemWait(this->sem);

        if (this->worker_thread)
            SDL_WaitThread(this->worker_thread, NULL);

        SDL_DestroySemaphore(this->sem);
        SDL_Quit();

        this->sem = 0;
        this->worker_thread = 0;

        delete this->pm;
        this->pm = 0;

        aud_hook_dissociate("playback begin", handle_playback_trigger);
    }

    int run(void *unused)
    {
        if (error)
            return -1;

        std::string configFile = read_config();
        this->initDisplay(this->wvw, this->wvh, &this->fvw, &this->fvh, this->fullscreen);
        this->pm = new projectM(configFile);
        this->pm->projectM_resetGL(this->wvw, this->wvh);

        SDL_SemPost(this->sem);

        while (SDL_SemValue(this->sem) == 1)
        {
            projectMEvent evt;
            projectMKeycode key;
            projectMModifier mod;

            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                evt = sdl2pmEvent(event);

                key = sdl2pmKeycode(event.key.keysym.sym);
                mod = sdl2pmModifier(event.key.keysym.mod);

                switch (evt)
                {
                  case PROJECTM_KEYDOWN:
                      switch (key)
                      {
                        case PROJECTM_K_c:
                            this->takeScreenshot();
                            break;

                        case PROJECTM_K_f:
                            int w, h;
                            if (fullscreen == 0)
                            {
                                w = fvw;
                                h = fvh;
                                fullscreen = 1;
                            }
                            else
                            {
                                w = wvw;
                                h = wvh;
                                fullscreen = 0;
                            }

                            this->resizeDisplay(w, h, fullscreen);
                            this->pm->projectM_resetGL(w, h);

                            break;

                        default:
                            this->pm->key_handler(evt, key, mod);
                            break;
                      }

                      break;

                  case PROJECTM_VIDEORESIZE:
                      wvw = event.resize.w;
                      wvh = event.resize.h;
                      this->resizeDisplay(wvw, wvh, fullscreen);
                      this->pm->projectM_resetGL(wvw, wvh);

                      break;

                  case PROJECTM_VIDEOQUIT:
                      std::cerr << "XXX: PROJECTM_VIDEOQUIT is not implemented yet!" << std::endl;
                      break;

                  default:
                      break;
                }
            }

            this->pm->renderFrame();

            SDL_GL_SwapBuffers();
        }

        return 0;
    }

    void launchThread()
    {
        /* SDL sucks and won't let you use C++ functors... */
        this->worker_thread = SDL_CreateThread(SDLThreadWrapper, NULL);
    }

    void addPCMData(gint16 pcm_data[2][512])
    {
        if (SDL_SemValue(this->sem) == 1)
            this->pm->pcm->addPCM16(pcm_data);
    }

    void initDisplay(gint width, gint height, gint *rwidth, gint *rheight, gboolean fullscreen)
    {
        this->wvw = width;
        this->wvh = height;
        this->fvw = *rwidth;
        this->fvw = *rheight;
        this->fullscreen = fullscreen;

        const SDL_VideoInfo *info = NULL;
        int bpp;
        int flags;

        info = SDL_GetVideoInfo();
        if (!info)
        {
            this->error++;
            return;
        }

        /* initialize fullscreen resolution to something "sane" */
        *rwidth = width;
        *rheight = height;

        bpp = info->vfmt->BitsPerPixel;
        SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

        if (this->fullscreen)
            flags = SDL_OPENGL | SDL_HWSURFACE | SDL_FULLSCREEN;
        else
            flags = SDL_OPENGL | SDL_HWSURFACE | SDL_RESIZABLE;

        this->screen = SDL_SetVideoMode(width, height, bpp, flags);
        if (!this->screen)
            this->error++;
    }

    void resizeDisplay(gint width, gint height, gboolean fullscreen)
    {
        this->fullscreen = fullscreen;

        int flags;

        if (this->fullscreen)
            flags = SDL_OPENGL | SDL_HWSURFACE | SDL_FULLSCREEN;
        else
            flags = SDL_OPENGL | SDL_HWSURFACE | SDL_RESIZABLE;

        this->screen = SDL_SetVideoMode(width, height, 0, flags);
        if (this->screen == 0)
            return;

        SDL_ShowCursor(this->fullscreen ? SDL_DISABLE : SDL_ENABLE);
    }

    void triggerPlaybackBegin(PlaylistEntry *entry)
    {
        std::string title(entry->title);
        this->pm->projectM_setTitle(title);
    }

    void takeScreenshot(void)
    {
        static int frame = 1;

        std::string dumpPath(g_get_home_dir());
        dumpPath.append("/.projectM/");

        gchar *frame_ = g_strdup_printf("%.8d.bmp", frame);
        dumpPath.append(frame_);
        g_free(frame_);

        SDL_Surface *bitmap;

        GLint viewport[4];
        long bytewidth;
        GLint width, height;
        long bytes;

        glReadBuffer(GL_FRONT);
        glGetIntegerv(GL_VIEWPORT, viewport);

        width = viewport[2];
        height = viewport[3];

        bytewidth = width * 4;
        bytewidth = (bytewidth + 3) & ~3;
        bytes = bytewidth * height;

        bitmap = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 32, 0, 0, 0, 0);
        glReadPixels(0, 0, width, height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, bitmap->pixels);

        SDL_SaveBMP(bitmap, dumpPath.c_str());

        SDL_FreeSurface(bitmap);

        frame++;
    }
};

/* glue to implementation section */
projectMPlugin *thePlugin = 0;

/* SDL sucks and won't let you use proper C++ functors. :( */
int SDLThreadWrapper(void *unused)
{
    return thePlugin->run(unused);
}

void handle_playback_trigger(gpointer plentry_p, gpointer unused)
{
    PlaylistEntry *entry = reinterpret_cast<PlaylistEntry *>(plentry_p);

    if (!thePlugin)
        return;

    thePlugin->triggerPlaybackBegin(entry);
}

extern "C" void projectM_xmms_init(void)
{
    thePlugin = new projectMPlugin;
    thePlugin->launchThread();
}

extern "C" void projectM_cleanup(void)
{
    if (!thePlugin)
        return;

    delete thePlugin;
}

extern "C" void projectM_render_pcm(gint16 pcm_data[2][512])
{
    if (!thePlugin)
        return;

    thePlugin->addPCMData(pcm_data);
}

/********************************************************************************
 * XXX: This code is from projectM and still needs to be rewritten!             *
 ********************************************************************************/
std::string read_config()
{

//   int n;

    char num[512];
    FILE *in;
    FILE *out;

    char *home;
    char projectM_home[1024];
    char projectM_config[1024];

    strcpy(projectM_config, PROJECTM_PREFIX);
    strcpy(projectM_config + strlen(PROJECTM_PREFIX), CONFIG_FILE);
    projectM_config[strlen(PROJECTM_PREFIX) + strlen(CONFIG_FILE)] = '\0';
    //printf("dir:%s \n",projectM_config);
    home = getenv("HOME");
    strcpy(projectM_home, home);
    strcpy(projectM_home + strlen(home), "/.projectM/config.inp");
    projectM_home[strlen(home) + strlen("/.projectM/config.inp")] = '\0';


    if ((in = fopen(projectM_home, "r")) != 0)
    {
        //printf("reading ~/.projectM/config.inp \n");
        fclose(in);
        return std::string(projectM_home);
    }
    else
    {
        printf("trying to create ~/.projectM/config.inp \n");

        strcpy(projectM_home, home);
        strcpy(projectM_home + strlen(home), "/.projectM");
        projectM_home[strlen(home) + strlen("/.projectM")] = '\0';
        mkdir(projectM_home, 0755);

        strcpy(projectM_home, home);
        strcpy(projectM_home + strlen(home), "/.projectM/config.inp");
        projectM_home[strlen(home) + strlen("/.projectM/config.inp")] = '\0';

        if ((out = fopen(projectM_home, "w")) != 0)
        {

            if ((in = fopen(projectM_config, "r")) != 0)
            {

                while (fgets(num, 80, in) != NULL)
                {
                    fputs(num, out);
                }
                fclose(in);
                fclose(out);


                if ((in = fopen(projectM_home, "r")) != 0)
                {
                    printf("created ~/.projectM/config.inp successfully\n");
                    fclose(in);
                    return std::string(projectM_home);
                }
                else
                {
                    printf("This shouldn't happen, using implementation defualts\n");
                    abort();
                }
            }
            else
            {
                printf("Cannot find projectM default config, using implementation defaults\n");
                abort();
            }
        }
        else
        {
            printf("Cannot create ~/.projectM/config.inp, using default config file\n");
            if ((in = fopen(projectM_config, "r")) != 0)
            {
                printf("Successfully opened default config file\n");
                fclose(in);
                return std::string(projectM_config);
            }
            else
            {
                printf("Using implementation defaults, your system is really messed up, I'm suprised we even got this far\n");
                abort();
            }
        }

    }

    abort();
}
