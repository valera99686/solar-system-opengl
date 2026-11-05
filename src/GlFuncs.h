#pragma once

// Власний завантажувач функцій OpenGL 3.3 core.
//
// Windows віддає з opengl32.dll лише OpenGL 1.1; решту функцій драйвер надає через
// glfwGetProcAddress. Замість зовнішнього GLAD/GLEW тут перелічено тільки ті функції,
// які справді використовує програма (X-макрос). Типи й константи беруться зі стандартного
// заголовка Khronos <GL/glcorearb.h>, що входить до MinGW-w64.

#define GLFW_INCLUDE_NONE
#include <GL/glcorearb.h>

#include <string>

#define GL_FUNCTION_LIST(X)                                          \
    X(PFNGLCLEARPROC, glClear)                                       \
    X(PFNGLCLEARCOLORPROC, glClearColor)                             \
    X(PFNGLCLEARDEPTHPROC, glClearDepth)                             \
    X(PFNGLCLEARSTENCILPROC, glClearStencil)                         \
    X(PFNGLENABLEPROC, glEnable)                                     \
    X(PFNGLDISABLEPROC, glDisable)                                   \
    X(PFNGLISENABLEDPROC, glIsEnabled)                               \
    X(PFNGLBLENDFUNCPROC, glBlendFunc)                               \
    X(PFNGLDEPTHFUNCPROC, glDepthFunc)                               \
    X(PFNGLDEPTHMASKPROC, glDepthMask)                               \
    X(PFNGLCOLORMASKPROC, glColorMask)                               \
    X(PFNGLCULLFACEPROC, glCullFace)                                 \
    X(PFNGLFRONTFACEPROC, glFrontFace)                               \
    X(PFNGLVIEWPORTPROC, glViewport)                                 \
    X(PFNGLPOLYGONMODEPROC, glPolygonMode)                           \
    X(PFNGLLINEWIDTHPROC, glLineWidth)                               \
    X(PFNGLGETERRORPROC, glGetError)                                 \
    X(PFNGLGETINTEGERVPROC, glGetIntegerv)                           \
    X(PFNGLGETBOOLEANVPROC, glGetBooleanv)                           \
    X(PFNGLGETFLOATVPROC, glGetFloatv)                               \
    X(PFNGLGETSTRINGPROC, glGetString)                               \
    X(PFNGLREADPIXELSPROC, glReadPixels)                             \
    X(PFNGLPIXELSTOREIPROC, glPixelStorei)                           \
    X(PFNGLSTENCILFUNCPROC, glStencilFunc)                           \
    X(PFNGLSTENCILOPPROC, glStencilOp)                               \
    X(PFNGLSTENCILMASKPROC, glStencilMask)                           \
    X(PFNGLGENTEXTURESPROC, glGenTextures)                           \
    X(PFNGLBINDTEXTUREPROC, glBindTexture)                           \
    X(PFNGLDELETETEXTURESPROC, glDeleteTextures)                     \
    X(PFNGLTEXIMAGE2DPROC, glTexImage2D)                             \
    X(PFNGLTEXPARAMETERIPROC, glTexParameteri)                       \
    X(PFNGLTEXPARAMETERFPROC, glTexParameterf)                       \
    X(PFNGLGENERATEMIPMAPPROC, glGenerateMipmap)                     \
    X(PFNGLACTIVETEXTUREPROC, glActiveTexture)                       \
    X(PFNGLDRAWELEMENTSPROC, glDrawElements)                         \
    X(PFNGLDRAWARRAYSPROC, glDrawArrays)                             \
    X(PFNGLDRAWELEMENTSINSTANCEDPROC, glDrawElementsInstanced)       \
    X(PFNGLDRAWBUFFERPROC, glDrawBuffer)                             \
    X(PFNGLREADBUFFERPROC, glReadBuffer)                             \
    X(PFNGLFINISHPROC, glFinish)                                     \
    X(PFNGLPOLYGONOFFSETPROC, glPolygonOffset)                       \
    X(PFNGLGENBUFFERSPROC, glGenBuffers)                             \
    X(PFNGLBINDBUFFERPROC, glBindBuffer)                             \
    X(PFNGLBUFFERDATAPROC, glBufferData)                             \
    X(PFNGLBUFFERSUBDATAPROC, glBufferSubData)                       \
    X(PFNGLDELETEBUFFERSPROC, glDeleteBuffers)                       \
    X(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays)                   \
    X(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray)                   \
    X(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays)             \
    X(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray)   \
    X(PFNGLDISABLEVERTEXATTRIBARRAYPROC, glDisableVertexAttribArray) \
    X(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer)           \
    X(PFNGLVERTEXATTRIBDIVISORPROC, glVertexAttribDivisor)           \
    X(PFNGLCREATESHADERPROC, glCreateShader)                         \
    X(PFNGLSHADERSOURCEPROC, glShaderSource)                         \
    X(PFNGLCOMPILESHADERPROC, glCompileShader)                       \
    X(PFNGLGETSHADERIVPROC, glGetShaderiv)                           \
    X(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog)                 \
    X(PFNGLDELETESHADERPROC, glDeleteShader)                         \
    X(PFNGLCREATEPROGRAMPROC, glCreateProgram)                       \
    X(PFNGLATTACHSHADERPROC, glAttachShader)                         \
    X(PFNGLLINKPROGRAMPROC, glLinkProgram)                           \
    X(PFNGLGETPROGRAMIVPROC, glGetProgramiv)                         \
    X(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog)               \
    X(PFNGLDELETEPROGRAMPROC, glDeleteProgram)                       \
    X(PFNGLUSEPROGRAMPROC, glUseProgram)                             \
    X(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation)             \
    X(PFNGLUNIFORM1IPROC, glUniform1i)                               \
    X(PFNGLUNIFORM1FPROC, glUniform1f)                               \
    X(PFNGLUNIFORM2FPROC, glUniform2f)                               \
    X(PFNGLUNIFORM3FPROC, glUniform3f)                               \
    X(PFNGLUNIFORM4FPROC, glUniform4f)                               \
    X(PFNGLUNIFORMMATRIX3FVPROC, glUniformMatrix3fv)                 \
    X(PFNGLUNIFORMMATRIX4FVPROC, glUniformMatrix4fv)                 \
    X(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers)                   \
    X(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer)                   \
    X(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers)             \
    X(PFNGLFRAMEBUFFERTEXTUREPROC, glFramebufferTexture)             \
    X(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D)         \
    X(PFNGLFRAMEBUFFERRENDERBUFFERPROC, glFramebufferRenderbuffer)   \
    X(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus)     \
    X(PFNGLGENRENDERBUFFERSPROC, glGenRenderbuffers)                 \
    X(PFNGLBINDRENDERBUFFERPROC, glBindRenderbuffer)                 \
    X(PFNGLDELETERENDERBUFFERSPROC, glDeleteRenderbuffers)           \
    X(PFNGLRENDERBUFFERSTORAGEPROC, glRenderbufferStorage)           \
    X(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC, glRenderbufferStorageMultisample) \
    X(PFNGLBLITFRAMEBUFFERPROC, glBlitFramebuffer)                   \
    X(PFNGLGENQUERIESPROC, glGenQueries)                             \
    X(PFNGLDELETEQUERIESPROC, glDeleteQueries)                       \
    X(PFNGLBEGINQUERYPROC, glBeginQuery)                             \
    X(PFNGLENDQUERYPROC, glEndQuery)                                 \
    X(PFNGLGETQUERYOBJECTUIVPROC, glGetQueryObjectuiv)

#define GL_DECLARE_FUNCTION(type, name) extern type name;
GL_FUNCTION_LIST(GL_DECLARE_FUNCTION)
#undef GL_DECLARE_FUNCTION

// Завантажує всі функції через getProc (glfwGetProcAddress). Повертає false, якщо якоїсь
// не знайдено; імена відсутніх функцій дописуються в *missing.
bool loadGlFunctions(void* (*getProc)(const char*), std::string* missing = nullptr);
