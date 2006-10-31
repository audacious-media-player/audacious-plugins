#ifndef _PN_UTILS_H
#define _PN_UTILS_H

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define CAP(i,c) (i > c ? c : i < -(c) ? -(c) : i)
#define CAPHILO(i,h,l) (i > h ? h : i < l ? l : i)
#define CAPHI(i,h) (i > h ? h : i)
#define CAPLO(i,l) (i < l ? l : i)

#define PN_IMG_INDEX(x,y) ((x) + (pn_image_data->width * (y)))

#endif /* _PN_UTILS_H */
