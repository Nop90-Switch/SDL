/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2004 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Sam Lantinga
    slouken@libsdl.org
*/

#ifdef SAVE_RCSID
static char rcsid =
 "@(#) $Id$";
#endif

/* Handle the event stream, converting Amiga events into SDL events */
#include "SDL.h"

#include "SDL_syswm.h"
#include "SDL_sysevents.h"
#include "SDL_sysvideo.h"
#include "SDL_events_c.h"
#include "SDL_cgxvideo.h"
#include "SDL_cgxmodes_c.h"
#include "SDL_cgximage_c.h"
#include "SDL_cgxwm_c.h"
#include "SDL_amigaevents_c.h"


/* The translation tables from an Amiga keysym to a SDL keysym */
static SDLKey MISC_keymap[256];
SDL_keysym *amiga_TranslateKey(int code, SDL_keysym *keysym);
struct IOStdReq *ConReq=NULL;
struct MsgPort *ConPort=NULL;

/* Note:  The X server buffers and accumulates mouse motion events, so
   the motion event generated by the warp may not appear exactly as we
   expect it to.  We work around this (and improve performance) by only
   warping the pointer when it reaches the edge, and then wait for it.
*/
#define MOUSE_FUDGE_FACTOR	8

#if 0

static inline int amiga_WarpedMotion(_THIS, struct IntuiMessage *m)
{
	int w, h, i;
	int deltax, deltay;
	int posted;

	w = SDL_VideoSurface->w;
	h = SDL_VideoSurface->h;
	deltax = xevent->xmotion.x - mouse_last.x;
	deltay = xevent->xmotion.y - mouse_last.y;
#ifdef DEBUG_MOTION
  printf("Warped mouse motion: %d,%d\n", deltax, deltay);
#endif
	mouse_last.x = xevent->xmotion.x;
	mouse_last.y = xevent->xmotion.y;
	posted = SDL_PrivateMouseMotion(0, 1, deltax, deltay);

	if ( (xevent->xmotion.x < MOUSE_FUDGE_FACTOR) ||
	     (xevent->xmotion.x > (w-MOUSE_FUDGE_FACTOR)) ||
	     (xevent->xmotion.y < MOUSE_FUDGE_FACTOR) ||
	     (xevent->xmotion.y > (h-MOUSE_FUDGE_FACTOR)) ) {
		/* Get the events that have accumulated */
		while ( XCheckTypedEvent(SDL_Display, MotionNotify, xevent) ) {
			deltax = xevent->xmotion.x - mouse_last.x;
			deltay = xevent->xmotion.y - mouse_last.y;
#ifdef DEBUG_MOTION
  printf("Extra mouse motion: %d,%d\n", deltax, deltay);
#endif
			mouse_last.x = xevent->xmotion.x;
			mouse_last.y = xevent->xmotion.y;
			posted += SDL_PrivateMouseMotion(0, 1, deltax, deltay);
		}
		mouse_last.x = w/2;
		mouse_last.y = h/2;
		XWarpPointer(SDL_Display, None, SDL_Window, 0, 0, 0, 0,
					mouse_last.x, mouse_last.y);
		for ( i=0; i<10; ++i ) {
        		XMaskEvent(SDL_Display, PointerMotionMask, xevent);
			if ( (xevent->xmotion.x >
			          (mouse_last.x-MOUSE_FUDGE_FACTOR)) &&
			     (xevent->xmotion.x <
			          (mouse_last.x+MOUSE_FUDGE_FACTOR)) &&
			     (xevent->xmotion.y >
			          (mouse_last.y-MOUSE_FUDGE_FACTOR)) &&
			     (xevent->xmotion.y <
			          (mouse_last.y+MOUSE_FUDGE_FACTOR)) ) {
				break;
			}
#ifdef DEBUG_XEVENTS
  printf("Lost mouse motion: %d,%d\n", xevent->xmotion.x, xevent->xmotion.y);
#endif
		}
#ifdef DEBUG_XEVENTS
		if ( i == 10 ) {
			printf("Warning: didn't detect mouse warp motion\n");
		}
#endif
	}
	return(posted);
}

#endif

static int amiga_GetButton(int code)
{
	switch(code)
	{
		case IECODE_MBUTTON:
			return SDL_BUTTON_MIDDLE;
		case IECODE_RBUTTON:
			return SDL_BUTTON_RIGHT;
		default:
			return SDL_BUTTON_LEFT;
	}
}

