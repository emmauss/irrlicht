// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "IrrCompileConfig.h"

#ifndef __C_IRR_DEVICE_SDL_H_INCLUDED__
#define __C_IRR_DEVICE_SDL_H_INCLUDED__
#endif
#ifndef _IRR_COMPILE_WITH_SDL_DEVICE_
#define _IRR_COMPILE_WITH_SDL_DEVICE_
#endif

#include "CIrrDeviceSDL.h"
#include "IEventReceiver.h"
#include "irrList.h"
#include "os.h"
#include "CTimer.h"
#include "irrString.h"
#include "Keycodes.h"
#include "COSOperator.h"
#include <stdio.h>
#include <stdlib.h>
#include "SIrrCreationParameters.h"
#include <SDL_syswm.h>
#include <SDL_video.h>
#include <SDL.h>
#include <glad/gl.h>

#ifdef _IRR_EMSCRIPTEN_PLATFORM_
#ifdef _IRR_COMPILE_WITH_OGLES2_
#include "CEGLManager.h"
#endif
#include <emscripten.h>
#endif

#ifdef _MSC_VER
#pragma comment(lib, "SDL.lib")
#endif // _MSC_VER

static int SDLDeviceInstances = 0;

namespace irr
{
	namespace video
	{
		#ifdef _IRR_COMPILE_WITH_OPENGL_
		IVideoDriver* createOpenGLDriver(const SIrrlichtCreationParameters& params,
				io::IFileSystem* io, CIrrDeviceSDL* device);
		#endif
	} // end namespace video

} // end namespace irr


namespace irr
{

float g_native_scale_x = 1.0f;
float g_native_scale_y = 1.0f;
//! constructor
CIrrDeviceSDL::CIrrDeviceSDL(const SIrrlichtCreationParameters& param)
	: CIrrDeviceStub(param),
	MouseX(0), MouseY(0), MouseXRel(0), MouseYRel(0), MouseButtonStates(0),
	Width(param.WindowSize.Width), Height(param.WindowSize.Height),
	Resizable(param.WindowResizable), WindowMinimized(false)
{	
	#ifdef _DEBUG
	setDebugName("CIrrDeviceSDL");
	#endif

	if ( ++SDLDeviceInstances == 1 )
	{
		// Initialize SDL... Timer for sleep, video for the obvious, and
		// noparachute prevents SDL from catching fatal errors.
		if (SDL_Init( SDL_INIT_TIMER|SDL_INIT_VIDEO|
#if defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_)
					SDL_INIT_JOYSTICK|
#endif
					SDL_INIT_NOPARACHUTE ) < 0)
		{
			os::Printer::log( "Unable to initialize SDL!", SDL_GetError());
			Close = true;
		}
		else
		{
			os::Printer::log("SDL initialized", ELL_INFORMATION);
		}
	}

//	SDL_putenv("SDL_WINDOWID=");

	SDL_VERSION(&Info.version);

#ifndef __SWITCH__
	SDL_GetWMInfo(&Info);
#endif //_IRR_EMSCRIPTEN_PLATFORM_
	core::stringc sdlversion = "SDL Version ";
	sdlversion += Info.version.major;
	sdlversion += ".";
	sdlversion += Info.version.minor;
	sdlversion += ".";
	sdlversion += Info.version.patch;

	Operator = new COSOperator(sdlversion);
	if ( SDLDeviceInstances == 1 )
	{
		os::Printer::log(sdlversion.c_str(), ELL_INFORMATION);
	}

	// create keymap
	createKeyMap();

	//(void)SDL_EnableKeyRepeat(500, 30);

#ifdef _IRR_EMSCRIPTEN_PLATFORM_
	SDL_Flags |= SDL_OPENGL;
#endif //_IRR_EMSCRIPTEN_PLATFORM_

	// create window
	if (CreationParams.DriverType != video::EDT_NULL)
	{
		// create the window, only if we do not use the null device

		Window = NULL;
		Context = NULL;
		createWindow();
	}

	// create cursor control
	CursorControl = new CCursorControl(this);

	// create driver
	createDriver();

	if (VideoDriver)
		createGUIAndScene();
}


//! destructor
CIrrDeviceSDL::~CIrrDeviceSDL()
{
	if ( --SDLDeviceInstances == 0 )
	{
#if defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_)
		const u32 numJoysticks = Joysticks.size();
		for (u32 i=0; i<numJoysticks; ++i)
			SDL_JoystickClose(Joysticks[i]);
#endif
		if (VideoDriver) {
			VideoDriver->drop();
			VideoDriver = NULL;
		}

		if (Context)
			SDL_GL_DeleteContext(Context);
		if (Window)
			SDL_DestroyWindow(Window);
		SDL_Quit();

		os::Printer::log("Quit SDL", ELL_INFORMATION);
	}
}

void CIrrDeviceSDL::logAttributes()
{
	core::stringc sdl_attr("SDL attribs:");
	int value = 0;
	if ( SDL_GL_GetAttribute( SDL_GL_RED_SIZE, &value ) == 0 )
		sdl_attr += core::stringc(" r:") + core::stringc(value);
	if ( SDL_GL_GetAttribute( SDL_GL_GREEN_SIZE, &value ) == 0 )
		sdl_attr += core::stringc(" g:") + core::stringc(value);
	if ( SDL_GL_GetAttribute( SDL_GL_BLUE_SIZE, &value ) == 0 )
		sdl_attr += core::stringc(" b:") + core::stringc(value);
	if ( SDL_GL_GetAttribute( SDL_GL_ALPHA_SIZE, &value ) == 0 )
		sdl_attr += core::stringc(" a:") + core::stringc(value);

	if ( SDL_GL_GetAttribute( SDL_GL_DEPTH_SIZE, &value) == 0 )
		sdl_attr += core::stringc(" depth:") + core::stringc(value);
	if ( SDL_GL_GetAttribute( SDL_GL_STENCIL_SIZE, &value ) == 0 )
		sdl_attr += core::stringc(" stencil:") + core::stringc(value);
	if ( SDL_GL_GetAttribute( SDL_GL_DOUBLEBUFFER, &value ) == 0 )
		sdl_attr += core::stringc(" doublebuf:") + core::stringc(value);
	if ( SDL_GL_GetAttribute( SDL_GL_MULTISAMPLEBUFFERS, &value ) == 0 )
		sdl_attr += core::stringc(" aa:") + core::stringc(value);
	if ( SDL_GL_GetAttribute( SDL_GL_MULTISAMPLESAMPLES, &value ) == 0 )
		sdl_attr += core::stringc(" aa-samples:") + core::stringc(value);

	os::Printer::log(sdl_attr.c_str());
}


bool versionCorrect(int major, int minor)
{
#ifdef _IRR_COMPILE_WITH_OGLES2_
	return true;
#else
	int created_major = 2;
	int created_minor = 0;
	glGetIntegerv(GL_MAJOR_VERSION, &created_major);
	glGetIntegerv(GL_MINOR_VERSION, &created_minor);
	if (created_major > major || (created_major == major && created_minor >= minor))
		return true;
	return false;
#endif
}

// Used in OptionsScreenVideo for live updating vertical sync config
extern "C" void update_swap_interval(int swap_interval)
{
#ifndef IOS_STK
	// iOS always use vertical sync
	if (swap_interval > 1)
		swap_interval = 1;

	// Try adaptive vsync first if support
	if (swap_interval > 0) {
		int ret = SDL_GL_SetSwapInterval(-1);
		if (ret == 0)
			return;
	}
	SDL_GL_SetSwapInterval(swap_interval);
#endif
}


void CIrrDeviceSDL::tryCreateOpenGLContext(u32 flags)
{

	os::Printer::print("Set Double Buffer", ELL_INFORMATION);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, CreationParams.Doublebuffer);

