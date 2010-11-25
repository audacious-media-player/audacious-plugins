/*
 * Copyright (c) 2010 William Pitcock <nenolod@dereferenced.org>.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <glib/gi18n.h>
#include <audacious/plugin.h>
#include <audacious/interface.h>

static gboolean daemon_initialize(InterfaceCbs * cbs)
{
	gint i;

#if 0
	i = fork();
	if (i < 0)
	{
		g_print("cannot fork into background.\n");
		return FALSE;
	}
	else if (i > 0)
	{
		g_print("forked as pid %d\n", i);
		exit(EXIT_FAILURE);
	}

	if (setsid() < 0)
	{
		g_print("unable to create new session: %s\n", strerror(errno));
		return FALSE;
	}
#endif

	return TRUE;
}

static gboolean daemon_finalize(void)
{
	return TRUE;
}

Interface daemon_interface = {
    .id = "daemon",
    .desc = N_("Daemon Interface (like old headless mode)"),
    .init = daemon_initialize,
    .fini = daemon_finalize,
};

SIMPLE_INTERFACE_PLUGIN(daemon, &daemon_interface);