static int amiga_DispatchEvent(_THIS,struct IntuiMessage *msg)
{
	int class=msg->Class,code=msg->Code;
	int posted;

	posted = 0;
	switch (class) {
	    /* Gaining mouse coverage? */
	    case IDCMP_ACTIVEWINDOW:
			posted = SDL_PrivateAppActive(1, SDL_APPMOUSEFOCUS);
			break;

	    /* Losing mouse coverage? */
	    case IDCMP_INACTIVEWINDOW:
			posted = SDL_PrivateAppActive(0, SDL_APPMOUSEFOCUS);
			break;
#if 0
	    /* Gaining input focus? */
	    case IDCMP_ACTIVEWINDOW:
			posted = SDL_PrivateAppActive(1, SDL_APPINPUTFOCUS);

			/* Queue entry into fullscreen mode */
			switch_waiting = 0x01 | SDL_FULLSCREEN;
			switch_time = SDL_GetTicks() + 1500;
		    break;

	    /* Losing input focus? */
	    case IDCMP_INACTIVEWINDOW:
			posted = SDL_PrivateAppActive(0, SDL_APPINPUTFOCUS);

		/* Queue leaving fullscreen mode */
			switch_waiting = 0x01;
			switch_time = SDL_GetTicks() + 200;
		    break;
#endif
	    /* Mouse motion? */
	    case IDCMP_MOUSEMOVE:
			if ( SDL_VideoSurface ) {
				posted = SDL_PrivateMouseMotion(0, 0,
						msg->MouseX-SDL_Window->BorderLeft,
						msg->MouseY-SDL_Window->BorderTop);
			}
	    	break;

	    /* Mouse button press? */
		case IDCMP_MOUSEBUTTONS:

			if(!(code&IECODE_UP_PREFIX))
			{
				posted = SDL_PrivateMouseButton(SDL_PRESSED,
						amiga_GetButton(code), 0, 0);
			    }
	    /* Mouse button release? */
			else
			{
				code&=~IECODE_UP_PREFIX;
				posted = SDL_PrivateMouseButton(SDL_RELEASED,
						amiga_GetButton(code), 0, 0);
			}
			break;

	    case IDCMP_RAWKEY:

		    /* Key press? */

		    if( !(code&IECODE_UP_PREFIX) )
		    {
				SDL_keysym keysym;
				posted = SDL_PrivateKeyboard(SDL_PRESSED,
					amiga_TranslateKey(code, &keysym));
		    }
		    else
		    {
	    /* Key release? */

				SDL_keysym keysym;
				code&=~IECODE_UP_PREFIX;

			/* Check to see if this is a repeated key */
/*			if ( ! X11_KeyRepeat(SDL_Display, &xevent) )  */

				posted = SDL_PrivateKeyboard(SDL_RELEASED,
					amiga_TranslateKey(code, &keysym));
		    }
		    break;
	    /* Have we been iconified? */
#if 0
	    case UnmapNotify: {
#ifdef DEBUG_XEVENTS
printf("UnmapNotify!\n");
#endif
		posted=SDL_PrivateAppActive(0, SDL_APPACTIVE|SDL_APPINPUTFOCUS);
	    }
	    break;

	    /* Have we been restored? */

	    case MapNotify: {
#ifdef DEBUG_XEVENTS
printf("MapNotify!\n");
#endif

		posted = SDL_PrivateAppActive(1, SDL_APPACTIVE);

		if ( SDL_VideoSurface &&
		     (SDL_VideoSurface->flags & SDL_FULLSCREEN) )
		{
			CGX_EnterFullScreen(this);
		} else {
			X11_GrabInputNoLock(this, this->input_grab);
		}
		if ( SDL_VideoSurface ) {
			CGX_RefreshDisplay(this);
		}
	    }
	    break;
	    case Expose:
		if ( SDL_VideoSurface && (xevent.xexpose.count == 0) ) {
			CGX_RefreshDisplay(this);
		}
		break;
#endif

	    /* Have we been resized? */
	    case IDCMP_NEWSIZE:
			SDL_PrivateResize(SDL_Window->Width-SDL_Window->BorderLeft-SDL_Window->BorderRight,
		                  SDL_Window->Height-SDL_Window->BorderTop-SDL_Window->BorderBottom);

			break;

	    /* Have we been requested to quit? */
	    case IDCMP_CLOSEWINDOW:
		posted = SDL_PrivateQuit();
		break;

	    /* Do we need to refresh ourselves? */

	    default: {
		/* Only post the event if we're watching for it */
		if ( SDL_ProcessEvents[SDL_SYSWMEVENT] == SDL_ENABLE ) {
			SDL_SysWMmsg wmmsg;

			SDL_VERSION(&wmmsg.version);
#if 0
			wmmsg.subsystem = SDL_SYSWM_CGX;
			wmmsg.event.xevent = xevent;
#endif
			posted = SDL_PrivateSysWMEvent(&wmmsg);
		}
	    }
	    break;
	}
	ReplyMsg((struct Message *)msg);


	return(posted);
}