	os::Printer::print("Set Compat Profile", ELL_INFORMATION);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
			SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);

	if (Context != NULL) {
			os::Printer::print("Delete Context", ELL_INFORMATION);
		SDL_GL_DeleteContext(Context);
		Context = NULL;
	}
	if (Window != NULL) {
		os::Printer::print("Delete Window", ELL_INFORMATION);
		SDL_DestroyWindow(Window);
		Window = NULL;
	}

	os::Printer::print("Creating SDL2 Window with Context 3.2", ELL_INFORMATION);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	Window = SDL_CreateWindow("",
			(float)CreationParams.WindowPosition.X / g_native_scale_x,
			(float)CreationParams.WindowPosition.Y / g_native_scale_y,
			(float)CreationParams.WindowSize.Width / g_native_scale_x,
			(float)CreationParams.WindowSize.Height / g_native_scale_y,
			flags);
	if (Window) {
		os::Printer::print(
				"Creating SDL2 Context 3.2", ELL_INFORMATION);
		Context = SDL_GL_CreateContext(Window);
		os::Printer::print("Context Created. Loading GL", ELL_INFORMATION);
		if (Context && gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress) != 0 &&
				versionCorrect(4, 3))
			return;
	}

	os::Printer::print("Creating SDL2 Window with Context 3.2 Failed", ELL_INFORMATION);

	if (Context != NULL) {
		SDL_GL_DeleteContext(Context);
		Context = NULL;
	}
	if (Window != NULL) {
		SDL_DestroyWindow(Window);
		Window = NULL;
	}
	/*
	os::Printer::print("Creating SDL2 Window with Context 3.3", ELL_INFORMATION);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	Window = SDL_CreateWindow("",
			(float)CreationParams.WindowPosition.X / g_native_scale_x,
			(float)CreationParams.WindowPosition.Y / g_native_scale_y,
			(float)CreationParams.WindowSize.Width / g_native_scale_x,
			(float)CreationParams.WindowSize.Height / g_native_scale_y,
			flags);
	if (Window) {
		Context = SDL_GL_CreateContext(Window);
		if (Context && gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress) != 0 &&
				versionCorrect(3, 3))
			return;
	}

	if (Context != NULL) {
		SDL_GL_DeleteContext(Context);
		Context = NULL;
	}
	if (Window != NULL) {
		SDL_DestroyWindow(Window);
		Window = NULL;
	}
	*/
	os::Printer::print("Creating SDL2 Window with Context 3.1", ELL_INFORMATION);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	Window = SDL_CreateWindow("",
			(float)CreationParams.WindowPosition.X / g_native_scale_x,
			(float)CreationParams.WindowPosition.Y / g_native_scale_y,
			(float)CreationParams.WindowSize.Width / g_native_scale_x,
			(float)CreationParams.WindowSize.Height / g_native_scale_y,
			flags);
	if (Window) {
		Context = SDL_GL_CreateContext(Window);
		if (Context && gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress) != 0 &&
				versionCorrect(3, 1))
			return;
	}

	os::Printer::print("Unable to Create Context", ELL_INFORMATION);
}

