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
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "sapfile.h"

int load_sap (char *fname) {

        struct stat st; 
        int i, k;

        FILE *f;

	/* flush static buffers */

        memset(author, 0, 2048);
        memset(name, 0, 2048);
        memset(date, 0, 2048);
        memset(type, 0, 20);
        
        /* get filesize */

        if (stat(fname, &st) < 0) 
	    
	    return -1;
        
	filesize = st.st_size;
    
        /* open & read file into buffer */

        f = fopen(fname, "rb");

        if ( !f ) 
	    
	    return -1;

        fread(buffer, 1, filesize, f);
	
        fclose(f);
    
        /* check if it is a sap file */
    
        if (strncmp(buffer, "SAP", 3) != 0 ) 
	    
	    return -1;
    
        /* set headersize */
    
        for (i = 0; i < filesize; i++) {
	
	      if (buffer[i] == '\xFF' ) {
	           
	    	    headersize = i;
	            break;
    	      }
	      
    	      if (buffer[i] != '\xFF' && i == filesize-1) 
	    
		    return -1;
       }
     
        /* sanity checking done - parse the info tags 
           please someone rewrite this !!! :)
	   this code sux0r ... */
    
        /* check for author */
	
        k = 0;
	
	for (i = 0;i < headersize; i++) {
	
	     if (strncmp(buffer + i, "AUTHOR", 6) == 0 )  { 
	
		    i = i + 8;

    		    while (buffer[i] != '"' && buffer[i] != 0x0D && buffer[i] != 0x0A) {
        
			author[k] = buffer[i];
		        k++; i++;
                    }
	    	    break;
            }
	    /* notfound */
	    sprintf(author, "%s", "");
        }
    
        /* check for name */
	
	k = 0;
	
        for (i = 0;i < headersize; i++) {
	
	     if ( strncmp(buffer + i, "NAME", 4) == 0 )  { 

		    i = i + 6;
		    
    		    while (buffer[i] != '"' && buffer[i] != 0x0D && buffer[i] != 0x0A) {

		        name[k] = buffer[i];
		        k++; i++;
    		    }
		    break;
             }
	    /* notfound */
	    sprintf(name, "%s", "");
        }

	/* check for date */
	
	k = 0;
	
        for(i = 0;i < headersize; i++) {
	
	     if (strncmp(buffer + i, "DATE", 4) == 0 )  {
	      
		    i = i + 6;

	            while (buffer[i] != '"' && buffer[i] != 0x0D && buffer[i] != 0x0A) {
        
			date[k] = buffer[i];
		        k++; i++;
                    }
		    break;
             }
	    /* notfound */
	    sprintf(date, "%s", "");
	}


        /* check for stereo */
	
	for (i = 0; i < headersize; i++) {
	
	      if (strncmp(buffer + i, "STEREO", 6) == 0 )  { 

			is_stereo = 1;
			break;
              }
	    /* notfound */
	    is_stereo = -1;
	}

        /* check fastplay; */
	
        for (i = 0; i < headersize; i++) {

	      if (strncmp(buffer + i, "FASTPLAY", 8) == 0 )  { 

			k = strtol(&buffer[i + 1 + 8], NULL, 10);

			if ( k == 0 || k > 312 ) 
			 
			     /* illegal fastplay value */
			     return -1;
			
			else 

			     fastplay = k;

				switch (k) {
				
				    case 156:
		
					  times_per_frame = 2;
					  break;
		    		    
				    case 104:
			                    
				          times_per_frame = 3;
			                  break;
					  
			            case 78:
				    
			                  times_per_frame = 4;
					  break;
					  
			       }
		break;
              }
	    /* notfound */
	    fastplay = -1;
	    times_per_frame = 1;
        }

        /* check songs */
        for (i = 0; i < headersize; i++) {

	      if (strncmp(buffer + i, "SONGS", 5) == 0)  { 

			k = strtol(&buffer[i + 1 + 5], NULL, 10);

			if ( (k == 0) || (k > 0xffff) ) 
				 /* illegal songs value */
				 return -1;
			else 
			
				 songs = k;
				 break;
              }
	    /* notfound */
	    songs = -1;
        }
	
        /* check defsong */
        for (i = 0;i < headersize; i++) {

	      if (strncmp(buffer + i, "DEFSONG", 7) == 0 )  { 

			k = strtol(&buffer[i + 1 + 7], NULL, 10);

			if ( (k < 0) || (k > 0xffff) ) 
				 /* illegal defsong value */
				 return -1;
			
			else 
			
				 defsong = k;
				 break;
              }
	    /* notfound */
	    defsong = -1;
        }

	
	/* check songtype */
        for (i = 0;i < headersize; i++) {

	     if (strncmp(buffer + i, "TYPE", 4) == 0 )  { 

		    i = i + 5;
		   
		    if (buffer[i] == 'B') {
			      /* SAP documentation says
			       "any player" */
			      sprintf(type, "Standard");
			      type_h = 'B';
	    		      break;
	            }
	     
	    	    if (buffer[i] == 'S') {
	       
	         	      sprintf(type, "SoftSynth");
			      type_h = 'S';
			      break;
	    	    }
		    
	    	    if (buffer[i] == 'D') {
		
			      sprintf(type, "DigiSynth");
			      type_h = 'D';
			      break;
	    	    }
		    
        	    if (buffer[i] == 'C') {
		    
			      sprintf(type, "CMC Synth");
			      type_h= 'C';
			      break;
	            }
		    
		    if (buffer[i] == 'M') {
			      /* unsupported (?) */
			      sprintf(type, "M-Type");
			      type_h = 'M';
			      break;
		    }	    	    
		    
		    if (buffer[i] == 'R') {
			      /* this is unsupported - perhaps we */
			      /* should return -1 as for now */
			      sprintf(type, "Regs");
			      type_h = 'R';
			      break;
	    	    }	     
		/* unsupported songtype */
		return -1;
              }
	/* no type specified */
	if (i == headersize - 1) return -1;
      
      }

        /* msx_address */
        for (i = 0;i < headersize; i++) {

	      if (strncmp(buffer + i, "MUSIC", 5) == 0 )  { 

			k = strtol(&buffer[i + 1 + 5], NULL, 16);

			if ( (k == 0) || (k > 0xffff) ) 
				 /* illegal address */
				 return -1;
			else 
			
				 msx_address = k;
				 break;
              }
	    /* notfound */
	    msx_address = -1;
        }

         /* ini address */
         for (i = 0;i < headersize; i++) {

	      if (strncmp(buffer + i, "INIT", 4) == 0 )  { 

			k = strtol(&buffer[i + 1 + 4], NULL, 16);

			if ( (k == 0) || (k > 0xffff) ) 
				 /* illegal address */
				 return -1;
			else 
			
				 ini_address = k;
				 break;
              }
	    /* notfound */
	    ini_address = -1;
        }
	 /* player */
         for (i = 0; i < headersize; i++) {

	      if (strncmp(buffer + i,"PLAYER", 6) == 0 )  { 

			k = strtol(&buffer[i + 1 + 6], NULL, 16);

			if ( (k == 0) || (k > 0xffff) ) 
				 /* illegal */
				 return -1;
			else 
			
				 plr_address = k;
				 break;
              }
	    /* notfound */
	    plr_address = -1;
        }

     return 1;
}