void amiga_PumpEvents(_THIS)
{
	int pending;
	struct IntuiMessage *m;

	/* Keep processing pending events */
	pending = 0;
	while ( m=(struct IntuiMessage *)GetMsg(SDL_Window->UserPort) ) {
		amiga_DispatchEvent(this,m);
		++pending;
	}
}

void amiga_InitKeymap(void)
{
	int i;

	/* Map the miscellaneous keys */
	for ( i=0; i<SDL_TABLESIZE(MISC_keymap); ++i )
		MISC_keymap[i] = SDLK_UNKNOWN;

	/* These X keysyms have 0xFF as the high byte */
	MISC_keymap[65] = SDLK_BACKSPACE;
	MISC_keymap[66] = SDLK_TAB;
	MISC_keymap[70] = SDLK_CLEAR;
	MISC_keymap[70] = SDLK_DELETE;
	MISC_keymap[68] = SDLK_RETURN;
//	MISC_keymap[XK_Pause&0xFF] = SDLK_PAUSE;
	MISC_keymap[69] = SDLK_ESCAPE;
	MISC_keymap[70] = SDLK_DELETE;
/*
	SDLK_SPACE		= 32,
	SDLK_MINUS		= 45,
	SDLK_LESS		= 60,
	SDLK_COMMA		= 44,
	SDLK_PERIOD		= 46,
	SDLK_0			= 48,
	SDLK_1			= 49,
	SDLK_2			= 50,
	SDLK_3			= 51,
	SDLK_4			= 52,
	SDLK_5			= 53,
	SDLK_6			= 54,
	SDLK_7			= 55,
	SDLK_8			= 56,
	SDLK_9			= 57,
	SDLK_BACKQUOTE		= 96,
	SDLK_BACKSLASH		= 92,
	SDLK_a			= 97,
	SDLK_b			= 98,
	SDLK_c			= 99,
	SDLK_d			= 100,
	SDLK_e			= 101,
	SDLK_f			= 102,
	SDLK_g			= 103,
	SDLK_h			= 104,
	SDLK_i			= 105,
	SDLK_j			= 106,
	SDLK_k			= 107,
	SDLK_l			= 108,
	SDLK_m			= 109,
	SDLK_n			= 110,
	SDLK_o			= 111,
	SDLK_p			= 112,
	SDLK_q			= 113,
	SDLK_r			= 114,
	SDLK_s			= 115,
	SDLK_t			= 116,
	SDLK_u			= 117,
	SDLK_v			= 118,
	SDLK_w			= 119,
	SDLK_x			= 120,
	SDLK_y			= 121,
	SDLK_z			= 122,
*/
	MISC_keymap[15] = SDLK_KP0;		/* Keypad 0-9 */
	MISC_keymap[29] = SDLK_KP1;
	MISC_keymap[30] = SDLK_KP2;
	MISC_keymap[31] = SDLK_KP3;
	MISC_keymap[45] = SDLK_KP4;
	MISC_keymap[46] = SDLK_KP5;
	MISC_keymap[47] = SDLK_KP6;
	MISC_keymap[61] = SDLK_KP7;
	MISC_keymap[62] = SDLK_KP8;
	MISC_keymap[63] = SDLK_KP9;
	MISC_keymap[60] = SDLK_KP_PERIOD;
	MISC_keymap[92] = SDLK_KP_DIVIDE;
	MISC_keymap[93] = SDLK_KP_MULTIPLY;
	MISC_keymap[74] = SDLK_KP_MINUS;
	MISC_keymap[94] = SDLK_KP_PLUS;
	MISC_keymap[67] = SDLK_KP_ENTER;
//	MISC_keymap[XK_KP_Equal&0xFF] = SDLK_KP_EQUALS;

	MISC_keymap[76] = SDLK_UP;
	MISC_keymap[77] = SDLK_DOWN;
	MISC_keymap[78] = SDLK_RIGHT;
	MISC_keymap[79] = SDLK_LEFT;
/*
	MISC_keymap[XK_Insert&0xFF] = SDLK_INSERT;
	MISC_keymap[XK_Home&0xFF] = SDLK_HOME;
	MISC_keymap[XK_End&0xFF] = SDLK_END;
*/
// Mappati sulle parentesi del taastierino
	MISC_keymap[90] = SDLK_PAGEUP;
	MISC_keymap[91] = SDLK_PAGEDOWN;

	MISC_keymap[80] = SDLK_F1;
	MISC_keymap[81] = SDLK_F2;
	MISC_keymap[82] = SDLK_F3;
	MISC_keymap[83] = SDLK_F4;
	MISC_keymap[84] = SDLK_F5;
	MISC_keymap[85] = SDLK_F6;
	MISC_keymap[86] = SDLK_F7;
	MISC_keymap[87] = SDLK_F8;
	MISC_keymap[88] = SDLK_F9;
	MISC_keymap[89] = SDLK_F10;
//	MISC_keymap[XK_F11&0xFF] = SDLK_F11;
//	MISC_keymap[XK_F12&0xFF] = SDLK_F12;
//	MISC_keymap[XK_F13&0xFF] = SDLK_F13;
//	MISC_keymap[XK_F14&0xFF] = SDLK_F14;
//	MISC_keymap[XK_F15&0xFF] = SDLK_F15;

//	MISC_keymap[XK_Num_Lock&0xFF] = SDLK_NUMLOCK;
	MISC_keymap[98] = SDLK_CAPSLOCK;
//	MISC_keymap[XK_Scroll_Lock&0xFF] = SDLK_SCROLLOCK;
	MISC_keymap[97] = SDLK_RSHIFT;
	MISC_keymap[96] = SDLK_LSHIFT;
	MISC_keymap[99] = SDLK_LCTRL;
	MISC_keymap[99] = SDLK_LCTRL;
	MISC_keymap[101] = SDLK_RALT;
	MISC_keymap[100] = SDLK_LALT;
//	MISC_keymap[XK_Meta_R&0xFF] = SDLK_RMETA;
//	MISC_keymap[XK_Meta_L&0xFF] = SDLK_LMETA;
	MISC_keymap[103] = SDLK_LSUPER; /* Left "Windows" */
	MISC_keymap[102] = SDLK_RSUPER; /* Right "Windows */

	MISC_keymap[95] = SDLK_HELP;
}