bool CIrrDeviceSDL::createWindow()
{
	if ( Close )
		return false;

	
	os::Printer::print("Creating SDL2 Window", ELL_INFORMATION);

	// Ignore alpha size here, this follow irr_driver.cpp:450
	// Try 32 and, upon failure, 24 then 16 bit per pixels
	if (CreationParams.DriverType == video::EDT_OPENGL ||
			CreationParams.DriverType == video::EDT_OGLES2) {
		if (CreationParams.Bits == 32) {
			SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
			SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
			SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
			SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
		} else if (CreationParams.Bits == 24) {
			SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
			SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
			SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
			SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
		} else {
			SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 3);
			SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 3);
			SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 2);
			SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
		}
	}

	u32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI;
	if (CreationParams.Fullscreen)
		flags |= SDL_WINDOW_FULLSCREEN;

	if (CreationParams.DriverType == video::EDT_OPENGL ||
			CreationParams.DriverType == video::EDT_OGLES2)
		flags |= SDL_WINDOW_OPENGL;

#ifdef MOBILE_STK
	flags |= SDL_WINDOW_BORDERLESS | SDL_WINDOW_MAXIMIZED;
#endif

	if (CreationParams.DriverType == video::EDT_OPENGL ||
			CreationParams.DriverType == video::EDT_OGLES2) {
		tryCreateOpenGLContext(flags);
		if (!Window || !Context) {
			os::Printer::log("Could not initialize display!");
			return false;
		}
		os::Printer::log("Context and Window Created");
		SDL_GL_MakeCurrent(Window, Context);
		update_swap_interval(CreationParams.SwapInterval);
	} else {
		Window = SDL_CreateWindow("",
				(float)CreationParams.WindowPosition.X / g_native_scale_x,
				(float)CreationParams.WindowPosition.Y / g_native_scale_y,
				(float)CreationParams.WindowSize.Width / g_native_scale_x,
				(float)CreationParams.WindowSize.Height /
						g_native_scale_y,
				flags);
		if (!Window) {
			os::Printer::log("Could not initialize display!");
			return false;
		}
	}
	return true;
}


//! create the driver
void CIrrDeviceSDL::createDriver()
{
	switch (CreationParams.DriverType) {
	case video::EDT_OPENGL:
#ifdef _IRR_COMPILE_WITH_OPENGL_
		VideoDriver = video::createOpenGLDriver(CreationParams, FileSystem, this);
#else
		os::Printer::log("No OpenGL support compiled in.", ELL_ERROR);
#endif
		break;
	default:
		os::Printer::log("Unable to create video driver of unknown type.",
				ELL_ERROR);
		break;
	}
}


