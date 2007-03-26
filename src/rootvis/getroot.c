/*  I've modified the following file and renamed it. My modifications were just
 *  about getting rid of some toon.h dependants. If you're interested in the
 *  original version of the file, you can find it either in the sources of
 *  xpenguins or of xsnow.
 *  http://xpenguins.seul.org/ is my actual source.
 *
 *  Johannes Jordan
 */

/* toon_root.c - finding the correct background window / virtual root
 * Copyright (C) 1999-2001  Robin Hogan
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* Since xpenguins version 2.1, the ToonGetRootWindow() function
 * attempts to find the window IDs of
 *
 * 1) The background window that is behind the toplevel client
 *    windows; this is the window that we draw the toons on.
 *
 * 2) The parent window of the toplevel client windows; this is used
 *    by ToonLocateWindows() to build up a map of the space that the
 *    toons can occupy.
 *
 * In simple (sensible?) window managers (e.g. blackbox, sawfish, fvwm
 * and countless others), both of these are the root window. The other
 * more complex scenarios that ToonGetRootWindow() attempts to cope
 * with are:
 *
 * Some `virtual' window managers (e.g. amiwm, swm and tvtwm) that
 * reparent all client windows to a desktop window that sits on top of
 * the root window. This desktop window is easy to find - we just look
 * for a property __SWM_VROOT in the immediate children of the root
 * window that contains the window ID of this desktop window. The
 * desktop plays both roles (1 and 2 above). This functionality was
 * detected in xpenguins 1.x with the vroot.h header file.
 *
 * Enlightenment (0.16) can have a number of desktops with different
 * backgrounds; client windows on these are reparented, except for
 * Desktop 0 which is the root window. Therefore versions less than
 * 2.1 of xpenguins worked on Desktop 0 but not on any others. To fix
 * this we look for a root-window property _WIN_WORKSPACE which
 * contains the numerical index of the currently active desktop. The
 * active desktop is then simply the immediate child of the root
 * window that has a property ENLIGHTENMENT_DESKTOP set to this value.
 *
 * KDE 2.0: Oh dear. The kdesktop is a program separate from the
 * window manager that launches a window which sits behind all the
 * other client windows and has all the icons on it. Thus the other
 * client windows are still children of the root window, but we want
 * to draw to the uppermost window of the kdesktop. This is difficult
 * to find - it is the great-great-grandchild of the root window and
 * in KDE 2.0 has nothing to identify it from its siblings other than
 * its size. KDE 2.1+ usefully implements the __SWM_VROOT property in
 * a child of the root window, but the client windows are still
 * children of the root window. A problem is that the penguins erase
 * the desktop icons when they walk which is a bit messy. The icons
 * are not lost - they reappear when the desktop window gets an expose
 * event (i.e. move some windows over where they were and back again).
 *
 * Nautilus (GNOME 1.4+): Creates a background window to draw icons
 * on, but does not reparent the client windows. The toplevel window
 * of the desktop is indicated by the root window property
 * NAUTILUS_DESKTOP_WINDOW_ID, but then we must descend down the tree
 * from this toplevel window looking for subwindows that are the same
 * size as the screen. The bottom one is the one to draw to. Hopefully
 * one day Nautilus will implement __SWM_VROOT in exactly the same way
 * as KDE 2.1+.
 *
 * Other cases: CDE, the common desktop environment. This is a
 * commercial product that has been packaged with Sun (and other)
 * workstations. It typically implements four virtual desktops but
 * provides NO properties at all for apps such as xpenguins to use to
 * work out where to draw to. Seeing as Sun are moving over to GNOME,
 * CDE use is on the decline so I don't have any current plans to try
 * and get xpenguins to work with it.
 *
 * As a note to developers of window managers and big screen hoggers
 * like kdesktop, please visit www.freedesktop.org and implement their
 * Extended Window Manager Hints spec that help pagers and apps like
 * xpenguins and xearth to find their way around. In particular,
 * please use the _NET_CURRENT_DESKTOP and _NET_VIRTUAL_ROOTS
 * properties if you reparent any windows (e.g. Enlightenment). Since
 * no window managers that I know yet use these particular hints, I
 * haven't yet added any code to parse them.  */

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xproto.h>
#include <string.h>

/* Time to throw up. Here is a kludgey function that recursively calls
 * itself (up to a limit) to find the window ID of the KDE desktop to
 * draw on. It works with KDE 2.0, but since KDE 2.0 is less stable
 * than Windows 95, I don't expect many people to remain using it now
 * that 2.1 is available, which implements __SWM_VROOT and makes this
 * function redundant. This is the hierarchy we're trying to traverse:
 *
 * -> The root window
 * 0 -> window with name="KDE Desktop"
 * 1   -> window with no name
 * 2     -> window with name="KDE Desktop" & _NET_WM_WINDOW_TYPE_DESKTOP
 * 3       -> window with no name and width >= width of screen
 *
 * The last window in the hierarchy is the one to draw to.  The
 * numbers show the value of the `depth' argument.  */