SDL_keysym *amiga_TranslateKey(int code, SDL_keysym *keysym)
{
	#ifdef STORMC4_WOS
	static struct Library *KeymapBase=NULL; /* Linking failed in WOS version if ConsoleDevice was used */
	#else
	static struct Library *ConsoleDevice=NULL;
	#endif

	/* Get the raw keyboard scancode */
	keysym->scancode = code;
	keysym->sym = MISC_keymap[code];

#ifdef DEBUG_KEYS
	fprintf(stderr, "Translating key 0x%.4x (%d)\n", xsym, xkey->keycode);
#endif
	/* Get the translated SDL virtual keysym */
	if ( keysym->sym==SDLK_UNKNOWN )
	{
		#ifdef STORMC4_WOS
		if(!KeymapBase)
		#else
		if(!ConsoleDevice)
		#endif
		{
			#ifdef STORMC4_WOS
			KeymapBase=OpenLibrary("keymap.library", 0L);
			#else
			if(ConPort=CreateMsgPort())
			{
				if(ConReq=CreateIORequest(ConPort,sizeof(struct IOStdReq)))
				{
					if(!OpenDevice("console.device",-1,(struct IORequest *)ConReq,0))
						ConsoleDevice=(struct Library *)ConReq->io_Device;
					else
					{
						DeleteIORequest(ConReq);
						ConReq=NULL;
					}
				}
				else
				{
					DeleteMsgPort(ConPort);
					ConPort=NULL;
				}
			}
			#endif
		}

		#ifdef STORMC4_WOS
		if(KeymapBase)
		#else
		if(ConsoleDevice)
		#endif
		{
			struct InputEvent event;
			long actual;
			char buffer[5];

			event.ie_Qualifier=0;
			event.ie_Class=IECLASS_RAWKEY;
			event.ie_SubClass=0L;
			event.ie_Code=code;
			event.ie_X=event.ie_Y=0;
			event.ie_EventAddress=NULL;
			event.ie_NextEvent=NULL;
			event.ie_Prev1DownCode=event.ie_Prev1DownQual=event.ie_Prev2DownCode=event.ie_Prev2DownQual=0;

			#ifdef STORMC4_WOS
			if( (actual=MapRawKey(&event,buffer,5,NULL))>=0)
			#else
			if( (actual=RawKeyConvert(&event,buffer,5,NULL))>=0)
			#endif
			{
				if(actual>1)
				{
					D(bug("Warning (%ld) character conversion!\n",actual));
				}
				else if(actual==1)
				{
					keysym->sym=*buffer;
					D(bug("Converted rawcode %ld to <%lc>\n",code,*buffer));
// Bufferizzo x le successive chiamate!
					MISC_keymap[code]=*buffer;
				}
			}
		}

	}
	keysym->mod = KMOD_NONE;

	/* If UNICODE is on, get the UNICODE value for the key */
	keysym->unicode = 0;
	if ( SDL_TranslateUNICODE ) {
#if 0
		static XComposeStatus state;
		/* Until we handle the IM protocol, use XLookupString() */
		unsigned char keybuf[32];
		if ( XLookupString(xkey, (char *)keybuf, sizeof(keybuf),
							NULL, &state) ) {
			keysym->unicode = keybuf[0];
		}
#endif
	}
	return(keysym);
}

void amiga_InitOSKeymap(_THIS)
{
	amiga_InitKeymap();
}
