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
#include "kanashi_utils.h"

JSBool
kanashi_js_fade(JSContext *cx_, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    gint amt;

    if (!JS_ConvertArguments(cx_, argc, argv, "i", &amt))
        return JS_FALSE;

    if (amt < 1)
        amt = 1;

    kanashi_fade(amt);

    *rval = JSVAL_VOID;
    return JS_TRUE;
}

JSBool
kanashi_js_mosaic(JSContext *cx_, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    gint amt;

    if (!JS_ConvertArguments(cx_, argc, argv, "i", &amt))
        return JS_FALSE;

    if (amt < 1)
        amt = 1;

    kanashi_mosaic(amt);

    *rval = JSVAL_VOID;
    return JS_TRUE;
}

JSBool
kanashi_js_blur(JSContext *cx_, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    gint amt, i;

    if (!JS_ConvertArguments(cx_, argc, argv, "i", &amt))
        return JS_FALSE;

    if (amt < 1)
        amt = 1;

    for (i = 0; i < amt; i++)
        kanashi_blur();

    *rval = JSVAL_VOID;
    return JS_TRUE;
}

JSBool
kanashi_js_invert(JSContext *cx_, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    kanashi_invert();

    *rval = JSVAL_VOID;
    return JS_TRUE;
}

JSBool
kanashi_js_set_colormap_gradient(JSContext *cx_, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    gint low_index, high_index;
    struct kanashi_color color;

    if (!JS_ConvertArguments(cx_, argc, argv, "iiiii", &low_index, &high_index, &color.r, &color.g, &color.b))
        return JS_FALSE;

    kanashi_set_colormap_gradient(low_index, high_index, &color);

    *rval = JSVAL_VOID;
    return JS_TRUE;
}

JSBool
kanashi_js_render_horizontal_waveform(JSContext *cx_, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    gint channels, value;

    if (!JS_ConvertArguments(cx_, argc, argv, "ii", &channels, &value))
        return JS_FALSE;

    kanashi_render_horizontal_waveform(channels, value);

    *rval = JSVAL_VOID;
    return JS_TRUE;
}

JSBool
kanashi_js_render_vertical_waveform(JSContext *cx_, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    gint channels, value;

    if (!JS_ConvertArguments(cx_, argc, argv, "ii", &channels, &value))
        return JS_FALSE;

    kanashi_render_vertical_waveform(channels, value);

    *rval = JSVAL_VOID;
    return JS_TRUE;
}

JSBool
kanashi_js_is_beat(JSContext *cx_, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    if (kanashi_new_beat)
        *rval = JSVAL_TRUE;
    else
        *rval = JSVAL_FALSE;

    return JS_TRUE;
}
