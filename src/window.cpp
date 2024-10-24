/************************************************************************/
/*                                                                      */
/* This file is part of VDrift.                                         */
/*                                                                      */
/* VDrift is free software: you can redistribute it and/or modify       */
/* it under the terms of the GNU General Public License as published by */
/* the Free Software Foundation, either version 3 of the License, or    */
/* (at your option) any later version.                                  */
/*                                                                      */
/* VDrift is distributed in the hope that it will be useful,            */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of       */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        */
/* GNU General Public License for more details.                         */
/*                                                                      */
/* You should have received a copy of the GNU General Public License    */
/* along with VDrift.  If not, see <http://www.gnu.org/licenses/>.      */
/*                                                                      */
/************************************************************************/

#include "window.h"
#include "graphics/glcore.h"
#include <SDL2/SDL.h>
#include <sstream>
#include <cassert>
#include <cstdlib>
#include <cstring>

Window::Window() :
    window(NULL),
    glcontext(NULL),
    fsaa(1),
    w(0),
    h(0),
    initialized(false)
{
    // Constructor.
}

Window::~Window()
{
    if (glcontext)
        SDL_GL_DeleteContext(glcontext);

    if (window)
        SDL_DestroyWindow(window);

    if (initialized)
        SDL_Quit();
}

void Window::Init(
    const std::string & caption,
    int resx,
    int resy,
    int depth_bpp,
    int antialiasing,
    bool fullscreen,
    bool vsync,
    std::ostream & info_output,
    std::ostream & error_output)
{
    Uint32 sdl_flags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC;
    if (SDL_Init(sdl_flags) < 0)
    {
        error_output << "SDL initialization failed: " << SDL_GetError() << std::endl;
        return;
    }

    info_output << "SDL initialized successfully." << std::endl;

    ChangeDisplay(
        resx, resy, depth_bpp, antialiasing,
        fullscreen, vsync, false,
        info_output, error_output);

    if (!window)
    {
        error_output << "Failed to create window." << std::endl;
        return;
    }

    if (!glcontext)
    {
        error_output << "No OpenGL context." << std::endl;
        return;
    }

    SDL_SetWindowTitle(window, caption.c_str());

    // Make sure we have a current context before loading GL functions
    if (SDL_GL_MakeCurrent(window, glcontext) < 0)
    {
        error_output << "Failed to make OpenGL context current before loading functions: " << SDL_GetError() << std::endl;
        return;
    }

    info_output << "About to load OpenGL functions..." << std::endl;

    // Load OpenGL functions
    int result = glcLoadFunctions();
    if (result == GLC_LOAD_FAILED)
    {
        error_output << "OpenGL function loading failed." << std::endl;
        initialized = false;
        return;
    }

    info_output << "OpenGL functions loaded successfully." << std::endl;
    initialized = true;

    // Now try to log OpenGL info after functions are loaded
    try 
    {
        LogOpenGLInfo(info_output);
    }
    catch (...)
    {
        error_output << "Error while logging OpenGL information." << std::endl;
    }
}

void Window::SwapBuffers()
{
    SDL_GL_SwapWindow(window);
}

void Window::ShowMouseCursor(bool value)
{
    if (value)
    {
        SDL_ShowCursor(SDL_ENABLE);
        SDL_SetWindowGrab(window, SDL_FALSE);
    }
    else
    {
        SDL_ShowCursor(SDL_DISABLE);
        SDL_SetWindowGrab(window, SDL_TRUE);
    }
}

void Window::Screenshot(const std::string & filename)
{
    SDL_Surface * temp = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 24,
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
                                0x000000FF, 0x0000FF00, 0x00FF0000, 0
#else
                                0x00FF0000, 0x0000FF00, 0x000000FF, 0
#endif
                                );
    assert(temp);

    unsigned char *pixels = (unsigned char *) malloc(3 * w * h);
    assert(pixels);

    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels);

    for (int i = 0; i < h; i++)
        memcpy(((char *) temp->pixels) + temp->pitch * i, pixels + 3 * w * (h - i - 1), w * 3);
    free(pixels);

    SDL_SaveBMP(temp, filename.c_str());
    SDL_FreeSurface(temp);
}