static
Window
__ToonGetKDEDesktop(Display *display, int screen, Window window,
		    Atom atom, char *atomname, int depth)
{
  char *name = NULL;
  unsigned char *wintype = NULL;
  Window winreturn = 0;
  unsigned long nitems, bytesafter;
  Atom actual_type;
  int actual_format;
  Window rootReturn, parentReturn, *children;
  unsigned int nChildren;
  char go_deeper = 0;

  if (XFetchName(display, window, &name)) {
    if (strcasecmp(name, "KDE Desktop") == 0) {
      /* Presumably either at depth 0 or 2 */
      if (XGetWindowProperty(display, window, atom, 0, 1,
			     False, XA_ATOM,
			     &actual_type, &actual_format,
			     &nitems, &bytesafter,
			     &wintype) == Success
	  && wintype) {
	char *tmpatomname = XGetAtomName(display, *(Atom*)wintype);
	if (tmpatomname) {
	  if (strcmp(atomname, tmpatomname) == 0 && depth == 2) {
	    /* OK, at depth 2 */
	    go_deeper = 1;
	  }
	  XFree((char *) tmpatomname);
	}
      }
      else if (depth < 2) {
	go_deeper = 1;
      }
    }
    else if (depth == 1) {
      go_deeper = 1;
    }
    XFree((char *) name);
  }
  else if (depth == 1) {
    go_deeper = 1;
  }

  /* If go_deeper is 1 then there is a possibility that the background
   * window is a descendant of the current window; otherwise we're
   * barking up the wrong tree. */
  if (go_deeper && XQueryTree(display, window, &rootReturn,
			      &parentReturn, &children,
			      &nChildren)) {
    int i;
    for (i = 0; i < nChildren; ++i) {
      /* children[i] is now at depth 3 */
      if (depth == 2) {
	XWindowAttributes attributes;
	if (XGetWindowAttributes(display, children[i], &attributes)) {
	  if (attributes.width >= DisplayWidth(display, screen)/2
	      && attributes.height > 0) {
	    /* Found it! */
	    winreturn = children[i];
	    break;
	  }
	}
      }
      else if ((winreturn = __ToonGetKDEDesktop(display, screen,
						children[i],
						atom, atomname,
						depth+1))) {
	break;
      }
    }
    XFree((char *) children);
  }

  return winreturn;
}

/* Look for the Nautilus desktop window to draw to, given the toplevel
 * window of the Nautilus desktop. Basically recursively calls itself
 * looking for subwindows the same size as the root window. */
static
Window
__ToonGetNautilusDesktop(Display *display, int screen, Window window,
			 int depth)
{
  Window rootReturn, parentReturn, *children;
  Window winreturn = window;
  unsigned int nChildren;

  if (depth > 5) {
    return ((Window) 0);
  }
  else if (XQueryTree(display, window, &rootReturn, &parentReturn,
		 &children, &nChildren)) {
    int i;
    for (i = 0; i < nChildren; ++i) {
      XWindowAttributes attributes;
      if (XGetWindowAttributes(display, children[i], &attributes)) {
	if (attributes.width == DisplayWidth(display, screen)
	    && attributes.height == DisplayHeight(display, screen)) {
	  /* Found a possible desktop window */
	  winreturn = __ToonGetNautilusDesktop(display, screen,
					       children[i], depth+1);
	}
      }
    }
    XFree((char *) children);
  }
  return winreturn;
}


/*
 * Returns the window ID of the `background' window on to which the
 * toons should be drawn. Also returned (in clientparent) is the ID of
 * the parent of all the client windows, since this may not be the
 * same as the background window. If no recognised virtual window
 * manager or desktop environment is found then the root window is
 * returned in both cases. The string toon_message contains
 * information about the window manager that was found.
 */