//! runs the device. Returns false if device wants to be deleted
bool CIrrDeviceSDL::run()
{
	os::Timer::tick();

	SEvent irrevent;
	SDL_Event SDL_event;

	while ( !Close && SDL_PollEvent( &SDL_event ) )
	{
		// os::Printer::log("event: ", core::stringc((int)SDL_event.type).c_str(),   ELL_INFORMATION);	// just for debugging

		switch ( SDL_event.type )
		{
		case SDL_MOUSEMOTION:
			irrevent.EventType = irr::EET_MOUSE_INPUT_EVENT;
			irrevent.MouseInput.Event = irr::EMIE_MOUSE_MOVED;
			MouseX = irrevent.MouseInput.X = SDL_event.motion.x;
			MouseY = irrevent.MouseInput.Y = SDL_event.motion.y;
			MouseXRel = SDL_event.motion.xrel;
			MouseYRel = SDL_event.motion.yrel;
			irrevent.MouseInput.ButtonStates = MouseButtonStates;

			postEventFromUser(irrevent);
			break;
		case SDL_MOUSEWHEEL:
			irrevent.MouseInput.Event = irr::EMIE_MOUSE_WHEEL;
			irrevent.MouseInput.Wheel = SDL_event.wheel.y  >  0 ? 1.0f : -1.0f;
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:

			irrevent.EventType = irr::EET_MOUSE_INPUT_EVENT;
			irrevent.MouseInput.X = SDL_event.button.x;
			irrevent.MouseInput.Y = SDL_event.button.y;

			irrevent.MouseInput.Event = irr::EMIE_MOUSE_MOVED;


#ifdef _IRR_EMSCRIPTEN_PLATFORM_
			// Handle mouselocking in emscripten in Windowed mode.
			// In fullscreen SDL will handle it.
			// The behavior we want windowed is - when the canvas was clicked then
			// we will lock the mouse-pointer if it should be invisible.
			// For security reasons this will be delayed until the next mouse-up event.
			// We do not pass on this event as we don't want the activation click to do anything.
			if ( SDL_event.type == SDL_MOUSEBUTTONDOWN && !isFullscreen() )
			{
				EmscriptenPointerlockChangeEvent pointerlockStatus; // let's hope that test is not expensive ...
				if ( emscripten_get_pointerlock_status(&pointerlockStatus) == EMSCRIPTEN_RESULT_SUCCESS )
				{
					if ( CursorControl->isVisible() && pointerlockStatus.isActive )
					{
						emscripten_exit_pointerlock();
						return !Close;
					}
					else if ( !CursorControl->isVisible() && !pointerlockStatus.isActive )
					{
						emscripten_request_pointerlock(0, true);
						return !Close;
					}
				}
			}
#endif

			switch(SDL_event.button.button)
			{
			case SDL_BUTTON_LEFT:
				if (SDL_event.type == SDL_MOUSEBUTTONDOWN)
				{
					irrevent.MouseInput.Event = irr::EMIE_LMOUSE_PRESSED_DOWN;
					MouseButtonStates |= irr::EMBSM_LEFT;
				}
				else
				{
					irrevent.MouseInput.Event = irr::EMIE_LMOUSE_LEFT_UP;
					MouseButtonStates &= !irr::EMBSM_LEFT;
				}
				break;

			case SDL_BUTTON_RIGHT:
				if (SDL_event.type == SDL_MOUSEBUTTONDOWN)
				{
					irrevent.MouseInput.Event = irr::EMIE_RMOUSE_PRESSED_DOWN;
					MouseButtonStates |= irr::EMBSM_RIGHT;
				}
				else
				{
					irrevent.MouseInput.Event = irr::EMIE_RMOUSE_LEFT_UP;
					MouseButtonStates &= !irr::EMBSM_RIGHT;
				}
				break;

			case SDL_BUTTON_MIDDLE:
				if (SDL_event.type == SDL_MOUSEBUTTONDOWN)
				{
					irrevent.MouseInput.Event = irr::EMIE_MMOUSE_PRESSED_DOWN;
					MouseButtonStates |= irr::EMBSM_MIDDLE;
				}
				else
				{
					irrevent.MouseInput.Event = irr::EMIE_MMOUSE_LEFT_UP;
					MouseButtonStates &= !irr::EMBSM_MIDDLE;
				}
				break;
			}

			irrevent.MouseInput.ButtonStates = MouseButtonStates;

			if (irrevent.MouseInput.Event != irr::EMIE_MOUSE_MOVED)
			{
				postEventFromUser(irrevent);

				if ( irrevent.MouseInput.Event >= EMIE_LMOUSE_PRESSED_DOWN && irrevent.MouseInput.Event <= EMIE_MMOUSE_PRESSED_DOWN )
				{
					u32 clicks = checkSuccessiveClicks(irrevent.MouseInput.X, irrevent.MouseInput.Y, irrevent.MouseInput.Event);
					if ( clicks == 2 )
					{
						irrevent.MouseInput.Event = (EMOUSE_INPUT_EVENT)(EMIE_LMOUSE_DOUBLE_CLICK + irrevent.MouseInput.Event-EMIE_LMOUSE_PRESSED_DOWN);
						postEventFromUser(irrevent);
					}
					else if ( clicks == 3 )
					{
						irrevent.MouseInput.Event = (EMOUSE_INPUT_EVENT)(EMIE_LMOUSE_TRIPLE_CLICK + irrevent.MouseInput.Event-EMIE_LMOUSE_PRESSED_DOWN);
						postEventFromUser(irrevent);
					}
				}
			}
			break;

		case SDL_KEYDOWN:
		case SDL_KEYUP:
			{
				SKeyMap mp;
				mp.SDLKey = SDL_event.key.keysym.sym;
				s32 idx = KeyMap.binary_search(mp);

				EKEY_CODE key;
				if (idx == -1) {
					// Fallback to use scancode directly if not found, happens
					// in belarusian keyboard layout for example
					auto it = ScanCodeMap.find(SDL_event.key.keysym.scancode);
					if (it != ScanCodeMap.end())
						key = it->second;
					else
						key = (EKEY_CODE)0;
				} else
					key = (EKEY_CODE)KeyMap[idx].Win32Key;

				irrevent.EventType = irr::EET_KEY_INPUT_EVENT;
				irrevent.KeyInput.Char = 0;
				irrevent.KeyInput.Key = key;
				irrevent.KeyInput.PressedDown = (SDL_event.type == SDL_KEYDOWN);
				irrevent.KeyInput.Shift =
						(SDL_event.key.keysym.mod & KMOD_SHIFT) != 0;
				irrevent.KeyInput.Control =
						(SDL_event.key.keysym.mod & KMOD_CTRL) != 0;
				postEventFromUser(irrevent);
			}
			break;

		case SDL_QUIT:
			Close = true;
			break;

		case SDL_WINDOWEVENT: {
			u32 new_width = SDL_event.window.data1 * g_native_scale_x;
			u32 new_height = SDL_event.window.data2 * g_native_scale_y;
			if (SDL_event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED &&
					((new_width != Width) ||
							(new_height != Height))) {
				Width = new_width;
				Height = new_height;
				if (VideoDriver)
					VideoDriver->OnResize(core::dimension2d<u32>(
							Width, Height));
			} else if (SDL_event.window.event == SDL_WINDOWEVENT_MINIMIZED) {
				WindowMinimized = true;
			} else if (SDL_event.window.event == SDL_WINDOWEVENT_MAXIMIZED) {
				WindowMinimized = false;
			} else if (SDL_event.window.event ==
					SDL_WINDOWEVENT_FOCUS_GAINED) {
				WindowHasFocus = true;
			} else if (SDL_event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
				WindowHasFocus = false;
			} else if (SDL_event.window.event == SDL_WINDOWEVENT_MOVED) {
			}
		} break;
		case SDL_FINGERDOWN:
			irrevent.EventType = EET_TOUCH_INPUT_EVENT;
			irrevent.TouchInput.Event = ETIE_PRESSED_DOWN;
			irrevent.TouchInput.ID = SDL_event.tfinger.fingerId;
			irrevent.TouchInput.X = SDL_event.tfinger.x * 1280;
			irrevent.TouchInput.Y = SDL_event.tfinger.y * 720;
			irrevent.TouchInput.touchedCount = 1;
			postEventFromUser(irrevent);
			break;
		case SDL_FINGERMOTION:
			irrevent.EventType = EET_TOUCH_INPUT_EVENT;
			irrevent.TouchInput.Event = ETIE_MOVED;
			irrevent.TouchInput.ID = SDL_event.tfinger.fingerId;
			irrevent.TouchInput.X = SDL_event.tfinger.x * 1280;
			irrevent.TouchInput.Y = SDL_event.tfinger.y * 720;
			irrevent.TouchInput.touchedCount = 1;
			postEventFromUser(irrevent);
			break;
		case SDL_FINGERUP:
			irrevent.EventType = EET_TOUCH_INPUT_EVENT;
			irrevent.TouchInput.Event = ETIE_LEFT_UP;
			irrevent.TouchInput.ID = SDL_event.tfinger.fingerId;
			irrevent.TouchInput.X = SDL_event.tfinger.x * 1280;
			irrevent.TouchInput.Y = SDL_event.tfinger.y * 720;
			irrevent.TouchInput.touchedCount = 1;
			postEventFromUser(irrevent);
			break;
		case SDL_USEREVENT:
			irrevent.EventType = irr::EET_USER_EVENT;
			irrevent.UserEvent.UserData1 = reinterpret_cast<uintptr_t>(SDL_event.user.data1);
			irrevent.UserEvent.UserData2 = reinterpret_cast<uintptr_t>(SDL_event.user.data2);

			postEventFromUser(irrevent);
			break;

		default:
			break;
		} // end switch

	} // end while

#if defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_)
	// TODO: Check if the multiple open/close calls are too expensive, then
	// open/close in the constructor/destructor instead

	// update joystick states manually
	SDL_JoystickUpdate();
	// we'll always send joystick input events...
	SEvent joyevent;
	joyevent.EventType = EET_JOYSTICK_INPUT_EVENT;
	for (u32 i=0; i<Joysticks.size(); ++i)
	{
		SDL_Joystick* joystick = Joysticks[i];
		if (joystick)
		{
			int j;
			// query all buttons
			const int numButtons = core::min_(SDL_JoystickNumButtons(joystick), 32);
			joyevent.JoystickEvent.ButtonStates=0;
			for (j=0; j<numButtons; ++j)
				joyevent.JoystickEvent.ButtonStates |= (SDL_JoystickGetButton(joystick, j)<<j);

			// query all axes, already in correct range
			const int numAxes = core::min_(SDL_JoystickNumAxes(joystick), (int)SEvent::SJoystickEvent::NUMBER_OF_AXES);
			joyevent.JoystickEvent.Axis[SEvent::SJoystickEvent::AXIS_X]=0;
			joyevent.JoystickEvent.Axis[SEvent::SJoystickEvent::AXIS_Y]=0;
			joyevent.JoystickEvent.Axis[SEvent::SJoystickEvent::AXIS_Z]=0;
			joyevent.JoystickEvent.Axis[SEvent::SJoystickEvent::AXIS_R]=0;
			joyevent.JoystickEvent.Axis[SEvent::SJoystickEvent::AXIS_U]=0;
			joyevent.JoystickEvent.Axis[SEvent::SJoystickEvent::AXIS_V]=0;
			for (j=0; j<numAxes; ++j)
				joyevent.JoystickEvent.Axis[j] = SDL_JoystickGetAxis(joystick, j);

			// we can only query one hat, SDL only supports 8 directions
			if (SDL_JoystickNumHats(joystick)>0)
			{
				switch (SDL_JoystickGetHat(joystick, 0))
				{
					case SDL_HAT_UP:
						joyevent.JoystickEvent.POV=0;
						break;
					case SDL_HAT_RIGHTUP:
						joyevent.JoystickEvent.POV=4500;
						break;
					case SDL_HAT_RIGHT:
						joyevent.JoystickEvent.POV=9000;
						break;
					case SDL_HAT_RIGHTDOWN:
						joyevent.JoystickEvent.POV=13500;
						break;
					case SDL_HAT_DOWN:
						joyevent.JoystickEvent.POV=18000;
						break;
					case SDL_HAT_LEFTDOWN:
						joyevent.JoystickEvent.POV=22500;
						break;
					case SDL_HAT_LEFT:
						joyevent.JoystickEvent.POV=27000;
						break;
					case SDL_HAT_LEFTUP:
						joyevent.JoystickEvent.POV=31500;
						break;
					case SDL_HAT_CENTERED:
					default:
						joyevent.JoystickEvent.POV=65535;
						break;
				}
			}
			else
			{
				joyevent.JoystickEvent.POV=65535;
			}

			// we map the number directly
			joyevent.JoystickEvent.Joystick=static_cast<u8>(i);
			// now post the event
			postEventFromUser(joyevent);
			// and close the joystick
		}
	}
#endif
	return !Close;
}

