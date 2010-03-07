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

#include "config.h"

#include <gtk/gtk.h>

/* flags */
#define ALARM_OFF     (1 << 0)
#define ALARM_DEFAULT (1 << 1)

/* defaults */
#define DEFAULT_ALARM_HOUR      06
#define DEFAULT_ALARM_MIN       30
#define DEFAULT_STOP_HOURS      01
#define DEFAULT_STOP_MINS       00
#define DEFAULT_VOLUME          80
#define DEFAULT_FADING          60
#define DEFAULT_QUIET_VOL       25
#define DEFAULT_FLAGS           ALARM_DEFAULT

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
  guint start, end;
} fader;


#endif /* __ALARM_H */
/*
 * vi:ai:expandtab:ts=2 sts=2 shiftwidth=2:nowrap:
 */
