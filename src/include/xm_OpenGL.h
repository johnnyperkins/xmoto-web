#ifdef ENABLE_OPENGL

#if defined(__EMSCRIPTEN__)
/* No glad: entry points come straight from Emscripten's GL library, with
 * fixed-function support from -sLEGACY_GL_EMULATION. The ARB/EXT names xmoto
 * uses are mapped to their core equivalents. */
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glu.h>
#define glGenBuffersARB glGenBuffers
#define glBindBufferARB glBindBuffer
#define glBufferDataARB glBufferData
#define glDeleteBuffersARB glDeleteBuffers
/* Shaders and FBOs are off at runtime (--noexts); these only need to link. */
#define glGenFramebuffersEXT glGenFramebuffers
#define glBindFramebufferEXT glBindFramebuffer
#define glDeleteFramebuffersEXT glDeleteFramebuffers
#define glFramebufferTexture2DEXT glFramebufferTexture2D
#define glCreateShaderObjectARB glCreateShader
#define glCreateProgramObjectARB glCreateProgram
#define glAttachObjectARB glAttachShader
#define glCompileShaderARB glCompileShader
#define glLinkProgramARB glLinkProgram
#define glShaderSourceARB glShaderSource
#define glUseProgramObjectARB glUseProgram
/* GL_POLYGON is convex by definition, so a fan is identical; the emulation only
 * supports modes up to GL_QUADS. */
#undef GL_POLYGON
#define GL_POLYGON GL_TRIANGLE_FAN
#else
#include <glad.h>
#endif

/* Pull in OpenGL headers */
/* following scissored from SDL_opengl.h */
#if defined(__EMSCRIPTEN__)
/* included above */
#elif defined(__APPLE__) || defined(HAVE_APPLE_OPENGL_FRAMEWORK)
#include <OpenGL/gl.h> /* Header File For The OpenGL Library */
#include <OpenGL/glu.h> /* Header File For The GLU Library */
#elif defined(__MACOS__)
#include <gl.h> /* Header File For The OpenGL Library */
#include <glu.h> /* Header File For The GLU Library */
#else
#include <GL/gl.h> /* Header File For The OpenGL Library */
#include <GL/glu.h> /* Header File For The GLU Library */
#endif

#endif