//! Activate any joysticks, and generate events for them.
bool CIrrDeviceSDL::activateJoysticks(core::array<SJoystickInfo> & joystickInfo)
{
/*
	#if defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_)
		joystickInfo.clear();

		// we can name up to 256 different joysticks
		const int numJoysticks = core::min_(SDL_NumJoysticks(), 256);
		Joysticks.reallocate(numJoysticks);
		joystickInfo.reallocate(numJoysticks);

		int joystick = 0;
		for (; joystick<numJoysticks; ++joystick)
		{
			Joysticks.push_back(SDL_JoystickOpen(joystick));
			SJoystickInfo info;

			info.Joystick = joystick;
			info.Axes = SDL_JoystickNumAxes(Joysticks[joystick]);
			info.Buttons = SDL_JoystickNumButtons(Joysticks[joystick]);
			info.Name = SDL_JoystickName(joystick);
			info.PovHat = (SDL_JoystickNumHats(Joysticks[joystick]) > 0)
							? SJoystickInfo::POV_HAT_PRESENT : SJoystickInfo::POV_HAT_ABSENT;

			joystickInfo.push_back(info);
		}

		for(joystick = 0; joystick < (int)joystickInfo.size(); ++joystick)
		{
			char logString[256];
			(void)sprintf(logString, "Found joystick %d, %d axes, %d buttons '%s'",
			joystick, joystickInfo[joystick].Axes,
			joystickInfo[joystick].Buttons, joystickInfo[joystick].Name.c_str());
			os::Printer::log(logString, ELL_INFORMATION);
		}

		return true;

	#endif // _IRR_COMPILE_WITH_JOYSTICK_EVENTS_
	*/
		return false;
}



