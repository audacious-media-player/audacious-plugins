/*
 * $Id: alarm.h,v 1.2 2003/11/23 19:29:36 adamf Exp $
 * alarm.h
 *
 * Adam Feakin
 * adamf@snika.uklinux.net
 *
 * we need some struct to hold the info about the days
 */
#ifndef __ALARM_H
#define __ALARM_H

#include <gtk/gtk.h>

/* flags */
#define ALARM_OFF     (1 << 0)
#define ALARM_DEFAULT (1 << 1)

typedef struct AlarmDay {
  GtkCheckButton *cb;
  GtkCheckButton *cb_def;
  GtkSpinButton *spin_hr;
  GtkSpinButton *spin_min;
  int flags;
  int hour;
  int min;
} alarmday;


typedef struct Fader {
  unsigned start, end;
} fader;


#endif /* __ALARM_H */