Window
ToonGetRootWindow(Display *display, int screen, Window *clientparent)
{
  Window background = 0; /* The return value */
  Window root = RootWindow(display, screen);
  Window rootReturn, parentReturn, *children;
  unsigned char *toplevel = NULL;
  unsigned int nChildren;
  unsigned long nitems, bytesafter;
  Atom actual_type;
  int actual_format;
  unsigned char *workspace = NULL;
  unsigned char *desktop = NULL;
  Atom NAUTILUS_DESKTOP_WINDOW_ID = XInternAtom(display,
			   "NAUTILUS_DESKTOP_WINDOW_ID",
						False);

  *clientparent = root;

  if (XGetWindowProperty(display, root,
			 NAUTILUS_DESKTOP_WINDOW_ID,
			 0, 1, False, XA_WINDOW,
			 &actual_type, &actual_format,
			 &nitems, &bytesafter,
			 &toplevel) == Success
      && toplevel) {
    /* Nautilus is running */
    background = __ToonGetNautilusDesktop(display, screen,
					*(Window*)toplevel, 0);
    XFree(toplevel);
  }

  /* Next look for a virtual root or a KDE Desktop */
  if (!background
      && XQueryTree(display, root, &rootReturn, &parentReturn,
		    &children, &nChildren)) {
    int i;
    Atom _NET_WM_WINDOW_TYPE = XInternAtom(display,
			     "_NET_WM_WINDOW_TYPE",
					   False);
    Atom __SWM_VROOT = XInternAtom(display, "__SWM_VROOT", False);

    for (i = 0; i < nChildren && !background; ++i) {
      unsigned char *newroot = NULL;
      if (XGetWindowProperty(display, children[i],
			     __SWM_VROOT, 0, 1, False, XA_WINDOW,
			     &actual_type, &actual_format,
			     &nitems, &bytesafter,
			     &newroot) == Success
	  && newroot) {
	/* Found a window with a __SWM_VROOT property that contains
	 * the window ID of the virtual root. Now we must check
	 * whether it is KDE (2.1+) or not. If it is KDE then it does
	 * not reparent the clients. If the root window has the
	 * _NET_SUPPORTED property but not the _NET_VIRTUAL_ROOTS
	 * property then we assume it is KDE. */
	Atom _NET_SUPPORTED = XInternAtom(display,
					  "_NET_SUPPORTED",
					  False);
	unsigned char *tmpatom;
	if (XGetWindowProperty(display, root,
			       _NET_SUPPORTED, 0, 1, False,
			       XA_ATOM, &actual_type, &actual_format,
			       &nitems, &bytesafter,
			       &tmpatom) == Success
	    && tmpatom) {
	  unsigned char *tmpwindow = NULL;
	  Atom _NET_VIRTUAL_ROOTS = XInternAtom(display,
						"_NET_VIRTUAL_ROOTS",
						False);
	  XFree(tmpatom);
	  if (XGetWindowProperty(display, root,
				 _NET_VIRTUAL_ROOTS, 0, 1, False,
				 XA_WINDOW, &actual_type, &actual_format,
				 &nitems, &bytesafter,
				 &tmpwindow) != Success
	      || !tmpwindow) {
	    /* Must be KDE 2.1+ */
	    background = *(Window*)newroot;
	  }
	  else if (tmpwindow) {
	    XFree(tmpwindow);
	  }
	}

	if (!background) {
	  /* Not KDE: assume windows are reparented */
	  background = *clientparent = *newroot;
	}
	XFree((char *) newroot);
      }
      else background = __ToonGetKDEDesktop(display, screen, children[i],
						 _NET_WM_WINDOW_TYPE,
						 "_NET_WM_WINDOW_TYPE_DESKTOP",
						 0);
    }
    XFree((char *) children);
  }

  if (!background) {
    /* Look for a _WIN_WORKSPACE property, used by Enlightenment */
    Atom _WIN_WORKSPACE = XInternAtom(display, "_WIN_WORKSPACE", False);
    if (XGetWindowProperty(display, root, _WIN_WORKSPACE,
			   0, 1, False, XA_CARDINAL,
			   &actual_type, &actual_format,
			   &nitems, &bytesafter,
			   &workspace) == Success
	&& workspace) {
      /* Found a _WIN_WORKSPACE property - this is the desktop to look for.
       * For now assume that this is Enlightenment.
       * We're looking for a child of the root window that has an
       * ENLIGHTENMENT_DESKTOP atom with a value equal to the root window's
       * _WIN_WORKSPACE atom. */
      Atom ENLIGHTENMENT_DESKTOP = XInternAtom(display,
					       "ENLIGHTENMENT_DESKTOP",
					       False);
      /* First check to see if the root window is the current desktop... */
      if (XGetWindowProperty(display, root,
			     ENLIGHTENMENT_DESKTOP, 0, 1,
			     False, XA_CARDINAL,
			     &actual_type, &actual_format,
			     &nitems, &bytesafter,
			     &desktop) == Success
	  && desktop && *desktop == *workspace) {
	/* The root window is the current Enlightenment desktop */
	background = root;
	XFree(desktop);
      }
      /* Now look at each immediate child window of root to see if it is
       * the current desktop */
      else if (XQueryTree(display, root, &rootReturn, &parentReturn,
			  &children, &nChildren)) {
	int i;
	for (i = 0; i < nChildren; ++i) {
	  if (XGetWindowProperty(display, children[i],
				 ENLIGHTENMENT_DESKTOP, 0, 1,
				 False, XA_CARDINAL,
				 &actual_type, &actual_format,
				 &nitems, &bytesafter,
				 &desktop) == Success
	      && desktop && *desktop == *workspace) {
	    /* Found current Enlightenment desktop */
	    background = *clientparent = children[i];
	    XFree(desktop);
	  }
	}
	XFree((char *) children);
      }
      XFree(workspace);
    }
  }
  if (background) {
    return background;
  }
  else {
    return root;
  }
}
