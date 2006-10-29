/*
 * SAP xmms plug-in. 
 * Copyright 2002/2003 by Michal 'Mikey' Szwaczko <mikey@scene.pl>
 *
 * SAP Library ver. 1.56 by Adam Bienias
 *
 * This is free software. You can modify it and distribute it under the terms
 * of the GNU General Public License. The verbatim text of the license can 
 * be found in file named COPYING in the source directory.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR 
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

static GtkWidget *title_entry,  *performer_entry;
static GtkWidget *date_entry;
static GtkWidget *filename_entry;
static GtkWidget *window = NULL;

extern int load_sap (char *);
extern unsigned char buffer;

extern long filesize;
extern int headersize;    
extern char author[];
extern char name[];
extern char date[];
extern int is_stereo;
extern int fastplay;
extern char type_h; /* type as seen in SAP header; */
extern char type[]; /* type in ascii; */
extern int plr_address;
extern int msx_address;
extern int ini_address;
extern int songs;
extern int defsong; 
extern int times_per_frame;
