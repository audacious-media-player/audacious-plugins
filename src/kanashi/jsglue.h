/*
 * kanashi: iterated javascript-driven visualization engine
 * Copyright (c) 2009 William Pitcock <nenolod@dereferenced.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "kanashi.h"

extern JSBool kanashi_js_fade(JSContext *cx_, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
extern JSBool kanashi_js_blur(JSContext *cx_, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
extern JSBool kanashi_js_mosaic(JSContext *cx_, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
extern JSBool kanashi_js_invert(JSContext *cx_, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

extern JSBool kanashi_js_set_colormap_gradient(JSContext *cx_, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

extern JSBool kanashi_js_render_horizontal_waveform(JSContext *cx_, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
extern JSBool kanashi_js_render_vertical_waveform(JSContext *cx_, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
extern JSBool kanashi_js_render_line(JSContext *cx_, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
extern JSBool kanashi_js_render_dot(JSContext *cx_, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

extern JSBool kanashi_js_is_beat(JSContext *cx_, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

extern JSBool kanashi_js_get_canvas_width(JSContext *cx_, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
extern JSBool kanashi_js_get_canvas_height(JSContext *cx_, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

extern JSBool kanashi_js_get_pcm_data(JSContext *cx_, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

extern JSBool kanashi_js_translate_polar_x(JSContext *cx_, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
extern JSBool kanashi_js_translate_polar_y(JSContext *cx_, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