//! pause execution temporarily
void CIrrDeviceSDL::yield()
{
	SDL_Delay(0);
}


//! pause execution for a specified time
void CIrrDeviceSDL::sleep(u32 timeMs, bool pauseTimer)
{
	const bool wasStopped = Timer ? Timer->isStopped() : true;
	if (pauseTimer && !wasStopped)
		Timer->stop();

	SDL_Delay(timeMs);

	if (pauseTimer && !wasStopped)
		Timer->start();
}


//! sets the caption of the window
void CIrrDeviceSDL::setWindowCaption(const wchar_t* text)
{
	core::stringc textc = text;
	SDL_SetWindowTitle(Window, textc.c_str());
}


//! presents a surface in the client area
bool CIrrDeviceSDL::present(video::IImage* surface, void* windowId, core::rect<s32>* srcClip)
{
	return false;
}


//! notifies the device that it should close itself
void CIrrDeviceSDL::closeDevice()
{
	Close = true;
}


//! \return Pointer to a list with all video modes supported
video::IVideoModeList* CIrrDeviceSDL::getVideoModeList()
{
	if (!VideoModeList->getVideoModeCount()) {
		// enumerate video modes.
		int display_count = 0;
		if ((display_count = SDL_GetNumVideoDisplays()) < 1) {
			os::Printer::log("No display created: ", SDL_GetError(),
					ELL_ERROR);
			return VideoModeList;
		}

		int mode_count = 0;
		if ((mode_count = SDL_GetNumDisplayModes(0)) < 1) {
			os::Printer::log("No display modes available: ", SDL_GetError(),
					ELL_ERROR);
			return VideoModeList;
		}

		SDL_DisplayMode mode = {SDL_PIXELFORMAT_UNKNOWN, 0, 0, 0, 0};
		if (SDL_GetDesktopDisplayMode(0, &mode) == 0) {
			VideoModeList->setDesktop(SDL_BITSPERPIXEL(mode.format),
					core::dimension2d<u32>(mode.w * g_native_scale_x,
							mode.h * g_native_scale_y));
		}

#ifdef MOBILE_STK
		// SDL2 will return w,h and h,w for mobile STK, as we only use landscape
		// so we just use desktop resolution for now
		VideoModeList.addMode(core::dimension2d<u32>(mode.w * g_native_scale_x,
						      mode.h * g_native_scale_y),
				SDL_BITSPERPIXEL(mode.format));
#else
		for (int i = 0; i < mode_count; i++) {
			if (SDL_GetDisplayMode(0, i, &mode) == 0) {
				VideoModeList->addMode(
						core::dimension2d<u32>(
								mode.w * g_native_scale_x,
								mode.h * g_native_scale_y),
						SDL_BITSPERPIXEL(mode.format));
			}
		}
#endif
	}

	return VideoModeList;
}

//! Sets if the window should be resizable in windowed mode.
void CIrrDeviceSDL::setResizable(bool resize)
{

#if SDL_VERSION_ATLEAST(2, 0, 5)
	if (CreationParams.Fullscreen)
		return;
	SDL_SetWindowResizable(Window, resize ? SDL_TRUE : SDL_FALSE);
	Resizable = resize;
#endif
}


//! Minimizes window if possible
void CIrrDeviceSDL::minimizeWindow()
{

}


//! Maximize window
void CIrrDeviceSDL::maximizeWindow()
{
	// do nothing
}

//! Get the position of this window on screen
core::position2di CIrrDeviceSDL::getWindowPosition()
{
    return core::position2di(-1, -1);
}
//! Get the position of this window on screen
bool CIrrDeviceSDL::getWindowPosition(int* x, int* y)
{
	return true;
}


//! Restore original window size
void CIrrDeviceSDL::restoreWindow()
{
	// do nothing
}

bool CIrrDeviceSDL::isFullscreen() const
{
#ifdef _IRR_EMSCRIPTEN_PLATFORM_
	return SDL_GetWindowFlags(0) == SDL_WINDOW_FULLSCREEN;
#else

	return CIrrDeviceStub::isFullscreen();
#endif
}


//! returns if window is active. if not, nothing need to be drawn
bool CIrrDeviceSDL::isWindowActive() const
{
	return (WindowHasFocus && !WindowMinimized);
}


//! returns if window has focus.
bool CIrrDeviceSDL::isWindowFocused() const
{
	return WindowHasFocus;
}

//! returns if window is minimized.
bool CIrrDeviceSDL::isWindowMinimized() const
{
	return WindowMinimized;
}

//! Set the current Gamma Value for the Display
bool CIrrDeviceSDL::setGammaRamp( f32 red, f32 green, f32 blue, f32 brightness, f32 contrast )
{
	/*
	// todo: Gamma in SDL takes ints, what does Irrlicht use?
	return (SDL_SetGamma(red, green, blue) != -1);
	*/
	return false;
}

//! Get the current Gamma Value for the Display
bool CIrrDeviceSDL::getGammaRamp( f32 &red, f32 &green, f32 &blue, f32 &brightness, f32 &contrast )
{
/*	brightness = 0.f;
	contrast = 0.f;
	return (SDL_GetGamma(&red, &green, &blue) != -1);*/
	return false;
}

