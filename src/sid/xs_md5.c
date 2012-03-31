/* Wrapper for GLib MD5 implementation */
/* John Lindgren, March 31, 2012 */
/* Public domain */

#include <glib.h>

#include "xs_md5.h"

void xs_md5_init (xs_md5state_t * state)
{
    state->priv = g_checksum_new (G_CHECKSUM_MD5);
}

void xs_md5_append (xs_md5state_t * state, const void * data, int length)
{
    g_checksum_update (state->priv, data, length);
}

void xs_md5_finish (xs_md5state_t * state, xs_md5hash_t hash)
{
    gsize length = XS_MD5HASH_LENGTH;
    g_checksum_get_digest (state->priv, hash, & length);
    g_checksum_free (state->priv);
    state->priv = NULL;
}