bool Window::Initialized() const
{
    return initialized;
}

int Window::GetW() const
{
    return w;
}

int Window::GetH() const
{
    return h;
}

static int GetVideoDisplay()
{
    const char *variable = SDL_getenv("SDL_VIDEO_FULLSCREEN_DISPLAY");
    if (!variable)
        variable = SDL_getenv("SDL_VIDEO_FULLSCREEN_HEAD");

    if (variable)
        return SDL_atoi(variable);
    else
        return 0;
}

bool Window::ResizeWindow(int width, int height)
{
    // We can't resize something we don't have.
    if (!window || !glcontext)
        return false;

    // Resize window
    SDL_GetWindowSize(window, &w, &h);
    if (w != width || h != height)
        SDL_SetWindowSize(window, width, height);

    w = width;
    h = height;
    return true;
}

void Window::ChangeDisplay(
    int width,
    int height,
    int depth_bpp,
    int antialiasing,
    bool fullscreen,
    bool vsync,
    bool gl3_core,
    std::ostream & info_output,
    std::ostream & error_output)
{
    SDL_DisplayMode desktop_mode;
    int display = GetVideoDisplay();

    SDL_GetDesktopDisplayMode(display, &desktop_mode);

    if (width == 0)
        width = desktop_mode.w;

    if (height == 0)
        height = desktop_mode.h;

    // Try to resize the existing window and surface
    if (!fullscreen && ResizeWindow(width, height))
        return;

    if (glcontext)
    {
        SDL_GL_DeleteContext(glcontext);
        glcontext = NULL;
    }

    if (window)
    {
        SDL_DestroyWindow(window);
    }

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, depth_bpp);

    if (antialiasing > 1)
    {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, antialiasing);
        info_output << "Enabling antialiasing: " << antialiasing << "X" << std::endl;
    }
    else
    {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
        info_output << "Disabling antialiasing" << std::endl;
    }

    // Create a new window
	Uint32 window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
	if (fullscreen)
		window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

	window = SDL_CreateWindow(NULL,
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		width, height, window_flags);
	if (!window)
	{
		error_output << "Window creation failed: " << SDL_GetError() << std::endl;
		return;
	}
	
	
	glcontext = SDL_GL_CreateContext(window);
	if (!glcontext)
	{
		error_output << "OpenGL context creation failed: " << SDL_GetError() << std::endl;
		return;
	}

	// Make our context current
	if (SDL_GL_MakeCurrent(window, glcontext) < 0)
	{
		error_output << "Failed to make OpenGL context current: " << SDL_GetError() << std::endl;
		return;
	}
	
	

	info_output << "Window created successfully." << std::endl;

    int vsync_set = SDL_GL_SetSwapInterval(vsync ? 1 : 0);
    if (vsync_set != -1)
    {
        if (vsync)
            info_output << "Enabling vertical synchronization." << std::endl;
        else
            info_output << "Disabling vertical synchronization." << std::endl;
    }
    else
    {
        info_output << "Setting vertical synchronization not supported." << std::endl;
    }

    w = width;
    h = height;
}

void Window::LogOpenGLInfo(std::ostream & info_output)
{
    std::ostringstream cardinfo;
    cardinfo << "Video card information:" << std::endl;
    cardinfo << "GL Vendor: " << glGetString(GL_VENDOR) << std::endl;
    cardinfo << "GL Renderer: " << glGetString(GL_RENDERER) << std::endl;
    cardinfo << "GL Version: " << glGetString(GL_VERSION) << std::endl;

    GLint texUnits(0), texSize(0);
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &texUnits);
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &texSize);
    cardinfo << "Texture units: " << texUnits << std::endl;
    cardinfo << "Maximum texture size: " << texSize;

    info_output << cardinfo.str() << std::endl;
}