//! returns color format of the window.
video::ECOLOR_FORMAT CIrrDeviceSDL::getColorFormat() const
{
	if (Window) {
		u32 pixel_format = SDL_GetWindowPixelFormat(Window);
		if (SDL_BITSPERPIXEL(pixel_format) == 16) {
			if (SDL_ISPIXELFORMAT_ALPHA(pixel_format))
				return video::ECF_A1R5G5B5;
			else
				return video::ECF_R5G6B5;
		} else {
			if (SDL_ISPIXELFORMAT_ALPHA(pixel_format))
				return video::ECF_A8R8G8B8;
			else
				return video::ECF_R8G8B8;
		}
	} else
		return CIrrDeviceStub::getColorFormat();
}


void CIrrDeviceSDL::createKeyMap()
{
	// I don't know if this is the best method  to create
	// the lookuptable, but I'll leave it like that until
	// I find a better version.

	KeyMap.reallocate(105);

	// buttons missing

	KeyMap.push_back(SKeyMap(SDLK_BACKSPACE, KEY_BACK));
	KeyMap.push_back(SKeyMap(SDLK_TAB, KEY_TAB));
	KeyMap.push_back(SKeyMap(SDLK_CLEAR, KEY_CLEAR));
	KeyMap.push_back(SKeyMap(SDLK_RETURN, KEY_RETURN));

	// combined modifiers missing

	KeyMap.push_back(SKeyMap(SDLK_PAUSE, KEY_PAUSE));
	KeyMap.push_back(SKeyMap(SDLK_CAPSLOCK, KEY_CAPITAL));

	// asian letter keys missing

	KeyMap.push_back(SKeyMap(SDLK_ESCAPE, KEY_ESCAPE));

	// asian letter keys missing

	KeyMap.push_back(SKeyMap(SDLK_SPACE, KEY_SPACE));
	KeyMap.push_back(SKeyMap(SDLK_PAGEUP, KEY_PRIOR));
	KeyMap.push_back(SKeyMap(SDLK_PAGEDOWN, KEY_NEXT));
	KeyMap.push_back(SKeyMap(SDLK_END, KEY_END));
	KeyMap.push_back(SKeyMap(SDLK_HOME, KEY_HOME));
	KeyMap.push_back(SKeyMap(SDLK_LEFT, KEY_LEFT));
	KeyMap.push_back(SKeyMap(SDLK_UP, KEY_UP));
	KeyMap.push_back(SKeyMap(SDLK_RIGHT, KEY_RIGHT));
	KeyMap.push_back(SKeyMap(SDLK_DOWN, KEY_DOWN));

	// select missing
	KeyMap.push_back(SKeyMap(SDLK_PRINTSCREEN, KEY_PRINT));
	// execute missing
	KeyMap.push_back(SKeyMap(SDLK_PRINTSCREEN, KEY_SNAPSHOT));

	KeyMap.push_back(SKeyMap(SDLK_INSERT, KEY_INSERT));
	KeyMap.push_back(SKeyMap(SDLK_DELETE, KEY_DELETE));
	KeyMap.push_back(SKeyMap(SDLK_HELP, KEY_HELP));

	KeyMap.push_back(SKeyMap(SDLK_0, KEY_KEY_0));
	KeyMap.push_back(SKeyMap(SDLK_1, KEY_KEY_1));
	KeyMap.push_back(SKeyMap(SDLK_2, KEY_KEY_2));
	KeyMap.push_back(SKeyMap(SDLK_3, KEY_KEY_3));
	KeyMap.push_back(SKeyMap(SDLK_4, KEY_KEY_4));
	KeyMap.push_back(SKeyMap(SDLK_5, KEY_KEY_5));
	KeyMap.push_back(SKeyMap(SDLK_6, KEY_KEY_6));
	KeyMap.push_back(SKeyMap(SDLK_7, KEY_KEY_7));
	KeyMap.push_back(SKeyMap(SDLK_8, KEY_KEY_8));
	KeyMap.push_back(SKeyMap(SDLK_9, KEY_KEY_9));

	KeyMap.push_back(SKeyMap(SDLK_a, KEY_KEY_A));
	KeyMap.push_back(SKeyMap(SDLK_b, KEY_KEY_B));
	KeyMap.push_back(SKeyMap(SDLK_c, KEY_KEY_C));
	KeyMap.push_back(SKeyMap(SDLK_d, KEY_KEY_D));
	KeyMap.push_back(SKeyMap(SDLK_e, KEY_KEY_E));
	KeyMap.push_back(SKeyMap(SDLK_f, KEY_KEY_F));
	KeyMap.push_back(SKeyMap(SDLK_g, KEY_KEY_G));
	KeyMap.push_back(SKeyMap(SDLK_h, KEY_KEY_H));
	KeyMap.push_back(SKeyMap(SDLK_i, KEY_KEY_I));
	KeyMap.push_back(SKeyMap(SDLK_j, KEY_KEY_J));
	KeyMap.push_back(SKeyMap(SDLK_k, KEY_KEY_K));
	KeyMap.push_back(SKeyMap(SDLK_l, KEY_KEY_L));
	KeyMap.push_back(SKeyMap(SDLK_m, KEY_KEY_M));
	KeyMap.push_back(SKeyMap(SDLK_n, KEY_KEY_N));
	KeyMap.push_back(SKeyMap(SDLK_o, KEY_KEY_O));
	KeyMap.push_back(SKeyMap(SDLK_p, KEY_KEY_P));
	KeyMap.push_back(SKeyMap(SDLK_q, KEY_KEY_Q));
	KeyMap.push_back(SKeyMap(SDLK_r, KEY_KEY_R));
	KeyMap.push_back(SKeyMap(SDLK_s, KEY_KEY_S));
	KeyMap.push_back(SKeyMap(SDLK_t, KEY_KEY_T));
	KeyMap.push_back(SKeyMap(SDLK_u, KEY_KEY_U));
	KeyMap.push_back(SKeyMap(SDLK_v, KEY_KEY_V));
	KeyMap.push_back(SKeyMap(SDLK_w, KEY_KEY_W));
	KeyMap.push_back(SKeyMap(SDLK_x, KEY_KEY_X));
	KeyMap.push_back(SKeyMap(SDLK_y, KEY_KEY_Y));
	KeyMap.push_back(SKeyMap(SDLK_z, KEY_KEY_Z));

	KeyMap.push_back(SKeyMap(SDLK_LGUI, KEY_LWIN));
	KeyMap.push_back(SKeyMap(SDLK_RGUI, KEY_RWIN));
	// apps missing
	KeyMap.push_back(SKeyMap(SDLK_POWER, KEY_SLEEP)); //??

	KeyMap.push_back(SKeyMap(SDLK_KP_0, KEY_NUMPAD0));
	KeyMap.push_back(SKeyMap(SDLK_KP_1, KEY_NUMPAD1));
	KeyMap.push_back(SKeyMap(SDLK_KP_2, KEY_NUMPAD2));
	KeyMap.push_back(SKeyMap(SDLK_KP_3, KEY_NUMPAD3));
	KeyMap.push_back(SKeyMap(SDLK_KP_4, KEY_NUMPAD4));
	KeyMap.push_back(SKeyMap(SDLK_KP_5, KEY_NUMPAD5));
	KeyMap.push_back(SKeyMap(SDLK_KP_6, KEY_NUMPAD6));
	KeyMap.push_back(SKeyMap(SDLK_KP_7, KEY_NUMPAD7));
	KeyMap.push_back(SKeyMap(SDLK_KP_8, KEY_NUMPAD8));
	KeyMap.push_back(SKeyMap(SDLK_KP_9, KEY_NUMPAD9));
	KeyMap.push_back(SKeyMap(SDLK_KP_MULTIPLY, KEY_MULTIPLY));
	KeyMap.push_back(SKeyMap(SDLK_KP_PLUS, KEY_ADD));
//	KeyMap.push_back(SKeyMap(SDLK_KP_, KEY_SEPARATOR));
	KeyMap.push_back(SKeyMap(SDLK_KP_MINUS, KEY_SUBTRACT));
	KeyMap.push_back(SKeyMap(SDLK_KP_PERIOD, KEY_DECIMAL));
	KeyMap.push_back(SKeyMap(SDLK_KP_DIVIDE, KEY_DIVIDE));

	KeyMap.push_back(SKeyMap(SDLK_F1,  KEY_F1));
	KeyMap.push_back(SKeyMap(SDLK_F2,  KEY_F2));
	KeyMap.push_back(SKeyMap(SDLK_F3,  KEY_F3));
	KeyMap.push_back(SKeyMap(SDLK_F4,  KEY_F4));
	KeyMap.push_back(SKeyMap(SDLK_F5,  KEY_F5));
	KeyMap.push_back(SKeyMap(SDLK_F6,  KEY_F6));
	KeyMap.push_back(SKeyMap(SDLK_F7,  KEY_F7));
	KeyMap.push_back(SKeyMap(SDLK_F8,  KEY_F8));
	KeyMap.push_back(SKeyMap(SDLK_F9,  KEY_F9));
	KeyMap.push_back(SKeyMap(SDLK_F10, KEY_F10));
	KeyMap.push_back(SKeyMap(SDLK_F11, KEY_F11));
	KeyMap.push_back(SKeyMap(SDLK_F12, KEY_F12));
	KeyMap.push_back(SKeyMap(SDLK_F13, KEY_F13));
	KeyMap.push_back(SKeyMap(SDLK_F14, KEY_F14));
	KeyMap.push_back(SKeyMap(SDLK_F15, KEY_F15));
	// no higher F-keys

	KeyMap.push_back(SKeyMap(SDLK_NUMLOCKCLEAR, KEY_NUMLOCK));
	KeyMap.push_back(SKeyMap(SDLK_SCROLLLOCK, KEY_SCROLL));
	KeyMap.push_back(SKeyMap(SDLK_LSHIFT, KEY_LSHIFT));
	KeyMap.push_back(SKeyMap(SDLK_RSHIFT, KEY_RSHIFT));
	KeyMap.push_back(SKeyMap(SDLK_LCTRL,  KEY_LCONTROL));
	KeyMap.push_back(SKeyMap(SDLK_RCTRL,  KEY_RCONTROL));
	KeyMap.push_back(SKeyMap(SDLK_LALT,  KEY_LMENU));
	KeyMap.push_back(SKeyMap(SDLK_RALT,  KEY_RMENU));

	KeyMap.push_back(SKeyMap(SDLK_PLUS,   KEY_PLUS));
	KeyMap.push_back(SKeyMap(SDLK_COMMA,  KEY_COMMA));
	KeyMap.push_back(SKeyMap(SDLK_MINUS,  KEY_MINUS));
	KeyMap.push_back(SKeyMap(SDLK_PERIOD, KEY_PERIOD));

	// some special keys missing

	KeyMap.sort();
}

} // end namespace irr


