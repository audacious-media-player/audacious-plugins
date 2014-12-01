/*
*
* Author: Giacomo Lozito <james@develia.org>, (C) 2005-2007
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2 of the License, or (at your
* option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*
*/

#ifndef _I_AOSD_TRIGGER_PRIVATE_H
#define _I_AOSD_TRIGGER_PRIVATE_H 1

/* trigger object
   ----------------------------------------------------------
   name           name
   desc           description
   onoff_func     function used to enable/disable the trigger
   callback_func  function called by triggering event
*/
typedef struct
{
  const char * name;
  const char * desc;
  void (*onoff_func)( bool );
  void (*callback_func)( void * , void * );
}
aosd_trigger_t;

#endif /* !_I_AOSD_TRIGGER_PRIVATE_H */
