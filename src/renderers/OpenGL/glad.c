#if RELEASE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glad.h"

static void *get_proc(const char *namez);

#if defined(_WIN32) || defined(__CYGWIN__)
#ifndef _WINDOWS_
#undef APIENTRY
#endif
#include <windows.h>
static HMODULE libGL;

typedef void *(APIENTRYP PFNWGLGETPROCADDRESSPROC_PRIVATE)(const char *);
static PFNWGLGETPROCADDRESSPROC_PRIVATE gladGetProcAddressPtr;

#ifdef _MSC_VER
#ifdef __has_include
#if __has_include(<winapifamily.h>)
#define HAVE_WINAPIFAMILY 1
#endif
#elif _MSC_VER >= 1700 && !_USING_V110_SDK71_
#define HAVE_WINAPIFAMILY 1
#endif
#endif

#ifdef HAVE_WINAPIFAMILY
#include <winapifamily.h>
#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP)
#define IS_UWP 1
#endif
#endif

static
int open_gl(void) {
#ifndef IS_UWP
    libGL = LoadLibraryW(L"opengl32.dll");
    if (libGL != NULL) {
        void (*tmp)(void);
        tmp = (void(*)(void)) GetProcAddress(libGL, "wglGetProcAddress");
        gladGetProcAddressPtr = (PFNWGLGETPROCADDRESSPROC_PRIVATE)tmp;
        return gladGetProcAddressPtr != NULL;
    }
#endif

    return 0;
}

static
void close_gl(void) {
    if (libGL != NULL) {
        FreeLibrary((HMODULE)libGL);
        libGL = NULL;
    }
}
#else
#include <dlfcn.h>
static void *libGL;

#if !defined(__APPLE__) && !defined(__HAIKU__)
typedef void *(APIENTRYP PFNGLXGETPROCADDRESSPROC_PRIVATE)(const char *);
static PFNGLXGETPROCADDRESSPROC_PRIVATE gladGetProcAddressPtr;
#endif

static
int open_gl(void) {
#ifdef __APPLE__
    static const char *NAMES[] = {
        "../Frameworks/OpenGL.framework/OpenGL",
        "/Library/Frameworks/OpenGL.framework/OpenGL",
        "/System/Library/Frameworks/OpenGL.framework/OpenGL",
        "/System/Library/Frameworks/OpenGL.framework/Versions/Current/OpenGL"
    };
#else
    static const char *NAMES[] = { "libGL.so.1", "libGL.so" };
#endif

    unsigned int index = 0;
    for (index = 0; index < (sizeof(NAMES) / sizeof(NAMES[0])); index++) {
        libGL = dlopen(NAMES[index], RTLD_NOW | RTLD_GLOBAL);

        if (libGL != NULL) {
#if defined(__APPLE__) || defined(__HAIKU__)
            return 1;
#else
            gladGetProcAddressPtr = (PFNGLXGETPROCADDRESSPROC_PRIVATE)dlsym(libGL,
                "glXGetProcAddressARB");
            return gladGetProcAddressPtr != NULL;
#endif
        }
    }

    return 0;
}

static
void close_gl(void) {
    if (libGL != NULL) {
        dlclose(libGL);
        libGL = NULL;
    }
}
#endif

static
void *get_proc(const char *namez) {
    void *result = NULL;
    if (libGL == NULL) return NULL;

#if !defined(__APPLE__) && !defined(__HAIKU__)
    if (gladGetProcAddressPtr != NULL) {
        result = gladGetProcAddressPtr(namez);
    }
#endif
    if (result == NULL) {
#if defined(_WIN32) || defined(__CYGWIN__)
        result = (void *)GetProcAddress((HMODULE)libGL, namez);
#else
        result = dlsym(libGL, namez);
#endif
    }

    return result;
}

int gladLoadGL(void) {
    int status = 0;

    if (open_gl()) {
        status = gladLoadGLLoader(&get_proc);
        close_gl();
    }

    return status;
}

struct gladGLversionStruct GLVersion = { 0, 0 };

#if defined(GL_ES_VERSION_3_0) || defined(GL_VERSION_3_0)
#define _GLAD_IS_SOME_NEW_VERSION 1
#endif

static int max_loaded_major;
static int max_loaded_minor;

static const char *exts = NULL;
static int num_exts_i = 0;
static char **exts_i = NULL;

static int get_exts(void) {
#ifdef _GLAD_IS_SOME_NEW_VERSION
    if (max_loaded_major < 3) {
#endif
        exts = (const char *)glGetString(GL_EXTENSIONS);
#ifdef _GLAD_IS_SOME_NEW_VERSION
    }
    else {
        unsigned int index;

        num_exts_i = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &num_exts_i);
        if (num_exts_i > 0) {
            exts_i = (char **)malloc((size_t)num_exts_i * (sizeof * exts_i));
        }

        if (exts_i == NULL) {
            return 0;
        }

        for (index = 0; index < (unsigned)num_exts_i; index++) {
            const char *gl_str_tmp = (const char *)glGetStringi(GL_EXTENSIONS, index);
            size_t len = strlen(gl_str_tmp);

            char *local_str = (char *)malloc((len + 1) * sizeof(char));
            if (local_str != NULL) {
                memcpy(local_str, gl_str_tmp, (len + 1) * sizeof(char));
            }
            exts_i[index] = local_str;
        }
    }
#endif
    return 1;
}

static void free_exts(void) {
    if (exts_i != NULL) {
        int index;
        for (index = 0; index < num_exts_i; index++) {
            free((char *)exts_i[index]);
        }
        free((void *)exts_i);
        exts_i = NULL;
    }
}

static int has_ext(const char *ext) {
#ifdef _GLAD_IS_SOME_NEW_VERSION
    if (max_loaded_major < 3) {
#endif
        const char *extensions;
        const char *loc;
        const char *terminator;
        extensions = exts;
        if (extensions == NULL || ext == NULL) {
            return 0;
        }

        while (1) {
            loc = strstr(extensions, ext);
            if (loc == NULL) {
                return 0;
            }

            terminator = loc + strlen(ext);
            if ((loc == extensions || *(loc - 1) == ' ') &&
                (*terminator == ' ' || *terminator == '\0')) {
                return 1;
            }
            extensions = terminator;
        }
#ifdef _GLAD_IS_SOME_NEW_VERSION
    }
    else {
        int index;
        if (exts_i == NULL) return 0;
        for (index = 0; index < num_exts_i; index++) {
            const char *e = exts_i[index];

            if (exts_i[index] != NULL && strcmp(e, ext) == 0) {
                return 1;
            }
        }
    }
#endif

    return 0;
}
int GLAD_GL_VERSION_1_0 = 0;
int GLAD_GL_VERSION_1_1 = 0;
int GLAD_GL_VERSION_1_2 = 0;
int GLAD_GL_VERSION_1_3 = 0;
int GLAD_GL_VERSION_1_4 = 0;
int GLAD_GL_VERSION_1_5 = 0;
int GLAD_GL_VERSION_2_0 = 0;
int GLAD_GL_VERSION_2_1 = 0;
int GLAD_GL_VERSION_3_0 = 0;
int GLAD_GL_VERSION_3_1 = 0;
int GLAD_GL_VERSION_3_2 = 0;
int GLAD_GL_VERSION_3_3 = 0;
int GLAD_GL_VERSION_4_0 = 0;
int GLAD_GL_VERSION_4_1 = 0;
int GLAD_GL_VERSION_4_2 = 0;
int GLAD_GL_VERSION_4_3 = 0;
int GLAD_GL_VERSION_4_4 = 0;
int GLAD_GL_VERSION_4_5 = 0;
PFNGLACCUMPROC glad_glAccum = NULL;
PFNGLACTIVESHADERPROGRAMPROC glad_glActiveShaderProgram = NULL;
PFNGLACTIVETEXTUREPROC glad_glActiveTexture = NULL;
PFNGLALPHAFUNCPROC glad_glAlphaFunc = NULL;
PFNGLARETEXTURESRESIDENTPROC glad_glAreTexturesResident = NULL;
PFNGLARRAYELEMENTPROC glad_glArrayElement = NULL;
PFNGLATTACHSHADERPROC glad_glAttachShader = NULL;
PFNGLBEGINPROC glad_glBegin = NULL;
PFNGLBEGINCONDITIONALRENDERPROC glad_glBeginConditionalRender = NULL;
PFNGLBEGINQUERYPROC glad_glBeginQuery = NULL;
PFNGLBEGINQUERYINDEXEDPROC glad_glBeginQueryIndexed = NULL;
PFNGLBEGINTRANSFORMFEEDBACKPROC glad_glBeginTransformFeedback = NULL;
PFNGLBINDATTRIBLOCATIONPROC glad_glBindAttribLocation = NULL;
PFNGLBINDBUFFERPROC glad_glBindBuffer = NULL;
PFNGLBINDBUFFERBASEPROC glad_glBindBufferBase = NULL;
PFNGLBINDBUFFERRANGEPROC glad_glBindBufferRange = NULL;
PFNGLBINDBUFFERSBASEPROC glad_glBindBuffersBase = NULL;
PFNGLBINDBUFFERSRANGEPROC glad_glBindBuffersRange = NULL;
PFNGLBINDFRAGDATALOCATIONPROC glad_glBindFragDataLocation = NULL;
PFNGLBINDFRAGDATALOCATIONINDEXEDPROC glad_glBindFragDataLocationIndexed = NULL;
PFNGLBINDFRAMEBUFFERPROC glad_glBindFramebuffer = NULL;
PFNGLBINDIMAGETEXTUREPROC glad_glBindImageTexture = NULL;
PFNGLBINDIMAGETEXTURESPROC glad_glBindImageTextures = NULL;
PFNGLBINDPROGRAMPIPELINEPROC glad_glBindProgramPipeline = NULL;
PFNGLBINDRENDERBUFFERPROC glad_glBindRenderbuffer = NULL;
PFNGLBINDSAMPLERPROC glad_glBindSampler = NULL;
PFNGLBINDSAMPLERSPROC glad_glBindSamplers = NULL;
PFNGLBINDTEXTUREPROC glad_glBindTexture = NULL;
PFNGLBINDTEXTUREUNITPROC glad_glBindTextureUnit = NULL;
PFNGLBINDTEXTURESPROC glad_glBindTextures = NULL;
PFNGLBINDTRANSFORMFEEDBACKPROC glad_glBindTransformFeedback = NULL;
PFNGLBINDVERTEXARRAYPROC glad_glBindVertexArray = NULL;
PFNGLBINDVERTEXBUFFERPROC glad_glBindVertexBuffer = NULL;
PFNGLBINDVERTEXBUFFERSPROC glad_glBindVertexBuffers = NULL;
PFNGLBITMAPPROC glad_glBitmap = NULL;
PFNGLBLENDCOLORPROC glad_glBlendColor = NULL;
PFNGLBLENDEQUATIONPROC glad_glBlendEquation = NULL;
PFNGLBLENDEQUATIONSEPARATEPROC glad_glBlendEquationSeparate = NULL;
PFNGLBLENDEQUATIONSEPARATEIPROC glad_glBlendEquationSeparatei = NULL;
PFNGLBLENDEQUATIONIPROC glad_glBlendEquationi = NULL;
PFNGLBLENDFUNCPROC glad_glBlendFunc = NULL;
PFNGLBLENDFUNCSEPARATEPROC glad_glBlendFuncSeparate = NULL;
PFNGLBLENDFUNCSEPARATEIPROC glad_glBlendFuncSeparatei = NULL;
PFNGLBLENDFUNCIPROC glad_glBlendFunci = NULL;
PFNGLBLITFRAMEBUFFERPROC glad_glBlitFramebuffer = NULL;
PFNGLBLITNAMEDFRAMEBUFFERPROC glad_glBlitNamedFramebuffer = NULL;
PFNGLBUFFERDATAPROC glad_glBufferData = NULL;
PFNGLBUFFERSTORAGEPROC glad_glBufferStorage = NULL;
PFNGLBUFFERSUBDATAPROC glad_glBufferSubData = NULL;
PFNGLCALLLISTPROC glad_glCallList = NULL;
PFNGLCALLLISTSPROC glad_glCallLists = NULL;
PFNGLCHECKFRAMEBUFFERSTATUSPROC glad_glCheckFramebufferStatus = NULL;
PFNGLCHECKNAMEDFRAMEBUFFERSTATUSPROC glad_glCheckNamedFramebufferStatus = NULL;
PFNGLCLAMPCOLORPROC glad_glClampColor = NULL;
PFNGLCLEARPROC glad_glClear = NULL;
PFNGLCLEARACCUMPROC glad_glClearAccum = NULL;
PFNGLCLEARBUFFERDATAPROC glad_glClearBufferData = NULL;
PFNGLCLEARBUFFERSUBDATAPROC glad_glClearBufferSubData = NULL;
PFNGLCLEARBUFFERFIPROC glad_glClearBufferfi = NULL;
PFNGLCLEARBUFFERFVPROC glad_glClearBufferfv = NULL;
PFNGLCLEARBUFFERIVPROC glad_glClearBufferiv = NULL;
PFNGLCLEARBUFFERUIVPROC glad_glClearBufferuiv = NULL;
PFNGLCLEARCOLORPROC glad_glClearColor = NULL;
PFNGLCLEARDEPTHPROC glad_glClearDepth = NULL;
PFNGLCLEARDEPTHFPROC glad_glClearDepthf = NULL;
PFNGLCLEARINDEXPROC glad_glClearIndex = NULL;
PFNGLCLEARNAMEDBUFFERDATAPROC glad_glClearNamedBufferData = NULL;
PFNGLCLEARNAMEDBUFFERSUBDATAPROC glad_glClearNamedBufferSubData = NULL;
PFNGLCLEARNAMEDFRAMEBUFFERFIPROC glad_glClearNamedFramebufferfi = NULL;
PFNGLCLEARNAMEDFRAMEBUFFERFVPROC glad_glClearNamedFramebufferfv = NULL;
PFNGLCLEARNAMEDFRAMEBUFFERIVPROC glad_glClearNamedFramebufferiv = NULL;
PFNGLCLEARNAMEDFRAMEBUFFERUIVPROC glad_glClearNamedFramebufferuiv = NULL;
PFNGLCLEARSTENCILPROC glad_glClearStencil = NULL;
PFNGLCLEARTEXIMAGEPROC glad_glClearTexImage = NULL;
PFNGLCLEARTEXSUBIMAGEPROC glad_glClearTexSubImage = NULL;
PFNGLCLIENTACTIVETEXTUREPROC glad_glClientActiveTexture = NULL;
PFNGLCLIENTWAITSYNCPROC glad_glClientWaitSync = NULL;
PFNGLCLIPCONTROLPROC glad_glClipControl = NULL;
PFNGLCLIPPLANEPROC glad_glClipPlane = NULL;
PFNGLCOLOR3BPROC glad_glColor3b = NULL;
PFNGLCOLOR3BVPROC glad_glColor3bv = NULL;
PFNGLCOLOR3DPROC glad_glColor3d = NULL;
PFNGLCOLOR3DVPROC glad_glColor3dv = NULL;
PFNGLCOLOR3FPROC glad_glColor3f = NULL;
PFNGLCOLOR3FVPROC glad_glColor3fv = NULL;
PFNGLCOLOR3IPROC glad_glColor3i = NULL;
PFNGLCOLOR3IVPROC glad_glColor3iv = NULL;
PFNGLCOLOR3SPROC glad_glColor3s = NULL;
PFNGLCOLOR3SVPROC glad_glColor3sv = NULL;
PFNGLCOLOR3UBPROC glad_glColor3ub = NULL;
PFNGLCOLOR3UBVPROC glad_glColor3ubv = NULL;
PFNGLCOLOR3UIPROC glad_glColor3ui = NULL;
PFNGLCOLOR3UIVPROC glad_glColor3uiv = NULL;
PFNGLCOLOR3USPROC glad_glColor3us = NULL;
PFNGLCOLOR3USVPROC glad_glColor3usv = NULL;
PFNGLCOLOR4BPROC glad_glColor4b = NULL;
PFNGLCOLOR4BVPROC glad_glColor4bv = NULL;
PFNGLCOLOR4DPROC glad_glColor4d = NULL;
PFNGLCOLOR4DVPROC glad_glColor4dv = NULL;
PFNGLCOLOR4FPROC glad_glColor4f = NULL;
PFNGLCOLOR4FVPROC glad_glColor4fv = NULL;
PFNGLCOLOR4IPROC glad_glColor4i = NULL;
PFNGLCOLOR4IVPROC glad_glColor4iv = NULL;
PFNGLCOLOR4SPROC glad_glColor4s = NULL;
PFNGLCOLOR4SVPROC glad_glColor4sv = NULL;
PFNGLCOLOR4UBPROC glad_glColor4ub = NULL;
PFNGLCOLOR4UBVPROC glad_glColor4ubv = NULL;
PFNGLCOLOR4UIPROC glad_glColor4ui = NULL;
PFNGLCOLOR4UIVPROC glad_glColor4uiv = NULL;
PFNGLCOLOR4USPROC glad_glColor4us = NULL;
PFNGLCOLOR4USVPROC glad_glColor4usv = NULL;
PFNGLCOLORMASKPROC glad_glColorMask = NULL;
PFNGLCOLORMASKIPROC glad_glColorMaski = NULL;
PFNGLCOLORMATERIALPROC glad_glColorMaterial = NULL;
PFNGLCOLORP3UIPROC glad_glColorP3ui = NULL;
PFNGLCOLORP3UIVPROC glad_glColorP3uiv = NULL;
PFNGLCOLORP4UIPROC glad_glColorP4ui = NULL;
PFNGLCOLORP4UIVPROC glad_glColorP4uiv = NULL;
PFNGLCOLORPOINTERPROC glad_glColorPointer = NULL;
PFNGLCOMPILESHADERPROC glad_glCompileShader = NULL;
PFNGLCOMPRESSEDTEXIMAGE1DPROC glad_glCompressedTexImage1D = NULL;
PFNGLCOMPRESSEDTEXIMAGE2DPROC glad_glCompressedTexImage2D = NULL;
PFNGLCOMPRESSEDTEXIMAGE3DPROC glad_glCompressedTexImage3D = NULL;
PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC glad_glCompressedTexSubImage1D = NULL;
PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC glad_glCompressedTexSubImage2D = NULL;
PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC glad_glCompressedTexSubImage3D = NULL;
PFNGLCOMPRESSEDTEXTURESUBIMAGE1DPROC glad_glCompressedTextureSubImage1D = NULL;
PFNGLCOMPRESSEDTEXTURESUBIMAGE2DPROC glad_glCompressedTextureSubImage2D = NULL;
PFNGLCOMPRESSEDTEXTURESUBIMAGE3DPROC glad_glCompressedTextureSubImage3D = NULL;
PFNGLCOPYBUFFERSUBDATAPROC glad_glCopyBufferSubData = NULL;
PFNGLCOPYIMAGESUBDATAPROC glad_glCopyImageSubData = NULL;
PFNGLCOPYNAMEDBUFFERSUBDATAPROC glad_glCopyNamedBufferSubData = NULL;
PFNGLCOPYPIXELSPROC glad_glCopyPixels = NULL;
PFNGLCOPYTEXIMAGE1DPROC glad_glCopyTexImage1D = NULL;
PFNGLCOPYTEXIMAGE2DPROC glad_glCopyTexImage2D = NULL;
PFNGLCOPYTEXSUBIMAGE1DPROC glad_glCopyTexSubImage1D = NULL;
PFNGLCOPYTEXSUBIMAGE2DPROC glad_glCopyTexSubImage2D = NULL;
PFNGLCOPYTEXSUBIMAGE3DPROC glad_glCopyTexSubImage3D = NULL;
PFNGLCOPYTEXTURESUBIMAGE1DPROC glad_glCopyTextureSubImage1D = NULL;
PFNGLCOPYTEXTURESUBIMAGE2DPROC glad_glCopyTextureSubImage2D = NULL;
PFNGLCOPYTEXTURESUBIMAGE3DPROC glad_glCopyTextureSubImage3D = NULL;
PFNGLCREATEBUFFERSPROC glad_glCreateBuffers = NULL;
PFNGLCREATEFRAMEBUFFERSPROC glad_glCreateFramebuffers = NULL;
PFNGLCREATEPROGRAMPROC glad_glCreateProgram = NULL;
PFNGLCREATEPROGRAMPIPELINESPROC glad_glCreateProgramPipelines = NULL;
PFNGLCREATEQUERIESPROC glad_glCreateQueries = NULL;
PFNGLCREATERENDERBUFFERSPROC glad_glCreateRenderbuffers = NULL;
PFNGLCREATESAMPLERSPROC glad_glCreateSamplers = NULL;
PFNGLCREATESHADERPROC glad_glCreateShader = NULL;
PFNGLCREATESHADERPROGRAMVPROC glad_glCreateShaderProgramv = NULL;
PFNGLCREATETEXTURESPROC glad_glCreateTextures = NULL;
PFNGLCREATETRANSFORMFEEDBACKSPROC glad_glCreateTransformFeedbacks = NULL;
PFNGLCREATEVERTEXARRAYSPROC glad_glCreateVertexArrays = NULL;
PFNGLCULLFACEPROC glad_glCullFace = NULL;
PFNGLDEBUGMESSAGECALLBACKPROC glad_glDebugMessageCallback = NULL;
PFNGLDEBUGMESSAGECONTROLPROC glad_glDebugMessageControl = NULL;
PFNGLDEBUGMESSAGEINSERTPROC glad_glDebugMessageInsert = NULL;
PFNGLDELETEBUFFERSPROC glad_glDeleteBuffers = NULL;
PFNGLDELETEFRAMEBUFFERSPROC glad_glDeleteFramebuffers = NULL;
PFNGLDELETELISTSPROC glad_glDeleteLists = NULL;
PFNGLDELETEPROGRAMPROC glad_glDeleteProgram = NULL;
PFNGLDELETEPROGRAMPIPELINESPROC glad_glDeleteProgramPipelines = NULL;
PFNGLDELETEQUERIESPROC glad_glDeleteQueries = NULL;
PFNGLDELETERENDERBUFFERSPROC glad_glDeleteRenderbuffers = NULL;
PFNGLDELETESAMPLERSPROC glad_glDeleteSamplers = NULL;
PFNGLDELETESHADERPROC glad_glDeleteShader = NULL;
PFNGLDELETESYNCPROC glad_glDeleteSync = NULL;
PFNGLDELETETEXTURESPROC glad_glDeleteTextures = NULL;
PFNGLDELETETRANSFORMFEEDBACKSPROC glad_glDeleteTransformFeedbacks = NULL;
PFNGLDELETEVERTEXARRAYSPROC glad_glDeleteVertexArrays = NULL;
PFNGLDEPTHFUNCPROC glad_glDepthFunc = NULL;
PFNGLDEPTHMASKPROC glad_glDepthMask = NULL;
PFNGLDEPTHRANGEPROC glad_glDepthRange = NULL;
PFNGLDEPTHRANGEARRAYVPROC glad_glDepthRangeArrayv = NULL;
PFNGLDEPTHRANGEINDEXEDPROC glad_glDepthRangeIndexed = NULL;
PFNGLDEPTHRANGEFPROC glad_glDepthRangef = NULL;
PFNGLDETACHSHADERPROC glad_glDetachShader = NULL;
PFNGLDISABLEPROC glad_glDisable = NULL;
PFNGLDISABLECLIENTSTATEPROC glad_glDisableClientState = NULL;
PFNGLDISABLEVERTEXARRAYATTRIBPROC glad_glDisableVertexArrayAttrib = NULL;
PFNGLDISABLEVERTEXATTRIBARRAYPROC glad_glDisableVertexAttribArray = NULL;
PFNGLDISABLEIPROC glad_glDisablei = NULL;
PFNGLDISPATCHCOMPUTEPROC glad_glDispatchCompute = NULL;
PFNGLDISPATCHCOMPUTEINDIRECTPROC glad_glDispatchComputeIndirect = NULL;
PFNGLDRAWARRAYSPROC glad_glDrawArrays = NULL;
PFNGLDRAWARRAYSINDIRECTPROC glad_glDrawArraysIndirect = NULL;
PFNGLDRAWARRAYSINSTANCEDPROC glad_glDrawArraysInstanced = NULL;
PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEPROC glad_glDrawArraysInstancedBaseInstance = NULL;
PFNGLDRAWBUFFERPROC glad_glDrawBuffer = NULL;
PFNGLDRAWBUFFERSPROC glad_glDrawBuffers = NULL;
PFNGLDRAWELEMENTSPROC glad_glDrawElements = NULL;
PFNGLDRAWELEMENTSBASEVERTEXPROC glad_glDrawElementsBaseVertex = NULL;
PFNGLDRAWELEMENTSINDIRECTPROC glad_glDrawElementsIndirect = NULL;
PFNGLDRAWELEMENTSINSTANCEDPROC glad_glDrawElementsInstanced = NULL;
PFNGLDRAWELEMENTSINSTANCEDBASEINSTANCEPROC glad_glDrawElementsInstancedBaseInstance = NULL;
PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC glad_glDrawElementsInstancedBaseVertex = NULL;
PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEPROC glad_glDrawElementsInstancedBaseVertexBaseInstance = NULL;
PFNGLDRAWPIXELSPROC glad_glDrawPixels = NULL;
PFNGLDRAWRANGEELEMENTSPROC glad_glDrawRangeElements = NULL;
PFNGLDRAWRANGEELEMENTSBASEVERTEXPROC glad_glDrawRangeElementsBaseVertex = NULL;
PFNGLDRAWTRANSFORMFEEDBACKPROC glad_glDrawTransformFeedback = NULL;
PFNGLDRAWTRANSFORMFEEDBACKINSTANCEDPROC glad_glDrawTransformFeedbackInstanced = NULL;
PFNGLDRAWTRANSFORMFEEDBACKSTREAMPROC glad_glDrawTransformFeedbackStream = NULL;
PFNGLDRAWTRANSFORMFEEDBACKSTREAMINSTANCEDPROC glad_glDrawTransformFeedbackStreamInstanced = NULL;
PFNGLEDGEFLAGPROC glad_glEdgeFlag = NULL;
PFNGLEDGEFLAGPOINTERPROC glad_glEdgeFlagPointer = NULL;
PFNGLEDGEFLAGVPROC glad_glEdgeFlagv = NULL;
PFNGLENABLEPROC glad_glEnable = NULL;
PFNGLENABLECLIENTSTATEPROC glad_glEnableClientState = NULL;
PFNGLENABLEVERTEXARRAYATTRIBPROC glad_glEnableVertexArrayAttrib = NULL;
PFNGLENABLEVERTEXATTRIBARRAYPROC glad_glEnableVertexAttribArray = NULL;
PFNGLENABLEIPROC glad_glEnablei = NULL;
PFNGLENDPROC glad_glEnd = NULL;
PFNGLENDCONDITIONALRENDERPROC glad_glEndConditionalRender = NULL;
PFNGLENDLISTPROC glad_glEndList = NULL;
PFNGLENDQUERYPROC glad_glEndQuery = NULL;
PFNGLENDQUERYINDEXEDPROC glad_glEndQueryIndexed = NULL;
PFNGLENDTRANSFORMFEEDBACKPROC glad_glEndTransformFeedback = NULL;
PFNGLEVALCOORD1DPROC glad_glEvalCoord1d = NULL;
PFNGLEVALCOORD1DVPROC glad_glEvalCoord1dv = NULL;
PFNGLEVALCOORD1FPROC glad_glEvalCoord1f = NULL;
PFNGLEVALCOORD1FVPROC glad_glEvalCoord1fv = NULL;
PFNGLEVALCOORD2DPROC glad_glEvalCoord2d = NULL;
PFNGLEVALCOORD2DVPROC glad_glEvalCoord2dv = NULL;
PFNGLEVALCOORD2FPROC glad_glEvalCoord2f = NULL;
PFNGLEVALCOORD2FVPROC glad_glEvalCoord2fv = NULL;
PFNGLEVALMESH1PROC glad_glEvalMesh1 = NULL;
PFNGLEVALMESH2PROC glad_glEvalMesh2 = NULL;
PFNGLEVALPOINT1PROC glad_glEvalPoint1 = NULL;
PFNGLEVALPOINT2PROC glad_glEvalPoint2 = NULL;
PFNGLFEEDBACKBUFFERPROC glad_glFeedbackBuffer = NULL;
PFNGLFENCESYNCPROC glad_glFenceSync = NULL;
PFNGLFINISHPROC glad_glFinish = NULL;
PFNGLFLUSHPROC glad_glFlush = NULL;
PFNGLFLUSHMAPPEDBUFFERRANGEPROC glad_glFlushMappedBufferRange = NULL;
PFNGLFLUSHMAPPEDNAMEDBUFFERRANGEPROC glad_glFlushMappedNamedBufferRange = NULL;
PFNGLFOGCOORDPOINTERPROC glad_glFogCoordPointer = NULL;
PFNGLFOGCOORDDPROC glad_glFogCoordd = NULL;
PFNGLFOGCOORDDVPROC glad_glFogCoorddv = NULL;
PFNGLFOGCOORDFPROC glad_glFogCoordf = NULL;
PFNGLFOGCOORDFVPROC glad_glFogCoordfv = NULL;
PFNGLFOGFPROC glad_glFogf = NULL;
PFNGLFOGFVPROC glad_glFogfv = NULL;
PFNGLFOGIPROC glad_glFogi = NULL;
PFNGLFOGIVPROC glad_glFogiv = NULL;
PFNGLFRAMEBUFFERPARAMETERIPROC glad_glFramebufferParameteri = NULL;
PFNGLFRAMEBUFFERRENDERBUFFERPROC glad_glFramebufferRenderbuffer = NULL;
PFNGLFRAMEBUFFERTEXTUREPROC glad_glFramebufferTexture = NULL;
PFNGLFRAMEBUFFERTEXTURE1DPROC glad_glFramebufferTexture1D = NULL;
PFNGLFRAMEBUFFERTEXTURE2DPROC glad_glFramebufferTexture2D = NULL;
PFNGLFRAMEBUFFERTEXTURE3DPROC glad_glFramebufferTexture3D = NULL;
PFNGLFRAMEBUFFERTEXTURELAYERPROC glad_glFramebufferTextureLayer = NULL;
PFNGLFRONTFACEPROC glad_glFrontFace = NULL;
PFNGLFRUSTUMPROC glad_glFrustum = NULL;
PFNGLGENBUFFERSPROC glad_glGenBuffers = NULL;
PFNGLGENFRAMEBUFFERSPROC glad_glGenFramebuffers = NULL;
PFNGLGENLISTSPROC glad_glGenLists = NULL;
PFNGLGENPROGRAMPIPELINESPROC glad_glGenProgramPipelines = NULL;
PFNGLGENQUERIESPROC glad_glGenQueries = NULL;
PFNGLGENRENDERBUFFERSPROC glad_glGenRenderbuffers = NULL;
PFNGLGENSAMPLERSPROC glad_glGenSamplers = NULL;
PFNGLGENTEXTURESPROC glad_glGenTextures = NULL;
PFNGLGENTRANSFORMFEEDBACKSPROC glad_glGenTransformFeedbacks = NULL;
PFNGLGENVERTEXARRAYSPROC glad_glGenVertexArrays = NULL;
PFNGLGENERATEMIPMAPPROC glad_glGenerateMipmap = NULL;
PFNGLGENERATETEXTUREMIPMAPPROC glad_glGenerateTextureMipmap = NULL;
PFNGLGETACTIVEATOMICCOUNTERBUFFERIVPROC glad_glGetActiveAtomicCounterBufferiv = NULL;
PFNGLGETACTIVEATTRIBPROC glad_glGetActiveAttrib = NULL;
PFNGLGETACTIVESUBROUTINENAMEPROC glad_glGetActiveSubroutineName = NULL;
PFNGLGETACTIVESUBROUTINEUNIFORMNAMEPROC glad_glGetActiveSubroutineUniformName = NULL;
PFNGLGETACTIVESUBROUTINEUNIFORMIVPROC glad_glGetActiveSubroutineUniformiv = NULL;
PFNGLGETACTIVEUNIFORMPROC glad_glGetActiveUniform = NULL;
PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC glad_glGetActiveUniformBlockName = NULL;
PFNGLGETACTIVEUNIFORMBLOCKIVPROC glad_glGetActiveUniformBlockiv = NULL;
PFNGLGETACTIVEUNIFORMNAMEPROC glad_glGetActiveUniformName = NULL;
PFNGLGETACTIVEUNIFORMSIVPROC glad_glGetActiveUniformsiv = NULL;
PFNGLGETATTACHEDSHADERSPROC glad_glGetAttachedShaders = NULL;
PFNGLGETATTRIBLOCATIONPROC glad_glGetAttribLocation = NULL;
PFNGLGETBOOLEANI_VPROC glad_glGetBooleani_v = NULL;
PFNGLGETBOOLEANVPROC glad_glGetBooleanv = NULL;
PFNGLGETBUFFERPARAMETERI64VPROC glad_glGetBufferParameteri64v = NULL;
PFNGLGETBUFFERPARAMETERIVPROC glad_glGetBufferParameteriv = NULL;
PFNGLGETBUFFERPOINTERVPROC glad_glGetBufferPointerv = NULL;
PFNGLGETBUFFERSUBDATAPROC glad_glGetBufferSubData = NULL;
PFNGLGETCLIPPLANEPROC glad_glGetClipPlane = NULL;
PFNGLGETCOMPRESSEDTEXIMAGEPROC glad_glGetCompressedTexImage = NULL;
PFNGLGETCOMPRESSEDTEXTUREIMAGEPROC glad_glGetCompressedTextureImage = NULL;
PFNGLGETCOMPRESSEDTEXTURESUBIMAGEPROC glad_glGetCompressedTextureSubImage = NULL;
PFNGLGETDEBUGMESSAGELOGPROC glad_glGetDebugMessageLog = NULL;
PFNGLGETDOUBLEI_VPROC glad_glGetDoublei_v = NULL;
PFNGLGETDOUBLEVPROC glad_glGetDoublev = NULL;
PFNGLGETERRORPROC glad_glGetError = NULL;
PFNGLGETFLOATI_VPROC glad_glGetFloati_v = NULL;
PFNGLGETFLOATVPROC glad_glGetFloatv = NULL;
PFNGLGETFRAGDATAINDEXPROC glad_glGetFragDataIndex = NULL;
PFNGLGETFRAGDATALOCATIONPROC glad_glGetFragDataLocation = NULL;
PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC glad_glGetFramebufferAttachmentParameteriv = NULL;
PFNGLGETFRAMEBUFFERPARAMETERIVPROC glad_glGetFramebufferParameteriv = NULL;
PFNGLGETGRAPHICSRESETSTATUSPROC glad_glGetGraphicsResetStatus = NULL;
PFNGLGETINTEGER64I_VPROC glad_glGetInteger64i_v = NULL;
PFNGLGETINTEGER64VPROC glad_glGetInteger64v = NULL;
PFNGLGETINTEGERI_VPROC glad_glGetIntegeri_v = NULL;
PFNGLGETINTEGERVPROC glad_glGetIntegerv = NULL;
PFNGLGETINTERNALFORMATI64VPROC glad_glGetInternalformati64v = NULL;
PFNGLGETINTERNALFORMATIVPROC glad_glGetInternalformativ = NULL;
PFNGLGETLIGHTFVPROC glad_glGetLightfv = NULL;
PFNGLGETLIGHTIVPROC glad_glGetLightiv = NULL;
PFNGLGETMAPDVPROC glad_glGetMapdv = NULL;
PFNGLGETMAPFVPROC glad_glGetMapfv = NULL;
PFNGLGETMAPIVPROC glad_glGetMapiv = NULL;
PFNGLGETMATERIALFVPROC glad_glGetMaterialfv = NULL;
PFNGLGETMATERIALIVPROC glad_glGetMaterialiv = NULL;
PFNGLGETMULTISAMPLEFVPROC glad_glGetMultisamplefv = NULL;
PFNGLGETNAMEDBUFFERPARAMETERI64VPROC glad_glGetNamedBufferParameteri64v = NULL;
PFNGLGETNAMEDBUFFERPARAMETERIVPROC glad_glGetNamedBufferParameteriv = NULL;
PFNGLGETNAMEDBUFFERPOINTERVPROC glad_glGetNamedBufferPointerv = NULL;
PFNGLGETNAMEDBUFFERSUBDATAPROC glad_glGetNamedBufferSubData = NULL;
PFNGLGETNAMEDFRAMEBUFFERATTACHMENTPARAMETERIVPROC glad_glGetNamedFramebufferAttachmentParameteriv = NULL;
PFNGLGETNAMEDFRAMEBUFFERPARAMETERIVPROC glad_glGetNamedFramebufferParameteriv = NULL;
PFNGLGETNAMEDRENDERBUFFERPARAMETERIVPROC glad_glGetNamedRenderbufferParameteriv = NULL;
PFNGLGETOBJECTLABELPROC glad_glGetObjectLabel = NULL;
PFNGLGETOBJECTPTRLABELPROC glad_glGetObjectPtrLabel = NULL;
PFNGLGETPIXELMAPFVPROC glad_glGetPixelMapfv = NULL;
PFNGLGETPIXELMAPUIVPROC glad_glGetPixelMapuiv = NULL;
PFNGLGETPIXELMAPUSVPROC glad_glGetPixelMapusv = NULL;
PFNGLGETPOINTERVPROC glad_glGetPointerv = NULL;
PFNGLGETPOLYGONSTIPPLEPROC glad_glGetPolygonStipple = NULL;
PFNGLGETPROGRAMBINARYPROC glad_glGetProgramBinary = NULL;
PFNGLGETPROGRAMINFOLOGPROC glad_glGetProgramInfoLog = NULL;
PFNGLGETPROGRAMINTERFACEIVPROC glad_glGetProgramInterfaceiv = NULL;
PFNGLGETPROGRAMPIPELINEINFOLOGPROC glad_glGetProgramPipelineInfoLog = NULL;
PFNGLGETPROGRAMPIPELINEIVPROC glad_glGetProgramPipelineiv = NULL;
PFNGLGETPROGRAMRESOURCEINDEXPROC glad_glGetProgramResourceIndex = NULL;
PFNGLGETPROGRAMRESOURCELOCATIONPROC glad_glGetProgramResourceLocation = NULL;
PFNGLGETPROGRAMRESOURCELOCATIONINDEXPROC glad_glGetProgramResourceLocationIndex = NULL;
PFNGLGETPROGRAMRESOURCENAMEPROC glad_glGetProgramResourceName = NULL;
PFNGLGETPROGRAMRESOURCEIVPROC glad_glGetProgramResourceiv = NULL;
PFNGLGETPROGRAMSTAGEIVPROC glad_glGetProgramStageiv = NULL;
PFNGLGETPROGRAMIVPROC glad_glGetProgramiv = NULL;
PFNGLGETQUERYBUFFEROBJECTI64VPROC glad_glGetQueryBufferObjecti64v = NULL;
PFNGLGETQUERYBUFFEROBJECTIVPROC glad_glGetQueryBufferObjectiv = NULL;
PFNGLGETQUERYBUFFEROBJECTUI64VPROC glad_glGetQueryBufferObjectui64v = NULL;
PFNGLGETQUERYBUFFEROBJECTUIVPROC glad_glGetQueryBufferObjectuiv = NULL;
PFNGLGETQUERYINDEXEDIVPROC glad_glGetQueryIndexediv = NULL;
PFNGLGETQUERYOBJECTI64VPROC glad_glGetQueryObjecti64v = NULL;
PFNGLGETQUERYOBJECTIVPROC glad_glGetQueryObjectiv = NULL;
PFNGLGETQUERYOBJECTUI64VPROC glad_glGetQueryObjectui64v = NULL;
PFNGLGETQUERYOBJECTUIVPROC glad_glGetQueryObjectuiv = NULL;
PFNGLGETQUERYIVPROC glad_glGetQueryiv = NULL;
PFNGLGETRENDERBUFFERPARAMETERIVPROC glad_glGetRenderbufferParameteriv = NULL;
PFNGLGETSAMPLERPARAMETERIIVPROC glad_glGetSamplerParameterIiv = NULL;
PFNGLGETSAMPLERPARAMETERIUIVPROC glad_glGetSamplerParameterIuiv = NULL;
PFNGLGETSAMPLERPARAMETERFVPROC glad_glGetSamplerParameterfv = NULL;
PFNGLGETSAMPLERPARAMETERIVPROC glad_glGetSamplerParameteriv = NULL;
PFNGLGETSHADERINFOLOGPROC glad_glGetShaderInfoLog = NULL;
PFNGLGETSHADERPRECISIONFORMATPROC glad_glGetShaderPrecisionFormat = NULL;
PFNGLGETSHADERSOURCEPROC glad_glGetShaderSource = NULL;
PFNGLGETSHADERIVPROC glad_glGetShaderiv = NULL;
PFNGLGETSTRINGPROC glad_glGetString = NULL;
PFNGLGETSTRINGIPROC glad_glGetStringi = NULL;
PFNGLGETSUBROUTINEINDEXPROC glad_glGetSubroutineIndex = NULL;
PFNGLGETSUBROUTINEUNIFORMLOCATIONPROC glad_glGetSubroutineUniformLocation = NULL;
PFNGLGETSYNCIVPROC glad_glGetSynciv = NULL;
PFNGLGETTEXENVFVPROC glad_glGetTexEnvfv = NULL;
PFNGLGETTEXENVIVPROC glad_glGetTexEnviv = NULL;
PFNGLGETTEXGENDVPROC glad_glGetTexGendv = NULL;
PFNGLGETTEXGENFVPROC glad_glGetTexGenfv = NULL;
PFNGLGETTEXGENIVPROC glad_glGetTexGeniv = NULL;
PFNGLGETTEXIMAGEPROC glad_glGetTexImage = NULL;
PFNGLGETTEXLEVELPARAMETERFVPROC glad_glGetTexLevelParameterfv = NULL;
PFNGLGETTEXLEVELPARAMETERIVPROC glad_glGetTexLevelParameteriv = NULL;
PFNGLGETTEXPARAMETERIIVPROC glad_glGetTexParameterIiv = NULL;
PFNGLGETTEXPARAMETERIUIVPROC glad_glGetTexParameterIuiv = NULL;
PFNGLGETTEXPARAMETERFVPROC glad_glGetTexParameterfv = NULL;
PFNGLGETTEXPARAMETERIVPROC glad_glGetTexParameteriv = NULL;
PFNGLGETTEXTUREIMAGEPROC glad_glGetTextureImage = NULL;
PFNGLGETTEXTURELEVELPARAMETERFVPROC glad_glGetTextureLevelParameterfv = NULL;
PFNGLGETTEXTURELEVELPARAMETERIVPROC glad_glGetTextureLevelParameteriv = NULL;
PFNGLGETTEXTUREPARAMETERIIVPROC glad_glGetTextureParameterIiv = NULL;
PFNGLGETTEXTUREPARAMETERIUIVPROC glad_glGetTextureParameterIuiv = NULL;
PFNGLGETTEXTUREPARAMETERFVPROC glad_glGetTextureParameterfv = NULL;
PFNGLGETTEXTUREPARAMETERIVPROC glad_glGetTextureParameteriv = NULL;
PFNGLGETTEXTURESUBIMAGEPROC glad_glGetTextureSubImage = NULL;
PFNGLGETTRANSFORMFEEDBACKVARYINGPROC glad_glGetTransformFeedbackVarying = NULL;
PFNGLGETTRANSFORMFEEDBACKI64_VPROC glad_glGetTransformFeedbacki64_v = NULL;
PFNGLGETTRANSFORMFEEDBACKI_VPROC glad_glGetTransformFeedbacki_v = NULL;
PFNGLGETTRANSFORMFEEDBACKIVPROC glad_glGetTransformFeedbackiv = NULL;
PFNGLGETUNIFORMBLOCKINDEXPROC glad_glGetUniformBlockIndex = NULL;
PFNGLGETUNIFORMINDICESPROC glad_glGetUniformIndices = NULL;
PFNGLGETUNIFORMLOCATIONPROC glad_glGetUniformLocation = NULL;
PFNGLGETUNIFORMSUBROUTINEUIVPROC glad_glGetUniformSubroutineuiv = NULL;
PFNGLGETUNIFORMDVPROC glad_glGetUniformdv = NULL;
PFNGLGETUNIFORMFVPROC glad_glGetUniformfv = NULL;
PFNGLGETUNIFORMIVPROC glad_glGetUniformiv = NULL;
PFNGLGETUNIFORMUIVPROC glad_glGetUniformuiv = NULL;
PFNGLGETVERTEXARRAYINDEXED64IVPROC glad_glGetVertexArrayIndexed64iv = NULL;
PFNGLGETVERTEXARRAYINDEXEDIVPROC glad_glGetVertexArrayIndexediv = NULL;
PFNGLGETVERTEXARRAYIVPROC glad_glGetVertexArrayiv = NULL;
PFNGLGETVERTEXATTRIBIIVPROC glad_glGetVertexAttribIiv = NULL;
PFNGLGETVERTEXATTRIBIUIVPROC glad_glGetVertexAttribIuiv = NULL;
PFNGLGETVERTEXATTRIBLDVPROC glad_glGetVertexAttribLdv = NULL;
PFNGLGETVERTEXATTRIBPOINTERVPROC glad_glGetVertexAttribPointerv = NULL;
PFNGLGETVERTEXATTRIBDVPROC glad_glGetVertexAttribdv = NULL;
PFNGLGETVERTEXATTRIBFVPROC glad_glGetVertexAttribfv = NULL;
PFNGLGETVERTEXATTRIBIVPROC glad_glGetVertexAttribiv = NULL;
PFNGLGETNCOLORTABLEPROC glad_glGetnColorTable = NULL;
PFNGLGETNCOMPRESSEDTEXIMAGEPROC glad_glGetnCompressedTexImage = NULL;
PFNGLGETNCONVOLUTIONFILTERPROC glad_glGetnConvolutionFilter = NULL;
PFNGLGETNHISTOGRAMPROC glad_glGetnHistogram = NULL;
PFNGLGETNMAPDVPROC glad_glGetnMapdv = NULL;
PFNGLGETNMAPFVPROC glad_glGetnMapfv = NULL;
PFNGLGETNMAPIVPROC glad_glGetnMapiv = NULL;
PFNGLGETNMINMAXPROC glad_glGetnMinmax = NULL;
PFNGLGETNPIXELMAPFVPROC glad_glGetnPixelMapfv = NULL;
PFNGLGETNPIXELMAPUIVPROC glad_glGetnPixelMapuiv = NULL;
PFNGLGETNPIXELMAPUSVPROC glad_glGetnPixelMapusv = NULL;
PFNGLGETNPOLYGONSTIPPLEPROC glad_glGetnPolygonStipple = NULL;
PFNGLGETNSEPARABLEFILTERPROC glad_glGetnSeparableFilter = NULL;
PFNGLGETNTEXIMAGEPROC glad_glGetnTexImage = NULL;
PFNGLGETNUNIFORMDVPROC glad_glGetnUniformdv = NULL;
PFNGLGETNUNIFORMFVPROC glad_glGetnUniformfv = NULL;
PFNGLGETNUNIFORMIVPROC glad_glGetnUniformiv = NULL;
PFNGLGETNUNIFORMUIVPROC glad_glGetnUniformuiv = NULL;
PFNGLHINTPROC glad_glHint = NULL;
PFNGLINDEXMASKPROC glad_glIndexMask = NULL;
PFNGLINDEXPOINTERPROC glad_glIndexPointer = NULL;
PFNGLINDEXDPROC glad_glIndexd = NULL;
PFNGLINDEXDVPROC glad_glIndexdv = NULL;
PFNGLINDEXFPROC glad_glIndexf = NULL;
PFNGLINDEXFVPROC glad_glIndexfv = NULL;
PFNGLINDEXIPROC glad_glIndexi = NULL;
PFNGLINDEXIVPROC glad_glIndexiv = NULL;
PFNGLINDEXSPROC glad_glIndexs = NULL;
PFNGLINDEXSVPROC glad_glIndexsv = NULL;
PFNGLINDEXUBPROC glad_glIndexub = NULL;
PFNGLINDEXUBVPROC glad_glIndexubv = NULL;
PFNGLINITNAMESPROC glad_glInitNames = NULL;
PFNGLINTERLEAVEDARRAYSPROC glad_glInterleavedArrays = NULL;
PFNGLINVALIDATEBUFFERDATAPROC glad_glInvalidateBufferData = NULL;
PFNGLINVALIDATEBUFFERSUBDATAPROC glad_glInvalidateBufferSubData = NULL;
PFNGLINVALIDATEFRAMEBUFFERPROC glad_glInvalidateFramebuffer = NULL;
PFNGLINVALIDATENAMEDFRAMEBUFFERDATAPROC glad_glInvalidateNamedFramebufferData = NULL;
PFNGLINVALIDATENAMEDFRAMEBUFFERSUBDATAPROC glad_glInvalidateNamedFramebufferSubData = NULL;
PFNGLINVALIDATESUBFRAMEBUFFERPROC glad_glInvalidateSubFramebuffer = NULL;
PFNGLINVALIDATETEXIMAGEPROC glad_glInvalidateTexImage = NULL;
PFNGLINVALIDATETEXSUBIMAGEPROC glad_glInvalidateTexSubImage = NULL;
PFNGLISBUFFERPROC glad_glIsBuffer = NULL;
PFNGLISENABLEDPROC glad_glIsEnabled = NULL;
PFNGLISENABLEDIPROC glad_glIsEnabledi = NULL;
PFNGLISFRAMEBUFFERPROC glad_glIsFramebuffer = NULL;
PFNGLISLISTPROC glad_glIsList = NULL;
PFNGLISPROGRAMPROC glad_glIsProgram = NULL;
PFNGLISPROGRAMPIPELINEPROC glad_glIsProgramPipeline = NULL;
PFNGLISQUERYPROC glad_glIsQuery = NULL;
PFNGLISRENDERBUFFERPROC glad_glIsRenderbuffer = NULL;
PFNGLISSAMPLERPROC glad_glIsSampler = NULL;
PFNGLISSHADERPROC glad_glIsShader = NULL;
PFNGLISSYNCPROC glad_glIsSync = NULL;
PFNGLISTEXTUREPROC glad_glIsTexture = NULL;
PFNGLISTRANSFORMFEEDBACKPROC glad_glIsTransformFeedback = NULL;
PFNGLISVERTEXARRAYPROC glad_glIsVertexArray = NULL;
PFNGLLIGHTMODELFPROC glad_glLightModelf = NULL;
PFNGLLIGHTMODELFVPROC glad_glLightModelfv = NULL;
PFNGLLIGHTMODELIPROC glad_glLightModeli = NULL;
PFNGLLIGHTMODELIVPROC glad_glLightModeliv = NULL;
PFNGLLIGHTFPROC glad_glLightf = NULL;
PFNGLLIGHTFVPROC glad_glLightfv = NULL;
PFNGLLIGHTIPROC glad_glLighti = NULL;
PFNGLLIGHTIVPROC glad_glLightiv = NULL;
PFNGLLINESTIPPLEPROC glad_glLineStipple = NULL;
PFNGLLINEWIDTHPROC glad_glLineWidth = NULL;
PFNGLLINKPROGRAMPROC glad_glLinkProgram = NULL;
PFNGLLISTBASEPROC glad_glListBase = NULL;
PFNGLLOADIDENTITYPROC glad_glLoadIdentity = NULL;
PFNGLLOADMATRIXDPROC glad_glLoadMatrixd = NULL;
PFNGLLOADMATRIXFPROC glad_glLoadMatrixf = NULL;
PFNGLLOADNAMEPROC glad_glLoadName = NULL;
PFNGLLOADTRANSPOSEMATRIXDPROC glad_glLoadTransposeMatrixd = NULL;
PFNGLLOADTRANSPOSEMATRIXFPROC glad_glLoadTransposeMatrixf = NULL;
PFNGLLOGICOPPROC glad_glLogicOp = NULL;
PFNGLMAP1DPROC glad_glMap1d = NULL;
PFNGLMAP1FPROC glad_glMap1f = NULL;
PFNGLMAP2DPROC glad_glMap2d = NULL;
PFNGLMAP2FPROC glad_glMap2f = NULL;
PFNGLMAPBUFFERPROC glad_glMapBuffer = NULL;
PFNGLMAPBUFFERRANGEPROC glad_glMapBufferRange = NULL;
PFNGLMAPGRID1DPROC glad_glMapGrid1d = NULL;
PFNGLMAPGRID1FPROC glad_glMapGrid1f = NULL;
PFNGLMAPGRID2DPROC glad_glMapGrid2d = NULL;
PFNGLMAPGRID2FPROC glad_glMapGrid2f = NULL;
PFNGLMAPNAMEDBUFFERPROC glad_glMapNamedBuffer = NULL;
PFNGLMAPNAMEDBUFFERRANGEPROC glad_glMapNamedBufferRange = NULL;
PFNGLMATERIALFPROC glad_glMaterialf = NULL;
PFNGLMATERIALFVPROC glad_glMaterialfv = NULL;
PFNGLMATERIALIPROC glad_glMateriali = NULL;
PFNGLMATERIALIVPROC glad_glMaterialiv = NULL;
PFNGLMATRIXMODEPROC glad_glMatrixMode = NULL;
PFNGLMEMORYBARRIERPROC glad_glMemoryBarrier = NULL;
PFNGLMEMORYBARRIERBYREGIONPROC glad_glMemoryBarrierByRegion = NULL;
PFNGLMINSAMPLESHADINGPROC glad_glMinSampleShading = NULL;
PFNGLMULTMATRIXDPROC glad_glMultMatrixd = NULL;
PFNGLMULTMATRIXFPROC glad_glMultMatrixf = NULL;
PFNGLMULTTRANSPOSEMATRIXDPROC glad_glMultTransposeMatrixd = NULL;
PFNGLMULTTRANSPOSEMATRIXFPROC glad_glMultTransposeMatrixf = NULL;
PFNGLMULTIDRAWARRAYSPROC glad_glMultiDrawArrays = NULL;
PFNGLMULTIDRAWARRAYSINDIRECTPROC glad_glMultiDrawArraysIndirect = NULL;
PFNGLMULTIDRAWELEMENTSPROC glad_glMultiDrawElements = NULL;
PFNGLMULTIDRAWELEMENTSBASEVERTEXPROC glad_glMultiDrawElementsBaseVertex = NULL;
PFNGLMULTIDRAWELEMENTSINDIRECTPROC glad_glMultiDrawElementsIndirect = NULL;
PFNGLMULTITEXCOORD1DPROC glad_glMultiTexCoord1d = NULL;
PFNGLMULTITEXCOORD1DVPROC glad_glMultiTexCoord1dv = NULL;
PFNGLMULTITEXCOORD1FPROC glad_glMultiTexCoord1f = NULL;
PFNGLMULTITEXCOORD1FVPROC glad_glMultiTexCoord1fv = NULL;
PFNGLMULTITEXCOORD1IPROC glad_glMultiTexCoord1i = NULL;
PFNGLMULTITEXCOORD1IVPROC glad_glMultiTexCoord1iv = NULL;
PFNGLMULTITEXCOORD1SPROC glad_glMultiTexCoord1s = NULL;
PFNGLMULTITEXCOORD1SVPROC glad_glMultiTexCoord1sv = NULL;
PFNGLMULTITEXCOORD2DPROC glad_glMultiTexCoord2d = NULL;
PFNGLMULTITEXCOORD2DVPROC glad_glMultiTexCoord2dv = NULL;
PFNGLMULTITEXCOORD2FPROC glad_glMultiTexCoord2f = NULL;
PFNGLMULTITEXCOORD2FVPROC glad_glMultiTexCoord2fv = NULL;
PFNGLMULTITEXCOORD2IPROC glad_glMultiTexCoord2i = NULL;
PFNGLMULTITEXCOORD2IVPROC glad_glMultiTexCoord2iv = NULL;
PFNGLMULTITEXCOORD2SPROC glad_glMultiTexCoord2s = NULL;
PFNGLMULTITEXCOORD2SVPROC glad_glMultiTexCoord2sv = NULL;
PFNGLMULTITEXCOORD3DPROC glad_glMultiTexCoord3d = NULL;
PFNGLMULTITEXCOORD3DVPROC glad_glMultiTexCoord3dv = NULL;
PFNGLMULTITEXCOORD3FPROC glad_glMultiTexCoord3f = NULL;
PFNGLMULTITEXCOORD3FVPROC glad_glMultiTexCoord3fv = NULL;
PFNGLMULTITEXCOORD3IPROC glad_glMultiTexCoord3i = NULL;
PFNGLMULTITEXCOORD3IVPROC glad_glMultiTexCoord3iv = NULL;
PFNGLMULTITEXCOORD3SPROC glad_glMultiTexCoord3s = NULL;
PFNGLMULTITEXCOORD3SVPROC glad_glMultiTexCoord3sv = NULL;
PFNGLMULTITEXCOORD4DPROC glad_glMultiTexCoord4d = NULL;
PFNGLMULTITEXCOORD4DVPROC glad_glMultiTexCoord4dv = NULL;
PFNGLMULTITEXCOORD4FPROC glad_glMultiTexCoord4f = NULL;
PFNGLMULTITEXCOORD4FVPROC glad_glMultiTexCoord4fv = NULL;
PFNGLMULTITEXCOORD4IPROC glad_glMultiTexCoord4i = NULL;
PFNGLMULTITEXCOORD4IVPROC glad_glMultiTexCoord4iv = NULL;
PFNGLMULTITEXCOORD4SPROC glad_glMultiTexCoord4s = NULL;
PFNGLMULTITEXCOORD4SVPROC glad_glMultiTexCoord4sv = NULL;
PFNGLMULTITEXCOORDP1UIPROC glad_glMultiTexCoordP1ui = NULL;
PFNGLMULTITEXCOORDP1UIVPROC glad_glMultiTexCoordP1uiv = NULL;
PFNGLMULTITEXCOORDP2UIPROC glad_glMultiTexCoordP2ui = NULL;
PFNGLMULTITEXCOORDP2UIVPROC glad_glMultiTexCoordP2uiv = NULL;
PFNGLMULTITEXCOORDP3UIPROC glad_glMultiTexCoordP3ui = NULL;
PFNGLMULTITEXCOORDP3UIVPROC glad_glMultiTexCoordP3uiv = NULL;
PFNGLMULTITEXCOORDP4UIPROC glad_glMultiTexCoordP4ui = NULL;
PFNGLMULTITEXCOORDP4UIVPROC glad_glMultiTexCoordP4uiv = NULL;
PFNGLNAMEDBUFFERDATAPROC glad_glNamedBufferData = NULL;
PFNGLNAMEDBUFFERSTORAGEPROC glad_glNamedBufferStorage = NULL;
PFNGLNAMEDBUFFERSUBDATAPROC glad_glNamedBufferSubData = NULL;
PFNGLNAMEDFRAMEBUFFERDRAWBUFFERPROC glad_glNamedFramebufferDrawBuffer = NULL;
PFNGLNAMEDFRAMEBUFFERDRAWBUFFERSPROC glad_glNamedFramebufferDrawBuffers = NULL;
PFNGLNAMEDFRAMEBUFFERPARAMETERIPROC glad_glNamedFramebufferParameteri = NULL;
PFNGLNAMEDFRAMEBUFFERREADBUFFERPROC glad_glNamedFramebufferReadBuffer = NULL;
PFNGLNAMEDFRAMEBUFFERRENDERBUFFERPROC glad_glNamedFramebufferRenderbuffer = NULL;
PFNGLNAMEDFRAMEBUFFERTEXTUREPROC glad_glNamedFramebufferTexture = NULL;
PFNGLNAMEDFRAMEBUFFERTEXTURELAYERPROC glad_glNamedFramebufferTextureLayer = NULL;
PFNGLNAMEDRENDERBUFFERSTORAGEPROC glad_glNamedRenderbufferStorage = NULL;
PFNGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLEPROC glad_glNamedRenderbufferStorageMultisample = NULL;
PFNGLNEWLISTPROC glad_glNewList = NULL;
PFNGLNORMAL3BPROC glad_glNormal3b = NULL;
PFNGLNORMAL3BVPROC glad_glNormal3bv = NULL;
PFNGLNORMAL3DPROC glad_glNormal3d = NULL;
PFNGLNORMAL3DVPROC glad_glNormal3dv = NULL;
PFNGLNORMAL3FPROC glad_glNormal3f = NULL;
PFNGLNORMAL3FVPROC glad_glNormal3fv = NULL;
PFNGLNORMAL3IPROC glad_glNormal3i = NULL;
PFNGLNORMAL3IVPROC glad_glNormal3iv = NULL;
PFNGLNORMAL3SPROC glad_glNormal3s = NULL;
PFNGLNORMAL3SVPROC glad_glNormal3sv = NULL;
PFNGLNORMALP3UIPROC glad_glNormalP3ui = NULL;
PFNGLNORMALP3UIVPROC glad_glNormalP3uiv = NULL;
PFNGLNORMALPOINTERPROC glad_glNormalPointer = NULL;
PFNGLOBJECTLABELPROC glad_glObjectLabel = NULL;
PFNGLOBJECTPTRLABELPROC glad_glObjectPtrLabel = NULL;
PFNGLORTHOPROC glad_glOrtho = NULL;
PFNGLPASSTHROUGHPROC glad_glPassThrough = NULL;
PFNGLPATCHPARAMETERFVPROC glad_glPatchParameterfv = NULL;
PFNGLPATCHPARAMETERIPROC glad_glPatchParameteri = NULL;
PFNGLPAUSETRANSFORMFEEDBACKPROC glad_glPauseTransformFeedback = NULL;
PFNGLPIXELMAPFVPROC glad_glPixelMapfv = NULL;
PFNGLPIXELMAPUIVPROC glad_glPixelMapuiv = NULL;
PFNGLPIXELMAPUSVPROC glad_glPixelMapusv = NULL;
PFNGLPIXELSTOREFPROC glad_glPixelStoref = NULL;
PFNGLPIXELSTOREIPROC glad_glPixelStorei = NULL;
PFNGLPIXELTRANSFERFPROC glad_glPixelTransferf = NULL;
PFNGLPIXELTRANSFERIPROC glad_glPixelTransferi = NULL;
PFNGLPIXELZOOMPROC glad_glPixelZoom = NULL;
PFNGLPOINTPARAMETERFPROC glad_glPointParameterf = NULL;
PFNGLPOINTPARAMETERFVPROC glad_glPointParameterfv = NULL;
PFNGLPOINTPARAMETERIPROC glad_glPointParameteri = NULL;
PFNGLPOINTPARAMETERIVPROC glad_glPointParameteriv = NULL;
PFNGLPOINTSIZEPROC glad_glPointSize = NULL;
PFNGLPOLYGONMODEPROC glad_glPolygonMode = NULL;
PFNGLPOLYGONOFFSETPROC glad_glPolygonOffset = NULL;
PFNGLPOLYGONSTIPPLEPROC glad_glPolygonStipple = NULL;
PFNGLPOPATTRIBPROC glad_glPopAttrib = NULL;
PFNGLPOPCLIENTATTRIBPROC glad_glPopClientAttrib = NULL;
PFNGLPOPDEBUGGROUPPROC glad_glPopDebugGroup = NULL;
PFNGLPOPMATRIXPROC glad_glPopMatrix = NULL;
PFNGLPOPNAMEPROC glad_glPopName = NULL;
PFNGLPRIMITIVERESTARTINDEXPROC glad_glPrimitiveRestartIndex = NULL;
PFNGLPRIORITIZETEXTURESPROC glad_glPrioritizeTextures = NULL;
PFNGLPROGRAMBINARYPROC glad_glProgramBinary = NULL;
PFNGLPROGRAMPARAMETERIPROC glad_glProgramParameteri = NULL;
PFNGLPROGRAMUNIFORM1DPROC glad_glProgramUniform1d = NULL;
PFNGLPROGRAMUNIFORM1DVPROC glad_glProgramUniform1dv = NULL;
PFNGLPROGRAMUNIFORM1FPROC glad_glProgramUniform1f = NULL;
PFNGLPROGRAMUNIFORM1FVPROC glad_glProgramUniform1fv = NULL;
PFNGLPROGRAMUNIFORM1IPROC glad_glProgramUniform1i = NULL;
PFNGLPROGRAMUNIFORM1IVPROC glad_glProgramUniform1iv = NULL;
PFNGLPROGRAMUNIFORM1UIPROC glad_glProgramUniform1ui = NULL;
PFNGLPROGRAMUNIFORM1UIVPROC glad_glProgramUniform1uiv = NULL;
PFNGLPROGRAMUNIFORM2DPROC glad_glProgramUniform2d = NULL;
PFNGLPROGRAMUNIFORM2DVPROC glad_glProgramUniform2dv = NULL;
PFNGLPROGRAMUNIFORM2FPROC glad_glProgramUniform2f = NULL;
PFNGLPROGRAMUNIFORM2FVPROC glad_glProgramUniform2fv = NULL;
PFNGLPROGRAMUNIFORM2IPROC glad_glProgramUniform2i = NULL;
PFNGLPROGRAMUNIFORM2IVPROC glad_glProgramUniform2iv = NULL;
PFNGLPROGRAMUNIFORM2UIPROC glad_glProgramUniform2ui = NULL;
PFNGLPROGRAMUNIFORM2UIVPROC glad_glProgramUniform2uiv = NULL;
PFNGLPROGRAMUNIFORM3DPROC glad_glProgramUniform3d = NULL;
PFNGLPROGRAMUNIFORM3DVPROC glad_glProgramUniform3dv = NULL;
PFNGLPROGRAMUNIFORM3FPROC glad_glProgramUniform3f = NULL;
PFNGLPROGRAMUNIFORM3FVPROC glad_glProgramUniform3fv = NULL;
PFNGLPROGRAMUNIFORM3IPROC glad_glProgramUniform3i = NULL;
PFNGLPROGRAMUNIFORM3IVPROC glad_glProgramUniform3iv = NULL;
PFNGLPROGRAMUNIFORM3UIPROC glad_glProgramUniform3ui = NULL;
PFNGLPROGRAMUNIFORM3UIVPROC glad_glProgramUniform3uiv = NULL;
PFNGLPROGRAMUNIFORM4DPROC glad_glProgramUniform4d = NULL;
PFNGLPROGRAMUNIFORM4DVPROC glad_glProgramUniform4dv = NULL;
PFNGLPROGRAMUNIFORM4FPROC glad_glProgramUniform4f = NULL;
PFNGLPROGRAMUNIFORM4FVPROC glad_glProgramUniform4fv = NULL;
PFNGLPROGRAMUNIFORM4IPROC glad_glProgramUniform4i = NULL;
PFNGLPROGRAMUNIFORM4IVPROC glad_glProgramUniform4iv = NULL;
PFNGLPROGRAMUNIFORM4UIPROC glad_glProgramUniform4ui = NULL;
PFNGLPROGRAMUNIFORM4UIVPROC glad_glProgramUniform4uiv = NULL;
PFNGLPROGRAMUNIFORMMATRIX2DVPROC glad_glProgramUniformMatrix2dv = NULL;
PFNGLPROGRAMUNIFORMMATRIX2FVPROC glad_glProgramUniformMatrix2fv = NULL;
PFNGLPROGRAMUNIFORMMATRIX2X3DVPROC glad_glProgramUniformMatrix2x3dv = NULL;
PFNGLPROGRAMUNIFORMMATRIX2X3FVPROC glad_glProgramUniformMatrix2x3fv = NULL;
PFNGLPROGRAMUNIFORMMATRIX2X4DVPROC glad_glProgramUniformMatrix2x4dv = NULL;
PFNGLPROGRAMUNIFORMMATRIX2X4FVPROC glad_glProgramUniformMatrix2x4fv = NULL;
PFNGLPROGRAMUNIFORMMATRIX3DVPROC glad_glProgramUniformMatrix3dv = NULL;
PFNGLPROGRAMUNIFORMMATRIX3FVPROC glad_glProgramUniformMatrix3fv = NULL;
PFNGLPROGRAMUNIFORMMATRIX3X2DVPROC glad_glProgramUniformMatrix3x2dv = NULL;
PFNGLPROGRAMUNIFORMMATRIX3X2FVPROC glad_glProgramUniformMatrix3x2fv = NULL;
PFNGLPROGRAMUNIFORMMATRIX3X4DVPROC glad_glProgramUniformMatrix3x4dv = NULL;
PFNGLPROGRAMUNIFORMMATRIX3X4FVPROC glad_glProgramUniformMatrix3x4fv = NULL;
PFNGLPROGRAMUNIFORMMATRIX4DVPROC glad_glProgramUniformMatrix4dv = NULL;
PFNGLPROGRAMUNIFORMMATRIX4FVPROC glad_glProgramUniformMatrix4fv = NULL;
PFNGLPROGRAMUNIFORMMATRIX4X2DVPROC glad_glProgramUniformMatrix4x2dv = NULL;
PFNGLPROGRAMUNIFORMMATRIX4X2FVPROC glad_glProgramUniformMatrix4x2fv = NULL;
PFNGLPROGRAMUNIFORMMATRIX4X3DVPROC glad_glProgramUniformMatrix4x3dv = NULL;
PFNGLPROGRAMUNIFORMMATRIX4X3FVPROC glad_glProgramUniformMatrix4x3fv = NULL;
PFNGLPROVOKINGVERTEXPROC glad_glProvokingVertex = NULL;
PFNGLPUSHATTRIBPROC glad_glPushAttrib = NULL;
PFNGLPUSHCLIENTATTRIBPROC glad_glPushClientAttrib = NULL;
PFNGLPUSHDEBUGGROUPPROC glad_glPushDebugGroup = NULL;
PFNGLPUSHMATRIXPROC glad_glPushMatrix = NULL;
PFNGLPUSHNAMEPROC glad_glPushName = NULL;
PFNGLQUERYCOUNTERPROC glad_glQueryCounter = NULL;
PFNGLRASTERPOS2DPROC glad_glRasterPos2d = NULL;
PFNGLRASTERPOS2DVPROC glad_glRasterPos2dv = NULL;
PFNGLRASTERPOS2FPROC glad_glRasterPos2f = NULL;
PFNGLRASTERPOS2FVPROC glad_glRasterPos2fv = NULL;
PFNGLRASTERPOS2IPROC glad_glRasterPos2i = NULL;
PFNGLRASTERPOS2IVPROC glad_glRasterPos2iv = NULL;
PFNGLRASTERPOS2SPROC glad_glRasterPos2s = NULL;
PFNGLRASTERPOS2SVPROC glad_glRasterPos2sv = NULL;
PFNGLRASTERPOS3DPROC glad_glRasterPos3d = NULL;
PFNGLRASTERPOS3DVPROC glad_glRasterPos3dv = NULL;
PFNGLRASTERPOS3FPROC glad_glRasterPos3f = NULL;
PFNGLRASTERPOS3FVPROC glad_glRasterPos3fv = NULL;
PFNGLRASTERPOS3IPROC glad_glRasterPos3i = NULL;
PFNGLRASTERPOS3IVPROC glad_glRasterPos3iv = NULL;
PFNGLRASTERPOS3SPROC glad_glRasterPos3s = NULL;
PFNGLRASTERPOS3SVPROC glad_glRasterPos3sv = NULL;
PFNGLRASTERPOS4DPROC glad_glRasterPos4d = NULL;
PFNGLRASTERPOS4DVPROC glad_glRasterPos4dv = NULL;
PFNGLRASTERPOS4FPROC glad_glRasterPos4f = NULL;
PFNGLRASTERPOS4FVPROC glad_glRasterPos4fv = NULL;
PFNGLRASTERPOS4IPROC glad_glRasterPos4i = NULL;
PFNGLRASTERPOS4IVPROC glad_glRasterPos4iv = NULL;
PFNGLRASTERPOS4SPROC glad_glRasterPos4s = NULL;
PFNGLRASTERPOS4SVPROC glad_glRasterPos4sv = NULL;
PFNGLREADBUFFERPROC glad_glReadBuffer = NULL;
PFNGLREADPIXELSPROC glad_glReadPixels = NULL;
PFNGLREADNPIXELSPROC glad_glReadnPixels = NULL;
PFNGLRECTDPROC glad_glRectd = NULL;
PFNGLRECTDVPROC glad_glRectdv = NULL;
PFNGLRECTFPROC glad_glRectf = NULL;
PFNGLRECTFVPROC glad_glRectfv = NULL;
PFNGLRECTIPROC glad_glRecti = NULL;
PFNGLRECTIVPROC glad_glRectiv = NULL;
PFNGLRECTSPROC glad_glRects = NULL;
PFNGLRECTSVPROC glad_glRectsv = NULL;
PFNGLRELEASESHADERCOMPILERPROC glad_glReleaseShaderCompiler = NULL;
PFNGLRENDERMODEPROC glad_glRenderMode = NULL;
PFNGLRENDERBUFFERSTORAGEPROC glad_glRenderbufferStorage = NULL;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC glad_glRenderbufferStorageMultisample = NULL;
PFNGLRESUMETRANSFORMFEEDBACKPROC glad_glResumeTransformFeedback = NULL;
PFNGLROTATEDPROC glad_glRotated = NULL;
PFNGLROTATEFPROC glad_glRotatef = NULL;
PFNGLSAMPLECOVERAGEPROC glad_glSampleCoverage = NULL;
PFNGLSAMPLEMASKIPROC glad_glSampleMaski = NULL;
PFNGLSAMPLERPARAMETERIIVPROC glad_glSamplerParameterIiv = NULL;
PFNGLSAMPLERPARAMETERIUIVPROC glad_glSamplerParameterIuiv = NULL;
PFNGLSAMPLERPARAMETERFPROC glad_glSamplerParameterf = NULL;
PFNGLSAMPLERPARAMETERFVPROC glad_glSamplerParameterfv = NULL;
PFNGLSAMPLERPARAMETERIPROC glad_glSamplerParameteri = NULL;
PFNGLSAMPLERPARAMETERIVPROC glad_glSamplerParameteriv = NULL;
PFNGLSCALEDPROC glad_glScaled = NULL;
PFNGLSCALEFPROC glad_glScalef = NULL;
PFNGLSCISSORPROC glad_glScissor = NULL;
PFNGLSCISSORARRAYVPROC glad_glScissorArrayv = NULL;
PFNGLSCISSORINDEXEDPROC glad_glScissorIndexed = NULL;
PFNGLSCISSORINDEXEDVPROC glad_glScissorIndexedv = NULL;
PFNGLSECONDARYCOLOR3BPROC glad_glSecondaryColor3b = NULL;
PFNGLSECONDARYCOLOR3BVPROC glad_glSecondaryColor3bv = NULL;
PFNGLSECONDARYCOLOR3DPROC glad_glSecondaryColor3d = NULL;
PFNGLSECONDARYCOLOR3DVPROC glad_glSecondaryColor3dv = NULL;
PFNGLSECONDARYCOLOR3FPROC glad_glSecondaryColor3f = NULL;
PFNGLSECONDARYCOLOR3FVPROC glad_glSecondaryColor3fv = NULL;
PFNGLSECONDARYCOLOR3IPROC glad_glSecondaryColor3i = NULL;
PFNGLSECONDARYCOLOR3IVPROC glad_glSecondaryColor3iv = NULL;
PFNGLSECONDARYCOLOR3SPROC glad_glSecondaryColor3s = NULL;
PFNGLSECONDARYCOLOR3SVPROC glad_glSecondaryColor3sv = NULL;
PFNGLSECONDARYCOLOR3UBPROC glad_glSecondaryColor3ub = NULL;
PFNGLSECONDARYCOLOR3UBVPROC glad_glSecondaryColor3ubv = NULL;
PFNGLSECONDARYCOLOR3UIPROC glad_glSecondaryColor3ui = NULL;
PFNGLSECONDARYCOLOR3UIVPROC glad_glSecondaryColor3uiv = NULL;
PFNGLSECONDARYCOLOR3USPROC glad_glSecondaryColor3us = NULL;
PFNGLSECONDARYCOLOR3USVPROC glad_glSecondaryColor3usv = NULL;
PFNGLSECONDARYCOLORP3UIPROC glad_glSecondaryColorP3ui = NULL;
PFNGLSECONDARYCOLORP3UIVPROC glad_glSecondaryColorP3uiv = NULL;
PFNGLSECONDARYCOLORPOINTERPROC glad_glSecondaryColorPointer = NULL;
PFNGLSELECTBUFFERPROC glad_glSelectBuffer = NULL;
PFNGLSHADEMODELPROC glad_glShadeModel = NULL;
PFNGLSHADERBINARYPROC glad_glShaderBinary = NULL;
PFNGLSHADERSOURCEPROC glad_glShaderSource = NULL;
PFNGLSHADERSTORAGEBLOCKBINDINGPROC glad_glShaderStorageBlockBinding = NULL;
PFNGLSTENCILFUNCPROC glad_glStencilFunc = NULL;
PFNGLSTENCILFUNCSEPARATEPROC glad_glStencilFuncSeparate = NULL;
PFNGLSTENCILMASKPROC glad_glStencilMask = NULL;
PFNGLSTENCILMASKSEPARATEPROC glad_glStencilMaskSeparate = NULL;
PFNGLSTENCILOPPROC glad_glStencilOp = NULL;
PFNGLSTENCILOPSEPARATEPROC glad_glStencilOpSeparate = NULL;
PFNGLTEXBUFFERPROC glad_glTexBuffer = NULL;
PFNGLTEXBUFFERRANGEPROC glad_glTexBufferRange = NULL;
PFNGLTEXCOORD1DPROC glad_glTexCoord1d = NULL;
PFNGLTEXCOORD1DVPROC glad_glTexCoord1dv = NULL;
PFNGLTEXCOORD1FPROC glad_glTexCoord1f = NULL;
PFNGLTEXCOORD1FVPROC glad_glTexCoord1fv = NULL;
PFNGLTEXCOORD1IPROC glad_glTexCoord1i = NULL;
PFNGLTEXCOORD1IVPROC glad_glTexCoord1iv = NULL;
PFNGLTEXCOORD1SPROC glad_glTexCoord1s = NULL;
PFNGLTEXCOORD1SVPROC glad_glTexCoord1sv = NULL;
PFNGLTEXCOORD2DPROC glad_glTexCoord2d = NULL;
PFNGLTEXCOORD2DVPROC glad_glTexCoord2dv = NULL;
PFNGLTEXCOORD2FPROC glad_glTexCoord2f = NULL;
PFNGLTEXCOORD2FVPROC glad_glTexCoord2fv = NULL;
PFNGLTEXCOORD2IPROC glad_glTexCoord2i = NULL;
PFNGLTEXCOORD2IVPROC glad_glTexCoord2iv = NULL;
PFNGLTEXCOORD2SPROC glad_glTexCoord2s = NULL;
PFNGLTEXCOORD2SVPROC glad_glTexCoord2sv = NULL;
PFNGLTEXCOORD3DPROC glad_glTexCoord3d = NULL;
PFNGLTEXCOORD3DVPROC glad_glTexCoord3dv = NULL;
PFNGLTEXCOORD3FPROC glad_glTexCoord3f = NULL;
PFNGLTEXCOORD3FVPROC glad_glTexCoord3fv = NULL;
PFNGLTEXCOORD3IPROC glad_glTexCoord3i = NULL;
PFNGLTEXCOORD3IVPROC glad_glTexCoord3iv = NULL;
PFNGLTEXCOORD3SPROC glad_glTexCoord3s = NULL;
PFNGLTEXCOORD3SVPROC glad_glTexCoord3sv = NULL;
PFNGLTEXCOORD4DPROC glad_glTexCoord4d = NULL;
PFNGLTEXCOORD4DVPROC glad_glTexCoord4dv = NULL;
PFNGLTEXCOORD4FPROC glad_glTexCoord4f = NULL;
PFNGLTEXCOORD4FVPROC glad_glTexCoord4fv = NULL;
PFNGLTEXCOORD4IPROC glad_glTexCoord4i = NULL;
PFNGLTEXCOORD4IVPROC glad_glTexCoord4iv = NULL;
PFNGLTEXCOORD4SPROC glad_glTexCoord4s = NULL;
PFNGLTEXCOORD4SVPROC glad_glTexCoord4sv = NULL;
PFNGLTEXCOORDP1UIPROC glad_glTexCoordP1ui = NULL;
PFNGLTEXCOORDP1UIVPROC glad_glTexCoordP1uiv = NULL;
PFNGLTEXCOORDP2UIPROC glad_glTexCoordP2ui = NULL;
PFNGLTEXCOORDP2UIVPROC glad_glTexCoordP2uiv = NULL;
PFNGLTEXCOORDP3UIPROC glad_glTexCoordP3ui = NULL;
PFNGLTEXCOORDP3UIVPROC glad_glTexCoordP3uiv = NULL;
PFNGLTEXCOORDP4UIPROC glad_glTexCoordP4ui = NULL;
PFNGLTEXCOORDP4UIVPROC glad_glTexCoordP4uiv = NULL;
PFNGLTEXCOORDPOINTERPROC glad_glTexCoordPointer = NULL;
PFNGLTEXENVFPROC glad_glTexEnvf = NULL;
PFNGLTEXENVFVPROC glad_glTexEnvfv = NULL;
PFNGLTEXENVIPROC glad_glTexEnvi = NULL;
PFNGLTEXENVIVPROC glad_glTexEnviv = NULL;
PFNGLTEXGENDPROC glad_glTexGend = NULL;
PFNGLTEXGENDVPROC glad_glTexGendv = NULL;
PFNGLTEXGENFPROC glad_glTexGenf = NULL;
PFNGLTEXGENFVPROC glad_glTexGenfv = NULL;
PFNGLTEXGENIPROC glad_glTexGeni = NULL;
PFNGLTEXGENIVPROC glad_glTexGeniv = NULL;
PFNGLTEXIMAGE1DPROC glad_glTexImage1D = NULL;
PFNGLTEXIMAGE2DPROC glad_glTexImage2D = NULL;
PFNGLTEXIMAGE2DMULTISAMPLEPROC glad_glTexImage2DMultisample = NULL;
PFNGLTEXIMAGE3DPROC glad_glTexImage3D = NULL;
PFNGLTEXIMAGE3DMULTISAMPLEPROC glad_glTexImage3DMultisample = NULL;
PFNGLTEXPARAMETERIIVPROC glad_glTexParameterIiv = NULL;
PFNGLTEXPARAMETERIUIVPROC glad_glTexParameterIuiv = NULL;
PFNGLTEXPARAMETERFPROC glad_glTexParameterf = NULL;
PFNGLTEXPARAMETERFVPROC glad_glTexParameterfv = NULL;
PFNGLTEXPARAMETERIPROC glad_glTexParameteri = NULL;
PFNGLTEXPARAMETERIVPROC glad_glTexParameteriv = NULL;
PFNGLTEXSTORAGE1DPROC glad_glTexStorage1D = NULL;
PFNGLTEXSTORAGE2DPROC glad_glTexStorage2D = NULL;
PFNGLTEXSTORAGE2DMULTISAMPLEPROC glad_glTexStorage2DMultisample = NULL;
PFNGLTEXSTORAGE3DPROC glad_glTexStorage3D = NULL;
PFNGLTEXSTORAGE3DMULTISAMPLEPROC glad_glTexStorage3DMultisample = NULL;
PFNGLTEXSUBIMAGE1DPROC glad_glTexSubImage1D = NULL;
PFNGLTEXSUBIMAGE2DPROC glad_glTexSubImage2D = NULL;
PFNGLTEXSUBIMAGE3DPROC glad_glTexSubImage3D = NULL;
PFNGLTEXTUREBARRIERPROC glad_glTextureBarrier = NULL;
PFNGLTEXTUREBUFFERPROC glad_glTextureBuffer = NULL;
PFNGLTEXTUREBUFFERRANGEPROC glad_glTextureBufferRange = NULL;
PFNGLTEXTUREPARAMETERIIVPROC glad_glTextureParameterIiv = NULL;
PFNGLTEXTUREPARAMETERIUIVPROC glad_glTextureParameterIuiv = NULL;
PFNGLTEXTUREPARAMETERFPROC glad_glTextureParameterf = NULL;
PFNGLTEXTUREPARAMETERFVPROC glad_glTextureParameterfv = NULL;
PFNGLTEXTUREPARAMETERIPROC glad_glTextureParameteri = NULL;
PFNGLTEXTUREPARAMETERIVPROC glad_glTextureParameteriv = NULL;
PFNGLTEXTURESTORAGE1DPROC glad_glTextureStorage1D = NULL;
PFNGLTEXTURESTORAGE2DPROC glad_glTextureStorage2D = NULL;
PFNGLTEXTURESTORAGE2DMULTISAMPLEPROC glad_glTextureStorage2DMultisample = NULL;
PFNGLTEXTURESTORAGE3DPROC glad_glTextureStorage3D = NULL;
PFNGLTEXTURESTORAGE3DMULTISAMPLEPROC glad_glTextureStorage3DMultisample = NULL;
PFNGLTEXTURESUBIMAGE1DPROC glad_glTextureSubImage1D = NULL;
PFNGLTEXTURESUBIMAGE2DPROC glad_glTextureSubImage2D = NULL;
PFNGLTEXTURESUBIMAGE3DPROC glad_glTextureSubImage3D = NULL;
PFNGLTEXTUREVIEWPROC glad_glTextureView = NULL;
PFNGLTRANSFORMFEEDBACKBUFFERBASEPROC glad_glTransformFeedbackBufferBase = NULL;
PFNGLTRANSFORMFEEDBACKBUFFERRANGEPROC glad_glTransformFeedbackBufferRange = NULL;
PFNGLTRANSFORMFEEDBACKVARYINGSPROC glad_glTransformFeedbackVaryings = NULL;
PFNGLTRANSLATEDPROC glad_glTranslated = NULL;
PFNGLTRANSLATEFPROC glad_glTranslatef = NULL;
PFNGLUNIFORM1DPROC glad_glUniform1d = NULL;
PFNGLUNIFORM1DVPROC glad_glUniform1dv = NULL;
PFNGLUNIFORM1FPROC glad_glUniform1f = NULL;
PFNGLUNIFORM1FVPROC glad_glUniform1fv = NULL;
PFNGLUNIFORM1IPROC glad_glUniform1i = NULL;
PFNGLUNIFORM1IVPROC glad_glUniform1iv = NULL;
PFNGLUNIFORM1UIPROC glad_glUniform1ui = NULL;
PFNGLUNIFORM1UIVPROC glad_glUniform1uiv = NULL;
PFNGLUNIFORM2DPROC glad_glUniform2d = NULL;
PFNGLUNIFORM2DVPROC glad_glUniform2dv = NULL;
PFNGLUNIFORM2FPROC glad_glUniform2f = NULL;
PFNGLUNIFORM2FVPROC glad_glUniform2fv = NULL;
PFNGLUNIFORM2IPROC glad_glUniform2i = NULL;
PFNGLUNIFORM2IVPROC glad_glUniform2iv = NULL;
PFNGLUNIFORM2UIPROC glad_glUniform2ui = NULL;
PFNGLUNIFORM2UIVPROC glad_glUniform2uiv = NULL;
PFNGLUNIFORM3DPROC glad_glUniform3d = NULL;
PFNGLUNIFORM3DVPROC glad_glUniform3dv = NULL;
PFNGLUNIFORM3FPROC glad_glUniform3f = NULL;
PFNGLUNIFORM3FVPROC glad_glUniform3fv = NULL;
PFNGLUNIFORM3IPROC glad_glUniform3i = NULL;
PFNGLUNIFORM3IVPROC glad_glUniform3iv = NULL;
PFNGLUNIFORM3UIPROC glad_glUniform3ui = NULL;
PFNGLUNIFORM3UIVPROC glad_glUniform3uiv = NULL;
PFNGLUNIFORM4DPROC glad_glUniform4d = NULL;
PFNGLUNIFORM4DVPROC glad_glUniform4dv = NULL;
PFNGLUNIFORM4FPROC glad_glUniform4f = NULL;
PFNGLUNIFORM4FVPROC glad_glUniform4fv = NULL;
PFNGLUNIFORM4IPROC glad_glUniform4i = NULL;
PFNGLUNIFORM4IVPROC glad_glUniform4iv = NULL;
PFNGLUNIFORM4UIPROC glad_glUniform4ui = NULL;
PFNGLUNIFORM4UIVPROC glad_glUniform4uiv = NULL;
PFNGLUNIFORMBLOCKBINDINGPROC glad_glUniformBlockBinding = NULL;
PFNGLUNIFORMMATRIX2DVPROC glad_glUniformMatrix2dv = NULL;
PFNGLUNIFORMMATRIX2FVPROC glad_glUniformMatrix2fv = NULL;
PFNGLUNIFORMMATRIX2X3DVPROC glad_glUniformMatrix2x3dv = NULL;
PFNGLUNIFORMMATRIX2X3FVPROC glad_glUniformMatrix2x3fv = NULL;
PFNGLUNIFORMMATRIX2X4DVPROC glad_glUniformMatrix2x4dv = NULL;
PFNGLUNIFORMMATRIX2X4FVPROC glad_glUniformMatrix2x4fv = NULL;
PFNGLUNIFORMMATRIX3DVPROC glad_glUniformMatrix3dv = NULL;
PFNGLUNIFORMMATRIX3FVPROC glad_glUniformMatrix3fv = NULL;
PFNGLUNIFORMMATRIX3X2DVPROC glad_glUniformMatrix3x2dv = NULL;
PFNGLUNIFORMMATRIX3X2FVPROC glad_glUniformMatrix3x2fv = NULL;
PFNGLUNIFORMMATRIX3X4DVPROC glad_glUniformMatrix3x4dv = NULL;
PFNGLUNIFORMMATRIX3X4FVPROC glad_glUniformMatrix3x4fv = NULL;
PFNGLUNIFORMMATRIX4DVPROC glad_glUniformMatrix4dv = NULL;
PFNGLUNIFORMMATRIX4FVPROC glad_glUniformMatrix4fv = NULL;
PFNGLUNIFORMMATRIX4X2DVPROC glad_glUniformMatrix4x2dv = NULL;
PFNGLUNIFORMMATRIX4X2FVPROC glad_glUniformMatrix4x2fv = NULL;
PFNGLUNIFORMMATRIX4X3DVPROC glad_glUniformMatrix4x3dv = NULL;
PFNGLUNIFORMMATRIX4X3FVPROC glad_glUniformMatrix4x3fv = NULL;
PFNGLUNIFORMSUBROUTINESUIVPROC glad_glUniformSubroutinesuiv = NULL;
PFNGLUNMAPBUFFERPROC glad_glUnmapBuffer = NULL;
PFNGLUNMAPNAMEDBUFFERPROC glad_glUnmapNamedBuffer = NULL;
PFNGLUSEPROGRAMPROC glad_glUseProgram = NULL;
PFNGLUSEPROGRAMSTAGESPROC glad_glUseProgramStages = NULL;
PFNGLVALIDATEPROGRAMPROC glad_glValidateProgram = NULL;
PFNGLVALIDATEPROGRAMPIPELINEPROC glad_glValidateProgramPipeline = NULL;
PFNGLVERTEX2DPROC glad_glVertex2d = NULL;
PFNGLVERTEX2DVPROC glad_glVertex2dv = NULL;
PFNGLVERTEX2FPROC glad_glVertex2f = NULL;
PFNGLVERTEX2FVPROC glad_glVertex2fv = NULL;
PFNGLVERTEX2IPROC glad_glVertex2i = NULL;
PFNGLVERTEX2IVPROC glad_glVertex2iv = NULL;
PFNGLVERTEX2SPROC glad_glVertex2s = NULL;
PFNGLVERTEX2SVPROC glad_glVertex2sv = NULL;
PFNGLVERTEX3DPROC glad_glVertex3d = NULL;
PFNGLVERTEX3DVPROC glad_glVertex3dv = NULL;
PFNGLVERTEX3FPROC glad_glVertex3f = NULL;
PFNGLVERTEX3FVPROC glad_glVertex3fv = NULL;
PFNGLVERTEX3IPROC glad_glVertex3i = NULL;
PFNGLVERTEX3IVPROC glad_glVertex3iv = NULL;
PFNGLVERTEX3SPROC glad_glVertex3s = NULL;
PFNGLVERTEX3SVPROC glad_glVertex3sv = NULL;
PFNGLVERTEX4DPROC glad_glVertex4d = NULL;
PFNGLVERTEX4DVPROC glad_glVertex4dv = NULL;
PFNGLVERTEX4FPROC glad_glVertex4f = NULL;
PFNGLVERTEX4FVPROC glad_glVertex4fv = NULL;
PFNGLVERTEX4IPROC glad_glVertex4i = NULL;
PFNGLVERTEX4IVPROC glad_glVertex4iv = NULL;
PFNGLVERTEX4SPROC glad_glVertex4s = NULL;
PFNGLVERTEX4SVPROC glad_glVertex4sv = NULL;
PFNGLVERTEXARRAYATTRIBBINDINGPROC glad_glVertexArrayAttribBinding = NULL;
PFNGLVERTEXARRAYATTRIBFORMATPROC glad_glVertexArrayAttribFormat = NULL;
PFNGLVERTEXARRAYATTRIBIFORMATPROC glad_glVertexArrayAttribIFormat = NULL;
PFNGLVERTEXARRAYATTRIBLFORMATPROC glad_glVertexArrayAttribLFormat = NULL;
PFNGLVERTEXARRAYBINDINGDIVISORPROC glad_glVertexArrayBindingDivisor = NULL;
PFNGLVERTEXARRAYELEMENTBUFFERPROC glad_glVertexArrayElementBuffer = NULL;
PFNGLVERTEXARRAYVERTEXBUFFERPROC glad_glVertexArrayVertexBuffer = NULL;
PFNGLVERTEXARRAYVERTEXBUFFERSPROC glad_glVertexArrayVertexBuffers = NULL;
PFNGLVERTEXATTRIB1DPROC glad_glVertexAttrib1d = NULL;
PFNGLVERTEXATTRIB1DVPROC glad_glVertexAttrib1dv = NULL;
PFNGLVERTEXATTRIB1FPROC glad_glVertexAttrib1f = NULL;
PFNGLVERTEXATTRIB1FVPROC glad_glVertexAttrib1fv = NULL;
PFNGLVERTEXATTRIB1SPROC glad_glVertexAttrib1s = NULL;
PFNGLVERTEXATTRIB1SVPROC glad_glVertexAttrib1sv = NULL;
PFNGLVERTEXATTRIB2DPROC glad_glVertexAttrib2d = NULL;
PFNGLVERTEXATTRIB2DVPROC glad_glVertexAttrib2dv = NULL;
PFNGLVERTEXATTRIB2FPROC glad_glVertexAttrib2f = NULL;
PFNGLVERTEXATTRIB2FVPROC glad_glVertexAttrib2fv = NULL;
PFNGLVERTEXATTRIB2SPROC glad_glVertexAttrib2s = NULL;
PFNGLVERTEXATTRIB2SVPROC glad_glVertexAttrib2sv = NULL;
PFNGLVERTEXATTRIB3DPROC glad_glVertexAttrib3d = NULL;
PFNGLVERTEXATTRIB3DVPROC glad_glVertexAttrib3dv = NULL;
PFNGLVERTEXATTRIB3FPROC glad_glVertexAttrib3f = NULL;
PFNGLVERTEXATTRIB3FVPROC glad_glVertexAttrib3fv = NULL;
PFNGLVERTEXATTRIB3SPROC glad_glVertexAttrib3s = NULL;
PFNGLVERTEXATTRIB3SVPROC glad_glVertexAttrib3sv = NULL;
PFNGLVERTEXATTRIB4NBVPROC glad_glVertexAttrib4Nbv = NULL;
PFNGLVERTEXATTRIB4NIVPROC glad_glVertexAttrib4Niv = NULL;
PFNGLVERTEXATTRIB4NSVPROC glad_glVertexAttrib4Nsv = NULL;
PFNGLVERTEXATTRIB4NUBPROC glad_glVertexAttrib4Nub = NULL;
PFNGLVERTEXATTRIB4NUBVPROC glad_glVertexAttrib4Nubv = NULL;
PFNGLVERTEXATTRIB4NUIVPROC glad_glVertexAttrib4Nuiv = NULL;
PFNGLVERTEXATTRIB4NUSVPROC glad_glVertexAttrib4Nusv = NULL;
PFNGLVERTEXATTRIB4BVPROC glad_glVertexAttrib4bv = NULL;
PFNGLVERTEXATTRIB4DPROC glad_glVertexAttrib4d = NULL;
PFNGLVERTEXATTRIB4DVPROC glad_glVertexAttrib4dv = NULL;
PFNGLVERTEXATTRIB4FPROC glad_glVertexAttrib4f = NULL;
PFNGLVERTEXATTRIB4FVPROC glad_glVertexAttrib4fv = NULL;
PFNGLVERTEXATTRIB4IVPROC glad_glVertexAttrib4iv = NULL;
PFNGLVERTEXATTRIB4SPROC glad_glVertexAttrib4s = NULL;
PFNGLVERTEXATTRIB4SVPROC glad_glVertexAttrib4sv = NULL;
PFNGLVERTEXATTRIB4UBVPROC glad_glVertexAttrib4ubv = NULL;
PFNGLVERTEXATTRIB4UIVPROC glad_glVertexAttrib4uiv = NULL;
PFNGLVERTEXATTRIB4USVPROC glad_glVertexAttrib4usv = NULL;
PFNGLVERTEXATTRIBBINDINGPROC glad_glVertexAttribBinding = NULL;
PFNGLVERTEXATTRIBDIVISORPROC glad_glVertexAttribDivisor = NULL;
PFNGLVERTEXATTRIBFORMATPROC glad_glVertexAttribFormat = NULL;
PFNGLVERTEXATTRIBI1IPROC glad_glVertexAttribI1i = NULL;
PFNGLVERTEXATTRIBI1IVPROC glad_glVertexAttribI1iv = NULL;
PFNGLVERTEXATTRIBI1UIPROC glad_glVertexAttribI1ui = NULL;
PFNGLVERTEXATTRIBI1UIVPROC glad_glVertexAttribI1uiv = NULL;
PFNGLVERTEXATTRIBI2IPROC glad_glVertexAttribI2i = NULL;
PFNGLVERTEXATTRIBI2IVPROC glad_glVertexAttribI2iv = NULL;
PFNGLVERTEXATTRIBI2UIPROC glad_glVertexAttribI2ui = NULL;
PFNGLVERTEXATTRIBI2UIVPROC glad_glVertexAttribI2uiv = NULL;
PFNGLVERTEXATTRIBI3IPROC glad_glVertexAttribI3i = NULL;
PFNGLVERTEXATTRIBI3IVPROC glad_glVertexAttribI3iv = NULL;
PFNGLVERTEXATTRIBI3UIPROC glad_glVertexAttribI3ui = NULL;
PFNGLVERTEXATTRIBI3UIVPROC glad_glVertexAttribI3uiv = NULL;
PFNGLVERTEXATTRIBI4BVPROC glad_glVertexAttribI4bv = NULL;
PFNGLVERTEXATTRIBI4IPROC glad_glVertexAttribI4i = NULL;
PFNGLVERTEXATTRIBI4IVPROC glad_glVertexAttribI4iv = NULL;
PFNGLVERTEXATTRIBI4SVPROC glad_glVertexAttribI4sv = NULL;
PFNGLVERTEXATTRIBI4UBVPROC glad_glVertexAttribI4ubv = NULL;
PFNGLVERTEXATTRIBI4UIPROC glad_glVertexAttribI4ui = NULL;
PFNGLVERTEXATTRIBI4UIVPROC glad_glVertexAttribI4uiv = NULL;
PFNGLVERTEXATTRIBI4USVPROC glad_glVertexAttribI4usv = NULL;
PFNGLVERTEXATTRIBIFORMATPROC glad_glVertexAttribIFormat = NULL;
PFNGLVERTEXATTRIBIPOINTERPROC glad_glVertexAttribIPointer = NULL;
PFNGLVERTEXATTRIBL1DPROC glad_glVertexAttribL1d = NULL;
PFNGLVERTEXATTRIBL1DVPROC glad_glVertexAttribL1dv = NULL;
PFNGLVERTEXATTRIBL2DPROC glad_glVertexAttribL2d = NULL;
PFNGLVERTEXATTRIBL2DVPROC glad_glVertexAttribL2dv = NULL;
PFNGLVERTEXATTRIBL3DPROC glad_glVertexAttribL3d = NULL;
PFNGLVERTEXATTRIBL3DVPROC glad_glVertexAttribL3dv = NULL;
PFNGLVERTEXATTRIBL4DPROC glad_glVertexAttribL4d = NULL;
PFNGLVERTEXATTRIBL4DVPROC glad_glVertexAttribL4dv = NULL;
PFNGLVERTEXATTRIBLFORMATPROC glad_glVertexAttribLFormat = NULL;
PFNGLVERTEXATTRIBLPOINTERPROC glad_glVertexAttribLPointer = NULL;
PFNGLVERTEXATTRIBP1UIPROC glad_glVertexAttribP1ui = NULL;
PFNGLVERTEXATTRIBP1UIVPROC glad_glVertexAttribP1uiv = NULL;
PFNGLVERTEXATTRIBP2UIPROC glad_glVertexAttribP2ui = NULL;
PFNGLVERTEXATTRIBP2UIVPROC glad_glVertexAttribP2uiv = NULL;
PFNGLVERTEXATTRIBP3UIPROC glad_glVertexAttribP3ui = NULL;
PFNGLVERTEXATTRIBP3UIVPROC glad_glVertexAttribP3uiv = NULL;
PFNGLVERTEXATTRIBP4UIPROC glad_glVertexAttribP4ui = NULL;
PFNGLVERTEXATTRIBP4UIVPROC glad_glVertexAttribP4uiv = NULL;
PFNGLVERTEXATTRIBPOINTERPROC glad_glVertexAttribPointer = NULL;
PFNGLVERTEXBINDINGDIVISORPROC glad_glVertexBindingDivisor = NULL;
PFNGLVERTEXP2UIPROC glad_glVertexP2ui = NULL;
PFNGLVERTEXP2UIVPROC glad_glVertexP2uiv = NULL;
PFNGLVERTEXP3UIPROC glad_glVertexP3ui = NULL;
PFNGLVERTEXP3UIVPROC glad_glVertexP3uiv = NULL;
PFNGLVERTEXP4UIPROC glad_glVertexP4ui = NULL;
PFNGLVERTEXP4UIVPROC glad_glVertexP4uiv = NULL;
PFNGLVERTEXPOINTERPROC glad_glVertexPointer = NULL;
PFNGLVIEWPORTPROC glad_glViewport = NULL;
PFNGLVIEWPORTARRAYVPROC glad_glViewportArrayv = NULL;
PFNGLVIEWPORTINDEXEDFPROC glad_glViewportIndexedf = NULL;
PFNGLVIEWPORTINDEXEDFVPROC glad_glViewportIndexedfv = NULL;
PFNGLWAITSYNCPROC glad_glWaitSync = NULL;
PFNGLWINDOWPOS2DPROC glad_glWindowPos2d = NULL;
PFNGLWINDOWPOS2DVPROC glad_glWindowPos2dv = NULL;
PFNGLWINDOWPOS2FPROC glad_glWindowPos2f = NULL;
PFNGLWINDOWPOS2FVPROC glad_glWindowPos2fv = NULL;
PFNGLWINDOWPOS2IPROC glad_glWindowPos2i = NULL;
PFNGLWINDOWPOS2IVPROC glad_glWindowPos2iv = NULL;
PFNGLWINDOWPOS2SPROC glad_glWindowPos2s = NULL;
PFNGLWINDOWPOS2SVPROC glad_glWindowPos2sv = NULL;
PFNGLWINDOWPOS3DPROC glad_glWindowPos3d = NULL;
PFNGLWINDOWPOS3DVPROC glad_glWindowPos3dv = NULL;
PFNGLWINDOWPOS3FPROC glad_glWindowPos3f = NULL;
PFNGLWINDOWPOS3FVPROC glad_glWindowPos3fv = NULL;
PFNGLWINDOWPOS3IPROC glad_glWindowPos3i = NULL;
PFNGLWINDOWPOS3IVPROC glad_glWindowPos3iv = NULL;
PFNGLWINDOWPOS3SPROC glad_glWindowPos3s = NULL;
PFNGLWINDOWPOS3SVPROC glad_glWindowPos3sv = NULL;
static void load_GL_VERSION_1_0(GLADloadproc load) {
    if (!GLAD_GL_VERSION_1_0) return;
    glad_glCullFace = (PFNGLCULLFACEPROC)load("glCullFace");
    glad_glFrontFace = (PFNGLFRONTFACEPROC)load("glFrontFace");
    glad_glHint = (PFNGLHINTPROC)load("glHint");
    glad_glLineWidth = (PFNGLLINEWIDTHPROC)load("glLineWidth");
    glad_glPointSize = (PFNGLPOINTSIZEPROC)load("glPointSize");
    glad_glPolygonMode = (PFNGLPOLYGONMODEPROC)load("glPolygonMode");
    glad_glScissor = (PFNGLSCISSORPROC)load("glScissor");
    glad_glTexParameterf = (PFNGLTEXPARAMETERFPROC)load("glTexParameterf");
    glad_glTexParameterfv = (PFNGLTEXPARAMETERFVPROC)load("glTexParameterfv");
    glad_glTexParameteri = (PFNGLTEXPARAMETERIPROC)load("glTexParameteri");
    glad_glTexParameteriv = (PFNGLTEXPARAMETERIVPROC)load("glTexParameteriv");
    glad_glTexImage1D = (PFNGLTEXIMAGE1DPROC)load("glTexImage1D");
    glad_glTexImage2D = (PFNGLTEXIMAGE2DPROC)load("glTexImage2D");
    glad_glDrawBuffer = (PFNGLDRAWBUFFERPROC)load("glDrawBuffer");
    glad_glClear = (PFNGLCLEARPROC)load("glClear");
    glad_glClearColor = (PFNGLCLEARCOLORPROC)load("glClearColor");
    glad_glClearStencil = (PFNGLCLEARSTENCILPROC)load("glClearStencil");
    glad_glClearDepth = (PFNGLCLEARDEPTHPROC)load("glClearDepth");
    glad_glStencilMask = (PFNGLSTENCILMASKPROC)load("glStencilMask");
    glad_glColorMask = (PFNGLCOLORMASKPROC)load("glColorMask");
    glad_glDepthMask = (PFNGLDEPTHMASKPROC)load("glDepthMask");
    glad_glDisable = (PFNGLDISABLEPROC)load("glDisable");
    glad_glEnable = (PFNGLENABLEPROC)load("glEnable");
    glad_glFinish = (PFNGLFINISHPROC)load("glFinish");
    glad_glFlush = (PFNGLFLUSHPROC)load("glFlush");
    glad_glBlendFunc = (PFNGLBLENDFUNCPROC)load("glBlendFunc");
    glad_glLogicOp = (PFNGLLOGICOPPROC)load("glLogicOp");
    glad_glStencilFunc = (PFNGLSTENCILFUNCPROC)load("glStencilFunc");
    glad_glStencilOp = (PFNGLSTENCILOPPROC)load("glStencilOp");
    glad_glDepthFunc = (PFNGLDEPTHFUNCPROC)load("glDepthFunc");
    glad_glPixelStoref = (PFNGLPIXELSTOREFPROC)load("glPixelStoref");
    glad_glPixelStorei = (PFNGLPIXELSTOREIPROC)load("glPixelStorei");
    glad_glReadBuffer = (PFNGLREADBUFFERPROC)load("glReadBuffer");
    glad_glReadPixels = (PFNGLREADPIXELSPROC)load("glReadPixels");
    glad_glGetBooleanv = (PFNGLGETBOOLEANVPROC)load("glGetBooleanv");
    glad_glGetDoublev = (PFNGLGETDOUBLEVPROC)load("glGetDoublev");
    glad_glGetError = (PFNGLGETERRORPROC)load("glGetError");
    glad_glGetFloatv = (PFNGLGETFLOATVPROC)load("glGetFloatv");
    glad_glGetIntegerv = (PFNGLGETINTEGERVPROC)load("glGetIntegerv");
    glad_glGetString = (PFNGLGETSTRINGPROC)load("glGetString");
    glad_glGetTexImage = (PFNGLGETTEXIMAGEPROC)load("glGetTexImage");
    glad_glGetTexParameterfv = (PFNGLGETTEXPARAMETERFVPROC)load("glGetTexParameterfv");
    glad_glGetTexParameteriv = (PFNGLGETTEXPARAMETERIVPROC)load("glGetTexParameteriv");
    glad_glGetTexLevelParameterfv = (PFNGLGETTEXLEVELPARAMETERFVPROC)load("glGetTexLevelParameterfv");
    glad_glGetTexLevelParameteriv = (PFNGLGETTEXLEVELPARAMETERIVPROC)load("glGetTexLevelParameteriv");
    glad_glIsEnabled = (PFNGLISENABLEDPROC)load("glIsEnabled");
    glad_glDepthRange = (PFNGLDEPTHRANGEPROC)load("glDepthRange");
    glad_glViewport = (PFNGLVIEWPORTPROC)load("glViewport");
    glad_glNewList = (PFNGLNEWLISTPROC)load("glNewList");
    glad_glEndList = (PFNGLENDLISTPROC)load("glEndList");
    glad_glCallList = (PFNGLCALLLISTPROC)load("glCallList");
    glad_glCallLists = (PFNGLCALLLISTSPROC)load("glCallLists");
    glad_glDeleteLists = (PFNGLDELETELISTSPROC)load("glDeleteLists");
    glad_glGenLists = (PFNGLGENLISTSPROC)load("glGenLists");
    glad_glListBase = (PFNGLLISTBASEPROC)load("glListBase");
    glad_glBegin = (PFNGLBEGINPROC)load("glBegin");
    glad_glBitmap = (PFNGLBITMAPPROC)load("glBitmap");
    glad_glColor3b = (PFNGLCOLOR3BPROC)load("glColor3b");
    glad_glColor3bv = (PFNGLCOLOR3BVPROC)load("glColor3bv");
    glad_glColor3d = (PFNGLCOLOR3DPROC)load("glColor3d");
    glad_glColor3dv = (PFNGLCOLOR3DVPROC)load("glColor3dv");
    glad_glColor3f = (PFNGLCOLOR3FPROC)load("glColor3f");
    glad_glColor3fv = (PFNGLCOLOR3FVPROC)load("glColor3fv");
    glad_glColor3i = (PFNGLCOLOR3IPROC)load("glColor3i");
    glad_glColor3iv = (PFNGLCOLOR3IVPROC)load("glColor3iv");
    glad_glColor3s = (PFNGLCOLOR3SPROC)load("glColor3s");
    glad_glColor3sv = (PFNGLCOLOR3SVPROC)load("glColor3sv");
    glad_glColor3ub = (PFNGLCOLOR3UBPROC)load("glColor3ub");
    glad_glColor3ubv = (PFNGLCOLOR3UBVPROC)load("glColor3ubv");
    glad_glColor3ui = (PFNGLCOLOR3UIPROC)load("glColor3ui");
    glad_glColor3uiv = (PFNGLCOLOR3UIVPROC)load("glColor3uiv");
    glad_glColor3us = (PFNGLCOLOR3USPROC)load("glColor3us");
    glad_glColor3usv = (PFNGLCOLOR3USVPROC)load("glColor3usv");
    glad_glColor4b = (PFNGLCOLOR4BPROC)load("glColor4b");
    glad_glColor4bv = (PFNGLCOLOR4BVPROC)load("glColor4bv");
    glad_glColor4d = (PFNGLCOLOR4DPROC)load("glColor4d");
    glad_glColor4dv = (PFNGLCOLOR4DVPROC)load("glColor4dv");
    glad_glColor4f = (PFNGLCOLOR4FPROC)load("glColor4f");
    glad_glColor4fv = (PFNGLCOLOR4FVPROC)load("glColor4fv");
    glad_glColor4i = (PFNGLCOLOR4IPROC)load("glColor4i");
    glad_glColor4iv = (PFNGLCOLOR4IVPROC)load("glColor4iv");
    glad_glColor4s = (PFNGLCOLOR4SPROC)load("glColor4s");
    glad_glColor4sv = (PFNGLCOLOR4SVPROC)load("glColor4sv");
    glad_glColor4ub = (PFNGLCOLOR4UBPROC)load("glColor4ub");
    glad_glColor4ubv = (PFNGLCOLOR4UBVPROC)load("glColor4ubv");
    glad_glColor4ui = (PFNGLCOLOR4UIPROC)load("glColor4ui");
    glad_glColor4uiv = (PFNGLCOLOR4UIVPROC)load("glColor4uiv");
    glad_glColor4us = (PFNGLCOLOR4USPROC)load("glColor4us");
    glad_glColor4usv = (PFNGLCOLOR4USVPROC)load("glColor4usv");
    glad_glEdgeFlag = (PFNGLEDGEFLAGPROC)load("glEdgeFlag");
    glad_glEdgeFlagv = (PFNGLEDGEFLAGVPROC)load("glEdgeFlagv");
    glad_glEnd = (PFNGLENDPROC)load("glEnd");
    glad_glIndexd = (PFNGLINDEXDPROC)load("glIndexd");
    glad_glIndexdv = (PFNGLINDEXDVPROC)load("glIndexdv");
    glad_glIndexf = (PFNGLINDEXFPROC)load("glIndexf");
    glad_glIndexfv = (PFNGLINDEXFVPROC)load("glIndexfv");
    glad_glIndexi = (PFNGLINDEXIPROC)load("glIndexi");
    glad_glIndexiv = (PFNGLINDEXIVPROC)load("glIndexiv");
    glad_glIndexs = (PFNGLINDEXSPROC)load("glIndexs");
    glad_glIndexsv = (PFNGLINDEXSVPROC)load("glIndexsv");
    glad_glNormal3b = (PFNGLNORMAL3BPROC)load("glNormal3b");
    glad_glNormal3bv = (PFNGLNORMAL3BVPROC)load("glNormal3bv");
    glad_glNormal3d = (PFNGLNORMAL3DPROC)load("glNormal3d");
    glad_glNormal3dv = (PFNGLNORMAL3DVPROC)load("glNormal3dv");
    glad_glNormal3f = (PFNGLNORMAL3FPROC)load("glNormal3f");
    glad_glNormal3fv = (PFNGLNORMAL3FVPROC)load("glNormal3fv");
    glad_glNormal3i = (PFNGLNORMAL3IPROC)load("glNormal3i");
    glad_glNormal3iv = (PFNGLNORMAL3IVPROC)load("glNormal3iv");
    glad_glNormal3s = (PFNGLNORMAL3SPROC)load("glNormal3s");
    glad_glNormal3sv = (PFNGLNORMAL3SVPROC)load("glNormal3sv");
    glad_glRasterPos2d = (PFNGLRASTERPOS2DPROC)load("glRasterPos2d");
    glad_glRasterPos2dv = (PFNGLRASTERPOS2DVPROC)load("glRasterPos2dv");
    glad_glRasterPos2f = (PFNGLRASTERPOS2FPROC)load("glRasterPos2f");
    glad_glRasterPos2fv = (PFNGLRASTERPOS2FVPROC)load("glRasterPos2fv");
    glad_glRasterPos2i = (PFNGLRASTERPOS2IPROC)load("glRasterPos2i");
    glad_glRasterPos2iv = (PFNGLRASTERPOS2IVPROC)load("glRasterPos2iv");
    glad_glRasterPos2s = (PFNGLRASTERPOS2SPROC)load("glRasterPos2s");
    glad_glRasterPos2sv = (PFNGLRASTERPOS2SVPROC)load("glRasterPos2sv");
    glad_glRasterPos3d = (PFNGLRASTERPOS3DPROC)load("glRasterPos3d");
    glad_glRasterPos3dv = (PFNGLRASTERPOS3DVPROC)load("glRasterPos3dv");
    glad_glRasterPos3f = (PFNGLRASTERPOS3FPROC)load("glRasterPos3f");
    glad_glRasterPos3fv = (PFNGLRASTERPOS3FVPROC)load("glRasterPos3fv");
    glad_glRasterPos3i = (PFNGLRASTERPOS3IPROC)load("glRasterPos3i");
    glad_glRasterPos3iv = (PFNGLRASTERPOS3IVPROC)load("glRasterPos3iv");
    glad_glRasterPos3s = (PFNGLRASTERPOS3SPROC)load("glRasterPos3s");
    glad_glRasterPos3sv = (PFNGLRASTERPOS3SVPROC)load("glRasterPos3sv");
    glad_glRasterPos4d = (PFNGLRASTERPOS4DPROC)load("glRasterPos4d");
    glad_glRasterPos4dv = (PFNGLRASTERPOS4DVPROC)load("glRasterPos4dv");
    glad_glRasterPos4f = (PFNGLRASTERPOS4FPROC)load("glRasterPos4f");
    glad_glRasterPos4fv = (PFNGLRASTERPOS4FVPROC)load("glRasterPos4fv");
    glad_glRasterPos4i = (PFNGLRASTERPOS4IPROC)load("glRasterPos4i");
    glad_glRasterPos4iv = (PFNGLRASTERPOS4IVPROC)load("glRasterPos4iv");
    glad_glRasterPos4s = (PFNGLRASTERPOS4SPROC)load("glRasterPos4s");
    glad_glRasterPos4sv = (PFNGLRASTERPOS4SVPROC)load("glRasterPos4sv");
    glad_glRectd = (PFNGLRECTDPROC)load("glRectd");
    glad_glRectdv = (PFNGLRECTDVPROC)load("glRectdv");
    glad_glRectf = (PFNGLRECTFPROC)load("glRectf");
    glad_glRectfv = (PFNGLRECTFVPROC)load("glRectfv");
    glad_glRecti = (PFNGLRECTIPROC)load("glRecti");
    glad_glRectiv = (PFNGLRECTIVPROC)load("glRectiv");
    glad_glRects = (PFNGLRECTSPROC)load("glRects");
    glad_glRectsv = (PFNGLRECTSVPROC)load("glRectsv");
    glad_glTexCoord1d = (PFNGLTEXCOORD1DPROC)load("glTexCoord1d");
    glad_glTexCoord1dv = (PFNGLTEXCOORD1DVPROC)load("glTexCoord1dv");
    glad_glTexCoord1f = (PFNGLTEXCOORD1FPROC)load("glTexCoord1f");
    glad_glTexCoord1fv = (PFNGLTEXCOORD1FVPROC)load("glTexCoord1fv");
    glad_glTexCoord1i = (PFNGLTEXCOORD1IPROC)load("glTexCoord1i");
    glad_glTexCoord1iv = (PFNGLTEXCOORD1IVPROC)load("glTexCoord1iv");
    glad_glTexCoord1s = (PFNGLTEXCOORD1SPROC)load("glTexCoord1s");
    glad_glTexCoord1sv = (PFNGLTEXCOORD1SVPROC)load("glTexCoord1sv");
    glad_glTexCoord2d = (PFNGLTEXCOORD2DPROC)load("glTexCoord2d");
    glad_glTexCoord2dv = (PFNGLTEXCOORD2DVPROC)load("glTexCoord2dv");
    glad_glTexCoord2f = (PFNGLTEXCOORD2FPROC)load("glTexCoord2f");
    glad_glTexCoord2fv = (PFNGLTEXCOORD2FVPROC)load("glTexCoord2fv");
    glad_glTexCoord2i = (PFNGLTEXCOORD2IPROC)load("glTexCoord2i");
    glad_glTexCoord2iv = (PFNGLTEXCOORD2IVPROC)load("glTexCoord2iv");
    glad_glTexCoord2s = (PFNGLTEXCOORD2SPROC)load("glTexCoord2s");
    glad_glTexCoord2sv = (PFNGLTEXCOORD2SVPROC)load("glTexCoord2sv");
    glad_glTexCoord3d = (PFNGLTEXCOORD3DPROC)load("glTexCoord3d");
    glad_glTexCoord3dv = (PFNGLTEXCOORD3DVPROC)load("glTexCoord3dv");
    glad_glTexCoord3f = (PFNGLTEXCOORD3FPROC)load("glTexCoord3f");
    glad_glTexCoord3fv = (PFNGLTEXCOORD3FVPROC)load("glTexCoord3fv");
    glad_glTexCoord3i = (PFNGLTEXCOORD3IPROC)load("glTexCoord3i");
    glad_glTexCoord3iv = (PFNGLTEXCOORD3IVPROC)load("glTexCoord3iv");
    glad_glTexCoord3s = (PFNGLTEXCOORD3SPROC)load("glTexCoord3s");
    glad_glTexCoord3sv = (PFNGLTEXCOORD3SVPROC)load("glTexCoord3sv");
    glad_glTexCoord4d = (PFNGLTEXCOORD4DPROC)load("glTexCoord4d");
    glad_glTexCoord4dv = (PFNGLTEXCOORD4DVPROC)load("glTexCoord4dv");
    glad_glTexCoord4f = (PFNGLTEXCOORD4FPROC)load("glTexCoord4f");
    glad_glTexCoord4fv = (PFNGLTEXCOORD4FVPROC)load("glTexCoord4fv");
    glad_glTexCoord4i = (PFNGLTEXCOORD4IPROC)load("glTexCoord4i");
    glad_glTexCoord4iv = (PFNGLTEXCOORD4IVPROC)load("glTexCoord4iv");
    glad_glTexCoord4s = (PFNGLTEXCOORD4SPROC)load("glTexCoord4s");
    glad_glTexCoord4sv = (PFNGLTEXCOORD4SVPROC)load("glTexCoord4sv");
    glad_glVertex2d = (PFNGLVERTEX2DPROC)load("glVertex2d");
    glad_glVertex2dv = (PFNGLVERTEX2DVPROC)load("glVertex2dv");
    glad_glVertex2f = (PFNGLVERTEX2FPROC)load("glVertex2f");
    glad_glVertex2fv = (PFNGLVERTEX2FVPROC)load("glVertex2fv");
    glad_glVertex2i = (PFNGLVERTEX2IPROC)load("glVertex2i");
    glad_glVertex2iv = (PFNGLVERTEX2IVPROC)load("glVertex2iv");
    glad_glVertex2s = (PFNGLVERTEX2SPROC)load("glVertex2s");
    glad_glVertex2sv = (PFNGLVERTEX2SVPROC)load("glVertex2sv");
    glad_glVertex3d = (PFNGLVERTEX3DPROC)load("glVertex3d");
    glad_glVertex3dv = (PFNGLVERTEX3DVPROC)load("glVertex3dv");
    glad_glVertex3f = (PFNGLVERTEX3FPROC)load("glVertex3f");
    glad_glVertex3fv = (PFNGLVERTEX3FVPROC)load("glVertex3fv");
    glad_glVertex3i = (PFNGLVERTEX3IPROC)load("glVertex3i");
    glad_glVertex3iv = (PFNGLVERTEX3IVPROC)load("glVertex3iv");
    glad_glVertex3s = (PFNGLVERTEX3SPROC)load("glVertex3s");
    glad_glVertex3sv = (PFNGLVERTEX3SVPROC)load("glVertex3sv");
    glad_glVertex4d = (PFNGLVERTEX4DPROC)load("glVertex4d");
    glad_glVertex4dv = (PFNGLVERTEX4DVPROC)load("glVertex4dv");
    glad_glVertex4f = (PFNGLVERTEX4FPROC)load("glVertex4f");
    glad_glVertex4fv = (PFNGLVERTEX4FVPROC)load("glVertex4fv");
    glad_glVertex4i = (PFNGLVERTEX4IPROC)load("glVertex4i");
    glad_glVertex4iv = (PFNGLVERTEX4IVPROC)load("glVertex4iv");
    glad_glVertex4s = (PFNGLVERTEX4SPROC)load("glVertex4s");
    glad_glVertex4sv = (PFNGLVERTEX4SVPROC)load("glVertex4sv");
    glad_glClipPlane = (PFNGLCLIPPLANEPROC)load("glClipPlane");
    glad_glColorMaterial = (PFNGLCOLORMATERIALPROC)load("glColorMaterial");
    glad_glFogf = (PFNGLFOGFPROC)load("glFogf");
    glad_glFogfv = (PFNGLFOGFVPROC)load("glFogfv");
    glad_glFogi = (PFNGLFOGIPROC)load("glFogi");
    glad_glFogiv = (PFNGLFOGIVPROC)load("glFogiv");
    glad_glLightf = (PFNGLLIGHTFPROC)load("glLightf");
    glad_glLightfv = (PFNGLLIGHTFVPROC)load("glLightfv");
    glad_glLighti = (PFNGLLIGHTIPROC)load("glLighti");
    glad_glLightiv = (PFNGLLIGHTIVPROC)load("glLightiv");
    glad_glLightModelf = (PFNGLLIGHTMODELFPROC)load("glLightModelf");
    glad_glLightModelfv = (PFNGLLIGHTMODELFVPROC)load("glLightModelfv");
    glad_glLightModeli = (PFNGLLIGHTMODELIPROC)load("glLightModeli");
    glad_glLightModeliv = (PFNGLLIGHTMODELIVPROC)load("glLightModeliv");
    glad_glLineStipple = (PFNGLLINESTIPPLEPROC)load("glLineStipple");
    glad_glMaterialf = (PFNGLMATERIALFPROC)load("glMaterialf");
    glad_glMaterialfv = (PFNGLMATERIALFVPROC)load("glMaterialfv");
    glad_glMateriali = (PFNGLMATERIALIPROC)load("glMateriali");
    glad_glMaterialiv = (PFNGLMATERIALIVPROC)load("glMaterialiv");
    glad_glPolygonStipple = (PFNGLPOLYGONSTIPPLEPROC)load("glPolygonStipple");
    glad_glShadeModel = (PFNGLSHADEMODELPROC)load("glShadeModel");
    glad_glTexEnvf = (PFNGLTEXENVFPROC)load("glTexEnvf");
    glad_glTexEnvfv = (PFNGLTEXENVFVPROC)load("glTexEnvfv");
    glad_glTexEnvi = (PFNGLTEXENVIPROC)load("glTexEnvi");
    glad_glTexEnviv = (PFNGLTEXENVIVPROC)load("glTexEnviv");
    glad_glTexGend = (PFNGLTEXGENDPROC)load("glTexGend");
    glad_glTexGendv = (PFNGLTEXGENDVPROC)load("glTexGendv");
    glad_glTexGenf = (PFNGLTEXGENFPROC)load("glTexGenf");
    glad_glTexGenfv = (PFNGLTEXGENFVPROC)load("glTexGenfv");
    glad_glTexGeni = (PFNGLTEXGENIPROC)load("glTexGeni");
    glad_glTexGeniv = (PFNGLTEXGENIVPROC)load("glTexGeniv");
    glad_glFeedbackBuffer = (PFNGLFEEDBACKBUFFERPROC)load("glFeedbackBuffer");
    glad_glSelectBuffer = (PFNGLSELECTBUFFERPROC)load("glSelectBuffer");
    glad_glRenderMode = (PFNGLRENDERMODEPROC)load("glRenderMode");
    glad_glInitNames = (PFNGLINITNAMESPROC)load("glInitNames");
    glad_glLoadName = (PFNGLLOADNAMEPROC)load("glLoadName");
    glad_glPassThrough = (PFNGLPASSTHROUGHPROC)load("glPassThrough");
    glad_glPopName = (PFNGLPOPNAMEPROC)load("glPopName");
    glad_glPushName = (PFNGLPUSHNAMEPROC)load("glPushName");
    glad_glClearAccum = (PFNGLCLEARACCUMPROC)load("glClearAccum");
    glad_glClearIndex = (PFNGLCLEARINDEXPROC)load("glClearIndex");
    glad_glIndexMask = (PFNGLINDEXMASKPROC)load("glIndexMask");
    glad_glAccum = (PFNGLACCUMPROC)load("glAccum");
    glad_glPopAttrib = (PFNGLPOPATTRIBPROC)load("glPopAttrib");
    glad_glPushAttrib = (PFNGLPUSHATTRIBPROC)load("glPushAttrib");
    glad_glMap1d = (PFNGLMAP1DPROC)load("glMap1d");
    glad_glMap1f = (PFNGLMAP1FPROC)load("glMap1f");
    glad_glMap2d = (PFNGLMAP2DPROC)load("glMap2d");
    glad_glMap2f = (PFNGLMAP2FPROC)load("glMap2f");
    glad_glMapGrid1d = (PFNGLMAPGRID1DPROC)load("glMapGrid1d");
    glad_glMapGrid1f = (PFNGLMAPGRID1FPROC)load("glMapGrid1f");
    glad_glMapGrid2d = (PFNGLMAPGRID2DPROC)load("glMapGrid2d");
    glad_glMapGrid2f = (PFNGLMAPGRID2FPROC)load("glMapGrid2f");
    glad_glEvalCoord1d = (PFNGLEVALCOORD1DPROC)load("glEvalCoord1d");
    glad_glEvalCoord1dv = (PFNGLEVALCOORD1DVPROC)load("glEvalCoord1dv");
    glad_glEvalCoord1f = (PFNGLEVALCOORD1FPROC)load("glEvalCoord1f");
    glad_glEvalCoord1fv = (PFNGLEVALCOORD1FVPROC)load("glEvalCoord1fv");
    glad_glEvalCoord2d = (PFNGLEVALCOORD2DPROC)load("glEvalCoord2d");
    glad_glEvalCoord2dv = (PFNGLEVALCOORD2DVPROC)load("glEvalCoord2dv");
    glad_glEvalCoord2f = (PFNGLEVALCOORD2FPROC)load("glEvalCoord2f");
    glad_glEvalCoord2fv = (PFNGLEVALCOORD2FVPROC)load("glEvalCoord2fv");
    glad_glEvalMesh1 = (PFNGLEVALMESH1PROC)load("glEvalMesh1");
    glad_glEvalPoint1 = (PFNGLEVALPOINT1PROC)load("glEvalPoint1");
    glad_glEvalMesh2 = (PFNGLEVALMESH2PROC)load("glEvalMesh2");
    glad_glEvalPoint2 = (PFNGLEVALPOINT2PROC)load("glEvalPoint2");
    glad_glAlphaFunc = (PFNGLALPHAFUNCPROC)load("glAlphaFunc");
    glad_glPixelZoom = (PFNGLPIXELZOOMPROC)load("glPixelZoom");
    glad_glPixelTransferf = (PFNGLPIXELTRANSFERFPROC)load("glPixelTransferf");
    glad_glPixelTransferi = (PFNGLPIXELTRANSFERIPROC)load("glPixelTransferi");
    glad_glPixelMapfv = (PFNGLPIXELMAPFVPROC)load("glPixelMapfv");
    glad_glPixelMapuiv = (PFNGLPIXELMAPUIVPROC)load("glPixelMapuiv");
    glad_glPixelMapusv = (PFNGLPIXELMAPUSVPROC)load("glPixelMapusv");
    glad_glCopyPixels = (PFNGLCOPYPIXELSPROC)load("glCopyPixels");
    glad_glDrawPixels = (PFNGLDRAWPIXELSPROC)load("glDrawPixels");
    glad_glGetClipPlane = (PFNGLGETCLIPPLANEPROC)load("glGetClipPlane");
    glad_glGetLightfv = (PFNGLGETLIGHTFVPROC)load("glGetLightfv");
    glad_glGetLightiv = (PFNGLGETLIGHTIVPROC)load("glGetLightiv");
    glad_glGetMapdv = (PFNGLGETMAPDVPROC)load("glGetMapdv");
    glad_glGetMapfv = (PFNGLGETMAPFVPROC)load("glGetMapfv");
    glad_glGetMapiv = (PFNGLGETMAPIVPROC)load("glGetMapiv");
    glad_glGetMaterialfv = (PFNGLGETMATERIALFVPROC)load("glGetMaterialfv");
    glad_glGetMaterialiv = (PFNGLGETMATERIALIVPROC)load("glGetMaterialiv");
    glad_glGetPixelMapfv = (PFNGLGETPIXELMAPFVPROC)load("glGetPixelMapfv");
    glad_glGetPixelMapuiv = (PFNGLGETPIXELMAPUIVPROC)load("glGetPixelMapuiv");
    glad_glGetPixelMapusv = (PFNGLGETPIXELMAPUSVPROC)load("glGetPixelMapusv");
    glad_glGetPolygonStipple = (PFNGLGETPOLYGONSTIPPLEPROC)load("glGetPolygonStipple");
    glad_glGetTexEnvfv = (PFNGLGETTEXENVFVPROC)load("glGetTexEnvfv");
    glad_glGetTexEnviv = (PFNGLGETTEXENVIVPROC)load("glGetTexEnviv");
    glad_glGetTexGendv = (PFNGLGETTEXGENDVPROC)load("glGetTexGendv");
    glad_glGetTexGenfv = (PFNGLGETTEXGENFVPROC)load("glGetTexGenfv");
    glad_glGetTexGeniv = (PFNGLGETTEXGENIVPROC)load("glGetTexGeniv");
    glad_glIsList = (PFNGLISLISTPROC)load("glIsList");
    glad_glFrustum = (PFNGLFRUSTUMPROC)load("glFrustum");
    glad_glLoadIdentity = (PFNGLLOADIDENTITYPROC)load("glLoadIdentity");
    glad_glLoadMatrixf = (PFNGLLOADMATRIXFPROC)load("glLoadMatrixf");
    glad_glLoadMatrixd = (PFNGLLOADMATRIXDPROC)load("glLoadMatrixd");
    glad_glMatrixMode = (PFNGLMATRIXMODEPROC)load("glMatrixMode");
    glad_glMultMatrixf = (PFNGLMULTMATRIXFPROC)load("glMultMatrixf");
    glad_glMultMatrixd = (PFNGLMULTMATRIXDPROC)load("glMultMatrixd");
    glad_glOrtho = (PFNGLORTHOPROC)load("glOrtho");
    glad_glPopMatrix = (PFNGLPOPMATRIXPROC)load("glPopMatrix");
    glad_glPushMatrix = (PFNGLPUSHMATRIXPROC)load("glPushMatrix");
    glad_glRotated = (PFNGLROTATEDPROC)load("glRotated");
    glad_glRotatef = (PFNGLROTATEFPROC)load("glRotatef");
    glad_glScaled = (PFNGLSCALEDPROC)load("glScaled");
    glad_glScalef = (PFNGLSCALEFPROC)load("glScalef");
    glad_glTranslated = (PFNGLTRANSLATEDPROC)load("glTranslated");
    glad_glTranslatef = (PFNGLTRANSLATEFPROC)load("glTranslatef");
}
static void load_GL_VERSION_1_1(GLADloadproc load) {
    if (!GLAD_GL_VERSION_1_1) return;
    glad_glDrawArrays = (PFNGLDRAWARRAYSPROC)load("glDrawArrays");
    glad_glDrawElements = (PFNGLDRAWELEMENTSPROC)load("glDrawElements");
    glad_glGetPointerv = (PFNGLGETPOINTERVPROC)load("glGetPointerv");
    glad_glPolygonOffset = (PFNGLPOLYGONOFFSETPROC)load("glPolygonOffset");
    glad_glCopyTexImage1D = (PFNGLCOPYTEXIMAGE1DPROC)load("glCopyTexImage1D");
    glad_glCopyTexImage2D = (PFNGLCOPYTEXIMAGE2DPROC)load("glCopyTexImage2D");
    glad_glCopyTexSubImage1D = (PFNGLCOPYTEXSUBIMAGE1DPROC)load("glCopyTexSubImage1D");
    glad_glCopyTexSubImage2D = (PFNGLCOPYTEXSUBIMAGE2DPROC)load("glCopyTexSubImage2D");
    glad_glTexSubImage1D = (PFNGLTEXSUBIMAGE1DPROC)load("glTexSubImage1D");
    glad_glTexSubImage2D = (PFNGLTEXSUBIMAGE2DPROC)load("glTexSubImage2D");
    glad_glBindTexture = (PFNGLBINDTEXTUREPROC)load("glBindTexture");
    glad_glDeleteTextures = (PFNGLDELETETEXTURESPROC)load("glDeleteTextures");
    glad_glGenTextures = (PFNGLGENTEXTURESPROC)load("glGenTextures");
    glad_glIsTexture = (PFNGLISTEXTUREPROC)load("glIsTexture");
    glad_glArrayElement = (PFNGLARRAYELEMENTPROC)load("glArrayElement");
    glad_glColorPointer = (PFNGLCOLORPOINTERPROC)load("glColorPointer");
    glad_glDisableClientState = (PFNGLDISABLECLIENTSTATEPROC)load("glDisableClientState");
    glad_glEdgeFlagPointer = (PFNGLEDGEFLAGPOINTERPROC)load("glEdgeFlagPointer");
    glad_glEnableClientState = (PFNGLENABLECLIENTSTATEPROC)load("glEnableClientState");
    glad_glIndexPointer = (PFNGLINDEXPOINTERPROC)load("glIndexPointer");
    glad_glInterleavedArrays = (PFNGLINTERLEAVEDARRAYSPROC)load("glInterleavedArrays");
    glad_glNormalPointer = (PFNGLNORMALPOINTERPROC)load("glNormalPointer");
    glad_glTexCoordPointer = (PFNGLTEXCOORDPOINTERPROC)load("glTexCoordPointer");
    glad_glVertexPointer = (PFNGLVERTEXPOINTERPROC)load("glVertexPointer");
    glad_glAreTexturesResident = (PFNGLARETEXTURESRESIDENTPROC)load("glAreTexturesResident");
    glad_glPrioritizeTextures = (PFNGLPRIORITIZETEXTURESPROC)load("glPrioritizeTextures");
    glad_glIndexub = (PFNGLINDEXUBPROC)load("glIndexub");
    glad_glIndexubv = (PFNGLINDEXUBVPROC)load("glIndexubv");
    glad_glPopClientAttrib = (PFNGLPOPCLIENTATTRIBPROC)load("glPopClientAttrib");
    glad_glPushClientAttrib = (PFNGLPUSHCLIENTATTRIBPROC)load("glPushClientAttrib");
}
static void load_GL_VERSION_1_2(GLADloadproc load) {
    if (!GLAD_GL_VERSION_1_2) return;
    glad_glDrawRangeElements = (PFNGLDRAWRANGEELEMENTSPROC)load("glDrawRangeElements");
    glad_glTexImage3D = (PFNGLTEXIMAGE3DPROC)load("glTexImage3D");
    glad_glTexSubImage3D = (PFNGLTEXSUBIMAGE3DPROC)load("glTexSubImage3D");
    glad_glCopyTexSubImage3D = (PFNGLCOPYTEXSUBIMAGE3DPROC)load("glCopyTexSubImage3D");
}
static void load_GL_VERSION_1_3(GLADloadproc load) {
    if (!GLAD_GL_VERSION_1_3) return;
    glad_glActiveTexture = (PFNGLACTIVETEXTUREPROC)load("glActiveTexture");
    glad_glSampleCoverage = (PFNGLSAMPLECOVERAGEPROC)load("glSampleCoverage");
    glad_glCompressedTexImage3D = (PFNGLCOMPRESSEDTEXIMAGE3DPROC)load("glCompressedTexImage3D");
    glad_glCompressedTexImage2D = (PFNGLCOMPRESSEDTEXIMAGE2DPROC)load("glCompressedTexImage2D");
    glad_glCompressedTexImage1D = (PFNGLCOMPRESSEDTEXIMAGE1DPROC)load("glCompressedTexImage1D");
    glad_glCompressedTexSubImage3D = (PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC)load("glCompressedTexSubImage3D");
    glad_glCompressedTexSubImage2D = (PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC)load("glCompressedTexSubImage2D");
    glad_glCompressedTexSubImage1D = (PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC)load("glCompressedTexSubImage1D");
    glad_glGetCompressedTexImage = (PFNGLGETCOMPRESSEDTEXIMAGEPROC)load("glGetCompressedTexImage");
    glad_glClientActiveTexture = (PFNGLCLIENTACTIVETEXTUREPROC)load("glClientActiveTexture");
    glad_glMultiTexCoord1d = (PFNGLMULTITEXCOORD1DPROC)load("glMultiTexCoord1d");
    glad_glMultiTexCoord1dv = (PFNGLMULTITEXCOORD1DVPROC)load("glMultiTexCoord1dv");
    glad_glMultiTexCoord1f = (PFNGLMULTITEXCOORD1FPROC)load("glMultiTexCoord1f");
    glad_glMultiTexCoord1fv = (PFNGLMULTITEXCOORD1FVPROC)load("glMultiTexCoord1fv");
    glad_glMultiTexCoord1i = (PFNGLMULTITEXCOORD1IPROC)load("glMultiTexCoord1i");
    glad_glMultiTexCoord1iv = (PFNGLMULTITEXCOORD1IVPROC)load("glMultiTexCoord1iv");
    glad_glMultiTexCoord1s = (PFNGLMULTITEXCOORD1SPROC)load("glMultiTexCoord1s");
    glad_glMultiTexCoord1sv = (PFNGLMULTITEXCOORD1SVPROC)load("glMultiTexCoord1sv");
    glad_glMultiTexCoord2d = (PFNGLMULTITEXCOORD2DPROC)load("glMultiTexCoord2d");
    glad_glMultiTexCoord2dv = (PFNGLMULTITEXCOORD2DVPROC)load("glMultiTexCoord2dv");
    glad_glMultiTexCoord2f = (PFNGLMULTITEXCOORD2FPROC)load("glMultiTexCoord2f");
    glad_glMultiTexCoord2fv = (PFNGLMULTITEXCOORD2FVPROC)load("glMultiTexCoord2fv");
    glad_glMultiTexCoord2i = (PFNGLMULTITEXCOORD2IPROC)load("glMultiTexCoord2i");
    glad_glMultiTexCoord2iv = (PFNGLMULTITEXCOORD2IVPROC)load("glMultiTexCoord2iv");
    glad_glMultiTexCoord2s = (PFNGLMULTITEXCOORD2SPROC)load("glMultiTexCoord2s");
    glad_glMultiTexCoord2sv = (PFNGLMULTITEXCOORD2SVPROC)load("glMultiTexCoord2sv");
    glad_glMultiTexCoord3d = (PFNGLMULTITEXCOORD3DPROC)load("glMultiTexCoord3d");
    glad_glMultiTexCoord3dv = (PFNGLMULTITEXCOORD3DVPROC)load("glMultiTexCoord3dv");
    glad_glMultiTexCoord3f = (PFNGLMULTITEXCOORD3FPROC)load("glMultiTexCoord3f");
    glad_glMultiTexCoord3fv = (PFNGLMULTITEXCOORD3FVPROC)load("glMultiTexCoord3fv");
    glad_glMultiTexCoord3i = (PFNGLMULTITEXCOORD3IPROC)load("glMultiTexCoord3i");
    glad_glMultiTexCoord3iv = (PFNGLMULTITEXCOORD3IVPROC)load("glMultiTexCoord3iv");
    glad_glMultiTexCoord3s = (PFNGLMULTITEXCOORD3SPROC)load("glMultiTexCoord3s");
    glad_glMultiTexCoord3sv = (PFNGLMULTITEXCOORD3SVPROC)load("glMultiTexCoord3sv");
    glad_glMultiTexCoord4d = (PFNGLMULTITEXCOORD4DPROC)load("glMultiTexCoord4d");
    glad_glMultiTexCoord4dv = (PFNGLMULTITEXCOORD4DVPROC)load("glMultiTexCoord4dv");
    glad_glMultiTexCoord4f = (PFNGLMULTITEXCOORD4FPROC)load("glMultiTexCoord4f");
    glad_glMultiTexCoord4fv = (PFNGLMULTITEXCOORD4FVPROC)load("glMultiTexCoord4fv");
    glad_glMultiTexCoord4i = (PFNGLMULTITEXCOORD4IPROC)load("glMultiTexCoord4i");
    glad_glMultiTexCoord4iv = (PFNGLMULTITEXCOORD4IVPROC)load("glMultiTexCoord4iv");
    glad_glMultiTexCoord4s = (PFNGLMULTITEXCOORD4SPROC)load("glMultiTexCoord4s");
    glad_glMultiTexCoord4sv = (PFNGLMULTITEXCOORD4SVPROC)load("glMultiTexCoord4sv");
    glad_glLoadTransposeMatrixf = (PFNGLLOADTRANSPOSEMATRIXFPROC)load("glLoadTransposeMatrixf");
    glad_glLoadTransposeMatrixd = (PFNGLLOADTRANSPOSEMATRIXDPROC)load("glLoadTransposeMatrixd");
    glad_glMultTransposeMatrixf = (PFNGLMULTTRANSPOSEMATRIXFPROC)load("glMultTransposeMatrixf");
    glad_glMultTransposeMatrixd = (PFNGLMULTTRANSPOSEMATRIXDPROC)load("glMultTransposeMatrixd");
}
static void load_GL_VERSION_1_4(GLADloadproc load) {
    if (!GLAD_GL_VERSION_1_4) return;
    glad_glBlendFuncSeparate = (PFNGLBLENDFUNCSEPARATEPROC)load("glBlendFuncSeparate");
    glad_glMultiDrawArrays = (PFNGLMULTIDRAWARRAYSPROC)load("glMultiDrawArrays");
    glad_glMultiDrawElements = (PFNGLMULTIDRAWELEMENTSPROC)load("glMultiDrawElements");
    glad_glPointParameterf = (PFNGLPOINTPARAMETERFPROC)load("glPointParameterf");
    glad_glPointParameterfv = (PFNGLPOINTPARAMETERFVPROC)load("glPointParameterfv");
    glad_glPointParameteri = (PFNGLPOINTPARAMETERIPROC)load("glPointParameteri");
    glad_glPointParameteriv = (PFNGLPOINTPARAMETERIVPROC)load("glPointParameteriv");
    glad_glFogCoordf = (PFNGLFOGCOORDFPROC)load("glFogCoordf");
    glad_glFogCoordfv = (PFNGLFOGCOORDFVPROC)load("glFogCoordfv");
    glad_glFogCoordd = (PFNGLFOGCOORDDPROC)load("glFogCoordd");
    glad_glFogCoorddv = (PFNGLFOGCOORDDVPROC)load("glFogCoorddv");
    glad_glFogCoordPointer = (PFNGLFOGCOORDPOINTERPROC)load("glFogCoordPointer");
    glad_glSecondaryColor3b = (PFNGLSECONDARYCOLOR3BPROC)load("glSecondaryColor3b");
    glad_glSecondaryColor3bv = (PFNGLSECONDARYCOLOR3BVPROC)load("glSecondaryColor3bv");
    glad_glSecondaryColor3d = (PFNGLSECONDARYCOLOR3DPROC)load("glSecondaryColor3d");
    glad_glSecondaryColor3dv = (PFNGLSECONDARYCOLOR3DVPROC)load("glSecondaryColor3dv");
    glad_glSecondaryColor3f = (PFNGLSECONDARYCOLOR3FPROC)load("glSecondaryColor3f");
    glad_glSecondaryColor3fv = (PFNGLSECONDARYCOLOR3FVPROC)load("glSecondaryColor3fv");
    glad_glSecondaryColor3i = (PFNGLSECONDARYCOLOR3IPROC)load("glSecondaryColor3i");
    glad_glSecondaryColor3iv = (PFNGLSECONDARYCOLOR3IVPROC)load("glSecondaryColor3iv");
    glad_glSecondaryColor3s = (PFNGLSECONDARYCOLOR3SPROC)load("glSecondaryColor3s");
    glad_glSecondaryColor3sv = (PFNGLSECONDARYCOLOR3SVPROC)load("glSecondaryColor3sv");
    glad_glSecondaryColor3ub = (PFNGLSECONDARYCOLOR3UBPROC)load("glSecondaryColor3ub");
    glad_glSecondaryColor3ubv = (PFNGLSECONDARYCOLOR3UBVPROC)load("glSecondaryColor3ubv");
    glad_glSecondaryColor3ui = (PFNGLSECONDARYCOLOR3UIPROC)load("glSecondaryColor3ui");
    glad_glSecondaryColor3uiv = (PFNGLSECONDARYCOLOR3UIVPROC)load("glSecondaryColor3uiv");
    glad_glSecondaryColor3us = (PFNGLSECONDARYCOLOR3USPROC)load("glSecondaryColor3us");
    glad_glSecondaryColor3usv = (PFNGLSECONDARYCOLOR3USVPROC)load("glSecondaryColor3usv");
    glad_glSecondaryColorPointer = (PFNGLSECONDARYCOLORPOINTERPROC)load("glSecondaryColorPointer");
    glad_glWindowPos2d = (PFNGLWINDOWPOS2DPROC)load("glWindowPos2d");
    glad_glWindowPos2dv = (PFNGLWINDOWPOS2DVPROC)load("glWindowPos2dv");
    glad_glWindowPos2f = (PFNGLWINDOWPOS2FPROC)load("glWindowPos2f");
    glad_glWindowPos2fv = (PFNGLWINDOWPOS2FVPROC)load("glWindowPos2fv");
    glad_glWindowPos2i = (PFNGLWINDOWPOS2IPROC)load("glWindowPos2i");
    glad_glWindowPos2iv = (PFNGLWINDOWPOS2IVPROC)load("glWindowPos2iv");
    glad_glWindowPos2s = (PFNGLWINDOWPOS2SPROC)load("glWindowPos2s");
    glad_glWindowPos2sv = (PFNGLWINDOWPOS2SVPROC)load("glWindowPos2sv");
    glad_glWindowPos3d = (PFNGLWINDOWPOS3DPROC)load("glWindowPos3d");
    glad_glWindowPos3dv = (PFNGLWINDOWPOS3DVPROC)load("glWindowPos3dv");
    glad_glWindowPos3f = (PFNGLWINDOWPOS3FPROC)load("glWindowPos3f");
    glad_glWindowPos3fv = (PFNGLWINDOWPOS3FVPROC)load("glWindowPos3fv");
    glad_glWindowPos3i = (PFNGLWINDOWPOS3IPROC)load("glWindowPos3i");
    glad_glWindowPos3iv = (PFNGLWINDOWPOS3IVPROC)load("glWindowPos3iv");
    glad_glWindowPos3s = (PFNGLWINDOWPOS3SPROC)load("glWindowPos3s");
    glad_glWindowPos3sv = (PFNGLWINDOWPOS3SVPROC)load("glWindowPos3sv");
    glad_glBlendColor = (PFNGLBLENDCOLORPROC)load("glBlendColor");
    glad_glBlendEquation = (PFNGLBLENDEQUATIONPROC)load("glBlendEquation");
}
static void load_GL_VERSION_1_5(GLADloadproc load) {
    if (!GLAD_GL_VERSION_1_5) return;
    glad_glGenQueries = (PFNGLGENQUERIESPROC)load("glGenQueries");
    glad_glDeleteQueries = (PFNGLDELETEQUERIESPROC)load("glDeleteQueries");
    glad_glIsQuery = (PFNGLISQUERYPROC)load("glIsQuery");
    glad_glBeginQuery = (PFNGLBEGINQUERYPROC)load("glBeginQuery");
    glad_glEndQuery = (PFNGLENDQUERYPROC)load("glEndQuery");
    glad_glGetQueryiv = (PFNGLGETQUERYIVPROC)load("glGetQueryiv");
    glad_glGetQueryObjectiv = (PFNGLGETQUERYOBJECTIVPROC)load("glGetQueryObjectiv");
    glad_glGetQueryObjectuiv = (PFNGLGETQUERYOBJECTUIVPROC)load("glGetQueryObjectuiv");
    glad_glBindBuffer = (PFNGLBINDBUFFERPROC)load("glBindBuffer");
    glad_glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)load("glDeleteBuffers");
    glad_glGenBuffers = (PFNGLGENBUFFERSPROC)load("glGenBuffers");
    glad_glIsBuffer = (PFNGLISBUFFERPROC)load("glIsBuffer");
    glad_glBufferData = (PFNGLBUFFERDATAPROC)load("glBufferData");
    glad_glBufferSubData = (PFNGLBUFFERSUBDATAPROC)load("glBufferSubData");
    glad_glGetBufferSubData = (PFNGLGETBUFFERSUBDATAPROC)load("glGetBufferSubData");
    glad_glMapBuffer = (PFNGLMAPBUFFERPROC)load("glMapBuffer");
    glad_glUnmapBuffer = (PFNGLUNMAPBUFFERPROC)load("glUnmapBuffer");
    glad_glGetBufferParameteriv = (PFNGLGETBUFFERPARAMETERIVPROC)load("glGetBufferParameteriv");
    glad_glGetBufferPointerv = (PFNGLGETBUFFERPOINTERVPROC)load("glGetBufferPointerv");
}
static void load_GL_VERSION_2_0(GLADloadproc load) {
    if (!GLAD_GL_VERSION_2_0) return;
    glad_glBlendEquationSeparate = (PFNGLBLENDEQUATIONSEPARATEPROC)load("glBlendEquationSeparate");
    glad_glDrawBuffers = (PFNGLDRAWBUFFERSPROC)load("glDrawBuffers");
    glad_glStencilOpSeparate = (PFNGLSTENCILOPSEPARATEPROC)load("glStencilOpSeparate");
    glad_glStencilFuncSeparate = (PFNGLSTENCILFUNCSEPARATEPROC)load("glStencilFuncSeparate");
    glad_glStencilMaskSeparate = (PFNGLSTENCILMASKSEPARATEPROC)load("glStencilMaskSeparate");
    glad_glAttachShader = (PFNGLATTACHSHADERPROC)load("glAttachShader");
    glad_glBindAttribLocation = (PFNGLBINDATTRIBLOCATIONPROC)load("glBindAttribLocation");
    glad_glCompileShader = (PFNGLCOMPILESHADERPROC)load("glCompileShader");
    glad_glCreateProgram = (PFNGLCREATEPROGRAMPROC)load("glCreateProgram");
    glad_glCreateShader = (PFNGLCREATESHADERPROC)load("glCreateShader");
    glad_glDeleteProgram = (PFNGLDELETEPROGRAMPROC)load("glDeleteProgram");
    glad_glDeleteShader = (PFNGLDELETESHADERPROC)load("glDeleteShader");
    glad_glDetachShader = (PFNGLDETACHSHADERPROC)load("glDetachShader");
    glad_glDisableVertexAttribArray = (PFNGLDISABLEVERTEXATTRIBARRAYPROC)load("glDisableVertexAttribArray");
    glad_glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)load("glEnableVertexAttribArray");
    glad_glGetActiveAttrib = (PFNGLGETACTIVEATTRIBPROC)load("glGetActiveAttrib");
    glad_glGetActiveUniform = (PFNGLGETACTIVEUNIFORMPROC)load("glGetActiveUniform");
    glad_glGetAttachedShaders = (PFNGLGETATTACHEDSHADERSPROC)load("glGetAttachedShaders");
    glad_glGetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC)load("glGetAttribLocation");
    glad_glGetProgramiv = (PFNGLGETPROGRAMIVPROC)load("glGetProgramiv");
    glad_glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)load("glGetProgramInfoLog");
    glad_glGetShaderiv = (PFNGLGETSHADERIVPROC)load("glGetShaderiv");
    glad_glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)load("glGetShaderInfoLog");
    glad_glGetShaderSource = (PFNGLGETSHADERSOURCEPROC)load("glGetShaderSource");
    glad_glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)load("glGetUniformLocation");
    glad_glGetUniformfv = (PFNGLGETUNIFORMFVPROC)load("glGetUniformfv");
    glad_glGetUniformiv = (PFNGLGETUNIFORMIVPROC)load("glGetUniformiv");
    glad_glGetVertexAttribdv = (PFNGLGETVERTEXATTRIBDVPROC)load("glGetVertexAttribdv");
    glad_glGetVertexAttribfv = (PFNGLGETVERTEXATTRIBFVPROC)load("glGetVertexAttribfv");
    glad_glGetVertexAttribiv = (PFNGLGETVERTEXATTRIBIVPROC)load("glGetVertexAttribiv");
    glad_glGetVertexAttribPointerv = (PFNGLGETVERTEXATTRIBPOINTERVPROC)load("glGetVertexAttribPointerv");
    glad_glIsProgram = (PFNGLISPROGRAMPROC)load("glIsProgram");
    glad_glIsShader = (PFNGLISSHADERPROC)load("glIsShader");
    glad_glLinkProgram = (PFNGLLINKPROGRAMPROC)load("glLinkProgram");
    glad_glShaderSource = (PFNGLSHADERSOURCEPROC)load("glShaderSource");
    glad_glUseProgram = (PFNGLUSEPROGRAMPROC)load("glUseProgram");
    glad_glUniform1f = (PFNGLUNIFORM1FPROC)load("glUniform1f");
    glad_glUniform2f = (PFNGLUNIFORM2FPROC)load("glUniform2f");
    glad_glUniform3f = (PFNGLUNIFORM3FPROC)load("glUniform3f");
    glad_glUniform4f = (PFNGLUNIFORM4FPROC)load("glUniform4f");
    glad_glUniform1i = (PFNGLUNIFORM1IPROC)load("glUniform1i");
    glad_glUniform2i = (PFNGLUNIFORM2IPROC)load("glUniform2i");
    glad_glUniform3i = (PFNGLUNIFORM3IPROC)load("glUniform3i");
    glad_glUniform4i = (PFNGLUNIFORM4IPROC)load("glUniform4i");
    glad_glUniform1fv = (PFNGLUNIFORM1FVPROC)load("glUniform1fv");
    glad_glUniform2fv = (PFNGLUNIFORM2FVPROC)load("glUniform2fv");
    glad_glUniform3fv = (PFNGLUNIFORM3FVPROC)load("glUniform3fv");
    glad_glUniform4fv = (PFNGLUNIFORM4FVPROC)load("glUniform4fv");
    glad_glUniform1iv = (PFNGLUNIFORM1IVPROC)load("glUniform1iv");
    glad_glUniform2iv = (PFNGLUNIFORM2IVPROC)load("glUniform2iv");
    glad_glUniform3iv = (PFNGLUNIFORM3IVPROC)load("glUniform3iv");
    glad_glUniform4iv = (PFNGLUNIFORM4IVPROC)load("glUniform4iv");
    glad_glUniformMatrix2fv = (PFNGLUNIFORMMATRIX2FVPROC)load("glUniformMatrix2fv");
    glad_glUniformMatrix3fv = (PFNGLUNIFORMMATRIX3FVPROC)load("glUniformMatrix3fv");
    glad_glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC)load("glUniformMatrix4fv");
    glad_glValidateProgram = (PFNGLVALIDATEPROGRAMPROC)load("glValidateProgram");
    glad_glVertexAttrib1d = (PFNGLVERTEXATTRIB1DPROC)load("glVertexAttrib1d");
    glad_glVertexAttrib1dv = (PFNGLVERTEXATTRIB1DVPROC)load("glVertexAttrib1dv");
    glad_glVertexAttrib1f = (PFNGLVERTEXATTRIB1FPROC)load("glVertexAttrib1f");
    glad_glVertexAttrib1fv = (PFNGLVERTEXATTRIB1FVPROC)load("glVertexAttrib1fv");
    glad_glVertexAttrib1s = (PFNGLVERTEXATTRIB1SPROC)load("glVertexAttrib1s");
    glad_glVertexAttrib1sv = (PFNGLVERTEXATTRIB1SVPROC)load("glVertexAttrib1sv");
    glad_glVertexAttrib2d = (PFNGLVERTEXATTRIB2DPROC)load("glVertexAttrib2d");
    glad_glVertexAttrib2dv = (PFNGLVERTEXATTRIB2DVPROC)load("glVertexAttrib2dv");
    glad_glVertexAttrib2f = (PFNGLVERTEXATTRIB2FPROC)load("glVertexAttrib2f");
    glad_glVertexAttrib2fv = (PFNGLVERTEXATTRIB2FVPROC)load("glVertexAttrib2fv");
    glad_glVertexAttrib2s = (PFNGLVERTEXATTRIB2SPROC)load("glVertexAttrib2s");
    glad_glVertexAttrib2sv = (PFNGLVERTEXATTRIB2SVPROC)load("glVertexAttrib2sv");
    glad_glVertexAttrib3d = (PFNGLVERTEXATTRIB3DPROC)load("glVertexAttrib3d");
    glad_glVertexAttrib3dv = (PFNGLVERTEXATTRIB3DVPROC)load("glVertexAttrib3dv");
    glad_glVertexAttrib3f = (PFNGLVERTEXATTRIB3FPROC)load("glVertexAttrib3f");
    glad_glVertexAttrib3fv = (PFNGLVERTEXATTRIB3FVPROC)load("glVertexAttrib3fv");
    glad_glVertexAttrib3s = (PFNGLVERTEXATTRIB3SPROC)load("glVertexAttrib3s");
    glad_glVertexAttrib3sv = (PFNGLVERTEXATTRIB3SVPROC)load("glVertexAttrib3sv");
    glad_glVertexAttrib4Nbv = (PFNGLVERTEXATTRIB4NBVPROC)load("glVertexAttrib4Nbv");
    glad_glVertexAttrib4Niv = (PFNGLVERTEXATTRIB4NIVPROC)load("glVertexAttrib4Niv");
    glad_glVertexAttrib4Nsv = (PFNGLVERTEXATTRIB4NSVPROC)load("glVertexAttrib4Nsv");
    glad_glVertexAttrib4Nub = (PFNGLVERTEXATTRIB4NUBPROC)load("glVertexAttrib4Nub");
    glad_glVertexAttrib4Nubv = (PFNGLVERTEXATTRIB4NUBVPROC)load("glVertexAttrib4Nubv");
    glad_glVertexAttrib4Nuiv = (PFNGLVERTEXATTRIB4NUIVPROC)load("glVertexAttrib4Nuiv");
    glad_glVertexAttrib4Nusv = (PFNGLVERTEXATTRIB4NUSVPROC)load("glVertexAttrib4Nusv");
    glad_glVertexAttrib4bv = (PFNGLVERTEXATTRIB4BVPROC)load("glVertexAttrib4bv");
    glad_glVertexAttrib4d = (PFNGLVERTEXATTRIB4DPROC)load("glVertexAttrib4d");
    glad_glVertexAttrib4dv = (PFNGLVERTEXATTRIB4DVPROC)load("glVertexAttrib4dv");
    glad_glVertexAttrib4f = (PFNGLVERTEXATTRIB4FPROC)load("glVertexAttrib4f");
    glad_glVertexAttrib4fv = (PFNGLVERTEXATTRIB4FVPROC)load("glVertexAttrib4fv");
    glad_glVertexAttrib4iv = (PFNGLVERTEXATTRIB4IVPROC)load("glVertexAttrib4iv");
    glad_glVertexAttrib4s = (PFNGLVERTEXATTRIB4SPROC)load("glVertexAttrib4s");
    glad_glVertexAttrib4sv = (PFNGLVERTEXATTRIB4SVPROC)load("glVertexAttrib4sv");
    glad_glVertexAttrib4ubv = (PFNGLVERTEXATTRIB4UBVPROC)load("glVertexAttrib4ubv");
    glad_glVertexAttrib4uiv = (PFNGLVERTEXATTRIB4UIVPROC)load("glVertexAttrib4uiv");
    glad_glVertexAttrib4usv = (PFNGLVERTEXATTRIB4USVPROC)load("glVertexAttrib4usv");
    glad_glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)load("glVertexAttribPointer");
}
static void load_GL_VERSION_2_1(GLADloadproc load) {
    if (!GLAD_GL_VERSION_2_1) return;
    glad_glUniformMatrix2x3fv = (PFNGLUNIFORMMATRIX2X3FVPROC)load("glUniformMatrix2x3fv");
    glad_glUniformMatrix3x2fv = (PFNGLUNIFORMMATRIX3X2FVPROC)load("glUniformMatrix3x2fv");
    glad_glUniformMatrix2x4fv = (PFNGLUNIFORMMATRIX2X4FVPROC)load("glUniformMatrix2x4fv");
    glad_glUniformMatrix4x2fv = (PFNGLUNIFORMMATRIX4X2FVPROC)load("glUniformMatrix4x2fv");
    glad_glUniformMatrix3x4fv = (PFNGLUNIFORMMATRIX3X4FVPROC)load("glUniformMatrix3x4fv");
    glad_glUniformMatrix4x3fv = (PFNGLUNIFORMMATRIX4X3FVPROC)load("glUniformMatrix4x3fv");
}
static void load_GL_VERSION_3_0(GLADloadproc load) {
    if (!GLAD_GL_VERSION_3_0) return;
    glad_glColorMaski = (PFNGLCOLORMASKIPROC)load("glColorMaski");
    glad_glGetBooleani_v = (PFNGLGETBOOLEANI_VPROC)load("glGetBooleani_v");
    glad_glGetIntegeri_v = (PFNGLGETINTEGERI_VPROC)load("glGetIntegeri_v");
    glad_glEnablei = (PFNGLENABLEIPROC)load("glEnablei");
    glad_glDisablei = (PFNGLDISABLEIPROC)load("glDisablei");
    glad_glIsEnabledi = (PFNGLISENABLEDIPROC)load("glIsEnabledi");
    glad_glBeginTransformFeedback = (PFNGLBEGINTRANSFORMFEEDBACKPROC)load("glBeginTransformFeedback");
    glad_glEndTransformFeedback = (PFNGLENDTRANSFORMFEEDBACKPROC)load("glEndTransformFeedback");
    glad_glBindBufferRange = (PFNGLBINDBUFFERRANGEPROC)load("glBindBufferRange");
    glad_glBindBufferBase = (PFNGLBINDBUFFERBASEPROC)load("glBindBufferBase");
    glad_glTransformFeedbackVaryings = (PFNGLTRANSFORMFEEDBACKVARYINGSPROC)load("glTransformFeedbackVaryings");
    glad_glGetTransformFeedbackVarying = (PFNGLGETTRANSFORMFEEDBACKVARYINGPROC)load("glGetTransformFeedbackVarying");
    glad_glClampColor = (PFNGLCLAMPCOLORPROC)load("glClampColor");
    glad_glBeginConditionalRender = (PFNGLBEGINCONDITIONALRENDERPROC)load("glBeginConditionalRender");
    glad_glEndConditionalRender = (PFNGLENDCONDITIONALRENDERPROC)load("glEndConditionalRender");
    glad_glVertexAttribIPointer = (PFNGLVERTEXATTRIBIPOINTERPROC)load("glVertexAttribIPointer");
    glad_glGetVertexAttribIiv = (PFNGLGETVERTEXATTRIBIIVPROC)load("glGetVertexAttribIiv");
    glad_glGetVertexAttribIuiv = (PFNGLGETVERTEXATTRIBIUIVPROC)load("glGetVertexAttribIuiv");
    glad_glVertexAttribI1i = (PFNGLVERTEXATTRIBI1IPROC)load("glVertexAttribI1i");
    glad_glVertexAttribI2i = (PFNGLVERTEXATTRIBI2IPROC)load("glVertexAttribI2i");
    glad_glVertexAttribI3i = (PFNGLVERTEXATTRIBI3IPROC)load("glVertexAttribI3i");
    glad_glVertexAttribI4i = (PFNGLVERTEXATTRIBI4IPROC)load("glVertexAttribI4i");
    glad_glVertexAttribI1ui = (PFNGLVERTEXATTRIBI1UIPROC)load("glVertexAttribI1ui");
    glad_glVertexAttribI2ui = (PFNGLVERTEXATTRIBI2UIPROC)load("glVertexAttribI2ui");
    glad_glVertexAttribI3ui = (PFNGLVERTEXATTRIBI3UIPROC)load("glVertexAttribI3ui");
    glad_glVertexAttribI4ui = (PFNGLVERTEXATTRIBI4UIPROC)load("glVertexAttribI4ui");
    glad_glVertexAttribI1iv = (PFNGLVERTEXATTRIBI1IVPROC)load("glVertexAttribI1iv");
    glad_glVertexAttribI2iv = (PFNGLVERTEXATTRIBI2IVPROC)load("glVertexAttribI2iv");
    glad_glVertexAttribI3iv = (PFNGLVERTEXATTRIBI3IVPROC)load("glVertexAttribI3iv");
    glad_glVertexAttribI4iv = (PFNGLVERTEXATTRIBI4IVPROC)load("glVertexAttribI4iv");
    glad_glVertexAttribI1uiv = (PFNGLVERTEXATTRIBI1UIVPROC)load("glVertexAttribI1uiv");
    glad_glVertexAttribI2uiv = (PFNGLVERTEXATTRIBI2UIVPROC)load("glVertexAttribI2uiv");
    glad_glVertexAttribI3uiv = (PFNGLVERTEXATTRIBI3UIVPROC)load("glVertexAttribI3uiv");
    glad_glVertexAttribI4uiv = (PFNGLVERTEXATTRIBI4UIVPROC)load("glVertexAttribI4uiv");
    glad_glVertexAttribI4bv = (PFNGLVERTEXATTRIBI4BVPROC)load("glVertexAttribI4bv");
    glad_glVertexAttribI4sv = (PFNGLVERTEXATTRIBI4SVPROC)load("glVertexAttribI4sv");
    glad_glVertexAttribI4ubv = (PFNGLVERTEXATTRIBI4UBVPROC)load("glVertexAttribI4ubv");
    glad_glVertexAttribI4usv = (PFNGLVERTEXATTRIBI4USVPROC)load("glVertexAttribI4usv");
    glad_glGetUniformuiv = (PFNGLGETUNIFORMUIVPROC)load("glGetUniformuiv");
    glad_glBindFragDataLocation = (PFNGLBINDFRAGDATALOCATIONPROC)load("glBindFragDataLocation");
    glad_glGetFragDataLocation = (PFNGLGETFRAGDATALOCATIONPROC)load("glGetFragDataLocation");
    glad_glUniform1ui = (PFNGLUNIFORM1UIPROC)load("glUniform1ui");
    glad_glUniform2ui = (PFNGLUNIFORM2UIPROC)load("glUniform2ui");
    glad_glUniform3ui = (PFNGLUNIFORM3UIPROC)load("glUniform3ui");
    glad_glUniform4ui = (PFNGLUNIFORM4UIPROC)load("glUniform4ui");
    glad_glUniform1uiv = (PFNGLUNIFORM1UIVPROC)load("glUniform1uiv");
    glad_glUniform2uiv = (PFNGLUNIFORM2UIVPROC)load("glUniform2uiv");
    glad_glUniform3uiv = (PFNGLUNIFORM3UIVPROC)load("glUniform3uiv");
    glad_glUniform4uiv = (PFNGLUNIFORM4UIVPROC)load("glUniform4uiv");
    glad_glTexParameterIiv = (PFNGLTEXPARAMETERIIVPROC)load("glTexParameterIiv");
    glad_glTexParameterIuiv = (PFNGLTEXPARAMETERIUIVPROC)load("glTexParameterIuiv");
    glad_glGetTexParameterIiv = (PFNGLGETTEXPARAMETERIIVPROC)load("glGetTexParameterIiv");
    glad_glGetTexParameterIuiv = (PFNGLGETTEXPARAMETERIUIVPROC)load("glGetTexParameterIuiv");
    glad_glClearBufferiv = (PFNGLCLEARBUFFERIVPROC)load("glClearBufferiv");
    glad_glClearBufferuiv = (PFNGLCLEARBUFFERUIVPROC)load("glClearBufferuiv");
    glad_glClearBufferfv = (PFNGLCLEARBUFFERFVPROC)load("glClearBufferfv");
    glad_glClearBufferfi = (PFNGLCLEARBUFFERFIPROC)load("glClearBufferfi");
    glad_glGetStringi = (PFNGLGETSTRINGIPROC)load("glGetStringi");
    glad_glIsRenderbuffer = (PFNGLISRENDERBUFFERPROC)load("glIsRenderbuffer");
    glad_glBindRenderbuffer = (PFNGLBINDRENDERBUFFERPROC)load("glBindRenderbuffer");
    glad_glDeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSPROC)load("glDeleteRenderbuffers");
    glad_glGenRenderbuffers = (PFNGLGENRENDERBUFFERSPROC)load("glGenRenderbuffers");
    glad_glRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC)load("glRenderbufferStorage");
    glad_glGetRenderbufferParameteriv = (PFNGLGETRENDERBUFFERPARAMETERIVPROC)load("glGetRenderbufferParameteriv");
    glad_glIsFramebuffer = (PFNGLISFRAMEBUFFERPROC)load("glIsFramebuffer");
    glad_glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)load("glBindFramebuffer");
    glad_glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)load("glDeleteFramebuffers");
    glad_glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)load("glGenFramebuffers");
    glad_glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)load("glCheckFramebufferStatus");
    glad_glFramebufferTexture1D = (PFNGLFRAMEBUFFERTEXTURE1DPROC)load("glFramebufferTexture1D");
    glad_glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)load("glFramebufferTexture2D");
    glad_glFramebufferTexture3D = (PFNGLFRAMEBUFFERTEXTURE3DPROC)load("glFramebufferTexture3D");
    glad_glFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFERPROC)load("glFramebufferRenderbuffer");
    glad_glGetFramebufferAttachmentParameteriv = (PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC)load("glGetFramebufferAttachmentParameteriv");
    glad_glGenerateMipmap = (PFNGLGENERATEMIPMAPPROC)load("glGenerateMipmap");
    glad_glBlitFramebuffer = (PFNGLBLITFRAMEBUFFERPROC)load("glBlitFramebuffer");
    glad_glRenderbufferStorageMultisample = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC)load("glRenderbufferStorageMultisample");
    glad_glFramebufferTextureLayer = (PFNGLFRAMEBUFFERTEXTURELAYERPROC)load("glFramebufferTextureLayer");
    glad_glMapBufferRange = (PFNGLMAPBUFFERRANGEPROC)load("glMapBufferRange");
    glad_glFlushMappedBufferRange = (PFNGLFLUSHMAPPEDBUFFERRANGEPROC)load("glFlushMappedBufferRange");
    glad_glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)load("glBindVertexArray");
    glad_glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)load("glDeleteVertexArrays");
    glad_glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)load("glGenVertexArrays");
    glad_glIsVertexArray = (PFNGLISVERTEXARRAYPROC)load("glIsVertexArray");
}
static void load_GL_VERSION_3_1(GLADloadproc load) {
    if (!GLAD_GL_VERSION_3_1) return;
    glad_glDrawArraysInstanced = (PFNGLDRAWARRAYSINSTANCEDPROC)load("glDrawArraysInstanced");
    glad_glDrawElementsInstanced = (PFNGLDRAWELEMENTSINSTANCEDPROC)load("glDrawElementsInstanced");
    glad_glTexBuffer = (PFNGLTEXBUFFERPROC)load("glTexBuffer");
    glad_glPrimitiveRestartIndex = (PFNGLPRIMITIVERESTARTINDEXPROC)load("glPrimitiveRestartIndex");
    glad_glCopyBufferSubData = (PFNGLCOPYBUFFERSUBDATAPROC)load("glCopyBufferSubData");
    glad_glGetUniformIndices = (PFNGLGETUNIFORMINDICESPROC)load("glGetUniformIndices");
    glad_glGetActiveUniformsiv = (PFNGLGETACTIVEUNIFORMSIVPROC)load("glGetActiveUniformsiv");
    glad_glGetActiveUniformName = (PFNGLGETACTIVEUNIFORMNAMEPROC)load("glGetActiveUniformName");
    glad_glGetUniformBlockIndex = (PFNGLGETUNIFORMBLOCKINDEXPROC)load("glGetUniformBlockIndex");
    glad_glGetActiveUniformBlockiv = (PFNGLGETACTIVEUNIFORMBLOCKIVPROC)load("glGetActiveUniformBlockiv");
    glad_glGetActiveUniformBlockName = (PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC)load("glGetActiveUniformBlockName");
    glad_glUniformBlockBinding = (PFNGLUNIFORMBLOCKBINDINGPROC)load("glUniformBlockBinding");
    glad_glBindBufferRange = (PFNGLBINDBUFFERRANGEPROC)load("glBindBufferRange");
    glad_glBindBufferBase = (PFNGLBINDBUFFERBASEPROC)load("glBindBufferBase");
    glad_glGetIntegeri_v = (PFNGLGETINTEGERI_VPROC)load("glGetIntegeri_v");
}
static void load_GL_VERSION_3_2(GLADloadproc load) {
    if (!GLAD_GL_VERSION_3_2) return;
    glad_glDrawElementsBaseVertex = (PFNGLDRAWELEMENTSBASEVERTEXPROC)load("glDrawElementsBaseVertex");
    glad_glDrawRangeElementsBaseVertex = (PFNGLDRAWRANGEELEMENTSBASEVERTEXPROC)load("glDrawRangeElementsBaseVertex");
    glad_glDrawElementsInstancedBaseVertex = (PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC)load("glDrawElementsInstancedBaseVertex");
    glad_glMultiDrawElementsBaseVertex = (PFNGLMULTIDRAWELEMENTSBASEVERTEXPROC)load("glMultiDrawElementsBaseVertex");
    glad_glProvokingVertex = (PFNGLPROVOKINGVERTEXPROC)load("glProvokingVertex");
    glad_glFenceSync = (PFNGLFENCESYNCPROC)load("glFenceSync");
    glad_glIsSync = (PFNGLISSYNCPROC)load("glIsSync");
    glad_glDeleteSync = (PFNGLDELETESYNCPROC)load("glDeleteSync");
    glad_glClientWaitSync = (PFNGLCLIENTWAITSYNCPROC)load("glClientWaitSync");
    glad_glWaitSync = (PFNGLWAITSYNCPROC)load("glWaitSync");
    glad_glGetInteger64v = (PFNGLGETINTEGER64VPROC)load("glGetInteger64v");
    glad_glGetSynciv = (PFNGLGETSYNCIVPROC)load("glGetSynciv");
    glad_glGetInteger64i_v = (PFNGLGETINTEGER64I_VPROC)load("glGetInteger64i_v");
    glad_glGetBufferParameteri64v = (PFNGLGETBUFFERPARAMETERI64VPROC)load("glGetBufferParameteri64v");
    glad_glFramebufferTexture = (PFNGLFRAMEBUFFERTEXTUREPROC)load("glFramebufferTexture");
    glad_glTexImage2DMultisample = (PFNGLTEXIMAGE2DMULTISAMPLEPROC)load("glTexImage2DMultisample");
    glad_glTexImage3DMultisample = (PFNGLTEXIMAGE3DMULTISAMPLEPROC)load("glTexImage3DMultisample");
    glad_glGetMultisamplefv = (PFNGLGETMULTISAMPLEFVPROC)load("glGetMultisamplefv");
    glad_glSampleMaski = (PFNGLSAMPLEMASKIPROC)load("glSampleMaski");
}
static void load_GL_VERSION_3_3(GLADloadproc load) {
    if (!GLAD_GL_VERSION_3_3) return;
    glad_glBindFragDataLocationIndexed = (PFNGLBINDFRAGDATALOCATIONINDEXEDPROC)load("glBindFragDataLocationIndexed");
    glad_glGetFragDataIndex = (PFNGLGETFRAGDATAINDEXPROC)load("glGetFragDataIndex");
    glad_glGenSamplers = (PFNGLGENSAMPLERSPROC)load("glGenSamplers");
    glad_glDeleteSamplers = (PFNGLDELETESAMPLERSPROC)load("glDeleteSamplers");
    glad_glIsSampler = (PFNGLISSAMPLERPROC)load("glIsSampler");
    glad_glBindSampler = (PFNGLBINDSAMPLERPROC)load("glBindSampler");
    glad_glSamplerParameteri = (PFNGLSAMPLERPARAMETERIPROC)load("glSamplerParameteri");
    glad_glSamplerParameteriv = (PFNGLSAMPLERPARAMETERIVPROC)load("glSamplerParameteriv");
    glad_glSamplerParameterf = (PFNGLSAMPLERPARAMETERFPROC)load("glSamplerParameterf");
    glad_glSamplerParameterfv = (PFNGLSAMPLERPARAMETERFVPROC)load("glSamplerParameterfv");
    glad_glSamplerParameterIiv = (PFNGLSAMPLERPARAMETERIIVPROC)load("glSamplerParameterIiv");
    glad_glSamplerParameterIuiv = (PFNGLSAMPLERPARAMETERIUIVPROC)load("glSamplerParameterIuiv");
    glad_glGetSamplerParameteriv = (PFNGLGETSAMPLERPARAMETERIVPROC)load("glGetSamplerParameteriv");
    glad_glGetSamplerParameterIiv = (PFNGLGETSAMPLERPARAMETERIIVPROC)load("glGetSamplerParameterIiv");
    glad_glGetSamplerParameterfv = (PFNGLGETSAMPLERPARAMETERFVPROC)load("glGetSamplerParameterfv");
    glad_glGetSamplerParameterIuiv = (PFNGLGETSAMPLERPARAMETERIUIVPROC)load("glGetSamplerParameterIuiv");
    glad_glQueryCounter = (PFNGLQUERYCOUNTERPROC)load("glQueryCounter");
    glad_glGetQueryObjecti64v = (PFNGLGETQUERYOBJECTI64VPROC)load("glGetQueryObjecti64v");
    glad_glGetQueryObjectui64v = (PFNGLGETQUERYOBJECTUI64VPROC)load("glGetQueryObjectui64v");
    glad_glVertexAttribDivisor = (PFNGLVERTEXATTRIBDIVISORPROC)load("glVertexAttribDivisor");
    glad_glVertexAttribP1ui = (PFNGLVERTEXATTRIBP1UIPROC)load("glVertexAttribP1ui");
    glad_glVertexAttribP1uiv = (PFNGLVERTEXATTRIBP1UIVPROC)load("glVertexAttribP1uiv");
    glad_glVertexAttribP2ui = (PFNGLVERTEXATTRIBP2UIPROC)load("glVertexAttribP2ui");
    glad_glVertexAttribP2uiv = (PFNGLVERTEXATTRIBP2UIVPROC)load("glVertexAttribP2uiv");
    glad_glVertexAttribP3ui = (PFNGLVERTEXATTRIBP3UIPROC)load("glVertexAttribP3ui");
    glad_glVertexAttribP3uiv = (PFNGLVERTEXATTRIBP3UIVPROC)load("glVertexAttribP3uiv");
    glad_glVertexAttribP4ui = (PFNGLVERTEXATTRIBP4UIPROC)load("glVertexAttribP4ui");
    glad_glVertexAttribP4uiv = (PFNGLVERTEXATTRIBP4UIVPROC)load("glVertexAttribP4uiv");
    glad_glVertexP2ui = (PFNGLVERTEXP2UIPROC)load("glVertexP2ui");
    glad_glVertexP2uiv = (PFNGLVERTEXP2UIVPROC)load("glVertexP2uiv");
    glad_glVertexP3ui = (PFNGLVERTEXP3UIPROC)load("glVertexP3ui");
    glad_glVertexP3uiv = (PFNGLVERTEXP3UIVPROC)load("glVertexP3uiv");
    glad_glVertexP4ui = (PFNGLVERTEXP4UIPROC)load("glVertexP4ui");
    glad_glVertexP4uiv = (PFNGLVERTEXP4UIVPROC)load("glVertexP4uiv");
    glad_glTexCoordP1ui = (PFNGLTEXCOORDP1UIPROC)load("glTexCoordP1ui");
    glad_glTexCoordP1uiv = (PFNGLTEXCOORDP1UIVPROC)load("glTexCoordP1uiv");
    glad_glTexCoordP2ui = (PFNGLTEXCOORDP2UIPROC)load("glTexCoordP2ui");
    glad_glTexCoordP2uiv = (PFNGLTEXCOORDP2UIVPROC)load("glTexCoordP2uiv");
    glad_glTexCoordP3ui = (PFNGLTEXCOORDP3UIPROC)load("glTexCoordP3ui");
    glad_glTexCoordP3uiv = (PFNGLTEXCOORDP3UIVPROC)load("glTexCoordP3uiv");
    glad_glTexCoordP4ui = (PFNGLTEXCOORDP4UIPROC)load("glTexCoordP4ui");
    glad_glTexCoordP4uiv = (PFNGLTEXCOORDP4UIVPROC)load("glTexCoordP4uiv");
    glad_glMultiTexCoordP1ui = (PFNGLMULTITEXCOORDP1UIPROC)load("glMultiTexCoordP1ui");
    glad_glMultiTexCoordP1uiv = (PFNGLMULTITEXCOORDP1UIVPROC)load("glMultiTexCoordP1uiv");
    glad_glMultiTexCoordP2ui = (PFNGLMULTITEXCOORDP2UIPROC)load("glMultiTexCoordP2ui");
    glad_glMultiTexCoordP2uiv = (PFNGLMULTITEXCOORDP2UIVPROC)load("glMultiTexCoordP2uiv");
    glad_glMultiTexCoordP3ui = (PFNGLMULTITEXCOORDP3UIPROC)load("glMultiTexCoordP3ui");
    glad_glMultiTexCoordP3uiv = (PFNGLMULTITEXCOORDP3UIVPROC)load("glMultiTexCoordP3uiv");
    glad_glMultiTexCoordP4ui = (PFNGLMULTITEXCOORDP4UIPROC)load("glMultiTexCoordP4ui");
    glad_glMultiTexCoordP4uiv = (PFNGLMULTITEXCOORDP4UIVPROC)load("glMultiTexCoordP4uiv");
    glad_glNormalP3ui = (PFNGLNORMALP3UIPROC)load("glNormalP3ui");
    glad_glNormalP3uiv = (PFNGLNORMALP3UIVPROC)load("glNormalP3uiv");
    glad_glColorP3ui = (PFNGLCOLORP3UIPROC)load("glColorP3ui");
    glad_glColorP3uiv = (PFNGLCOLORP3UIVPROC)load("glColorP3uiv");
    glad_glColorP4ui = (PFNGLCOLORP4UIPROC)load("glColorP4ui");
    glad_glColorP4uiv = (PFNGLCOLORP4UIVPROC)load("glColorP4uiv");
    glad_glSecondaryColorP3ui = (PFNGLSECONDARYCOLORP3UIPROC)load("glSecondaryColorP3ui");
    glad_glSecondaryColorP3uiv = (PFNGLSECONDARYCOLORP3UIVPROC)load("glSecondaryColorP3uiv");
}
static void load_GL_VERSION_4_0(GLADloadproc load) {
    if (!GLAD_GL_VERSION_4_0) return;
    glad_glMinSampleShading = (PFNGLMINSAMPLESHADINGPROC)load("glMinSampleShading");
    glad_glBlendEquationi = (PFNGLBLENDEQUATIONIPROC)load("glBlendEquationi");
    glad_glBlendEquationSeparatei = (PFNGLBLENDEQUATIONSEPARATEIPROC)load("glBlendEquationSeparatei");
    glad_glBlendFunci = (PFNGLBLENDFUNCIPROC)load("glBlendFunci");
    glad_glBlendFuncSeparatei = (PFNGLBLENDFUNCSEPARATEIPROC)load("glBlendFuncSeparatei");
    glad_glDrawArraysIndirect = (PFNGLDRAWARRAYSINDIRECTPROC)load("glDrawArraysIndirect");
    glad_glDrawElementsIndirect = (PFNGLDRAWELEMENTSINDIRECTPROC)load("glDrawElementsIndirect");
    glad_glUniform1d = (PFNGLUNIFORM1DPROC)load("glUniform1d");
    glad_glUniform2d = (PFNGLUNIFORM2DPROC)load("glUniform2d");
    glad_glUniform3d = (PFNGLUNIFORM3DPROC)load("glUniform3d");
    glad_glUniform4d = (PFNGLUNIFORM4DPROC)load("glUniform4d");
    glad_glUniform1dv = (PFNGLUNIFORM1DVPROC)load("glUniform1dv");
    glad_glUniform2dv = (PFNGLUNIFORM2DVPROC)load("glUniform2dv");
    glad_glUniform3dv = (PFNGLUNIFORM3DVPROC)load("glUniform3dv");
    glad_glUniform4dv = (PFNGLUNIFORM4DVPROC)load("glUniform4dv");
    glad_glUniformMatrix2dv = (PFNGLUNIFORMMATRIX2DVPROC)load("glUniformMatrix2dv");
    glad_glUniformMatrix3dv = (PFNGLUNIFORMMATRIX3DVPROC)load("glUniformMatrix3dv");
    glad_glUniformMatrix4dv = (PFNGLUNIFORMMATRIX4DVPROC)load("glUniformMatrix4dv");
    glad_glUniformMatrix2x3dv = (PFNGLUNIFORMMATRIX2X3DVPROC)load("glUniformMatrix2x3dv");
    glad_glUniformMatrix2x4dv = (PFNGLUNIFORMMATRIX2X4DVPROC)load("glUniformMatrix2x4dv");
    glad_glUniformMatrix3x2dv = (PFNGLUNIFORMMATRIX3X2DVPROC)load("glUniformMatrix3x2dv");
    glad_glUniformMatrix3x4dv = (PFNGLUNIFORMMATRIX3X4DVPROC)load("glUniformMatrix3x4dv");
    glad_glUniformMatrix4x2dv = (PFNGLUNIFORMMATRIX4X2DVPROC)load("glUniformMatrix4x2dv");
    glad_glUniformMatrix4x3dv = (PFNGLUNIFORMMATRIX4X3DVPROC)load("glUniformMatrix4x3dv");
    glad_glGetUniformdv = (PFNGLGETUNIFORMDVPROC)load("glGetUniformdv");
    glad_glGetSubroutineUniformLocation = (PFNGLGETSUBROUTINEUNIFORMLOCATIONPROC)load("glGetSubroutineUniformLocation");
    glad_glGetSubroutineIndex = (PFNGLGETSUBROUTINEINDEXPROC)load("glGetSubroutineIndex");
    glad_glGetActiveSubroutineUniformiv = (PFNGLGETACTIVESUBROUTINEUNIFORMIVPROC)load("glGetActiveSubroutineUniformiv");
    glad_glGetActiveSubroutineUniformName = (PFNGLGETACTIVESUBROUTINEUNIFORMNAMEPROC)load("glGetActiveSubroutineUniformName");
    glad_glGetActiveSubroutineName = (PFNGLGETACTIVESUBROUTINENAMEPROC)load("glGetActiveSubroutineName");
    glad_glUniformSubroutinesuiv = (PFNGLUNIFORMSUBROUTINESUIVPROC)load("glUniformSubroutinesuiv");
    glad_glGetUniformSubroutineuiv = (PFNGLGETUNIFORMSUBROUTINEUIVPROC)load("glGetUniformSubroutineuiv");
    glad_glGetProgramStageiv = (PFNGLGETPROGRAMSTAGEIVPROC)load("glGetProgramStageiv");
    glad_glPatchParameteri = (PFNGLPATCHPARAMETERIPROC)load("glPatchParameteri");
    glad_glPatchParameterfv = (PFNGLPATCHPARAMETERFVPROC)load("glPatchParameterfv");
    glad_glBindTransformFeedback = (PFNGLBINDTRANSFORMFEEDBACKPROC)load("glBindTransformFeedback");
    glad_glDeleteTransformFeedbacks = (PFNGLDELETETRANSFORMFEEDBACKSPROC)load("glDeleteTransformFeedbacks");
    glad_glGenTransformFeedbacks = (PFNGLGENTRANSFORMFEEDBACKSPROC)load("glGenTransformFeedbacks");
    glad_glIsTransformFeedback = (PFNGLISTRANSFORMFEEDBACKPROC)load("glIsTransformFeedback");
    glad_glPauseTransformFeedback = (PFNGLPAUSETRANSFORMFEEDBACKPROC)load("glPauseTransformFeedback");
    glad_glResumeTransformFeedback = (PFNGLRESUMETRANSFORMFEEDBACKPROC)load("glResumeTransformFeedback");
    glad_glDrawTransformFeedback = (PFNGLDRAWTRANSFORMFEEDBACKPROC)load("glDrawTransformFeedback");
    glad_glDrawTransformFeedbackStream = (PFNGLDRAWTRANSFORMFEEDBACKSTREAMPROC)load("glDrawTransformFeedbackStream");
    glad_glBeginQueryIndexed = (PFNGLBEGINQUERYINDEXEDPROC)load("glBeginQueryIndexed");
    glad_glEndQueryIndexed = (PFNGLENDQUERYINDEXEDPROC)load("glEndQueryIndexed");
    glad_glGetQueryIndexediv = (PFNGLGETQUERYINDEXEDIVPROC)load("glGetQueryIndexediv");
}
static void load_GL_VERSION_4_1(GLADloadproc load) {
    if (!GLAD_GL_VERSION_4_1) return;
    glad_glReleaseShaderCompiler = (PFNGLRELEASESHADERCOMPILERPROC)load("glReleaseShaderCompiler");
    glad_glShaderBinary = (PFNGLSHADERBINARYPROC)load("glShaderBinary");
    glad_glGetShaderPrecisionFormat = (PFNGLGETSHADERPRECISIONFORMATPROC)load("glGetShaderPrecisionFormat");
    glad_glDepthRangef = (PFNGLDEPTHRANGEFPROC)load("glDepthRangef");
    glad_glClearDepthf = (PFNGLCLEARDEPTHFPROC)load("glClearDepthf");
    glad_glGetProgramBinary = (PFNGLGETPROGRAMBINARYPROC)load("glGetProgramBinary");
    glad_glProgramBinary = (PFNGLPROGRAMBINARYPROC)load("glProgramBinary");
    glad_glProgramParameteri = (PFNGLPROGRAMPARAMETERIPROC)load("glProgramParameteri");
    glad_glUseProgramStages = (PFNGLUSEPROGRAMSTAGESPROC)load("glUseProgramStages");
    glad_glActiveShaderProgram = (PFNGLACTIVESHADERPROGRAMPROC)load("glActiveShaderProgram");
    glad_glCreateShaderProgramv = (PFNGLCREATESHADERPROGRAMVPROC)load("glCreateShaderProgramv");
    glad_glBindProgramPipeline = (PFNGLBINDPROGRAMPIPELINEPROC)load("glBindProgramPipeline");
    glad_glDeleteProgramPipelines = (PFNGLDELETEPROGRAMPIPELINESPROC)load("glDeleteProgramPipelines");
    glad_glGenProgramPipelines = (PFNGLGENPROGRAMPIPELINESPROC)load("glGenProgramPipelines");
    glad_glIsProgramPipeline = (PFNGLISPROGRAMPIPELINEPROC)load("glIsProgramPipeline");
    glad_glGetProgramPipelineiv = (PFNGLGETPROGRAMPIPELINEIVPROC)load("glGetProgramPipelineiv");
    glad_glProgramParameteri = (PFNGLPROGRAMPARAMETERIPROC)load("glProgramParameteri");
    glad_glProgramUniform1i = (PFNGLPROGRAMUNIFORM1IPROC)load("glProgramUniform1i");
    glad_glProgramUniform1iv = (PFNGLPROGRAMUNIFORM1IVPROC)load("glProgramUniform1iv");
    glad_glProgramUniform1f = (PFNGLPROGRAMUNIFORM1FPROC)load("glProgramUniform1f");
    glad_glProgramUniform1fv = (PFNGLPROGRAMUNIFORM1FVPROC)load("glProgramUniform1fv");
    glad_glProgramUniform1d = (PFNGLPROGRAMUNIFORM1DPROC)load("glProgramUniform1d");
    glad_glProgramUniform1dv = (PFNGLPROGRAMUNIFORM1DVPROC)load("glProgramUniform1dv");
    glad_glProgramUniform1ui = (PFNGLPROGRAMUNIFORM1UIPROC)load("glProgramUniform1ui");
    glad_glProgramUniform1uiv = (PFNGLPROGRAMUNIFORM1UIVPROC)load("glProgramUniform1uiv");
    glad_glProgramUniform2i = (PFNGLPROGRAMUNIFORM2IPROC)load("glProgramUniform2i");
    glad_glProgramUniform2iv = (PFNGLPROGRAMUNIFORM2IVPROC)load("glProgramUniform2iv");
    glad_glProgramUniform2f = (PFNGLPROGRAMUNIFORM2FPROC)load("glProgramUniform2f");
    glad_glProgramUniform2fv = (PFNGLPROGRAMUNIFORM2FVPROC)load("glProgramUniform2fv");
    glad_glProgramUniform2d = (PFNGLPROGRAMUNIFORM2DPROC)load("glProgramUniform2d");
    glad_glProgramUniform2dv = (PFNGLPROGRAMUNIFORM2DVPROC)load("glProgramUniform2dv");
    glad_glProgramUniform2ui = (PFNGLPROGRAMUNIFORM2UIPROC)load("glProgramUniform2ui");
    glad_glProgramUniform2uiv = (PFNGLPROGRAMUNIFORM2UIVPROC)load("glProgramUniform2uiv");
    glad_glProgramUniform3i = (PFNGLPROGRAMUNIFORM3IPROC)load("glProgramUniform3i");
    glad_glProgramUniform3iv = (PFNGLPROGRAMUNIFORM3IVPROC)load("glProgramUniform3iv");
    glad_glProgramUniform3f = (PFNGLPROGRAMUNIFORM3FPROC)load("glProgramUniform3f");
    glad_glProgramUniform3fv = (PFNGLPROGRAMUNIFORM3FVPROC)load("glProgramUniform3fv");
    glad_glProgramUniform3d = (PFNGLPROGRAMUNIFORM3DPROC)load("glProgramUniform3d");
    glad_glProgramUniform3dv = (PFNGLPROGRAMUNIFORM3DVPROC)load("glProgramUniform3dv");
    glad_glProgramUniform3ui = (PFNGLPROGRAMUNIFORM3UIPROC)load("glProgramUniform3ui");
    glad_glProgramUniform3uiv = (PFNGLPROGRAMUNIFORM3UIVPROC)load("glProgramUniform3uiv");
    glad_glProgramUniform4i = (PFNGLPROGRAMUNIFORM4IPROC)load("glProgramUniform4i");
    glad_glProgramUniform4iv = (PFNGLPROGRAMUNIFORM4IVPROC)load("glProgramUniform4iv");
    glad_glProgramUniform4f = (PFNGLPROGRAMUNIFORM4FPROC)load("glProgramUniform4f");
    glad_glProgramUniform4fv = (PFNGLPROGRAMUNIFORM4FVPROC)load("glProgramUniform4fv");
    glad_glProgramUniform4d = (PFNGLPROGRAMUNIFORM4DPROC)load("glProgramUniform4d");
    glad_glProgramUniform4dv = (PFNGLPROGRAMUNIFORM4DVPROC)load("glProgramUniform4dv");
    glad_glProgramUniform4ui = (PFNGLPROGRAMUNIFORM4UIPROC)load("glProgramUniform4ui");
    glad_glProgramUniform4uiv = (PFNGLPROGRAMUNIFORM4UIVPROC)load("glProgramUniform4uiv");
    glad_glProgramUniformMatrix2fv = (PFNGLPROGRAMUNIFORMMATRIX2FVPROC)load("glProgramUniformMatrix2fv");
    glad_glProgramUniformMatrix3fv = (PFNGLPROGRAMUNIFORMMATRIX3FVPROC)load("glProgramUniformMatrix3fv");
    glad_glProgramUniformMatrix4fv = (PFNGLPROGRAMUNIFORMMATRIX4FVPROC)load("glProgramUniformMatrix4fv");
    glad_glProgramUniformMatrix2dv = (PFNGLPROGRAMUNIFORMMATRIX2DVPROC)load("glProgramUniformMatrix2dv");
    glad_glProgramUniformMatrix3dv = (PFNGLPROGRAMUNIFORMMATRIX3DVPROC)load("glProgramUniformMatrix3dv");
    glad_glProgramUniformMatrix4dv = (PFNGLPROGRAMUNIFORMMATRIX4DVPROC)load("glProgramUniformMatrix4dv");
    glad_glProgramUniformMatrix2x3fv = (PFNGLPROGRAMUNIFORMMATRIX2X3FVPROC)load("glProgramUniformMatrix2x3fv");
    glad_glProgramUniformMatrix3x2fv = (PFNGLPROGRAMUNIFORMMATRIX3X2FVPROC)load("glProgramUniformMatrix3x2fv");
    glad_glProgramUniformMatrix2x4fv = (PFNGLPROGRAMUNIFORMMATRIX2X4FVPROC)load("glProgramUniformMatrix2x4fv");
    glad_glProgramUniformMatrix4x2fv = (PFNGLPROGRAMUNIFORMMATRIX4X2FVPROC)load("glProgramUniformMatrix4x2fv");
    glad_glProgramUniformMatrix3x4fv = (PFNGLPROGRAMUNIFORMMATRIX3X4FVPROC)load("glProgramUniformMatrix3x4fv");
    glad_glProgramUniformMatrix4x3fv = (PFNGLPROGRAMUNIFORMMATRIX4X3FVPROC)load("glProgramUniformMatrix4x3fv");
    glad_glProgramUniformMatrix2x3dv = (PFNGLPROGRAMUNIFORMMATRIX2X3DVPROC)load("glProgramUniformMatrix2x3dv");
    glad_glProgramUniformMatrix3x2dv = (PFNGLPROGRAMUNIFORMMATRIX3X2DVPROC)load("glProgramUniformMatrix3x2dv");
    glad_glProgramUniformMatrix2x4dv = (PFNGLPROGRAMUNIFORMMATRIX2X4DVPROC)load("glProgramUniformMatrix2x4dv");
    glad_glProgramUniformMatrix4x2dv = (PFNGLPROGRAMUNIFORMMATRIX4X2DVPROC)load("glProgramUniformMatrix4x2dv");
    glad_glProgramUniformMatrix3x4dv = (PFNGLPROGRAMUNIFORMMATRIX3X4DVPROC)load("glProgramUniformMatrix3x4dv");
    glad_glProgramUniformMatrix4x3dv = (PFNGLPROGRAMUNIFORMMATRIX4X3DVPROC)load("glProgramUniformMatrix4x3dv");
    glad_glValidateProgramPipeline = (PFNGLVALIDATEPROGRAMPIPELINEPROC)load("glValidateProgramPipeline");
    glad_glGetProgramPipelineInfoLog = (PFNGLGETPROGRAMPIPELINEINFOLOGPROC)load("glGetProgramPipelineInfoLog");
    glad_glVertexAttribL1d = (PFNGLVERTEXATTRIBL1DPROC)load("glVertexAttribL1d");
    glad_glVertexAttribL2d = (PFNGLVERTEXATTRIBL2DPROC)load("glVertexAttribL2d");
    glad_glVertexAttribL3d = (PFNGLVERTEXATTRIBL3DPROC)load("glVertexAttribL3d");
    glad_glVertexAttribL4d = (PFNGLVERTEXATTRIBL4DPROC)load("glVertexAttribL4d");
    glad_glVertexAttribL1dv = (PFNGLVERTEXATTRIBL1DVPROC)load("glVertexAttribL1dv");
    glad_glVertexAttribL2dv = (PFNGLVERTEXATTRIBL2DVPROC)load("glVertexAttribL2dv");
    glad_glVertexAttribL3dv = (PFNGLVERTEXATTRIBL3DVPROC)load("glVertexAttribL3dv");
    glad_glVertexAttribL4dv = (PFNGLVERTEXATTRIBL4DVPROC)load("glVertexAttribL4dv");
    glad_glVertexAttribLPointer = (PFNGLVERTEXATTRIBLPOINTERPROC)load("glVertexAttribLPointer");
    glad_glGetVertexAttribLdv = (PFNGLGETVERTEXATTRIBLDVPROC)load("glGetVertexAttribLdv");
    glad_glViewportArrayv = (PFNGLVIEWPORTARRAYVPROC)load("glViewportArrayv");
    glad_glViewportIndexedf = (PFNGLVIEWPORTINDEXEDFPROC)load("glViewportIndexedf");
    glad_glViewportIndexedfv = (PFNGLVIEWPORTINDEXEDFVPROC)load("glViewportIndexedfv");
    glad_glScissorArrayv = (PFNGLSCISSORARRAYVPROC)load("glScissorArrayv");
    glad_glScissorIndexed = (PFNGLSCISSORINDEXEDPROC)load("glScissorIndexed");
    glad_glScissorIndexedv = (PFNGLSCISSORINDEXEDVPROC)load("glScissorIndexedv");
    glad_glDepthRangeArrayv = (PFNGLDEPTHRANGEARRAYVPROC)load("glDepthRangeArrayv");
    glad_glDepthRangeIndexed = (PFNGLDEPTHRANGEINDEXEDPROC)load("glDepthRangeIndexed");
    glad_glGetFloati_v = (PFNGLGETFLOATI_VPROC)load("glGetFloati_v");
    glad_glGetDoublei_v = (PFNGLGETDOUBLEI_VPROC)load("glGetDoublei_v");
}
static void load_GL_VERSION_4_2(GLADloadproc load) {
    if (!GLAD_GL_VERSION_4_2) return;
    glad_glDrawArraysInstancedBaseInstance = (PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEPROC)load("glDrawArraysInstancedBaseInstance");
    glad_glDrawElementsInstancedBaseInstance = (PFNGLDRAWELEMENTSINSTANCEDBASEINSTANCEPROC)load("glDrawElementsInstancedBaseInstance");
    glad_glDrawElementsInstancedBaseVertexBaseInstance = (PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEPROC)load("glDrawElementsInstancedBaseVertexBaseInstance");
    glad_glGetInternalformativ = (PFNGLGETINTERNALFORMATIVPROC)load("glGetInternalformativ");
    glad_glGetActiveAtomicCounterBufferiv = (PFNGLGETACTIVEATOMICCOUNTERBUFFERIVPROC)load("glGetActiveAtomicCounterBufferiv");
    glad_glBindImageTexture = (PFNGLBINDIMAGETEXTUREPROC)load("glBindImageTexture");
    glad_glMemoryBarrier = (PFNGLMEMORYBARRIERPROC)load("glMemoryBarrier");
    glad_glTexStorage1D = (PFNGLTEXSTORAGE1DPROC)load("glTexStorage1D");
    glad_glTexStorage2D = (PFNGLTEXSTORAGE2DPROC)load("glTexStorage2D");
    glad_glTexStorage3D = (PFNGLTEXSTORAGE3DPROC)load("glTexStorage3D");
    glad_glDrawTransformFeedbackInstanced = (PFNGLDRAWTRANSFORMFEEDBACKINSTANCEDPROC)load("glDrawTransformFeedbackInstanced");
    glad_glDrawTransformFeedbackStreamInstanced = (PFNGLDRAWTRANSFORMFEEDBACKSTREAMINSTANCEDPROC)load("glDrawTransformFeedbackStreamInstanced");
}
static void load_GL_VERSION_4_3(GLADloadproc load) {
    if (!GLAD_GL_VERSION_4_3) return;
    glad_glClearBufferData = (PFNGLCLEARBUFFERDATAPROC)load("glClearBufferData");
    glad_glClearBufferSubData = (PFNGLCLEARBUFFERSUBDATAPROC)load("glClearBufferSubData");
    glad_glDispatchCompute = (PFNGLDISPATCHCOMPUTEPROC)load("glDispatchCompute");
    glad_glDispatchComputeIndirect = (PFNGLDISPATCHCOMPUTEINDIRECTPROC)load("glDispatchComputeIndirect");
    glad_glCopyImageSubData = (PFNGLCOPYIMAGESUBDATAPROC)load("glCopyImageSubData");
    glad_glFramebufferParameteri = (PFNGLFRAMEBUFFERPARAMETERIPROC)load("glFramebufferParameteri");
    glad_glGetFramebufferParameteriv = (PFNGLGETFRAMEBUFFERPARAMETERIVPROC)load("glGetFramebufferParameteriv");
    glad_glGetInternalformati64v = (PFNGLGETINTERNALFORMATI64VPROC)load("glGetInternalformati64v");
    glad_glInvalidateTexSubImage = (PFNGLINVALIDATETEXSUBIMAGEPROC)load("glInvalidateTexSubImage");
    glad_glInvalidateTexImage = (PFNGLINVALIDATETEXIMAGEPROC)load("glInvalidateTexImage");
    glad_glInvalidateBufferSubData = (PFNGLINVALIDATEBUFFERSUBDATAPROC)load("glInvalidateBufferSubData");
    glad_glInvalidateBufferData = (PFNGLINVALIDATEBUFFERDATAPROC)load("glInvalidateBufferData");
    glad_glInvalidateFramebuffer = (PFNGLINVALIDATEFRAMEBUFFERPROC)load("glInvalidateFramebuffer");
    glad_glInvalidateSubFramebuffer = (PFNGLINVALIDATESUBFRAMEBUFFERPROC)load("glInvalidateSubFramebuffer");
    glad_glMultiDrawArraysIndirect = (PFNGLMULTIDRAWARRAYSINDIRECTPROC)load("glMultiDrawArraysIndirect");
    glad_glMultiDrawElementsIndirect = (PFNGLMULTIDRAWELEMENTSINDIRECTPROC)load("glMultiDrawElementsIndirect");
    glad_glGetProgramInterfaceiv = (PFNGLGETPROGRAMINTERFACEIVPROC)load("glGetProgramInterfaceiv");
    glad_glGetProgramResourceIndex = (PFNGLGETPROGRAMRESOURCEINDEXPROC)load("glGetProgramResourceIndex");
    glad_glGetProgramResourceName = (PFNGLGETPROGRAMRESOURCENAMEPROC)load("glGetProgramResourceName");
    glad_glGetProgramResourceiv = (PFNGLGETPROGRAMRESOURCEIVPROC)load("glGetProgramResourceiv");
    glad_glGetProgramResourceLocation = (PFNGLGETPROGRAMRESOURCELOCATIONPROC)load("glGetProgramResourceLocation");
    glad_glGetProgramResourceLocationIndex = (PFNGLGETPROGRAMRESOURCELOCATIONINDEXPROC)load("glGetProgramResourceLocationIndex");
    glad_glShaderStorageBlockBinding = (PFNGLSHADERSTORAGEBLOCKBINDINGPROC)load("glShaderStorageBlockBinding");
    glad_glTexBufferRange = (PFNGLTEXBUFFERRANGEPROC)load("glTexBufferRange");
    glad_glTexStorage2DMultisample = (PFNGLTEXSTORAGE2DMULTISAMPLEPROC)load("glTexStorage2DMultisample");
    glad_glTexStorage3DMultisample = (PFNGLTEXSTORAGE3DMULTISAMPLEPROC)load("glTexStorage3DMultisample");
    glad_glTextureView = (PFNGLTEXTUREVIEWPROC)load("glTextureView");
    glad_glBindVertexBuffer = (PFNGLBINDVERTEXBUFFERPROC)load("glBindVertexBuffer");
    glad_glVertexAttribFormat = (PFNGLVERTEXATTRIBFORMATPROC)load("glVertexAttribFormat");
    glad_glVertexAttribIFormat = (PFNGLVERTEXATTRIBIFORMATPROC)load("glVertexAttribIFormat");
    glad_glVertexAttribLFormat = (PFNGLVERTEXATTRIBLFORMATPROC)load("glVertexAttribLFormat");
    glad_glVertexAttribBinding = (PFNGLVERTEXATTRIBBINDINGPROC)load("glVertexAttribBinding");
    glad_glVertexBindingDivisor = (PFNGLVERTEXBINDINGDIVISORPROC)load("glVertexBindingDivisor");
    glad_glDebugMessageControl = (PFNGLDEBUGMESSAGECONTROLPROC)load("glDebugMessageControl");
    glad_glDebugMessageInsert = (PFNGLDEBUGMESSAGEINSERTPROC)load("glDebugMessageInsert");
    glad_glDebugMessageCallback = (PFNGLDEBUGMESSAGECALLBACKPROC)load("glDebugMessageCallback");
    glad_glGetDebugMessageLog = (PFNGLGETDEBUGMESSAGELOGPROC)load("glGetDebugMessageLog");
    glad_glPushDebugGroup = (PFNGLPUSHDEBUGGROUPPROC)load("glPushDebugGroup");
    glad_glPopDebugGroup = (PFNGLPOPDEBUGGROUPPROC)load("glPopDebugGroup");
    glad_glObjectLabel = (PFNGLOBJECTLABELPROC)load("glObjectLabel");
    glad_glGetObjectLabel = (PFNGLGETOBJECTLABELPROC)load("glGetObjectLabel");
    glad_glObjectPtrLabel = (PFNGLOBJECTPTRLABELPROC)load("glObjectPtrLabel");
    glad_glGetObjectPtrLabel = (PFNGLGETOBJECTPTRLABELPROC)load("glGetObjectPtrLabel");
    glad_glGetPointerv = (PFNGLGETPOINTERVPROC)load("glGetPointerv");
}
static void load_GL_VERSION_4_4(GLADloadproc load) {
    if (!GLAD_GL_VERSION_4_4) return;
    glad_glBufferStorage = (PFNGLBUFFERSTORAGEPROC)load("glBufferStorage");
    glad_glClearTexImage = (PFNGLCLEARTEXIMAGEPROC)load("glClearTexImage");
    glad_glClearTexSubImage = (PFNGLCLEARTEXSUBIMAGEPROC)load("glClearTexSubImage");
    glad_glBindBuffersBase = (PFNGLBINDBUFFERSBASEPROC)load("glBindBuffersBase");
    glad_glBindBuffersRange = (PFNGLBINDBUFFERSRANGEPROC)load("glBindBuffersRange");
    glad_glBindTextures = (PFNGLBINDTEXTURESPROC)load("glBindTextures");
    glad_glBindSamplers = (PFNGLBINDSAMPLERSPROC)load("glBindSamplers");
    glad_glBindImageTextures = (PFNGLBINDIMAGETEXTURESPROC)load("glBindImageTextures");
    glad_glBindVertexBuffers = (PFNGLBINDVERTEXBUFFERSPROC)load("glBindVertexBuffers");
}
static void load_GL_VERSION_4_5(GLADloadproc load) {
    if (!GLAD_GL_VERSION_4_5) return;
    glad_glClipControl = (PFNGLCLIPCONTROLPROC)load("glClipControl");
    glad_glCreateTransformFeedbacks = (PFNGLCREATETRANSFORMFEEDBACKSPROC)load("glCreateTransformFeedbacks");
    glad_glTransformFeedbackBufferBase = (PFNGLTRANSFORMFEEDBACKBUFFERBASEPROC)load("glTransformFeedbackBufferBase");
    glad_glTransformFeedbackBufferRange = (PFNGLTRANSFORMFEEDBACKBUFFERRANGEPROC)load("glTransformFeedbackBufferRange");
    glad_glGetTransformFeedbackiv = (PFNGLGETTRANSFORMFEEDBACKIVPROC)load("glGetTransformFeedbackiv");
    glad_glGetTransformFeedbacki_v = (PFNGLGETTRANSFORMFEEDBACKI_VPROC)load("glGetTransformFeedbacki_v");
    glad_glGetTransformFeedbacki64_v = (PFNGLGETTRANSFORMFEEDBACKI64_VPROC)load("glGetTransformFeedbacki64_v");
    glad_glCreateBuffers = (PFNGLCREATEBUFFERSPROC)load("glCreateBuffers");
    glad_glNamedBufferStorage = (PFNGLNAMEDBUFFERSTORAGEPROC)load("glNamedBufferStorage");
    glad_glNamedBufferData = (PFNGLNAMEDBUFFERDATAPROC)load("glNamedBufferData");
    glad_glNamedBufferSubData = (PFNGLNAMEDBUFFERSUBDATAPROC)load("glNamedBufferSubData");
    glad_glCopyNamedBufferSubData = (PFNGLCOPYNAMEDBUFFERSUBDATAPROC)load("glCopyNamedBufferSubData");
    glad_glClearNamedBufferData = (PFNGLCLEARNAMEDBUFFERDATAPROC)load("glClearNamedBufferData");
    glad_glClearNamedBufferSubData = (PFNGLCLEARNAMEDBUFFERSUBDATAPROC)load("glClearNamedBufferSubData");
    glad_glMapNamedBuffer = (PFNGLMAPNAMEDBUFFERPROC)load("glMapNamedBuffer");
    glad_glMapNamedBufferRange = (PFNGLMAPNAMEDBUFFERRANGEPROC)load("glMapNamedBufferRange");
    glad_glUnmapNamedBuffer = (PFNGLUNMAPNAMEDBUFFERPROC)load("glUnmapNamedBuffer");
    glad_glFlushMappedNamedBufferRange = (PFNGLFLUSHMAPPEDNAMEDBUFFERRANGEPROC)load("glFlushMappedNamedBufferRange");
    glad_glGetNamedBufferParameteriv = (PFNGLGETNAMEDBUFFERPARAMETERIVPROC)load("glGetNamedBufferParameteriv");
    glad_glGetNamedBufferParameteri64v = (PFNGLGETNAMEDBUFFERPARAMETERI64VPROC)load("glGetNamedBufferParameteri64v");
    glad_glGetNamedBufferPointerv = (PFNGLGETNAMEDBUFFERPOINTERVPROC)load("glGetNamedBufferPointerv");
    glad_glGetNamedBufferSubData = (PFNGLGETNAMEDBUFFERSUBDATAPROC)load("glGetNamedBufferSubData");
    glad_glCreateFramebuffers = (PFNGLCREATEFRAMEBUFFERSPROC)load("glCreateFramebuffers");
    glad_glNamedFramebufferRenderbuffer = (PFNGLNAMEDFRAMEBUFFERRENDERBUFFERPROC)load("glNamedFramebufferRenderbuffer");
    glad_glNamedFramebufferParameteri = (PFNGLNAMEDFRAMEBUFFERPARAMETERIPROC)load("glNamedFramebufferParameteri");
    glad_glNamedFramebufferTexture = (PFNGLNAMEDFRAMEBUFFERTEXTUREPROC)load("glNamedFramebufferTexture");
    glad_glNamedFramebufferTextureLayer = (PFNGLNAMEDFRAMEBUFFERTEXTURELAYERPROC)load("glNamedFramebufferTextureLayer");
    glad_glNamedFramebufferDrawBuffer = (PFNGLNAMEDFRAMEBUFFERDRAWBUFFERPROC)load("glNamedFramebufferDrawBuffer");
    glad_glNamedFramebufferDrawBuffers = (PFNGLNAMEDFRAMEBUFFERDRAWBUFFERSPROC)load("glNamedFramebufferDrawBuffers");
    glad_glNamedFramebufferReadBuffer = (PFNGLNAMEDFRAMEBUFFERREADBUFFERPROC)load("glNamedFramebufferReadBuffer");
    glad_glInvalidateNamedFramebufferData = (PFNGLINVALIDATENAMEDFRAMEBUFFERDATAPROC)load("glInvalidateNamedFramebufferData");
    glad_glInvalidateNamedFramebufferSubData = (PFNGLINVALIDATENAMEDFRAMEBUFFERSUBDATAPROC)load("glInvalidateNamedFramebufferSubData");
    glad_glClearNamedFramebufferiv = (PFNGLCLEARNAMEDFRAMEBUFFERIVPROC)load("glClearNamedFramebufferiv");
    glad_glClearNamedFramebufferuiv = (PFNGLCLEARNAMEDFRAMEBUFFERUIVPROC)load("glClearNamedFramebufferuiv");
    glad_glClearNamedFramebufferfv = (PFNGLCLEARNAMEDFRAMEBUFFERFVPROC)load("glClearNamedFramebufferfv");
    glad_glClearNamedFramebufferfi = (PFNGLCLEARNAMEDFRAMEBUFFERFIPROC)load("glClearNamedFramebufferfi");
    glad_glBlitNamedFramebuffer = (PFNGLBLITNAMEDFRAMEBUFFERPROC)load("glBlitNamedFramebuffer");
    glad_glCheckNamedFramebufferStatus = (PFNGLCHECKNAMEDFRAMEBUFFERSTATUSPROC)load("glCheckNamedFramebufferStatus");
    glad_glGetNamedFramebufferParameteriv = (PFNGLGETNAMEDFRAMEBUFFERPARAMETERIVPROC)load("glGetNamedFramebufferParameteriv");
    glad_glGetNamedFramebufferAttachmentParameteriv = (PFNGLGETNAMEDFRAMEBUFFERATTACHMENTPARAMETERIVPROC)load("glGetNamedFramebufferAttachmentParameteriv");
    glad_glCreateRenderbuffers = (PFNGLCREATERENDERBUFFERSPROC)load("glCreateRenderbuffers");
    glad_glNamedRenderbufferStorage = (PFNGLNAMEDRENDERBUFFERSTORAGEPROC)load("glNamedRenderbufferStorage");
    glad_glNamedRenderbufferStorageMultisample = (PFNGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLEPROC)load("glNamedRenderbufferStorageMultisample");
    glad_glGetNamedRenderbufferParameteriv = (PFNGLGETNAMEDRENDERBUFFERPARAMETERIVPROC)load("glGetNamedRenderbufferParameteriv");
    glad_glCreateTextures = (PFNGLCREATETEXTURESPROC)load("glCreateTextures");
    glad_glTextureBuffer = (PFNGLTEXTUREBUFFERPROC)load("glTextureBuffer");
    glad_glTextureBufferRange = (PFNGLTEXTUREBUFFERRANGEPROC)load("glTextureBufferRange");
    glad_glTextureStorage1D = (PFNGLTEXTURESTORAGE1DPROC)load("glTextureStorage1D");
    glad_glTextureStorage2D = (PFNGLTEXTURESTORAGE2DPROC)load("glTextureStorage2D");
    glad_glTextureStorage3D = (PFNGLTEXTURESTORAGE3DPROC)load("glTextureStorage3D");
    glad_glTextureStorage2DMultisample = (PFNGLTEXTURESTORAGE2DMULTISAMPLEPROC)load("glTextureStorage2DMultisample");
    glad_glTextureStorage3DMultisample = (PFNGLTEXTURESTORAGE3DMULTISAMPLEPROC)load("glTextureStorage3DMultisample");
    glad_glTextureSubImage1D = (PFNGLTEXTURESUBIMAGE1DPROC)load("glTextureSubImage1D");
    glad_glTextureSubImage2D = (PFNGLTEXTURESUBIMAGE2DPROC)load("glTextureSubImage2D");
    glad_glTextureSubImage3D = (PFNGLTEXTURESUBIMAGE3DPROC)load("glTextureSubImage3D");
    glad_glCompressedTextureSubImage1D = (PFNGLCOMPRESSEDTEXTURESUBIMAGE1DPROC)load("glCompressedTextureSubImage1D");
    glad_glCompressedTextureSubImage2D = (PFNGLCOMPRESSEDTEXTURESUBIMAGE2DPROC)load("glCompressedTextureSubImage2D");
    glad_glCompressedTextureSubImage3D = (PFNGLCOMPRESSEDTEXTURESUBIMAGE3DPROC)load("glCompressedTextureSubImage3D");
    glad_glCopyTextureSubImage1D = (PFNGLCOPYTEXTURESUBIMAGE1DPROC)load("glCopyTextureSubImage1D");
    glad_glCopyTextureSubImage2D = (PFNGLCOPYTEXTURESUBIMAGE2DPROC)load("glCopyTextureSubImage2D");
    glad_glCopyTextureSubImage3D = (PFNGLCOPYTEXTURESUBIMAGE3DPROC)load("glCopyTextureSubImage3D");
    glad_glTextureParameterf = (PFNGLTEXTUREPARAMETERFPROC)load("glTextureParameterf");
    glad_glTextureParameterfv = (PFNGLTEXTUREPARAMETERFVPROC)load("glTextureParameterfv");
    glad_glTextureParameteri = (PFNGLTEXTUREPARAMETERIPROC)load("glTextureParameteri");
    glad_glTextureParameterIiv = (PFNGLTEXTUREPARAMETERIIVPROC)load("glTextureParameterIiv");
    glad_glTextureParameterIuiv = (PFNGLTEXTUREPARAMETERIUIVPROC)load("glTextureParameterIuiv");
    glad_glTextureParameteriv = (PFNGLTEXTUREPARAMETERIVPROC)load("glTextureParameteriv");
    glad_glGenerateTextureMipmap = (PFNGLGENERATETEXTUREMIPMAPPROC)load("glGenerateTextureMipmap");
    glad_glBindTextureUnit = (PFNGLBINDTEXTUREUNITPROC)load("glBindTextureUnit");
    glad_glGetTextureImage = (PFNGLGETTEXTUREIMAGEPROC)load("glGetTextureImage");
    glad_glGetCompressedTextureImage = (PFNGLGETCOMPRESSEDTEXTUREIMAGEPROC)load("glGetCompressedTextureImage");
    glad_glGetTextureLevelParameterfv = (PFNGLGETTEXTURELEVELPARAMETERFVPROC)load("glGetTextureLevelParameterfv");
    glad_glGetTextureLevelParameteriv = (PFNGLGETTEXTURELEVELPARAMETERIVPROC)load("glGetTextureLevelParameteriv");
    glad_glGetTextureParameterfv = (PFNGLGETTEXTUREPARAMETERFVPROC)load("glGetTextureParameterfv");
    glad_glGetTextureParameterIiv = (PFNGLGETTEXTUREPARAMETERIIVPROC)load("glGetTextureParameterIiv");
    glad_glGetTextureParameterIuiv = (PFNGLGETTEXTUREPARAMETERIUIVPROC)load("glGetTextureParameterIuiv");
    glad_glGetTextureParameteriv = (PFNGLGETTEXTUREPARAMETERIVPROC)load("glGetTextureParameteriv");
    glad_glCreateVertexArrays = (PFNGLCREATEVERTEXARRAYSPROC)load("glCreateVertexArrays");
    glad_glDisableVertexArrayAttrib = (PFNGLDISABLEVERTEXARRAYATTRIBPROC)load("glDisableVertexArrayAttrib");
    glad_glEnableVertexArrayAttrib = (PFNGLENABLEVERTEXARRAYATTRIBPROC)load("glEnableVertexArrayAttrib");
    glad_glVertexArrayElementBuffer = (PFNGLVERTEXARRAYELEMENTBUFFERPROC)load("glVertexArrayElementBuffer");
    glad_glVertexArrayVertexBuffer = (PFNGLVERTEXARRAYVERTEXBUFFERPROC)load("glVertexArrayVertexBuffer");
    glad_glVertexArrayVertexBuffers = (PFNGLVERTEXARRAYVERTEXBUFFERSPROC)load("glVertexArrayVertexBuffers");
    glad_glVertexArrayAttribBinding = (PFNGLVERTEXARRAYATTRIBBINDINGPROC)load("glVertexArrayAttribBinding");
    glad_glVertexArrayAttribFormat = (PFNGLVERTEXARRAYATTRIBFORMATPROC)load("glVertexArrayAttribFormat");
    glad_glVertexArrayAttribIFormat = (PFNGLVERTEXARRAYATTRIBIFORMATPROC)load("glVertexArrayAttribIFormat");
    glad_glVertexArrayAttribLFormat = (PFNGLVERTEXARRAYATTRIBLFORMATPROC)load("glVertexArrayAttribLFormat");
    glad_glVertexArrayBindingDivisor = (PFNGLVERTEXARRAYBINDINGDIVISORPROC)load("glVertexArrayBindingDivisor");
    glad_glGetVertexArrayiv = (PFNGLGETVERTEXARRAYIVPROC)load("glGetVertexArrayiv");
    glad_glGetVertexArrayIndexediv = (PFNGLGETVERTEXARRAYINDEXEDIVPROC)load("glGetVertexArrayIndexediv");
    glad_glGetVertexArrayIndexed64iv = (PFNGLGETVERTEXARRAYINDEXED64IVPROC)load("glGetVertexArrayIndexed64iv");
    glad_glCreateSamplers = (PFNGLCREATESAMPLERSPROC)load("glCreateSamplers");
    glad_glCreateProgramPipelines = (PFNGLCREATEPROGRAMPIPELINESPROC)load("glCreateProgramPipelines");
    glad_glCreateQueries = (PFNGLCREATEQUERIESPROC)load("glCreateQueries");
    glad_glGetQueryBufferObjecti64v = (PFNGLGETQUERYBUFFEROBJECTI64VPROC)load("glGetQueryBufferObjecti64v");
    glad_glGetQueryBufferObjectiv = (PFNGLGETQUERYBUFFEROBJECTIVPROC)load("glGetQueryBufferObjectiv");
    glad_glGetQueryBufferObjectui64v = (PFNGLGETQUERYBUFFEROBJECTUI64VPROC)load("glGetQueryBufferObjectui64v");
    glad_glGetQueryBufferObjectuiv = (PFNGLGETQUERYBUFFEROBJECTUIVPROC)load("glGetQueryBufferObjectuiv");
    glad_glMemoryBarrierByRegion = (PFNGLMEMORYBARRIERBYREGIONPROC)load("glMemoryBarrierByRegion");
    glad_glGetTextureSubImage = (PFNGLGETTEXTURESUBIMAGEPROC)load("glGetTextureSubImage");
    glad_glGetCompressedTextureSubImage = (PFNGLGETCOMPRESSEDTEXTURESUBIMAGEPROC)load("glGetCompressedTextureSubImage");
    glad_glGetGraphicsResetStatus = (PFNGLGETGRAPHICSRESETSTATUSPROC)load("glGetGraphicsResetStatus");
    glad_glGetnCompressedTexImage = (PFNGLGETNCOMPRESSEDTEXIMAGEPROC)load("glGetnCompressedTexImage");
    glad_glGetnTexImage = (PFNGLGETNTEXIMAGEPROC)load("glGetnTexImage");
    glad_glGetnUniformdv = (PFNGLGETNUNIFORMDVPROC)load("glGetnUniformdv");
    glad_glGetnUniformfv = (PFNGLGETNUNIFORMFVPROC)load("glGetnUniformfv");
    glad_glGetnUniformiv = (PFNGLGETNUNIFORMIVPROC)load("glGetnUniformiv");
    glad_glGetnUniformuiv = (PFNGLGETNUNIFORMUIVPROC)load("glGetnUniformuiv");
    glad_glReadnPixels = (PFNGLREADNPIXELSPROC)load("glReadnPixels");
    glad_glGetnMapdv = (PFNGLGETNMAPDVPROC)load("glGetnMapdv");
    glad_glGetnMapfv = (PFNGLGETNMAPFVPROC)load("glGetnMapfv");
    glad_glGetnMapiv = (PFNGLGETNMAPIVPROC)load("glGetnMapiv");
    glad_glGetnPixelMapfv = (PFNGLGETNPIXELMAPFVPROC)load("glGetnPixelMapfv");
    glad_glGetnPixelMapuiv = (PFNGLGETNPIXELMAPUIVPROC)load("glGetnPixelMapuiv");
    glad_glGetnPixelMapusv = (PFNGLGETNPIXELMAPUSVPROC)load("glGetnPixelMapusv");
    glad_glGetnPolygonStipple = (PFNGLGETNPOLYGONSTIPPLEPROC)load("glGetnPolygonStipple");
    glad_glGetnColorTable = (PFNGLGETNCOLORTABLEPROC)load("glGetnColorTable");
    glad_glGetnConvolutionFilter = (PFNGLGETNCONVOLUTIONFILTERPROC)load("glGetnConvolutionFilter");
    glad_glGetnSeparableFilter = (PFNGLGETNSEPARABLEFILTERPROC)load("glGetnSeparableFilter");
    glad_glGetnHistogram = (PFNGLGETNHISTOGRAMPROC)load("glGetnHistogram");
    glad_glGetnMinmax = (PFNGLGETNMINMAXPROC)load("glGetnMinmax");
    glad_glTextureBarrier = (PFNGLTEXTUREBARRIERPROC)load("glTextureBarrier");
}
static int find_extensionsGL(void) {
    if (!get_exts()) return 0;
    (void)&has_ext;
    free_exts();
    return 1;
}

static void find_coreGL(void) {

    /* Thank you @elmindreda
     * https://github.com/elmindreda/greg/blob/master/templates/greg.c.in#L176
     * https://github.com/glfw/glfw/blob/master/src/context.c#L36
     */
    int i, major, minor;

    const char *version;
    const char *prefixes[] = {
        "OpenGL ES-CM ",
        "OpenGL ES-CL ",
        "OpenGL ES ",
        NULL
    };

    version = (const char *)glGetString(GL_VERSION);
    if (!version) return;

    for (i = 0; prefixes[i]; i++) {
        const size_t length = strlen(prefixes[i]);
        if (strncmp(version, prefixes[i], length) == 0) {
            version += length;
            break;
        }
    }

    /* PR #18 */
#ifdef _MSC_VER
    sscanf_s(version, "%d.%d", &major, &minor);
#else
    sscanf(version, "%d.%d", &major, &minor);
#endif

    GLVersion.major = major; GLVersion.minor = minor;
    max_loaded_major = major; max_loaded_minor = minor;
    GLAD_GL_VERSION_1_0 = (major == 1 && minor >= 0) || major > 1;
    GLAD_GL_VERSION_1_1 = (major == 1 && minor >= 1) || major > 1;
    GLAD_GL_VERSION_1_2 = (major == 1 && minor >= 2) || major > 1;
    GLAD_GL_VERSION_1_3 = (major == 1 && minor >= 3) || major > 1;
    GLAD_GL_VERSION_1_4 = (major == 1 && minor >= 4) || major > 1;
    GLAD_GL_VERSION_1_5 = (major == 1 && minor >= 5) || major > 1;
    GLAD_GL_VERSION_2_0 = (major == 2 && minor >= 0) || major > 2;
    GLAD_GL_VERSION_2_1 = (major == 2 && minor >= 1) || major > 2;
    GLAD_GL_VERSION_3_0 = (major == 3 && minor >= 0) || major > 3;
    GLAD_GL_VERSION_3_1 = (major == 3 && minor >= 1) || major > 3;
    GLAD_GL_VERSION_3_2 = (major == 3 && minor >= 2) || major > 3;
    GLAD_GL_VERSION_3_3 = (major == 3 && minor >= 3) || major > 3;
    GLAD_GL_VERSION_4_0 = (major == 4 && minor >= 0) || major > 4;
    GLAD_GL_VERSION_4_1 = (major == 4 && minor >= 1) || major > 4;
    GLAD_GL_VERSION_4_2 = (major == 4 && minor >= 2) || major > 4;
    GLAD_GL_VERSION_4_3 = (major == 4 && minor >= 3) || major > 4;
    GLAD_GL_VERSION_4_4 = (major == 4 && minor >= 4) || major > 4;
    GLAD_GL_VERSION_4_5 = (major == 4 && minor >= 5) || major > 4;
    if (GLVersion.major > 4 || (GLVersion.major >= 4 && GLVersion.minor >= 5)) {
        max_loaded_major = 4;
        max_loaded_minor = 5;
    }
}

int gladLoadGLLoader(GLADloadproc load) {
    GLVersion.major = 0; GLVersion.minor = 0;
    glGetString = (PFNGLGETSTRINGPROC)load("glGetString");
    if (glGetString == NULL) return 0;
    if (glGetString(GL_VERSION) == NULL) return 0;
    find_coreGL();
    load_GL_VERSION_1_0(load);
    load_GL_VERSION_1_1(load);
    load_GL_VERSION_1_2(load);
    load_GL_VERSION_1_3(load);
    load_GL_VERSION_1_4(load);
    load_GL_VERSION_1_5(load);
    load_GL_VERSION_2_0(load);
    load_GL_VERSION_2_1(load);
    load_GL_VERSION_3_0(load);
    load_GL_VERSION_3_1(load);
    load_GL_VERSION_3_2(load);
    load_GL_VERSION_3_3(load);
    load_GL_VERSION_4_0(load);
    load_GL_VERSION_4_1(load);
    load_GL_VERSION_4_2(load);
    load_GL_VERSION_4_3(load);
    load_GL_VERSION_4_4(load);
    load_GL_VERSION_4_5(load);

    if (!find_extensionsGL()) return 0;
    return GLVersion.major != 0 || GLVersion.minor != 0;
}


#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glad.h"

void _pre_call_callback_default(const char *name, void *funcptr, int len_args, ...) {
    (void) name;
    (void) funcptr;
    (void) len_args;
}
void _post_call_callback_default(const char *name, void *funcptr, int len_args, ...) {
    GLenum error_code;

    (void) funcptr;
    (void) len_args;

    error_code = glad_glGetError();

    if (error_code != GL_NO_ERROR) {
        fprintf(stderr, "ERROR %d in %s\n", error_code, name);
    }
}

static GLADcallback _pre_call_callback = _pre_call_callback_default;
void glad_set_pre_callback(GLADcallback cb) {
    _pre_call_callback = cb;
}

static GLADcallback _post_call_callback = _post_call_callback_default;
void glad_set_post_callback(GLADcallback cb) {
    _post_call_callback = cb;
}

static void* get_proc(const char *namez);

#if defined(_WIN32) || defined(__CYGWIN__)
#ifndef _WINDOWS_
#undef APIENTRY
#endif
#include <windows.h>
static HMODULE libGL;

typedef void* (APIENTRYP PFNWGLGETPROCADDRESSPROC_PRIVATE)(const char*);
static PFNWGLGETPROCADDRESSPROC_PRIVATE gladGetProcAddressPtr;

#ifdef _MSC_VER
#ifdef __has_include
  #if __has_include(<winapifamily.h>)
    #define HAVE_WINAPIFAMILY 1
  #endif
#elif _MSC_VER >= 1700 && !_USING_V110_SDK71_
  #define HAVE_WINAPIFAMILY 1
#endif
#endif

#ifdef HAVE_WINAPIFAMILY
  #include <winapifamily.h>
  #if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP)
    #define IS_UWP 1
  #endif
#endif

static
int open_gl(void) {
#ifndef IS_UWP
    libGL = LoadLibraryW(L"opengl32.dll");
    if(libGL != NULL) {
        void (* tmp)(void);
        tmp = (void(*)(void)) GetProcAddress(libGL, "wglGetProcAddress");
        gladGetProcAddressPtr = (PFNWGLGETPROCADDRESSPROC_PRIVATE) tmp;
        return gladGetProcAddressPtr != NULL;
    }
#endif

    return 0;
}

static
void close_gl(void) {
    if(libGL != NULL) {
        FreeLibrary((HMODULE) libGL);
        libGL = NULL;
    }
}
#else
#include <dlfcn.h>
static void* libGL;

#if !defined(__APPLE__) && !defined(__HAIKU__)
typedef void* (APIENTRYP PFNGLXGETPROCADDRESSPROC_PRIVATE)(const char*);
static PFNGLXGETPROCADDRESSPROC_PRIVATE gladGetProcAddressPtr;
#endif

static
int open_gl(void) {
#ifdef __APPLE__
    static const char *NAMES[] = {
        "../Frameworks/OpenGL.framework/OpenGL",
        "/Library/Frameworks/OpenGL.framework/OpenGL",
        "/System/Library/Frameworks/OpenGL.framework/OpenGL",
        "/System/Library/Frameworks/OpenGL.framework/Versions/Current/OpenGL"
    };
#else
    static const char *NAMES[] = {"libGL.so.1", "libGL.so"};
#endif

    unsigned int index = 0;
    for(index = 0; index < (sizeof(NAMES) / sizeof(NAMES[0])); index++) {
        libGL = dlopen(NAMES[index], RTLD_NOW | RTLD_GLOBAL);

        if(libGL != NULL) {
#if defined(__APPLE__) || defined(__HAIKU__)
            return 1;
#else
            gladGetProcAddressPtr = (PFNGLXGETPROCADDRESSPROC_PRIVATE)dlsym(libGL,
                "glXGetProcAddressARB");
            return gladGetProcAddressPtr != NULL;
#endif
        }
    }

    return 0;
}

static
void close_gl(void) {
    if(libGL != NULL) {
        dlclose(libGL);
        libGL = NULL;
    }
}
#endif

static
void* get_proc(const char *namez) {
    void* result = NULL;
    if(libGL == NULL) return NULL;

#if !defined(__APPLE__) && !defined(__HAIKU__)
    if(gladGetProcAddressPtr != NULL) {
        result = gladGetProcAddressPtr(namez);
    }
#endif
    if(result == NULL) {
#if defined(_WIN32) || defined(__CYGWIN__)
        result = (void*)GetProcAddress((HMODULE) libGL, namez);
#else
        result = dlsym(libGL, namez);
#endif
    }

    return result;
}

int gladLoadGL(void) {
    int status = 0;

    if(open_gl()) {
        status = gladLoadGLLoader(&get_proc);
        close_gl();
    }

    return status;
}

struct gladGLversionStruct GLVersion = { 0, 0 };

#if defined(GL_ES_VERSION_3_0) || defined(GL_VERSION_3_0)
#define _GLAD_IS_SOME_NEW_VERSION 1
#endif

static int max_loaded_major;
static int max_loaded_minor;

static const char *exts = NULL;
static int num_exts_i = 0;
static char **exts_i = NULL;

static int get_exts(void) {
#ifdef _GLAD_IS_SOME_NEW_VERSION
    if(max_loaded_major < 3) {
#endif
        exts = (const char *)glGetString(GL_EXTENSIONS);
#ifdef _GLAD_IS_SOME_NEW_VERSION
    } else {
        unsigned int index;

        num_exts_i = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &num_exts_i);
        if (num_exts_i > 0) {
            exts_i = (char **)malloc((size_t)num_exts_i * (sizeof *exts_i));
        }

        if (exts_i == NULL) {
            return 0;
        }

        for(index = 0; index < (unsigned)num_exts_i; index++) {
            const char *gl_str_tmp = (const char*)glGetStringi(GL_EXTENSIONS, index);
            size_t len = strlen(gl_str_tmp);

            char *local_str = (char*)malloc((len+1) * sizeof(char));
            if(local_str != NULL) {
                memcpy(local_str, gl_str_tmp, (len+1) * sizeof(char));
            }
            exts_i[index] = local_str;
        }
    }
#endif
    return 1;
}

static void free_exts(void) {
    if (exts_i != NULL) {
        int index;
        for(index = 0; index < num_exts_i; index++) {
            free((char *)exts_i[index]);
        }
        free((void *)exts_i);
        exts_i = NULL;
    }
}

static int has_ext(const char *ext) {
#ifdef _GLAD_IS_SOME_NEW_VERSION
    if(max_loaded_major < 3) {
#endif
        const char *extensions;
        const char *loc;
        const char *terminator;
        extensions = exts;
        if(extensions == NULL || ext == NULL) {
            return 0;
        }

        while(1) {
            loc = strstr(extensions, ext);
            if(loc == NULL) {
                return 0;
            }

            terminator = loc + strlen(ext);
            if((loc == extensions || *(loc - 1) == ' ') &&
                (*terminator == ' ' || *terminator == '\0')) {
                return 1;
            }
            extensions = terminator;
        }
#ifdef _GLAD_IS_SOME_NEW_VERSION
    } else {
        int index;
        if(exts_i == NULL) return 0;
        for(index = 0; index < num_exts_i; index++) {
            const char *e = exts_i[index];

            if(exts_i[index] != NULL && strcmp(e, ext) == 0) {
                return 1;
            }
        }
    }
#endif

    return 0;
}
int GLAD_GL_VERSION_1_0 = 0;
int GLAD_GL_VERSION_1_1 = 0;
int GLAD_GL_VERSION_1_2 = 0;
int GLAD_GL_VERSION_1_3 = 0;
int GLAD_GL_VERSION_1_4 = 0;
int GLAD_GL_VERSION_1_5 = 0;
int GLAD_GL_VERSION_2_0 = 0;
int GLAD_GL_VERSION_2_1 = 0;
int GLAD_GL_VERSION_3_0 = 0;
int GLAD_GL_VERSION_3_1 = 0;
int GLAD_GL_VERSION_3_2 = 0;
int GLAD_GL_VERSION_3_3 = 0;
int GLAD_GL_VERSION_4_0 = 0;
int GLAD_GL_VERSION_4_1 = 0;
int GLAD_GL_VERSION_4_2 = 0;
int GLAD_GL_VERSION_4_3 = 0;
int GLAD_GL_VERSION_4_4 = 0;
int GLAD_GL_VERSION_4_5 = 0;
PFNGLACCUMPROC glad_glAccum;
void APIENTRY glad_debug_impl_glAccum(GLenum arg0, GLfloat arg1) {    
    _pre_call_callback("glAccum", (void*)glAccum, 2, arg0, arg1);
     glad_glAccum(arg0, arg1);
    _post_call_callback("glAccum", (void*)glAccum, 2, arg0, arg1);
    
}
PFNGLACCUMPROC glad_debug_glAccum = glad_debug_impl_glAccum;
PFNGLACTIVESHADERPROGRAMPROC glad_glActiveShaderProgram;
void APIENTRY glad_debug_impl_glActiveShaderProgram(GLuint arg0, GLuint arg1) {    
    _pre_call_callback("glActiveShaderProgram", (void*)glActiveShaderProgram, 2, arg0, arg1);
     glad_glActiveShaderProgram(arg0, arg1);
    _post_call_callback("glActiveShaderProgram", (void*)glActiveShaderProgram, 2, arg0, arg1);
    
}
PFNGLACTIVESHADERPROGRAMPROC glad_debug_glActiveShaderProgram = glad_debug_impl_glActiveShaderProgram;
PFNGLACTIVETEXTUREPROC glad_glActiveTexture;
void APIENTRY glad_debug_impl_glActiveTexture(GLenum arg0) {    
    _pre_call_callback("glActiveTexture", (void*)glActiveTexture, 1, arg0);
     glad_glActiveTexture(arg0);
    _post_call_callback("glActiveTexture", (void*)glActiveTexture, 1, arg0);
    
}
PFNGLACTIVETEXTUREPROC glad_debug_glActiveTexture = glad_debug_impl_glActiveTexture;
PFNGLALPHAFUNCPROC glad_glAlphaFunc;
void APIENTRY glad_debug_impl_glAlphaFunc(GLenum arg0, GLfloat arg1) {    
    _pre_call_callback("glAlphaFunc", (void*)glAlphaFunc, 2, arg0, arg1);
     glad_glAlphaFunc(arg0, arg1);
    _post_call_callback("glAlphaFunc", (void*)glAlphaFunc, 2, arg0, arg1);
    
}
PFNGLALPHAFUNCPROC glad_debug_glAlphaFunc = glad_debug_impl_glAlphaFunc;
PFNGLARETEXTURESRESIDENTPROC glad_glAreTexturesResident;
GLboolean APIENTRY glad_debug_impl_glAreTexturesResident(GLsizei arg0, const GLuint * arg1, GLboolean * arg2) {    
    GLboolean ret;
    _pre_call_callback("glAreTexturesResident", (void*)glAreTexturesResident, 3, arg0, arg1, arg2);
    ret =  glad_glAreTexturesResident(arg0, arg1, arg2);
    _post_call_callback("glAreTexturesResident", (void*)glAreTexturesResident, 3, arg0, arg1, arg2);
    return ret;
}
PFNGLARETEXTURESRESIDENTPROC glad_debug_glAreTexturesResident = glad_debug_impl_glAreTexturesResident;
PFNGLARRAYELEMENTPROC glad_glArrayElement;
void APIENTRY glad_debug_impl_glArrayElement(GLint arg0) {    
    _pre_call_callback("glArrayElement", (void*)glArrayElement, 1, arg0);
     glad_glArrayElement(arg0);
    _post_call_callback("glArrayElement", (void*)glArrayElement, 1, arg0);
    
}
PFNGLARRAYELEMENTPROC glad_debug_glArrayElement = glad_debug_impl_glArrayElement;
PFNGLATTACHSHADERPROC glad_glAttachShader;
void APIENTRY glad_debug_impl_glAttachShader(GLuint arg0, GLuint arg1) {    
    _pre_call_callback("glAttachShader", (void*)glAttachShader, 2, arg0, arg1);
     glad_glAttachShader(arg0, arg1);
    _post_call_callback("glAttachShader", (void*)glAttachShader, 2, arg0, arg1);
    
}
PFNGLATTACHSHADERPROC glad_debug_glAttachShader = glad_debug_impl_glAttachShader;
PFNGLBEGINPROC glad_glBegin;
void APIENTRY glad_debug_impl_glBegin(GLenum arg0) {    
    _pre_call_callback("glBegin", (void*)glBegin, 1, arg0);
     glad_glBegin(arg0);
    _post_call_callback("glBegin", (void*)glBegin, 1, arg0);
    
}
PFNGLBEGINPROC glad_debug_glBegin = glad_debug_impl_glBegin;
PFNGLBEGINCONDITIONALRENDERPROC glad_glBeginConditionalRender;
void APIENTRY glad_debug_impl_glBeginConditionalRender(GLuint arg0, GLenum arg1) {    
    _pre_call_callback("glBeginConditionalRender", (void*)glBeginConditionalRender, 2, arg0, arg1);
     glad_glBeginConditionalRender(arg0, arg1);
    _post_call_callback("glBeginConditionalRender", (void*)glBeginConditionalRender, 2, arg0, arg1);
    
}
PFNGLBEGINCONDITIONALRENDERPROC glad_debug_glBeginConditionalRender = glad_debug_impl_glBeginConditionalRender;
PFNGLBEGINQUERYPROC glad_glBeginQuery;
void APIENTRY glad_debug_impl_glBeginQuery(GLenum arg0, GLuint arg1) {    
    _pre_call_callback("glBeginQuery", (void*)glBeginQuery, 2, arg0, arg1);
     glad_glBeginQuery(arg0, arg1);
    _post_call_callback("glBeginQuery", (void*)glBeginQuery, 2, arg0, arg1);
    
}
PFNGLBEGINQUERYPROC glad_debug_glBeginQuery = glad_debug_impl_glBeginQuery;
PFNGLBEGINQUERYINDEXEDPROC glad_glBeginQueryIndexed;
void APIENTRY glad_debug_impl_glBeginQueryIndexed(GLenum arg0, GLuint arg1, GLuint arg2) {    
    _pre_call_callback("glBeginQueryIndexed", (void*)glBeginQueryIndexed, 3, arg0, arg1, arg2);
     glad_glBeginQueryIndexed(arg0, arg1, arg2);
    _post_call_callback("glBeginQueryIndexed", (void*)glBeginQueryIndexed, 3, arg0, arg1, arg2);
    
}
PFNGLBEGINQUERYINDEXEDPROC glad_debug_glBeginQueryIndexed = glad_debug_impl_glBeginQueryIndexed;
PFNGLBEGINTRANSFORMFEEDBACKPROC glad_glBeginTransformFeedback;
void APIENTRY glad_debug_impl_glBeginTransformFeedback(GLenum arg0) {    
    _pre_call_callback("glBeginTransformFeedback", (void*)glBeginTransformFeedback, 1, arg0);
     glad_glBeginTransformFeedback(arg0);
    _post_call_callback("glBeginTransformFeedback", (void*)glBeginTransformFeedback, 1, arg0);
    
}
PFNGLBEGINTRANSFORMFEEDBACKPROC glad_debug_glBeginTransformFeedback = glad_debug_impl_glBeginTransformFeedback;
PFNGLBINDATTRIBLOCATIONPROC glad_glBindAttribLocation;
void APIENTRY glad_debug_impl_glBindAttribLocation(GLuint arg0, GLuint arg1, const GLchar * arg2) {    
    _pre_call_callback("glBindAttribLocation", (void*)glBindAttribLocation, 3, arg0, arg1, arg2);
     glad_glBindAttribLocation(arg0, arg1, arg2);
    _post_call_callback("glBindAttribLocation", (void*)glBindAttribLocation, 3, arg0, arg1, arg2);
    
}
PFNGLBINDATTRIBLOCATIONPROC glad_debug_glBindAttribLocation = glad_debug_impl_glBindAttribLocation;
PFNGLBINDBUFFERPROC glad_glBindBuffer;
void APIENTRY glad_debug_impl_glBindBuffer(GLenum arg0, GLuint arg1) {    
    _pre_call_callback("glBindBuffer", (void*)glBindBuffer, 2, arg0, arg1);
     glad_glBindBuffer(arg0, arg1);
    _post_call_callback("glBindBuffer", (void*)glBindBuffer, 2, arg0, arg1);
    
}
PFNGLBINDBUFFERPROC glad_debug_glBindBuffer = glad_debug_impl_glBindBuffer;
PFNGLBINDBUFFERBASEPROC glad_glBindBufferBase;
void APIENTRY glad_debug_impl_glBindBufferBase(GLenum arg0, GLuint arg1, GLuint arg2) {    
    _pre_call_callback("glBindBufferBase", (void*)glBindBufferBase, 3, arg0, arg1, arg2);
     glad_glBindBufferBase(arg0, arg1, arg2);
    _post_call_callback("glBindBufferBase", (void*)glBindBufferBase, 3, arg0, arg1, arg2);
    
}
PFNGLBINDBUFFERBASEPROC glad_debug_glBindBufferBase = glad_debug_impl_glBindBufferBase;
PFNGLBINDBUFFERRANGEPROC glad_glBindBufferRange;
void APIENTRY glad_debug_impl_glBindBufferRange(GLenum arg0, GLuint arg1, GLuint arg2, GLintptr arg3, GLsizeiptr arg4) {    
    _pre_call_callback("glBindBufferRange", (void*)glBindBufferRange, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glBindBufferRange(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glBindBufferRange", (void*)glBindBufferRange, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLBINDBUFFERRANGEPROC glad_debug_glBindBufferRange = glad_debug_impl_glBindBufferRange;
PFNGLBINDBUFFERSBASEPROC glad_glBindBuffersBase;
void APIENTRY glad_debug_impl_glBindBuffersBase(GLenum arg0, GLuint arg1, GLsizei arg2, const GLuint * arg3) {    
    _pre_call_callback("glBindBuffersBase", (void*)glBindBuffersBase, 4, arg0, arg1, arg2, arg3);
     glad_glBindBuffersBase(arg0, arg1, arg2, arg3);
    _post_call_callback("glBindBuffersBase", (void*)glBindBuffersBase, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLBINDBUFFERSBASEPROC glad_debug_glBindBuffersBase = glad_debug_impl_glBindBuffersBase;
PFNGLBINDBUFFERSRANGEPROC glad_glBindBuffersRange;
void APIENTRY glad_debug_impl_glBindBuffersRange(GLenum arg0, GLuint arg1, GLsizei arg2, const GLuint * arg3, const GLintptr * arg4, const GLsizeiptr * arg5) {    
    _pre_call_callback("glBindBuffersRange", (void*)glBindBuffersRange, 6, arg0, arg1, arg2, arg3, arg4, arg5);
     glad_glBindBuffersRange(arg0, arg1, arg2, arg3, arg4, arg5);
    _post_call_callback("glBindBuffersRange", (void*)glBindBuffersRange, 6, arg0, arg1, arg2, arg3, arg4, arg5);
    
}
PFNGLBINDBUFFERSRANGEPROC glad_debug_glBindBuffersRange = glad_debug_impl_glBindBuffersRange;
PFNGLBINDFRAGDATALOCATIONPROC glad_glBindFragDataLocation;
void APIENTRY glad_debug_impl_glBindFragDataLocation(GLuint arg0, GLuint arg1, const GLchar * arg2) {    
    _pre_call_callback("glBindFragDataLocation", (void*)glBindFragDataLocation, 3, arg0, arg1, arg2);
     glad_glBindFragDataLocation(arg0, arg1, arg2);
    _post_call_callback("glBindFragDataLocation", (void*)glBindFragDataLocation, 3, arg0, arg1, arg2);
    
}
PFNGLBINDFRAGDATALOCATIONPROC glad_debug_glBindFragDataLocation = glad_debug_impl_glBindFragDataLocation;
PFNGLBINDFRAGDATALOCATIONINDEXEDPROC glad_glBindFragDataLocationIndexed;
void APIENTRY glad_debug_impl_glBindFragDataLocationIndexed(GLuint arg0, GLuint arg1, GLuint arg2, const GLchar * arg3) {    
    _pre_call_callback("glBindFragDataLocationIndexed", (void*)glBindFragDataLocationIndexed, 4, arg0, arg1, arg2, arg3);
     glad_glBindFragDataLocationIndexed(arg0, arg1, arg2, arg3);
    _post_call_callback("glBindFragDataLocationIndexed", (void*)glBindFragDataLocationIndexed, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLBINDFRAGDATALOCATIONINDEXEDPROC glad_debug_glBindFragDataLocationIndexed = glad_debug_impl_glBindFragDataLocationIndexed;
PFNGLBINDFRAMEBUFFERPROC glad_glBindFramebuffer;
void APIENTRY glad_debug_impl_glBindFramebuffer(GLenum arg0, GLuint arg1) {    
    _pre_call_callback("glBindFramebuffer", (void*)glBindFramebuffer, 2, arg0, arg1);
     glad_glBindFramebuffer(arg0, arg1);
    _post_call_callback("glBindFramebuffer", (void*)glBindFramebuffer, 2, arg0, arg1);
    
}
PFNGLBINDFRAMEBUFFERPROC glad_debug_glBindFramebuffer = glad_debug_impl_glBindFramebuffer;
PFNGLBINDIMAGETEXTUREPROC glad_glBindImageTexture;
void APIENTRY glad_debug_impl_glBindImageTexture(GLuint arg0, GLuint arg1, GLint arg2, GLboolean arg3, GLint arg4, GLenum arg5, GLenum arg6) {    
    _pre_call_callback("glBindImageTexture", (void*)glBindImageTexture, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
     glad_glBindImageTexture(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    _post_call_callback("glBindImageTexture", (void*)glBindImageTexture, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    
}
PFNGLBINDIMAGETEXTUREPROC glad_debug_glBindImageTexture = glad_debug_impl_glBindImageTexture;
PFNGLBINDIMAGETEXTURESPROC glad_glBindImageTextures;
void APIENTRY glad_debug_impl_glBindImageTextures(GLuint arg0, GLsizei arg1, const GLuint * arg2) {    
    _pre_call_callback("glBindImageTextures", (void*)glBindImageTextures, 3, arg0, arg1, arg2);
     glad_glBindImageTextures(arg0, arg1, arg2);
    _post_call_callback("glBindImageTextures", (void*)glBindImageTextures, 3, arg0, arg1, arg2);
    
}
PFNGLBINDIMAGETEXTURESPROC glad_debug_glBindImageTextures = glad_debug_impl_glBindImageTextures;
PFNGLBINDPROGRAMPIPELINEPROC glad_glBindProgramPipeline;
void APIENTRY glad_debug_impl_glBindProgramPipeline(GLuint arg0) {    
    _pre_call_callback("glBindProgramPipeline", (void*)glBindProgramPipeline, 1, arg0);
     glad_glBindProgramPipeline(arg0);
    _post_call_callback("glBindProgramPipeline", (void*)glBindProgramPipeline, 1, arg0);
    
}
PFNGLBINDPROGRAMPIPELINEPROC glad_debug_glBindProgramPipeline = glad_debug_impl_glBindProgramPipeline;
PFNGLBINDRENDERBUFFERPROC glad_glBindRenderbuffer;
void APIENTRY glad_debug_impl_glBindRenderbuffer(GLenum arg0, GLuint arg1) {    
    _pre_call_callback("glBindRenderbuffer", (void*)glBindRenderbuffer, 2, arg0, arg1);
     glad_glBindRenderbuffer(arg0, arg1);
    _post_call_callback("glBindRenderbuffer", (void*)glBindRenderbuffer, 2, arg0, arg1);
    
}
PFNGLBINDRENDERBUFFERPROC glad_debug_glBindRenderbuffer = glad_debug_impl_glBindRenderbuffer;
PFNGLBINDSAMPLERPROC glad_glBindSampler;
void APIENTRY glad_debug_impl_glBindSampler(GLuint arg0, GLuint arg1) {    
    _pre_call_callback("glBindSampler", (void*)glBindSampler, 2, arg0, arg1);
     glad_glBindSampler(arg0, arg1);
    _post_call_callback("glBindSampler", (void*)glBindSampler, 2, arg0, arg1);
    
}
PFNGLBINDSAMPLERPROC glad_debug_glBindSampler = glad_debug_impl_glBindSampler;
PFNGLBINDSAMPLERSPROC glad_glBindSamplers;
void APIENTRY glad_debug_impl_glBindSamplers(GLuint arg0, GLsizei arg1, const GLuint * arg2) {    
    _pre_call_callback("glBindSamplers", (void*)glBindSamplers, 3, arg0, arg1, arg2);
     glad_glBindSamplers(arg0, arg1, arg2);
    _post_call_callback("glBindSamplers", (void*)glBindSamplers, 3, arg0, arg1, arg2);
    
}
PFNGLBINDSAMPLERSPROC glad_debug_glBindSamplers = glad_debug_impl_glBindSamplers;
PFNGLBINDTEXTUREPROC glad_glBindTexture;
void APIENTRY glad_debug_impl_glBindTexture(GLenum arg0, GLuint arg1) {    
    _pre_call_callback("glBindTexture", (void*)glBindTexture, 2, arg0, arg1);
     glad_glBindTexture(arg0, arg1);
    _post_call_callback("glBindTexture", (void*)glBindTexture, 2, arg0, arg1);
    
}
PFNGLBINDTEXTUREPROC glad_debug_glBindTexture = glad_debug_impl_glBindTexture;
PFNGLBINDTEXTUREUNITPROC glad_glBindTextureUnit;
void APIENTRY glad_debug_impl_glBindTextureUnit(GLuint arg0, GLuint arg1) {    
    _pre_call_callback("glBindTextureUnit", (void*)glBindTextureUnit, 2, arg0, arg1);
     glad_glBindTextureUnit(arg0, arg1);
    _post_call_callback("glBindTextureUnit", (void*)glBindTextureUnit, 2, arg0, arg1);
    
}
PFNGLBINDTEXTUREUNITPROC glad_debug_glBindTextureUnit = glad_debug_impl_glBindTextureUnit;
PFNGLBINDTEXTURESPROC glad_glBindTextures;
void APIENTRY glad_debug_impl_glBindTextures(GLuint arg0, GLsizei arg1, const GLuint * arg2) {    
    _pre_call_callback("glBindTextures", (void*)glBindTextures, 3, arg0, arg1, arg2);
     glad_glBindTextures(arg0, arg1, arg2);
    _post_call_callback("glBindTextures", (void*)glBindTextures, 3, arg0, arg1, arg2);
    
}
PFNGLBINDTEXTURESPROC glad_debug_glBindTextures = glad_debug_impl_glBindTextures;
PFNGLBINDTRANSFORMFEEDBACKPROC glad_glBindTransformFeedback;
void APIENTRY glad_debug_impl_glBindTransformFeedback(GLenum arg0, GLuint arg1) {    
    _pre_call_callback("glBindTransformFeedback", (void*)glBindTransformFeedback, 2, arg0, arg1);
     glad_glBindTransformFeedback(arg0, arg1);
    _post_call_callback("glBindTransformFeedback", (void*)glBindTransformFeedback, 2, arg0, arg1);
    
}
PFNGLBINDTRANSFORMFEEDBACKPROC glad_debug_glBindTransformFeedback = glad_debug_impl_glBindTransformFeedback;
PFNGLBINDVERTEXARRAYPROC glad_glBindVertexArray;
void APIENTRY glad_debug_impl_glBindVertexArray(GLuint arg0) {    
    _pre_call_callback("glBindVertexArray", (void*)glBindVertexArray, 1, arg0);
     glad_glBindVertexArray(arg0);
    _post_call_callback("glBindVertexArray", (void*)glBindVertexArray, 1, arg0);
    
}
PFNGLBINDVERTEXARRAYPROC glad_debug_glBindVertexArray = glad_debug_impl_glBindVertexArray;
PFNGLBINDVERTEXBUFFERPROC glad_glBindVertexBuffer;
void APIENTRY glad_debug_impl_glBindVertexBuffer(GLuint arg0, GLuint arg1, GLintptr arg2, GLsizei arg3) {    
    _pre_call_callback("glBindVertexBuffer", (void*)glBindVertexBuffer, 4, arg0, arg1, arg2, arg3);
     glad_glBindVertexBuffer(arg0, arg1, arg2, arg3);
    _post_call_callback("glBindVertexBuffer", (void*)glBindVertexBuffer, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLBINDVERTEXBUFFERPROC glad_debug_glBindVertexBuffer = glad_debug_impl_glBindVertexBuffer;
PFNGLBINDVERTEXBUFFERSPROC glad_glBindVertexBuffers;
void APIENTRY glad_debug_impl_glBindVertexBuffers(GLuint arg0, GLsizei arg1, const GLuint * arg2, const GLintptr * arg3, const GLsizei * arg4) {    
    _pre_call_callback("glBindVertexBuffers", (void*)glBindVertexBuffers, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glBindVertexBuffers(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glBindVertexBuffers", (void*)glBindVertexBuffers, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLBINDVERTEXBUFFERSPROC glad_debug_glBindVertexBuffers = glad_debug_impl_glBindVertexBuffers;
PFNGLBITMAPPROC glad_glBitmap;
void APIENTRY glad_debug_impl_glBitmap(GLsizei arg0, GLsizei arg1, GLfloat arg2, GLfloat arg3, GLfloat arg4, GLfloat arg5, const GLubyte * arg6) {    
    _pre_call_callback("glBitmap", (void*)glBitmap, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
     glad_glBitmap(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    _post_call_callback("glBitmap", (void*)glBitmap, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    
}
PFNGLBITMAPPROC glad_debug_glBitmap = glad_debug_impl_glBitmap;
PFNGLBLENDCOLORPROC glad_glBlendColor;
void APIENTRY glad_debug_impl_glBlendColor(GLfloat arg0, GLfloat arg1, GLfloat arg2, GLfloat arg3) {    
    _pre_call_callback("glBlendColor", (void*)glBlendColor, 4, arg0, arg1, arg2, arg3);
     glad_glBlendColor(arg0, arg1, arg2, arg3);
    _post_call_callback("glBlendColor", (void*)glBlendColor, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLBLENDCOLORPROC glad_debug_glBlendColor = glad_debug_impl_glBlendColor;
PFNGLBLENDEQUATIONPROC glad_glBlendEquation;
void APIENTRY glad_debug_impl_glBlendEquation(GLenum arg0) {    
    _pre_call_callback("glBlendEquation", (void*)glBlendEquation, 1, arg0);
     glad_glBlendEquation(arg0);
    _post_call_callback("glBlendEquation", (void*)glBlendEquation, 1, arg0);
    
}
PFNGLBLENDEQUATIONPROC glad_debug_glBlendEquation = glad_debug_impl_glBlendEquation;
PFNGLBLENDEQUATIONSEPARATEPROC glad_glBlendEquationSeparate;
void APIENTRY glad_debug_impl_glBlendEquationSeparate(GLenum arg0, GLenum arg1) {    
    _pre_call_callback("glBlendEquationSeparate", (void*)glBlendEquationSeparate, 2, arg0, arg1);
     glad_glBlendEquationSeparate(arg0, arg1);
    _post_call_callback("glBlendEquationSeparate", (void*)glBlendEquationSeparate, 2, arg0, arg1);
    
}
PFNGLBLENDEQUATIONSEPARATEPROC glad_debug_glBlendEquationSeparate = glad_debug_impl_glBlendEquationSeparate;
PFNGLBLENDEQUATIONSEPARATEIPROC glad_glBlendEquationSeparatei;
void APIENTRY glad_debug_impl_glBlendEquationSeparatei(GLuint arg0, GLenum arg1, GLenum arg2) {    
    _pre_call_callback("glBlendEquationSeparatei", (void*)glBlendEquationSeparatei, 3, arg0, arg1, arg2);
     glad_glBlendEquationSeparatei(arg0, arg1, arg2);
    _post_call_callback("glBlendEquationSeparatei", (void*)glBlendEquationSeparatei, 3, arg0, arg1, arg2);
    
}
PFNGLBLENDEQUATIONSEPARATEIPROC glad_debug_glBlendEquationSeparatei = glad_debug_impl_glBlendEquationSeparatei;
PFNGLBLENDEQUATIONIPROC glad_glBlendEquationi;
void APIENTRY glad_debug_impl_glBlendEquationi(GLuint arg0, GLenum arg1) {    
    _pre_call_callback("glBlendEquationi", (void*)glBlendEquationi, 2, arg0, arg1);
     glad_glBlendEquationi(arg0, arg1);
    _post_call_callback("glBlendEquationi", (void*)glBlendEquationi, 2, arg0, arg1);
    
}
PFNGLBLENDEQUATIONIPROC glad_debug_glBlendEquationi = glad_debug_impl_glBlendEquationi;
PFNGLBLENDFUNCPROC glad_glBlendFunc;
void APIENTRY glad_debug_impl_glBlendFunc(GLenum arg0, GLenum arg1) {    
    _pre_call_callback("glBlendFunc", (void*)glBlendFunc, 2, arg0, arg1);
     glad_glBlendFunc(arg0, arg1);
    _post_call_callback("glBlendFunc", (void*)glBlendFunc, 2, arg0, arg1);
    
}
PFNGLBLENDFUNCPROC glad_debug_glBlendFunc = glad_debug_impl_glBlendFunc;
PFNGLBLENDFUNCSEPARATEPROC glad_glBlendFuncSeparate;
void APIENTRY glad_debug_impl_glBlendFuncSeparate(GLenum arg0, GLenum arg1, GLenum arg2, GLenum arg3) {    
    _pre_call_callback("glBlendFuncSeparate", (void*)glBlendFuncSeparate, 4, arg0, arg1, arg2, arg3);
     glad_glBlendFuncSeparate(arg0, arg1, arg2, arg3);
    _post_call_callback("glBlendFuncSeparate", (void*)glBlendFuncSeparate, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLBLENDFUNCSEPARATEPROC glad_debug_glBlendFuncSeparate = glad_debug_impl_glBlendFuncSeparate;
PFNGLBLENDFUNCSEPARATEIPROC glad_glBlendFuncSeparatei;
void APIENTRY glad_debug_impl_glBlendFuncSeparatei(GLuint arg0, GLenum arg1, GLenum arg2, GLenum arg3, GLenum arg4) {    
    _pre_call_callback("glBlendFuncSeparatei", (void*)glBlendFuncSeparatei, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glBlendFuncSeparatei(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glBlendFuncSeparatei", (void*)glBlendFuncSeparatei, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLBLENDFUNCSEPARATEIPROC glad_debug_glBlendFuncSeparatei = glad_debug_impl_glBlendFuncSeparatei;
PFNGLBLENDFUNCIPROC glad_glBlendFunci;
void APIENTRY glad_debug_impl_glBlendFunci(GLuint arg0, GLenum arg1, GLenum arg2) {    
    _pre_call_callback("glBlendFunci", (void*)glBlendFunci, 3, arg0, arg1, arg2);
     glad_glBlendFunci(arg0, arg1, arg2);
    _post_call_callback("glBlendFunci", (void*)glBlendFunci, 3, arg0, arg1, arg2);
    
}
PFNGLBLENDFUNCIPROC glad_debug_glBlendFunci = glad_debug_impl_glBlendFunci;
PFNGLBLITFRAMEBUFFERPROC glad_glBlitFramebuffer;
void APIENTRY glad_debug_impl_glBlitFramebuffer(GLint arg0, GLint arg1, GLint arg2, GLint arg3, GLint arg4, GLint arg5, GLint arg6, GLint arg7, GLbitfield arg8, GLenum arg9) {    
    _pre_call_callback("glBlitFramebuffer", (void*)glBlitFramebuffer, 10, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
     glad_glBlitFramebuffer(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
    _post_call_callback("glBlitFramebuffer", (void*)glBlitFramebuffer, 10, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
    
}
PFNGLBLITFRAMEBUFFERPROC glad_debug_glBlitFramebuffer = glad_debug_impl_glBlitFramebuffer;
PFNGLBLITNAMEDFRAMEBUFFERPROC glad_glBlitNamedFramebuffer;
void APIENTRY glad_debug_impl_glBlitNamedFramebuffer(GLuint arg0, GLuint arg1, GLint arg2, GLint arg3, GLint arg4, GLint arg5, GLint arg6, GLint arg7, GLint arg8, GLint arg9, GLbitfield arg10, GLenum arg11) {    
    _pre_call_callback("glBlitNamedFramebuffer", (void*)glBlitNamedFramebuffer, 12, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11);
     glad_glBlitNamedFramebuffer(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11);
    _post_call_callback("glBlitNamedFramebuffer", (void*)glBlitNamedFramebuffer, 12, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11);
    
}
PFNGLBLITNAMEDFRAMEBUFFERPROC glad_debug_glBlitNamedFramebuffer = glad_debug_impl_glBlitNamedFramebuffer;
PFNGLBUFFERDATAPROC glad_glBufferData;
void APIENTRY glad_debug_impl_glBufferData(GLenum arg0, GLsizeiptr arg1, const void * arg2, GLenum arg3) {    
    _pre_call_callback("glBufferData", (void*)glBufferData, 4, arg0, arg1, arg2, arg3);
     glad_glBufferData(arg0, arg1, arg2, arg3);
    _post_call_callback("glBufferData", (void*)glBufferData, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLBUFFERDATAPROC glad_debug_glBufferData = glad_debug_impl_glBufferData;
PFNGLBUFFERSTORAGEPROC glad_glBufferStorage;
void APIENTRY glad_debug_impl_glBufferStorage(GLenum arg0, GLsizeiptr arg1, const void * arg2, GLbitfield arg3) {    
    _pre_call_callback("glBufferStorage", (void*)glBufferStorage, 4, arg0, arg1, arg2, arg3);
     glad_glBufferStorage(arg0, arg1, arg2, arg3);
    _post_call_callback("glBufferStorage", (void*)glBufferStorage, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLBUFFERSTORAGEPROC glad_debug_glBufferStorage = glad_debug_impl_glBufferStorage;
PFNGLBUFFERSUBDATAPROC glad_glBufferSubData;
void APIENTRY glad_debug_impl_glBufferSubData(GLenum arg0, GLintptr arg1, GLsizeiptr arg2, const void * arg3) {    
    _pre_call_callback("glBufferSubData", (void*)glBufferSubData, 4, arg0, arg1, arg2, arg3);
     glad_glBufferSubData(arg0, arg1, arg2, arg3);
    _post_call_callback("glBufferSubData", (void*)glBufferSubData, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLBUFFERSUBDATAPROC glad_debug_glBufferSubData = glad_debug_impl_glBufferSubData;
PFNGLCALLLISTPROC glad_glCallList;
void APIENTRY glad_debug_impl_glCallList(GLuint arg0) {    
    _pre_call_callback("glCallList", (void*)glCallList, 1, arg0);
     glad_glCallList(arg0);
    _post_call_callback("glCallList", (void*)glCallList, 1, arg0);
    
}
PFNGLCALLLISTPROC glad_debug_glCallList = glad_debug_impl_glCallList;
PFNGLCALLLISTSPROC glad_glCallLists;
void APIENTRY glad_debug_impl_glCallLists(GLsizei arg0, GLenum arg1, const void * arg2) {    
    _pre_call_callback("glCallLists", (void*)glCallLists, 3, arg0, arg1, arg2);
     glad_glCallLists(arg0, arg1, arg2);
    _post_call_callback("glCallLists", (void*)glCallLists, 3, arg0, arg1, arg2);
    
}
PFNGLCALLLISTSPROC glad_debug_glCallLists = glad_debug_impl_glCallLists;
PFNGLCHECKFRAMEBUFFERSTATUSPROC glad_glCheckFramebufferStatus;
GLenum APIENTRY glad_debug_impl_glCheckFramebufferStatus(GLenum arg0) {    
    GLenum ret;
    _pre_call_callback("glCheckFramebufferStatus", (void*)glCheckFramebufferStatus, 1, arg0);
    ret =  glad_glCheckFramebufferStatus(arg0);
    _post_call_callback("glCheckFramebufferStatus", (void*)glCheckFramebufferStatus, 1, arg0);
    return ret;
}
PFNGLCHECKFRAMEBUFFERSTATUSPROC glad_debug_glCheckFramebufferStatus = glad_debug_impl_glCheckFramebufferStatus;
PFNGLCHECKNAMEDFRAMEBUFFERSTATUSPROC glad_glCheckNamedFramebufferStatus;
GLenum APIENTRY glad_debug_impl_glCheckNamedFramebufferStatus(GLuint arg0, GLenum arg1) {    
    GLenum ret;
    _pre_call_callback("glCheckNamedFramebufferStatus", (void*)glCheckNamedFramebufferStatus, 2, arg0, arg1);
    ret =  glad_glCheckNamedFramebufferStatus(arg0, arg1);
    _post_call_callback("glCheckNamedFramebufferStatus", (void*)glCheckNamedFramebufferStatus, 2, arg0, arg1);
    return ret;
}
PFNGLCHECKNAMEDFRAMEBUFFERSTATUSPROC glad_debug_glCheckNamedFramebufferStatus = glad_debug_impl_glCheckNamedFramebufferStatus;
PFNGLCLAMPCOLORPROC glad_glClampColor;
void APIENTRY glad_debug_impl_glClampColor(GLenum arg0, GLenum arg1) {    
    _pre_call_callback("glClampColor", (void*)glClampColor, 2, arg0, arg1);
     glad_glClampColor(arg0, arg1);
    _post_call_callback("glClampColor", (void*)glClampColor, 2, arg0, arg1);
    
}
PFNGLCLAMPCOLORPROC glad_debug_glClampColor = glad_debug_impl_glClampColor;
PFNGLCLEARPROC glad_glClear;
void APIENTRY glad_debug_impl_glClear(GLbitfield arg0) {    
    _pre_call_callback("glClear", (void*)glClear, 1, arg0);
     glad_glClear(arg0);
    _post_call_callback("glClear", (void*)glClear, 1, arg0);
    
}
PFNGLCLEARPROC glad_debug_glClear = glad_debug_impl_glClear;
PFNGLCLEARACCUMPROC glad_glClearAccum;
void APIENTRY glad_debug_impl_glClearAccum(GLfloat arg0, GLfloat arg1, GLfloat arg2, GLfloat arg3) {    
    _pre_call_callback("glClearAccum", (void*)glClearAccum, 4, arg0, arg1, arg2, arg3);
     glad_glClearAccum(arg0, arg1, arg2, arg3);
    _post_call_callback("glClearAccum", (void*)glClearAccum, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLCLEARACCUMPROC glad_debug_glClearAccum = glad_debug_impl_glClearAccum;
PFNGLCLEARBUFFERDATAPROC glad_glClearBufferData;
void APIENTRY glad_debug_impl_glClearBufferData(GLenum arg0, GLenum arg1, GLenum arg2, GLenum arg3, const void * arg4) {    
    _pre_call_callback("glClearBufferData", (void*)glClearBufferData, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glClearBufferData(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glClearBufferData", (void*)glClearBufferData, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLCLEARBUFFERDATAPROC glad_debug_glClearBufferData = glad_debug_impl_glClearBufferData;
PFNGLCLEARBUFFERSUBDATAPROC glad_glClearBufferSubData;
void APIENTRY glad_debug_impl_glClearBufferSubData(GLenum arg0, GLenum arg1, GLintptr arg2, GLsizeiptr arg3, GLenum arg4, GLenum arg5, const void * arg6) {    
    _pre_call_callback("glClearBufferSubData", (void*)glClearBufferSubData, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
     glad_glClearBufferSubData(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    _post_call_callback("glClearBufferSubData", (void*)glClearBufferSubData, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    
}
PFNGLCLEARBUFFERSUBDATAPROC glad_debug_glClearBufferSubData = glad_debug_impl_glClearBufferSubData;
PFNGLCLEARBUFFERFIPROC glad_glClearBufferfi;
void APIENTRY glad_debug_impl_glClearBufferfi(GLenum arg0, GLint arg1, GLfloat arg2, GLint arg3) {    
    _pre_call_callback("glClearBufferfi", (void*)glClearBufferfi, 4, arg0, arg1, arg2, arg3);
     glad_glClearBufferfi(arg0, arg1, arg2, arg3);
    _post_call_callback("glClearBufferfi", (void*)glClearBufferfi, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLCLEARBUFFERFIPROC glad_debug_glClearBufferfi = glad_debug_impl_glClearBufferfi;
PFNGLCLEARBUFFERFVPROC glad_glClearBufferfv;
void APIENTRY glad_debug_impl_glClearBufferfv(GLenum arg0, GLint arg1, const GLfloat * arg2) {    
    _pre_call_callback("glClearBufferfv", (void*)glClearBufferfv, 3, arg0, arg1, arg2);
     glad_glClearBufferfv(arg0, arg1, arg2);
    _post_call_callback("glClearBufferfv", (void*)glClearBufferfv, 3, arg0, arg1, arg2);
    
}
PFNGLCLEARBUFFERFVPROC glad_debug_glClearBufferfv = glad_debug_impl_glClearBufferfv;
PFNGLCLEARBUFFERIVPROC glad_glClearBufferiv;
void APIENTRY glad_debug_impl_glClearBufferiv(GLenum arg0, GLint arg1, const GLint * arg2) {    
    _pre_call_callback("glClearBufferiv", (void*)glClearBufferiv, 3, arg0, arg1, arg2);
     glad_glClearBufferiv(arg0, arg1, arg2);
    _post_call_callback("glClearBufferiv", (void*)glClearBufferiv, 3, arg0, arg1, arg2);
    
}
PFNGLCLEARBUFFERIVPROC glad_debug_glClearBufferiv = glad_debug_impl_glClearBufferiv;
PFNGLCLEARBUFFERUIVPROC glad_glClearBufferuiv;
void APIENTRY glad_debug_impl_glClearBufferuiv(GLenum arg0, GLint arg1, const GLuint * arg2) {    
    _pre_call_callback("glClearBufferuiv", (void*)glClearBufferuiv, 3, arg0, arg1, arg2);
     glad_glClearBufferuiv(arg0, arg1, arg2);
    _post_call_callback("glClearBufferuiv", (void*)glClearBufferuiv, 3, arg0, arg1, arg2);
    
}
PFNGLCLEARBUFFERUIVPROC glad_debug_glClearBufferuiv = glad_debug_impl_glClearBufferuiv;
PFNGLCLEARCOLORPROC glad_glClearColor;
void APIENTRY glad_debug_impl_glClearColor(GLfloat arg0, GLfloat arg1, GLfloat arg2, GLfloat arg3) {    
    _pre_call_callback("glClearColor", (void*)glClearColor, 4, arg0, arg1, arg2, arg3);
     glad_glClearColor(arg0, arg1, arg2, arg3);
    _post_call_callback("glClearColor", (void*)glClearColor, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLCLEARCOLORPROC glad_debug_glClearColor = glad_debug_impl_glClearColor;
PFNGLCLEARDEPTHPROC glad_glClearDepth;
void APIENTRY glad_debug_impl_glClearDepth(GLdouble arg0) {    
    _pre_call_callback("glClearDepth", (void*)glClearDepth, 1, arg0);
     glad_glClearDepth(arg0);
    _post_call_callback("glClearDepth", (void*)glClearDepth, 1, arg0);
    
}
PFNGLCLEARDEPTHPROC glad_debug_glClearDepth = glad_debug_impl_glClearDepth;
PFNGLCLEARDEPTHFPROC glad_glClearDepthf;
void APIENTRY glad_debug_impl_glClearDepthf(GLfloat arg0) {    
    _pre_call_callback("glClearDepthf", (void*)glClearDepthf, 1, arg0);
     glad_glClearDepthf(arg0);
    _post_call_callback("glClearDepthf", (void*)glClearDepthf, 1, arg0);
    
}
PFNGLCLEARDEPTHFPROC glad_debug_glClearDepthf = glad_debug_impl_glClearDepthf;
PFNGLCLEARINDEXPROC glad_glClearIndex;
void APIENTRY glad_debug_impl_glClearIndex(GLfloat arg0) {    
    _pre_call_callback("glClearIndex", (void*)glClearIndex, 1, arg0);
     glad_glClearIndex(arg0);
    _post_call_callback("glClearIndex", (void*)glClearIndex, 1, arg0);
    
}
PFNGLCLEARINDEXPROC glad_debug_glClearIndex = glad_debug_impl_glClearIndex;
PFNGLCLEARNAMEDBUFFERDATAPROC glad_glClearNamedBufferData;
void APIENTRY glad_debug_impl_glClearNamedBufferData(GLuint arg0, GLenum arg1, GLenum arg2, GLenum arg3, const void * arg4) {    
    _pre_call_callback("glClearNamedBufferData", (void*)glClearNamedBufferData, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glClearNamedBufferData(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glClearNamedBufferData", (void*)glClearNamedBufferData, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLCLEARNAMEDBUFFERDATAPROC glad_debug_glClearNamedBufferData = glad_debug_impl_glClearNamedBufferData;
PFNGLCLEARNAMEDBUFFERSUBDATAPROC glad_glClearNamedBufferSubData;
void APIENTRY glad_debug_impl_glClearNamedBufferSubData(GLuint arg0, GLenum arg1, GLintptr arg2, GLsizeiptr arg3, GLenum arg4, GLenum arg5, const void * arg6) {    
    _pre_call_callback("glClearNamedBufferSubData", (void*)glClearNamedBufferSubData, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
     glad_glClearNamedBufferSubData(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    _post_call_callback("glClearNamedBufferSubData", (void*)glClearNamedBufferSubData, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    
}
PFNGLCLEARNAMEDBUFFERSUBDATAPROC glad_debug_glClearNamedBufferSubData = glad_debug_impl_glClearNamedBufferSubData;
PFNGLCLEARNAMEDFRAMEBUFFERFIPROC glad_glClearNamedFramebufferfi;
void APIENTRY glad_debug_impl_glClearNamedFramebufferfi(GLuint arg0, GLenum arg1, GLint arg2, GLfloat arg3, GLint arg4) {    
    _pre_call_callback("glClearNamedFramebufferfi", (void*)glClearNamedFramebufferfi, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glClearNamedFramebufferfi(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glClearNamedFramebufferfi", (void*)glClearNamedFramebufferfi, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLCLEARNAMEDFRAMEBUFFERFIPROC glad_debug_glClearNamedFramebufferfi = glad_debug_impl_glClearNamedFramebufferfi;
PFNGLCLEARNAMEDFRAMEBUFFERFVPROC glad_glClearNamedFramebufferfv;
void APIENTRY glad_debug_impl_glClearNamedFramebufferfv(GLuint arg0, GLenum arg1, GLint arg2, const GLfloat * arg3) {    
    _pre_call_callback("glClearNamedFramebufferfv", (void*)glClearNamedFramebufferfv, 4, arg0, arg1, arg2, arg3);
     glad_glClearNamedFramebufferfv(arg0, arg1, arg2, arg3);
    _post_call_callback("glClearNamedFramebufferfv", (void*)glClearNamedFramebufferfv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLCLEARNAMEDFRAMEBUFFERFVPROC glad_debug_glClearNamedFramebufferfv = glad_debug_impl_glClearNamedFramebufferfv;
PFNGLCLEARNAMEDFRAMEBUFFERIVPROC glad_glClearNamedFramebufferiv;
void APIENTRY glad_debug_impl_glClearNamedFramebufferiv(GLuint arg0, GLenum arg1, GLint arg2, const GLint * arg3) {    
    _pre_call_callback("glClearNamedFramebufferiv", (void*)glClearNamedFramebufferiv, 4, arg0, arg1, arg2, arg3);
     glad_glClearNamedFramebufferiv(arg0, arg1, arg2, arg3);
    _post_call_callback("glClearNamedFramebufferiv", (void*)glClearNamedFramebufferiv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLCLEARNAMEDFRAMEBUFFERIVPROC glad_debug_glClearNamedFramebufferiv = glad_debug_impl_glClearNamedFramebufferiv;
PFNGLCLEARNAMEDFRAMEBUFFERUIVPROC glad_glClearNamedFramebufferuiv;
void APIENTRY glad_debug_impl_glClearNamedFramebufferuiv(GLuint arg0, GLenum arg1, GLint arg2, const GLuint * arg3) {    
    _pre_call_callback("glClearNamedFramebufferuiv", (void*)glClearNamedFramebufferuiv, 4, arg0, arg1, arg2, arg3);
     glad_glClearNamedFramebufferuiv(arg0, arg1, arg2, arg3);
    _post_call_callback("glClearNamedFramebufferuiv", (void*)glClearNamedFramebufferuiv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLCLEARNAMEDFRAMEBUFFERUIVPROC glad_debug_glClearNamedFramebufferuiv = glad_debug_impl_glClearNamedFramebufferuiv;
PFNGLCLEARSTENCILPROC glad_glClearStencil;
void APIENTRY glad_debug_impl_glClearStencil(GLint arg0) {    
    _pre_call_callback("glClearStencil", (void*)glClearStencil, 1, arg0);
     glad_glClearStencil(arg0);
    _post_call_callback("glClearStencil", (void*)glClearStencil, 1, arg0);
    
}
PFNGLCLEARSTENCILPROC glad_debug_glClearStencil = glad_debug_impl_glClearStencil;
PFNGLCLEARTEXIMAGEPROC glad_glClearTexImage;
void APIENTRY glad_debug_impl_glClearTexImage(GLuint arg0, GLint arg1, GLenum arg2, GLenum arg3, const void * arg4) {    
    _pre_call_callback("glClearTexImage", (void*)glClearTexImage, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glClearTexImage(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glClearTexImage", (void*)glClearTexImage, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLCLEARTEXIMAGEPROC glad_debug_glClearTexImage = glad_debug_impl_glClearTexImage;
PFNGLCLEARTEXSUBIMAGEPROC glad_glClearTexSubImage;
void APIENTRY glad_debug_impl_glClearTexSubImage(GLuint arg0, GLint arg1, GLint arg2, GLint arg3, GLint arg4, GLsizei arg5, GLsizei arg6, GLsizei arg7, GLenum arg8, GLenum arg9, const void * arg10) {    
    _pre_call_callback("glClearTexSubImage", (void*)glClearTexSubImage, 11, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10);
     glad_glClearTexSubImage(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10);
    _post_call_callback("glClearTexSubImage", (void*)glClearTexSubImage, 11, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10);
    
}
PFNGLCLEARTEXSUBIMAGEPROC glad_debug_glClearTexSubImage = glad_debug_impl_glClearTexSubImage;
PFNGLCLIENTACTIVETEXTUREPROC glad_glClientActiveTexture;
void APIENTRY glad_debug_impl_glClientActiveTexture(GLenum arg0) {    
    _pre_call_callback("glClientActiveTexture", (void*)glClientActiveTexture, 1, arg0);
     glad_glClientActiveTexture(arg0);
    _post_call_callback("glClientActiveTexture", (void*)glClientActiveTexture, 1, arg0);
    
}
PFNGLCLIENTACTIVETEXTUREPROC glad_debug_glClientActiveTexture = glad_debug_impl_glClientActiveTexture;
PFNGLCLIENTWAITSYNCPROC glad_glClientWaitSync;
GLenum APIENTRY glad_debug_impl_glClientWaitSync(GLsync arg0, GLbitfield arg1, GLuint64 arg2) {    
    GLenum ret;
    _pre_call_callback("glClientWaitSync", (void*)glClientWaitSync, 3, arg0, arg1, arg2);
    ret =  glad_glClientWaitSync(arg0, arg1, arg2);
    _post_call_callback("glClientWaitSync", (void*)glClientWaitSync, 3, arg0, arg1, arg2);
    return ret;
}
PFNGLCLIENTWAITSYNCPROC glad_debug_glClientWaitSync = glad_debug_impl_glClientWaitSync;
PFNGLCLIPCONTROLPROC glad_glClipControl;
void APIENTRY glad_debug_impl_glClipControl(GLenum arg0, GLenum arg1) {    
    _pre_call_callback("glClipControl", (void*)glClipControl, 2, arg0, arg1);
     glad_glClipControl(arg0, arg1);
    _post_call_callback("glClipControl", (void*)glClipControl, 2, arg0, arg1);
    
}
PFNGLCLIPCONTROLPROC glad_debug_glClipControl = glad_debug_impl_glClipControl;
PFNGLCLIPPLANEPROC glad_glClipPlane;
void APIENTRY glad_debug_impl_glClipPlane(GLenum arg0, const GLdouble * arg1) {    
    _pre_call_callback("glClipPlane", (void*)glClipPlane, 2, arg0, arg1);
     glad_glClipPlane(arg0, arg1);
    _post_call_callback("glClipPlane", (void*)glClipPlane, 2, arg0, arg1);
    
}
PFNGLCLIPPLANEPROC glad_debug_glClipPlane = glad_debug_impl_glClipPlane;
PFNGLCOLOR3BPROC glad_glColor3b;
void APIENTRY glad_debug_impl_glColor3b(GLbyte arg0, GLbyte arg1, GLbyte arg2) {    
    _pre_call_callback("glColor3b", (void*)glColor3b, 3, arg0, arg1, arg2);
     glad_glColor3b(arg0, arg1, arg2);
    _post_call_callback("glColor3b", (void*)glColor3b, 3, arg0, arg1, arg2);
    
}
PFNGLCOLOR3BPROC glad_debug_glColor3b = glad_debug_impl_glColor3b;
PFNGLCOLOR3BVPROC glad_glColor3bv;
void APIENTRY glad_debug_impl_glColor3bv(const GLbyte * arg0) {    
    _pre_call_callback("glColor3bv", (void*)glColor3bv, 1, arg0);
     glad_glColor3bv(arg0);
    _post_call_callback("glColor3bv", (void*)glColor3bv, 1, arg0);
    
}
PFNGLCOLOR3BVPROC glad_debug_glColor3bv = glad_debug_impl_glColor3bv;
PFNGLCOLOR3DPROC glad_glColor3d;
void APIENTRY glad_debug_impl_glColor3d(GLdouble arg0, GLdouble arg1, GLdouble arg2) {    
    _pre_call_callback("glColor3d", (void*)glColor3d, 3, arg0, arg1, arg2);
     glad_glColor3d(arg0, arg1, arg2);
    _post_call_callback("glColor3d", (void*)glColor3d, 3, arg0, arg1, arg2);
    
}
PFNGLCOLOR3DPROC glad_debug_glColor3d = glad_debug_impl_glColor3d;
PFNGLCOLOR3DVPROC glad_glColor3dv;
void APIENTRY glad_debug_impl_glColor3dv(const GLdouble * arg0) {    
    _pre_call_callback("glColor3dv", (void*)glColor3dv, 1, arg0);
     glad_glColor3dv(arg0);
    _post_call_callback("glColor3dv", (void*)glColor3dv, 1, arg0);
    
}
PFNGLCOLOR3DVPROC glad_debug_glColor3dv = glad_debug_impl_glColor3dv;
PFNGLCOLOR3FPROC glad_glColor3f;
void APIENTRY glad_debug_impl_glColor3f(GLfloat arg0, GLfloat arg1, GLfloat arg2) {    
    _pre_call_callback("glColor3f", (void*)glColor3f, 3, arg0, arg1, arg2);
     glad_glColor3f(arg0, arg1, arg2);
    _post_call_callback("glColor3f", (void*)glColor3f, 3, arg0, arg1, arg2);
    
}
PFNGLCOLOR3FPROC glad_debug_glColor3f = glad_debug_impl_glColor3f;
PFNGLCOLOR3FVPROC glad_glColor3fv;
void APIENTRY glad_debug_impl_glColor3fv(const GLfloat * arg0) {    
    _pre_call_callback("glColor3fv", (void*)glColor3fv, 1, arg0);
     glad_glColor3fv(arg0);
    _post_call_callback("glColor3fv", (void*)glColor3fv, 1, arg0);
    
}
PFNGLCOLOR3FVPROC glad_debug_glColor3fv = glad_debug_impl_glColor3fv;
PFNGLCOLOR3IPROC glad_glColor3i;
void APIENTRY glad_debug_impl_glColor3i(GLint arg0, GLint arg1, GLint arg2) {    
    _pre_call_callback("glColor3i", (void*)glColor3i, 3, arg0, arg1, arg2);
     glad_glColor3i(arg0, arg1, arg2);
    _post_call_callback("glColor3i", (void*)glColor3i, 3, arg0, arg1, arg2);
    
}
PFNGLCOLOR3IPROC glad_debug_glColor3i = glad_debug_impl_glColor3i;
PFNGLCOLOR3IVPROC glad_glColor3iv;
void APIENTRY glad_debug_impl_glColor3iv(const GLint * arg0) {    
    _pre_call_callback("glColor3iv", (void*)glColor3iv, 1, arg0);
     glad_glColor3iv(arg0);
    _post_call_callback("glColor3iv", (void*)glColor3iv, 1, arg0);
    
}
PFNGLCOLOR3IVPROC glad_debug_glColor3iv = glad_debug_impl_glColor3iv;
PFNGLCOLOR3SPROC glad_glColor3s;
void APIENTRY glad_debug_impl_glColor3s(GLshort arg0, GLshort arg1, GLshort arg2) {    
    _pre_call_callback("glColor3s", (void*)glColor3s, 3, arg0, arg1, arg2);
     glad_glColor3s(arg0, arg1, arg2);
    _post_call_callback("glColor3s", (void*)glColor3s, 3, arg0, arg1, arg2);
    
}
PFNGLCOLOR3SPROC glad_debug_glColor3s = glad_debug_impl_glColor3s;
PFNGLCOLOR3SVPROC glad_glColor3sv;
void APIENTRY glad_debug_impl_glColor3sv(const GLshort * arg0) {    
    _pre_call_callback("glColor3sv", (void*)glColor3sv, 1, arg0);
     glad_glColor3sv(arg0);
    _post_call_callback("glColor3sv", (void*)glColor3sv, 1, arg0);
    
}
PFNGLCOLOR3SVPROC glad_debug_glColor3sv = glad_debug_impl_glColor3sv;
PFNGLCOLOR3UBPROC glad_glColor3ub;
void APIENTRY glad_debug_impl_glColor3ub(GLubyte arg0, GLubyte arg1, GLubyte arg2) {    
    _pre_call_callback("glColor3ub", (void*)glColor3ub, 3, arg0, arg1, arg2);
     glad_glColor3ub(arg0, arg1, arg2);
    _post_call_callback("glColor3ub", (void*)glColor3ub, 3, arg0, arg1, arg2);
    
}
PFNGLCOLOR3UBPROC glad_debug_glColor3ub = glad_debug_impl_glColor3ub;
PFNGLCOLOR3UBVPROC glad_glColor3ubv;
void APIENTRY glad_debug_impl_glColor3ubv(const GLubyte * arg0) {    
    _pre_call_callback("glColor3ubv", (void*)glColor3ubv, 1, arg0);
     glad_glColor3ubv(arg0);
    _post_call_callback("glColor3ubv", (void*)glColor3ubv, 1, arg0);
    
}
PFNGLCOLOR3UBVPROC glad_debug_glColor3ubv = glad_debug_impl_glColor3ubv;
PFNGLCOLOR3UIPROC glad_glColor3ui;
void APIENTRY glad_debug_impl_glColor3ui(GLuint arg0, GLuint arg1, GLuint arg2) {    
    _pre_call_callback("glColor3ui", (void*)glColor3ui, 3, arg0, arg1, arg2);
     glad_glColor3ui(arg0, arg1, arg2);
    _post_call_callback("glColor3ui", (void*)glColor3ui, 3, arg0, arg1, arg2);
    
}
PFNGLCOLOR3UIPROC glad_debug_glColor3ui = glad_debug_impl_glColor3ui;
PFNGLCOLOR3UIVPROC glad_glColor3uiv;
void APIENTRY glad_debug_impl_glColor3uiv(const GLuint * arg0) {    
    _pre_call_callback("glColor3uiv", (void*)glColor3uiv, 1, arg0);
     glad_glColor3uiv(arg0);
    _post_call_callback("glColor3uiv", (void*)glColor3uiv, 1, arg0);
    
}
PFNGLCOLOR3UIVPROC glad_debug_glColor3uiv = glad_debug_impl_glColor3uiv;
PFNGLCOLOR3USPROC glad_glColor3us;
void APIENTRY glad_debug_impl_glColor3us(GLushort arg0, GLushort arg1, GLushort arg2) {    
    _pre_call_callback("glColor3us", (void*)glColor3us, 3, arg0, arg1, arg2);
     glad_glColor3us(arg0, arg1, arg2);
    _post_call_callback("glColor3us", (void*)glColor3us, 3, arg0, arg1, arg2);
    
}
PFNGLCOLOR3USPROC glad_debug_glColor3us = glad_debug_impl_glColor3us;
PFNGLCOLOR3USVPROC glad_glColor3usv;
void APIENTRY glad_debug_impl_glColor3usv(const GLushort * arg0) {    
    _pre_call_callback("glColor3usv", (void*)glColor3usv, 1, arg0);
     glad_glColor3usv(arg0);
    _post_call_callback("glColor3usv", (void*)glColor3usv, 1, arg0);
    
}
PFNGLCOLOR3USVPROC glad_debug_glColor3usv = glad_debug_impl_glColor3usv;
PFNGLCOLOR4BPROC glad_glColor4b;
void APIENTRY glad_debug_impl_glColor4b(GLbyte arg0, GLbyte arg1, GLbyte arg2, GLbyte arg3) {    
    _pre_call_callback("glColor4b", (void*)glColor4b, 4, arg0, arg1, arg2, arg3);
     glad_glColor4b(arg0, arg1, arg2, arg3);
    _post_call_callback("glColor4b", (void*)glColor4b, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLCOLOR4BPROC glad_debug_glColor4b = glad_debug_impl_glColor4b;
PFNGLCOLOR4BVPROC glad_glColor4bv;
void APIENTRY glad_debug_impl_glColor4bv(const GLbyte * arg0) {    
    _pre_call_callback("glColor4bv", (void*)glColor4bv, 1, arg0);
     glad_glColor4bv(arg0);
    _post_call_callback("glColor4bv", (void*)glColor4bv, 1, arg0);
    
}
PFNGLCOLOR4BVPROC glad_debug_glColor4bv = glad_debug_impl_glColor4bv;
PFNGLCOLOR4DPROC glad_glColor4d;
void APIENTRY glad_debug_impl_glColor4d(GLdouble arg0, GLdouble arg1, GLdouble arg2, GLdouble arg3) {    
    _pre_call_callback("glColor4d", (void*)glColor4d, 4, arg0, arg1, arg2, arg3);
     glad_glColor4d(arg0, arg1, arg2, arg3);
    _post_call_callback("glColor4d", (void*)glColor4d, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLCOLOR4DPROC glad_debug_glColor4d = glad_debug_impl_glColor4d;
PFNGLCOLOR4DVPROC glad_glColor4dv;
void APIENTRY glad_debug_impl_glColor4dv(const GLdouble * arg0) {    
    _pre_call_callback("glColor4dv", (void*)glColor4dv, 1, arg0);
     glad_glColor4dv(arg0);
    _post_call_callback("glColor4dv", (void*)glColor4dv, 1, arg0);
    
}
PFNGLCOLOR4DVPROC glad_debug_glColor4dv = glad_debug_impl_glColor4dv;
PFNGLCOLOR4FPROC glad_glColor4f;
void APIENTRY glad_debug_impl_glColor4f(GLfloat arg0, GLfloat arg1, GLfloat arg2, GLfloat arg3) {    
    _pre_call_callback("glColor4f", (void*)glColor4f, 4, arg0, arg1, arg2, arg3);
     glad_glColor4f(arg0, arg1, arg2, arg3);
    _post_call_callback("glColor4f", (void*)glColor4f, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLCOLOR4FPROC glad_debug_glColor4f = glad_debug_impl_glColor4f;
PFNGLCOLOR4FVPROC glad_glColor4fv;
void APIENTRY glad_debug_impl_glColor4fv(const GLfloat * arg0) {    
    _pre_call_callback("glColor4fv", (void*)glColor4fv, 1, arg0);
     glad_glColor4fv(arg0);
    _post_call_callback("glColor4fv", (void*)glColor4fv, 1, arg0);
    
}
PFNGLCOLOR4FVPROC glad_debug_glColor4fv = glad_debug_impl_glColor4fv;
PFNGLCOLOR4IPROC glad_glColor4i;
void APIENTRY glad_debug_impl_glColor4i(GLint arg0, GLint arg1, GLint arg2, GLint arg3) {    
    _pre_call_callback("glColor4i", (void*)glColor4i, 4, arg0, arg1, arg2, arg3);
     glad_glColor4i(arg0, arg1, arg2, arg3);
    _post_call_callback("glColor4i", (void*)glColor4i, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLCOLOR4IPROC glad_debug_glColor4i = glad_debug_impl_glColor4i;
PFNGLCOLOR4IVPROC glad_glColor4iv;
void APIENTRY glad_debug_impl_glColor4iv(const GLint * arg0) {    
    _pre_call_callback("glColor4iv", (void*)glColor4iv, 1, arg0);
     glad_glColor4iv(arg0);
    _post_call_callback("glColor4iv", (void*)glColor4iv, 1, arg0);
    
}
PFNGLCOLOR4IVPROC glad_debug_glColor4iv = glad_debug_impl_glColor4iv;
PFNGLCOLOR4SPROC glad_glColor4s;
void APIENTRY glad_debug_impl_glColor4s(GLshort arg0, GLshort arg1, GLshort arg2, GLshort arg3) {    
    _pre_call_callback("glColor4s", (void*)glColor4s, 4, arg0, arg1, arg2, arg3);
     glad_glColor4s(arg0, arg1, arg2, arg3);
    _post_call_callback("glColor4s", (void*)glColor4s, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLCOLOR4SPROC glad_debug_glColor4s = glad_debug_impl_glColor4s;
PFNGLCOLOR4SVPROC glad_glColor4sv;
void APIENTRY glad_debug_impl_glColor4sv(const GLshort * arg0) {    
    _pre_call_callback("glColor4sv", (void*)glColor4sv, 1, arg0);
     glad_glColor4sv(arg0);
    _post_call_callback("glColor4sv", (void*)glColor4sv, 1, arg0);
    
}
PFNGLCOLOR4SVPROC glad_debug_glColor4sv = glad_debug_impl_glColor4sv;
PFNGLCOLOR4UBPROC glad_glColor4ub;
void APIENTRY glad_debug_impl_glColor4ub(GLubyte arg0, GLubyte arg1, GLubyte arg2, GLubyte arg3) {    
    _pre_call_callback("glColor4ub", (void*)glColor4ub, 4, arg0, arg1, arg2, arg3);
     glad_glColor4ub(arg0, arg1, arg2, arg3);
    _post_call_callback("glColor4ub", (void*)glColor4ub, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLCOLOR4UBPROC glad_debug_glColor4ub = glad_debug_impl_glColor4ub;
PFNGLCOLOR4UBVPROC glad_glColor4ubv;
void APIENTRY glad_debug_impl_glColor4ubv(const GLubyte * arg0) {    
    _pre_call_callback("glColor4ubv", (void*)glColor4ubv, 1, arg0);
     glad_glColor4ubv(arg0);
    _post_call_callback("glColor4ubv", (void*)glColor4ubv, 1, arg0);
    
}
PFNGLCOLOR4UBVPROC glad_debug_glColor4ubv = glad_debug_impl_glColor4ubv;
PFNGLCOLOR4UIPROC glad_glColor4ui;
void APIENTRY glad_debug_impl_glColor4ui(GLuint arg0, GLuint arg1, GLuint arg2, GLuint arg3) {    
    _pre_call_callback("glColor4ui", (void*)glColor4ui, 4, arg0, arg1, arg2, arg3);
     glad_glColor4ui(arg0, arg1, arg2, arg3);
    _post_call_callback("glColor4ui", (void*)glColor4ui, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLCOLOR4UIPROC glad_debug_glColor4ui = glad_debug_impl_glColor4ui;
PFNGLCOLOR4UIVPROC glad_glColor4uiv;
void APIENTRY glad_debug_impl_glColor4uiv(const GLuint * arg0) {    
    _pre_call_callback("glColor4uiv", (void*)glColor4uiv, 1, arg0);
     glad_glColor4uiv(arg0);
    _post_call_callback("glColor4uiv", (void*)glColor4uiv, 1, arg0);
    
}
PFNGLCOLOR4UIVPROC glad_debug_glColor4uiv = glad_debug_impl_glColor4uiv;
PFNGLCOLOR4USPROC glad_glColor4us;
void APIENTRY glad_debug_impl_glColor4us(GLushort arg0, GLushort arg1, GLushort arg2, GLushort arg3) {    
    _pre_call_callback("glColor4us", (void*)glColor4us, 4, arg0, arg1, arg2, arg3);
     glad_glColor4us(arg0, arg1, arg2, arg3);
    _post_call_callback("glColor4us", (void*)glColor4us, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLCOLOR4USPROC glad_debug_glColor4us = glad_debug_impl_glColor4us;
PFNGLCOLOR4USVPROC glad_glColor4usv;
void APIENTRY glad_debug_impl_glColor4usv(const GLushort * arg0) {    
    _pre_call_callback("glColor4usv", (void*)glColor4usv, 1, arg0);
     glad_glColor4usv(arg0);
    _post_call_callback("glColor4usv", (void*)glColor4usv, 1, arg0);
    
}
PFNGLCOLOR4USVPROC glad_debug_glColor4usv = glad_debug_impl_glColor4usv;
PFNGLCOLORMASKPROC glad_glColorMask;
void APIENTRY glad_debug_impl_glColorMask(GLboolean arg0, GLboolean arg1, GLboolean arg2, GLboolean arg3) {    
    _pre_call_callback("glColorMask", (void*)glColorMask, 4, arg0, arg1, arg2, arg3);
     glad_glColorMask(arg0, arg1, arg2, arg3);
    _post_call_callback("glColorMask", (void*)glColorMask, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLCOLORMASKPROC glad_debug_glColorMask = glad_debug_impl_glColorMask;
PFNGLCOLORMASKIPROC glad_glColorMaski;
void APIENTRY glad_debug_impl_glColorMaski(GLuint arg0, GLboolean arg1, GLboolean arg2, GLboolean arg3, GLboolean arg4) {    
    _pre_call_callback("glColorMaski", (void*)glColorMaski, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glColorMaski(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glColorMaski", (void*)glColorMaski, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLCOLORMASKIPROC glad_debug_glColorMaski = glad_debug_impl_glColorMaski;
PFNGLCOLORMATERIALPROC glad_glColorMaterial;
void APIENTRY glad_debug_impl_glColorMaterial(GLenum arg0, GLenum arg1) {    
    _pre_call_callback("glColorMaterial", (void*)glColorMaterial, 2, arg0, arg1);
     glad_glColorMaterial(arg0, arg1);
    _post_call_callback("glColorMaterial", (void*)glColorMaterial, 2, arg0, arg1);
    
}
PFNGLCOLORMATERIALPROC glad_debug_glColorMaterial = glad_debug_impl_glColorMaterial;
PFNGLCOLORP3UIPROC glad_glColorP3ui;
void APIENTRY glad_debug_impl_glColorP3ui(GLenum arg0, GLuint arg1) {    
    _pre_call_callback("glColorP3ui", (void*)glColorP3ui, 2, arg0, arg1);
     glad_glColorP3ui(arg0, arg1);
    _post_call_callback("glColorP3ui", (void*)glColorP3ui, 2, arg0, arg1);
    
}
PFNGLCOLORP3UIPROC glad_debug_glColorP3ui = glad_debug_impl_glColorP3ui;
PFNGLCOLORP3UIVPROC glad_glColorP3uiv;
void APIENTRY glad_debug_impl_glColorP3uiv(GLenum arg0, const GLuint * arg1) {    
    _pre_call_callback("glColorP3uiv", (void*)glColorP3uiv, 2, arg0, arg1);
     glad_glColorP3uiv(arg0, arg1);
    _post_call_callback("glColorP3uiv", (void*)glColorP3uiv, 2, arg0, arg1);
    
}
PFNGLCOLORP3UIVPROC glad_debug_glColorP3uiv = glad_debug_impl_glColorP3uiv;
PFNGLCOLORP4UIPROC glad_glColorP4ui;
void APIENTRY glad_debug_impl_glColorP4ui(GLenum arg0, GLuint arg1) {    
    _pre_call_callback("glColorP4ui", (void*)glColorP4ui, 2, arg0, arg1);
     glad_glColorP4ui(arg0, arg1);
    _post_call_callback("glColorP4ui", (void*)glColorP4ui, 2, arg0, arg1);
    
}
PFNGLCOLORP4UIPROC glad_debug_glColorP4ui = glad_debug_impl_glColorP4ui;
PFNGLCOLORP4UIVPROC glad_glColorP4uiv;
void APIENTRY glad_debug_impl_glColorP4uiv(GLenum arg0, const GLuint * arg1) {    
    _pre_call_callback("glColorP4uiv", (void*)glColorP4uiv, 2, arg0, arg1);
     glad_glColorP4uiv(arg0, arg1);
    _post_call_callback("glColorP4uiv", (void*)glColorP4uiv, 2, arg0, arg1);
    
}
PFNGLCOLORP4UIVPROC glad_debug_glColorP4uiv = glad_debug_impl_glColorP4uiv;
PFNGLCOLORPOINTERPROC glad_glColorPointer;
void APIENTRY glad_debug_impl_glColorPointer(GLint arg0, GLenum arg1, GLsizei arg2, const void * arg3) {    
    _pre_call_callback("glColorPointer", (void*)glColorPointer, 4, arg0, arg1, arg2, arg3);
     glad_glColorPointer(arg0, arg1, arg2, arg3);
    _post_call_callback("glColorPointer", (void*)glColorPointer, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLCOLORPOINTERPROC glad_debug_glColorPointer = glad_debug_impl_glColorPointer;
PFNGLCOMPILESHADERPROC glad_glCompileShader;
void APIENTRY glad_debug_impl_glCompileShader(GLuint arg0) {    
    _pre_call_callback("glCompileShader", (void*)glCompileShader, 1, arg0);
     glad_glCompileShader(arg0);
    _post_call_callback("glCompileShader", (void*)glCompileShader, 1, arg0);
    
}
PFNGLCOMPILESHADERPROC glad_debug_glCompileShader = glad_debug_impl_glCompileShader;
PFNGLCOMPRESSEDTEXIMAGE1DPROC glad_glCompressedTexImage1D;
void APIENTRY glad_debug_impl_glCompressedTexImage1D(GLenum arg0, GLint arg1, GLenum arg2, GLsizei arg3, GLint arg4, GLsizei arg5, const void * arg6) {    
    _pre_call_callback("glCompressedTexImage1D", (void*)glCompressedTexImage1D, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
     glad_glCompressedTexImage1D(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    _post_call_callback("glCompressedTexImage1D", (void*)glCompressedTexImage1D, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    
}
PFNGLCOMPRESSEDTEXIMAGE1DPROC glad_debug_glCompressedTexImage1D = glad_debug_impl_glCompressedTexImage1D;
PFNGLCOMPRESSEDTEXIMAGE2DPROC glad_glCompressedTexImage2D;
void APIENTRY glad_debug_impl_glCompressedTexImage2D(GLenum arg0, GLint arg1, GLenum arg2, GLsizei arg3, GLsizei arg4, GLint arg5, GLsizei arg6, const void * arg7) {    
    _pre_call_callback("glCompressedTexImage2D", (void*)glCompressedTexImage2D, 8, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
     glad_glCompressedTexImage2D(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    _post_call_callback("glCompressedTexImage2D", (void*)glCompressedTexImage2D, 8, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    
}
PFNGLCOMPRESSEDTEXIMAGE2DPROC glad_debug_glCompressedTexImage2D = glad_debug_impl_glCompressedTexImage2D;
PFNGLCOMPRESSEDTEXIMAGE3DPROC glad_glCompressedTexImage3D;
void APIENTRY glad_debug_impl_glCompressedTexImage3D(GLenum arg0, GLint arg1, GLenum arg2, GLsizei arg3, GLsizei arg4, GLsizei arg5, GLint arg6, GLsizei arg7, const void * arg8) {    
    _pre_call_callback("glCompressedTexImage3D", (void*)glCompressedTexImage3D, 9, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
     glad_glCompressedTexImage3D(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
    _post_call_callback("glCompressedTexImage3D", (void*)glCompressedTexImage3D, 9, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
    
}
PFNGLCOMPRESSEDTEXIMAGE3DPROC glad_debug_glCompressedTexImage3D = glad_debug_impl_glCompressedTexImage3D;
PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC glad_glCompressedTexSubImage1D;
void APIENTRY glad_debug_impl_glCompressedTexSubImage1D(GLenum arg0, GLint arg1, GLint arg2, GLsizei arg3, GLenum arg4, GLsizei arg5, const void * arg6) {    
    _pre_call_callback("glCompressedTexSubImage1D", (void*)glCompressedTexSubImage1D, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
     glad_glCompressedTexSubImage1D(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    _post_call_callback("glCompressedTexSubImage1D", (void*)glCompressedTexSubImage1D, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    
}
PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC glad_debug_glCompressedTexSubImage1D = glad_debug_impl_glCompressedTexSubImage1D;
PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC glad_glCompressedTexSubImage2D;
void APIENTRY glad_debug_impl_glCompressedTexSubImage2D(GLenum arg0, GLint arg1, GLint arg2, GLint arg3, GLsizei arg4, GLsizei arg5, GLenum arg6, GLsizei arg7, const void * arg8) {    
    _pre_call_callback("glCompressedTexSubImage2D", (void*)glCompressedTexSubImage2D, 9, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
     glad_glCompressedTexSubImage2D(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
    _post_call_callback("glCompressedTexSubImage2D", (void*)glCompressedTexSubImage2D, 9, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
    
}
PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC glad_debug_glCompressedTexSubImage2D = glad_debug_impl_glCompressedTexSubImage2D;
PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC glad_glCompressedTexSubImage3D;
void APIENTRY glad_debug_impl_glCompressedTexSubImage3D(GLenum arg0, GLint arg1, GLint arg2, GLint arg3, GLint arg4, GLsizei arg5, GLsizei arg6, GLsizei arg7, GLenum arg8, GLsizei arg9, const void * arg10) {    
    _pre_call_callback("glCompressedTexSubImage3D", (void*)glCompressedTexSubImage3D, 11, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10);
     glad_glCompressedTexSubImage3D(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10);
    _post_call_callback("glCompressedTexSubImage3D", (void*)glCompressedTexSubImage3D, 11, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10);
    
}
PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC glad_debug_glCompressedTexSubImage3D = glad_debug_impl_glCompressedTexSubImage3D;
PFNGLCOMPRESSEDTEXTURESUBIMAGE1DPROC glad_glCompressedTextureSubImage1D;
void APIENTRY glad_debug_impl_glCompressedTextureSubImage1D(GLuint arg0, GLint arg1, GLint arg2, GLsizei arg3, GLenum arg4, GLsizei arg5, const void * arg6) {    
    _pre_call_callback("glCompressedTextureSubImage1D", (void*)glCompressedTextureSubImage1D, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
     glad_glCompressedTextureSubImage1D(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    _post_call_callback("glCompressedTextureSubImage1D", (void*)glCompressedTextureSubImage1D, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    
}
PFNGLCOMPRESSEDTEXTURESUBIMAGE1DPROC glad_debug_glCompressedTextureSubImage1D = glad_debug_impl_glCompressedTextureSubImage1D;
PFNGLCOMPRESSEDTEXTURESUBIMAGE2DPROC glad_glCompressedTextureSubImage2D;
void APIENTRY glad_debug_impl_glCompressedTextureSubImage2D(GLuint arg0, GLint arg1, GLint arg2, GLint arg3, GLsizei arg4, GLsizei arg5, GLenum arg6, GLsizei arg7, const void * arg8) {    
    _pre_call_callback("glCompressedTextureSubImage2D", (void*)glCompressedTextureSubImage2D, 9, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
     glad_glCompressedTextureSubImage2D(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
    _post_call_callback("glCompressedTextureSubImage2D", (void*)glCompressedTextureSubImage2D, 9, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
    
}
PFNGLCOMPRESSEDTEXTURESUBIMAGE2DPROC glad_debug_glCompressedTextureSubImage2D = glad_debug_impl_glCompressedTextureSubImage2D;
PFNGLCOMPRESSEDTEXTURESUBIMAGE3DPROC glad_glCompressedTextureSubImage3D;
void APIENTRY glad_debug_impl_glCompressedTextureSubImage3D(GLuint arg0, GLint arg1, GLint arg2, GLint arg3, GLint arg4, GLsizei arg5, GLsizei arg6, GLsizei arg7, GLenum arg8, GLsizei arg9, const void * arg10) {    
    _pre_call_callback("glCompressedTextureSubImage3D", (void*)glCompressedTextureSubImage3D, 11, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10);
     glad_glCompressedTextureSubImage3D(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10);
    _post_call_callback("glCompressedTextureSubImage3D", (void*)glCompressedTextureSubImage3D, 11, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10);
    
}
PFNGLCOMPRESSEDTEXTURESUBIMAGE3DPROC glad_debug_glCompressedTextureSubImage3D = glad_debug_impl_glCompressedTextureSubImage3D;
PFNGLCOPYBUFFERSUBDATAPROC glad_glCopyBufferSubData;
void APIENTRY glad_debug_impl_glCopyBufferSubData(GLenum arg0, GLenum arg1, GLintptr arg2, GLintptr arg3, GLsizeiptr arg4) {    
    _pre_call_callback("glCopyBufferSubData", (void*)glCopyBufferSubData, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glCopyBufferSubData(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glCopyBufferSubData", (void*)glCopyBufferSubData, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLCOPYBUFFERSUBDATAPROC glad_debug_glCopyBufferSubData = glad_debug_impl_glCopyBufferSubData;
PFNGLCOPYIMAGESUBDATAPROC glad_glCopyImageSubData;
void APIENTRY glad_debug_impl_glCopyImageSubData(GLuint arg0, GLenum arg1, GLint arg2, GLint arg3, GLint arg4, GLint arg5, GLuint arg6, GLenum arg7, GLint arg8, GLint arg9, GLint arg10, GLint arg11, GLsizei arg12, GLsizei arg13, GLsizei arg14) {    
    _pre_call_callback("glCopyImageSubData", (void*)glCopyImageSubData, 15, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13, arg14);
     glad_glCopyImageSubData(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13, arg14);
    _post_call_callback("glCopyImageSubData", (void*)glCopyImageSubData, 15, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13, arg14);
    
}
PFNGLCOPYIMAGESUBDATAPROC glad_debug_glCopyImageSubData = glad_debug_impl_glCopyImageSubData;
PFNGLCOPYNAMEDBUFFERSUBDATAPROC glad_glCopyNamedBufferSubData;
void APIENTRY glad_debug_impl_glCopyNamedBufferSubData(GLuint arg0, GLuint arg1, GLintptr arg2, GLintptr arg3, GLsizeiptr arg4) {    
    _pre_call_callback("glCopyNamedBufferSubData", (void*)glCopyNamedBufferSubData, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glCopyNamedBufferSubData(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glCopyNamedBufferSubData", (void*)glCopyNamedBufferSubData, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLCOPYNAMEDBUFFERSUBDATAPROC glad_debug_glCopyNamedBufferSubData = glad_debug_impl_glCopyNamedBufferSubData;
PFNGLCOPYPIXELSPROC glad_glCopyPixels;
void APIENTRY glad_debug_impl_glCopyPixels(GLint arg0, GLint arg1, GLsizei arg2, GLsizei arg3, GLenum arg4) {    
    _pre_call_callback("glCopyPixels", (void*)glCopyPixels, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glCopyPixels(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glCopyPixels", (void*)glCopyPixels, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLCOPYPIXELSPROC glad_debug_glCopyPixels = glad_debug_impl_glCopyPixels;
PFNGLCOPYTEXIMAGE1DPROC glad_glCopyTexImage1D;
void APIENTRY glad_debug_impl_glCopyTexImage1D(GLenum arg0, GLint arg1, GLenum arg2, GLint arg3, GLint arg4, GLsizei arg5, GLint arg6) {    
    _pre_call_callback("glCopyTexImage1D", (void*)glCopyTexImage1D, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
     glad_glCopyTexImage1D(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    _post_call_callback("glCopyTexImage1D", (void*)glCopyTexImage1D, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    
}
PFNGLCOPYTEXIMAGE1DPROC glad_debug_glCopyTexImage1D = glad_debug_impl_glCopyTexImage1D;
PFNGLCOPYTEXIMAGE2DPROC glad_glCopyTexImage2D;
void APIENTRY glad_debug_impl_glCopyTexImage2D(GLenum arg0, GLint arg1, GLenum arg2, GLint arg3, GLint arg4, GLsizei arg5, GLsizei arg6, GLint arg7) {    
    _pre_call_callback("glCopyTexImage2D", (void*)glCopyTexImage2D, 8, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
     glad_glCopyTexImage2D(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    _post_call_callback("glCopyTexImage2D", (void*)glCopyTexImage2D, 8, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    
}
PFNGLCOPYTEXIMAGE2DPROC glad_debug_glCopyTexImage2D = glad_debug_impl_glCopyTexImage2D;
PFNGLCOPYTEXSUBIMAGE1DPROC glad_glCopyTexSubImage1D;
void APIENTRY glad_debug_impl_glCopyTexSubImage1D(GLenum arg0, GLint arg1, GLint arg2, GLint arg3, GLint arg4, GLsizei arg5) {    
    _pre_call_callback("glCopyTexSubImage1D", (void*)glCopyTexSubImage1D, 6, arg0, arg1, arg2, arg3, arg4, arg5);
     glad_glCopyTexSubImage1D(arg0, arg1, arg2, arg3, arg4, arg5);
    _post_call_callback("glCopyTexSubImage1D", (void*)glCopyTexSubImage1D, 6, arg0, arg1, arg2, arg3, arg4, arg5);
    
}
PFNGLCOPYTEXSUBIMAGE1DPROC glad_debug_glCopyTexSubImage1D = glad_debug_impl_glCopyTexSubImage1D;
PFNGLCOPYTEXSUBIMAGE2DPROC glad_glCopyTexSubImage2D;
void APIENTRY glad_debug_impl_glCopyTexSubImage2D(GLenum arg0, GLint arg1, GLint arg2, GLint arg3, GLint arg4, GLint arg5, GLsizei arg6, GLsizei arg7) {    
    _pre_call_callback("glCopyTexSubImage2D", (void*)glCopyTexSubImage2D, 8, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
     glad_glCopyTexSubImage2D(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    _post_call_callback("glCopyTexSubImage2D", (void*)glCopyTexSubImage2D, 8, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    
}
PFNGLCOPYTEXSUBIMAGE2DPROC glad_debug_glCopyTexSubImage2D = glad_debug_impl_glCopyTexSubImage2D;
PFNGLCOPYTEXSUBIMAGE3DPROC glad_glCopyTexSubImage3D;
void APIENTRY glad_debug_impl_glCopyTexSubImage3D(GLenum arg0, GLint arg1, GLint arg2, GLint arg3, GLint arg4, GLint arg5, GLint arg6, GLsizei arg7, GLsizei arg8) {    
    _pre_call_callback("glCopyTexSubImage3D", (void*)glCopyTexSubImage3D, 9, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
     glad_glCopyTexSubImage3D(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
    _post_call_callback("glCopyTexSubImage3D", (void*)glCopyTexSubImage3D, 9, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
    
}
PFNGLCOPYTEXSUBIMAGE3DPROC glad_debug_glCopyTexSubImage3D = glad_debug_impl_glCopyTexSubImage3D;
PFNGLCOPYTEXTURESUBIMAGE1DPROC glad_glCopyTextureSubImage1D;
void APIENTRY glad_debug_impl_glCopyTextureSubImage1D(GLuint arg0, GLint arg1, GLint arg2, GLint arg3, GLint arg4, GLsizei arg5) {    
    _pre_call_callback("glCopyTextureSubImage1D", (void*)glCopyTextureSubImage1D, 6, arg0, arg1, arg2, arg3, arg4, arg5);
     glad_glCopyTextureSubImage1D(arg0, arg1, arg2, arg3, arg4, arg5);
    _post_call_callback("glCopyTextureSubImage1D", (void*)glCopyTextureSubImage1D, 6, arg0, arg1, arg2, arg3, arg4, arg5);
    
}
PFNGLCOPYTEXTURESUBIMAGE1DPROC glad_debug_glCopyTextureSubImage1D = glad_debug_impl_glCopyTextureSubImage1D;
PFNGLCOPYTEXTURESUBIMAGE2DPROC glad_glCopyTextureSubImage2D;
void APIENTRY glad_debug_impl_glCopyTextureSubImage2D(GLuint arg0, GLint arg1, GLint arg2, GLint arg3, GLint arg4, GLint arg5, GLsizei arg6, GLsizei arg7) {    
    _pre_call_callback("glCopyTextureSubImage2D", (void*)glCopyTextureSubImage2D, 8, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
     glad_glCopyTextureSubImage2D(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    _post_call_callback("glCopyTextureSubImage2D", (void*)glCopyTextureSubImage2D, 8, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    
}
PFNGLCOPYTEXTURESUBIMAGE2DPROC glad_debug_glCopyTextureSubImage2D = glad_debug_impl_glCopyTextureSubImage2D;
PFNGLCOPYTEXTURESUBIMAGE3DPROC glad_glCopyTextureSubImage3D;
void APIENTRY glad_debug_impl_glCopyTextureSubImage3D(GLuint arg0, GLint arg1, GLint arg2, GLint arg3, GLint arg4, GLint arg5, GLint arg6, GLsizei arg7, GLsizei arg8) {    
    _pre_call_callback("glCopyTextureSubImage3D", (void*)glCopyTextureSubImage3D, 9, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
     glad_glCopyTextureSubImage3D(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
    _post_call_callback("glCopyTextureSubImage3D", (void*)glCopyTextureSubImage3D, 9, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
    
}
PFNGLCOPYTEXTURESUBIMAGE3DPROC glad_debug_glCopyTextureSubImage3D = glad_debug_impl_glCopyTextureSubImage3D;
PFNGLCREATEBUFFERSPROC glad_glCreateBuffers;
void APIENTRY glad_debug_impl_glCreateBuffers(GLsizei arg0, GLuint * arg1) {    
    _pre_call_callback("glCreateBuffers", (void*)glCreateBuffers, 2, arg0, arg1);
     glad_glCreateBuffers(arg0, arg1);
    _post_call_callback("glCreateBuffers", (void*)glCreateBuffers, 2, arg0, arg1);
    
}
PFNGLCREATEBUFFERSPROC glad_debug_glCreateBuffers = glad_debug_impl_glCreateBuffers;
PFNGLCREATEFRAMEBUFFERSPROC glad_glCreateFramebuffers;
void APIENTRY glad_debug_impl_glCreateFramebuffers(GLsizei arg0, GLuint * arg1) {    
    _pre_call_callback("glCreateFramebuffers", (void*)glCreateFramebuffers, 2, arg0, arg1);
     glad_glCreateFramebuffers(arg0, arg1);
    _post_call_callback("glCreateFramebuffers", (void*)glCreateFramebuffers, 2, arg0, arg1);
    
}
PFNGLCREATEFRAMEBUFFERSPROC glad_debug_glCreateFramebuffers = glad_debug_impl_glCreateFramebuffers;
PFNGLCREATEPROGRAMPROC glad_glCreateProgram;
GLuint APIENTRY glad_debug_impl_glCreateProgram(void) {    
    GLuint ret;
    _pre_call_callback("glCreateProgram", (void*)glCreateProgram, 0);
    ret =  glad_glCreateProgram();
    _post_call_callback("glCreateProgram", (void*)glCreateProgram, 0);
    return ret;
}
PFNGLCREATEPROGRAMPROC glad_debug_glCreateProgram = glad_debug_impl_glCreateProgram;
PFNGLCREATEPROGRAMPIPELINESPROC glad_glCreateProgramPipelines;
void APIENTRY glad_debug_impl_glCreateProgramPipelines(GLsizei arg0, GLuint * arg1) {    
    _pre_call_callback("glCreateProgramPipelines", (void*)glCreateProgramPipelines, 2, arg0, arg1);
     glad_glCreateProgramPipelines(arg0, arg1);
    _post_call_callback("glCreateProgramPipelines", (void*)glCreateProgramPipelines, 2, arg0, arg1);
    
}
PFNGLCREATEPROGRAMPIPELINESPROC glad_debug_glCreateProgramPipelines = glad_debug_impl_glCreateProgramPipelines;
PFNGLCREATEQUERIESPROC glad_glCreateQueries;
void APIENTRY glad_debug_impl_glCreateQueries(GLenum arg0, GLsizei arg1, GLuint * arg2) {    
    _pre_call_callback("glCreateQueries", (void*)glCreateQueries, 3, arg0, arg1, arg2);
     glad_glCreateQueries(arg0, arg1, arg2);
    _post_call_callback("glCreateQueries", (void*)glCreateQueries, 3, arg0, arg1, arg2);
    
}
PFNGLCREATEQUERIESPROC glad_debug_glCreateQueries = glad_debug_impl_glCreateQueries;
PFNGLCREATERENDERBUFFERSPROC glad_glCreateRenderbuffers;
void APIENTRY glad_debug_impl_glCreateRenderbuffers(GLsizei arg0, GLuint * arg1) {    
    _pre_call_callback("glCreateRenderbuffers", (void*)glCreateRenderbuffers, 2, arg0, arg1);
     glad_glCreateRenderbuffers(arg0, arg1);
    _post_call_callback("glCreateRenderbuffers", (void*)glCreateRenderbuffers, 2, arg0, arg1);
    
}
PFNGLCREATERENDERBUFFERSPROC glad_debug_glCreateRenderbuffers = glad_debug_impl_glCreateRenderbuffers;
PFNGLCREATESAMPLERSPROC glad_glCreateSamplers;
void APIENTRY glad_debug_impl_glCreateSamplers(GLsizei arg0, GLuint * arg1) {    
    _pre_call_callback("glCreateSamplers", (void*)glCreateSamplers, 2, arg0, arg1);
     glad_glCreateSamplers(arg0, arg1);
    _post_call_callback("glCreateSamplers", (void*)glCreateSamplers, 2, arg0, arg1);
    
}
PFNGLCREATESAMPLERSPROC glad_debug_glCreateSamplers = glad_debug_impl_glCreateSamplers;
PFNGLCREATESHADERPROC glad_glCreateShader;
GLuint APIENTRY glad_debug_impl_glCreateShader(GLenum arg0) {    
    GLuint ret;
    _pre_call_callback("glCreateShader", (void*)glCreateShader, 1, arg0);
    ret =  glad_glCreateShader(arg0);
    _post_call_callback("glCreateShader", (void*)glCreateShader, 1, arg0);
    return ret;
}
PFNGLCREATESHADERPROC glad_debug_glCreateShader = glad_debug_impl_glCreateShader;
PFNGLCREATESHADERPROGRAMVPROC glad_glCreateShaderProgramv;
GLuint APIENTRY glad_debug_impl_glCreateShaderProgramv(GLenum arg0, GLsizei arg1, const GLchar *const* arg2) {    
    GLuint ret;
    _pre_call_callback("glCreateShaderProgramv", (void*)glCreateShaderProgramv, 3, arg0, arg1, arg2);
    ret =  glad_glCreateShaderProgramv(arg0, arg1, arg2);
    _post_call_callback("glCreateShaderProgramv", (void*)glCreateShaderProgramv, 3, arg0, arg1, arg2);
    return ret;
}
PFNGLCREATESHADERPROGRAMVPROC glad_debug_glCreateShaderProgramv = glad_debug_impl_glCreateShaderProgramv;
PFNGLCREATETEXTURESPROC glad_glCreateTextures;
void APIENTRY glad_debug_impl_glCreateTextures(GLenum arg0, GLsizei arg1, GLuint * arg2) {    
    _pre_call_callback("glCreateTextures", (void*)glCreateTextures, 3, arg0, arg1, arg2);
     glad_glCreateTextures(arg0, arg1, arg2);
    _post_call_callback("glCreateTextures", (void*)glCreateTextures, 3, arg0, arg1, arg2);
    
}
PFNGLCREATETEXTURESPROC glad_debug_glCreateTextures = glad_debug_impl_glCreateTextures;
PFNGLCREATETRANSFORMFEEDBACKSPROC glad_glCreateTransformFeedbacks;
void APIENTRY glad_debug_impl_glCreateTransformFeedbacks(GLsizei arg0, GLuint * arg1) {    
    _pre_call_callback("glCreateTransformFeedbacks", (void*)glCreateTransformFeedbacks, 2, arg0, arg1);
     glad_glCreateTransformFeedbacks(arg0, arg1);
    _post_call_callback("glCreateTransformFeedbacks", (void*)glCreateTransformFeedbacks, 2, arg0, arg1);
    
}
PFNGLCREATETRANSFORMFEEDBACKSPROC glad_debug_glCreateTransformFeedbacks = glad_debug_impl_glCreateTransformFeedbacks;
PFNGLCREATEVERTEXARRAYSPROC glad_glCreateVertexArrays;
void APIENTRY glad_debug_impl_glCreateVertexArrays(GLsizei arg0, GLuint * arg1) {    
    _pre_call_callback("glCreateVertexArrays", (void*)glCreateVertexArrays, 2, arg0, arg1);
     glad_glCreateVertexArrays(arg0, arg1);
    _post_call_callback("glCreateVertexArrays", (void*)glCreateVertexArrays, 2, arg0, arg1);
    
}
PFNGLCREATEVERTEXARRAYSPROC glad_debug_glCreateVertexArrays = glad_debug_impl_glCreateVertexArrays;
PFNGLCULLFACEPROC glad_glCullFace;
void APIENTRY glad_debug_impl_glCullFace(GLenum arg0) {    
    _pre_call_callback("glCullFace", (void*)glCullFace, 1, arg0);
     glad_glCullFace(arg0);
    _post_call_callback("glCullFace", (void*)glCullFace, 1, arg0);
    
}
PFNGLCULLFACEPROC glad_debug_glCullFace = glad_debug_impl_glCullFace;
PFNGLDEBUGMESSAGECALLBACKPROC glad_glDebugMessageCallback;
void APIENTRY glad_debug_impl_glDebugMessageCallback(GLDEBUGPROC arg0, const void * arg1) {    
    _pre_call_callback("glDebugMessageCallback", (void*)glDebugMessageCallback, 2, arg0, arg1);
     glad_glDebugMessageCallback(arg0, arg1);
    _post_call_callback("glDebugMessageCallback", (void*)glDebugMessageCallback, 2, arg0, arg1);
    
}
PFNGLDEBUGMESSAGECALLBACKPROC glad_debug_glDebugMessageCallback = glad_debug_impl_glDebugMessageCallback;
PFNGLDEBUGMESSAGECONTROLPROC glad_glDebugMessageControl;
void APIENTRY glad_debug_impl_glDebugMessageControl(GLenum arg0, GLenum arg1, GLenum arg2, GLsizei arg3, const GLuint * arg4, GLboolean arg5) {    
    _pre_call_callback("glDebugMessageControl", (void*)glDebugMessageControl, 6, arg0, arg1, arg2, arg3, arg4, arg5);
     glad_glDebugMessageControl(arg0, arg1, arg2, arg3, arg4, arg5);
    _post_call_callback("glDebugMessageControl", (void*)glDebugMessageControl, 6, arg0, arg1, arg2, arg3, arg4, arg5);
    
}
PFNGLDEBUGMESSAGECONTROLPROC glad_debug_glDebugMessageControl = glad_debug_impl_glDebugMessageControl;
PFNGLDEBUGMESSAGEINSERTPROC glad_glDebugMessageInsert;
void APIENTRY glad_debug_impl_glDebugMessageInsert(GLenum arg0, GLenum arg1, GLuint arg2, GLenum arg3, GLsizei arg4, const GLchar * arg5) {    
    _pre_call_callback("glDebugMessageInsert", (void*)glDebugMessageInsert, 6, arg0, arg1, arg2, arg3, arg4, arg5);
     glad_glDebugMessageInsert(arg0, arg1, arg2, arg3, arg4, arg5);
    _post_call_callback("glDebugMessageInsert", (void*)glDebugMessageInsert, 6, arg0, arg1, arg2, arg3, arg4, arg5);
    
}
PFNGLDEBUGMESSAGEINSERTPROC glad_debug_glDebugMessageInsert = glad_debug_impl_glDebugMessageInsert;
PFNGLDELETEBUFFERSPROC glad_glDeleteBuffers;
void APIENTRY glad_debug_impl_glDeleteBuffers(GLsizei arg0, const GLuint * arg1) {    
    _pre_call_callback("glDeleteBuffers", (void*)glDeleteBuffers, 2, arg0, arg1);
     glad_glDeleteBuffers(arg0, arg1);
    _post_call_callback("glDeleteBuffers", (void*)glDeleteBuffers, 2, arg0, arg1);
    
}
PFNGLDELETEBUFFERSPROC glad_debug_glDeleteBuffers = glad_debug_impl_glDeleteBuffers;
PFNGLDELETEFRAMEBUFFERSPROC glad_glDeleteFramebuffers;
void APIENTRY glad_debug_impl_glDeleteFramebuffers(GLsizei arg0, const GLuint * arg1) {    
    _pre_call_callback("glDeleteFramebuffers", (void*)glDeleteFramebuffers, 2, arg0, arg1);
     glad_glDeleteFramebuffers(arg0, arg1);
    _post_call_callback("glDeleteFramebuffers", (void*)glDeleteFramebuffers, 2, arg0, arg1);
    
}
PFNGLDELETEFRAMEBUFFERSPROC glad_debug_glDeleteFramebuffers = glad_debug_impl_glDeleteFramebuffers;
PFNGLDELETELISTSPROC glad_glDeleteLists;
void APIENTRY glad_debug_impl_glDeleteLists(GLuint arg0, GLsizei arg1) {    
    _pre_call_callback("glDeleteLists", (void*)glDeleteLists, 2, arg0, arg1);
     glad_glDeleteLists(arg0, arg1);
    _post_call_callback("glDeleteLists", (void*)glDeleteLists, 2, arg0, arg1);
    
}
PFNGLDELETELISTSPROC glad_debug_glDeleteLists = glad_debug_impl_glDeleteLists;
PFNGLDELETEPROGRAMPROC glad_glDeleteProgram;
void APIENTRY glad_debug_impl_glDeleteProgram(GLuint arg0) {    
    _pre_call_callback("glDeleteProgram", (void*)glDeleteProgram, 1, arg0);
     glad_glDeleteProgram(arg0);
    _post_call_callback("glDeleteProgram", (void*)glDeleteProgram, 1, arg0);
    
}
PFNGLDELETEPROGRAMPROC glad_debug_glDeleteProgram = glad_debug_impl_glDeleteProgram;
PFNGLDELETEPROGRAMPIPELINESPROC glad_glDeleteProgramPipelines;
void APIENTRY glad_debug_impl_glDeleteProgramPipelines(GLsizei arg0, const GLuint * arg1) {    
    _pre_call_callback("glDeleteProgramPipelines", (void*)glDeleteProgramPipelines, 2, arg0, arg1);
     glad_glDeleteProgramPipelines(arg0, arg1);
    _post_call_callback("glDeleteProgramPipelines", (void*)glDeleteProgramPipelines, 2, arg0, arg1);
    
}
PFNGLDELETEPROGRAMPIPELINESPROC glad_debug_glDeleteProgramPipelines = glad_debug_impl_glDeleteProgramPipelines;
PFNGLDELETEQUERIESPROC glad_glDeleteQueries;
void APIENTRY glad_debug_impl_glDeleteQueries(GLsizei arg0, const GLuint * arg1) {    
    _pre_call_callback("glDeleteQueries", (void*)glDeleteQueries, 2, arg0, arg1);
     glad_glDeleteQueries(arg0, arg1);
    _post_call_callback("glDeleteQueries", (void*)glDeleteQueries, 2, arg0, arg1);
    
}
PFNGLDELETEQUERIESPROC glad_debug_glDeleteQueries = glad_debug_impl_glDeleteQueries;
PFNGLDELETERENDERBUFFERSPROC glad_glDeleteRenderbuffers;
void APIENTRY glad_debug_impl_glDeleteRenderbuffers(GLsizei arg0, const GLuint * arg1) {    
    _pre_call_callback("glDeleteRenderbuffers", (void*)glDeleteRenderbuffers, 2, arg0, arg1);
     glad_glDeleteRenderbuffers(arg0, arg1);
    _post_call_callback("glDeleteRenderbuffers", (void*)glDeleteRenderbuffers, 2, arg0, arg1);
    
}
PFNGLDELETERENDERBUFFERSPROC glad_debug_glDeleteRenderbuffers = glad_debug_impl_glDeleteRenderbuffers;
PFNGLDELETESAMPLERSPROC glad_glDeleteSamplers;
void APIENTRY glad_debug_impl_glDeleteSamplers(GLsizei arg0, const GLuint * arg1) {    
    _pre_call_callback("glDeleteSamplers", (void*)glDeleteSamplers, 2, arg0, arg1);
     glad_glDeleteSamplers(arg0, arg1);
    _post_call_callback("glDeleteSamplers", (void*)glDeleteSamplers, 2, arg0, arg1);
    
}
PFNGLDELETESAMPLERSPROC glad_debug_glDeleteSamplers = glad_debug_impl_glDeleteSamplers;
PFNGLDELETESHADERPROC glad_glDeleteShader;
void APIENTRY glad_debug_impl_glDeleteShader(GLuint arg0) {    
    _pre_call_callback("glDeleteShader", (void*)glDeleteShader, 1, arg0);
     glad_glDeleteShader(arg0);
    _post_call_callback("glDeleteShader", (void*)glDeleteShader, 1, arg0);
    
}
PFNGLDELETESHADERPROC glad_debug_glDeleteShader = glad_debug_impl_glDeleteShader;
PFNGLDELETESYNCPROC glad_glDeleteSync;
void APIENTRY glad_debug_impl_glDeleteSync(GLsync arg0) {    
    _pre_call_callback("glDeleteSync", (void*)glDeleteSync, 1, arg0);
     glad_glDeleteSync(arg0);
    _post_call_callback("glDeleteSync", (void*)glDeleteSync, 1, arg0);
    
}
PFNGLDELETESYNCPROC glad_debug_glDeleteSync = glad_debug_impl_glDeleteSync;
PFNGLDELETETEXTURESPROC glad_glDeleteTextures;
void APIENTRY glad_debug_impl_glDeleteTextures(GLsizei arg0, const GLuint * arg1) {    
    _pre_call_callback("glDeleteTextures", (void*)glDeleteTextures, 2, arg0, arg1);
     glad_glDeleteTextures(arg0, arg1);
    _post_call_callback("glDeleteTextures", (void*)glDeleteTextures, 2, arg0, arg1);
    
}
PFNGLDELETETEXTURESPROC glad_debug_glDeleteTextures = glad_debug_impl_glDeleteTextures;
PFNGLDELETETRANSFORMFEEDBACKSPROC glad_glDeleteTransformFeedbacks;
void APIENTRY glad_debug_impl_glDeleteTransformFeedbacks(GLsizei arg0, const GLuint * arg1) {    
    _pre_call_callback("glDeleteTransformFeedbacks", (void*)glDeleteTransformFeedbacks, 2, arg0, arg1);
     glad_glDeleteTransformFeedbacks(arg0, arg1);
    _post_call_callback("glDeleteTransformFeedbacks", (void*)glDeleteTransformFeedbacks, 2, arg0, arg1);
    
}
PFNGLDELETETRANSFORMFEEDBACKSPROC glad_debug_glDeleteTransformFeedbacks = glad_debug_impl_glDeleteTransformFeedbacks;
PFNGLDELETEVERTEXARRAYSPROC glad_glDeleteVertexArrays;
void APIENTRY glad_debug_impl_glDeleteVertexArrays(GLsizei arg0, const GLuint * arg1) {    
    _pre_call_callback("glDeleteVertexArrays", (void*)glDeleteVertexArrays, 2, arg0, arg1);
     glad_glDeleteVertexArrays(arg0, arg1);
    _post_call_callback("glDeleteVertexArrays", (void*)glDeleteVertexArrays, 2, arg0, arg1);
    
}
PFNGLDELETEVERTEXARRAYSPROC glad_debug_glDeleteVertexArrays = glad_debug_impl_glDeleteVertexArrays;
PFNGLDEPTHFUNCPROC glad_glDepthFunc;
void APIENTRY glad_debug_impl_glDepthFunc(GLenum arg0) {    
    _pre_call_callback("glDepthFunc", (void*)glDepthFunc, 1, arg0);
     glad_glDepthFunc(arg0);
    _post_call_callback("glDepthFunc", (void*)glDepthFunc, 1, arg0);
    
}
PFNGLDEPTHFUNCPROC glad_debug_glDepthFunc = glad_debug_impl_glDepthFunc;
PFNGLDEPTHMASKPROC glad_glDepthMask;
void APIENTRY glad_debug_impl_glDepthMask(GLboolean arg0) {    
    _pre_call_callback("glDepthMask", (void*)glDepthMask, 1, arg0);
     glad_glDepthMask(arg0);
    _post_call_callback("glDepthMask", (void*)glDepthMask, 1, arg0);
    
}
PFNGLDEPTHMASKPROC glad_debug_glDepthMask = glad_debug_impl_glDepthMask;
PFNGLDEPTHRANGEPROC glad_glDepthRange;
void APIENTRY glad_debug_impl_glDepthRange(GLdouble arg0, GLdouble arg1) {    
    _pre_call_callback("glDepthRange", (void*)glDepthRange, 2, arg0, arg1);
     glad_glDepthRange(arg0, arg1);
    _post_call_callback("glDepthRange", (void*)glDepthRange, 2, arg0, arg1);
    
}
PFNGLDEPTHRANGEPROC glad_debug_glDepthRange = glad_debug_impl_glDepthRange;
PFNGLDEPTHRANGEARRAYVPROC glad_glDepthRangeArrayv;
void APIENTRY glad_debug_impl_glDepthRangeArrayv(GLuint arg0, GLsizei arg1, const GLdouble * arg2) {    
    _pre_call_callback("glDepthRangeArrayv", (void*)glDepthRangeArrayv, 3, arg0, arg1, arg2);
     glad_glDepthRangeArrayv(arg0, arg1, arg2);
    _post_call_callback("glDepthRangeArrayv", (void*)glDepthRangeArrayv, 3, arg0, arg1, arg2);
    
}
PFNGLDEPTHRANGEARRAYVPROC glad_debug_glDepthRangeArrayv = glad_debug_impl_glDepthRangeArrayv;
PFNGLDEPTHRANGEINDEXEDPROC glad_glDepthRangeIndexed;
void APIENTRY glad_debug_impl_glDepthRangeIndexed(GLuint arg0, GLdouble arg1, GLdouble arg2) {    
    _pre_call_callback("glDepthRangeIndexed", (void*)glDepthRangeIndexed, 3, arg0, arg1, arg2);
     glad_glDepthRangeIndexed(arg0, arg1, arg2);
    _post_call_callback("glDepthRangeIndexed", (void*)glDepthRangeIndexed, 3, arg0, arg1, arg2);
    
}
PFNGLDEPTHRANGEINDEXEDPROC glad_debug_glDepthRangeIndexed = glad_debug_impl_glDepthRangeIndexed;
PFNGLDEPTHRANGEFPROC glad_glDepthRangef;
void APIENTRY glad_debug_impl_glDepthRangef(GLfloat arg0, GLfloat arg1) {    
    _pre_call_callback("glDepthRangef", (void*)glDepthRangef, 2, arg0, arg1);
     glad_glDepthRangef(arg0, arg1);
    _post_call_callback("glDepthRangef", (void*)glDepthRangef, 2, arg0, arg1);
    
}
PFNGLDEPTHRANGEFPROC glad_debug_glDepthRangef = glad_debug_impl_glDepthRangef;
PFNGLDETACHSHADERPROC glad_glDetachShader;
void APIENTRY glad_debug_impl_glDetachShader(GLuint arg0, GLuint arg1) {    
    _pre_call_callback("glDetachShader", (void*)glDetachShader, 2, arg0, arg1);
     glad_glDetachShader(arg0, arg1);
    _post_call_callback("glDetachShader", (void*)glDetachShader, 2, arg0, arg1);
    
}
PFNGLDETACHSHADERPROC glad_debug_glDetachShader = glad_debug_impl_glDetachShader;
PFNGLDISABLEPROC glad_glDisable;
void APIENTRY glad_debug_impl_glDisable(GLenum arg0) {    
    _pre_call_callback("glDisable", (void*)glDisable, 1, arg0);
     glad_glDisable(arg0);
    _post_call_callback("glDisable", (void*)glDisable, 1, arg0);
    
}
PFNGLDISABLEPROC glad_debug_glDisable = glad_debug_impl_glDisable;
PFNGLDISABLECLIENTSTATEPROC glad_glDisableClientState;
void APIENTRY glad_debug_impl_glDisableClientState(GLenum arg0) {    
    _pre_call_callback("glDisableClientState", (void*)glDisableClientState, 1, arg0);
     glad_glDisableClientState(arg0);
    _post_call_callback("glDisableClientState", (void*)glDisableClientState, 1, arg0);
    
}
PFNGLDISABLECLIENTSTATEPROC glad_debug_glDisableClientState = glad_debug_impl_glDisableClientState;
PFNGLDISABLEVERTEXARRAYATTRIBPROC glad_glDisableVertexArrayAttrib;
void APIENTRY glad_debug_impl_glDisableVertexArrayAttrib(GLuint arg0, GLuint arg1) {    
    _pre_call_callback("glDisableVertexArrayAttrib", (void*)glDisableVertexArrayAttrib, 2, arg0, arg1);
     glad_glDisableVertexArrayAttrib(arg0, arg1);
    _post_call_callback("glDisableVertexArrayAttrib", (void*)glDisableVertexArrayAttrib, 2, arg0, arg1);
    
}
PFNGLDISABLEVERTEXARRAYATTRIBPROC glad_debug_glDisableVertexArrayAttrib = glad_debug_impl_glDisableVertexArrayAttrib;
PFNGLDISABLEVERTEXATTRIBARRAYPROC glad_glDisableVertexAttribArray;
void APIENTRY glad_debug_impl_glDisableVertexAttribArray(GLuint arg0) {    
    _pre_call_callback("glDisableVertexAttribArray", (void*)glDisableVertexAttribArray, 1, arg0);
     glad_glDisableVertexAttribArray(arg0);
    _post_call_callback("glDisableVertexAttribArray", (void*)glDisableVertexAttribArray, 1, arg0);
    
}
PFNGLDISABLEVERTEXATTRIBARRAYPROC glad_debug_glDisableVertexAttribArray = glad_debug_impl_glDisableVertexAttribArray;
PFNGLDISABLEIPROC glad_glDisablei;
void APIENTRY glad_debug_impl_glDisablei(GLenum arg0, GLuint arg1) {    
    _pre_call_callback("glDisablei", (void*)glDisablei, 2, arg0, arg1);
     glad_glDisablei(arg0, arg1);
    _post_call_callback("glDisablei", (void*)glDisablei, 2, arg0, arg1);
    
}
PFNGLDISABLEIPROC glad_debug_glDisablei = glad_debug_impl_glDisablei;
PFNGLDISPATCHCOMPUTEPROC glad_glDispatchCompute;
void APIENTRY glad_debug_impl_glDispatchCompute(GLuint arg0, GLuint arg1, GLuint arg2) {    
    _pre_call_callback("glDispatchCompute", (void*)glDispatchCompute, 3, arg0, arg1, arg2);
     glad_glDispatchCompute(arg0, arg1, arg2);
    _post_call_callback("glDispatchCompute", (void*)glDispatchCompute, 3, arg0, arg1, arg2);
    
}
PFNGLDISPATCHCOMPUTEPROC glad_debug_glDispatchCompute = glad_debug_impl_glDispatchCompute;
PFNGLDISPATCHCOMPUTEINDIRECTPROC glad_glDispatchComputeIndirect;
void APIENTRY glad_debug_impl_glDispatchComputeIndirect(GLintptr arg0) {    
    _pre_call_callback("glDispatchComputeIndirect", (void*)glDispatchComputeIndirect, 1, arg0);
     glad_glDispatchComputeIndirect(arg0);
    _post_call_callback("glDispatchComputeIndirect", (void*)glDispatchComputeIndirect, 1, arg0);
    
}
PFNGLDISPATCHCOMPUTEINDIRECTPROC glad_debug_glDispatchComputeIndirect = glad_debug_impl_glDispatchComputeIndirect;
PFNGLDRAWARRAYSPROC glad_glDrawArrays;
void APIENTRY glad_debug_impl_glDrawArrays(GLenum arg0, GLint arg1, GLsizei arg2) {    
    _pre_call_callback("glDrawArrays", (void*)glDrawArrays, 3, arg0, arg1, arg2);
     glad_glDrawArrays(arg0, arg1, arg2);
    _post_call_callback("glDrawArrays", (void*)glDrawArrays, 3, arg0, arg1, arg2);
    
}
PFNGLDRAWARRAYSPROC glad_debug_glDrawArrays = glad_debug_impl_glDrawArrays;
PFNGLDRAWARRAYSINDIRECTPROC glad_glDrawArraysIndirect;
void APIENTRY glad_debug_impl_glDrawArraysIndirect(GLenum arg0, const void * arg1) {    
    _pre_call_callback("glDrawArraysIndirect", (void*)glDrawArraysIndirect, 2, arg0, arg1);
     glad_glDrawArraysIndirect(arg0, arg1);
    _post_call_callback("glDrawArraysIndirect", (void*)glDrawArraysIndirect, 2, arg0, arg1);
    
}
PFNGLDRAWARRAYSINDIRECTPROC glad_debug_glDrawArraysIndirect = glad_debug_impl_glDrawArraysIndirect;
PFNGLDRAWARRAYSINSTANCEDPROC glad_glDrawArraysInstanced;
void APIENTRY glad_debug_impl_glDrawArraysInstanced(GLenum arg0, GLint arg1, GLsizei arg2, GLsizei arg3) {    
    _pre_call_callback("glDrawArraysInstanced", (void*)glDrawArraysInstanced, 4, arg0, arg1, arg2, arg3);
     glad_glDrawArraysInstanced(arg0, arg1, arg2, arg3);
    _post_call_callback("glDrawArraysInstanced", (void*)glDrawArraysInstanced, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLDRAWARRAYSINSTANCEDPROC glad_debug_glDrawArraysInstanced = glad_debug_impl_glDrawArraysInstanced;
PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEPROC glad_glDrawArraysInstancedBaseInstance;
void APIENTRY glad_debug_impl_glDrawArraysInstancedBaseInstance(GLenum arg0, GLint arg1, GLsizei arg2, GLsizei arg3, GLuint arg4) {    
    _pre_call_callback("glDrawArraysInstancedBaseInstance", (void*)glDrawArraysInstancedBaseInstance, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glDrawArraysInstancedBaseInstance(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glDrawArraysInstancedBaseInstance", (void*)glDrawArraysInstancedBaseInstance, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEPROC glad_debug_glDrawArraysInstancedBaseInstance = glad_debug_impl_glDrawArraysInstancedBaseInstance;
PFNGLDRAWBUFFERPROC glad_glDrawBuffer;
void APIENTRY glad_debug_impl_glDrawBuffer(GLenum arg0) {    
    _pre_call_callback("glDrawBuffer", (void*)glDrawBuffer, 1, arg0);
     glad_glDrawBuffer(arg0);
    _post_call_callback("glDrawBuffer", (void*)glDrawBuffer, 1, arg0);
    
}
PFNGLDRAWBUFFERPROC glad_debug_glDrawBuffer = glad_debug_impl_glDrawBuffer;
PFNGLDRAWBUFFERSPROC glad_glDrawBuffers;
void APIENTRY glad_debug_impl_glDrawBuffers(GLsizei arg0, const GLenum * arg1) {    
    _pre_call_callback("glDrawBuffers", (void*)glDrawBuffers, 2, arg0, arg1);
     glad_glDrawBuffers(arg0, arg1);
    _post_call_callback("glDrawBuffers", (void*)glDrawBuffers, 2, arg0, arg1);
    
}
PFNGLDRAWBUFFERSPROC glad_debug_glDrawBuffers = glad_debug_impl_glDrawBuffers;
PFNGLDRAWELEMENTSPROC glad_glDrawElements;
void APIENTRY glad_debug_impl_glDrawElements(GLenum arg0, GLsizei arg1, GLenum arg2, const void * arg3) {    
    _pre_call_callback("glDrawElements", (void*)glDrawElements, 4, arg0, arg1, arg2, arg3);
     glad_glDrawElements(arg0, arg1, arg2, arg3);
    _post_call_callback("glDrawElements", (void*)glDrawElements, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLDRAWELEMENTSPROC glad_debug_glDrawElements = glad_debug_impl_glDrawElements;
PFNGLDRAWELEMENTSBASEVERTEXPROC glad_glDrawElementsBaseVertex;
void APIENTRY glad_debug_impl_glDrawElementsBaseVertex(GLenum arg0, GLsizei arg1, GLenum arg2, const void * arg3, GLint arg4) {    
    _pre_call_callback("glDrawElementsBaseVertex", (void*)glDrawElementsBaseVertex, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glDrawElementsBaseVertex(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glDrawElementsBaseVertex", (void*)glDrawElementsBaseVertex, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLDRAWELEMENTSBASEVERTEXPROC glad_debug_glDrawElementsBaseVertex = glad_debug_impl_glDrawElementsBaseVertex;
PFNGLDRAWELEMENTSINDIRECTPROC glad_glDrawElementsIndirect;
void APIENTRY glad_debug_impl_glDrawElementsIndirect(GLenum arg0, GLenum arg1, const void * arg2) {    
    _pre_call_callback("glDrawElementsIndirect", (void*)glDrawElementsIndirect, 3, arg0, arg1, arg2);
     glad_glDrawElementsIndirect(arg0, arg1, arg2);
    _post_call_callback("glDrawElementsIndirect", (void*)glDrawElementsIndirect, 3, arg0, arg1, arg2);
    
}
PFNGLDRAWELEMENTSINDIRECTPROC glad_debug_glDrawElementsIndirect = glad_debug_impl_glDrawElementsIndirect;
PFNGLDRAWELEMENTSINSTANCEDPROC glad_glDrawElementsInstanced;
void APIENTRY glad_debug_impl_glDrawElementsInstanced(GLenum arg0, GLsizei arg1, GLenum arg2, const void * arg3, GLsizei arg4) {    
    _pre_call_callback("glDrawElementsInstanced", (void*)glDrawElementsInstanced, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glDrawElementsInstanced(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glDrawElementsInstanced", (void*)glDrawElementsInstanced, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLDRAWELEMENTSINSTANCEDPROC glad_debug_glDrawElementsInstanced = glad_debug_impl_glDrawElementsInstanced;
PFNGLDRAWELEMENTSINSTANCEDBASEINSTANCEPROC glad_glDrawElementsInstancedBaseInstance;
void APIENTRY glad_debug_impl_glDrawElementsInstancedBaseInstance(GLenum arg0, GLsizei arg1, GLenum arg2, const void * arg3, GLsizei arg4, GLuint arg5) {    
    _pre_call_callback("glDrawElementsInstancedBaseInstance", (void*)glDrawElementsInstancedBaseInstance, 6, arg0, arg1, arg2, arg3, arg4, arg5);
     glad_glDrawElementsInstancedBaseInstance(arg0, arg1, arg2, arg3, arg4, arg5);
    _post_call_callback("glDrawElementsInstancedBaseInstance", (void*)glDrawElementsInstancedBaseInstance, 6, arg0, arg1, arg2, arg3, arg4, arg5);
    
}
PFNGLDRAWELEMENTSINSTANCEDBASEINSTANCEPROC glad_debug_glDrawElementsInstancedBaseInstance = glad_debug_impl_glDrawElementsInstancedBaseInstance;
PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC glad_glDrawElementsInstancedBaseVertex;
void APIENTRY glad_debug_impl_glDrawElementsInstancedBaseVertex(GLenum arg0, GLsizei arg1, GLenum arg2, const void * arg3, GLsizei arg4, GLint arg5) {    
    _pre_call_callback("glDrawElementsInstancedBaseVertex", (void*)glDrawElementsInstancedBaseVertex, 6, arg0, arg1, arg2, arg3, arg4, arg5);
     glad_glDrawElementsInstancedBaseVertex(arg0, arg1, arg2, arg3, arg4, arg5);
    _post_call_callback("glDrawElementsInstancedBaseVertex", (void*)glDrawElementsInstancedBaseVertex, 6, arg0, arg1, arg2, arg3, arg4, arg5);
    
}
PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC glad_debug_glDrawElementsInstancedBaseVertex = glad_debug_impl_glDrawElementsInstancedBaseVertex;
PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEPROC glad_glDrawElementsInstancedBaseVertexBaseInstance;
void APIENTRY glad_debug_impl_glDrawElementsInstancedBaseVertexBaseInstance(GLenum arg0, GLsizei arg1, GLenum arg2, const void * arg3, GLsizei arg4, GLint arg5, GLuint arg6) {    
    _pre_call_callback("glDrawElementsInstancedBaseVertexBaseInstance", (void*)glDrawElementsInstancedBaseVertexBaseInstance, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
     glad_glDrawElementsInstancedBaseVertexBaseInstance(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    _post_call_callback("glDrawElementsInstancedBaseVertexBaseInstance", (void*)glDrawElementsInstancedBaseVertexBaseInstance, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    
}
PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEPROC glad_debug_glDrawElementsInstancedBaseVertexBaseInstance = glad_debug_impl_glDrawElementsInstancedBaseVertexBaseInstance;
PFNGLDRAWPIXELSPROC glad_glDrawPixels;
void APIENTRY glad_debug_impl_glDrawPixels(GLsizei arg0, GLsizei arg1, GLenum arg2, GLenum arg3, const void * arg4) {    
    _pre_call_callback("glDrawPixels", (void*)glDrawPixels, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glDrawPixels(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glDrawPixels", (void*)glDrawPixels, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLDRAWPIXELSPROC glad_debug_glDrawPixels = glad_debug_impl_glDrawPixels;
PFNGLDRAWRANGEELEMENTSPROC glad_glDrawRangeElements;
void APIENTRY glad_debug_impl_glDrawRangeElements(GLenum arg0, GLuint arg1, GLuint arg2, GLsizei arg3, GLenum arg4, const void * arg5) {    
    _pre_call_callback("glDrawRangeElements", (void*)glDrawRangeElements, 6, arg0, arg1, arg2, arg3, arg4, arg5);
     glad_glDrawRangeElements(arg0, arg1, arg2, arg3, arg4, arg5);
    _post_call_callback("glDrawRangeElements", (void*)glDrawRangeElements, 6, arg0, arg1, arg2, arg3, arg4, arg5);
    
}
PFNGLDRAWRANGEELEMENTSPROC glad_debug_glDrawRangeElements = glad_debug_impl_glDrawRangeElements;
PFNGLDRAWRANGEELEMENTSBASEVERTEXPROC glad_glDrawRangeElementsBaseVertex;
void APIENTRY glad_debug_impl_glDrawRangeElementsBaseVertex(GLenum arg0, GLuint arg1, GLuint arg2, GLsizei arg3, GLenum arg4, const void * arg5, GLint arg6) {    
    _pre_call_callback("glDrawRangeElementsBaseVertex", (void*)glDrawRangeElementsBaseVertex, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
     glad_glDrawRangeElementsBaseVertex(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    _post_call_callback("glDrawRangeElementsBaseVertex", (void*)glDrawRangeElementsBaseVertex, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    
}
PFNGLDRAWRANGEELEMENTSBASEVERTEXPROC glad_debug_glDrawRangeElementsBaseVertex = glad_debug_impl_glDrawRangeElementsBaseVertex;
PFNGLDRAWTRANSFORMFEEDBACKPROC glad_glDrawTransformFeedback;
void APIENTRY glad_debug_impl_glDrawTransformFeedback(GLenum arg0, GLuint arg1) {    
    _pre_call_callback("glDrawTransformFeedback", (void*)glDrawTransformFeedback, 2, arg0, arg1);
     glad_glDrawTransformFeedback(arg0, arg1);
    _post_call_callback("glDrawTransformFeedback", (void*)glDrawTransformFeedback, 2, arg0, arg1);
    
}
PFNGLDRAWTRANSFORMFEEDBACKPROC glad_debug_glDrawTransformFeedback = glad_debug_impl_glDrawTransformFeedback;
PFNGLDRAWTRANSFORMFEEDBACKINSTANCEDPROC glad_glDrawTransformFeedbackInstanced;
void APIENTRY glad_debug_impl_glDrawTransformFeedbackInstanced(GLenum arg0, GLuint arg1, GLsizei arg2) {    
    _pre_call_callback("glDrawTransformFeedbackInstanced", (void*)glDrawTransformFeedbackInstanced, 3, arg0, arg1, arg2);
     glad_glDrawTransformFeedbackInstanced(arg0, arg1, arg2);
    _post_call_callback("glDrawTransformFeedbackInstanced", (void*)glDrawTransformFeedbackInstanced, 3, arg0, arg1, arg2);
    
}
PFNGLDRAWTRANSFORMFEEDBACKINSTANCEDPROC glad_debug_glDrawTransformFeedbackInstanced = glad_debug_impl_glDrawTransformFeedbackInstanced;
PFNGLDRAWTRANSFORMFEEDBACKSTREAMPROC glad_glDrawTransformFeedbackStream;
void APIENTRY glad_debug_impl_glDrawTransformFeedbackStream(GLenum arg0, GLuint arg1, GLuint arg2) {    
    _pre_call_callback("glDrawTransformFeedbackStream", (void*)glDrawTransformFeedbackStream, 3, arg0, arg1, arg2);
     glad_glDrawTransformFeedbackStream(arg0, arg1, arg2);
    _post_call_callback("glDrawTransformFeedbackStream", (void*)glDrawTransformFeedbackStream, 3, arg0, arg1, arg2);
    
}
PFNGLDRAWTRANSFORMFEEDBACKSTREAMPROC glad_debug_glDrawTransformFeedbackStream = glad_debug_impl_glDrawTransformFeedbackStream;
PFNGLDRAWTRANSFORMFEEDBACKSTREAMINSTANCEDPROC glad_glDrawTransformFeedbackStreamInstanced;
void APIENTRY glad_debug_impl_glDrawTransformFeedbackStreamInstanced(GLenum arg0, GLuint arg1, GLuint arg2, GLsizei arg3) {    
    _pre_call_callback("glDrawTransformFeedbackStreamInstanced", (void*)glDrawTransformFeedbackStreamInstanced, 4, arg0, arg1, arg2, arg3);
     glad_glDrawTransformFeedbackStreamInstanced(arg0, arg1, arg2, arg3);
    _post_call_callback("glDrawTransformFeedbackStreamInstanced", (void*)glDrawTransformFeedbackStreamInstanced, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLDRAWTRANSFORMFEEDBACKSTREAMINSTANCEDPROC glad_debug_glDrawTransformFeedbackStreamInstanced = glad_debug_impl_glDrawTransformFeedbackStreamInstanced;
PFNGLEDGEFLAGPROC glad_glEdgeFlag;
void APIENTRY glad_debug_impl_glEdgeFlag(GLboolean arg0) {    
    _pre_call_callback("glEdgeFlag", (void*)glEdgeFlag, 1, arg0);
     glad_glEdgeFlag(arg0);
    _post_call_callback("glEdgeFlag", (void*)glEdgeFlag, 1, arg0);
    
}
PFNGLEDGEFLAGPROC glad_debug_glEdgeFlag = glad_debug_impl_glEdgeFlag;
PFNGLEDGEFLAGPOINTERPROC glad_glEdgeFlagPointer;
void APIENTRY glad_debug_impl_glEdgeFlagPointer(GLsizei arg0, const void * arg1) {    
    _pre_call_callback("glEdgeFlagPointer", (void*)glEdgeFlagPointer, 2, arg0, arg1);
     glad_glEdgeFlagPointer(arg0, arg1);
    _post_call_callback("glEdgeFlagPointer", (void*)glEdgeFlagPointer, 2, arg0, arg1);
    
}
PFNGLEDGEFLAGPOINTERPROC glad_debug_glEdgeFlagPointer = glad_debug_impl_glEdgeFlagPointer;
PFNGLEDGEFLAGVPROC glad_glEdgeFlagv;
void APIENTRY glad_debug_impl_glEdgeFlagv(const GLboolean * arg0) {    
    _pre_call_callback("glEdgeFlagv", (void*)glEdgeFlagv, 1, arg0);
     glad_glEdgeFlagv(arg0);
    _post_call_callback("glEdgeFlagv", (void*)glEdgeFlagv, 1, arg0);
    
}
PFNGLEDGEFLAGVPROC glad_debug_glEdgeFlagv = glad_debug_impl_glEdgeFlagv;
PFNGLENABLEPROC glad_glEnable;
void APIENTRY glad_debug_impl_glEnable(GLenum arg0) {    
    _pre_call_callback("glEnable", (void*)glEnable, 1, arg0);
     glad_glEnable(arg0);
    _post_call_callback("glEnable", (void*)glEnable, 1, arg0);
    
}
PFNGLENABLEPROC glad_debug_glEnable = glad_debug_impl_glEnable;
PFNGLENABLECLIENTSTATEPROC glad_glEnableClientState;
void APIENTRY glad_debug_impl_glEnableClientState(GLenum arg0) {    
    _pre_call_callback("glEnableClientState", (void*)glEnableClientState, 1, arg0);
     glad_glEnableClientState(arg0);
    _post_call_callback("glEnableClientState", (void*)glEnableClientState, 1, arg0);
    
}
PFNGLENABLECLIENTSTATEPROC glad_debug_glEnableClientState = glad_debug_impl_glEnableClientState;
PFNGLENABLEVERTEXARRAYATTRIBPROC glad_glEnableVertexArrayAttrib;
void APIENTRY glad_debug_impl_glEnableVertexArrayAttrib(GLuint arg0, GLuint arg1) {    
    _pre_call_callback("glEnableVertexArrayAttrib", (void*)glEnableVertexArrayAttrib, 2, arg0, arg1);
     glad_glEnableVertexArrayAttrib(arg0, arg1);
    _post_call_callback("glEnableVertexArrayAttrib", (void*)glEnableVertexArrayAttrib, 2, arg0, arg1);
    
}
PFNGLENABLEVERTEXARRAYATTRIBPROC glad_debug_glEnableVertexArrayAttrib = glad_debug_impl_glEnableVertexArrayAttrib;
PFNGLENABLEVERTEXATTRIBARRAYPROC glad_glEnableVertexAttribArray;
void APIENTRY glad_debug_impl_glEnableVertexAttribArray(GLuint arg0) {    
    _pre_call_callback("glEnableVertexAttribArray", (void*)glEnableVertexAttribArray, 1, arg0);
     glad_glEnableVertexAttribArray(arg0);
    _post_call_callback("glEnableVertexAttribArray", (void*)glEnableVertexAttribArray, 1, arg0);
    
}
PFNGLENABLEVERTEXATTRIBARRAYPROC glad_debug_glEnableVertexAttribArray = glad_debug_impl_glEnableVertexAttribArray;
PFNGLENABLEIPROC glad_glEnablei;
void APIENTRY glad_debug_impl_glEnablei(GLenum arg0, GLuint arg1) {    
    _pre_call_callback("glEnablei", (void*)glEnablei, 2, arg0, arg1);
     glad_glEnablei(arg0, arg1);
    _post_call_callback("glEnablei", (void*)glEnablei, 2, arg0, arg1);
    
}
PFNGLENABLEIPROC glad_debug_glEnablei = glad_debug_impl_glEnablei;
PFNGLENDPROC glad_glEnd;
void APIENTRY glad_debug_impl_glEnd(void) {    
    _pre_call_callback("glEnd", (void*)glEnd, 0);
     glad_glEnd();
    _post_call_callback("glEnd", (void*)glEnd, 0);
    
}
PFNGLENDPROC glad_debug_glEnd = glad_debug_impl_glEnd;
PFNGLENDCONDITIONALRENDERPROC glad_glEndConditionalRender;
void APIENTRY glad_debug_impl_glEndConditionalRender(void) {    
    _pre_call_callback("glEndConditionalRender", (void*)glEndConditionalRender, 0);
     glad_glEndConditionalRender();
    _post_call_callback("glEndConditionalRender", (void*)glEndConditionalRender, 0);
    
}
PFNGLENDCONDITIONALRENDERPROC glad_debug_glEndConditionalRender = glad_debug_impl_glEndConditionalRender;
PFNGLENDLISTPROC glad_glEndList;
void APIENTRY glad_debug_impl_glEndList(void) {    
    _pre_call_callback("glEndList", (void*)glEndList, 0);
     glad_glEndList();
    _post_call_callback("glEndList", (void*)glEndList, 0);
    
}
PFNGLENDLISTPROC glad_debug_glEndList = glad_debug_impl_glEndList;
PFNGLENDQUERYPROC glad_glEndQuery;
void APIENTRY glad_debug_impl_glEndQuery(GLenum arg0) {    
    _pre_call_callback("glEndQuery", (void*)glEndQuery, 1, arg0);
     glad_glEndQuery(arg0);
    _post_call_callback("glEndQuery", (void*)glEndQuery, 1, arg0);
    
}
PFNGLENDQUERYPROC glad_debug_glEndQuery = glad_debug_impl_glEndQuery;
PFNGLENDQUERYINDEXEDPROC glad_glEndQueryIndexed;
void APIENTRY glad_debug_impl_glEndQueryIndexed(GLenum arg0, GLuint arg1) {    
    _pre_call_callback("glEndQueryIndexed", (void*)glEndQueryIndexed, 2, arg0, arg1);
     glad_glEndQueryIndexed(arg0, arg1);
    _post_call_callback("glEndQueryIndexed", (void*)glEndQueryIndexed, 2, arg0, arg1);
    
}
PFNGLENDQUERYINDEXEDPROC glad_debug_glEndQueryIndexed = glad_debug_impl_glEndQueryIndexed;
PFNGLENDTRANSFORMFEEDBACKPROC glad_glEndTransformFeedback;
void APIENTRY glad_debug_impl_glEndTransformFeedback(void) {    
    _pre_call_callback("glEndTransformFeedback", (void*)glEndTransformFeedback, 0);
     glad_glEndTransformFeedback();
    _post_call_callback("glEndTransformFeedback", (void*)glEndTransformFeedback, 0);
    
}
PFNGLENDTRANSFORMFEEDBACKPROC glad_debug_glEndTransformFeedback = glad_debug_impl_glEndTransformFeedback;
PFNGLEVALCOORD1DPROC glad_glEvalCoord1d;
void APIENTRY glad_debug_impl_glEvalCoord1d(GLdouble arg0) {    
    _pre_call_callback("glEvalCoord1d", (void*)glEvalCoord1d, 1, arg0);
     glad_glEvalCoord1d(arg0);
    _post_call_callback("glEvalCoord1d", (void*)glEvalCoord1d, 1, arg0);
    
}
PFNGLEVALCOORD1DPROC glad_debug_glEvalCoord1d = glad_debug_impl_glEvalCoord1d;
PFNGLEVALCOORD1DVPROC glad_glEvalCoord1dv;
void APIENTRY glad_debug_impl_glEvalCoord1dv(const GLdouble * arg0) {    
    _pre_call_callback("glEvalCoord1dv", (void*)glEvalCoord1dv, 1, arg0);
     glad_glEvalCoord1dv(arg0);
    _post_call_callback("glEvalCoord1dv", (void*)glEvalCoord1dv, 1, arg0);
    
}
PFNGLEVALCOORD1DVPROC glad_debug_glEvalCoord1dv = glad_debug_impl_glEvalCoord1dv;
PFNGLEVALCOORD1FPROC glad_glEvalCoord1f;
void APIENTRY glad_debug_impl_glEvalCoord1f(GLfloat arg0) {    
    _pre_call_callback("glEvalCoord1f", (void*)glEvalCoord1f, 1, arg0);
     glad_glEvalCoord1f(arg0);
    _post_call_callback("glEvalCoord1f", (void*)glEvalCoord1f, 1, arg0);
    
}
PFNGLEVALCOORD1FPROC glad_debug_glEvalCoord1f = glad_debug_impl_glEvalCoord1f;
PFNGLEVALCOORD1FVPROC glad_glEvalCoord1fv;
void APIENTRY glad_debug_impl_glEvalCoord1fv(const GLfloat * arg0) {    
    _pre_call_callback("glEvalCoord1fv", (void*)glEvalCoord1fv, 1, arg0);
     glad_glEvalCoord1fv(arg0);
    _post_call_callback("glEvalCoord1fv", (void*)glEvalCoord1fv, 1, arg0);
    
}
PFNGLEVALCOORD1FVPROC glad_debug_glEvalCoord1fv = glad_debug_impl_glEvalCoord1fv;
PFNGLEVALCOORD2DPROC glad_glEvalCoord2d;
void APIENTRY glad_debug_impl_glEvalCoord2d(GLdouble arg0, GLdouble arg1) {    
    _pre_call_callback("glEvalCoord2d", (void*)glEvalCoord2d, 2, arg0, arg1);
     glad_glEvalCoord2d(arg0, arg1);
    _post_call_callback("glEvalCoord2d", (void*)glEvalCoord2d, 2, arg0, arg1);
    
}
PFNGLEVALCOORD2DPROC glad_debug_glEvalCoord2d = glad_debug_impl_glEvalCoord2d;
PFNGLEVALCOORD2DVPROC glad_glEvalCoord2dv;
void APIENTRY glad_debug_impl_glEvalCoord2dv(const GLdouble * arg0) {    
    _pre_call_callback("glEvalCoord2dv", (void*)glEvalCoord2dv, 1, arg0);
     glad_glEvalCoord2dv(arg0);
    _post_call_callback("glEvalCoord2dv", (void*)glEvalCoord2dv, 1, arg0);
    
}
PFNGLEVALCOORD2DVPROC glad_debug_glEvalCoord2dv = glad_debug_impl_glEvalCoord2dv;
PFNGLEVALCOORD2FPROC glad_glEvalCoord2f;
void APIENTRY glad_debug_impl_glEvalCoord2f(GLfloat arg0, GLfloat arg1) {    
    _pre_call_callback("glEvalCoord2f", (void*)glEvalCoord2f, 2, arg0, arg1);
     glad_glEvalCoord2f(arg0, arg1);
    _post_call_callback("glEvalCoord2f", (void*)glEvalCoord2f, 2, arg0, arg1);
    
}
PFNGLEVALCOORD2FPROC glad_debug_glEvalCoord2f = glad_debug_impl_glEvalCoord2f;
PFNGLEVALCOORD2FVPROC glad_glEvalCoord2fv;
void APIENTRY glad_debug_impl_glEvalCoord2fv(const GLfloat * arg0) {    
    _pre_call_callback("glEvalCoord2fv", (void*)glEvalCoord2fv, 1, arg0);
     glad_glEvalCoord2fv(arg0);
    _post_call_callback("glEvalCoord2fv", (void*)glEvalCoord2fv, 1, arg0);
    
}
PFNGLEVALCOORD2FVPROC glad_debug_glEvalCoord2fv = glad_debug_impl_glEvalCoord2fv;
PFNGLEVALMESH1PROC glad_glEvalMesh1;
void APIENTRY glad_debug_impl_glEvalMesh1(GLenum arg0, GLint arg1, GLint arg2) {    
    _pre_call_callback("glEvalMesh1", (void*)glEvalMesh1, 3, arg0, arg1, arg2);
     glad_glEvalMesh1(arg0, arg1, arg2);
    _post_call_callback("glEvalMesh1", (void*)glEvalMesh1, 3, arg0, arg1, arg2);
    
}
PFNGLEVALMESH1PROC glad_debug_glEvalMesh1 = glad_debug_impl_glEvalMesh1;
PFNGLEVALMESH2PROC glad_glEvalMesh2;
void APIENTRY glad_debug_impl_glEvalMesh2(GLenum arg0, GLint arg1, GLint arg2, GLint arg3, GLint arg4) {    
    _pre_call_callback("glEvalMesh2", (void*)glEvalMesh2, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glEvalMesh2(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glEvalMesh2", (void*)glEvalMesh2, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLEVALMESH2PROC glad_debug_glEvalMesh2 = glad_debug_impl_glEvalMesh2;
PFNGLEVALPOINT1PROC glad_glEvalPoint1;
void APIENTRY glad_debug_impl_glEvalPoint1(GLint arg0) {    
    _pre_call_callback("glEvalPoint1", (void*)glEvalPoint1, 1, arg0);
     glad_glEvalPoint1(arg0);
    _post_call_callback("glEvalPoint1", (void*)glEvalPoint1, 1, arg0);
    
}
PFNGLEVALPOINT1PROC glad_debug_glEvalPoint1 = glad_debug_impl_glEvalPoint1;
PFNGLEVALPOINT2PROC glad_glEvalPoint2;
void APIENTRY glad_debug_impl_glEvalPoint2(GLint arg0, GLint arg1) {    
    _pre_call_callback("glEvalPoint2", (void*)glEvalPoint2, 2, arg0, arg1);
     glad_glEvalPoint2(arg0, arg1);
    _post_call_callback("glEvalPoint2", (void*)glEvalPoint2, 2, arg0, arg1);
    
}
PFNGLEVALPOINT2PROC glad_debug_glEvalPoint2 = glad_debug_impl_glEvalPoint2;
PFNGLFEEDBACKBUFFERPROC glad_glFeedbackBuffer;
void APIENTRY glad_debug_impl_glFeedbackBuffer(GLsizei arg0, GLenum arg1, GLfloat * arg2) {    
    _pre_call_callback("glFeedbackBuffer", (void*)glFeedbackBuffer, 3, arg0, arg1, arg2);
     glad_glFeedbackBuffer(arg0, arg1, arg2);
    _post_call_callback("glFeedbackBuffer", (void*)glFeedbackBuffer, 3, arg0, arg1, arg2);
    
}
PFNGLFEEDBACKBUFFERPROC glad_debug_glFeedbackBuffer = glad_debug_impl_glFeedbackBuffer;
PFNGLFENCESYNCPROC glad_glFenceSync;
GLsync APIENTRY glad_debug_impl_glFenceSync(GLenum arg0, GLbitfield arg1) {    
    GLsync ret;
    _pre_call_callback("glFenceSync", (void*)glFenceSync, 2, arg0, arg1);
    ret =  glad_glFenceSync(arg0, arg1);
    _post_call_callback("glFenceSync", (void*)glFenceSync, 2, arg0, arg1);
    return ret;
}
PFNGLFENCESYNCPROC glad_debug_glFenceSync = glad_debug_impl_glFenceSync;
PFNGLFINISHPROC glad_glFinish;
void APIENTRY glad_debug_impl_glFinish(void) {    
    _pre_call_callback("glFinish", (void*)glFinish, 0);
     glad_glFinish();
    _post_call_callback("glFinish", (void*)glFinish, 0);
    
}
PFNGLFINISHPROC glad_debug_glFinish = glad_debug_impl_glFinish;
PFNGLFLUSHPROC glad_glFlush;
void APIENTRY glad_debug_impl_glFlush(void) {    
    _pre_call_callback("glFlush", (void*)glFlush, 0);
     glad_glFlush();
    _post_call_callback("glFlush", (void*)glFlush, 0);
    
}
PFNGLFLUSHPROC glad_debug_glFlush = glad_debug_impl_glFlush;
PFNGLFLUSHMAPPEDBUFFERRANGEPROC glad_glFlushMappedBufferRange;
void APIENTRY glad_debug_impl_glFlushMappedBufferRange(GLenum arg0, GLintptr arg1, GLsizeiptr arg2) {    
    _pre_call_callback("glFlushMappedBufferRange", (void*)glFlushMappedBufferRange, 3, arg0, arg1, arg2);
     glad_glFlushMappedBufferRange(arg0, arg1, arg2);
    _post_call_callback("glFlushMappedBufferRange", (void*)glFlushMappedBufferRange, 3, arg0, arg1, arg2);
    
}
PFNGLFLUSHMAPPEDBUFFERRANGEPROC glad_debug_glFlushMappedBufferRange = glad_debug_impl_glFlushMappedBufferRange;
PFNGLFLUSHMAPPEDNAMEDBUFFERRANGEPROC glad_glFlushMappedNamedBufferRange;
void APIENTRY glad_debug_impl_glFlushMappedNamedBufferRange(GLuint arg0, GLintptr arg1, GLsizeiptr arg2) {    
    _pre_call_callback("glFlushMappedNamedBufferRange", (void*)glFlushMappedNamedBufferRange, 3, arg0, arg1, arg2);
     glad_glFlushMappedNamedBufferRange(arg0, arg1, arg2);
    _post_call_callback("glFlushMappedNamedBufferRange", (void*)glFlushMappedNamedBufferRange, 3, arg0, arg1, arg2);
    
}
PFNGLFLUSHMAPPEDNAMEDBUFFERRANGEPROC glad_debug_glFlushMappedNamedBufferRange = glad_debug_impl_glFlushMappedNamedBufferRange;
PFNGLFOGCOORDPOINTERPROC glad_glFogCoordPointer;
void APIENTRY glad_debug_impl_glFogCoordPointer(GLenum arg0, GLsizei arg1, const void * arg2) {    
    _pre_call_callback("glFogCoordPointer", (void*)glFogCoordPointer, 3, arg0, arg1, arg2);
     glad_glFogCoordPointer(arg0, arg1, arg2);
    _post_call_callback("glFogCoordPointer", (void*)glFogCoordPointer, 3, arg0, arg1, arg2);
    
}
PFNGLFOGCOORDPOINTERPROC glad_debug_glFogCoordPointer = glad_debug_impl_glFogCoordPointer;
PFNGLFOGCOORDDPROC glad_glFogCoordd;
void APIENTRY glad_debug_impl_glFogCoordd(GLdouble arg0) {    
    _pre_call_callback("glFogCoordd", (void*)glFogCoordd, 1, arg0);
     glad_glFogCoordd(arg0);
    _post_call_callback("glFogCoordd", (void*)glFogCoordd, 1, arg0);
    
}
PFNGLFOGCOORDDPROC glad_debug_glFogCoordd = glad_debug_impl_glFogCoordd;
PFNGLFOGCOORDDVPROC glad_glFogCoorddv;
void APIENTRY glad_debug_impl_glFogCoorddv(const GLdouble * arg0) {    
    _pre_call_callback("glFogCoorddv", (void*)glFogCoorddv, 1, arg0);
     glad_glFogCoorddv(arg0);
    _post_call_callback("glFogCoorddv", (void*)glFogCoorddv, 1, arg0);
    
}
PFNGLFOGCOORDDVPROC glad_debug_glFogCoorddv = glad_debug_impl_glFogCoorddv;
PFNGLFOGCOORDFPROC glad_glFogCoordf;
void APIENTRY glad_debug_impl_glFogCoordf(GLfloat arg0) {    
    _pre_call_callback("glFogCoordf", (void*)glFogCoordf, 1, arg0);
     glad_glFogCoordf(arg0);
    _post_call_callback("glFogCoordf", (void*)glFogCoordf, 1, arg0);
    
}
PFNGLFOGCOORDFPROC glad_debug_glFogCoordf = glad_debug_impl_glFogCoordf;
PFNGLFOGCOORDFVPROC glad_glFogCoordfv;
void APIENTRY glad_debug_impl_glFogCoordfv(const GLfloat * arg0) {    
    _pre_call_callback("glFogCoordfv", (void*)glFogCoordfv, 1, arg0);
     glad_glFogCoordfv(arg0);
    _post_call_callback("glFogCoordfv", (void*)glFogCoordfv, 1, arg0);
    
}
PFNGLFOGCOORDFVPROC glad_debug_glFogCoordfv = glad_debug_impl_glFogCoordfv;
PFNGLFOGFPROC glad_glFogf;
void APIENTRY glad_debug_impl_glFogf(GLenum arg0, GLfloat arg1) {    
    _pre_call_callback("glFogf", (void*)glFogf, 2, arg0, arg1);
     glad_glFogf(arg0, arg1);
    _post_call_callback("glFogf", (void*)glFogf, 2, arg0, arg1);
    
}
PFNGLFOGFPROC glad_debug_glFogf = glad_debug_impl_glFogf;
PFNGLFOGFVPROC glad_glFogfv;
void APIENTRY glad_debug_impl_glFogfv(GLenum arg0, const GLfloat * arg1) {    
    _pre_call_callback("glFogfv", (void*)glFogfv, 2, arg0, arg1);
     glad_glFogfv(arg0, arg1);
    _post_call_callback("glFogfv", (void*)glFogfv, 2, arg0, arg1);
    
}
PFNGLFOGFVPROC glad_debug_glFogfv = glad_debug_impl_glFogfv;
PFNGLFOGIPROC glad_glFogi;
void APIENTRY glad_debug_impl_glFogi(GLenum arg0, GLint arg1) {    
    _pre_call_callback("glFogi", (void*)glFogi, 2, arg0, arg1);
     glad_glFogi(arg0, arg1);
    _post_call_callback("glFogi", (void*)glFogi, 2, arg0, arg1);
    
}
PFNGLFOGIPROC glad_debug_glFogi = glad_debug_impl_glFogi;
PFNGLFOGIVPROC glad_glFogiv;
void APIENTRY glad_debug_impl_glFogiv(GLenum arg0, const GLint * arg1) {    
    _pre_call_callback("glFogiv", (void*)glFogiv, 2, arg0, arg1);
     glad_glFogiv(arg0, arg1);
    _post_call_callback("glFogiv", (void*)glFogiv, 2, arg0, arg1);
    
}
PFNGLFOGIVPROC glad_debug_glFogiv = glad_debug_impl_glFogiv;
PFNGLFRAMEBUFFERPARAMETERIPROC glad_glFramebufferParameteri;
void APIENTRY glad_debug_impl_glFramebufferParameteri(GLenum arg0, GLenum arg1, GLint arg2) {    
    _pre_call_callback("glFramebufferParameteri", (void*)glFramebufferParameteri, 3, arg0, arg1, arg2);
     glad_glFramebufferParameteri(arg0, arg1, arg2);
    _post_call_callback("glFramebufferParameteri", (void*)glFramebufferParameteri, 3, arg0, arg1, arg2);
    
}
PFNGLFRAMEBUFFERPARAMETERIPROC glad_debug_glFramebufferParameteri = glad_debug_impl_glFramebufferParameteri;
PFNGLFRAMEBUFFERRENDERBUFFERPROC glad_glFramebufferRenderbuffer;
void APIENTRY glad_debug_impl_glFramebufferRenderbuffer(GLenum arg0, GLenum arg1, GLenum arg2, GLuint arg3) {    
    _pre_call_callback("glFramebufferRenderbuffer", (void*)glFramebufferRenderbuffer, 4, arg0, arg1, arg2, arg3);
     glad_glFramebufferRenderbuffer(arg0, arg1, arg2, arg3);
    _post_call_callback("glFramebufferRenderbuffer", (void*)glFramebufferRenderbuffer, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLFRAMEBUFFERRENDERBUFFERPROC glad_debug_glFramebufferRenderbuffer = glad_debug_impl_glFramebufferRenderbuffer;
PFNGLFRAMEBUFFERTEXTUREPROC glad_glFramebufferTexture;
void APIENTRY glad_debug_impl_glFramebufferTexture(GLenum arg0, GLenum arg1, GLuint arg2, GLint arg3) {    
    _pre_call_callback("glFramebufferTexture", (void*)glFramebufferTexture, 4, arg0, arg1, arg2, arg3);
     glad_glFramebufferTexture(arg0, arg1, arg2, arg3);
    _post_call_callback("glFramebufferTexture", (void*)glFramebufferTexture, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLFRAMEBUFFERTEXTUREPROC glad_debug_glFramebufferTexture = glad_debug_impl_glFramebufferTexture;
PFNGLFRAMEBUFFERTEXTURE1DPROC glad_glFramebufferTexture1D;
void APIENTRY glad_debug_impl_glFramebufferTexture1D(GLenum arg0, GLenum arg1, GLenum arg2, GLuint arg3, GLint arg4) {    
    _pre_call_callback("glFramebufferTexture1D", (void*)glFramebufferTexture1D, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glFramebufferTexture1D(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glFramebufferTexture1D", (void*)glFramebufferTexture1D, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLFRAMEBUFFERTEXTURE1DPROC glad_debug_glFramebufferTexture1D = glad_debug_impl_glFramebufferTexture1D;
PFNGLFRAMEBUFFERTEXTURE2DPROC glad_glFramebufferTexture2D;
void APIENTRY glad_debug_impl_glFramebufferTexture2D(GLenum arg0, GLenum arg1, GLenum arg2, GLuint arg3, GLint arg4) {    
    _pre_call_callback("glFramebufferTexture2D", (void*)glFramebufferTexture2D, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glFramebufferTexture2D(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glFramebufferTexture2D", (void*)glFramebufferTexture2D, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLFRAMEBUFFERTEXTURE2DPROC glad_debug_glFramebufferTexture2D = glad_debug_impl_glFramebufferTexture2D;
PFNGLFRAMEBUFFERTEXTURE3DPROC glad_glFramebufferTexture3D;
void APIENTRY glad_debug_impl_glFramebufferTexture3D(GLenum arg0, GLenum arg1, GLenum arg2, GLuint arg3, GLint arg4, GLint arg5) {    
    _pre_call_callback("glFramebufferTexture3D", (void*)glFramebufferTexture3D, 6, arg0, arg1, arg2, arg3, arg4, arg5);
     glad_glFramebufferTexture3D(arg0, arg1, arg2, arg3, arg4, arg5);
    _post_call_callback("glFramebufferTexture3D", (void*)glFramebufferTexture3D, 6, arg0, arg1, arg2, arg3, arg4, arg5);
    
}
PFNGLFRAMEBUFFERTEXTURE3DPROC glad_debug_glFramebufferTexture3D = glad_debug_impl_glFramebufferTexture3D;
PFNGLFRAMEBUFFERTEXTURELAYERPROC glad_glFramebufferTextureLayer;
void APIENTRY glad_debug_impl_glFramebufferTextureLayer(GLenum arg0, GLenum arg1, GLuint arg2, GLint arg3, GLint arg4) {    
    _pre_call_callback("glFramebufferTextureLayer", (void*)glFramebufferTextureLayer, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glFramebufferTextureLayer(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glFramebufferTextureLayer", (void*)glFramebufferTextureLayer, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLFRAMEBUFFERTEXTURELAYERPROC glad_debug_glFramebufferTextureLayer = glad_debug_impl_glFramebufferTextureLayer;
PFNGLFRONTFACEPROC glad_glFrontFace;
void APIENTRY glad_debug_impl_glFrontFace(GLenum arg0) {    
    _pre_call_callback("glFrontFace", (void*)glFrontFace, 1, arg0);
     glad_glFrontFace(arg0);
    _post_call_callback("glFrontFace", (void*)glFrontFace, 1, arg0);
    
}
PFNGLFRONTFACEPROC glad_debug_glFrontFace = glad_debug_impl_glFrontFace;
PFNGLFRUSTUMPROC glad_glFrustum;
void APIENTRY glad_debug_impl_glFrustum(GLdouble arg0, GLdouble arg1, GLdouble arg2, GLdouble arg3, GLdouble arg4, GLdouble arg5) {    
    _pre_call_callback("glFrustum", (void*)glFrustum, 6, arg0, arg1, arg2, arg3, arg4, arg5);
     glad_glFrustum(arg0, arg1, arg2, arg3, arg4, arg5);
    _post_call_callback("glFrustum", (void*)glFrustum, 6, arg0, arg1, arg2, arg3, arg4, arg5);
    
}
PFNGLFRUSTUMPROC glad_debug_glFrustum = glad_debug_impl_glFrustum;
PFNGLGENBUFFERSPROC glad_glGenBuffers;
void APIENTRY glad_debug_impl_glGenBuffers(GLsizei arg0, GLuint * arg1) {    
    _pre_call_callback("glGenBuffers", (void*)glGenBuffers, 2, arg0, arg1);
     glad_glGenBuffers(arg0, arg1);
    _post_call_callback("glGenBuffers", (void*)glGenBuffers, 2, arg0, arg1);
    
}
PFNGLGENBUFFERSPROC glad_debug_glGenBuffers = glad_debug_impl_glGenBuffers;
PFNGLGENFRAMEBUFFERSPROC glad_glGenFramebuffers;
void APIENTRY glad_debug_impl_glGenFramebuffers(GLsizei arg0, GLuint * arg1) {    
    _pre_call_callback("glGenFramebuffers", (void*)glGenFramebuffers, 2, arg0, arg1);
     glad_glGenFramebuffers(arg0, arg1);
    _post_call_callback("glGenFramebuffers", (void*)glGenFramebuffers, 2, arg0, arg1);
    
}
PFNGLGENFRAMEBUFFERSPROC glad_debug_glGenFramebuffers = glad_debug_impl_glGenFramebuffers;
PFNGLGENLISTSPROC glad_glGenLists;
GLuint APIENTRY glad_debug_impl_glGenLists(GLsizei arg0) {    
    GLuint ret;
    _pre_call_callback("glGenLists", (void*)glGenLists, 1, arg0);
    ret =  glad_glGenLists(arg0);
    _post_call_callback("glGenLists", (void*)glGenLists, 1, arg0);
    return ret;
}
PFNGLGENLISTSPROC glad_debug_glGenLists = glad_debug_impl_glGenLists;
PFNGLGENPROGRAMPIPELINESPROC glad_glGenProgramPipelines;
void APIENTRY glad_debug_impl_glGenProgramPipelines(GLsizei arg0, GLuint * arg1) {    
    _pre_call_callback("glGenProgramPipelines", (void*)glGenProgramPipelines, 2, arg0, arg1);
     glad_glGenProgramPipelines(arg0, arg1);
    _post_call_callback("glGenProgramPipelines", (void*)glGenProgramPipelines, 2, arg0, arg1);
    
}
PFNGLGENPROGRAMPIPELINESPROC glad_debug_glGenProgramPipelines = glad_debug_impl_glGenProgramPipelines;
PFNGLGENQUERIESPROC glad_glGenQueries;
void APIENTRY glad_debug_impl_glGenQueries(GLsizei arg0, GLuint * arg1) {    
    _pre_call_callback("glGenQueries", (void*)glGenQueries, 2, arg0, arg1);
     glad_glGenQueries(arg0, arg1);
    _post_call_callback("glGenQueries", (void*)glGenQueries, 2, arg0, arg1);
    
}
PFNGLGENQUERIESPROC glad_debug_glGenQueries = glad_debug_impl_glGenQueries;
PFNGLGENRENDERBUFFERSPROC glad_glGenRenderbuffers;
void APIENTRY glad_debug_impl_glGenRenderbuffers(GLsizei arg0, GLuint * arg1) {    
    _pre_call_callback("glGenRenderbuffers", (void*)glGenRenderbuffers, 2, arg0, arg1);
     glad_glGenRenderbuffers(arg0, arg1);
    _post_call_callback("glGenRenderbuffers", (void*)glGenRenderbuffers, 2, arg0, arg1);
    
}
PFNGLGENRENDERBUFFERSPROC glad_debug_glGenRenderbuffers = glad_debug_impl_glGenRenderbuffers;
PFNGLGENSAMPLERSPROC glad_glGenSamplers;
void APIENTRY glad_debug_impl_glGenSamplers(GLsizei arg0, GLuint * arg1) {    
    _pre_call_callback("glGenSamplers", (void*)glGenSamplers, 2, arg0, arg1);
     glad_glGenSamplers(arg0, arg1);
    _post_call_callback("glGenSamplers", (void*)glGenSamplers, 2, arg0, arg1);
    
}
PFNGLGENSAMPLERSPROC glad_debug_glGenSamplers = glad_debug_impl_glGenSamplers;
PFNGLGENTEXTURESPROC glad_glGenTextures;
void APIENTRY glad_debug_impl_glGenTextures(GLsizei arg0, GLuint * arg1) {    
    _pre_call_callback("glGenTextures", (void*)glGenTextures, 2, arg0, arg1);
     glad_glGenTextures(arg0, arg1);
    _post_call_callback("glGenTextures", (void*)glGenTextures, 2, arg0, arg1);
    
}
PFNGLGENTEXTURESPROC glad_debug_glGenTextures = glad_debug_impl_glGenTextures;
PFNGLGENTRANSFORMFEEDBACKSPROC glad_glGenTransformFeedbacks;
void APIENTRY glad_debug_impl_glGenTransformFeedbacks(GLsizei arg0, GLuint * arg1) {    
    _pre_call_callback("glGenTransformFeedbacks", (void*)glGenTransformFeedbacks, 2, arg0, arg1);
     glad_glGenTransformFeedbacks(arg0, arg1);
    _post_call_callback("glGenTransformFeedbacks", (void*)glGenTransformFeedbacks, 2, arg0, arg1);
    
}
PFNGLGENTRANSFORMFEEDBACKSPROC glad_debug_glGenTransformFeedbacks = glad_debug_impl_glGenTransformFeedbacks;
PFNGLGENVERTEXARRAYSPROC glad_glGenVertexArrays;
void APIENTRY glad_debug_impl_glGenVertexArrays(GLsizei arg0, GLuint * arg1) {    
    _pre_call_callback("glGenVertexArrays", (void*)glGenVertexArrays, 2, arg0, arg1);
     glad_glGenVertexArrays(arg0, arg1);
    _post_call_callback("glGenVertexArrays", (void*)glGenVertexArrays, 2, arg0, arg1);
    
}
PFNGLGENVERTEXARRAYSPROC glad_debug_glGenVertexArrays = glad_debug_impl_glGenVertexArrays;
PFNGLGENERATEMIPMAPPROC glad_glGenerateMipmap;
void APIENTRY glad_debug_impl_glGenerateMipmap(GLenum arg0) {    
    _pre_call_callback("glGenerateMipmap", (void*)glGenerateMipmap, 1, arg0);
     glad_glGenerateMipmap(arg0);
    _post_call_callback("glGenerateMipmap", (void*)glGenerateMipmap, 1, arg0);
    
}
PFNGLGENERATEMIPMAPPROC glad_debug_glGenerateMipmap = glad_debug_impl_glGenerateMipmap;
PFNGLGENERATETEXTUREMIPMAPPROC glad_glGenerateTextureMipmap;
void APIENTRY glad_debug_impl_glGenerateTextureMipmap(GLuint arg0) {    
    _pre_call_callback("glGenerateTextureMipmap", (void*)glGenerateTextureMipmap, 1, arg0);
     glad_glGenerateTextureMipmap(arg0);
    _post_call_callback("glGenerateTextureMipmap", (void*)glGenerateTextureMipmap, 1, arg0);
    
}
PFNGLGENERATETEXTUREMIPMAPPROC glad_debug_glGenerateTextureMipmap = glad_debug_impl_glGenerateTextureMipmap;
PFNGLGETACTIVEATOMICCOUNTERBUFFERIVPROC glad_glGetActiveAtomicCounterBufferiv;
void APIENTRY glad_debug_impl_glGetActiveAtomicCounterBufferiv(GLuint arg0, GLuint arg1, GLenum arg2, GLint * arg3) {    
    _pre_call_callback("glGetActiveAtomicCounterBufferiv", (void*)glGetActiveAtomicCounterBufferiv, 4, arg0, arg1, arg2, arg3);
     glad_glGetActiveAtomicCounterBufferiv(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetActiveAtomicCounterBufferiv", (void*)glGetActiveAtomicCounterBufferiv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETACTIVEATOMICCOUNTERBUFFERIVPROC glad_debug_glGetActiveAtomicCounterBufferiv = glad_debug_impl_glGetActiveAtomicCounterBufferiv;
PFNGLGETACTIVEATTRIBPROC glad_glGetActiveAttrib;
void APIENTRY glad_debug_impl_glGetActiveAttrib(GLuint arg0, GLuint arg1, GLsizei arg2, GLsizei * arg3, GLint * arg4, GLenum * arg5, GLchar * arg6) {    
    _pre_call_callback("glGetActiveAttrib", (void*)glGetActiveAttrib, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
     glad_glGetActiveAttrib(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    _post_call_callback("glGetActiveAttrib", (void*)glGetActiveAttrib, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    
}
PFNGLGETACTIVEATTRIBPROC glad_debug_glGetActiveAttrib = glad_debug_impl_glGetActiveAttrib;
PFNGLGETACTIVESUBROUTINENAMEPROC glad_glGetActiveSubroutineName;
void APIENTRY glad_debug_impl_glGetActiveSubroutineName(GLuint arg0, GLenum arg1, GLuint arg2, GLsizei arg3, GLsizei * arg4, GLchar * arg5) {    
    _pre_call_callback("glGetActiveSubroutineName", (void*)glGetActiveSubroutineName, 6, arg0, arg1, arg2, arg3, arg4, arg5);
     glad_glGetActiveSubroutineName(arg0, arg1, arg2, arg3, arg4, arg5);
    _post_call_callback("glGetActiveSubroutineName", (void*)glGetActiveSubroutineName, 6, arg0, arg1, arg2, arg3, arg4, arg5);
    
}
PFNGLGETACTIVESUBROUTINENAMEPROC glad_debug_glGetActiveSubroutineName = glad_debug_impl_glGetActiveSubroutineName;
PFNGLGETACTIVESUBROUTINEUNIFORMNAMEPROC glad_glGetActiveSubroutineUniformName;
void APIENTRY glad_debug_impl_glGetActiveSubroutineUniformName(GLuint arg0, GLenum arg1, GLuint arg2, GLsizei arg3, GLsizei * arg4, GLchar * arg5) {    
    _pre_call_callback("glGetActiveSubroutineUniformName", (void*)glGetActiveSubroutineUniformName, 6, arg0, arg1, arg2, arg3, arg4, arg5);
     glad_glGetActiveSubroutineUniformName(arg0, arg1, arg2, arg3, arg4, arg5);
    _post_call_callback("glGetActiveSubroutineUniformName", (void*)glGetActiveSubroutineUniformName, 6, arg0, arg1, arg2, arg3, arg4, arg5);
    
}
PFNGLGETACTIVESUBROUTINEUNIFORMNAMEPROC glad_debug_glGetActiveSubroutineUniformName = glad_debug_impl_glGetActiveSubroutineUniformName;
PFNGLGETACTIVESUBROUTINEUNIFORMIVPROC glad_glGetActiveSubroutineUniformiv;
void APIENTRY glad_debug_impl_glGetActiveSubroutineUniformiv(GLuint arg0, GLenum arg1, GLuint arg2, GLenum arg3, GLint * arg4) {    
    _pre_call_callback("glGetActiveSubroutineUniformiv", (void*)glGetActiveSubroutineUniformiv, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glGetActiveSubroutineUniformiv(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glGetActiveSubroutineUniformiv", (void*)glGetActiveSubroutineUniformiv, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLGETACTIVESUBROUTINEUNIFORMIVPROC glad_debug_glGetActiveSubroutineUniformiv = glad_debug_impl_glGetActiveSubroutineUniformiv;
PFNGLGETACTIVEUNIFORMPROC glad_glGetActiveUniform;
void APIENTRY glad_debug_impl_glGetActiveUniform(GLuint arg0, GLuint arg1, GLsizei arg2, GLsizei * arg3, GLint * arg4, GLenum * arg5, GLchar * arg6) {    
    _pre_call_callback("glGetActiveUniform", (void*)glGetActiveUniform, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
     glad_glGetActiveUniform(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    _post_call_callback("glGetActiveUniform", (void*)glGetActiveUniform, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    
}
PFNGLGETACTIVEUNIFORMPROC glad_debug_glGetActiveUniform = glad_debug_impl_glGetActiveUniform;
PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC glad_glGetActiveUniformBlockName;
void APIENTRY glad_debug_impl_glGetActiveUniformBlockName(GLuint arg0, GLuint arg1, GLsizei arg2, GLsizei * arg3, GLchar * arg4) {    
    _pre_call_callback("glGetActiveUniformBlockName", (void*)glGetActiveUniformBlockName, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glGetActiveUniformBlockName(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glGetActiveUniformBlockName", (void*)glGetActiveUniformBlockName, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC glad_debug_glGetActiveUniformBlockName = glad_debug_impl_glGetActiveUniformBlockName;
PFNGLGETACTIVEUNIFORMBLOCKIVPROC glad_glGetActiveUniformBlockiv;
void APIENTRY glad_debug_impl_glGetActiveUniformBlockiv(GLuint arg0, GLuint arg1, GLenum arg2, GLint * arg3) {    
    _pre_call_callback("glGetActiveUniformBlockiv", (void*)glGetActiveUniformBlockiv, 4, arg0, arg1, arg2, arg3);
     glad_glGetActiveUniformBlockiv(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetActiveUniformBlockiv", (void*)glGetActiveUniformBlockiv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETACTIVEUNIFORMBLOCKIVPROC glad_debug_glGetActiveUniformBlockiv = glad_debug_impl_glGetActiveUniformBlockiv;
PFNGLGETACTIVEUNIFORMNAMEPROC glad_glGetActiveUniformName;
void APIENTRY glad_debug_impl_glGetActiveUniformName(GLuint arg0, GLuint arg1, GLsizei arg2, GLsizei * arg3, GLchar * arg4) {    
    _pre_call_callback("glGetActiveUniformName", (void*)glGetActiveUniformName, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glGetActiveUniformName(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glGetActiveUniformName", (void*)glGetActiveUniformName, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLGETACTIVEUNIFORMNAMEPROC glad_debug_glGetActiveUniformName = glad_debug_impl_glGetActiveUniformName;
PFNGLGETACTIVEUNIFORMSIVPROC glad_glGetActiveUniformsiv;
void APIENTRY glad_debug_impl_glGetActiveUniformsiv(GLuint arg0, GLsizei arg1, const GLuint * arg2, GLenum arg3, GLint * arg4) {    
    _pre_call_callback("glGetActiveUniformsiv", (void*)glGetActiveUniformsiv, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glGetActiveUniformsiv(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glGetActiveUniformsiv", (void*)glGetActiveUniformsiv, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLGETACTIVEUNIFORMSIVPROC glad_debug_glGetActiveUniformsiv = glad_debug_impl_glGetActiveUniformsiv;
PFNGLGETATTACHEDSHADERSPROC glad_glGetAttachedShaders;
void APIENTRY glad_debug_impl_glGetAttachedShaders(GLuint arg0, GLsizei arg1, GLsizei * arg2, GLuint * arg3) {    
    _pre_call_callback("glGetAttachedShaders", (void*)glGetAttachedShaders, 4, arg0, arg1, arg2, arg3);
     glad_glGetAttachedShaders(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetAttachedShaders", (void*)glGetAttachedShaders, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETATTACHEDSHADERSPROC glad_debug_glGetAttachedShaders = glad_debug_impl_glGetAttachedShaders;
PFNGLGETATTRIBLOCATIONPROC glad_glGetAttribLocation;
GLint APIENTRY glad_debug_impl_glGetAttribLocation(GLuint arg0, const GLchar * arg1) {    
    GLint ret;
    _pre_call_callback("glGetAttribLocation", (void*)glGetAttribLocation, 2, arg0, arg1);
    ret =  glad_glGetAttribLocation(arg0, arg1);
    _post_call_callback("glGetAttribLocation", (void*)glGetAttribLocation, 2, arg0, arg1);
    return ret;
}
PFNGLGETATTRIBLOCATIONPROC glad_debug_glGetAttribLocation = glad_debug_impl_glGetAttribLocation;
PFNGLGETBOOLEANI_VPROC glad_glGetBooleani_v;
void APIENTRY glad_debug_impl_glGetBooleani_v(GLenum arg0, GLuint arg1, GLboolean * arg2) {    
    _pre_call_callback("glGetBooleani_v", (void*)glGetBooleani_v, 3, arg0, arg1, arg2);
     glad_glGetBooleani_v(arg0, arg1, arg2);
    _post_call_callback("glGetBooleani_v", (void*)glGetBooleani_v, 3, arg0, arg1, arg2);
    
}
PFNGLGETBOOLEANI_VPROC glad_debug_glGetBooleani_v = glad_debug_impl_glGetBooleani_v;
PFNGLGETBOOLEANVPROC glad_glGetBooleanv;
void APIENTRY glad_debug_impl_glGetBooleanv(GLenum arg0, GLboolean * arg1) {    
    _pre_call_callback("glGetBooleanv", (void*)glGetBooleanv, 2, arg0, arg1);
     glad_glGetBooleanv(arg0, arg1);
    _post_call_callback("glGetBooleanv", (void*)glGetBooleanv, 2, arg0, arg1);
    
}
PFNGLGETBOOLEANVPROC glad_debug_glGetBooleanv = glad_debug_impl_glGetBooleanv;
PFNGLGETBUFFERPARAMETERI64VPROC glad_glGetBufferParameteri64v;
void APIENTRY glad_debug_impl_glGetBufferParameteri64v(GLenum arg0, GLenum arg1, GLint64 * arg2) {    
    _pre_call_callback("glGetBufferParameteri64v", (void*)glGetBufferParameteri64v, 3, arg0, arg1, arg2);
     glad_glGetBufferParameteri64v(arg0, arg1, arg2);
    _post_call_callback("glGetBufferParameteri64v", (void*)glGetBufferParameteri64v, 3, arg0, arg1, arg2);
    
}
PFNGLGETBUFFERPARAMETERI64VPROC glad_debug_glGetBufferParameteri64v = glad_debug_impl_glGetBufferParameteri64v;
PFNGLGETBUFFERPARAMETERIVPROC glad_glGetBufferParameteriv;
void APIENTRY glad_debug_impl_glGetBufferParameteriv(GLenum arg0, GLenum arg1, GLint * arg2) {    
    _pre_call_callback("glGetBufferParameteriv", (void*)glGetBufferParameteriv, 3, arg0, arg1, arg2);
     glad_glGetBufferParameteriv(arg0, arg1, arg2);
    _post_call_callback("glGetBufferParameteriv", (void*)glGetBufferParameteriv, 3, arg0, arg1, arg2);
    
}
PFNGLGETBUFFERPARAMETERIVPROC glad_debug_glGetBufferParameteriv = glad_debug_impl_glGetBufferParameteriv;
PFNGLGETBUFFERPOINTERVPROC glad_glGetBufferPointerv;
void APIENTRY glad_debug_impl_glGetBufferPointerv(GLenum arg0, GLenum arg1, void ** arg2) {    
    _pre_call_callback("glGetBufferPointerv", (void*)glGetBufferPointerv, 3, arg0, arg1, arg2);
     glad_glGetBufferPointerv(arg0, arg1, arg2);
    _post_call_callback("glGetBufferPointerv", (void*)glGetBufferPointerv, 3, arg0, arg1, arg2);
    
}
PFNGLGETBUFFERPOINTERVPROC glad_debug_glGetBufferPointerv = glad_debug_impl_glGetBufferPointerv;
PFNGLGETBUFFERSUBDATAPROC glad_glGetBufferSubData;
void APIENTRY glad_debug_impl_glGetBufferSubData(GLenum arg0, GLintptr arg1, GLsizeiptr arg2, void * arg3) {    
    _pre_call_callback("glGetBufferSubData", (void*)glGetBufferSubData, 4, arg0, arg1, arg2, arg3);
     glad_glGetBufferSubData(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetBufferSubData", (void*)glGetBufferSubData, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETBUFFERSUBDATAPROC glad_debug_glGetBufferSubData = glad_debug_impl_glGetBufferSubData;
PFNGLGETCLIPPLANEPROC glad_glGetClipPlane;
void APIENTRY glad_debug_impl_glGetClipPlane(GLenum arg0, GLdouble * arg1) {    
    _pre_call_callback("glGetClipPlane", (void*)glGetClipPlane, 2, arg0, arg1);
     glad_glGetClipPlane(arg0, arg1);
    _post_call_callback("glGetClipPlane", (void*)glGetClipPlane, 2, arg0, arg1);
    
}
PFNGLGETCLIPPLANEPROC glad_debug_glGetClipPlane = glad_debug_impl_glGetClipPlane;
PFNGLGETCOMPRESSEDTEXIMAGEPROC glad_glGetCompressedTexImage;
void APIENTRY glad_debug_impl_glGetCompressedTexImage(GLenum arg0, GLint arg1, void * arg2) {    
    _pre_call_callback("glGetCompressedTexImage", (void*)glGetCompressedTexImage, 3, arg0, arg1, arg2);
     glad_glGetCompressedTexImage(arg0, arg1, arg2);
    _post_call_callback("glGetCompressedTexImage", (void*)glGetCompressedTexImage, 3, arg0, arg1, arg2);
    
}
PFNGLGETCOMPRESSEDTEXIMAGEPROC glad_debug_glGetCompressedTexImage = glad_debug_impl_glGetCompressedTexImage;
PFNGLGETCOMPRESSEDTEXTUREIMAGEPROC glad_glGetCompressedTextureImage;
void APIENTRY glad_debug_impl_glGetCompressedTextureImage(GLuint arg0, GLint arg1, GLsizei arg2, void * arg3) {    
    _pre_call_callback("glGetCompressedTextureImage", (void*)glGetCompressedTextureImage, 4, arg0, arg1, arg2, arg3);
     glad_glGetCompressedTextureImage(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetCompressedTextureImage", (void*)glGetCompressedTextureImage, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETCOMPRESSEDTEXTUREIMAGEPROC glad_debug_glGetCompressedTextureImage = glad_debug_impl_glGetCompressedTextureImage;
PFNGLGETCOMPRESSEDTEXTURESUBIMAGEPROC glad_glGetCompressedTextureSubImage;
void APIENTRY glad_debug_impl_glGetCompressedTextureSubImage(GLuint arg0, GLint arg1, GLint arg2, GLint arg3, GLint arg4, GLsizei arg5, GLsizei arg6, GLsizei arg7, GLsizei arg8, void * arg9) {    
    _pre_call_callback("glGetCompressedTextureSubImage", (void*)glGetCompressedTextureSubImage, 10, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
     glad_glGetCompressedTextureSubImage(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
    _post_call_callback("glGetCompressedTextureSubImage", (void*)glGetCompressedTextureSubImage, 10, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
    
}
PFNGLGETCOMPRESSEDTEXTURESUBIMAGEPROC glad_debug_glGetCompressedTextureSubImage = glad_debug_impl_glGetCompressedTextureSubImage;
PFNGLGETDEBUGMESSAGELOGPROC glad_glGetDebugMessageLog;
GLuint APIENTRY glad_debug_impl_glGetDebugMessageLog(GLuint arg0, GLsizei arg1, GLenum * arg2, GLenum * arg3, GLuint * arg4, GLenum * arg5, GLsizei * arg6, GLchar * arg7) {    
    GLuint ret;
    _pre_call_callback("glGetDebugMessageLog", (void*)glGetDebugMessageLog, 8, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    ret =  glad_glGetDebugMessageLog(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    _post_call_callback("glGetDebugMessageLog", (void*)glGetDebugMessageLog, 8, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    return ret;
}
PFNGLGETDEBUGMESSAGELOGPROC glad_debug_glGetDebugMessageLog = glad_debug_impl_glGetDebugMessageLog;
PFNGLGETDOUBLEI_VPROC glad_glGetDoublei_v;
void APIENTRY glad_debug_impl_glGetDoublei_v(GLenum arg0, GLuint arg1, GLdouble * arg2) {    
    _pre_call_callback("glGetDoublei_v", (void*)glGetDoublei_v, 3, arg0, arg1, arg2);
     glad_glGetDoublei_v(arg0, arg1, arg2);
    _post_call_callback("glGetDoublei_v", (void*)glGetDoublei_v, 3, arg0, arg1, arg2);
    
}
PFNGLGETDOUBLEI_VPROC glad_debug_glGetDoublei_v = glad_debug_impl_glGetDoublei_v;
PFNGLGETDOUBLEVPROC glad_glGetDoublev;
void APIENTRY glad_debug_impl_glGetDoublev(GLenum arg0, GLdouble * arg1) {    
    _pre_call_callback("glGetDoublev", (void*)glGetDoublev, 2, arg0, arg1);
     glad_glGetDoublev(arg0, arg1);
    _post_call_callback("glGetDoublev", (void*)glGetDoublev, 2, arg0, arg1);
    
}
PFNGLGETDOUBLEVPROC glad_debug_glGetDoublev = glad_debug_impl_glGetDoublev;
PFNGLGETERRORPROC glad_glGetError;
GLenum APIENTRY glad_debug_impl_glGetError(void) {    
    GLenum ret;
    _pre_call_callback("glGetError", (void*)glGetError, 0);
    ret =  glad_glGetError();
    _post_call_callback("glGetError", (void*)glGetError, 0);
    return ret;
}
PFNGLGETERRORPROC glad_debug_glGetError = glad_debug_impl_glGetError;
PFNGLGETFLOATI_VPROC glad_glGetFloati_v;
void APIENTRY glad_debug_impl_glGetFloati_v(GLenum arg0, GLuint arg1, GLfloat * arg2) {    
    _pre_call_callback("glGetFloati_v", (void*)glGetFloati_v, 3, arg0, arg1, arg2);
     glad_glGetFloati_v(arg0, arg1, arg2);
    _post_call_callback("glGetFloati_v", (void*)glGetFloati_v, 3, arg0, arg1, arg2);
    
}
PFNGLGETFLOATI_VPROC glad_debug_glGetFloati_v = glad_debug_impl_glGetFloati_v;
PFNGLGETFLOATVPROC glad_glGetFloatv;
void APIENTRY glad_debug_impl_glGetFloatv(GLenum arg0, GLfloat * arg1) {    
    _pre_call_callback("glGetFloatv", (void*)glGetFloatv, 2, arg0, arg1);
     glad_glGetFloatv(arg0, arg1);
    _post_call_callback("glGetFloatv", (void*)glGetFloatv, 2, arg0, arg1);
    
}
PFNGLGETFLOATVPROC glad_debug_glGetFloatv = glad_debug_impl_glGetFloatv;
PFNGLGETFRAGDATAINDEXPROC glad_glGetFragDataIndex;
GLint APIENTRY glad_debug_impl_glGetFragDataIndex(GLuint arg0, const GLchar * arg1) {    
    GLint ret;
    _pre_call_callback("glGetFragDataIndex", (void*)glGetFragDataIndex, 2, arg0, arg1);
    ret =  glad_glGetFragDataIndex(arg0, arg1);
    _post_call_callback("glGetFragDataIndex", (void*)glGetFragDataIndex, 2, arg0, arg1);
    return ret;
}
PFNGLGETFRAGDATAINDEXPROC glad_debug_glGetFragDataIndex = glad_debug_impl_glGetFragDataIndex;
PFNGLGETFRAGDATALOCATIONPROC glad_glGetFragDataLocation;
GLint APIENTRY glad_debug_impl_glGetFragDataLocation(GLuint arg0, const GLchar * arg1) {    
    GLint ret;
    _pre_call_callback("glGetFragDataLocation", (void*)glGetFragDataLocation, 2, arg0, arg1);
    ret =  glad_glGetFragDataLocation(arg0, arg1);
    _post_call_callback("glGetFragDataLocation", (void*)glGetFragDataLocation, 2, arg0, arg1);
    return ret;
}
PFNGLGETFRAGDATALOCATIONPROC glad_debug_glGetFragDataLocation = glad_debug_impl_glGetFragDataLocation;
PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC glad_glGetFramebufferAttachmentParameteriv;
void APIENTRY glad_debug_impl_glGetFramebufferAttachmentParameteriv(GLenum arg0, GLenum arg1, GLenum arg2, GLint * arg3) {    
    _pre_call_callback("glGetFramebufferAttachmentParameteriv", (void*)glGetFramebufferAttachmentParameteriv, 4, arg0, arg1, arg2, arg3);
     glad_glGetFramebufferAttachmentParameteriv(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetFramebufferAttachmentParameteriv", (void*)glGetFramebufferAttachmentParameteriv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC glad_debug_glGetFramebufferAttachmentParameteriv = glad_debug_impl_glGetFramebufferAttachmentParameteriv;
PFNGLGETFRAMEBUFFERPARAMETERIVPROC glad_glGetFramebufferParameteriv;
void APIENTRY glad_debug_impl_glGetFramebufferParameteriv(GLenum arg0, GLenum arg1, GLint * arg2) {    
    _pre_call_callback("glGetFramebufferParameteriv", (void*)glGetFramebufferParameteriv, 3, arg0, arg1, arg2);
     glad_glGetFramebufferParameteriv(arg0, arg1, arg2);
    _post_call_callback("glGetFramebufferParameteriv", (void*)glGetFramebufferParameteriv, 3, arg0, arg1, arg2);
    
}
PFNGLGETFRAMEBUFFERPARAMETERIVPROC glad_debug_glGetFramebufferParameteriv = glad_debug_impl_glGetFramebufferParameteriv;
PFNGLGETGRAPHICSRESETSTATUSPROC glad_glGetGraphicsResetStatus;
GLenum APIENTRY glad_debug_impl_glGetGraphicsResetStatus(void) {    
    GLenum ret;
    _pre_call_callback("glGetGraphicsResetStatus", (void*)glGetGraphicsResetStatus, 0);
    ret =  glad_glGetGraphicsResetStatus();
    _post_call_callback("glGetGraphicsResetStatus", (void*)glGetGraphicsResetStatus, 0);
    return ret;
}
PFNGLGETGRAPHICSRESETSTATUSPROC glad_debug_glGetGraphicsResetStatus = glad_debug_impl_glGetGraphicsResetStatus;
PFNGLGETINTEGER64I_VPROC glad_glGetInteger64i_v;
void APIENTRY glad_debug_impl_glGetInteger64i_v(GLenum arg0, GLuint arg1, GLint64 * arg2) {    
    _pre_call_callback("glGetInteger64i_v", (void*)glGetInteger64i_v, 3, arg0, arg1, arg2);
     glad_glGetInteger64i_v(arg0, arg1, arg2);
    _post_call_callback("glGetInteger64i_v", (void*)glGetInteger64i_v, 3, arg0, arg1, arg2);
    
}
PFNGLGETINTEGER64I_VPROC glad_debug_glGetInteger64i_v = glad_debug_impl_glGetInteger64i_v;
PFNGLGETINTEGER64VPROC glad_glGetInteger64v;
void APIENTRY glad_debug_impl_glGetInteger64v(GLenum arg0, GLint64 * arg1) {    
    _pre_call_callback("glGetInteger64v", (void*)glGetInteger64v, 2, arg0, arg1);
     glad_glGetInteger64v(arg0, arg1);
    _post_call_callback("glGetInteger64v", (void*)glGetInteger64v, 2, arg0, arg1);
    
}
PFNGLGETINTEGER64VPROC glad_debug_glGetInteger64v = glad_debug_impl_glGetInteger64v;
PFNGLGETINTEGERI_VPROC glad_glGetIntegeri_v;
void APIENTRY glad_debug_impl_glGetIntegeri_v(GLenum arg0, GLuint arg1, GLint * arg2) {    
    _pre_call_callback("glGetIntegeri_v", (void*)glGetIntegeri_v, 3, arg0, arg1, arg2);
     glad_glGetIntegeri_v(arg0, arg1, arg2);
    _post_call_callback("glGetIntegeri_v", (void*)glGetIntegeri_v, 3, arg0, arg1, arg2);
    
}
PFNGLGETINTEGERI_VPROC glad_debug_glGetIntegeri_v = glad_debug_impl_glGetIntegeri_v;
PFNGLGETINTEGERVPROC glad_glGetIntegerv;
void APIENTRY glad_debug_impl_glGetIntegerv(GLenum arg0, GLint * arg1) {    
    _pre_call_callback("glGetIntegerv", (void*)glGetIntegerv, 2, arg0, arg1);
     glad_glGetIntegerv(arg0, arg1);
    _post_call_callback("glGetIntegerv", (void*)glGetIntegerv, 2, arg0, arg1);
    
}
PFNGLGETINTEGERVPROC glad_debug_glGetIntegerv = glad_debug_impl_glGetIntegerv;
PFNGLGETINTERNALFORMATI64VPROC glad_glGetInternalformati64v;
void APIENTRY glad_debug_impl_glGetInternalformati64v(GLenum arg0, GLenum arg1, GLenum arg2, GLsizei arg3, GLint64 * arg4) {    
    _pre_call_callback("glGetInternalformati64v", (void*)glGetInternalformati64v, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glGetInternalformati64v(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glGetInternalformati64v", (void*)glGetInternalformati64v, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLGETINTERNALFORMATI64VPROC glad_debug_glGetInternalformati64v = glad_debug_impl_glGetInternalformati64v;
PFNGLGETINTERNALFORMATIVPROC glad_glGetInternalformativ;
void APIENTRY glad_debug_impl_glGetInternalformativ(GLenum arg0, GLenum arg1, GLenum arg2, GLsizei arg3, GLint * arg4) {    
    _pre_call_callback("glGetInternalformativ", (void*)glGetInternalformativ, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glGetInternalformativ(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glGetInternalformativ", (void*)glGetInternalformativ, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLGETINTERNALFORMATIVPROC glad_debug_glGetInternalformativ = glad_debug_impl_glGetInternalformativ;
PFNGLGETLIGHTFVPROC glad_glGetLightfv;
void APIENTRY glad_debug_impl_glGetLightfv(GLenum arg0, GLenum arg1, GLfloat * arg2) {    
    _pre_call_callback("glGetLightfv", (void*)glGetLightfv, 3, arg0, arg1, arg2);
     glad_glGetLightfv(arg0, arg1, arg2);
    _post_call_callback("glGetLightfv", (void*)glGetLightfv, 3, arg0, arg1, arg2);
    
}
PFNGLGETLIGHTFVPROC glad_debug_glGetLightfv = glad_debug_impl_glGetLightfv;
PFNGLGETLIGHTIVPROC glad_glGetLightiv;
void APIENTRY glad_debug_impl_glGetLightiv(GLenum arg0, GLenum arg1, GLint * arg2) {    
    _pre_call_callback("glGetLightiv", (void*)glGetLightiv, 3, arg0, arg1, arg2);
     glad_glGetLightiv(arg0, arg1, arg2);
    _post_call_callback("glGetLightiv", (void*)glGetLightiv, 3, arg0, arg1, arg2);
    
}
PFNGLGETLIGHTIVPROC glad_debug_glGetLightiv = glad_debug_impl_glGetLightiv;
PFNGLGETMAPDVPROC glad_glGetMapdv;
void APIENTRY glad_debug_impl_glGetMapdv(GLenum arg0, GLenum arg1, GLdouble * arg2) {    
    _pre_call_callback("glGetMapdv", (void*)glGetMapdv, 3, arg0, arg1, arg2);
     glad_glGetMapdv(arg0, arg1, arg2);
    _post_call_callback("glGetMapdv", (void*)glGetMapdv, 3, arg0, arg1, arg2);
    
}
PFNGLGETMAPDVPROC glad_debug_glGetMapdv = glad_debug_impl_glGetMapdv;
PFNGLGETMAPFVPROC glad_glGetMapfv;
void APIENTRY glad_debug_impl_glGetMapfv(GLenum arg0, GLenum arg1, GLfloat * arg2) {    
    _pre_call_callback("glGetMapfv", (void*)glGetMapfv, 3, arg0, arg1, arg2);
     glad_glGetMapfv(arg0, arg1, arg2);
    _post_call_callback("glGetMapfv", (void*)glGetMapfv, 3, arg0, arg1, arg2);
    
}
PFNGLGETMAPFVPROC glad_debug_glGetMapfv = glad_debug_impl_glGetMapfv;
PFNGLGETMAPIVPROC glad_glGetMapiv;
void APIENTRY glad_debug_impl_glGetMapiv(GLenum arg0, GLenum arg1, GLint * arg2) {    
    _pre_call_callback("glGetMapiv", (void*)glGetMapiv, 3, arg0, arg1, arg2);
     glad_glGetMapiv(arg0, arg1, arg2);
    _post_call_callback("glGetMapiv", (void*)glGetMapiv, 3, arg0, arg1, arg2);
    
}
PFNGLGETMAPIVPROC glad_debug_glGetMapiv = glad_debug_impl_glGetMapiv;
PFNGLGETMATERIALFVPROC glad_glGetMaterialfv;
void APIENTRY glad_debug_impl_glGetMaterialfv(GLenum arg0, GLenum arg1, GLfloat * arg2) {    
    _pre_call_callback("glGetMaterialfv", (void*)glGetMaterialfv, 3, arg0, arg1, arg2);
     glad_glGetMaterialfv(arg0, arg1, arg2);
    _post_call_callback("glGetMaterialfv", (void*)glGetMaterialfv, 3, arg0, arg1, arg2);
    
}
PFNGLGETMATERIALFVPROC glad_debug_glGetMaterialfv = glad_debug_impl_glGetMaterialfv;
PFNGLGETMATERIALIVPROC glad_glGetMaterialiv;
void APIENTRY glad_debug_impl_glGetMaterialiv(GLenum arg0, GLenum arg1, GLint * arg2) {    
    _pre_call_callback("glGetMaterialiv", (void*)glGetMaterialiv, 3, arg0, arg1, arg2);
     glad_glGetMaterialiv(arg0, arg1, arg2);
    _post_call_callback("glGetMaterialiv", (void*)glGetMaterialiv, 3, arg0, arg1, arg2);
    
}
PFNGLGETMATERIALIVPROC glad_debug_glGetMaterialiv = glad_debug_impl_glGetMaterialiv;
PFNGLGETMULTISAMPLEFVPROC glad_glGetMultisamplefv;
void APIENTRY glad_debug_impl_glGetMultisamplefv(GLenum arg0, GLuint arg1, GLfloat * arg2) {    
    _pre_call_callback("glGetMultisamplefv", (void*)glGetMultisamplefv, 3, arg0, arg1, arg2);
     glad_glGetMultisamplefv(arg0, arg1, arg2);
    _post_call_callback("glGetMultisamplefv", (void*)glGetMultisamplefv, 3, arg0, arg1, arg2);
    
}
PFNGLGETMULTISAMPLEFVPROC glad_debug_glGetMultisamplefv = glad_debug_impl_glGetMultisamplefv;
PFNGLGETNAMEDBUFFERPARAMETERI64VPROC glad_glGetNamedBufferParameteri64v;
void APIENTRY glad_debug_impl_glGetNamedBufferParameteri64v(GLuint arg0, GLenum arg1, GLint64 * arg2) {    
    _pre_call_callback("glGetNamedBufferParameteri64v", (void*)glGetNamedBufferParameteri64v, 3, arg0, arg1, arg2);
     glad_glGetNamedBufferParameteri64v(arg0, arg1, arg2);
    _post_call_callback("glGetNamedBufferParameteri64v", (void*)glGetNamedBufferParameteri64v, 3, arg0, arg1, arg2);
    
}
PFNGLGETNAMEDBUFFERPARAMETERI64VPROC glad_debug_glGetNamedBufferParameteri64v = glad_debug_impl_glGetNamedBufferParameteri64v;
PFNGLGETNAMEDBUFFERPARAMETERIVPROC glad_glGetNamedBufferParameteriv;
void APIENTRY glad_debug_impl_glGetNamedBufferParameteriv(GLuint arg0, GLenum arg1, GLint * arg2) {    
    _pre_call_callback("glGetNamedBufferParameteriv", (void*)glGetNamedBufferParameteriv, 3, arg0, arg1, arg2);
     glad_glGetNamedBufferParameteriv(arg0, arg1, arg2);
    _post_call_callback("glGetNamedBufferParameteriv", (void*)glGetNamedBufferParameteriv, 3, arg0, arg1, arg2);
    
}
PFNGLGETNAMEDBUFFERPARAMETERIVPROC glad_debug_glGetNamedBufferParameteriv = glad_debug_impl_glGetNamedBufferParameteriv;
PFNGLGETNAMEDBUFFERPOINTERVPROC glad_glGetNamedBufferPointerv;
void APIENTRY glad_debug_impl_glGetNamedBufferPointerv(GLuint arg0, GLenum arg1, void ** arg2) {    
    _pre_call_callback("glGetNamedBufferPointerv", (void*)glGetNamedBufferPointerv, 3, arg0, arg1, arg2);
     glad_glGetNamedBufferPointerv(arg0, arg1, arg2);
    _post_call_callback("glGetNamedBufferPointerv", (void*)glGetNamedBufferPointerv, 3, arg0, arg1, arg2);
    
}
PFNGLGETNAMEDBUFFERPOINTERVPROC glad_debug_glGetNamedBufferPointerv = glad_debug_impl_glGetNamedBufferPointerv;
PFNGLGETNAMEDBUFFERSUBDATAPROC glad_glGetNamedBufferSubData;
void APIENTRY glad_debug_impl_glGetNamedBufferSubData(GLuint arg0, GLintptr arg1, GLsizeiptr arg2, void * arg3) {    
    _pre_call_callback("glGetNamedBufferSubData", (void*)glGetNamedBufferSubData, 4, arg0, arg1, arg2, arg3);
     glad_glGetNamedBufferSubData(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetNamedBufferSubData", (void*)glGetNamedBufferSubData, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETNAMEDBUFFERSUBDATAPROC glad_debug_glGetNamedBufferSubData = glad_debug_impl_glGetNamedBufferSubData;
PFNGLGETNAMEDFRAMEBUFFERATTACHMENTPARAMETERIVPROC glad_glGetNamedFramebufferAttachmentParameteriv;
void APIENTRY glad_debug_impl_glGetNamedFramebufferAttachmentParameteriv(GLuint arg0, GLenum arg1, GLenum arg2, GLint * arg3) {    
    _pre_call_callback("glGetNamedFramebufferAttachmentParameteriv", (void*)glGetNamedFramebufferAttachmentParameteriv, 4, arg0, arg1, arg2, arg3);
     glad_glGetNamedFramebufferAttachmentParameteriv(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetNamedFramebufferAttachmentParameteriv", (void*)glGetNamedFramebufferAttachmentParameteriv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETNAMEDFRAMEBUFFERATTACHMENTPARAMETERIVPROC glad_debug_glGetNamedFramebufferAttachmentParameteriv = glad_debug_impl_glGetNamedFramebufferAttachmentParameteriv;
PFNGLGETNAMEDFRAMEBUFFERPARAMETERIVPROC glad_glGetNamedFramebufferParameteriv;
void APIENTRY glad_debug_impl_glGetNamedFramebufferParameteriv(GLuint arg0, GLenum arg1, GLint * arg2) {    
    _pre_call_callback("glGetNamedFramebufferParameteriv", (void*)glGetNamedFramebufferParameteriv, 3, arg0, arg1, arg2);
     glad_glGetNamedFramebufferParameteriv(arg0, arg1, arg2);
    _post_call_callback("glGetNamedFramebufferParameteriv", (void*)glGetNamedFramebufferParameteriv, 3, arg0, arg1, arg2);
    
}
PFNGLGETNAMEDFRAMEBUFFERPARAMETERIVPROC glad_debug_glGetNamedFramebufferParameteriv = glad_debug_impl_glGetNamedFramebufferParameteriv;
PFNGLGETNAMEDRENDERBUFFERPARAMETERIVPROC glad_glGetNamedRenderbufferParameteriv;
void APIENTRY glad_debug_impl_glGetNamedRenderbufferParameteriv(GLuint arg0, GLenum arg1, GLint * arg2) {    
    _pre_call_callback("glGetNamedRenderbufferParameteriv", (void*)glGetNamedRenderbufferParameteriv, 3, arg0, arg1, arg2);
     glad_glGetNamedRenderbufferParameteriv(arg0, arg1, arg2);
    _post_call_callback("glGetNamedRenderbufferParameteriv", (void*)glGetNamedRenderbufferParameteriv, 3, arg0, arg1, arg2);
    
}
PFNGLGETNAMEDRENDERBUFFERPARAMETERIVPROC glad_debug_glGetNamedRenderbufferParameteriv = glad_debug_impl_glGetNamedRenderbufferParameteriv;
PFNGLGETOBJECTLABELPROC glad_glGetObjectLabel;
void APIENTRY glad_debug_impl_glGetObjectLabel(GLenum arg0, GLuint arg1, GLsizei arg2, GLsizei * arg3, GLchar * arg4) {    
    _pre_call_callback("glGetObjectLabel", (void*)glGetObjectLabel, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glGetObjectLabel(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glGetObjectLabel", (void*)glGetObjectLabel, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLGETOBJECTLABELPROC glad_debug_glGetObjectLabel = glad_debug_impl_glGetObjectLabel;
PFNGLGETOBJECTPTRLABELPROC glad_glGetObjectPtrLabel;
void APIENTRY glad_debug_impl_glGetObjectPtrLabel(const void * arg0, GLsizei arg1, GLsizei * arg2, GLchar * arg3) {    
    _pre_call_callback("glGetObjectPtrLabel", (void*)glGetObjectPtrLabel, 4, arg0, arg1, arg2, arg3);
     glad_glGetObjectPtrLabel(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetObjectPtrLabel", (void*)glGetObjectPtrLabel, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETOBJECTPTRLABELPROC glad_debug_glGetObjectPtrLabel = glad_debug_impl_glGetObjectPtrLabel;
PFNGLGETPIXELMAPFVPROC glad_glGetPixelMapfv;
void APIENTRY glad_debug_impl_glGetPixelMapfv(GLenum arg0, GLfloat * arg1) {    
    _pre_call_callback("glGetPixelMapfv", (void*)glGetPixelMapfv, 2, arg0, arg1);
     glad_glGetPixelMapfv(arg0, arg1);
    _post_call_callback("glGetPixelMapfv", (void*)glGetPixelMapfv, 2, arg0, arg1);
    
}
PFNGLGETPIXELMAPFVPROC glad_debug_glGetPixelMapfv = glad_debug_impl_glGetPixelMapfv;
PFNGLGETPIXELMAPUIVPROC glad_glGetPixelMapuiv;
void APIENTRY glad_debug_impl_glGetPixelMapuiv(GLenum arg0, GLuint * arg1) {    
    _pre_call_callback("glGetPixelMapuiv", (void*)glGetPixelMapuiv, 2, arg0, arg1);
     glad_glGetPixelMapuiv(arg0, arg1);
    _post_call_callback("glGetPixelMapuiv", (void*)glGetPixelMapuiv, 2, arg0, arg1);
    
}
PFNGLGETPIXELMAPUIVPROC glad_debug_glGetPixelMapuiv = glad_debug_impl_glGetPixelMapuiv;
PFNGLGETPIXELMAPUSVPROC glad_glGetPixelMapusv;
void APIENTRY glad_debug_impl_glGetPixelMapusv(GLenum arg0, GLushort * arg1) {    
    _pre_call_callback("glGetPixelMapusv", (void*)glGetPixelMapusv, 2, arg0, arg1);
     glad_glGetPixelMapusv(arg0, arg1);
    _post_call_callback("glGetPixelMapusv", (void*)glGetPixelMapusv, 2, arg0, arg1);
    
}
PFNGLGETPIXELMAPUSVPROC glad_debug_glGetPixelMapusv = glad_debug_impl_glGetPixelMapusv;
PFNGLGETPOINTERVPROC glad_glGetPointerv;
void APIENTRY glad_debug_impl_glGetPointerv(GLenum arg0, void ** arg1) {    
    _pre_call_callback("glGetPointerv", (void*)glGetPointerv, 2, arg0, arg1);
     glad_glGetPointerv(arg0, arg1);
    _post_call_callback("glGetPointerv", (void*)glGetPointerv, 2, arg0, arg1);
    
}
PFNGLGETPOINTERVPROC glad_debug_glGetPointerv = glad_debug_impl_glGetPointerv;
PFNGLGETPOLYGONSTIPPLEPROC glad_glGetPolygonStipple;
void APIENTRY glad_debug_impl_glGetPolygonStipple(GLubyte * arg0) {    
    _pre_call_callback("glGetPolygonStipple", (void*)glGetPolygonStipple, 1, arg0);
     glad_glGetPolygonStipple(arg0);
    _post_call_callback("glGetPolygonStipple", (void*)glGetPolygonStipple, 1, arg0);
    
}
PFNGLGETPOLYGONSTIPPLEPROC glad_debug_glGetPolygonStipple = glad_debug_impl_glGetPolygonStipple;
PFNGLGETPROGRAMBINARYPROC glad_glGetProgramBinary;
void APIENTRY glad_debug_impl_glGetProgramBinary(GLuint arg0, GLsizei arg1, GLsizei * arg2, GLenum * arg3, void * arg4) {    
    _pre_call_callback("glGetProgramBinary", (void*)glGetProgramBinary, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glGetProgramBinary(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glGetProgramBinary", (void*)glGetProgramBinary, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLGETPROGRAMBINARYPROC glad_debug_glGetProgramBinary = glad_debug_impl_glGetProgramBinary;
PFNGLGETPROGRAMINFOLOGPROC glad_glGetProgramInfoLog;
void APIENTRY glad_debug_impl_glGetProgramInfoLog(GLuint arg0, GLsizei arg1, GLsizei * arg2, GLchar * arg3) {    
    _pre_call_callback("glGetProgramInfoLog", (void*)glGetProgramInfoLog, 4, arg0, arg1, arg2, arg3);
     glad_glGetProgramInfoLog(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetProgramInfoLog", (void*)glGetProgramInfoLog, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETPROGRAMINFOLOGPROC glad_debug_glGetProgramInfoLog = glad_debug_impl_glGetProgramInfoLog;
PFNGLGETPROGRAMINTERFACEIVPROC glad_glGetProgramInterfaceiv;
void APIENTRY glad_debug_impl_glGetProgramInterfaceiv(GLuint arg0, GLenum arg1, GLenum arg2, GLint * arg3) {    
    _pre_call_callback("glGetProgramInterfaceiv", (void*)glGetProgramInterfaceiv, 4, arg0, arg1, arg2, arg3);
     glad_glGetProgramInterfaceiv(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetProgramInterfaceiv", (void*)glGetProgramInterfaceiv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETPROGRAMINTERFACEIVPROC glad_debug_glGetProgramInterfaceiv = glad_debug_impl_glGetProgramInterfaceiv;
PFNGLGETPROGRAMPIPELINEINFOLOGPROC glad_glGetProgramPipelineInfoLog;
void APIENTRY glad_debug_impl_glGetProgramPipelineInfoLog(GLuint arg0, GLsizei arg1, GLsizei * arg2, GLchar * arg3) {    
    _pre_call_callback("glGetProgramPipelineInfoLog", (void*)glGetProgramPipelineInfoLog, 4, arg0, arg1, arg2, arg3);
     glad_glGetProgramPipelineInfoLog(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetProgramPipelineInfoLog", (void*)glGetProgramPipelineInfoLog, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETPROGRAMPIPELINEINFOLOGPROC glad_debug_glGetProgramPipelineInfoLog = glad_debug_impl_glGetProgramPipelineInfoLog;
PFNGLGETPROGRAMPIPELINEIVPROC glad_glGetProgramPipelineiv;
void APIENTRY glad_debug_impl_glGetProgramPipelineiv(GLuint arg0, GLenum arg1, GLint * arg2) {    
    _pre_call_callback("glGetProgramPipelineiv", (void*)glGetProgramPipelineiv, 3, arg0, arg1, arg2);
     glad_glGetProgramPipelineiv(arg0, arg1, arg2);
    _post_call_callback("glGetProgramPipelineiv", (void*)glGetProgramPipelineiv, 3, arg0, arg1, arg2);
    
}
PFNGLGETPROGRAMPIPELINEIVPROC glad_debug_glGetProgramPipelineiv = glad_debug_impl_glGetProgramPipelineiv;
PFNGLGETPROGRAMRESOURCEINDEXPROC glad_glGetProgramResourceIndex;
GLuint APIENTRY glad_debug_impl_glGetProgramResourceIndex(GLuint arg0, GLenum arg1, const GLchar * arg2) {    
    GLuint ret;
    _pre_call_callback("glGetProgramResourceIndex", (void*)glGetProgramResourceIndex, 3, arg0, arg1, arg2);
    ret =  glad_glGetProgramResourceIndex(arg0, arg1, arg2);
    _post_call_callback("glGetProgramResourceIndex", (void*)glGetProgramResourceIndex, 3, arg0, arg1, arg2);
    return ret;
}
PFNGLGETPROGRAMRESOURCEINDEXPROC glad_debug_glGetProgramResourceIndex = glad_debug_impl_glGetProgramResourceIndex;
PFNGLGETPROGRAMRESOURCELOCATIONPROC glad_glGetProgramResourceLocation;
GLint APIENTRY glad_debug_impl_glGetProgramResourceLocation(GLuint arg0, GLenum arg1, const GLchar * arg2) {    
    GLint ret;
    _pre_call_callback("glGetProgramResourceLocation", (void*)glGetProgramResourceLocation, 3, arg0, arg1, arg2);
    ret =  glad_glGetProgramResourceLocation(arg0, arg1, arg2);
    _post_call_callback("glGetProgramResourceLocation", (void*)glGetProgramResourceLocation, 3, arg0, arg1, arg2);
    return ret;
}
PFNGLGETPROGRAMRESOURCELOCATIONPROC glad_debug_glGetProgramResourceLocation = glad_debug_impl_glGetProgramResourceLocation;
PFNGLGETPROGRAMRESOURCELOCATIONINDEXPROC glad_glGetProgramResourceLocationIndex;
GLint APIENTRY glad_debug_impl_glGetProgramResourceLocationIndex(GLuint arg0, GLenum arg1, const GLchar * arg2) {    
    GLint ret;
    _pre_call_callback("glGetProgramResourceLocationIndex", (void*)glGetProgramResourceLocationIndex, 3, arg0, arg1, arg2);
    ret =  glad_glGetProgramResourceLocationIndex(arg0, arg1, arg2);
    _post_call_callback("glGetProgramResourceLocationIndex", (void*)glGetProgramResourceLocationIndex, 3, arg0, arg1, arg2);
    return ret;
}
PFNGLGETPROGRAMRESOURCELOCATIONINDEXPROC glad_debug_glGetProgramResourceLocationIndex = glad_debug_impl_glGetProgramResourceLocationIndex;
PFNGLGETPROGRAMRESOURCENAMEPROC glad_glGetProgramResourceName;
void APIENTRY glad_debug_impl_glGetProgramResourceName(GLuint arg0, GLenum arg1, GLuint arg2, GLsizei arg3, GLsizei * arg4, GLchar * arg5) {    
    _pre_call_callback("glGetProgramResourceName", (void*)glGetProgramResourceName, 6, arg0, arg1, arg2, arg3, arg4, arg5);
     glad_glGetProgramResourceName(arg0, arg1, arg2, arg3, arg4, arg5);
    _post_call_callback("glGetProgramResourceName", (void*)glGetProgramResourceName, 6, arg0, arg1, arg2, arg3, arg4, arg5);
    
}
PFNGLGETPROGRAMRESOURCENAMEPROC glad_debug_glGetProgramResourceName = glad_debug_impl_glGetProgramResourceName;
PFNGLGETPROGRAMRESOURCEIVPROC glad_glGetProgramResourceiv;
void APIENTRY glad_debug_impl_glGetProgramResourceiv(GLuint arg0, GLenum arg1, GLuint arg2, GLsizei arg3, const GLenum * arg4, GLsizei arg5, GLsizei * arg6, GLint * arg7) {    
    _pre_call_callback("glGetProgramResourceiv", (void*)glGetProgramResourceiv, 8, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
     glad_glGetProgramResourceiv(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    _post_call_callback("glGetProgramResourceiv", (void*)glGetProgramResourceiv, 8, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    
}
PFNGLGETPROGRAMRESOURCEIVPROC glad_debug_glGetProgramResourceiv = glad_debug_impl_glGetProgramResourceiv;
PFNGLGETPROGRAMSTAGEIVPROC glad_glGetProgramStageiv;
void APIENTRY glad_debug_impl_glGetProgramStageiv(GLuint arg0, GLenum arg1, GLenum arg2, GLint * arg3) {    
    _pre_call_callback("glGetProgramStageiv", (void*)glGetProgramStageiv, 4, arg0, arg1, arg2, arg3);
     glad_glGetProgramStageiv(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetProgramStageiv", (void*)glGetProgramStageiv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETPROGRAMSTAGEIVPROC glad_debug_glGetProgramStageiv = glad_debug_impl_glGetProgramStageiv;
PFNGLGETPROGRAMIVPROC glad_glGetProgramiv;
void APIENTRY glad_debug_impl_glGetProgramiv(GLuint arg0, GLenum arg1, GLint * arg2) {    
    _pre_call_callback("glGetProgramiv", (void*)glGetProgramiv, 3, arg0, arg1, arg2);
     glad_glGetProgramiv(arg0, arg1, arg2);
    _post_call_callback("glGetProgramiv", (void*)glGetProgramiv, 3, arg0, arg1, arg2);
    
}
PFNGLGETPROGRAMIVPROC glad_debug_glGetProgramiv = glad_debug_impl_glGetProgramiv;
PFNGLGETQUERYBUFFEROBJECTI64VPROC glad_glGetQueryBufferObjecti64v;
void APIENTRY glad_debug_impl_glGetQueryBufferObjecti64v(GLuint arg0, GLuint arg1, GLenum arg2, GLintptr arg3) {    
    _pre_call_callback("glGetQueryBufferObjecti64v", (void*)glGetQueryBufferObjecti64v, 4, arg0, arg1, arg2, arg3);
     glad_glGetQueryBufferObjecti64v(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetQueryBufferObjecti64v", (void*)glGetQueryBufferObjecti64v, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETQUERYBUFFEROBJECTI64VPROC glad_debug_glGetQueryBufferObjecti64v = glad_debug_impl_glGetQueryBufferObjecti64v;
PFNGLGETQUERYBUFFEROBJECTIVPROC glad_glGetQueryBufferObjectiv;
void APIENTRY glad_debug_impl_glGetQueryBufferObjectiv(GLuint arg0, GLuint arg1, GLenum arg2, GLintptr arg3) {    
    _pre_call_callback("glGetQueryBufferObjectiv", (void*)glGetQueryBufferObjectiv, 4, arg0, arg1, arg2, arg3);
     glad_glGetQueryBufferObjectiv(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetQueryBufferObjectiv", (void*)glGetQueryBufferObjectiv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETQUERYBUFFEROBJECTIVPROC glad_debug_glGetQueryBufferObjectiv = glad_debug_impl_glGetQueryBufferObjectiv;
PFNGLGETQUERYBUFFEROBJECTUI64VPROC glad_glGetQueryBufferObjectui64v;
void APIENTRY glad_debug_impl_glGetQueryBufferObjectui64v(GLuint arg0, GLuint arg1, GLenum arg2, GLintptr arg3) {    
    _pre_call_callback("glGetQueryBufferObjectui64v", (void*)glGetQueryBufferObjectui64v, 4, arg0, arg1, arg2, arg3);
     glad_glGetQueryBufferObjectui64v(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetQueryBufferObjectui64v", (void*)glGetQueryBufferObjectui64v, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETQUERYBUFFEROBJECTUI64VPROC glad_debug_glGetQueryBufferObjectui64v = glad_debug_impl_glGetQueryBufferObjectui64v;
PFNGLGETQUERYBUFFEROBJECTUIVPROC glad_glGetQueryBufferObjectuiv;
void APIENTRY glad_debug_impl_glGetQueryBufferObjectuiv(GLuint arg0, GLuint arg1, GLenum arg2, GLintptr arg3) {    
    _pre_call_callback("glGetQueryBufferObjectuiv", (void*)glGetQueryBufferObjectuiv, 4, arg0, arg1, arg2, arg3);
     glad_glGetQueryBufferObjectuiv(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetQueryBufferObjectuiv", (void*)glGetQueryBufferObjectuiv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETQUERYBUFFEROBJECTUIVPROC glad_debug_glGetQueryBufferObjectuiv = glad_debug_impl_glGetQueryBufferObjectuiv;
PFNGLGETQUERYINDEXEDIVPROC glad_glGetQueryIndexediv;
void APIENTRY glad_debug_impl_glGetQueryIndexediv(GLenum arg0, GLuint arg1, GLenum arg2, GLint * arg3) {    
    _pre_call_callback("glGetQueryIndexediv", (void*)glGetQueryIndexediv, 4, arg0, arg1, arg2, arg3);
     glad_glGetQueryIndexediv(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetQueryIndexediv", (void*)glGetQueryIndexediv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETQUERYINDEXEDIVPROC glad_debug_glGetQueryIndexediv = glad_debug_impl_glGetQueryIndexediv;
PFNGLGETQUERYOBJECTI64VPROC glad_glGetQueryObjecti64v;
void APIENTRY glad_debug_impl_glGetQueryObjecti64v(GLuint arg0, GLenum arg1, GLint64 * arg2) {    
    _pre_call_callback("glGetQueryObjecti64v", (void*)glGetQueryObjecti64v, 3, arg0, arg1, arg2);
     glad_glGetQueryObjecti64v(arg0, arg1, arg2);
    _post_call_callback("glGetQueryObjecti64v", (void*)glGetQueryObjecti64v, 3, arg0, arg1, arg2);
    
}
PFNGLGETQUERYOBJECTI64VPROC glad_debug_glGetQueryObjecti64v = glad_debug_impl_glGetQueryObjecti64v;
PFNGLGETQUERYOBJECTIVPROC glad_glGetQueryObjectiv;
void APIENTRY glad_debug_impl_glGetQueryObjectiv(GLuint arg0, GLenum arg1, GLint * arg2) {    
    _pre_call_callback("glGetQueryObjectiv", (void*)glGetQueryObjectiv, 3, arg0, arg1, arg2);
     glad_glGetQueryObjectiv(arg0, arg1, arg2);
    _post_call_callback("glGetQueryObjectiv", (void*)glGetQueryObjectiv, 3, arg0, arg1, arg2);
    
}
PFNGLGETQUERYOBJECTIVPROC glad_debug_glGetQueryObjectiv = glad_debug_impl_glGetQueryObjectiv;
PFNGLGETQUERYOBJECTUI64VPROC glad_glGetQueryObjectui64v;
void APIENTRY glad_debug_impl_glGetQueryObjectui64v(GLuint arg0, GLenum arg1, GLuint64 * arg2) {    
    _pre_call_callback("glGetQueryObjectui64v", (void*)glGetQueryObjectui64v, 3, arg0, arg1, arg2);
     glad_glGetQueryObjectui64v(arg0, arg1, arg2);
    _post_call_callback("glGetQueryObjectui64v", (void*)glGetQueryObjectui64v, 3, arg0, arg1, arg2);
    
}
PFNGLGETQUERYOBJECTUI64VPROC glad_debug_glGetQueryObjectui64v = glad_debug_impl_glGetQueryObjectui64v;
PFNGLGETQUERYOBJECTUIVPROC glad_glGetQueryObjectuiv;
void APIENTRY glad_debug_impl_glGetQueryObjectuiv(GLuint arg0, GLenum arg1, GLuint * arg2) {    
    _pre_call_callback("glGetQueryObjectuiv", (void*)glGetQueryObjectuiv, 3, arg0, arg1, arg2);
     glad_glGetQueryObjectuiv(arg0, arg1, arg2);
    _post_call_callback("glGetQueryObjectuiv", (void*)glGetQueryObjectuiv, 3, arg0, arg1, arg2);
    
}
PFNGLGETQUERYOBJECTUIVPROC glad_debug_glGetQueryObjectuiv = glad_debug_impl_glGetQueryObjectuiv;
PFNGLGETQUERYIVPROC glad_glGetQueryiv;
void APIENTRY glad_debug_impl_glGetQueryiv(GLenum arg0, GLenum arg1, GLint * arg2) {    
    _pre_call_callback("glGetQueryiv", (void*)glGetQueryiv, 3, arg0, arg1, arg2);
     glad_glGetQueryiv(arg0, arg1, arg2);
    _post_call_callback("glGetQueryiv", (void*)glGetQueryiv, 3, arg0, arg1, arg2);
    
}
PFNGLGETQUERYIVPROC glad_debug_glGetQueryiv = glad_debug_impl_glGetQueryiv;
PFNGLGETRENDERBUFFERPARAMETERIVPROC glad_glGetRenderbufferParameteriv;
void APIENTRY glad_debug_impl_glGetRenderbufferParameteriv(GLenum arg0, GLenum arg1, GLint * arg2) {    
    _pre_call_callback("glGetRenderbufferParameteriv", (void*)glGetRenderbufferParameteriv, 3, arg0, arg1, arg2);
     glad_glGetRenderbufferParameteriv(arg0, arg1, arg2);
    _post_call_callback("glGetRenderbufferParameteriv", (void*)glGetRenderbufferParameteriv, 3, arg0, arg1, arg2);
    
}
PFNGLGETRENDERBUFFERPARAMETERIVPROC glad_debug_glGetRenderbufferParameteriv = glad_debug_impl_glGetRenderbufferParameteriv;
PFNGLGETSAMPLERPARAMETERIIVPROC glad_glGetSamplerParameterIiv;
void APIENTRY glad_debug_impl_glGetSamplerParameterIiv(GLuint arg0, GLenum arg1, GLint * arg2) {    
    _pre_call_callback("glGetSamplerParameterIiv", (void*)glGetSamplerParameterIiv, 3, arg0, arg1, arg2);
     glad_glGetSamplerParameterIiv(arg0, arg1, arg2);
    _post_call_callback("glGetSamplerParameterIiv", (void*)glGetSamplerParameterIiv, 3, arg0, arg1, arg2);
    
}
PFNGLGETSAMPLERPARAMETERIIVPROC glad_debug_glGetSamplerParameterIiv = glad_debug_impl_glGetSamplerParameterIiv;
PFNGLGETSAMPLERPARAMETERIUIVPROC glad_glGetSamplerParameterIuiv;
void APIENTRY glad_debug_impl_glGetSamplerParameterIuiv(GLuint arg0, GLenum arg1, GLuint * arg2) {    
    _pre_call_callback("glGetSamplerParameterIuiv", (void*)glGetSamplerParameterIuiv, 3, arg0, arg1, arg2);
     glad_glGetSamplerParameterIuiv(arg0, arg1, arg2);
    _post_call_callback("glGetSamplerParameterIuiv", (void*)glGetSamplerParameterIuiv, 3, arg0, arg1, arg2);
    
}
PFNGLGETSAMPLERPARAMETERIUIVPROC glad_debug_glGetSamplerParameterIuiv = glad_debug_impl_glGetSamplerParameterIuiv;
PFNGLGETSAMPLERPARAMETERFVPROC glad_glGetSamplerParameterfv;
void APIENTRY glad_debug_impl_glGetSamplerParameterfv(GLuint arg0, GLenum arg1, GLfloat * arg2) {    
    _pre_call_callback("glGetSamplerParameterfv", (void*)glGetSamplerParameterfv, 3, arg0, arg1, arg2);
     glad_glGetSamplerParameterfv(arg0, arg1, arg2);
    _post_call_callback("glGetSamplerParameterfv", (void*)glGetSamplerParameterfv, 3, arg0, arg1, arg2);
    
}
PFNGLGETSAMPLERPARAMETERFVPROC glad_debug_glGetSamplerParameterfv = glad_debug_impl_glGetSamplerParameterfv;
PFNGLGETSAMPLERPARAMETERIVPROC glad_glGetSamplerParameteriv;
void APIENTRY glad_debug_impl_glGetSamplerParameteriv(GLuint arg0, GLenum arg1, GLint * arg2) {    
    _pre_call_callback("glGetSamplerParameteriv", (void*)glGetSamplerParameteriv, 3, arg0, arg1, arg2);
     glad_glGetSamplerParameteriv(arg0, arg1, arg2);
    _post_call_callback("glGetSamplerParameteriv", (void*)glGetSamplerParameteriv, 3, arg0, arg1, arg2);
    
}
PFNGLGETSAMPLERPARAMETERIVPROC glad_debug_glGetSamplerParameteriv = glad_debug_impl_glGetSamplerParameteriv;
PFNGLGETSHADERINFOLOGPROC glad_glGetShaderInfoLog;
void APIENTRY glad_debug_impl_glGetShaderInfoLog(GLuint arg0, GLsizei arg1, GLsizei * arg2, GLchar * arg3) {    
    _pre_call_callback("glGetShaderInfoLog", (void*)glGetShaderInfoLog, 4, arg0, arg1, arg2, arg3);
     glad_glGetShaderInfoLog(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetShaderInfoLog", (void*)glGetShaderInfoLog, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETSHADERINFOLOGPROC glad_debug_glGetShaderInfoLog = glad_debug_impl_glGetShaderInfoLog;
PFNGLGETSHADERPRECISIONFORMATPROC glad_glGetShaderPrecisionFormat;
void APIENTRY glad_debug_impl_glGetShaderPrecisionFormat(GLenum arg0, GLenum arg1, GLint * arg2, GLint * arg3) {    
    _pre_call_callback("glGetShaderPrecisionFormat", (void*)glGetShaderPrecisionFormat, 4, arg0, arg1, arg2, arg3);
     glad_glGetShaderPrecisionFormat(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetShaderPrecisionFormat", (void*)glGetShaderPrecisionFormat, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETSHADERPRECISIONFORMATPROC glad_debug_glGetShaderPrecisionFormat = glad_debug_impl_glGetShaderPrecisionFormat;
PFNGLGETSHADERSOURCEPROC glad_glGetShaderSource;
void APIENTRY glad_debug_impl_glGetShaderSource(GLuint arg0, GLsizei arg1, GLsizei * arg2, GLchar * arg3) {    
    _pre_call_callback("glGetShaderSource", (void*)glGetShaderSource, 4, arg0, arg1, arg2, arg3);
     glad_glGetShaderSource(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetShaderSource", (void*)glGetShaderSource, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETSHADERSOURCEPROC glad_debug_glGetShaderSource = glad_debug_impl_glGetShaderSource;
PFNGLGETSHADERIVPROC glad_glGetShaderiv;
void APIENTRY glad_debug_impl_glGetShaderiv(GLuint arg0, GLenum arg1, GLint * arg2) {    
    _pre_call_callback("glGetShaderiv", (void*)glGetShaderiv, 3, arg0, arg1, arg2);
     glad_glGetShaderiv(arg0, arg1, arg2);
    _post_call_callback("glGetShaderiv", (void*)glGetShaderiv, 3, arg0, arg1, arg2);
    
}
PFNGLGETSHADERIVPROC glad_debug_glGetShaderiv = glad_debug_impl_glGetShaderiv;
PFNGLGETSTRINGPROC glad_glGetString;
const GLubyte * APIENTRY glad_debug_impl_glGetString(GLenum arg0) {    
    const GLubyte * ret;
    _pre_call_callback("glGetString", (void*)glGetString, 1, arg0);
    ret =  glad_glGetString(arg0);
    _post_call_callback("glGetString", (void*)glGetString, 1, arg0);
    return ret;
}
PFNGLGETSTRINGPROC glad_debug_glGetString = glad_debug_impl_glGetString;
PFNGLGETSTRINGIPROC glad_glGetStringi;
const GLubyte * APIENTRY glad_debug_impl_glGetStringi(GLenum arg0, GLuint arg1) {    
    const GLubyte * ret;
    _pre_call_callback("glGetStringi", (void*)glGetStringi, 2, arg0, arg1);
    ret =  glad_glGetStringi(arg0, arg1);
    _post_call_callback("glGetStringi", (void*)glGetStringi, 2, arg0, arg1);
    return ret;
}
PFNGLGETSTRINGIPROC glad_debug_glGetStringi = glad_debug_impl_glGetStringi;
PFNGLGETSUBROUTINEINDEXPROC glad_glGetSubroutineIndex;
GLuint APIENTRY glad_debug_impl_glGetSubroutineIndex(GLuint arg0, GLenum arg1, const GLchar * arg2) {    
    GLuint ret;
    _pre_call_callback("glGetSubroutineIndex", (void*)glGetSubroutineIndex, 3, arg0, arg1, arg2);
    ret =  glad_glGetSubroutineIndex(arg0, arg1, arg2);
    _post_call_callback("glGetSubroutineIndex", (void*)glGetSubroutineIndex, 3, arg0, arg1, arg2);
    return ret;
}
PFNGLGETSUBROUTINEINDEXPROC glad_debug_glGetSubroutineIndex = glad_debug_impl_glGetSubroutineIndex;
PFNGLGETSUBROUTINEUNIFORMLOCATIONPROC glad_glGetSubroutineUniformLocation;
GLint APIENTRY glad_debug_impl_glGetSubroutineUniformLocation(GLuint arg0, GLenum arg1, const GLchar * arg2) {    
    GLint ret;
    _pre_call_callback("glGetSubroutineUniformLocation", (void*)glGetSubroutineUniformLocation, 3, arg0, arg1, arg2);
    ret =  glad_glGetSubroutineUniformLocation(arg0, arg1, arg2);
    _post_call_callback("glGetSubroutineUniformLocation", (void*)glGetSubroutineUniformLocation, 3, arg0, arg1, arg2);
    return ret;
}
PFNGLGETSUBROUTINEUNIFORMLOCATIONPROC glad_debug_glGetSubroutineUniformLocation = glad_debug_impl_glGetSubroutineUniformLocation;
PFNGLGETSYNCIVPROC glad_glGetSynciv;
void APIENTRY glad_debug_impl_glGetSynciv(GLsync arg0, GLenum arg1, GLsizei arg2, GLsizei * arg3, GLint * arg4) {    
    _pre_call_callback("glGetSynciv", (void*)glGetSynciv, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glGetSynciv(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glGetSynciv", (void*)glGetSynciv, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLGETSYNCIVPROC glad_debug_glGetSynciv = glad_debug_impl_glGetSynciv;
PFNGLGETTEXENVFVPROC glad_glGetTexEnvfv;
void APIENTRY glad_debug_impl_glGetTexEnvfv(GLenum arg0, GLenum arg1, GLfloat * arg2) {    
    _pre_call_callback("glGetTexEnvfv", (void*)glGetTexEnvfv, 3, arg0, arg1, arg2);
     glad_glGetTexEnvfv(arg0, arg1, arg2);
    _post_call_callback("glGetTexEnvfv", (void*)glGetTexEnvfv, 3, arg0, arg1, arg2);
    
}
PFNGLGETTEXENVFVPROC glad_debug_glGetTexEnvfv = glad_debug_impl_glGetTexEnvfv;
PFNGLGETTEXENVIVPROC glad_glGetTexEnviv;
void APIENTRY glad_debug_impl_glGetTexEnviv(GLenum arg0, GLenum arg1, GLint * arg2) {    
    _pre_call_callback("glGetTexEnviv", (void*)glGetTexEnviv, 3, arg0, arg1, arg2);
     glad_glGetTexEnviv(arg0, arg1, arg2);
    _post_call_callback("glGetTexEnviv", (void*)glGetTexEnviv, 3, arg0, arg1, arg2);
    
}
PFNGLGETTEXENVIVPROC glad_debug_glGetTexEnviv = glad_debug_impl_glGetTexEnviv;
PFNGLGETTEXGENDVPROC glad_glGetTexGendv;
void APIENTRY glad_debug_impl_glGetTexGendv(GLenum arg0, GLenum arg1, GLdouble * arg2) {    
    _pre_call_callback("glGetTexGendv", (void*)glGetTexGendv, 3, arg0, arg1, arg2);
     glad_glGetTexGendv(arg0, arg1, arg2);
    _post_call_callback("glGetTexGendv", (void*)glGetTexGendv, 3, arg0, arg1, arg2);
    
}
PFNGLGETTEXGENDVPROC glad_debug_glGetTexGendv = glad_debug_impl_glGetTexGendv;
PFNGLGETTEXGENFVPROC glad_glGetTexGenfv;
void APIENTRY glad_debug_impl_glGetTexGenfv(GLenum arg0, GLenum arg1, GLfloat * arg2) {    
    _pre_call_callback("glGetTexGenfv", (void*)glGetTexGenfv, 3, arg0, arg1, arg2);
     glad_glGetTexGenfv(arg0, arg1, arg2);
    _post_call_callback("glGetTexGenfv", (void*)glGetTexGenfv, 3, arg0, arg1, arg2);
    
}
PFNGLGETTEXGENFVPROC glad_debug_glGetTexGenfv = glad_debug_impl_glGetTexGenfv;
PFNGLGETTEXGENIVPROC glad_glGetTexGeniv;
void APIENTRY glad_debug_impl_glGetTexGeniv(GLenum arg0, GLenum arg1, GLint * arg2) {    
    _pre_call_callback("glGetTexGeniv", (void*)glGetTexGeniv, 3, arg0, arg1, arg2);
     glad_glGetTexGeniv(arg0, arg1, arg2);
    _post_call_callback("glGetTexGeniv", (void*)glGetTexGeniv, 3, arg0, arg1, arg2);
    
}
PFNGLGETTEXGENIVPROC glad_debug_glGetTexGeniv = glad_debug_impl_glGetTexGeniv;
PFNGLGETTEXIMAGEPROC glad_glGetTexImage;
void APIENTRY glad_debug_impl_glGetTexImage(GLenum arg0, GLint arg1, GLenum arg2, GLenum arg3, void * arg4) {    
    _pre_call_callback("glGetTexImage", (void*)glGetTexImage, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glGetTexImage(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glGetTexImage", (void*)glGetTexImage, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLGETTEXIMAGEPROC glad_debug_glGetTexImage = glad_debug_impl_glGetTexImage;
PFNGLGETTEXLEVELPARAMETERFVPROC glad_glGetTexLevelParameterfv;
void APIENTRY glad_debug_impl_glGetTexLevelParameterfv(GLenum arg0, GLint arg1, GLenum arg2, GLfloat * arg3) {    
    _pre_call_callback("glGetTexLevelParameterfv", (void*)glGetTexLevelParameterfv, 4, arg0, arg1, arg2, arg3);
     glad_glGetTexLevelParameterfv(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetTexLevelParameterfv", (void*)glGetTexLevelParameterfv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETTEXLEVELPARAMETERFVPROC glad_debug_glGetTexLevelParameterfv = glad_debug_impl_glGetTexLevelParameterfv;
PFNGLGETTEXLEVELPARAMETERIVPROC glad_glGetTexLevelParameteriv;
void APIENTRY glad_debug_impl_glGetTexLevelParameteriv(GLenum arg0, GLint arg1, GLenum arg2, GLint * arg3) {    
    _pre_call_callback("glGetTexLevelParameteriv", (void*)glGetTexLevelParameteriv, 4, arg0, arg1, arg2, arg3);
     glad_glGetTexLevelParameteriv(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetTexLevelParameteriv", (void*)glGetTexLevelParameteriv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETTEXLEVELPARAMETERIVPROC glad_debug_glGetTexLevelParameteriv = glad_debug_impl_glGetTexLevelParameteriv;
PFNGLGETTEXPARAMETERIIVPROC glad_glGetTexParameterIiv;
void APIENTRY glad_debug_impl_glGetTexParameterIiv(GLenum arg0, GLenum arg1, GLint * arg2) {    
    _pre_call_callback("glGetTexParameterIiv", (void*)glGetTexParameterIiv, 3, arg0, arg1, arg2);
     glad_glGetTexParameterIiv(arg0, arg1, arg2);
    _post_call_callback("glGetTexParameterIiv", (void*)glGetTexParameterIiv, 3, arg0, arg1, arg2);
    
}
PFNGLGETTEXPARAMETERIIVPROC glad_debug_glGetTexParameterIiv = glad_debug_impl_glGetTexParameterIiv;
PFNGLGETTEXPARAMETERIUIVPROC glad_glGetTexParameterIuiv;
void APIENTRY glad_debug_impl_glGetTexParameterIuiv(GLenum arg0, GLenum arg1, GLuint * arg2) {    
    _pre_call_callback("glGetTexParameterIuiv", (void*)glGetTexParameterIuiv, 3, arg0, arg1, arg2);
     glad_glGetTexParameterIuiv(arg0, arg1, arg2);
    _post_call_callback("glGetTexParameterIuiv", (void*)glGetTexParameterIuiv, 3, arg0, arg1, arg2);
    
}
PFNGLGETTEXPARAMETERIUIVPROC glad_debug_glGetTexParameterIuiv = glad_debug_impl_glGetTexParameterIuiv;
PFNGLGETTEXPARAMETERFVPROC glad_glGetTexParameterfv;
void APIENTRY glad_debug_impl_glGetTexParameterfv(GLenum arg0, GLenum arg1, GLfloat * arg2) {    
    _pre_call_callback("glGetTexParameterfv", (void*)glGetTexParameterfv, 3, arg0, arg1, arg2);
     glad_glGetTexParameterfv(arg0, arg1, arg2);
    _post_call_callback("glGetTexParameterfv", (void*)glGetTexParameterfv, 3, arg0, arg1, arg2);
    
}
PFNGLGETTEXPARAMETERFVPROC glad_debug_glGetTexParameterfv = glad_debug_impl_glGetTexParameterfv;
PFNGLGETTEXPARAMETERIVPROC glad_glGetTexParameteriv;
void APIENTRY glad_debug_impl_glGetTexParameteriv(GLenum arg0, GLenum arg1, GLint * arg2) {    
    _pre_call_callback("glGetTexParameteriv", (void*)glGetTexParameteriv, 3, arg0, arg1, arg2);
     glad_glGetTexParameteriv(arg0, arg1, arg2);
    _post_call_callback("glGetTexParameteriv", (void*)glGetTexParameteriv, 3, arg0, arg1, arg2);
    
}
PFNGLGETTEXPARAMETERIVPROC glad_debug_glGetTexParameteriv = glad_debug_impl_glGetTexParameteriv;
PFNGLGETTEXTUREIMAGEPROC glad_glGetTextureImage;
void APIENTRY glad_debug_impl_glGetTextureImage(GLuint arg0, GLint arg1, GLenum arg2, GLenum arg3, GLsizei arg4, void * arg5) {    
    _pre_call_callback("glGetTextureImage", (void*)glGetTextureImage, 6, arg0, arg1, arg2, arg3, arg4, arg5);
     glad_glGetTextureImage(arg0, arg1, arg2, arg3, arg4, arg5);
    _post_call_callback("glGetTextureImage", (void*)glGetTextureImage, 6, arg0, arg1, arg2, arg3, arg4, arg5);
    
}
PFNGLGETTEXTUREIMAGEPROC glad_debug_glGetTextureImage = glad_debug_impl_glGetTextureImage;
PFNGLGETTEXTURELEVELPARAMETERFVPROC glad_glGetTextureLevelParameterfv;
void APIENTRY glad_debug_impl_glGetTextureLevelParameterfv(GLuint arg0, GLint arg1, GLenum arg2, GLfloat * arg3) {    
    _pre_call_callback("glGetTextureLevelParameterfv", (void*)glGetTextureLevelParameterfv, 4, arg0, arg1, arg2, arg3);
     glad_glGetTextureLevelParameterfv(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetTextureLevelParameterfv", (void*)glGetTextureLevelParameterfv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETTEXTURELEVELPARAMETERFVPROC glad_debug_glGetTextureLevelParameterfv = glad_debug_impl_glGetTextureLevelParameterfv;
PFNGLGETTEXTURELEVELPARAMETERIVPROC glad_glGetTextureLevelParameteriv;
void APIENTRY glad_debug_impl_glGetTextureLevelParameteriv(GLuint arg0, GLint arg1, GLenum arg2, GLint * arg3) {    
    _pre_call_callback("glGetTextureLevelParameteriv", (void*)glGetTextureLevelParameteriv, 4, arg0, arg1, arg2, arg3);
     glad_glGetTextureLevelParameteriv(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetTextureLevelParameteriv", (void*)glGetTextureLevelParameteriv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETTEXTURELEVELPARAMETERIVPROC glad_debug_glGetTextureLevelParameteriv = glad_debug_impl_glGetTextureLevelParameteriv;
PFNGLGETTEXTUREPARAMETERIIVPROC glad_glGetTextureParameterIiv;
void APIENTRY glad_debug_impl_glGetTextureParameterIiv(GLuint arg0, GLenum arg1, GLint * arg2) {    
    _pre_call_callback("glGetTextureParameterIiv", (void*)glGetTextureParameterIiv, 3, arg0, arg1, arg2);
     glad_glGetTextureParameterIiv(arg0, arg1, arg2);
    _post_call_callback("glGetTextureParameterIiv", (void*)glGetTextureParameterIiv, 3, arg0, arg1, arg2);
    
}
PFNGLGETTEXTUREPARAMETERIIVPROC glad_debug_glGetTextureParameterIiv = glad_debug_impl_glGetTextureParameterIiv;
PFNGLGETTEXTUREPARAMETERIUIVPROC glad_glGetTextureParameterIuiv;
void APIENTRY glad_debug_impl_glGetTextureParameterIuiv(GLuint arg0, GLenum arg1, GLuint * arg2) {    
    _pre_call_callback("glGetTextureParameterIuiv", (void*)glGetTextureParameterIuiv, 3, arg0, arg1, arg2);
     glad_glGetTextureParameterIuiv(arg0, arg1, arg2);
    _post_call_callback("glGetTextureParameterIuiv", (void*)glGetTextureParameterIuiv, 3, arg0, arg1, arg2);
    
}
PFNGLGETTEXTUREPARAMETERIUIVPROC glad_debug_glGetTextureParameterIuiv = glad_debug_impl_glGetTextureParameterIuiv;
PFNGLGETTEXTUREPARAMETERFVPROC glad_glGetTextureParameterfv;
void APIENTRY glad_debug_impl_glGetTextureParameterfv(GLuint arg0, GLenum arg1, GLfloat * arg2) {    
    _pre_call_callback("glGetTextureParameterfv", (void*)glGetTextureParameterfv, 3, arg0, arg1, arg2);
     glad_glGetTextureParameterfv(arg0, arg1, arg2);
    _post_call_callback("glGetTextureParameterfv", (void*)glGetTextureParameterfv, 3, arg0, arg1, arg2);
    
}
PFNGLGETTEXTUREPARAMETERFVPROC glad_debug_glGetTextureParameterfv = glad_debug_impl_glGetTextureParameterfv;
PFNGLGETTEXTUREPARAMETERIVPROC glad_glGetTextureParameteriv;
void APIENTRY glad_debug_impl_glGetTextureParameteriv(GLuint arg0, GLenum arg1, GLint * arg2) {    
    _pre_call_callback("glGetTextureParameteriv", (void*)glGetTextureParameteriv, 3, arg0, arg1, arg2);
     glad_glGetTextureParameteriv(arg0, arg1, arg2);
    _post_call_callback("glGetTextureParameteriv", (void*)glGetTextureParameteriv, 3, arg0, arg1, arg2);
    
}
PFNGLGETTEXTUREPARAMETERIVPROC glad_debug_glGetTextureParameteriv = glad_debug_impl_glGetTextureParameteriv;
PFNGLGETTEXTURESUBIMAGEPROC glad_glGetTextureSubImage;
void APIENTRY glad_debug_impl_glGetTextureSubImage(GLuint arg0, GLint arg1, GLint arg2, GLint arg3, GLint arg4, GLsizei arg5, GLsizei arg6, GLsizei arg7, GLenum arg8, GLenum arg9, GLsizei arg10, void * arg11) {    
    _pre_call_callback("glGetTextureSubImage", (void*)glGetTextureSubImage, 12, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11);
     glad_glGetTextureSubImage(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11);
    _post_call_callback("glGetTextureSubImage", (void*)glGetTextureSubImage, 12, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11);
    
}
PFNGLGETTEXTURESUBIMAGEPROC glad_debug_glGetTextureSubImage = glad_debug_impl_glGetTextureSubImage;
PFNGLGETTRANSFORMFEEDBACKVARYINGPROC glad_glGetTransformFeedbackVarying;
void APIENTRY glad_debug_impl_glGetTransformFeedbackVarying(GLuint arg0, GLuint arg1, GLsizei arg2, GLsizei * arg3, GLsizei * arg4, GLenum * arg5, GLchar * arg6) {    
    _pre_call_callback("glGetTransformFeedbackVarying", (void*)glGetTransformFeedbackVarying, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
     glad_glGetTransformFeedbackVarying(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    _post_call_callback("glGetTransformFeedbackVarying", (void*)glGetTransformFeedbackVarying, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    
}
PFNGLGETTRANSFORMFEEDBACKVARYINGPROC glad_debug_glGetTransformFeedbackVarying = glad_debug_impl_glGetTransformFeedbackVarying;
PFNGLGETTRANSFORMFEEDBACKI64_VPROC glad_glGetTransformFeedbacki64_v;
void APIENTRY glad_debug_impl_glGetTransformFeedbacki64_v(GLuint arg0, GLenum arg1, GLuint arg2, GLint64 * arg3) {    
    _pre_call_callback("glGetTransformFeedbacki64_v", (void*)glGetTransformFeedbacki64_v, 4, arg0, arg1, arg2, arg3);
     glad_glGetTransformFeedbacki64_v(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetTransformFeedbacki64_v", (void*)glGetTransformFeedbacki64_v, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETTRANSFORMFEEDBACKI64_VPROC glad_debug_glGetTransformFeedbacki64_v = glad_debug_impl_glGetTransformFeedbacki64_v;
PFNGLGETTRANSFORMFEEDBACKI_VPROC glad_glGetTransformFeedbacki_v;
void APIENTRY glad_debug_impl_glGetTransformFeedbacki_v(GLuint arg0, GLenum arg1, GLuint arg2, GLint * arg3) {    
    _pre_call_callback("glGetTransformFeedbacki_v", (void*)glGetTransformFeedbacki_v, 4, arg0, arg1, arg2, arg3);
     glad_glGetTransformFeedbacki_v(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetTransformFeedbacki_v", (void*)glGetTransformFeedbacki_v, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETTRANSFORMFEEDBACKI_VPROC glad_debug_glGetTransformFeedbacki_v = glad_debug_impl_glGetTransformFeedbacki_v;
PFNGLGETTRANSFORMFEEDBACKIVPROC glad_glGetTransformFeedbackiv;
void APIENTRY glad_debug_impl_glGetTransformFeedbackiv(GLuint arg0, GLenum arg1, GLint * arg2) {    
    _pre_call_callback("glGetTransformFeedbackiv", (void*)glGetTransformFeedbackiv, 3, arg0, arg1, arg2);
     glad_glGetTransformFeedbackiv(arg0, arg1, arg2);
    _post_call_callback("glGetTransformFeedbackiv", (void*)glGetTransformFeedbackiv, 3, arg0, arg1, arg2);
    
}
PFNGLGETTRANSFORMFEEDBACKIVPROC glad_debug_glGetTransformFeedbackiv = glad_debug_impl_glGetTransformFeedbackiv;
PFNGLGETUNIFORMBLOCKINDEXPROC glad_glGetUniformBlockIndex;
GLuint APIENTRY glad_debug_impl_glGetUniformBlockIndex(GLuint arg0, const GLchar * arg1) {    
    GLuint ret;
    _pre_call_callback("glGetUniformBlockIndex", (void*)glGetUniformBlockIndex, 2, arg0, arg1);
    ret =  glad_glGetUniformBlockIndex(arg0, arg1);
    _post_call_callback("glGetUniformBlockIndex", (void*)glGetUniformBlockIndex, 2, arg0, arg1);
    return ret;
}
PFNGLGETUNIFORMBLOCKINDEXPROC glad_debug_glGetUniformBlockIndex = glad_debug_impl_glGetUniformBlockIndex;
PFNGLGETUNIFORMINDICESPROC glad_glGetUniformIndices;
void APIENTRY glad_debug_impl_glGetUniformIndices(GLuint arg0, GLsizei arg1, const GLchar *const* arg2, GLuint * arg3) {    
    _pre_call_callback("glGetUniformIndices", (void*)glGetUniformIndices, 4, arg0, arg1, arg2, arg3);
     glad_glGetUniformIndices(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetUniformIndices", (void*)glGetUniformIndices, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETUNIFORMINDICESPROC glad_debug_glGetUniformIndices = glad_debug_impl_glGetUniformIndices;
PFNGLGETUNIFORMLOCATIONPROC glad_glGetUniformLocation;
GLint APIENTRY glad_debug_impl_glGetUniformLocation(GLuint arg0, const GLchar * arg1) {    
    GLint ret;
    _pre_call_callback("glGetUniformLocation", (void*)glGetUniformLocation, 2, arg0, arg1);
    ret =  glad_glGetUniformLocation(arg0, arg1);
    _post_call_callback("glGetUniformLocation", (void*)glGetUniformLocation, 2, arg0, arg1);
    return ret;
}
PFNGLGETUNIFORMLOCATIONPROC glad_debug_glGetUniformLocation = glad_debug_impl_glGetUniformLocation;
PFNGLGETUNIFORMSUBROUTINEUIVPROC glad_glGetUniformSubroutineuiv;
void APIENTRY glad_debug_impl_glGetUniformSubroutineuiv(GLenum arg0, GLint arg1, GLuint * arg2) {    
    _pre_call_callback("glGetUniformSubroutineuiv", (void*)glGetUniformSubroutineuiv, 3, arg0, arg1, arg2);
     glad_glGetUniformSubroutineuiv(arg0, arg1, arg2);
    _post_call_callback("glGetUniformSubroutineuiv", (void*)glGetUniformSubroutineuiv, 3, arg0, arg1, arg2);
    
}
PFNGLGETUNIFORMSUBROUTINEUIVPROC glad_debug_glGetUniformSubroutineuiv = glad_debug_impl_glGetUniformSubroutineuiv;
PFNGLGETUNIFORMDVPROC glad_glGetUniformdv;
void APIENTRY glad_debug_impl_glGetUniformdv(GLuint arg0, GLint arg1, GLdouble * arg2) {    
    _pre_call_callback("glGetUniformdv", (void*)glGetUniformdv, 3, arg0, arg1, arg2);
     glad_glGetUniformdv(arg0, arg1, arg2);
    _post_call_callback("glGetUniformdv", (void*)glGetUniformdv, 3, arg0, arg1, arg2);
    
}
PFNGLGETUNIFORMDVPROC glad_debug_glGetUniformdv = glad_debug_impl_glGetUniformdv;
PFNGLGETUNIFORMFVPROC glad_glGetUniformfv;
void APIENTRY glad_debug_impl_glGetUniformfv(GLuint arg0, GLint arg1, GLfloat * arg2) {    
    _pre_call_callback("glGetUniformfv", (void*)glGetUniformfv, 3, arg0, arg1, arg2);
     glad_glGetUniformfv(arg0, arg1, arg2);
    _post_call_callback("glGetUniformfv", (void*)glGetUniformfv, 3, arg0, arg1, arg2);
    
}
PFNGLGETUNIFORMFVPROC glad_debug_glGetUniformfv = glad_debug_impl_glGetUniformfv;
PFNGLGETUNIFORMIVPROC glad_glGetUniformiv;
void APIENTRY glad_debug_impl_glGetUniformiv(GLuint arg0, GLint arg1, GLint * arg2) {    
    _pre_call_callback("glGetUniformiv", (void*)glGetUniformiv, 3, arg0, arg1, arg2);
     glad_glGetUniformiv(arg0, arg1, arg2);
    _post_call_callback("glGetUniformiv", (void*)glGetUniformiv, 3, arg0, arg1, arg2);
    
}
PFNGLGETUNIFORMIVPROC glad_debug_glGetUniformiv = glad_debug_impl_glGetUniformiv;
PFNGLGETUNIFORMUIVPROC glad_glGetUniformuiv;
void APIENTRY glad_debug_impl_glGetUniformuiv(GLuint arg0, GLint arg1, GLuint * arg2) {    
    _pre_call_callback("glGetUniformuiv", (void*)glGetUniformuiv, 3, arg0, arg1, arg2);
     glad_glGetUniformuiv(arg0, arg1, arg2);
    _post_call_callback("glGetUniformuiv", (void*)glGetUniformuiv, 3, arg0, arg1, arg2);
    
}
PFNGLGETUNIFORMUIVPROC glad_debug_glGetUniformuiv = glad_debug_impl_glGetUniformuiv;
PFNGLGETVERTEXARRAYINDEXED64IVPROC glad_glGetVertexArrayIndexed64iv;
void APIENTRY glad_debug_impl_glGetVertexArrayIndexed64iv(GLuint arg0, GLuint arg1, GLenum arg2, GLint64 * arg3) {    
    _pre_call_callback("glGetVertexArrayIndexed64iv", (void*)glGetVertexArrayIndexed64iv, 4, arg0, arg1, arg2, arg3);
     glad_glGetVertexArrayIndexed64iv(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetVertexArrayIndexed64iv", (void*)glGetVertexArrayIndexed64iv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETVERTEXARRAYINDEXED64IVPROC glad_debug_glGetVertexArrayIndexed64iv = glad_debug_impl_glGetVertexArrayIndexed64iv;
PFNGLGETVERTEXARRAYINDEXEDIVPROC glad_glGetVertexArrayIndexediv;
void APIENTRY glad_debug_impl_glGetVertexArrayIndexediv(GLuint arg0, GLuint arg1, GLenum arg2, GLint * arg3) {    
    _pre_call_callback("glGetVertexArrayIndexediv", (void*)glGetVertexArrayIndexediv, 4, arg0, arg1, arg2, arg3);
     glad_glGetVertexArrayIndexediv(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetVertexArrayIndexediv", (void*)glGetVertexArrayIndexediv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETVERTEXARRAYINDEXEDIVPROC glad_debug_glGetVertexArrayIndexediv = glad_debug_impl_glGetVertexArrayIndexediv;
PFNGLGETVERTEXARRAYIVPROC glad_glGetVertexArrayiv;
void APIENTRY glad_debug_impl_glGetVertexArrayiv(GLuint arg0, GLenum arg1, GLint * arg2) {    
    _pre_call_callback("glGetVertexArrayiv", (void*)glGetVertexArrayiv, 3, arg0, arg1, arg2);
     glad_glGetVertexArrayiv(arg0, arg1, arg2);
    _post_call_callback("glGetVertexArrayiv", (void*)glGetVertexArrayiv, 3, arg0, arg1, arg2);
    
}
PFNGLGETVERTEXARRAYIVPROC glad_debug_glGetVertexArrayiv = glad_debug_impl_glGetVertexArrayiv;
PFNGLGETVERTEXATTRIBIIVPROC glad_glGetVertexAttribIiv;
void APIENTRY glad_debug_impl_glGetVertexAttribIiv(GLuint arg0, GLenum arg1, GLint * arg2) {    
    _pre_call_callback("glGetVertexAttribIiv", (void*)glGetVertexAttribIiv, 3, arg0, arg1, arg2);
     glad_glGetVertexAttribIiv(arg0, arg1, arg2);
    _post_call_callback("glGetVertexAttribIiv", (void*)glGetVertexAttribIiv, 3, arg0, arg1, arg2);
    
}
PFNGLGETVERTEXATTRIBIIVPROC glad_debug_glGetVertexAttribIiv = glad_debug_impl_glGetVertexAttribIiv;
PFNGLGETVERTEXATTRIBIUIVPROC glad_glGetVertexAttribIuiv;
void APIENTRY glad_debug_impl_glGetVertexAttribIuiv(GLuint arg0, GLenum arg1, GLuint * arg2) {    
    _pre_call_callback("glGetVertexAttribIuiv", (void*)glGetVertexAttribIuiv, 3, arg0, arg1, arg2);
     glad_glGetVertexAttribIuiv(arg0, arg1, arg2);
    _post_call_callback("glGetVertexAttribIuiv", (void*)glGetVertexAttribIuiv, 3, arg0, arg1, arg2);
    
}
PFNGLGETVERTEXATTRIBIUIVPROC glad_debug_glGetVertexAttribIuiv = glad_debug_impl_glGetVertexAttribIuiv;
PFNGLGETVERTEXATTRIBLDVPROC glad_glGetVertexAttribLdv;
void APIENTRY glad_debug_impl_glGetVertexAttribLdv(GLuint arg0, GLenum arg1, GLdouble * arg2) {    
    _pre_call_callback("glGetVertexAttribLdv", (void*)glGetVertexAttribLdv, 3, arg0, arg1, arg2);
     glad_glGetVertexAttribLdv(arg0, arg1, arg2);
    _post_call_callback("glGetVertexAttribLdv", (void*)glGetVertexAttribLdv, 3, arg0, arg1, arg2);
    
}
PFNGLGETVERTEXATTRIBLDVPROC glad_debug_glGetVertexAttribLdv = glad_debug_impl_glGetVertexAttribLdv;
PFNGLGETVERTEXATTRIBPOINTERVPROC glad_glGetVertexAttribPointerv;
void APIENTRY glad_debug_impl_glGetVertexAttribPointerv(GLuint arg0, GLenum arg1, void ** arg2) {    
    _pre_call_callback("glGetVertexAttribPointerv", (void*)glGetVertexAttribPointerv, 3, arg0, arg1, arg2);
     glad_glGetVertexAttribPointerv(arg0, arg1, arg2);
    _post_call_callback("glGetVertexAttribPointerv", (void*)glGetVertexAttribPointerv, 3, arg0, arg1, arg2);
    
}
PFNGLGETVERTEXATTRIBPOINTERVPROC glad_debug_glGetVertexAttribPointerv = glad_debug_impl_glGetVertexAttribPointerv;
PFNGLGETVERTEXATTRIBDVPROC glad_glGetVertexAttribdv;
void APIENTRY glad_debug_impl_glGetVertexAttribdv(GLuint arg0, GLenum arg1, GLdouble * arg2) {    
    _pre_call_callback("glGetVertexAttribdv", (void*)glGetVertexAttribdv, 3, arg0, arg1, arg2);
     glad_glGetVertexAttribdv(arg0, arg1, arg2);
    _post_call_callback("glGetVertexAttribdv", (void*)glGetVertexAttribdv, 3, arg0, arg1, arg2);
    
}
PFNGLGETVERTEXATTRIBDVPROC glad_debug_glGetVertexAttribdv = glad_debug_impl_glGetVertexAttribdv;
PFNGLGETVERTEXATTRIBFVPROC glad_glGetVertexAttribfv;
void APIENTRY glad_debug_impl_glGetVertexAttribfv(GLuint arg0, GLenum arg1, GLfloat * arg2) {    
    _pre_call_callback("glGetVertexAttribfv", (void*)glGetVertexAttribfv, 3, arg0, arg1, arg2);
     glad_glGetVertexAttribfv(arg0, arg1, arg2);
    _post_call_callback("glGetVertexAttribfv", (void*)glGetVertexAttribfv, 3, arg0, arg1, arg2);
    
}
PFNGLGETVERTEXATTRIBFVPROC glad_debug_glGetVertexAttribfv = glad_debug_impl_glGetVertexAttribfv;
PFNGLGETVERTEXATTRIBIVPROC glad_glGetVertexAttribiv;
void APIENTRY glad_debug_impl_glGetVertexAttribiv(GLuint arg0, GLenum arg1, GLint * arg2) {    
    _pre_call_callback("glGetVertexAttribiv", (void*)glGetVertexAttribiv, 3, arg0, arg1, arg2);
     glad_glGetVertexAttribiv(arg0, arg1, arg2);
    _post_call_callback("glGetVertexAttribiv", (void*)glGetVertexAttribiv, 3, arg0, arg1, arg2);
    
}
PFNGLGETVERTEXATTRIBIVPROC glad_debug_glGetVertexAttribiv = glad_debug_impl_glGetVertexAttribiv;
PFNGLGETNCOLORTABLEPROC glad_glGetnColorTable;
void APIENTRY glad_debug_impl_glGetnColorTable(GLenum arg0, GLenum arg1, GLenum arg2, GLsizei arg3, void * arg4) {    
    _pre_call_callback("glGetnColorTable", (void*)glGetnColorTable, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glGetnColorTable(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glGetnColorTable", (void*)glGetnColorTable, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLGETNCOLORTABLEPROC glad_debug_glGetnColorTable = glad_debug_impl_glGetnColorTable;
PFNGLGETNCOMPRESSEDTEXIMAGEPROC glad_glGetnCompressedTexImage;
void APIENTRY glad_debug_impl_glGetnCompressedTexImage(GLenum arg0, GLint arg1, GLsizei arg2, void * arg3) {    
    _pre_call_callback("glGetnCompressedTexImage", (void*)glGetnCompressedTexImage, 4, arg0, arg1, arg2, arg3);
     glad_glGetnCompressedTexImage(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetnCompressedTexImage", (void*)glGetnCompressedTexImage, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETNCOMPRESSEDTEXIMAGEPROC glad_debug_glGetnCompressedTexImage = glad_debug_impl_glGetnCompressedTexImage;
PFNGLGETNCONVOLUTIONFILTERPROC glad_glGetnConvolutionFilter;
void APIENTRY glad_debug_impl_glGetnConvolutionFilter(GLenum arg0, GLenum arg1, GLenum arg2, GLsizei arg3, void * arg4) {    
    _pre_call_callback("glGetnConvolutionFilter", (void*)glGetnConvolutionFilter, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glGetnConvolutionFilter(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glGetnConvolutionFilter", (void*)glGetnConvolutionFilter, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLGETNCONVOLUTIONFILTERPROC glad_debug_glGetnConvolutionFilter = glad_debug_impl_glGetnConvolutionFilter;
PFNGLGETNHISTOGRAMPROC glad_glGetnHistogram;
void APIENTRY glad_debug_impl_glGetnHistogram(GLenum arg0, GLboolean arg1, GLenum arg2, GLenum arg3, GLsizei arg4, void * arg5) {    
    _pre_call_callback("glGetnHistogram", (void*)glGetnHistogram, 6, arg0, arg1, arg2, arg3, arg4, arg5);
     glad_glGetnHistogram(arg0, arg1, arg2, arg3, arg4, arg5);
    _post_call_callback("glGetnHistogram", (void*)glGetnHistogram, 6, arg0, arg1, arg2, arg3, arg4, arg5);
    
}
PFNGLGETNHISTOGRAMPROC glad_debug_glGetnHistogram = glad_debug_impl_glGetnHistogram;
PFNGLGETNMAPDVPROC glad_glGetnMapdv;
void APIENTRY glad_debug_impl_glGetnMapdv(GLenum arg0, GLenum arg1, GLsizei arg2, GLdouble * arg3) {    
    _pre_call_callback("glGetnMapdv", (void*)glGetnMapdv, 4, arg0, arg1, arg2, arg3);
     glad_glGetnMapdv(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetnMapdv", (void*)glGetnMapdv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETNMAPDVPROC glad_debug_glGetnMapdv = glad_debug_impl_glGetnMapdv;
PFNGLGETNMAPFVPROC glad_glGetnMapfv;
void APIENTRY glad_debug_impl_glGetnMapfv(GLenum arg0, GLenum arg1, GLsizei arg2, GLfloat * arg3) {    
    _pre_call_callback("glGetnMapfv", (void*)glGetnMapfv, 4, arg0, arg1, arg2, arg3);
     glad_glGetnMapfv(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetnMapfv", (void*)glGetnMapfv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETNMAPFVPROC glad_debug_glGetnMapfv = glad_debug_impl_glGetnMapfv;
PFNGLGETNMAPIVPROC glad_glGetnMapiv;
void APIENTRY glad_debug_impl_glGetnMapiv(GLenum arg0, GLenum arg1, GLsizei arg2, GLint * arg3) {    
    _pre_call_callback("glGetnMapiv", (void*)glGetnMapiv, 4, arg0, arg1, arg2, arg3);
     glad_glGetnMapiv(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetnMapiv", (void*)glGetnMapiv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETNMAPIVPROC glad_debug_glGetnMapiv = glad_debug_impl_glGetnMapiv;
PFNGLGETNMINMAXPROC glad_glGetnMinmax;
void APIENTRY glad_debug_impl_glGetnMinmax(GLenum arg0, GLboolean arg1, GLenum arg2, GLenum arg3, GLsizei arg4, void * arg5) {    
    _pre_call_callback("glGetnMinmax", (void*)glGetnMinmax, 6, arg0, arg1, arg2, arg3, arg4, arg5);
     glad_glGetnMinmax(arg0, arg1, arg2, arg3, arg4, arg5);
    _post_call_callback("glGetnMinmax", (void*)glGetnMinmax, 6, arg0, arg1, arg2, arg3, arg4, arg5);
    
}
PFNGLGETNMINMAXPROC glad_debug_glGetnMinmax = glad_debug_impl_glGetnMinmax;
PFNGLGETNPIXELMAPFVPROC glad_glGetnPixelMapfv;
void APIENTRY glad_debug_impl_glGetnPixelMapfv(GLenum arg0, GLsizei arg1, GLfloat * arg2) {    
    _pre_call_callback("glGetnPixelMapfv", (void*)glGetnPixelMapfv, 3, arg0, arg1, arg2);
     glad_glGetnPixelMapfv(arg0, arg1, arg2);
    _post_call_callback("glGetnPixelMapfv", (void*)glGetnPixelMapfv, 3, arg0, arg1, arg2);
    
}
PFNGLGETNPIXELMAPFVPROC glad_debug_glGetnPixelMapfv = glad_debug_impl_glGetnPixelMapfv;
PFNGLGETNPIXELMAPUIVPROC glad_glGetnPixelMapuiv;
void APIENTRY glad_debug_impl_glGetnPixelMapuiv(GLenum arg0, GLsizei arg1, GLuint * arg2) {    
    _pre_call_callback("glGetnPixelMapuiv", (void*)glGetnPixelMapuiv, 3, arg0, arg1, arg2);
     glad_glGetnPixelMapuiv(arg0, arg1, arg2);
    _post_call_callback("glGetnPixelMapuiv", (void*)glGetnPixelMapuiv, 3, arg0, arg1, arg2);
    
}
PFNGLGETNPIXELMAPUIVPROC glad_debug_glGetnPixelMapuiv = glad_debug_impl_glGetnPixelMapuiv;
PFNGLGETNPIXELMAPUSVPROC glad_glGetnPixelMapusv;
void APIENTRY glad_debug_impl_glGetnPixelMapusv(GLenum arg0, GLsizei arg1, GLushort * arg2) {    
    _pre_call_callback("glGetnPixelMapusv", (void*)glGetnPixelMapusv, 3, arg0, arg1, arg2);
     glad_glGetnPixelMapusv(arg0, arg1, arg2);
    _post_call_callback("glGetnPixelMapusv", (void*)glGetnPixelMapusv, 3, arg0, arg1, arg2);
    
}
PFNGLGETNPIXELMAPUSVPROC glad_debug_glGetnPixelMapusv = glad_debug_impl_glGetnPixelMapusv;
PFNGLGETNPOLYGONSTIPPLEPROC glad_glGetnPolygonStipple;
void APIENTRY glad_debug_impl_glGetnPolygonStipple(GLsizei arg0, GLubyte * arg1) {    
    _pre_call_callback("glGetnPolygonStipple", (void*)glGetnPolygonStipple, 2, arg0, arg1);
     glad_glGetnPolygonStipple(arg0, arg1);
    _post_call_callback("glGetnPolygonStipple", (void*)glGetnPolygonStipple, 2, arg0, arg1);
    
}
PFNGLGETNPOLYGONSTIPPLEPROC glad_debug_glGetnPolygonStipple = glad_debug_impl_glGetnPolygonStipple;
PFNGLGETNSEPARABLEFILTERPROC glad_glGetnSeparableFilter;
void APIENTRY glad_debug_impl_glGetnSeparableFilter(GLenum arg0, GLenum arg1, GLenum arg2, GLsizei arg3, void * arg4, GLsizei arg5, void * arg6, void * arg7) {    
    _pre_call_callback("glGetnSeparableFilter", (void*)glGetnSeparableFilter, 8, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
     glad_glGetnSeparableFilter(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    _post_call_callback("glGetnSeparableFilter", (void*)glGetnSeparableFilter, 8, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    
}
PFNGLGETNSEPARABLEFILTERPROC glad_debug_glGetnSeparableFilter = glad_debug_impl_glGetnSeparableFilter;
PFNGLGETNTEXIMAGEPROC glad_glGetnTexImage;
void APIENTRY glad_debug_impl_glGetnTexImage(GLenum arg0, GLint arg1, GLenum arg2, GLenum arg3, GLsizei arg4, void * arg5) {    
    _pre_call_callback("glGetnTexImage", (void*)glGetnTexImage, 6, arg0, arg1, arg2, arg3, arg4, arg5);
     glad_glGetnTexImage(arg0, arg1, arg2, arg3, arg4, arg5);
    _post_call_callback("glGetnTexImage", (void*)glGetnTexImage, 6, arg0, arg1, arg2, arg3, arg4, arg5);
    
}
PFNGLGETNTEXIMAGEPROC glad_debug_glGetnTexImage = glad_debug_impl_glGetnTexImage;
PFNGLGETNUNIFORMDVPROC glad_glGetnUniformdv;
void APIENTRY glad_debug_impl_glGetnUniformdv(GLuint arg0, GLint arg1, GLsizei arg2, GLdouble * arg3) {    
    _pre_call_callback("glGetnUniformdv", (void*)glGetnUniformdv, 4, arg0, arg1, arg2, arg3);
     glad_glGetnUniformdv(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetnUniformdv", (void*)glGetnUniformdv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETNUNIFORMDVPROC glad_debug_glGetnUniformdv = glad_debug_impl_glGetnUniformdv;
PFNGLGETNUNIFORMFVPROC glad_glGetnUniformfv;
void APIENTRY glad_debug_impl_glGetnUniformfv(GLuint arg0, GLint arg1, GLsizei arg2, GLfloat * arg3) {    
    _pre_call_callback("glGetnUniformfv", (void*)glGetnUniformfv, 4, arg0, arg1, arg2, arg3);
     glad_glGetnUniformfv(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetnUniformfv", (void*)glGetnUniformfv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETNUNIFORMFVPROC glad_debug_glGetnUniformfv = glad_debug_impl_glGetnUniformfv;
PFNGLGETNUNIFORMIVPROC glad_glGetnUniformiv;
void APIENTRY glad_debug_impl_glGetnUniformiv(GLuint arg0, GLint arg1, GLsizei arg2, GLint * arg3) {    
    _pre_call_callback("glGetnUniformiv", (void*)glGetnUniformiv, 4, arg0, arg1, arg2, arg3);
     glad_glGetnUniformiv(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetnUniformiv", (void*)glGetnUniformiv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETNUNIFORMIVPROC glad_debug_glGetnUniformiv = glad_debug_impl_glGetnUniformiv;
PFNGLGETNUNIFORMUIVPROC glad_glGetnUniformuiv;
void APIENTRY glad_debug_impl_glGetnUniformuiv(GLuint arg0, GLint arg1, GLsizei arg2, GLuint * arg3) {    
    _pre_call_callback("glGetnUniformuiv", (void*)glGetnUniformuiv, 4, arg0, arg1, arg2, arg3);
     glad_glGetnUniformuiv(arg0, arg1, arg2, arg3);
    _post_call_callback("glGetnUniformuiv", (void*)glGetnUniformuiv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLGETNUNIFORMUIVPROC glad_debug_glGetnUniformuiv = glad_debug_impl_glGetnUniformuiv;
PFNGLHINTPROC glad_glHint;
void APIENTRY glad_debug_impl_glHint(GLenum arg0, GLenum arg1) {    
    _pre_call_callback("glHint", (void*)glHint, 2, arg0, arg1);
     glad_glHint(arg0, arg1);
    _post_call_callback("glHint", (void*)glHint, 2, arg0, arg1);
    
}
PFNGLHINTPROC glad_debug_glHint = glad_debug_impl_glHint;
PFNGLINDEXMASKPROC glad_glIndexMask;
void APIENTRY glad_debug_impl_glIndexMask(GLuint arg0) {    
    _pre_call_callback("glIndexMask", (void*)glIndexMask, 1, arg0);
     glad_glIndexMask(arg0);
    _post_call_callback("glIndexMask", (void*)glIndexMask, 1, arg0);
    
}
PFNGLINDEXMASKPROC glad_debug_glIndexMask = glad_debug_impl_glIndexMask;
PFNGLINDEXPOINTERPROC glad_glIndexPointer;
void APIENTRY glad_debug_impl_glIndexPointer(GLenum arg0, GLsizei arg1, const void * arg2) {    
    _pre_call_callback("glIndexPointer", (void*)glIndexPointer, 3, arg0, arg1, arg2);
     glad_glIndexPointer(arg0, arg1, arg2);
    _post_call_callback("glIndexPointer", (void*)glIndexPointer, 3, arg0, arg1, arg2);
    
}
PFNGLINDEXPOINTERPROC glad_debug_glIndexPointer = glad_debug_impl_glIndexPointer;
PFNGLINDEXDPROC glad_glIndexd;
void APIENTRY glad_debug_impl_glIndexd(GLdouble arg0) {    
    _pre_call_callback("glIndexd", (void*)glIndexd, 1, arg0);
     glad_glIndexd(arg0);
    _post_call_callback("glIndexd", (void*)glIndexd, 1, arg0);
    
}
PFNGLINDEXDPROC glad_debug_glIndexd = glad_debug_impl_glIndexd;
PFNGLINDEXDVPROC glad_glIndexdv;
void APIENTRY glad_debug_impl_glIndexdv(const GLdouble * arg0) {    
    _pre_call_callback("glIndexdv", (void*)glIndexdv, 1, arg0);
     glad_glIndexdv(arg0);
    _post_call_callback("glIndexdv", (void*)glIndexdv, 1, arg0);
    
}
PFNGLINDEXDVPROC glad_debug_glIndexdv = glad_debug_impl_glIndexdv;
PFNGLINDEXFPROC glad_glIndexf;
void APIENTRY glad_debug_impl_glIndexf(GLfloat arg0) {    
    _pre_call_callback("glIndexf", (void*)glIndexf, 1, arg0);
     glad_glIndexf(arg0);
    _post_call_callback("glIndexf", (void*)glIndexf, 1, arg0);
    
}
PFNGLINDEXFPROC glad_debug_glIndexf = glad_debug_impl_glIndexf;
PFNGLINDEXFVPROC glad_glIndexfv;
void APIENTRY glad_debug_impl_glIndexfv(const GLfloat * arg0) {    
    _pre_call_callback("glIndexfv", (void*)glIndexfv, 1, arg0);
     glad_glIndexfv(arg0);
    _post_call_callback("glIndexfv", (void*)glIndexfv, 1, arg0);
    
}
PFNGLINDEXFVPROC glad_debug_glIndexfv = glad_debug_impl_glIndexfv;
PFNGLINDEXIPROC glad_glIndexi;
void APIENTRY glad_debug_impl_glIndexi(GLint arg0) {    
    _pre_call_callback("glIndexi", (void*)glIndexi, 1, arg0);
     glad_glIndexi(arg0);
    _post_call_callback("glIndexi", (void*)glIndexi, 1, arg0);
    
}
PFNGLINDEXIPROC glad_debug_glIndexi = glad_debug_impl_glIndexi;
PFNGLINDEXIVPROC glad_glIndexiv;
void APIENTRY glad_debug_impl_glIndexiv(const GLint * arg0) {    
    _pre_call_callback("glIndexiv", (void*)glIndexiv, 1, arg0);
     glad_glIndexiv(arg0);
    _post_call_callback("glIndexiv", (void*)glIndexiv, 1, arg0);
    
}
PFNGLINDEXIVPROC glad_debug_glIndexiv = glad_debug_impl_glIndexiv;
PFNGLINDEXSPROC glad_glIndexs;
void APIENTRY glad_debug_impl_glIndexs(GLshort arg0) {    
    _pre_call_callback("glIndexs", (void*)glIndexs, 1, arg0);
     glad_glIndexs(arg0);
    _post_call_callback("glIndexs", (void*)glIndexs, 1, arg0);
    
}
PFNGLINDEXSPROC glad_debug_glIndexs = glad_debug_impl_glIndexs;
PFNGLINDEXSVPROC glad_glIndexsv;
void APIENTRY glad_debug_impl_glIndexsv(const GLshort * arg0) {    
    _pre_call_callback("glIndexsv", (void*)glIndexsv, 1, arg0);
     glad_glIndexsv(arg0);
    _post_call_callback("glIndexsv", (void*)glIndexsv, 1, arg0);
    
}
PFNGLINDEXSVPROC glad_debug_glIndexsv = glad_debug_impl_glIndexsv;
PFNGLINDEXUBPROC glad_glIndexub;
void APIENTRY glad_debug_impl_glIndexub(GLubyte arg0) {    
    _pre_call_callback("glIndexub", (void*)glIndexub, 1, arg0);
     glad_glIndexub(arg0);
    _post_call_callback("glIndexub", (void*)glIndexub, 1, arg0);
    
}
PFNGLINDEXUBPROC glad_debug_glIndexub = glad_debug_impl_glIndexub;
PFNGLINDEXUBVPROC glad_glIndexubv;
void APIENTRY glad_debug_impl_glIndexubv(const GLubyte * arg0) {    
    _pre_call_callback("glIndexubv", (void*)glIndexubv, 1, arg0);
     glad_glIndexubv(arg0);
    _post_call_callback("glIndexubv", (void*)glIndexubv, 1, arg0);
    
}
PFNGLINDEXUBVPROC glad_debug_glIndexubv = glad_debug_impl_glIndexubv;
PFNGLINITNAMESPROC glad_glInitNames;
void APIENTRY glad_debug_impl_glInitNames(void) {    
    _pre_call_callback("glInitNames", (void*)glInitNames, 0);
     glad_glInitNames();
    _post_call_callback("glInitNames", (void*)glInitNames, 0);
    
}
PFNGLINITNAMESPROC glad_debug_glInitNames = glad_debug_impl_glInitNames;
PFNGLINTERLEAVEDARRAYSPROC glad_glInterleavedArrays;
void APIENTRY glad_debug_impl_glInterleavedArrays(GLenum arg0, GLsizei arg1, const void * arg2) {    
    _pre_call_callback("glInterleavedArrays", (void*)glInterleavedArrays, 3, arg0, arg1, arg2);
     glad_glInterleavedArrays(arg0, arg1, arg2);
    _post_call_callback("glInterleavedArrays", (void*)glInterleavedArrays, 3, arg0, arg1, arg2);
    
}
PFNGLINTERLEAVEDARRAYSPROC glad_debug_glInterleavedArrays = glad_debug_impl_glInterleavedArrays;
PFNGLINVALIDATEBUFFERDATAPROC glad_glInvalidateBufferData;
void APIENTRY glad_debug_impl_glInvalidateBufferData(GLuint arg0) {    
    _pre_call_callback("glInvalidateBufferData", (void*)glInvalidateBufferData, 1, arg0);
     glad_glInvalidateBufferData(arg0);
    _post_call_callback("glInvalidateBufferData", (void*)glInvalidateBufferData, 1, arg0);
    
}
PFNGLINVALIDATEBUFFERDATAPROC glad_debug_glInvalidateBufferData = glad_debug_impl_glInvalidateBufferData;
PFNGLINVALIDATEBUFFERSUBDATAPROC glad_glInvalidateBufferSubData;
void APIENTRY glad_debug_impl_glInvalidateBufferSubData(GLuint arg0, GLintptr arg1, GLsizeiptr arg2) {    
    _pre_call_callback("glInvalidateBufferSubData", (void*)glInvalidateBufferSubData, 3, arg0, arg1, arg2);
     glad_glInvalidateBufferSubData(arg0, arg1, arg2);
    _post_call_callback("glInvalidateBufferSubData", (void*)glInvalidateBufferSubData, 3, arg0, arg1, arg2);
    
}
PFNGLINVALIDATEBUFFERSUBDATAPROC glad_debug_glInvalidateBufferSubData = glad_debug_impl_glInvalidateBufferSubData;
PFNGLINVALIDATEFRAMEBUFFERPROC glad_glInvalidateFramebuffer;
void APIENTRY glad_debug_impl_glInvalidateFramebuffer(GLenum arg0, GLsizei arg1, const GLenum * arg2) {    
    _pre_call_callback("glInvalidateFramebuffer", (void*)glInvalidateFramebuffer, 3, arg0, arg1, arg2);
     glad_glInvalidateFramebuffer(arg0, arg1, arg2);
    _post_call_callback("glInvalidateFramebuffer", (void*)glInvalidateFramebuffer, 3, arg0, arg1, arg2);
    
}
PFNGLINVALIDATEFRAMEBUFFERPROC glad_debug_glInvalidateFramebuffer = glad_debug_impl_glInvalidateFramebuffer;
PFNGLINVALIDATENAMEDFRAMEBUFFERDATAPROC glad_glInvalidateNamedFramebufferData;
void APIENTRY glad_debug_impl_glInvalidateNamedFramebufferData(GLuint arg0, GLsizei arg1, const GLenum * arg2) {    
    _pre_call_callback("glInvalidateNamedFramebufferData", (void*)glInvalidateNamedFramebufferData, 3, arg0, arg1, arg2);
     glad_glInvalidateNamedFramebufferData(arg0, arg1, arg2);
    _post_call_callback("glInvalidateNamedFramebufferData", (void*)glInvalidateNamedFramebufferData, 3, arg0, arg1, arg2);
    
}
PFNGLINVALIDATENAMEDFRAMEBUFFERDATAPROC glad_debug_glInvalidateNamedFramebufferData = glad_debug_impl_glInvalidateNamedFramebufferData;
PFNGLINVALIDATENAMEDFRAMEBUFFERSUBDATAPROC glad_glInvalidateNamedFramebufferSubData;
void APIENTRY glad_debug_impl_glInvalidateNamedFramebufferSubData(GLuint arg0, GLsizei arg1, const GLenum * arg2, GLint arg3, GLint arg4, GLsizei arg5, GLsizei arg6) {    
    _pre_call_callback("glInvalidateNamedFramebufferSubData", (void*)glInvalidateNamedFramebufferSubData, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
     glad_glInvalidateNamedFramebufferSubData(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    _post_call_callback("glInvalidateNamedFramebufferSubData", (void*)glInvalidateNamedFramebufferSubData, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    
}
PFNGLINVALIDATENAMEDFRAMEBUFFERSUBDATAPROC glad_debug_glInvalidateNamedFramebufferSubData = glad_debug_impl_glInvalidateNamedFramebufferSubData;
PFNGLINVALIDATESUBFRAMEBUFFERPROC glad_glInvalidateSubFramebuffer;
void APIENTRY glad_debug_impl_glInvalidateSubFramebuffer(GLenum arg0, GLsizei arg1, const GLenum * arg2, GLint arg3, GLint arg4, GLsizei arg5, GLsizei arg6) {    
    _pre_call_callback("glInvalidateSubFramebuffer", (void*)glInvalidateSubFramebuffer, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
     glad_glInvalidateSubFramebuffer(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    _post_call_callback("glInvalidateSubFramebuffer", (void*)glInvalidateSubFramebuffer, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    
}
PFNGLINVALIDATESUBFRAMEBUFFERPROC glad_debug_glInvalidateSubFramebuffer = glad_debug_impl_glInvalidateSubFramebuffer;
PFNGLINVALIDATETEXIMAGEPROC glad_glInvalidateTexImage;
void APIENTRY glad_debug_impl_glInvalidateTexImage(GLuint arg0, GLint arg1) {    
    _pre_call_callback("glInvalidateTexImage", (void*)glInvalidateTexImage, 2, arg0, arg1);
     glad_glInvalidateTexImage(arg0, arg1);
    _post_call_callback("glInvalidateTexImage", (void*)glInvalidateTexImage, 2, arg0, arg1);
    
}
PFNGLINVALIDATETEXIMAGEPROC glad_debug_glInvalidateTexImage = glad_debug_impl_glInvalidateTexImage;
PFNGLINVALIDATETEXSUBIMAGEPROC glad_glInvalidateTexSubImage;
void APIENTRY glad_debug_impl_glInvalidateTexSubImage(GLuint arg0, GLint arg1, GLint arg2, GLint arg3, GLint arg4, GLsizei arg5, GLsizei arg6, GLsizei arg7) {    
    _pre_call_callback("glInvalidateTexSubImage", (void*)glInvalidateTexSubImage, 8, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
     glad_glInvalidateTexSubImage(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    _post_call_callback("glInvalidateTexSubImage", (void*)glInvalidateTexSubImage, 8, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    
}
PFNGLINVALIDATETEXSUBIMAGEPROC glad_debug_glInvalidateTexSubImage = glad_debug_impl_glInvalidateTexSubImage;
PFNGLISBUFFERPROC glad_glIsBuffer;
GLboolean APIENTRY glad_debug_impl_glIsBuffer(GLuint arg0) {    
    GLboolean ret;
    _pre_call_callback("glIsBuffer", (void*)glIsBuffer, 1, arg0);
    ret =  glad_glIsBuffer(arg0);
    _post_call_callback("glIsBuffer", (void*)glIsBuffer, 1, arg0);
    return ret;
}
PFNGLISBUFFERPROC glad_debug_glIsBuffer = glad_debug_impl_glIsBuffer;
PFNGLISENABLEDPROC glad_glIsEnabled;
GLboolean APIENTRY glad_debug_impl_glIsEnabled(GLenum arg0) {    
    GLboolean ret;
    _pre_call_callback("glIsEnabled", (void*)glIsEnabled, 1, arg0);
    ret =  glad_glIsEnabled(arg0);
    _post_call_callback("glIsEnabled", (void*)glIsEnabled, 1, arg0);
    return ret;
}
PFNGLISENABLEDPROC glad_debug_glIsEnabled = glad_debug_impl_glIsEnabled;
PFNGLISENABLEDIPROC glad_glIsEnabledi;
GLboolean APIENTRY glad_debug_impl_glIsEnabledi(GLenum arg0, GLuint arg1) {    
    GLboolean ret;
    _pre_call_callback("glIsEnabledi", (void*)glIsEnabledi, 2, arg0, arg1);
    ret =  glad_glIsEnabledi(arg0, arg1);
    _post_call_callback("glIsEnabledi", (void*)glIsEnabledi, 2, arg0, arg1);
    return ret;
}
PFNGLISENABLEDIPROC glad_debug_glIsEnabledi = glad_debug_impl_glIsEnabledi;
PFNGLISFRAMEBUFFERPROC glad_glIsFramebuffer;
GLboolean APIENTRY glad_debug_impl_glIsFramebuffer(GLuint arg0) {    
    GLboolean ret;
    _pre_call_callback("glIsFramebuffer", (void*)glIsFramebuffer, 1, arg0);
    ret =  glad_glIsFramebuffer(arg0);
    _post_call_callback("glIsFramebuffer", (void*)glIsFramebuffer, 1, arg0);
    return ret;
}
PFNGLISFRAMEBUFFERPROC glad_debug_glIsFramebuffer = glad_debug_impl_glIsFramebuffer;
PFNGLISLISTPROC glad_glIsList;
GLboolean APIENTRY glad_debug_impl_glIsList(GLuint arg0) {    
    GLboolean ret;
    _pre_call_callback("glIsList", (void*)glIsList, 1, arg0);
    ret =  glad_glIsList(arg0);
    _post_call_callback("glIsList", (void*)glIsList, 1, arg0);
    return ret;
}
PFNGLISLISTPROC glad_debug_glIsList = glad_debug_impl_glIsList;
PFNGLISPROGRAMPROC glad_glIsProgram;
GLboolean APIENTRY glad_debug_impl_glIsProgram(GLuint arg0) {    
    GLboolean ret;
    _pre_call_callback("glIsProgram", (void*)glIsProgram, 1, arg0);
    ret =  glad_glIsProgram(arg0);
    _post_call_callback("glIsProgram", (void*)glIsProgram, 1, arg0);
    return ret;
}
PFNGLISPROGRAMPROC glad_debug_glIsProgram = glad_debug_impl_glIsProgram;
PFNGLISPROGRAMPIPELINEPROC glad_glIsProgramPipeline;
GLboolean APIENTRY glad_debug_impl_glIsProgramPipeline(GLuint arg0) {    
    GLboolean ret;
    _pre_call_callback("glIsProgramPipeline", (void*)glIsProgramPipeline, 1, arg0);
    ret =  glad_glIsProgramPipeline(arg0);
    _post_call_callback("glIsProgramPipeline", (void*)glIsProgramPipeline, 1, arg0);
    return ret;
}
PFNGLISPROGRAMPIPELINEPROC glad_debug_glIsProgramPipeline = glad_debug_impl_glIsProgramPipeline;
PFNGLISQUERYPROC glad_glIsQuery;
GLboolean APIENTRY glad_debug_impl_glIsQuery(GLuint arg0) {    
    GLboolean ret;
    _pre_call_callback("glIsQuery", (void*)glIsQuery, 1, arg0);
    ret =  glad_glIsQuery(arg0);
    _post_call_callback("glIsQuery", (void*)glIsQuery, 1, arg0);
    return ret;
}
PFNGLISQUERYPROC glad_debug_glIsQuery = glad_debug_impl_glIsQuery;
PFNGLISRENDERBUFFERPROC glad_glIsRenderbuffer;
GLboolean APIENTRY glad_debug_impl_glIsRenderbuffer(GLuint arg0) {    
    GLboolean ret;
    _pre_call_callback("glIsRenderbuffer", (void*)glIsRenderbuffer, 1, arg0);
    ret =  glad_glIsRenderbuffer(arg0);
    _post_call_callback("glIsRenderbuffer", (void*)glIsRenderbuffer, 1, arg0);
    return ret;
}
PFNGLISRENDERBUFFERPROC glad_debug_glIsRenderbuffer = glad_debug_impl_glIsRenderbuffer;
PFNGLISSAMPLERPROC glad_glIsSampler;
GLboolean APIENTRY glad_debug_impl_glIsSampler(GLuint arg0) {    
    GLboolean ret;
    _pre_call_callback("glIsSampler", (void*)glIsSampler, 1, arg0);
    ret =  glad_glIsSampler(arg0);
    _post_call_callback("glIsSampler", (void*)glIsSampler, 1, arg0);
    return ret;
}
PFNGLISSAMPLERPROC glad_debug_glIsSampler = glad_debug_impl_glIsSampler;
PFNGLISSHADERPROC glad_glIsShader;
GLboolean APIENTRY glad_debug_impl_glIsShader(GLuint arg0) {    
    GLboolean ret;
    _pre_call_callback("glIsShader", (void*)glIsShader, 1, arg0);
    ret =  glad_glIsShader(arg0);
    _post_call_callback("glIsShader", (void*)glIsShader, 1, arg0);
    return ret;
}
PFNGLISSHADERPROC glad_debug_glIsShader = glad_debug_impl_glIsShader;
PFNGLISSYNCPROC glad_glIsSync;
GLboolean APIENTRY glad_debug_impl_glIsSync(GLsync arg0) {    
    GLboolean ret;
    _pre_call_callback("glIsSync", (void*)glIsSync, 1, arg0);
    ret =  glad_glIsSync(arg0);
    _post_call_callback("glIsSync", (void*)glIsSync, 1, arg0);
    return ret;
}
PFNGLISSYNCPROC glad_debug_glIsSync = glad_debug_impl_glIsSync;
PFNGLISTEXTUREPROC glad_glIsTexture;
GLboolean APIENTRY glad_debug_impl_glIsTexture(GLuint arg0) {    
    GLboolean ret;
    _pre_call_callback("glIsTexture", (void*)glIsTexture, 1, arg0);
    ret =  glad_glIsTexture(arg0);
    _post_call_callback("glIsTexture", (void*)glIsTexture, 1, arg0);
    return ret;
}
PFNGLISTEXTUREPROC glad_debug_glIsTexture = glad_debug_impl_glIsTexture;
PFNGLISTRANSFORMFEEDBACKPROC glad_glIsTransformFeedback;
GLboolean APIENTRY glad_debug_impl_glIsTransformFeedback(GLuint arg0) {    
    GLboolean ret;
    _pre_call_callback("glIsTransformFeedback", (void*)glIsTransformFeedback, 1, arg0);
    ret =  glad_glIsTransformFeedback(arg0);
    _post_call_callback("glIsTransformFeedback", (void*)glIsTransformFeedback, 1, arg0);
    return ret;
}
PFNGLISTRANSFORMFEEDBACKPROC glad_debug_glIsTransformFeedback = glad_debug_impl_glIsTransformFeedback;
PFNGLISVERTEXARRAYPROC glad_glIsVertexArray;
GLboolean APIENTRY glad_debug_impl_glIsVertexArray(GLuint arg0) {    
    GLboolean ret;
    _pre_call_callback("glIsVertexArray", (void*)glIsVertexArray, 1, arg0);
    ret =  glad_glIsVertexArray(arg0);
    _post_call_callback("glIsVertexArray", (void*)glIsVertexArray, 1, arg0);
    return ret;
}
PFNGLISVERTEXARRAYPROC glad_debug_glIsVertexArray = glad_debug_impl_glIsVertexArray;
PFNGLLIGHTMODELFPROC glad_glLightModelf;
void APIENTRY glad_debug_impl_glLightModelf(GLenum arg0, GLfloat arg1) {    
    _pre_call_callback("glLightModelf", (void*)glLightModelf, 2, arg0, arg1);
     glad_glLightModelf(arg0, arg1);
    _post_call_callback("glLightModelf", (void*)glLightModelf, 2, arg0, arg1);
    
}
PFNGLLIGHTMODELFPROC glad_debug_glLightModelf = glad_debug_impl_glLightModelf;
PFNGLLIGHTMODELFVPROC glad_glLightModelfv;
void APIENTRY glad_debug_impl_glLightModelfv(GLenum arg0, const GLfloat * arg1) {    
    _pre_call_callback("glLightModelfv", (void*)glLightModelfv, 2, arg0, arg1);
     glad_glLightModelfv(arg0, arg1);
    _post_call_callback("glLightModelfv", (void*)glLightModelfv, 2, arg0, arg1);
    
}
PFNGLLIGHTMODELFVPROC glad_debug_glLightModelfv = glad_debug_impl_glLightModelfv;
PFNGLLIGHTMODELIPROC glad_glLightModeli;
void APIENTRY glad_debug_impl_glLightModeli(GLenum arg0, GLint arg1) {    
    _pre_call_callback("glLightModeli", (void*)glLightModeli, 2, arg0, arg1);
     glad_glLightModeli(arg0, arg1);
    _post_call_callback("glLightModeli", (void*)glLightModeli, 2, arg0, arg1);
    
}
PFNGLLIGHTMODELIPROC glad_debug_glLightModeli = glad_debug_impl_glLightModeli;
PFNGLLIGHTMODELIVPROC glad_glLightModeliv;
void APIENTRY glad_debug_impl_glLightModeliv(GLenum arg0, const GLint * arg1) {    
    _pre_call_callback("glLightModeliv", (void*)glLightModeliv, 2, arg0, arg1);
     glad_glLightModeliv(arg0, arg1);
    _post_call_callback("glLightModeliv", (void*)glLightModeliv, 2, arg0, arg1);
    
}
PFNGLLIGHTMODELIVPROC glad_debug_glLightModeliv = glad_debug_impl_glLightModeliv;
PFNGLLIGHTFPROC glad_glLightf;
void APIENTRY glad_debug_impl_glLightf(GLenum arg0, GLenum arg1, GLfloat arg2) {    
    _pre_call_callback("glLightf", (void*)glLightf, 3, arg0, arg1, arg2);
     glad_glLightf(arg0, arg1, arg2);
    _post_call_callback("glLightf", (void*)glLightf, 3, arg0, arg1, arg2);
    
}
PFNGLLIGHTFPROC glad_debug_glLightf = glad_debug_impl_glLightf;
PFNGLLIGHTFVPROC glad_glLightfv;
void APIENTRY glad_debug_impl_glLightfv(GLenum arg0, GLenum arg1, const GLfloat * arg2) {    
    _pre_call_callback("glLightfv", (void*)glLightfv, 3, arg0, arg1, arg2);
     glad_glLightfv(arg0, arg1, arg2);
    _post_call_callback("glLightfv", (void*)glLightfv, 3, arg0, arg1, arg2);
    
}
PFNGLLIGHTFVPROC glad_debug_glLightfv = glad_debug_impl_glLightfv;
PFNGLLIGHTIPROC glad_glLighti;
void APIENTRY glad_debug_impl_glLighti(GLenum arg0, GLenum arg1, GLint arg2) {    
    _pre_call_callback("glLighti", (void*)glLighti, 3, arg0, arg1, arg2);
     glad_glLighti(arg0, arg1, arg2);
    _post_call_callback("glLighti", (void*)glLighti, 3, arg0, arg1, arg2);
    
}
PFNGLLIGHTIPROC glad_debug_glLighti = glad_debug_impl_glLighti;
PFNGLLIGHTIVPROC glad_glLightiv;
void APIENTRY glad_debug_impl_glLightiv(GLenum arg0, GLenum arg1, const GLint * arg2) {    
    _pre_call_callback("glLightiv", (void*)glLightiv, 3, arg0, arg1, arg2);
     glad_glLightiv(arg0, arg1, arg2);
    _post_call_callback("glLightiv", (void*)glLightiv, 3, arg0, arg1, arg2);
    
}
PFNGLLIGHTIVPROC glad_debug_glLightiv = glad_debug_impl_glLightiv;
PFNGLLINESTIPPLEPROC glad_glLineStipple;
void APIENTRY glad_debug_impl_glLineStipple(GLint arg0, GLushort arg1) {    
    _pre_call_callback("glLineStipple", (void*)glLineStipple, 2, arg0, arg1);
     glad_glLineStipple(arg0, arg1);
    _post_call_callback("glLineStipple", (void*)glLineStipple, 2, arg0, arg1);
    
}
PFNGLLINESTIPPLEPROC glad_debug_glLineStipple = glad_debug_impl_glLineStipple;
PFNGLLINEWIDTHPROC glad_glLineWidth;
void APIENTRY glad_debug_impl_glLineWidth(GLfloat arg0) {    
    _pre_call_callback("glLineWidth", (void*)glLineWidth, 1, arg0);
     glad_glLineWidth(arg0);
    _post_call_callback("glLineWidth", (void*)glLineWidth, 1, arg0);
    
}
PFNGLLINEWIDTHPROC glad_debug_glLineWidth = glad_debug_impl_glLineWidth;
PFNGLLINKPROGRAMPROC glad_glLinkProgram;
void APIENTRY glad_debug_impl_glLinkProgram(GLuint arg0) {    
    _pre_call_callback("glLinkProgram", (void*)glLinkProgram, 1, arg0);
     glad_glLinkProgram(arg0);
    _post_call_callback("glLinkProgram", (void*)glLinkProgram, 1, arg0);
    
}
PFNGLLINKPROGRAMPROC glad_debug_glLinkProgram = glad_debug_impl_glLinkProgram;
PFNGLLISTBASEPROC glad_glListBase;
void APIENTRY glad_debug_impl_glListBase(GLuint arg0) {    
    _pre_call_callback("glListBase", (void*)glListBase, 1, arg0);
     glad_glListBase(arg0);
    _post_call_callback("glListBase", (void*)glListBase, 1, arg0);
    
}
PFNGLLISTBASEPROC glad_debug_glListBase = glad_debug_impl_glListBase;
PFNGLLOADIDENTITYPROC glad_glLoadIdentity;
void APIENTRY glad_debug_impl_glLoadIdentity(void) {    
    _pre_call_callback("glLoadIdentity", (void*)glLoadIdentity, 0);
     glad_glLoadIdentity();
    _post_call_callback("glLoadIdentity", (void*)glLoadIdentity, 0);
    
}
PFNGLLOADIDENTITYPROC glad_debug_glLoadIdentity = glad_debug_impl_glLoadIdentity;
PFNGLLOADMATRIXDPROC glad_glLoadMatrixd;
void APIENTRY glad_debug_impl_glLoadMatrixd(const GLdouble * arg0) {    
    _pre_call_callback("glLoadMatrixd", (void*)glLoadMatrixd, 1, arg0);
     glad_glLoadMatrixd(arg0);
    _post_call_callback("glLoadMatrixd", (void*)glLoadMatrixd, 1, arg0);
    
}
PFNGLLOADMATRIXDPROC glad_debug_glLoadMatrixd = glad_debug_impl_glLoadMatrixd;
PFNGLLOADMATRIXFPROC glad_glLoadMatrixf;
void APIENTRY glad_debug_impl_glLoadMatrixf(const GLfloat * arg0) {    
    _pre_call_callback("glLoadMatrixf", (void*)glLoadMatrixf, 1, arg0);
     glad_glLoadMatrixf(arg0);
    _post_call_callback("glLoadMatrixf", (void*)glLoadMatrixf, 1, arg0);
    
}
PFNGLLOADMATRIXFPROC glad_debug_glLoadMatrixf = glad_debug_impl_glLoadMatrixf;
PFNGLLOADNAMEPROC glad_glLoadName;
void APIENTRY glad_debug_impl_glLoadName(GLuint arg0) {    
    _pre_call_callback("glLoadName", (void*)glLoadName, 1, arg0);
     glad_glLoadName(arg0);
    _post_call_callback("glLoadName", (void*)glLoadName, 1, arg0);
    
}
PFNGLLOADNAMEPROC glad_debug_glLoadName = glad_debug_impl_glLoadName;
PFNGLLOADTRANSPOSEMATRIXDPROC glad_glLoadTransposeMatrixd;
void APIENTRY glad_debug_impl_glLoadTransposeMatrixd(const GLdouble * arg0) {    
    _pre_call_callback("glLoadTransposeMatrixd", (void*)glLoadTransposeMatrixd, 1, arg0);
     glad_glLoadTransposeMatrixd(arg0);
    _post_call_callback("glLoadTransposeMatrixd", (void*)glLoadTransposeMatrixd, 1, arg0);
    
}
PFNGLLOADTRANSPOSEMATRIXDPROC glad_debug_glLoadTransposeMatrixd = glad_debug_impl_glLoadTransposeMatrixd;
PFNGLLOADTRANSPOSEMATRIXFPROC glad_glLoadTransposeMatrixf;
void APIENTRY glad_debug_impl_glLoadTransposeMatrixf(const GLfloat * arg0) {    
    _pre_call_callback("glLoadTransposeMatrixf", (void*)glLoadTransposeMatrixf, 1, arg0);
     glad_glLoadTransposeMatrixf(arg0);
    _post_call_callback("glLoadTransposeMatrixf", (void*)glLoadTransposeMatrixf, 1, arg0);
    
}
PFNGLLOADTRANSPOSEMATRIXFPROC glad_debug_glLoadTransposeMatrixf = glad_debug_impl_glLoadTransposeMatrixf;
PFNGLLOGICOPPROC glad_glLogicOp;
void APIENTRY glad_debug_impl_glLogicOp(GLenum arg0) {    
    _pre_call_callback("glLogicOp", (void*)glLogicOp, 1, arg0);
     glad_glLogicOp(arg0);
    _post_call_callback("glLogicOp", (void*)glLogicOp, 1, arg0);
    
}
PFNGLLOGICOPPROC glad_debug_glLogicOp = glad_debug_impl_glLogicOp;
PFNGLMAP1DPROC glad_glMap1d;
void APIENTRY glad_debug_impl_glMap1d(GLenum arg0, GLdouble arg1, GLdouble arg2, GLint arg3, GLint arg4, const GLdouble * arg5) {    
    _pre_call_callback("glMap1d", (void*)glMap1d, 6, arg0, arg1, arg2, arg3, arg4, arg5);
     glad_glMap1d(arg0, arg1, arg2, arg3, arg4, arg5);
    _post_call_callback("glMap1d", (void*)glMap1d, 6, arg0, arg1, arg2, arg3, arg4, arg5);
    
}
PFNGLMAP1DPROC glad_debug_glMap1d = glad_debug_impl_glMap1d;
PFNGLMAP1FPROC glad_glMap1f;
void APIENTRY glad_debug_impl_glMap1f(GLenum arg0, GLfloat arg1, GLfloat arg2, GLint arg3, GLint arg4, const GLfloat * arg5) {    
    _pre_call_callback("glMap1f", (void*)glMap1f, 6, arg0, arg1, arg2, arg3, arg4, arg5);
     glad_glMap1f(arg0, arg1, arg2, arg3, arg4, arg5);
    _post_call_callback("glMap1f", (void*)glMap1f, 6, arg0, arg1, arg2, arg3, arg4, arg5);
    
}
PFNGLMAP1FPROC glad_debug_glMap1f = glad_debug_impl_glMap1f;
PFNGLMAP2DPROC glad_glMap2d;
void APIENTRY glad_debug_impl_glMap2d(GLenum arg0, GLdouble arg1, GLdouble arg2, GLint arg3, GLint arg4, GLdouble arg5, GLdouble arg6, GLint arg7, GLint arg8, const GLdouble * arg9) {    
    _pre_call_callback("glMap2d", (void*)glMap2d, 10, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
     glad_glMap2d(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
    _post_call_callback("glMap2d", (void*)glMap2d, 10, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
    
}
PFNGLMAP2DPROC glad_debug_glMap2d = glad_debug_impl_glMap2d;
PFNGLMAP2FPROC glad_glMap2f;
void APIENTRY glad_debug_impl_glMap2f(GLenum arg0, GLfloat arg1, GLfloat arg2, GLint arg3, GLint arg4, GLfloat arg5, GLfloat arg6, GLint arg7, GLint arg8, const GLfloat * arg9) {    
    _pre_call_callback("glMap2f", (void*)glMap2f, 10, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
     glad_glMap2f(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
    _post_call_callback("glMap2f", (void*)glMap2f, 10, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
    
}
PFNGLMAP2FPROC glad_debug_glMap2f = glad_debug_impl_glMap2f;
PFNGLMAPBUFFERPROC glad_glMapBuffer;
void * APIENTRY glad_debug_impl_glMapBuffer(GLenum arg0, GLenum arg1) {    
    void * ret;
    _pre_call_callback("glMapBuffer", (void*)glMapBuffer, 2, arg0, arg1);
    ret =  glad_glMapBuffer(arg0, arg1);
    _post_call_callback("glMapBuffer", (void*)glMapBuffer, 2, arg0, arg1);
    return ret;
}
PFNGLMAPBUFFERPROC glad_debug_glMapBuffer = glad_debug_impl_glMapBuffer;
PFNGLMAPBUFFERRANGEPROC glad_glMapBufferRange;
void * APIENTRY glad_debug_impl_glMapBufferRange(GLenum arg0, GLintptr arg1, GLsizeiptr arg2, GLbitfield arg3) {    
    void * ret;
    _pre_call_callback("glMapBufferRange", (void*)glMapBufferRange, 4, arg0, arg1, arg2, arg3);
    ret =  glad_glMapBufferRange(arg0, arg1, arg2, arg3);
    _post_call_callback("glMapBufferRange", (void*)glMapBufferRange, 4, arg0, arg1, arg2, arg3);
    return ret;
}
PFNGLMAPBUFFERRANGEPROC glad_debug_glMapBufferRange = glad_debug_impl_glMapBufferRange;
PFNGLMAPGRID1DPROC glad_glMapGrid1d;
void APIENTRY glad_debug_impl_glMapGrid1d(GLint arg0, GLdouble arg1, GLdouble arg2) {    
    _pre_call_callback("glMapGrid1d", (void*)glMapGrid1d, 3, arg0, arg1, arg2);
     glad_glMapGrid1d(arg0, arg1, arg2);
    _post_call_callback("glMapGrid1d", (void*)glMapGrid1d, 3, arg0, arg1, arg2);
    
}
PFNGLMAPGRID1DPROC glad_debug_glMapGrid1d = glad_debug_impl_glMapGrid1d;
PFNGLMAPGRID1FPROC glad_glMapGrid1f;
void APIENTRY glad_debug_impl_glMapGrid1f(GLint arg0, GLfloat arg1, GLfloat arg2) {    
    _pre_call_callback("glMapGrid1f", (void*)glMapGrid1f, 3, arg0, arg1, arg2);
     glad_glMapGrid1f(arg0, arg1, arg2);
    _post_call_callback("glMapGrid1f", (void*)glMapGrid1f, 3, arg0, arg1, arg2);
    
}
PFNGLMAPGRID1FPROC glad_debug_glMapGrid1f = glad_debug_impl_glMapGrid1f;
PFNGLMAPGRID2DPROC glad_glMapGrid2d;
void APIENTRY glad_debug_impl_glMapGrid2d(GLint arg0, GLdouble arg1, GLdouble arg2, GLint arg3, GLdouble arg4, GLdouble arg5) {    
    _pre_call_callback("glMapGrid2d", (void*)glMapGrid2d, 6, arg0, arg1, arg2, arg3, arg4, arg5);
     glad_glMapGrid2d(arg0, arg1, arg2, arg3, arg4, arg5);
    _post_call_callback("glMapGrid2d", (void*)glMapGrid2d, 6, arg0, arg1, arg2, arg3, arg4, arg5);
    
}
PFNGLMAPGRID2DPROC glad_debug_glMapGrid2d = glad_debug_impl_glMapGrid2d;
PFNGLMAPGRID2FPROC glad_glMapGrid2f;
void APIENTRY glad_debug_impl_glMapGrid2f(GLint arg0, GLfloat arg1, GLfloat arg2, GLint arg3, GLfloat arg4, GLfloat arg5) {    
    _pre_call_callback("glMapGrid2f", (void*)glMapGrid2f, 6, arg0, arg1, arg2, arg3, arg4, arg5);
     glad_glMapGrid2f(arg0, arg1, arg2, arg3, arg4, arg5);
    _post_call_callback("glMapGrid2f", (void*)glMapGrid2f, 6, arg0, arg1, arg2, arg3, arg4, arg5);
    
}
PFNGLMAPGRID2FPROC glad_debug_glMapGrid2f = glad_debug_impl_glMapGrid2f;
PFNGLMAPNAMEDBUFFERPROC glad_glMapNamedBuffer;
void * APIENTRY glad_debug_impl_glMapNamedBuffer(GLuint arg0, GLenum arg1) {    
    void * ret;
    _pre_call_callback("glMapNamedBuffer", (void*)glMapNamedBuffer, 2, arg0, arg1);
    ret =  glad_glMapNamedBuffer(arg0, arg1);
    _post_call_callback("glMapNamedBuffer", (void*)glMapNamedBuffer, 2, arg0, arg1);
    return ret;
}
PFNGLMAPNAMEDBUFFERPROC glad_debug_glMapNamedBuffer = glad_debug_impl_glMapNamedBuffer;
PFNGLMAPNAMEDBUFFERRANGEPROC glad_glMapNamedBufferRange;
void * APIENTRY glad_debug_impl_glMapNamedBufferRange(GLuint arg0, GLintptr arg1, GLsizeiptr arg2, GLbitfield arg3) {    
    void * ret;
    _pre_call_callback("glMapNamedBufferRange", (void*)glMapNamedBufferRange, 4, arg0, arg1, arg2, arg3);
    ret =  glad_glMapNamedBufferRange(arg0, arg1, arg2, arg3);
    _post_call_callback("glMapNamedBufferRange", (void*)glMapNamedBufferRange, 4, arg0, arg1, arg2, arg3);
    return ret;
}
PFNGLMAPNAMEDBUFFERRANGEPROC glad_debug_glMapNamedBufferRange = glad_debug_impl_glMapNamedBufferRange;
PFNGLMATERIALFPROC glad_glMaterialf;
void APIENTRY glad_debug_impl_glMaterialf(GLenum arg0, GLenum arg1, GLfloat arg2) {    
    _pre_call_callback("glMaterialf", (void*)glMaterialf, 3, arg0, arg1, arg2);
     glad_glMaterialf(arg0, arg1, arg2);
    _post_call_callback("glMaterialf", (void*)glMaterialf, 3, arg0, arg1, arg2);
    
}
PFNGLMATERIALFPROC glad_debug_glMaterialf = glad_debug_impl_glMaterialf;
PFNGLMATERIALFVPROC glad_glMaterialfv;
void APIENTRY glad_debug_impl_glMaterialfv(GLenum arg0, GLenum arg1, const GLfloat * arg2) {    
    _pre_call_callback("glMaterialfv", (void*)glMaterialfv, 3, arg0, arg1, arg2);
     glad_glMaterialfv(arg0, arg1, arg2);
    _post_call_callback("glMaterialfv", (void*)glMaterialfv, 3, arg0, arg1, arg2);
    
}
PFNGLMATERIALFVPROC glad_debug_glMaterialfv = glad_debug_impl_glMaterialfv;
PFNGLMATERIALIPROC glad_glMateriali;
void APIENTRY glad_debug_impl_glMateriali(GLenum arg0, GLenum arg1, GLint arg2) {    
    _pre_call_callback("glMateriali", (void*)glMateriali, 3, arg0, arg1, arg2);
     glad_glMateriali(arg0, arg1, arg2);
    _post_call_callback("glMateriali", (void*)glMateriali, 3, arg0, arg1, arg2);
    
}
PFNGLMATERIALIPROC glad_debug_glMateriali = glad_debug_impl_glMateriali;
PFNGLMATERIALIVPROC glad_glMaterialiv;
void APIENTRY glad_debug_impl_glMaterialiv(GLenum arg0, GLenum arg1, const GLint * arg2) {    
    _pre_call_callback("glMaterialiv", (void*)glMaterialiv, 3, arg0, arg1, arg2);
     glad_glMaterialiv(arg0, arg1, arg2);
    _post_call_callback("glMaterialiv", (void*)glMaterialiv, 3, arg0, arg1, arg2);
    
}
PFNGLMATERIALIVPROC glad_debug_glMaterialiv = glad_debug_impl_glMaterialiv;
PFNGLMATRIXMODEPROC glad_glMatrixMode;
void APIENTRY glad_debug_impl_glMatrixMode(GLenum arg0) {    
    _pre_call_callback("glMatrixMode", (void*)glMatrixMode, 1, arg0);
     glad_glMatrixMode(arg0);
    _post_call_callback("glMatrixMode", (void*)glMatrixMode, 1, arg0);
    
}
PFNGLMATRIXMODEPROC glad_debug_glMatrixMode = glad_debug_impl_glMatrixMode;
PFNGLMEMORYBARRIERPROC glad_glMemoryBarrier;
void APIENTRY glad_debug_impl_glMemoryBarrier(GLbitfield arg0) {    
    _pre_call_callback("glMemoryBarrier", (void*)glMemoryBarrier, 1, arg0);
     glad_glMemoryBarrier(arg0);
    _post_call_callback("glMemoryBarrier", (void*)glMemoryBarrier, 1, arg0);
    
}
PFNGLMEMORYBARRIERPROC glad_debug_glMemoryBarrier = glad_debug_impl_glMemoryBarrier;
PFNGLMEMORYBARRIERBYREGIONPROC glad_glMemoryBarrierByRegion;
void APIENTRY glad_debug_impl_glMemoryBarrierByRegion(GLbitfield arg0) {    
    _pre_call_callback("glMemoryBarrierByRegion", (void*)glMemoryBarrierByRegion, 1, arg0);
     glad_glMemoryBarrierByRegion(arg0);
    _post_call_callback("glMemoryBarrierByRegion", (void*)glMemoryBarrierByRegion, 1, arg0);
    
}
PFNGLMEMORYBARRIERBYREGIONPROC glad_debug_glMemoryBarrierByRegion = glad_debug_impl_glMemoryBarrierByRegion;
PFNGLMINSAMPLESHADINGPROC glad_glMinSampleShading;
void APIENTRY glad_debug_impl_glMinSampleShading(GLfloat arg0) {    
    _pre_call_callback("glMinSampleShading", (void*)glMinSampleShading, 1, arg0);
     glad_glMinSampleShading(arg0);
    _post_call_callback("glMinSampleShading", (void*)glMinSampleShading, 1, arg0);
    
}
PFNGLMINSAMPLESHADINGPROC glad_debug_glMinSampleShading = glad_debug_impl_glMinSampleShading;
PFNGLMULTMATRIXDPROC glad_glMultMatrixd;
void APIENTRY glad_debug_impl_glMultMatrixd(const GLdouble * arg0) {    
    _pre_call_callback("glMultMatrixd", (void*)glMultMatrixd, 1, arg0);
     glad_glMultMatrixd(arg0);
    _post_call_callback("glMultMatrixd", (void*)glMultMatrixd, 1, arg0);
    
}
PFNGLMULTMATRIXDPROC glad_debug_glMultMatrixd = glad_debug_impl_glMultMatrixd;
PFNGLMULTMATRIXFPROC glad_glMultMatrixf;
void APIENTRY glad_debug_impl_glMultMatrixf(const GLfloat * arg0) {    
    _pre_call_callback("glMultMatrixf", (void*)glMultMatrixf, 1, arg0);
     glad_glMultMatrixf(arg0);
    _post_call_callback("glMultMatrixf", (void*)glMultMatrixf, 1, arg0);
    
}
PFNGLMULTMATRIXFPROC glad_debug_glMultMatrixf = glad_debug_impl_glMultMatrixf;
PFNGLMULTTRANSPOSEMATRIXDPROC glad_glMultTransposeMatrixd;
void APIENTRY glad_debug_impl_glMultTransposeMatrixd(const GLdouble * arg0) {    
    _pre_call_callback("glMultTransposeMatrixd", (void*)glMultTransposeMatrixd, 1, arg0);
     glad_glMultTransposeMatrixd(arg0);
    _post_call_callback("glMultTransposeMatrixd", (void*)glMultTransposeMatrixd, 1, arg0);
    
}
PFNGLMULTTRANSPOSEMATRIXDPROC glad_debug_glMultTransposeMatrixd = glad_debug_impl_glMultTransposeMatrixd;
PFNGLMULTTRANSPOSEMATRIXFPROC glad_glMultTransposeMatrixf;
void APIENTRY glad_debug_impl_glMultTransposeMatrixf(const GLfloat * arg0) {    
    _pre_call_callback("glMultTransposeMatrixf", (void*)glMultTransposeMatrixf, 1, arg0);
     glad_glMultTransposeMatrixf(arg0);
    _post_call_callback("glMultTransposeMatrixf", (void*)glMultTransposeMatrixf, 1, arg0);
    
}
PFNGLMULTTRANSPOSEMATRIXFPROC glad_debug_glMultTransposeMatrixf = glad_debug_impl_glMultTransposeMatrixf;
PFNGLMULTIDRAWARRAYSPROC glad_glMultiDrawArrays;
void APIENTRY glad_debug_impl_glMultiDrawArrays(GLenum arg0, const GLint * arg1, const GLsizei * arg2, GLsizei arg3) {    
    _pre_call_callback("glMultiDrawArrays", (void*)glMultiDrawArrays, 4, arg0, arg1, arg2, arg3);
     glad_glMultiDrawArrays(arg0, arg1, arg2, arg3);
    _post_call_callback("glMultiDrawArrays", (void*)glMultiDrawArrays, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLMULTIDRAWARRAYSPROC glad_debug_glMultiDrawArrays = glad_debug_impl_glMultiDrawArrays;
PFNGLMULTIDRAWARRAYSINDIRECTPROC glad_glMultiDrawArraysIndirect;
void APIENTRY glad_debug_impl_glMultiDrawArraysIndirect(GLenum arg0, const void * arg1, GLsizei arg2, GLsizei arg3) {    
    _pre_call_callback("glMultiDrawArraysIndirect", (void*)glMultiDrawArraysIndirect, 4, arg0, arg1, arg2, arg3);
     glad_glMultiDrawArraysIndirect(arg0, arg1, arg2, arg3);
    _post_call_callback("glMultiDrawArraysIndirect", (void*)glMultiDrawArraysIndirect, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLMULTIDRAWARRAYSINDIRECTPROC glad_debug_glMultiDrawArraysIndirect = glad_debug_impl_glMultiDrawArraysIndirect;
PFNGLMULTIDRAWELEMENTSPROC glad_glMultiDrawElements;
void APIENTRY glad_debug_impl_glMultiDrawElements(GLenum arg0, const GLsizei * arg1, GLenum arg2, const void *const* arg3, GLsizei arg4) {    
    _pre_call_callback("glMultiDrawElements", (void*)glMultiDrawElements, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glMultiDrawElements(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glMultiDrawElements", (void*)glMultiDrawElements, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLMULTIDRAWELEMENTSPROC glad_debug_glMultiDrawElements = glad_debug_impl_glMultiDrawElements;
PFNGLMULTIDRAWELEMENTSBASEVERTEXPROC glad_glMultiDrawElementsBaseVertex;
void APIENTRY glad_debug_impl_glMultiDrawElementsBaseVertex(GLenum arg0, const GLsizei * arg1, GLenum arg2, const void *const* arg3, GLsizei arg4, const GLint * arg5) {    
    _pre_call_callback("glMultiDrawElementsBaseVertex", (void*)glMultiDrawElementsBaseVertex, 6, arg0, arg1, arg2, arg3, arg4, arg5);
     glad_glMultiDrawElementsBaseVertex(arg0, arg1, arg2, arg3, arg4, arg5);
    _post_call_callback("glMultiDrawElementsBaseVertex", (void*)glMultiDrawElementsBaseVertex, 6, arg0, arg1, arg2, arg3, arg4, arg5);
    
}
PFNGLMULTIDRAWELEMENTSBASEVERTEXPROC glad_debug_glMultiDrawElementsBaseVertex = glad_debug_impl_glMultiDrawElementsBaseVertex;
PFNGLMULTIDRAWELEMENTSINDIRECTPROC glad_glMultiDrawElementsIndirect;
void APIENTRY glad_debug_impl_glMultiDrawElementsIndirect(GLenum arg0, GLenum arg1, const void * arg2, GLsizei arg3, GLsizei arg4) {    
    _pre_call_callback("glMultiDrawElementsIndirect", (void*)glMultiDrawElementsIndirect, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glMultiDrawElementsIndirect(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glMultiDrawElementsIndirect", (void*)glMultiDrawElementsIndirect, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLMULTIDRAWELEMENTSINDIRECTPROC glad_debug_glMultiDrawElementsIndirect = glad_debug_impl_glMultiDrawElementsIndirect;
PFNGLMULTITEXCOORD1DPROC glad_glMultiTexCoord1d;
void APIENTRY glad_debug_impl_glMultiTexCoord1d(GLenum arg0, GLdouble arg1) {    
    _pre_call_callback("glMultiTexCoord1d", (void*)glMultiTexCoord1d, 2, arg0, arg1);
     glad_glMultiTexCoord1d(arg0, arg1);
    _post_call_callback("glMultiTexCoord1d", (void*)glMultiTexCoord1d, 2, arg0, arg1);
    
}
PFNGLMULTITEXCOORD1DPROC glad_debug_glMultiTexCoord1d = glad_debug_impl_glMultiTexCoord1d;
PFNGLMULTITEXCOORD1DVPROC glad_glMultiTexCoord1dv;
void APIENTRY glad_debug_impl_glMultiTexCoord1dv(GLenum arg0, const GLdouble * arg1) {    
    _pre_call_callback("glMultiTexCoord1dv", (void*)glMultiTexCoord1dv, 2, arg0, arg1);
     glad_glMultiTexCoord1dv(arg0, arg1);
    _post_call_callback("glMultiTexCoord1dv", (void*)glMultiTexCoord1dv, 2, arg0, arg1);
    
}
PFNGLMULTITEXCOORD1DVPROC glad_debug_glMultiTexCoord1dv = glad_debug_impl_glMultiTexCoord1dv;
PFNGLMULTITEXCOORD1FPROC glad_glMultiTexCoord1f;
void APIENTRY glad_debug_impl_glMultiTexCoord1f(GLenum arg0, GLfloat arg1) {    
    _pre_call_callback("glMultiTexCoord1f", (void*)glMultiTexCoord1f, 2, arg0, arg1);
     glad_glMultiTexCoord1f(arg0, arg1);
    _post_call_callback("glMultiTexCoord1f", (void*)glMultiTexCoord1f, 2, arg0, arg1);
    
}
PFNGLMULTITEXCOORD1FPROC glad_debug_glMultiTexCoord1f = glad_debug_impl_glMultiTexCoord1f;
PFNGLMULTITEXCOORD1FVPROC glad_glMultiTexCoord1fv;
void APIENTRY glad_debug_impl_glMultiTexCoord1fv(GLenum arg0, const GLfloat * arg1) {    
    _pre_call_callback("glMultiTexCoord1fv", (void*)glMultiTexCoord1fv, 2, arg0, arg1);
     glad_glMultiTexCoord1fv(arg0, arg1);
    _post_call_callback("glMultiTexCoord1fv", (void*)glMultiTexCoord1fv, 2, arg0, arg1);
    
}
PFNGLMULTITEXCOORD1FVPROC glad_debug_glMultiTexCoord1fv = glad_debug_impl_glMultiTexCoord1fv;
PFNGLMULTITEXCOORD1IPROC glad_glMultiTexCoord1i;
void APIENTRY glad_debug_impl_glMultiTexCoord1i(GLenum arg0, GLint arg1) {    
    _pre_call_callback("glMultiTexCoord1i", (void*)glMultiTexCoord1i, 2, arg0, arg1);
     glad_glMultiTexCoord1i(arg0, arg1);
    _post_call_callback("glMultiTexCoord1i", (void*)glMultiTexCoord1i, 2, arg0, arg1);
    
}
PFNGLMULTITEXCOORD1IPROC glad_debug_glMultiTexCoord1i = glad_debug_impl_glMultiTexCoord1i;
PFNGLMULTITEXCOORD1IVPROC glad_glMultiTexCoord1iv;
void APIENTRY glad_debug_impl_glMultiTexCoord1iv(GLenum arg0, const GLint * arg1) {    
    _pre_call_callback("glMultiTexCoord1iv", (void*)glMultiTexCoord1iv, 2, arg0, arg1);
     glad_glMultiTexCoord1iv(arg0, arg1);
    _post_call_callback("glMultiTexCoord1iv", (void*)glMultiTexCoord1iv, 2, arg0, arg1);
    
}
PFNGLMULTITEXCOORD1IVPROC glad_debug_glMultiTexCoord1iv = glad_debug_impl_glMultiTexCoord1iv;
PFNGLMULTITEXCOORD1SPROC glad_glMultiTexCoord1s;
void APIENTRY glad_debug_impl_glMultiTexCoord1s(GLenum arg0, GLshort arg1) {    
    _pre_call_callback("glMultiTexCoord1s", (void*)glMultiTexCoord1s, 2, arg0, arg1);
     glad_glMultiTexCoord1s(arg0, arg1);
    _post_call_callback("glMultiTexCoord1s", (void*)glMultiTexCoord1s, 2, arg0, arg1);
    
}
PFNGLMULTITEXCOORD1SPROC glad_debug_glMultiTexCoord1s = glad_debug_impl_glMultiTexCoord1s;
PFNGLMULTITEXCOORD1SVPROC glad_glMultiTexCoord1sv;
void APIENTRY glad_debug_impl_glMultiTexCoord1sv(GLenum arg0, const GLshort * arg1) {    
    _pre_call_callback("glMultiTexCoord1sv", (void*)glMultiTexCoord1sv, 2, arg0, arg1);
     glad_glMultiTexCoord1sv(arg0, arg1);
    _post_call_callback("glMultiTexCoord1sv", (void*)glMultiTexCoord1sv, 2, arg0, arg1);
    
}
PFNGLMULTITEXCOORD1SVPROC glad_debug_glMultiTexCoord1sv = glad_debug_impl_glMultiTexCoord1sv;
PFNGLMULTITEXCOORD2DPROC glad_glMultiTexCoord2d;
void APIENTRY glad_debug_impl_glMultiTexCoord2d(GLenum arg0, GLdouble arg1, GLdouble arg2) {    
    _pre_call_callback("glMultiTexCoord2d", (void*)glMultiTexCoord2d, 3, arg0, arg1, arg2);
     glad_glMultiTexCoord2d(arg0, arg1, arg2);
    _post_call_callback("glMultiTexCoord2d", (void*)glMultiTexCoord2d, 3, arg0, arg1, arg2);
    
}
PFNGLMULTITEXCOORD2DPROC glad_debug_glMultiTexCoord2d = glad_debug_impl_glMultiTexCoord2d;
PFNGLMULTITEXCOORD2DVPROC glad_glMultiTexCoord2dv;
void APIENTRY glad_debug_impl_glMultiTexCoord2dv(GLenum arg0, const GLdouble * arg1) {    
    _pre_call_callback("glMultiTexCoord2dv", (void*)glMultiTexCoord2dv, 2, arg0, arg1);
     glad_glMultiTexCoord2dv(arg0, arg1);
    _post_call_callback("glMultiTexCoord2dv", (void*)glMultiTexCoord2dv, 2, arg0, arg1);
    
}
PFNGLMULTITEXCOORD2DVPROC glad_debug_glMultiTexCoord2dv = glad_debug_impl_glMultiTexCoord2dv;
PFNGLMULTITEXCOORD2FPROC glad_glMultiTexCoord2f;
void APIENTRY glad_debug_impl_glMultiTexCoord2f(GLenum arg0, GLfloat arg1, GLfloat arg2) {    
    _pre_call_callback("glMultiTexCoord2f", (void*)glMultiTexCoord2f, 3, arg0, arg1, arg2);
     glad_glMultiTexCoord2f(arg0, arg1, arg2);
    _post_call_callback("glMultiTexCoord2f", (void*)glMultiTexCoord2f, 3, arg0, arg1, arg2);
    
}
PFNGLMULTITEXCOORD2FPROC glad_debug_glMultiTexCoord2f = glad_debug_impl_glMultiTexCoord2f;
PFNGLMULTITEXCOORD2FVPROC glad_glMultiTexCoord2fv;
void APIENTRY glad_debug_impl_glMultiTexCoord2fv(GLenum arg0, const GLfloat * arg1) {    
    _pre_call_callback("glMultiTexCoord2fv", (void*)glMultiTexCoord2fv, 2, arg0, arg1);
     glad_glMultiTexCoord2fv(arg0, arg1);
    _post_call_callback("glMultiTexCoord2fv", (void*)glMultiTexCoord2fv, 2, arg0, arg1);
    
}
PFNGLMULTITEXCOORD2FVPROC glad_debug_glMultiTexCoord2fv = glad_debug_impl_glMultiTexCoord2fv;
PFNGLMULTITEXCOORD2IPROC glad_glMultiTexCoord2i;
void APIENTRY glad_debug_impl_glMultiTexCoord2i(GLenum arg0, GLint arg1, GLint arg2) {    
    _pre_call_callback("glMultiTexCoord2i", (void*)glMultiTexCoord2i, 3, arg0, arg1, arg2);
     glad_glMultiTexCoord2i(arg0, arg1, arg2);
    _post_call_callback("glMultiTexCoord2i", (void*)glMultiTexCoord2i, 3, arg0, arg1, arg2);
    
}
PFNGLMULTITEXCOORD2IPROC glad_debug_glMultiTexCoord2i = glad_debug_impl_glMultiTexCoord2i;
PFNGLMULTITEXCOORD2IVPROC glad_glMultiTexCoord2iv;
void APIENTRY glad_debug_impl_glMultiTexCoord2iv(GLenum arg0, const GLint * arg1) {    
    _pre_call_callback("glMultiTexCoord2iv", (void*)glMultiTexCoord2iv, 2, arg0, arg1);
     glad_glMultiTexCoord2iv(arg0, arg1);
    _post_call_callback("glMultiTexCoord2iv", (void*)glMultiTexCoord2iv, 2, arg0, arg1);
    
}
PFNGLMULTITEXCOORD2IVPROC glad_debug_glMultiTexCoord2iv = glad_debug_impl_glMultiTexCoord2iv;
PFNGLMULTITEXCOORD2SPROC glad_glMultiTexCoord2s;
void APIENTRY glad_debug_impl_glMultiTexCoord2s(GLenum arg0, GLshort arg1, GLshort arg2) {    
    _pre_call_callback("glMultiTexCoord2s", (void*)glMultiTexCoord2s, 3, arg0, arg1, arg2);
     glad_glMultiTexCoord2s(arg0, arg1, arg2);
    _post_call_callback("glMultiTexCoord2s", (void*)glMultiTexCoord2s, 3, arg0, arg1, arg2);
    
}
PFNGLMULTITEXCOORD2SPROC glad_debug_glMultiTexCoord2s = glad_debug_impl_glMultiTexCoord2s;
PFNGLMULTITEXCOORD2SVPROC glad_glMultiTexCoord2sv;
void APIENTRY glad_debug_impl_glMultiTexCoord2sv(GLenum arg0, const GLshort * arg1) {    
    _pre_call_callback("glMultiTexCoord2sv", (void*)glMultiTexCoord2sv, 2, arg0, arg1);
     glad_glMultiTexCoord2sv(arg0, arg1);
    _post_call_callback("glMultiTexCoord2sv", (void*)glMultiTexCoord2sv, 2, arg0, arg1);
    
}
PFNGLMULTITEXCOORD2SVPROC glad_debug_glMultiTexCoord2sv = glad_debug_impl_glMultiTexCoord2sv;
PFNGLMULTITEXCOORD3DPROC glad_glMultiTexCoord3d;
void APIENTRY glad_debug_impl_glMultiTexCoord3d(GLenum arg0, GLdouble arg1, GLdouble arg2, GLdouble arg3) {    
    _pre_call_callback("glMultiTexCoord3d", (void*)glMultiTexCoord3d, 4, arg0, arg1, arg2, arg3);
     glad_glMultiTexCoord3d(arg0, arg1, arg2, arg3);
    _post_call_callback("glMultiTexCoord3d", (void*)glMultiTexCoord3d, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLMULTITEXCOORD3DPROC glad_debug_glMultiTexCoord3d = glad_debug_impl_glMultiTexCoord3d;
PFNGLMULTITEXCOORD3DVPROC glad_glMultiTexCoord3dv;
void APIENTRY glad_debug_impl_glMultiTexCoord3dv(GLenum arg0, const GLdouble * arg1) {    
    _pre_call_callback("glMultiTexCoord3dv", (void*)glMultiTexCoord3dv, 2, arg0, arg1);
     glad_glMultiTexCoord3dv(arg0, arg1);
    _post_call_callback("glMultiTexCoord3dv", (void*)glMultiTexCoord3dv, 2, arg0, arg1);
    
}
PFNGLMULTITEXCOORD3DVPROC glad_debug_glMultiTexCoord3dv = glad_debug_impl_glMultiTexCoord3dv;
PFNGLMULTITEXCOORD3FPROC glad_glMultiTexCoord3f;
void APIENTRY glad_debug_impl_glMultiTexCoord3f(GLenum arg0, GLfloat arg1, GLfloat arg2, GLfloat arg3) {    
    _pre_call_callback("glMultiTexCoord3f", (void*)glMultiTexCoord3f, 4, arg0, arg1, arg2, arg3);
     glad_glMultiTexCoord3f(arg0, arg1, arg2, arg3);
    _post_call_callback("glMultiTexCoord3f", (void*)glMultiTexCoord3f, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLMULTITEXCOORD3FPROC glad_debug_glMultiTexCoord3f = glad_debug_impl_glMultiTexCoord3f;
PFNGLMULTITEXCOORD3FVPROC glad_glMultiTexCoord3fv;
void APIENTRY glad_debug_impl_glMultiTexCoord3fv(GLenum arg0, const GLfloat * arg1) {    
    _pre_call_callback("glMultiTexCoord3fv", (void*)glMultiTexCoord3fv, 2, arg0, arg1);
     glad_glMultiTexCoord3fv(arg0, arg1);
    _post_call_callback("glMultiTexCoord3fv", (void*)glMultiTexCoord3fv, 2, arg0, arg1);
    
}
PFNGLMULTITEXCOORD3FVPROC glad_debug_glMultiTexCoord3fv = glad_debug_impl_glMultiTexCoord3fv;
PFNGLMULTITEXCOORD3IPROC glad_glMultiTexCoord3i;
void APIENTRY glad_debug_impl_glMultiTexCoord3i(GLenum arg0, GLint arg1, GLint arg2, GLint arg3) {    
    _pre_call_callback("glMultiTexCoord3i", (void*)glMultiTexCoord3i, 4, arg0, arg1, arg2, arg3);
     glad_glMultiTexCoord3i(arg0, arg1, arg2, arg3);
    _post_call_callback("glMultiTexCoord3i", (void*)glMultiTexCoord3i, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLMULTITEXCOORD3IPROC glad_debug_glMultiTexCoord3i = glad_debug_impl_glMultiTexCoord3i;
PFNGLMULTITEXCOORD3IVPROC glad_glMultiTexCoord3iv;
void APIENTRY glad_debug_impl_glMultiTexCoord3iv(GLenum arg0, const GLint * arg1) {    
    _pre_call_callback("glMultiTexCoord3iv", (void*)glMultiTexCoord3iv, 2, arg0, arg1);
     glad_glMultiTexCoord3iv(arg0, arg1);
    _post_call_callback("glMultiTexCoord3iv", (void*)glMultiTexCoord3iv, 2, arg0, arg1);
    
}
PFNGLMULTITEXCOORD3IVPROC glad_debug_glMultiTexCoord3iv = glad_debug_impl_glMultiTexCoord3iv;
PFNGLMULTITEXCOORD3SPROC glad_glMultiTexCoord3s;
void APIENTRY glad_debug_impl_glMultiTexCoord3s(GLenum arg0, GLshort arg1, GLshort arg2, GLshort arg3) {    
    _pre_call_callback("glMultiTexCoord3s", (void*)glMultiTexCoord3s, 4, arg0, arg1, arg2, arg3);
     glad_glMultiTexCoord3s(arg0, arg1, arg2, arg3);
    _post_call_callback("glMultiTexCoord3s", (void*)glMultiTexCoord3s, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLMULTITEXCOORD3SPROC glad_debug_glMultiTexCoord3s = glad_debug_impl_glMultiTexCoord3s;
PFNGLMULTITEXCOORD3SVPROC glad_glMultiTexCoord3sv;
void APIENTRY glad_debug_impl_glMultiTexCoord3sv(GLenum arg0, const GLshort * arg1) {    
    _pre_call_callback("glMultiTexCoord3sv", (void*)glMultiTexCoord3sv, 2, arg0, arg1);
     glad_glMultiTexCoord3sv(arg0, arg1);
    _post_call_callback("glMultiTexCoord3sv", (void*)glMultiTexCoord3sv, 2, arg0, arg1);
    
}
PFNGLMULTITEXCOORD3SVPROC glad_debug_glMultiTexCoord3sv = glad_debug_impl_glMultiTexCoord3sv;
PFNGLMULTITEXCOORD4DPROC glad_glMultiTexCoord4d;
void APIENTRY glad_debug_impl_glMultiTexCoord4d(GLenum arg0, GLdouble arg1, GLdouble arg2, GLdouble arg3, GLdouble arg4) {    
    _pre_call_callback("glMultiTexCoord4d", (void*)glMultiTexCoord4d, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glMultiTexCoord4d(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glMultiTexCoord4d", (void*)glMultiTexCoord4d, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLMULTITEXCOORD4DPROC glad_debug_glMultiTexCoord4d = glad_debug_impl_glMultiTexCoord4d;
PFNGLMULTITEXCOORD4DVPROC glad_glMultiTexCoord4dv;
void APIENTRY glad_debug_impl_glMultiTexCoord4dv(GLenum arg0, const GLdouble * arg1) {    
    _pre_call_callback("glMultiTexCoord4dv", (void*)glMultiTexCoord4dv, 2, arg0, arg1);
     glad_glMultiTexCoord4dv(arg0, arg1);
    _post_call_callback("glMultiTexCoord4dv", (void*)glMultiTexCoord4dv, 2, arg0, arg1);
    
}
PFNGLMULTITEXCOORD4DVPROC glad_debug_glMultiTexCoord4dv = glad_debug_impl_glMultiTexCoord4dv;
PFNGLMULTITEXCOORD4FPROC glad_glMultiTexCoord4f;
void APIENTRY glad_debug_impl_glMultiTexCoord4f(GLenum arg0, GLfloat arg1, GLfloat arg2, GLfloat arg3, GLfloat arg4) {    
    _pre_call_callback("glMultiTexCoord4f", (void*)glMultiTexCoord4f, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glMultiTexCoord4f(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glMultiTexCoord4f", (void*)glMultiTexCoord4f, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLMULTITEXCOORD4FPROC glad_debug_glMultiTexCoord4f = glad_debug_impl_glMultiTexCoord4f;
PFNGLMULTITEXCOORD4FVPROC glad_glMultiTexCoord4fv;
void APIENTRY glad_debug_impl_glMultiTexCoord4fv(GLenum arg0, const GLfloat * arg1) {    
    _pre_call_callback("glMultiTexCoord4fv", (void*)glMultiTexCoord4fv, 2, arg0, arg1);
     glad_glMultiTexCoord4fv(arg0, arg1);
    _post_call_callback("glMultiTexCoord4fv", (void*)glMultiTexCoord4fv, 2, arg0, arg1);
    
}
PFNGLMULTITEXCOORD4FVPROC glad_debug_glMultiTexCoord4fv = glad_debug_impl_glMultiTexCoord4fv;
PFNGLMULTITEXCOORD4IPROC glad_glMultiTexCoord4i;
void APIENTRY glad_debug_impl_glMultiTexCoord4i(GLenum arg0, GLint arg1, GLint arg2, GLint arg3, GLint arg4) {    
    _pre_call_callback("glMultiTexCoord4i", (void*)glMultiTexCoord4i, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glMultiTexCoord4i(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glMultiTexCoord4i", (void*)glMultiTexCoord4i, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLMULTITEXCOORD4IPROC glad_debug_glMultiTexCoord4i = glad_debug_impl_glMultiTexCoord4i;
PFNGLMULTITEXCOORD4IVPROC glad_glMultiTexCoord4iv;
void APIENTRY glad_debug_impl_glMultiTexCoord4iv(GLenum arg0, const GLint * arg1) {    
    _pre_call_callback("glMultiTexCoord4iv", (void*)glMultiTexCoord4iv, 2, arg0, arg1);
     glad_glMultiTexCoord4iv(arg0, arg1);
    _post_call_callback("glMultiTexCoord4iv", (void*)glMultiTexCoord4iv, 2, arg0, arg1);
    
}
PFNGLMULTITEXCOORD4IVPROC glad_debug_glMultiTexCoord4iv = glad_debug_impl_glMultiTexCoord4iv;
PFNGLMULTITEXCOORD4SPROC glad_glMultiTexCoord4s;
void APIENTRY glad_debug_impl_glMultiTexCoord4s(GLenum arg0, GLshort arg1, GLshort arg2, GLshort arg3, GLshort arg4) {    
    _pre_call_callback("glMultiTexCoord4s", (void*)glMultiTexCoord4s, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glMultiTexCoord4s(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glMultiTexCoord4s", (void*)glMultiTexCoord4s, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLMULTITEXCOORD4SPROC glad_debug_glMultiTexCoord4s = glad_debug_impl_glMultiTexCoord4s;
PFNGLMULTITEXCOORD4SVPROC glad_glMultiTexCoord4sv;
void APIENTRY glad_debug_impl_glMultiTexCoord4sv(GLenum arg0, const GLshort * arg1) {    
    _pre_call_callback("glMultiTexCoord4sv", (void*)glMultiTexCoord4sv, 2, arg0, arg1);
     glad_glMultiTexCoord4sv(arg0, arg1);
    _post_call_callback("glMultiTexCoord4sv", (void*)glMultiTexCoord4sv, 2, arg0, arg1);
    
}
PFNGLMULTITEXCOORD4SVPROC glad_debug_glMultiTexCoord4sv = glad_debug_impl_glMultiTexCoord4sv;
PFNGLMULTITEXCOORDP1UIPROC glad_glMultiTexCoordP1ui;
void APIENTRY glad_debug_impl_glMultiTexCoordP1ui(GLenum arg0, GLenum arg1, GLuint arg2) {    
    _pre_call_callback("glMultiTexCoordP1ui", (void*)glMultiTexCoordP1ui, 3, arg0, arg1, arg2);
     glad_glMultiTexCoordP1ui(arg0, arg1, arg2);
    _post_call_callback("glMultiTexCoordP1ui", (void*)glMultiTexCoordP1ui, 3, arg0, arg1, arg2);
    
}
PFNGLMULTITEXCOORDP1UIPROC glad_debug_glMultiTexCoordP1ui = glad_debug_impl_glMultiTexCoordP1ui;
PFNGLMULTITEXCOORDP1UIVPROC glad_glMultiTexCoordP1uiv;
void APIENTRY glad_debug_impl_glMultiTexCoordP1uiv(GLenum arg0, GLenum arg1, const GLuint * arg2) {    
    _pre_call_callback("glMultiTexCoordP1uiv", (void*)glMultiTexCoordP1uiv, 3, arg0, arg1, arg2);
     glad_glMultiTexCoordP1uiv(arg0, arg1, arg2);
    _post_call_callback("glMultiTexCoordP1uiv", (void*)glMultiTexCoordP1uiv, 3, arg0, arg1, arg2);
    
}
PFNGLMULTITEXCOORDP1UIVPROC glad_debug_glMultiTexCoordP1uiv = glad_debug_impl_glMultiTexCoordP1uiv;
PFNGLMULTITEXCOORDP2UIPROC glad_glMultiTexCoordP2ui;
void APIENTRY glad_debug_impl_glMultiTexCoordP2ui(GLenum arg0, GLenum arg1, GLuint arg2) {    
    _pre_call_callback("glMultiTexCoordP2ui", (void*)glMultiTexCoordP2ui, 3, arg0, arg1, arg2);
     glad_glMultiTexCoordP2ui(arg0, arg1, arg2);
    _post_call_callback("glMultiTexCoordP2ui", (void*)glMultiTexCoordP2ui, 3, arg0, arg1, arg2);
    
}
PFNGLMULTITEXCOORDP2UIPROC glad_debug_glMultiTexCoordP2ui = glad_debug_impl_glMultiTexCoordP2ui;
PFNGLMULTITEXCOORDP2UIVPROC glad_glMultiTexCoordP2uiv;
void APIENTRY glad_debug_impl_glMultiTexCoordP2uiv(GLenum arg0, GLenum arg1, const GLuint * arg2) {    
    _pre_call_callback("glMultiTexCoordP2uiv", (void*)glMultiTexCoordP2uiv, 3, arg0, arg1, arg2);
     glad_glMultiTexCoordP2uiv(arg0, arg1, arg2);
    _post_call_callback("glMultiTexCoordP2uiv", (void*)glMultiTexCoordP2uiv, 3, arg0, arg1, arg2);
    
}
PFNGLMULTITEXCOORDP2UIVPROC glad_debug_glMultiTexCoordP2uiv = glad_debug_impl_glMultiTexCoordP2uiv;
PFNGLMULTITEXCOORDP3UIPROC glad_glMultiTexCoordP3ui;
void APIENTRY glad_debug_impl_glMultiTexCoordP3ui(GLenum arg0, GLenum arg1, GLuint arg2) {    
    _pre_call_callback("glMultiTexCoordP3ui", (void*)glMultiTexCoordP3ui, 3, arg0, arg1, arg2);
     glad_glMultiTexCoordP3ui(arg0, arg1, arg2);
    _post_call_callback("glMultiTexCoordP3ui", (void*)glMultiTexCoordP3ui, 3, arg0, arg1, arg2);
    
}
PFNGLMULTITEXCOORDP3UIPROC glad_debug_glMultiTexCoordP3ui = glad_debug_impl_glMultiTexCoordP3ui;
PFNGLMULTITEXCOORDP3UIVPROC glad_glMultiTexCoordP3uiv;
void APIENTRY glad_debug_impl_glMultiTexCoordP3uiv(GLenum arg0, GLenum arg1, const GLuint * arg2) {    
    _pre_call_callback("glMultiTexCoordP3uiv", (void*)glMultiTexCoordP3uiv, 3, arg0, arg1, arg2);
     glad_glMultiTexCoordP3uiv(arg0, arg1, arg2);
    _post_call_callback("glMultiTexCoordP3uiv", (void*)glMultiTexCoordP3uiv, 3, arg0, arg1, arg2);
    
}
PFNGLMULTITEXCOORDP3UIVPROC glad_debug_glMultiTexCoordP3uiv = glad_debug_impl_glMultiTexCoordP3uiv;
PFNGLMULTITEXCOORDP4UIPROC glad_glMultiTexCoordP4ui;
void APIENTRY glad_debug_impl_glMultiTexCoordP4ui(GLenum arg0, GLenum arg1, GLuint arg2) {    
    _pre_call_callback("glMultiTexCoordP4ui", (void*)glMultiTexCoordP4ui, 3, arg0, arg1, arg2);
     glad_glMultiTexCoordP4ui(arg0, arg1, arg2);
    _post_call_callback("glMultiTexCoordP4ui", (void*)glMultiTexCoordP4ui, 3, arg0, arg1, arg2);
    
}
PFNGLMULTITEXCOORDP4UIPROC glad_debug_glMultiTexCoordP4ui = glad_debug_impl_glMultiTexCoordP4ui;
PFNGLMULTITEXCOORDP4UIVPROC glad_glMultiTexCoordP4uiv;
void APIENTRY glad_debug_impl_glMultiTexCoordP4uiv(GLenum arg0, GLenum arg1, const GLuint * arg2) {    
    _pre_call_callback("glMultiTexCoordP4uiv", (void*)glMultiTexCoordP4uiv, 3, arg0, arg1, arg2);
     glad_glMultiTexCoordP4uiv(arg0, arg1, arg2);
    _post_call_callback("glMultiTexCoordP4uiv", (void*)glMultiTexCoordP4uiv, 3, arg0, arg1, arg2);
    
}
PFNGLMULTITEXCOORDP4UIVPROC glad_debug_glMultiTexCoordP4uiv = glad_debug_impl_glMultiTexCoordP4uiv;
PFNGLNAMEDBUFFERDATAPROC glad_glNamedBufferData;
void APIENTRY glad_debug_impl_glNamedBufferData(GLuint arg0, GLsizeiptr arg1, const void * arg2, GLenum arg3) {    
    _pre_call_callback("glNamedBufferData", (void*)glNamedBufferData, 4, arg0, arg1, arg2, arg3);
     glad_glNamedBufferData(arg0, arg1, arg2, arg3);
    _post_call_callback("glNamedBufferData", (void*)glNamedBufferData, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLNAMEDBUFFERDATAPROC glad_debug_glNamedBufferData = glad_debug_impl_glNamedBufferData;
PFNGLNAMEDBUFFERSTORAGEPROC glad_glNamedBufferStorage;
void APIENTRY glad_debug_impl_glNamedBufferStorage(GLuint arg0, GLsizeiptr arg1, const void * arg2, GLbitfield arg3) {    
    _pre_call_callback("glNamedBufferStorage", (void*)glNamedBufferStorage, 4, arg0, arg1, arg2, arg3);
     glad_glNamedBufferStorage(arg0, arg1, arg2, arg3);
    _post_call_callback("glNamedBufferStorage", (void*)glNamedBufferStorage, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLNAMEDBUFFERSTORAGEPROC glad_debug_glNamedBufferStorage = glad_debug_impl_glNamedBufferStorage;
PFNGLNAMEDBUFFERSUBDATAPROC glad_glNamedBufferSubData;
void APIENTRY glad_debug_impl_glNamedBufferSubData(GLuint arg0, GLintptr arg1, GLsizeiptr arg2, const void * arg3) {    
    _pre_call_callback("glNamedBufferSubData", (void*)glNamedBufferSubData, 4, arg0, arg1, arg2, arg3);
     glad_glNamedBufferSubData(arg0, arg1, arg2, arg3);
    _post_call_callback("glNamedBufferSubData", (void*)glNamedBufferSubData, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLNAMEDBUFFERSUBDATAPROC glad_debug_glNamedBufferSubData = glad_debug_impl_glNamedBufferSubData;
PFNGLNAMEDFRAMEBUFFERDRAWBUFFERPROC glad_glNamedFramebufferDrawBuffer;
void APIENTRY glad_debug_impl_glNamedFramebufferDrawBuffer(GLuint arg0, GLenum arg1) {    
    _pre_call_callback("glNamedFramebufferDrawBuffer", (void*)glNamedFramebufferDrawBuffer, 2, arg0, arg1);
     glad_glNamedFramebufferDrawBuffer(arg0, arg1);
    _post_call_callback("glNamedFramebufferDrawBuffer", (void*)glNamedFramebufferDrawBuffer, 2, arg0, arg1);
    
}
PFNGLNAMEDFRAMEBUFFERDRAWBUFFERPROC glad_debug_glNamedFramebufferDrawBuffer = glad_debug_impl_glNamedFramebufferDrawBuffer;
PFNGLNAMEDFRAMEBUFFERDRAWBUFFERSPROC glad_glNamedFramebufferDrawBuffers;
void APIENTRY glad_debug_impl_glNamedFramebufferDrawBuffers(GLuint arg0, GLsizei arg1, const GLenum * arg2) {    
    _pre_call_callback("glNamedFramebufferDrawBuffers", (void*)glNamedFramebufferDrawBuffers, 3, arg0, arg1, arg2);
     glad_glNamedFramebufferDrawBuffers(arg0, arg1, arg2);
    _post_call_callback("glNamedFramebufferDrawBuffers", (void*)glNamedFramebufferDrawBuffers, 3, arg0, arg1, arg2);
    
}
PFNGLNAMEDFRAMEBUFFERDRAWBUFFERSPROC glad_debug_glNamedFramebufferDrawBuffers = glad_debug_impl_glNamedFramebufferDrawBuffers;
PFNGLNAMEDFRAMEBUFFERPARAMETERIPROC glad_glNamedFramebufferParameteri;
void APIENTRY glad_debug_impl_glNamedFramebufferParameteri(GLuint arg0, GLenum arg1, GLint arg2) {    
    _pre_call_callback("glNamedFramebufferParameteri", (void*)glNamedFramebufferParameteri, 3, arg0, arg1, arg2);
     glad_glNamedFramebufferParameteri(arg0, arg1, arg2);
    _post_call_callback("glNamedFramebufferParameteri", (void*)glNamedFramebufferParameteri, 3, arg0, arg1, arg2);
    
}
PFNGLNAMEDFRAMEBUFFERPARAMETERIPROC glad_debug_glNamedFramebufferParameteri = glad_debug_impl_glNamedFramebufferParameteri;
PFNGLNAMEDFRAMEBUFFERREADBUFFERPROC glad_glNamedFramebufferReadBuffer;
void APIENTRY glad_debug_impl_glNamedFramebufferReadBuffer(GLuint arg0, GLenum arg1) {    
    _pre_call_callback("glNamedFramebufferReadBuffer", (void*)glNamedFramebufferReadBuffer, 2, arg0, arg1);
     glad_glNamedFramebufferReadBuffer(arg0, arg1);
    _post_call_callback("glNamedFramebufferReadBuffer", (void*)glNamedFramebufferReadBuffer, 2, arg0, arg1);
    
}
PFNGLNAMEDFRAMEBUFFERREADBUFFERPROC glad_debug_glNamedFramebufferReadBuffer = glad_debug_impl_glNamedFramebufferReadBuffer;
PFNGLNAMEDFRAMEBUFFERRENDERBUFFERPROC glad_glNamedFramebufferRenderbuffer;
void APIENTRY glad_debug_impl_glNamedFramebufferRenderbuffer(GLuint arg0, GLenum arg1, GLenum arg2, GLuint arg3) {    
    _pre_call_callback("glNamedFramebufferRenderbuffer", (void*)glNamedFramebufferRenderbuffer, 4, arg0, arg1, arg2, arg3);
     glad_glNamedFramebufferRenderbuffer(arg0, arg1, arg2, arg3);
    _post_call_callback("glNamedFramebufferRenderbuffer", (void*)glNamedFramebufferRenderbuffer, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLNAMEDFRAMEBUFFERRENDERBUFFERPROC glad_debug_glNamedFramebufferRenderbuffer = glad_debug_impl_glNamedFramebufferRenderbuffer;
PFNGLNAMEDFRAMEBUFFERTEXTUREPROC glad_glNamedFramebufferTexture;
void APIENTRY glad_debug_impl_glNamedFramebufferTexture(GLuint arg0, GLenum arg1, GLuint arg2, GLint arg3) {    
    _pre_call_callback("glNamedFramebufferTexture", (void*)glNamedFramebufferTexture, 4, arg0, arg1, arg2, arg3);
     glad_glNamedFramebufferTexture(arg0, arg1, arg2, arg3);
    _post_call_callback("glNamedFramebufferTexture", (void*)glNamedFramebufferTexture, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLNAMEDFRAMEBUFFERTEXTUREPROC glad_debug_glNamedFramebufferTexture = glad_debug_impl_glNamedFramebufferTexture;
PFNGLNAMEDFRAMEBUFFERTEXTURELAYERPROC glad_glNamedFramebufferTextureLayer;
void APIENTRY glad_debug_impl_glNamedFramebufferTextureLayer(GLuint arg0, GLenum arg1, GLuint arg2, GLint arg3, GLint arg4) {    
    _pre_call_callback("glNamedFramebufferTextureLayer", (void*)glNamedFramebufferTextureLayer, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glNamedFramebufferTextureLayer(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glNamedFramebufferTextureLayer", (void*)glNamedFramebufferTextureLayer, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLNAMEDFRAMEBUFFERTEXTURELAYERPROC glad_debug_glNamedFramebufferTextureLayer = glad_debug_impl_glNamedFramebufferTextureLayer;
PFNGLNAMEDRENDERBUFFERSTORAGEPROC glad_glNamedRenderbufferStorage;
void APIENTRY glad_debug_impl_glNamedRenderbufferStorage(GLuint arg0, GLenum arg1, GLsizei arg2, GLsizei arg3) {    
    _pre_call_callback("glNamedRenderbufferStorage", (void*)glNamedRenderbufferStorage, 4, arg0, arg1, arg2, arg3);
     glad_glNamedRenderbufferStorage(arg0, arg1, arg2, arg3);
    _post_call_callback("glNamedRenderbufferStorage", (void*)glNamedRenderbufferStorage, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLNAMEDRENDERBUFFERSTORAGEPROC glad_debug_glNamedRenderbufferStorage = glad_debug_impl_glNamedRenderbufferStorage;
PFNGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLEPROC glad_glNamedRenderbufferStorageMultisample;
void APIENTRY glad_debug_impl_glNamedRenderbufferStorageMultisample(GLuint arg0, GLsizei arg1, GLenum arg2, GLsizei arg3, GLsizei arg4) {    
    _pre_call_callback("glNamedRenderbufferStorageMultisample", (void*)glNamedRenderbufferStorageMultisample, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glNamedRenderbufferStorageMultisample(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glNamedRenderbufferStorageMultisample", (void*)glNamedRenderbufferStorageMultisample, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLEPROC glad_debug_glNamedRenderbufferStorageMultisample = glad_debug_impl_glNamedRenderbufferStorageMultisample;
PFNGLNEWLISTPROC glad_glNewList;
void APIENTRY glad_debug_impl_glNewList(GLuint arg0, GLenum arg1) {    
    _pre_call_callback("glNewList", (void*)glNewList, 2, arg0, arg1);
     glad_glNewList(arg0, arg1);
    _post_call_callback("glNewList", (void*)glNewList, 2, arg0, arg1);
    
}
PFNGLNEWLISTPROC glad_debug_glNewList = glad_debug_impl_glNewList;
PFNGLNORMAL3BPROC glad_glNormal3b;
void APIENTRY glad_debug_impl_glNormal3b(GLbyte arg0, GLbyte arg1, GLbyte arg2) {    
    _pre_call_callback("glNormal3b", (void*)glNormal3b, 3, arg0, arg1, arg2);
     glad_glNormal3b(arg0, arg1, arg2);
    _post_call_callback("glNormal3b", (void*)glNormal3b, 3, arg0, arg1, arg2);
    
}
PFNGLNORMAL3BPROC glad_debug_glNormal3b = glad_debug_impl_glNormal3b;
PFNGLNORMAL3BVPROC glad_glNormal3bv;
void APIENTRY glad_debug_impl_glNormal3bv(const GLbyte * arg0) {    
    _pre_call_callback("glNormal3bv", (void*)glNormal3bv, 1, arg0);
     glad_glNormal3bv(arg0);
    _post_call_callback("glNormal3bv", (void*)glNormal3bv, 1, arg0);
    
}
PFNGLNORMAL3BVPROC glad_debug_glNormal3bv = glad_debug_impl_glNormal3bv;
PFNGLNORMAL3DPROC glad_glNormal3d;
void APIENTRY glad_debug_impl_glNormal3d(GLdouble arg0, GLdouble arg1, GLdouble arg2) {    
    _pre_call_callback("glNormal3d", (void*)glNormal3d, 3, arg0, arg1, arg2);
     glad_glNormal3d(arg0, arg1, arg2);
    _post_call_callback("glNormal3d", (void*)glNormal3d, 3, arg0, arg1, arg2);
    
}
PFNGLNORMAL3DPROC glad_debug_glNormal3d = glad_debug_impl_glNormal3d;
PFNGLNORMAL3DVPROC glad_glNormal3dv;
void APIENTRY glad_debug_impl_glNormal3dv(const GLdouble * arg0) {    
    _pre_call_callback("glNormal3dv", (void*)glNormal3dv, 1, arg0);
     glad_glNormal3dv(arg0);
    _post_call_callback("glNormal3dv", (void*)glNormal3dv, 1, arg0);
    
}
PFNGLNORMAL3DVPROC glad_debug_glNormal3dv = glad_debug_impl_glNormal3dv;
PFNGLNORMAL3FPROC glad_glNormal3f;
void APIENTRY glad_debug_impl_glNormal3f(GLfloat arg0, GLfloat arg1, GLfloat arg2) {    
    _pre_call_callback("glNormal3f", (void*)glNormal3f, 3, arg0, arg1, arg2);
     glad_glNormal3f(arg0, arg1, arg2);
    _post_call_callback("glNormal3f", (void*)glNormal3f, 3, arg0, arg1, arg2);
    
}
PFNGLNORMAL3FPROC glad_debug_glNormal3f = glad_debug_impl_glNormal3f;
PFNGLNORMAL3FVPROC glad_glNormal3fv;
void APIENTRY glad_debug_impl_glNormal3fv(const GLfloat * arg0) {    
    _pre_call_callback("glNormal3fv", (void*)glNormal3fv, 1, arg0);
     glad_glNormal3fv(arg0);
    _post_call_callback("glNormal3fv", (void*)glNormal3fv, 1, arg0);
    
}
PFNGLNORMAL3FVPROC glad_debug_glNormal3fv = glad_debug_impl_glNormal3fv;
PFNGLNORMAL3IPROC glad_glNormal3i;
void APIENTRY glad_debug_impl_glNormal3i(GLint arg0, GLint arg1, GLint arg2) {    
    _pre_call_callback("glNormal3i", (void*)glNormal3i, 3, arg0, arg1, arg2);
     glad_glNormal3i(arg0, arg1, arg2);
    _post_call_callback("glNormal3i", (void*)glNormal3i, 3, arg0, arg1, arg2);
    
}
PFNGLNORMAL3IPROC glad_debug_glNormal3i = glad_debug_impl_glNormal3i;
PFNGLNORMAL3IVPROC glad_glNormal3iv;
void APIENTRY glad_debug_impl_glNormal3iv(const GLint * arg0) {    
    _pre_call_callback("glNormal3iv", (void*)glNormal3iv, 1, arg0);
     glad_glNormal3iv(arg0);
    _post_call_callback("glNormal3iv", (void*)glNormal3iv, 1, arg0);
    
}
PFNGLNORMAL3IVPROC glad_debug_glNormal3iv = glad_debug_impl_glNormal3iv;
PFNGLNORMAL3SPROC glad_glNormal3s;
void APIENTRY glad_debug_impl_glNormal3s(GLshort arg0, GLshort arg1, GLshort arg2) {    
    _pre_call_callback("glNormal3s", (void*)glNormal3s, 3, arg0, arg1, arg2);
     glad_glNormal3s(arg0, arg1, arg2);
    _post_call_callback("glNormal3s", (void*)glNormal3s, 3, arg0, arg1, arg2);
    
}
PFNGLNORMAL3SPROC glad_debug_glNormal3s = glad_debug_impl_glNormal3s;
PFNGLNORMAL3SVPROC glad_glNormal3sv;
void APIENTRY glad_debug_impl_glNormal3sv(const GLshort * arg0) {    
    _pre_call_callback("glNormal3sv", (void*)glNormal3sv, 1, arg0);
     glad_glNormal3sv(arg0);
    _post_call_callback("glNormal3sv", (void*)glNormal3sv, 1, arg0);
    
}
PFNGLNORMAL3SVPROC glad_debug_glNormal3sv = glad_debug_impl_glNormal3sv;
PFNGLNORMALP3UIPROC glad_glNormalP3ui;
void APIENTRY glad_debug_impl_glNormalP3ui(GLenum arg0, GLuint arg1) {    
    _pre_call_callback("glNormalP3ui", (void*)glNormalP3ui, 2, arg0, arg1);
     glad_glNormalP3ui(arg0, arg1);
    _post_call_callback("glNormalP3ui", (void*)glNormalP3ui, 2, arg0, arg1);
    
}
PFNGLNORMALP3UIPROC glad_debug_glNormalP3ui = glad_debug_impl_glNormalP3ui;
PFNGLNORMALP3UIVPROC glad_glNormalP3uiv;
void APIENTRY glad_debug_impl_glNormalP3uiv(GLenum arg0, const GLuint * arg1) {    
    _pre_call_callback("glNormalP3uiv", (void*)glNormalP3uiv, 2, arg0, arg1);
     glad_glNormalP3uiv(arg0, arg1);
    _post_call_callback("glNormalP3uiv", (void*)glNormalP3uiv, 2, arg0, arg1);
    
}
PFNGLNORMALP3UIVPROC glad_debug_glNormalP3uiv = glad_debug_impl_glNormalP3uiv;
PFNGLNORMALPOINTERPROC glad_glNormalPointer;
void APIENTRY glad_debug_impl_glNormalPointer(GLenum arg0, GLsizei arg1, const void * arg2) {    
    _pre_call_callback("glNormalPointer", (void*)glNormalPointer, 3, arg0, arg1, arg2);
     glad_glNormalPointer(arg0, arg1, arg2);
    _post_call_callback("glNormalPointer", (void*)glNormalPointer, 3, arg0, arg1, arg2);
    
}
PFNGLNORMALPOINTERPROC glad_debug_glNormalPointer = glad_debug_impl_glNormalPointer;
PFNGLOBJECTLABELPROC glad_glObjectLabel;
void APIENTRY glad_debug_impl_glObjectLabel(GLenum arg0, GLuint arg1, GLsizei arg2, const GLchar * arg3) {    
    _pre_call_callback("glObjectLabel", (void*)glObjectLabel, 4, arg0, arg1, arg2, arg3);
     glad_glObjectLabel(arg0, arg1, arg2, arg3);
    _post_call_callback("glObjectLabel", (void*)glObjectLabel, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLOBJECTLABELPROC glad_debug_glObjectLabel = glad_debug_impl_glObjectLabel;
PFNGLOBJECTPTRLABELPROC glad_glObjectPtrLabel;
void APIENTRY glad_debug_impl_glObjectPtrLabel(const void * arg0, GLsizei arg1, const GLchar * arg2) {    
    _pre_call_callback("glObjectPtrLabel", (void*)glObjectPtrLabel, 3, arg0, arg1, arg2);
     glad_glObjectPtrLabel(arg0, arg1, arg2);
    _post_call_callback("glObjectPtrLabel", (void*)glObjectPtrLabel, 3, arg0, arg1, arg2);
    
}
PFNGLOBJECTPTRLABELPROC glad_debug_glObjectPtrLabel = glad_debug_impl_glObjectPtrLabel;
PFNGLORTHOPROC glad_glOrtho;
void APIENTRY glad_debug_impl_glOrtho(GLdouble arg0, GLdouble arg1, GLdouble arg2, GLdouble arg3, GLdouble arg4, GLdouble arg5) {    
    _pre_call_callback("glOrtho", (void*)glOrtho, 6, arg0, arg1, arg2, arg3, arg4, arg5);
     glad_glOrtho(arg0, arg1, arg2, arg3, arg4, arg5);
    _post_call_callback("glOrtho", (void*)glOrtho, 6, arg0, arg1, arg2, arg3, arg4, arg5);
    
}
PFNGLORTHOPROC glad_debug_glOrtho = glad_debug_impl_glOrtho;
PFNGLPASSTHROUGHPROC glad_glPassThrough;
void APIENTRY glad_debug_impl_glPassThrough(GLfloat arg0) {    
    _pre_call_callback("glPassThrough", (void*)glPassThrough, 1, arg0);
     glad_glPassThrough(arg0);
    _post_call_callback("glPassThrough", (void*)glPassThrough, 1, arg0);
    
}
PFNGLPASSTHROUGHPROC glad_debug_glPassThrough = glad_debug_impl_glPassThrough;
PFNGLPATCHPARAMETERFVPROC glad_glPatchParameterfv;
void APIENTRY glad_debug_impl_glPatchParameterfv(GLenum arg0, const GLfloat * arg1) {    
    _pre_call_callback("glPatchParameterfv", (void*)glPatchParameterfv, 2, arg0, arg1);
     glad_glPatchParameterfv(arg0, arg1);
    _post_call_callback("glPatchParameterfv", (void*)glPatchParameterfv, 2, arg0, arg1);
    
}
PFNGLPATCHPARAMETERFVPROC glad_debug_glPatchParameterfv = glad_debug_impl_glPatchParameterfv;
PFNGLPATCHPARAMETERIPROC glad_glPatchParameteri;
void APIENTRY glad_debug_impl_glPatchParameteri(GLenum arg0, GLint arg1) {    
    _pre_call_callback("glPatchParameteri", (void*)glPatchParameteri, 2, arg0, arg1);
     glad_glPatchParameteri(arg0, arg1);
    _post_call_callback("glPatchParameteri", (void*)glPatchParameteri, 2, arg0, arg1);
    
}
PFNGLPATCHPARAMETERIPROC glad_debug_glPatchParameteri = glad_debug_impl_glPatchParameteri;
PFNGLPAUSETRANSFORMFEEDBACKPROC glad_glPauseTransformFeedback;
void APIENTRY glad_debug_impl_glPauseTransformFeedback(void) {    
    _pre_call_callback("glPauseTransformFeedback", (void*)glPauseTransformFeedback, 0);
     glad_glPauseTransformFeedback();
    _post_call_callback("glPauseTransformFeedback", (void*)glPauseTransformFeedback, 0);
    
}
PFNGLPAUSETRANSFORMFEEDBACKPROC glad_debug_glPauseTransformFeedback = glad_debug_impl_glPauseTransformFeedback;
PFNGLPIXELMAPFVPROC glad_glPixelMapfv;
void APIENTRY glad_debug_impl_glPixelMapfv(GLenum arg0, GLsizei arg1, const GLfloat * arg2) {    
    _pre_call_callback("glPixelMapfv", (void*)glPixelMapfv, 3, arg0, arg1, arg2);
     glad_glPixelMapfv(arg0, arg1, arg2);
    _post_call_callback("glPixelMapfv", (void*)glPixelMapfv, 3, arg0, arg1, arg2);
    
}
PFNGLPIXELMAPFVPROC glad_debug_glPixelMapfv = glad_debug_impl_glPixelMapfv;
PFNGLPIXELMAPUIVPROC glad_glPixelMapuiv;
void APIENTRY glad_debug_impl_glPixelMapuiv(GLenum arg0, GLsizei arg1, const GLuint * arg2) {    
    _pre_call_callback("glPixelMapuiv", (void*)glPixelMapuiv, 3, arg0, arg1, arg2);
     glad_glPixelMapuiv(arg0, arg1, arg2);
    _post_call_callback("glPixelMapuiv", (void*)glPixelMapuiv, 3, arg0, arg1, arg2);
    
}
PFNGLPIXELMAPUIVPROC glad_debug_glPixelMapuiv = glad_debug_impl_glPixelMapuiv;
PFNGLPIXELMAPUSVPROC glad_glPixelMapusv;
void APIENTRY glad_debug_impl_glPixelMapusv(GLenum arg0, GLsizei arg1, const GLushort * arg2) {    
    _pre_call_callback("glPixelMapusv", (void*)glPixelMapusv, 3, arg0, arg1, arg2);
     glad_glPixelMapusv(arg0, arg1, arg2);
    _post_call_callback("glPixelMapusv", (void*)glPixelMapusv, 3, arg0, arg1, arg2);
    
}
PFNGLPIXELMAPUSVPROC glad_debug_glPixelMapusv = glad_debug_impl_glPixelMapusv;
PFNGLPIXELSTOREFPROC glad_glPixelStoref;
void APIENTRY glad_debug_impl_glPixelStoref(GLenum arg0, GLfloat arg1) {    
    _pre_call_callback("glPixelStoref", (void*)glPixelStoref, 2, arg0, arg1);
     glad_glPixelStoref(arg0, arg1);
    _post_call_callback("glPixelStoref", (void*)glPixelStoref, 2, arg0, arg1);
    
}
PFNGLPIXELSTOREFPROC glad_debug_glPixelStoref = glad_debug_impl_glPixelStoref;
PFNGLPIXELSTOREIPROC glad_glPixelStorei;
void APIENTRY glad_debug_impl_glPixelStorei(GLenum arg0, GLint arg1) {    
    _pre_call_callback("glPixelStorei", (void*)glPixelStorei, 2, arg0, arg1);
     glad_glPixelStorei(arg0, arg1);
    _post_call_callback("glPixelStorei", (void*)glPixelStorei, 2, arg0, arg1);
    
}
PFNGLPIXELSTOREIPROC glad_debug_glPixelStorei = glad_debug_impl_glPixelStorei;
PFNGLPIXELTRANSFERFPROC glad_glPixelTransferf;
void APIENTRY glad_debug_impl_glPixelTransferf(GLenum arg0, GLfloat arg1) {    
    _pre_call_callback("glPixelTransferf", (void*)glPixelTransferf, 2, arg0, arg1);
     glad_glPixelTransferf(arg0, arg1);
    _post_call_callback("glPixelTransferf", (void*)glPixelTransferf, 2, arg0, arg1);
    
}
PFNGLPIXELTRANSFERFPROC glad_debug_glPixelTransferf = glad_debug_impl_glPixelTransferf;
PFNGLPIXELTRANSFERIPROC glad_glPixelTransferi;
void APIENTRY glad_debug_impl_glPixelTransferi(GLenum arg0, GLint arg1) {    
    _pre_call_callback("glPixelTransferi", (void*)glPixelTransferi, 2, arg0, arg1);
     glad_glPixelTransferi(arg0, arg1);
    _post_call_callback("glPixelTransferi", (void*)glPixelTransferi, 2, arg0, arg1);
    
}
PFNGLPIXELTRANSFERIPROC glad_debug_glPixelTransferi = glad_debug_impl_glPixelTransferi;
PFNGLPIXELZOOMPROC glad_glPixelZoom;
void APIENTRY glad_debug_impl_glPixelZoom(GLfloat arg0, GLfloat arg1) {    
    _pre_call_callback("glPixelZoom", (void*)glPixelZoom, 2, arg0, arg1);
     glad_glPixelZoom(arg0, arg1);
    _post_call_callback("glPixelZoom", (void*)glPixelZoom, 2, arg0, arg1);
    
}
PFNGLPIXELZOOMPROC glad_debug_glPixelZoom = glad_debug_impl_glPixelZoom;
PFNGLPOINTPARAMETERFPROC glad_glPointParameterf;
void APIENTRY glad_debug_impl_glPointParameterf(GLenum arg0, GLfloat arg1) {    
    _pre_call_callback("glPointParameterf", (void*)glPointParameterf, 2, arg0, arg1);
     glad_glPointParameterf(arg0, arg1);
    _post_call_callback("glPointParameterf", (void*)glPointParameterf, 2, arg0, arg1);
    
}
PFNGLPOINTPARAMETERFPROC glad_debug_glPointParameterf = glad_debug_impl_glPointParameterf;
PFNGLPOINTPARAMETERFVPROC glad_glPointParameterfv;
void APIENTRY glad_debug_impl_glPointParameterfv(GLenum arg0, const GLfloat * arg1) {    
    _pre_call_callback("glPointParameterfv", (void*)glPointParameterfv, 2, arg0, arg1);
     glad_glPointParameterfv(arg0, arg1);
    _post_call_callback("glPointParameterfv", (void*)glPointParameterfv, 2, arg0, arg1);
    
}
PFNGLPOINTPARAMETERFVPROC glad_debug_glPointParameterfv = glad_debug_impl_glPointParameterfv;
PFNGLPOINTPARAMETERIPROC glad_glPointParameteri;
void APIENTRY glad_debug_impl_glPointParameteri(GLenum arg0, GLint arg1) {    
    _pre_call_callback("glPointParameteri", (void*)glPointParameteri, 2, arg0, arg1);
     glad_glPointParameteri(arg0, arg1);
    _post_call_callback("glPointParameteri", (void*)glPointParameteri, 2, arg0, arg1);
    
}
PFNGLPOINTPARAMETERIPROC glad_debug_glPointParameteri = glad_debug_impl_glPointParameteri;
PFNGLPOINTPARAMETERIVPROC glad_glPointParameteriv;
void APIENTRY glad_debug_impl_glPointParameteriv(GLenum arg0, const GLint * arg1) {    
    _pre_call_callback("glPointParameteriv", (void*)glPointParameteriv, 2, arg0, arg1);
     glad_glPointParameteriv(arg0, arg1);
    _post_call_callback("glPointParameteriv", (void*)glPointParameteriv, 2, arg0, arg1);
    
}
PFNGLPOINTPARAMETERIVPROC glad_debug_glPointParameteriv = glad_debug_impl_glPointParameteriv;
PFNGLPOINTSIZEPROC glad_glPointSize;
void APIENTRY glad_debug_impl_glPointSize(GLfloat arg0) {    
    _pre_call_callback("glPointSize", (void*)glPointSize, 1, arg0);
     glad_glPointSize(arg0);
    _post_call_callback("glPointSize", (void*)glPointSize, 1, arg0);
    
}
PFNGLPOINTSIZEPROC glad_debug_glPointSize = glad_debug_impl_glPointSize;
PFNGLPOLYGONMODEPROC glad_glPolygonMode;
void APIENTRY glad_debug_impl_glPolygonMode(GLenum arg0, GLenum arg1) {    
    _pre_call_callback("glPolygonMode", (void*)glPolygonMode, 2, arg0, arg1);
     glad_glPolygonMode(arg0, arg1);
    _post_call_callback("glPolygonMode", (void*)glPolygonMode, 2, arg0, arg1);
    
}
PFNGLPOLYGONMODEPROC glad_debug_glPolygonMode = glad_debug_impl_glPolygonMode;
PFNGLPOLYGONOFFSETPROC glad_glPolygonOffset;
void APIENTRY glad_debug_impl_glPolygonOffset(GLfloat arg0, GLfloat arg1) {    
    _pre_call_callback("glPolygonOffset", (void*)glPolygonOffset, 2, arg0, arg1);
     glad_glPolygonOffset(arg0, arg1);
    _post_call_callback("glPolygonOffset", (void*)glPolygonOffset, 2, arg0, arg1);
    
}
PFNGLPOLYGONOFFSETPROC glad_debug_glPolygonOffset = glad_debug_impl_glPolygonOffset;
PFNGLPOLYGONSTIPPLEPROC glad_glPolygonStipple;
void APIENTRY glad_debug_impl_glPolygonStipple(const GLubyte * arg0) {    
    _pre_call_callback("glPolygonStipple", (void*)glPolygonStipple, 1, arg0);
     glad_glPolygonStipple(arg0);
    _post_call_callback("glPolygonStipple", (void*)glPolygonStipple, 1, arg0);
    
}
PFNGLPOLYGONSTIPPLEPROC glad_debug_glPolygonStipple = glad_debug_impl_glPolygonStipple;
PFNGLPOPATTRIBPROC glad_glPopAttrib;
void APIENTRY glad_debug_impl_glPopAttrib(void) {    
    _pre_call_callback("glPopAttrib", (void*)glPopAttrib, 0);
     glad_glPopAttrib();
    _post_call_callback("glPopAttrib", (void*)glPopAttrib, 0);
    
}
PFNGLPOPATTRIBPROC glad_debug_glPopAttrib = glad_debug_impl_glPopAttrib;
PFNGLPOPCLIENTATTRIBPROC glad_glPopClientAttrib;
void APIENTRY glad_debug_impl_glPopClientAttrib(void) {    
    _pre_call_callback("glPopClientAttrib", (void*)glPopClientAttrib, 0);
     glad_glPopClientAttrib();
    _post_call_callback("glPopClientAttrib", (void*)glPopClientAttrib, 0);
    
}
PFNGLPOPCLIENTATTRIBPROC glad_debug_glPopClientAttrib = glad_debug_impl_glPopClientAttrib;
PFNGLPOPDEBUGGROUPPROC glad_glPopDebugGroup;
void APIENTRY glad_debug_impl_glPopDebugGroup(void) {    
    _pre_call_callback("glPopDebugGroup", (void*)glPopDebugGroup, 0);
     glad_glPopDebugGroup();
    _post_call_callback("glPopDebugGroup", (void*)glPopDebugGroup, 0);
    
}
PFNGLPOPDEBUGGROUPPROC glad_debug_glPopDebugGroup = glad_debug_impl_glPopDebugGroup;
PFNGLPOPMATRIXPROC glad_glPopMatrix;
void APIENTRY glad_debug_impl_glPopMatrix(void) {    
    _pre_call_callback("glPopMatrix", (void*)glPopMatrix, 0);
     glad_glPopMatrix();
    _post_call_callback("glPopMatrix", (void*)glPopMatrix, 0);
    
}
PFNGLPOPMATRIXPROC glad_debug_glPopMatrix = glad_debug_impl_glPopMatrix;
PFNGLPOPNAMEPROC glad_glPopName;
void APIENTRY glad_debug_impl_glPopName(void) {    
    _pre_call_callback("glPopName", (void*)glPopName, 0);
     glad_glPopName();
    _post_call_callback("glPopName", (void*)glPopName, 0);
    
}
PFNGLPOPNAMEPROC glad_debug_glPopName = glad_debug_impl_glPopName;
PFNGLPRIMITIVERESTARTINDEXPROC glad_glPrimitiveRestartIndex;
void APIENTRY glad_debug_impl_glPrimitiveRestartIndex(GLuint arg0) {    
    _pre_call_callback("glPrimitiveRestartIndex", (void*)glPrimitiveRestartIndex, 1, arg0);
     glad_glPrimitiveRestartIndex(arg0);
    _post_call_callback("glPrimitiveRestartIndex", (void*)glPrimitiveRestartIndex, 1, arg0);
    
}
PFNGLPRIMITIVERESTARTINDEXPROC glad_debug_glPrimitiveRestartIndex = glad_debug_impl_glPrimitiveRestartIndex;
PFNGLPRIORITIZETEXTURESPROC glad_glPrioritizeTextures;
void APIENTRY glad_debug_impl_glPrioritizeTextures(GLsizei arg0, const GLuint * arg1, const GLfloat * arg2) {    
    _pre_call_callback("glPrioritizeTextures", (void*)glPrioritizeTextures, 3, arg0, arg1, arg2);
     glad_glPrioritizeTextures(arg0, arg1, arg2);
    _post_call_callback("glPrioritizeTextures", (void*)glPrioritizeTextures, 3, arg0, arg1, arg2);
    
}
PFNGLPRIORITIZETEXTURESPROC glad_debug_glPrioritizeTextures = glad_debug_impl_glPrioritizeTextures;
PFNGLPROGRAMBINARYPROC glad_glProgramBinary;
void APIENTRY glad_debug_impl_glProgramBinary(GLuint arg0, GLenum arg1, const void * arg2, GLsizei arg3) {    
    _pre_call_callback("glProgramBinary", (void*)glProgramBinary, 4, arg0, arg1, arg2, arg3);
     glad_glProgramBinary(arg0, arg1, arg2, arg3);
    _post_call_callback("glProgramBinary", (void*)glProgramBinary, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLPROGRAMBINARYPROC glad_debug_glProgramBinary = glad_debug_impl_glProgramBinary;
PFNGLPROGRAMPARAMETERIPROC glad_glProgramParameteri;
void APIENTRY glad_debug_impl_glProgramParameteri(GLuint arg0, GLenum arg1, GLint arg2) {    
    _pre_call_callback("glProgramParameteri", (void*)glProgramParameteri, 3, arg0, arg1, arg2);
     glad_glProgramParameteri(arg0, arg1, arg2);
    _post_call_callback("glProgramParameteri", (void*)glProgramParameteri, 3, arg0, arg1, arg2);
    
}
PFNGLPROGRAMPARAMETERIPROC glad_debug_glProgramParameteri = glad_debug_impl_glProgramParameteri;
PFNGLPROGRAMUNIFORM1DPROC glad_glProgramUniform1d;
void APIENTRY glad_debug_impl_glProgramUniform1d(GLuint arg0, GLint arg1, GLdouble arg2) {    
    _pre_call_callback("glProgramUniform1d", (void*)glProgramUniform1d, 3, arg0, arg1, arg2);
     glad_glProgramUniform1d(arg0, arg1, arg2);
    _post_call_callback("glProgramUniform1d", (void*)glProgramUniform1d, 3, arg0, arg1, arg2);
    
}
PFNGLPROGRAMUNIFORM1DPROC glad_debug_glProgramUniform1d = glad_debug_impl_glProgramUniform1d;
PFNGLPROGRAMUNIFORM1DVPROC glad_glProgramUniform1dv;
void APIENTRY glad_debug_impl_glProgramUniform1dv(GLuint arg0, GLint arg1, GLsizei arg2, const GLdouble * arg3) {    
    _pre_call_callback("glProgramUniform1dv", (void*)glProgramUniform1dv, 4, arg0, arg1, arg2, arg3);
     glad_glProgramUniform1dv(arg0, arg1, arg2, arg3);
    _post_call_callback("glProgramUniform1dv", (void*)glProgramUniform1dv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLPROGRAMUNIFORM1DVPROC glad_debug_glProgramUniform1dv = glad_debug_impl_glProgramUniform1dv;
PFNGLPROGRAMUNIFORM1FPROC glad_glProgramUniform1f;
void APIENTRY glad_debug_impl_glProgramUniform1f(GLuint arg0, GLint arg1, GLfloat arg2) {    
    _pre_call_callback("glProgramUniform1f", (void*)glProgramUniform1f, 3, arg0, arg1, arg2);
     glad_glProgramUniform1f(arg0, arg1, arg2);
    _post_call_callback("glProgramUniform1f", (void*)glProgramUniform1f, 3, arg0, arg1, arg2);
    
}
PFNGLPROGRAMUNIFORM1FPROC glad_debug_glProgramUniform1f = glad_debug_impl_glProgramUniform1f;
PFNGLPROGRAMUNIFORM1FVPROC glad_glProgramUniform1fv;
void APIENTRY glad_debug_impl_glProgramUniform1fv(GLuint arg0, GLint arg1, GLsizei arg2, const GLfloat * arg3) {    
    _pre_call_callback("glProgramUniform1fv", (void*)glProgramUniform1fv, 4, arg0, arg1, arg2, arg3);
     glad_glProgramUniform1fv(arg0, arg1, arg2, arg3);
    _post_call_callback("glProgramUniform1fv", (void*)glProgramUniform1fv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLPROGRAMUNIFORM1FVPROC glad_debug_glProgramUniform1fv = glad_debug_impl_glProgramUniform1fv;
PFNGLPROGRAMUNIFORM1IPROC glad_glProgramUniform1i;
void APIENTRY glad_debug_impl_glProgramUniform1i(GLuint arg0, GLint arg1, GLint arg2) {    
    _pre_call_callback("glProgramUniform1i", (void*)glProgramUniform1i, 3, arg0, arg1, arg2);
     glad_glProgramUniform1i(arg0, arg1, arg2);
    _post_call_callback("glProgramUniform1i", (void*)glProgramUniform1i, 3, arg0, arg1, arg2);
    
}
PFNGLPROGRAMUNIFORM1IPROC glad_debug_glProgramUniform1i = glad_debug_impl_glProgramUniform1i;
PFNGLPROGRAMUNIFORM1IVPROC glad_glProgramUniform1iv;
void APIENTRY glad_debug_impl_glProgramUniform1iv(GLuint arg0, GLint arg1, GLsizei arg2, const GLint * arg3) {    
    _pre_call_callback("glProgramUniform1iv", (void*)glProgramUniform1iv, 4, arg0, arg1, arg2, arg3);
     glad_glProgramUniform1iv(arg0, arg1, arg2, arg3);
    _post_call_callback("glProgramUniform1iv", (void*)glProgramUniform1iv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLPROGRAMUNIFORM1IVPROC glad_debug_glProgramUniform1iv = glad_debug_impl_glProgramUniform1iv;
PFNGLPROGRAMUNIFORM1UIPROC glad_glProgramUniform1ui;
void APIENTRY glad_debug_impl_glProgramUniform1ui(GLuint arg0, GLint arg1, GLuint arg2) {    
    _pre_call_callback("glProgramUniform1ui", (void*)glProgramUniform1ui, 3, arg0, arg1, arg2);
     glad_glProgramUniform1ui(arg0, arg1, arg2);
    _post_call_callback("glProgramUniform1ui", (void*)glProgramUniform1ui, 3, arg0, arg1, arg2);
    
}
PFNGLPROGRAMUNIFORM1UIPROC glad_debug_glProgramUniform1ui = glad_debug_impl_glProgramUniform1ui;
PFNGLPROGRAMUNIFORM1UIVPROC glad_glProgramUniform1uiv;
void APIENTRY glad_debug_impl_glProgramUniform1uiv(GLuint arg0, GLint arg1, GLsizei arg2, const GLuint * arg3) {    
    _pre_call_callback("glProgramUniform1uiv", (void*)glProgramUniform1uiv, 4, arg0, arg1, arg2, arg3);
     glad_glProgramUniform1uiv(arg0, arg1, arg2, arg3);
    _post_call_callback("glProgramUniform1uiv", (void*)glProgramUniform1uiv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLPROGRAMUNIFORM1UIVPROC glad_debug_glProgramUniform1uiv = glad_debug_impl_glProgramUniform1uiv;
PFNGLPROGRAMUNIFORM2DPROC glad_glProgramUniform2d;
void APIENTRY glad_debug_impl_glProgramUniform2d(GLuint arg0, GLint arg1, GLdouble arg2, GLdouble arg3) {    
    _pre_call_callback("glProgramUniform2d", (void*)glProgramUniform2d, 4, arg0, arg1, arg2, arg3);
     glad_glProgramUniform2d(arg0, arg1, arg2, arg3);
    _post_call_callback("glProgramUniform2d", (void*)glProgramUniform2d, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLPROGRAMUNIFORM2DPROC glad_debug_glProgramUniform2d = glad_debug_impl_glProgramUniform2d;
PFNGLPROGRAMUNIFORM2DVPROC glad_glProgramUniform2dv;
void APIENTRY glad_debug_impl_glProgramUniform2dv(GLuint arg0, GLint arg1, GLsizei arg2, const GLdouble * arg3) {    
    _pre_call_callback("glProgramUniform2dv", (void*)glProgramUniform2dv, 4, arg0, arg1, arg2, arg3);
     glad_glProgramUniform2dv(arg0, arg1, arg2, arg3);
    _post_call_callback("glProgramUniform2dv", (void*)glProgramUniform2dv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLPROGRAMUNIFORM2DVPROC glad_debug_glProgramUniform2dv = glad_debug_impl_glProgramUniform2dv;
PFNGLPROGRAMUNIFORM2FPROC glad_glProgramUniform2f;
void APIENTRY glad_debug_impl_glProgramUniform2f(GLuint arg0, GLint arg1, GLfloat arg2, GLfloat arg3) {    
    _pre_call_callback("glProgramUniform2f", (void*)glProgramUniform2f, 4, arg0, arg1, arg2, arg3);
     glad_glProgramUniform2f(arg0, arg1, arg2, arg3);
    _post_call_callback("glProgramUniform2f", (void*)glProgramUniform2f, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLPROGRAMUNIFORM2FPROC glad_debug_glProgramUniform2f = glad_debug_impl_glProgramUniform2f;
PFNGLPROGRAMUNIFORM2FVPROC glad_glProgramUniform2fv;
void APIENTRY glad_debug_impl_glProgramUniform2fv(GLuint arg0, GLint arg1, GLsizei arg2, const GLfloat * arg3) {    
    _pre_call_callback("glProgramUniform2fv", (void*)glProgramUniform2fv, 4, arg0, arg1, arg2, arg3);
     glad_glProgramUniform2fv(arg0, arg1, arg2, arg3);
    _post_call_callback("glProgramUniform2fv", (void*)glProgramUniform2fv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLPROGRAMUNIFORM2FVPROC glad_debug_glProgramUniform2fv = glad_debug_impl_glProgramUniform2fv;
PFNGLPROGRAMUNIFORM2IPROC glad_glProgramUniform2i;
void APIENTRY glad_debug_impl_glProgramUniform2i(GLuint arg0, GLint arg1, GLint arg2, GLint arg3) {    
    _pre_call_callback("glProgramUniform2i", (void*)glProgramUniform2i, 4, arg0, arg1, arg2, arg3);
     glad_glProgramUniform2i(arg0, arg1, arg2, arg3);
    _post_call_callback("glProgramUniform2i", (void*)glProgramUniform2i, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLPROGRAMUNIFORM2IPROC glad_debug_glProgramUniform2i = glad_debug_impl_glProgramUniform2i;
PFNGLPROGRAMUNIFORM2IVPROC glad_glProgramUniform2iv;
void APIENTRY glad_debug_impl_glProgramUniform2iv(GLuint arg0, GLint arg1, GLsizei arg2, const GLint * arg3) {    
    _pre_call_callback("glProgramUniform2iv", (void*)glProgramUniform2iv, 4, arg0, arg1, arg2, arg3);
     glad_glProgramUniform2iv(arg0, arg1, arg2, arg3);
    _post_call_callback("glProgramUniform2iv", (void*)glProgramUniform2iv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLPROGRAMUNIFORM2IVPROC glad_debug_glProgramUniform2iv = glad_debug_impl_glProgramUniform2iv;
PFNGLPROGRAMUNIFORM2UIPROC glad_glProgramUniform2ui;
void APIENTRY glad_debug_impl_glProgramUniform2ui(GLuint arg0, GLint arg1, GLuint arg2, GLuint arg3) {    
    _pre_call_callback("glProgramUniform2ui", (void*)glProgramUniform2ui, 4, arg0, arg1, arg2, arg3);
     glad_glProgramUniform2ui(arg0, arg1, arg2, arg3);
    _post_call_callback("glProgramUniform2ui", (void*)glProgramUniform2ui, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLPROGRAMUNIFORM2UIPROC glad_debug_glProgramUniform2ui = glad_debug_impl_glProgramUniform2ui;
PFNGLPROGRAMUNIFORM2UIVPROC glad_glProgramUniform2uiv;
void APIENTRY glad_debug_impl_glProgramUniform2uiv(GLuint arg0, GLint arg1, GLsizei arg2, const GLuint * arg3) {    
    _pre_call_callback("glProgramUniform2uiv", (void*)glProgramUniform2uiv, 4, arg0, arg1, arg2, arg3);
     glad_glProgramUniform2uiv(arg0, arg1, arg2, arg3);
    _post_call_callback("glProgramUniform2uiv", (void*)glProgramUniform2uiv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLPROGRAMUNIFORM2UIVPROC glad_debug_glProgramUniform2uiv = glad_debug_impl_glProgramUniform2uiv;
PFNGLPROGRAMUNIFORM3DPROC glad_glProgramUniform3d;
void APIENTRY glad_debug_impl_glProgramUniform3d(GLuint arg0, GLint arg1, GLdouble arg2, GLdouble arg3, GLdouble arg4) {    
    _pre_call_callback("glProgramUniform3d", (void*)glProgramUniform3d, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glProgramUniform3d(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glProgramUniform3d", (void*)glProgramUniform3d, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLPROGRAMUNIFORM3DPROC glad_debug_glProgramUniform3d = glad_debug_impl_glProgramUniform3d;
PFNGLPROGRAMUNIFORM3DVPROC glad_glProgramUniform3dv;
void APIENTRY glad_debug_impl_glProgramUniform3dv(GLuint arg0, GLint arg1, GLsizei arg2, const GLdouble * arg3) {    
    _pre_call_callback("glProgramUniform3dv", (void*)glProgramUniform3dv, 4, arg0, arg1, arg2, arg3);
     glad_glProgramUniform3dv(arg0, arg1, arg2, arg3);
    _post_call_callback("glProgramUniform3dv", (void*)glProgramUniform3dv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLPROGRAMUNIFORM3DVPROC glad_debug_glProgramUniform3dv = glad_debug_impl_glProgramUniform3dv;
PFNGLPROGRAMUNIFORM3FPROC glad_glProgramUniform3f;
void APIENTRY glad_debug_impl_glProgramUniform3f(GLuint arg0, GLint arg1, GLfloat arg2, GLfloat arg3, GLfloat arg4) {    
    _pre_call_callback("glProgramUniform3f", (void*)glProgramUniform3f, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glProgramUniform3f(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glProgramUniform3f", (void*)glProgramUniform3f, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLPROGRAMUNIFORM3FPROC glad_debug_glProgramUniform3f = glad_debug_impl_glProgramUniform3f;
PFNGLPROGRAMUNIFORM3FVPROC glad_glProgramUniform3fv;
void APIENTRY glad_debug_impl_glProgramUniform3fv(GLuint arg0, GLint arg1, GLsizei arg2, const GLfloat * arg3) {    
    _pre_call_callback("glProgramUniform3fv", (void*)glProgramUniform3fv, 4, arg0, arg1, arg2, arg3);
     glad_glProgramUniform3fv(arg0, arg1, arg2, arg3);
    _post_call_callback("glProgramUniform3fv", (void*)glProgramUniform3fv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLPROGRAMUNIFORM3FVPROC glad_debug_glProgramUniform3fv = glad_debug_impl_glProgramUniform3fv;
PFNGLPROGRAMUNIFORM3IPROC glad_glProgramUniform3i;
void APIENTRY glad_debug_impl_glProgramUniform3i(GLuint arg0, GLint arg1, GLint arg2, GLint arg3, GLint arg4) {    
    _pre_call_callback("glProgramUniform3i", (void*)glProgramUniform3i, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glProgramUniform3i(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glProgramUniform3i", (void*)glProgramUniform3i, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLPROGRAMUNIFORM3IPROC glad_debug_glProgramUniform3i = glad_debug_impl_glProgramUniform3i;
PFNGLPROGRAMUNIFORM3IVPROC glad_glProgramUniform3iv;
void APIENTRY glad_debug_impl_glProgramUniform3iv(GLuint arg0, GLint arg1, GLsizei arg2, const GLint * arg3) {    
    _pre_call_callback("glProgramUniform3iv", (void*)glProgramUniform3iv, 4, arg0, arg1, arg2, arg3);
     glad_glProgramUniform3iv(arg0, arg1, arg2, arg3);
    _post_call_callback("glProgramUniform3iv", (void*)glProgramUniform3iv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLPROGRAMUNIFORM3IVPROC glad_debug_glProgramUniform3iv = glad_debug_impl_glProgramUniform3iv;
PFNGLPROGRAMUNIFORM3UIPROC glad_glProgramUniform3ui;
void APIENTRY glad_debug_impl_glProgramUniform3ui(GLuint arg0, GLint arg1, GLuint arg2, GLuint arg3, GLuint arg4) {    
    _pre_call_callback("glProgramUniform3ui", (void*)glProgramUniform3ui, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glProgramUniform3ui(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glProgramUniform3ui", (void*)glProgramUniform3ui, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLPROGRAMUNIFORM3UIPROC glad_debug_glProgramUniform3ui = glad_debug_impl_glProgramUniform3ui;
PFNGLPROGRAMUNIFORM3UIVPROC glad_glProgramUniform3uiv;
void APIENTRY glad_debug_impl_glProgramUniform3uiv(GLuint arg0, GLint arg1, GLsizei arg2, const GLuint * arg3) {    
    _pre_call_callback("glProgramUniform3uiv", (void*)glProgramUniform3uiv, 4, arg0, arg1, arg2, arg3);
     glad_glProgramUniform3uiv(arg0, arg1, arg2, arg3);
    _post_call_callback("glProgramUniform3uiv", (void*)glProgramUniform3uiv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLPROGRAMUNIFORM3UIVPROC glad_debug_glProgramUniform3uiv = glad_debug_impl_glProgramUniform3uiv;
PFNGLPROGRAMUNIFORM4DPROC glad_glProgramUniform4d;
void APIENTRY glad_debug_impl_glProgramUniform4d(GLuint arg0, GLint arg1, GLdouble arg2, GLdouble arg3, GLdouble arg4, GLdouble arg5) {    
    _pre_call_callback("glProgramUniform4d", (void*)glProgramUniform4d, 6, arg0, arg1, arg2, arg3, arg4, arg5);
     glad_glProgramUniform4d(arg0, arg1, arg2, arg3, arg4, arg5);
    _post_call_callback("glProgramUniform4d", (void*)glProgramUniform4d, 6, arg0, arg1, arg2, arg3, arg4, arg5);
    
}
PFNGLPROGRAMUNIFORM4DPROC glad_debug_glProgramUniform4d = glad_debug_impl_glProgramUniform4d;
PFNGLPROGRAMUNIFORM4DVPROC glad_glProgramUniform4dv;
void APIENTRY glad_debug_impl_glProgramUniform4dv(GLuint arg0, GLint arg1, GLsizei arg2, const GLdouble * arg3) {    
    _pre_call_callback("glProgramUniform4dv", (void*)glProgramUniform4dv, 4, arg0, arg1, arg2, arg3);
     glad_glProgramUniform4dv(arg0, arg1, arg2, arg3);
    _post_call_callback("glProgramUniform4dv", (void*)glProgramUniform4dv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLPROGRAMUNIFORM4DVPROC glad_debug_glProgramUniform4dv = glad_debug_impl_glProgramUniform4dv;
PFNGLPROGRAMUNIFORM4FPROC glad_glProgramUniform4f;
void APIENTRY glad_debug_impl_glProgramUniform4f(GLuint arg0, GLint arg1, GLfloat arg2, GLfloat arg3, GLfloat arg4, GLfloat arg5) {    
    _pre_call_callback("glProgramUniform4f", (void*)glProgramUniform4f, 6, arg0, arg1, arg2, arg3, arg4, arg5);
     glad_glProgramUniform4f(arg0, arg1, arg2, arg3, arg4, arg5);
    _post_call_callback("glProgramUniform4f", (void*)glProgramUniform4f, 6, arg0, arg1, arg2, arg3, arg4, arg5);
    
}
PFNGLPROGRAMUNIFORM4FPROC glad_debug_glProgramUniform4f = glad_debug_impl_glProgramUniform4f;
PFNGLPROGRAMUNIFORM4FVPROC glad_glProgramUniform4fv;
void APIENTRY glad_debug_impl_glProgramUniform4fv(GLuint arg0, GLint arg1, GLsizei arg2, const GLfloat * arg3) {    
    _pre_call_callback("glProgramUniform4fv", (void*)glProgramUniform4fv, 4, arg0, arg1, arg2, arg3);
     glad_glProgramUniform4fv(arg0, arg1, arg2, arg3);
    _post_call_callback("glProgramUniform4fv", (void*)glProgramUniform4fv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLPROGRAMUNIFORM4FVPROC glad_debug_glProgramUniform4fv = glad_debug_impl_glProgramUniform4fv;
PFNGLPROGRAMUNIFORM4IPROC glad_glProgramUniform4i;
void APIENTRY glad_debug_impl_glProgramUniform4i(GLuint arg0, GLint arg1, GLint arg2, GLint arg3, GLint arg4, GLint arg5) {    
    _pre_call_callback("glProgramUniform4i", (void*)glProgramUniform4i, 6, arg0, arg1, arg2, arg3, arg4, arg5);
     glad_glProgramUniform4i(arg0, arg1, arg2, arg3, arg4, arg5);
    _post_call_callback("glProgramUniform4i", (void*)glProgramUniform4i, 6, arg0, arg1, arg2, arg3, arg4, arg5);
    
}
PFNGLPROGRAMUNIFORM4IPROC glad_debug_glProgramUniform4i = glad_debug_impl_glProgramUniform4i;
PFNGLPROGRAMUNIFORM4IVPROC glad_glProgramUniform4iv;
void APIENTRY glad_debug_impl_glProgramUniform4iv(GLuint arg0, GLint arg1, GLsizei arg2, const GLint * arg3) {    
    _pre_call_callback("glProgramUniform4iv", (void*)glProgramUniform4iv, 4, arg0, arg1, arg2, arg3);
     glad_glProgramUniform4iv(arg0, arg1, arg2, arg3);
    _post_call_callback("glProgramUniform4iv", (void*)glProgramUniform4iv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLPROGRAMUNIFORM4IVPROC glad_debug_glProgramUniform4iv = glad_debug_impl_glProgramUniform4iv;
PFNGLPROGRAMUNIFORM4UIPROC glad_glProgramUniform4ui;
void APIENTRY glad_debug_impl_glProgramUniform4ui(GLuint arg0, GLint arg1, GLuint arg2, GLuint arg3, GLuint arg4, GLuint arg5) {    
    _pre_call_callback("glProgramUniform4ui", (void*)glProgramUniform4ui, 6, arg0, arg1, arg2, arg3, arg4, arg5);
     glad_glProgramUniform4ui(arg0, arg1, arg2, arg3, arg4, arg5);
    _post_call_callback("glProgramUniform4ui", (void*)glProgramUniform4ui, 6, arg0, arg1, arg2, arg3, arg4, arg5);
    
}
PFNGLPROGRAMUNIFORM4UIPROC glad_debug_glProgramUniform4ui = glad_debug_impl_glProgramUniform4ui;
PFNGLPROGRAMUNIFORM4UIVPROC glad_glProgramUniform4uiv;
void APIENTRY glad_debug_impl_glProgramUniform4uiv(GLuint arg0, GLint arg1, GLsizei arg2, const GLuint * arg3) {    
    _pre_call_callback("glProgramUniform4uiv", (void*)glProgramUniform4uiv, 4, arg0, arg1, arg2, arg3);
     glad_glProgramUniform4uiv(arg0, arg1, arg2, arg3);
    _post_call_callback("glProgramUniform4uiv", (void*)glProgramUniform4uiv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLPROGRAMUNIFORM4UIVPROC glad_debug_glProgramUniform4uiv = glad_debug_impl_glProgramUniform4uiv;
PFNGLPROGRAMUNIFORMMATRIX2DVPROC glad_glProgramUniformMatrix2dv;
void APIENTRY glad_debug_impl_glProgramUniformMatrix2dv(GLuint arg0, GLint arg1, GLsizei arg2, GLboolean arg3, const GLdouble * arg4) {    
    _pre_call_callback("glProgramUniformMatrix2dv", (void*)glProgramUniformMatrix2dv, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glProgramUniformMatrix2dv(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glProgramUniformMatrix2dv", (void*)glProgramUniformMatrix2dv, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLPROGRAMUNIFORMMATRIX2DVPROC glad_debug_glProgramUniformMatrix2dv = glad_debug_impl_glProgramUniformMatrix2dv;
PFNGLPROGRAMUNIFORMMATRIX2FVPROC glad_glProgramUniformMatrix2fv;
void APIENTRY glad_debug_impl_glProgramUniformMatrix2fv(GLuint arg0, GLint arg1, GLsizei arg2, GLboolean arg3, const GLfloat * arg4) {    
    _pre_call_callback("glProgramUniformMatrix2fv", (void*)glProgramUniformMatrix2fv, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glProgramUniformMatrix2fv(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glProgramUniformMatrix2fv", (void*)glProgramUniformMatrix2fv, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLPROGRAMUNIFORMMATRIX2FVPROC glad_debug_glProgramUniformMatrix2fv = glad_debug_impl_glProgramUniformMatrix2fv;
PFNGLPROGRAMUNIFORMMATRIX2X3DVPROC glad_glProgramUniformMatrix2x3dv;
void APIENTRY glad_debug_impl_glProgramUniformMatrix2x3dv(GLuint arg0, GLint arg1, GLsizei arg2, GLboolean arg3, const GLdouble * arg4) {    
    _pre_call_callback("glProgramUniformMatrix2x3dv", (void*)glProgramUniformMatrix2x3dv, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glProgramUniformMatrix2x3dv(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glProgramUniformMatrix2x3dv", (void*)glProgramUniformMatrix2x3dv, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLPROGRAMUNIFORMMATRIX2X3DVPROC glad_debug_glProgramUniformMatrix2x3dv = glad_debug_impl_glProgramUniformMatrix2x3dv;
PFNGLPROGRAMUNIFORMMATRIX2X3FVPROC glad_glProgramUniformMatrix2x3fv;
void APIENTRY glad_debug_impl_glProgramUniformMatrix2x3fv(GLuint arg0, GLint arg1, GLsizei arg2, GLboolean arg3, const GLfloat * arg4) {    
    _pre_call_callback("glProgramUniformMatrix2x3fv", (void*)glProgramUniformMatrix2x3fv, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glProgramUniformMatrix2x3fv(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glProgramUniformMatrix2x3fv", (void*)glProgramUniformMatrix2x3fv, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLPROGRAMUNIFORMMATRIX2X3FVPROC glad_debug_glProgramUniformMatrix2x3fv = glad_debug_impl_glProgramUniformMatrix2x3fv;
PFNGLPROGRAMUNIFORMMATRIX2X4DVPROC glad_glProgramUniformMatrix2x4dv;
void APIENTRY glad_debug_impl_glProgramUniformMatrix2x4dv(GLuint arg0, GLint arg1, GLsizei arg2, GLboolean arg3, const GLdouble * arg4) {    
    _pre_call_callback("glProgramUniformMatrix2x4dv", (void*)glProgramUniformMatrix2x4dv, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glProgramUniformMatrix2x4dv(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glProgramUniformMatrix2x4dv", (void*)glProgramUniformMatrix2x4dv, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLPROGRAMUNIFORMMATRIX2X4DVPROC glad_debug_glProgramUniformMatrix2x4dv = glad_debug_impl_glProgramUniformMatrix2x4dv;
PFNGLPROGRAMUNIFORMMATRIX2X4FVPROC glad_glProgramUniformMatrix2x4fv;
void APIENTRY glad_debug_impl_glProgramUniformMatrix2x4fv(GLuint arg0, GLint arg1, GLsizei arg2, GLboolean arg3, const GLfloat * arg4) {    
    _pre_call_callback("glProgramUniformMatrix2x4fv", (void*)glProgramUniformMatrix2x4fv, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glProgramUniformMatrix2x4fv(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glProgramUniformMatrix2x4fv", (void*)glProgramUniformMatrix2x4fv, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLPROGRAMUNIFORMMATRIX2X4FVPROC glad_debug_glProgramUniformMatrix2x4fv = glad_debug_impl_glProgramUniformMatrix2x4fv;
PFNGLPROGRAMUNIFORMMATRIX3DVPROC glad_glProgramUniformMatrix3dv;
void APIENTRY glad_debug_impl_glProgramUniformMatrix3dv(GLuint arg0, GLint arg1, GLsizei arg2, GLboolean arg3, const GLdouble * arg4) {    
    _pre_call_callback("glProgramUniformMatrix3dv", (void*)glProgramUniformMatrix3dv, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glProgramUniformMatrix3dv(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glProgramUniformMatrix3dv", (void*)glProgramUniformMatrix3dv, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLPROGRAMUNIFORMMATRIX3DVPROC glad_debug_glProgramUniformMatrix3dv = glad_debug_impl_glProgramUniformMatrix3dv;
PFNGLPROGRAMUNIFORMMATRIX3FVPROC glad_glProgramUniformMatrix3fv;
void APIENTRY glad_debug_impl_glProgramUniformMatrix3fv(GLuint arg0, GLint arg1, GLsizei arg2, GLboolean arg3, const GLfloat * arg4) {    
    _pre_call_callback("glProgramUniformMatrix3fv", (void*)glProgramUniformMatrix3fv, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glProgramUniformMatrix3fv(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glProgramUniformMatrix3fv", (void*)glProgramUniformMatrix3fv, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLPROGRAMUNIFORMMATRIX3FVPROC glad_debug_glProgramUniformMatrix3fv = glad_debug_impl_glProgramUniformMatrix3fv;
PFNGLPROGRAMUNIFORMMATRIX3X2DVPROC glad_glProgramUniformMatrix3x2dv;
void APIENTRY glad_debug_impl_glProgramUniformMatrix3x2dv(GLuint arg0, GLint arg1, GLsizei arg2, GLboolean arg3, const GLdouble * arg4) {    
    _pre_call_callback("glProgramUniformMatrix3x2dv", (void*)glProgramUniformMatrix3x2dv, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glProgramUniformMatrix3x2dv(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glProgramUniformMatrix3x2dv", (void*)glProgramUniformMatrix3x2dv, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLPROGRAMUNIFORMMATRIX3X2DVPROC glad_debug_glProgramUniformMatrix3x2dv = glad_debug_impl_glProgramUniformMatrix3x2dv;
PFNGLPROGRAMUNIFORMMATRIX3X2FVPROC glad_glProgramUniformMatrix3x2fv;
void APIENTRY glad_debug_impl_glProgramUniformMatrix3x2fv(GLuint arg0, GLint arg1, GLsizei arg2, GLboolean arg3, const GLfloat * arg4) {    
    _pre_call_callback("glProgramUniformMatrix3x2fv", (void*)glProgramUniformMatrix3x2fv, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glProgramUniformMatrix3x2fv(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glProgramUniformMatrix3x2fv", (void*)glProgramUniformMatrix3x2fv, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLPROGRAMUNIFORMMATRIX3X2FVPROC glad_debug_glProgramUniformMatrix3x2fv = glad_debug_impl_glProgramUniformMatrix3x2fv;
PFNGLPROGRAMUNIFORMMATRIX3X4DVPROC glad_glProgramUniformMatrix3x4dv;
void APIENTRY glad_debug_impl_glProgramUniformMatrix3x4dv(GLuint arg0, GLint arg1, GLsizei arg2, GLboolean arg3, const GLdouble * arg4) {    
    _pre_call_callback("glProgramUniformMatrix3x4dv", (void*)glProgramUniformMatrix3x4dv, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glProgramUniformMatrix3x4dv(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glProgramUniformMatrix3x4dv", (void*)glProgramUniformMatrix3x4dv, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLPROGRAMUNIFORMMATRIX3X4DVPROC glad_debug_glProgramUniformMatrix3x4dv = glad_debug_impl_glProgramUniformMatrix3x4dv;
PFNGLPROGRAMUNIFORMMATRIX3X4FVPROC glad_glProgramUniformMatrix3x4fv;
void APIENTRY glad_debug_impl_glProgramUniformMatrix3x4fv(GLuint arg0, GLint arg1, GLsizei arg2, GLboolean arg3, const GLfloat * arg4) {    
    _pre_call_callback("glProgramUniformMatrix3x4fv", (void*)glProgramUniformMatrix3x4fv, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glProgramUniformMatrix3x4fv(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glProgramUniformMatrix3x4fv", (void*)glProgramUniformMatrix3x4fv, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLPROGRAMUNIFORMMATRIX3X4FVPROC glad_debug_glProgramUniformMatrix3x4fv = glad_debug_impl_glProgramUniformMatrix3x4fv;
PFNGLPROGRAMUNIFORMMATRIX4DVPROC glad_glProgramUniformMatrix4dv;
void APIENTRY glad_debug_impl_glProgramUniformMatrix4dv(GLuint arg0, GLint arg1, GLsizei arg2, GLboolean arg3, const GLdouble * arg4) {    
    _pre_call_callback("glProgramUniformMatrix4dv", (void*)glProgramUniformMatrix4dv, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glProgramUniformMatrix4dv(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glProgramUniformMatrix4dv", (void*)glProgramUniformMatrix4dv, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLPROGRAMUNIFORMMATRIX4DVPROC glad_debug_glProgramUniformMatrix4dv = glad_debug_impl_glProgramUniformMatrix4dv;
PFNGLPROGRAMUNIFORMMATRIX4FVPROC glad_glProgramUniformMatrix4fv;
void APIENTRY glad_debug_impl_glProgramUniformMatrix4fv(GLuint arg0, GLint arg1, GLsizei arg2, GLboolean arg3, const GLfloat * arg4) {    
    _pre_call_callback("glProgramUniformMatrix4fv", (void*)glProgramUniformMatrix4fv, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glProgramUniformMatrix4fv(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glProgramUniformMatrix4fv", (void*)glProgramUniformMatrix4fv, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLPROGRAMUNIFORMMATRIX4FVPROC glad_debug_glProgramUniformMatrix4fv = glad_debug_impl_glProgramUniformMatrix4fv;
PFNGLPROGRAMUNIFORMMATRIX4X2DVPROC glad_glProgramUniformMatrix4x2dv;
void APIENTRY glad_debug_impl_glProgramUniformMatrix4x2dv(GLuint arg0, GLint arg1, GLsizei arg2, GLboolean arg3, const GLdouble * arg4) {    
    _pre_call_callback("glProgramUniformMatrix4x2dv", (void*)glProgramUniformMatrix4x2dv, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glProgramUniformMatrix4x2dv(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glProgramUniformMatrix4x2dv", (void*)glProgramUniformMatrix4x2dv, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLPROGRAMUNIFORMMATRIX4X2DVPROC glad_debug_glProgramUniformMatrix4x2dv = glad_debug_impl_glProgramUniformMatrix4x2dv;
PFNGLPROGRAMUNIFORMMATRIX4X2FVPROC glad_glProgramUniformMatrix4x2fv;
void APIENTRY glad_debug_impl_glProgramUniformMatrix4x2fv(GLuint arg0, GLint arg1, GLsizei arg2, GLboolean arg3, const GLfloat * arg4) {    
    _pre_call_callback("glProgramUniformMatrix4x2fv", (void*)glProgramUniformMatrix4x2fv, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glProgramUniformMatrix4x2fv(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glProgramUniformMatrix4x2fv", (void*)glProgramUniformMatrix4x2fv, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLPROGRAMUNIFORMMATRIX4X2FVPROC glad_debug_glProgramUniformMatrix4x2fv = glad_debug_impl_glProgramUniformMatrix4x2fv;
PFNGLPROGRAMUNIFORMMATRIX4X3DVPROC glad_glProgramUniformMatrix4x3dv;
void APIENTRY glad_debug_impl_glProgramUniformMatrix4x3dv(GLuint arg0, GLint arg1, GLsizei arg2, GLboolean arg3, const GLdouble * arg4) {    
    _pre_call_callback("glProgramUniformMatrix4x3dv", (void*)glProgramUniformMatrix4x3dv, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glProgramUniformMatrix4x3dv(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glProgramUniformMatrix4x3dv", (void*)glProgramUniformMatrix4x3dv, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLPROGRAMUNIFORMMATRIX4X3DVPROC glad_debug_glProgramUniformMatrix4x3dv = glad_debug_impl_glProgramUniformMatrix4x3dv;
PFNGLPROGRAMUNIFORMMATRIX4X3FVPROC glad_glProgramUniformMatrix4x3fv;
void APIENTRY glad_debug_impl_glProgramUniformMatrix4x3fv(GLuint arg0, GLint arg1, GLsizei arg2, GLboolean arg3, const GLfloat * arg4) {    
    _pre_call_callback("glProgramUniformMatrix4x3fv", (void*)glProgramUniformMatrix4x3fv, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glProgramUniformMatrix4x3fv(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glProgramUniformMatrix4x3fv", (void*)glProgramUniformMatrix4x3fv, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLPROGRAMUNIFORMMATRIX4X3FVPROC glad_debug_glProgramUniformMatrix4x3fv = glad_debug_impl_glProgramUniformMatrix4x3fv;
PFNGLPROVOKINGVERTEXPROC glad_glProvokingVertex;
void APIENTRY glad_debug_impl_glProvokingVertex(GLenum arg0) {    
    _pre_call_callback("glProvokingVertex", (void*)glProvokingVertex, 1, arg0);
     glad_glProvokingVertex(arg0);
    _post_call_callback("glProvokingVertex", (void*)glProvokingVertex, 1, arg0);
    
}
PFNGLPROVOKINGVERTEXPROC glad_debug_glProvokingVertex = glad_debug_impl_glProvokingVertex;
PFNGLPUSHATTRIBPROC glad_glPushAttrib;
void APIENTRY glad_debug_impl_glPushAttrib(GLbitfield arg0) {    
    _pre_call_callback("glPushAttrib", (void*)glPushAttrib, 1, arg0);
     glad_glPushAttrib(arg0);
    _post_call_callback("glPushAttrib", (void*)glPushAttrib, 1, arg0);
    
}
PFNGLPUSHATTRIBPROC glad_debug_glPushAttrib = glad_debug_impl_glPushAttrib;
PFNGLPUSHCLIENTATTRIBPROC glad_glPushClientAttrib;
void APIENTRY glad_debug_impl_glPushClientAttrib(GLbitfield arg0) {    
    _pre_call_callback("glPushClientAttrib", (void*)glPushClientAttrib, 1, arg0);
     glad_glPushClientAttrib(arg0);
    _post_call_callback("glPushClientAttrib", (void*)glPushClientAttrib, 1, arg0);
    
}
PFNGLPUSHCLIENTATTRIBPROC glad_debug_glPushClientAttrib = glad_debug_impl_glPushClientAttrib;
PFNGLPUSHDEBUGGROUPPROC glad_glPushDebugGroup;
void APIENTRY glad_debug_impl_glPushDebugGroup(GLenum arg0, GLuint arg1, GLsizei arg2, const GLchar * arg3) {    
    _pre_call_callback("glPushDebugGroup", (void*)glPushDebugGroup, 4, arg0, arg1, arg2, arg3);
     glad_glPushDebugGroup(arg0, arg1, arg2, arg3);
    _post_call_callback("glPushDebugGroup", (void*)glPushDebugGroup, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLPUSHDEBUGGROUPPROC glad_debug_glPushDebugGroup = glad_debug_impl_glPushDebugGroup;
PFNGLPUSHMATRIXPROC glad_glPushMatrix;
void APIENTRY glad_debug_impl_glPushMatrix(void) {    
    _pre_call_callback("glPushMatrix", (void*)glPushMatrix, 0);
     glad_glPushMatrix();
    _post_call_callback("glPushMatrix", (void*)glPushMatrix, 0);
    
}
PFNGLPUSHMATRIXPROC glad_debug_glPushMatrix = glad_debug_impl_glPushMatrix;
PFNGLPUSHNAMEPROC glad_glPushName;
void APIENTRY glad_debug_impl_glPushName(GLuint arg0) {    
    _pre_call_callback("glPushName", (void*)glPushName, 1, arg0);
     glad_glPushName(arg0);
    _post_call_callback("glPushName", (void*)glPushName, 1, arg0);
    
}
PFNGLPUSHNAMEPROC glad_debug_glPushName = glad_debug_impl_glPushName;
PFNGLQUERYCOUNTERPROC glad_glQueryCounter;
void APIENTRY glad_debug_impl_glQueryCounter(GLuint arg0, GLenum arg1) {    
    _pre_call_callback("glQueryCounter", (void*)glQueryCounter, 2, arg0, arg1);
     glad_glQueryCounter(arg0, arg1);
    _post_call_callback("glQueryCounter", (void*)glQueryCounter, 2, arg0, arg1);
    
}
PFNGLQUERYCOUNTERPROC glad_debug_glQueryCounter = glad_debug_impl_glQueryCounter;
PFNGLRASTERPOS2DPROC glad_glRasterPos2d;
void APIENTRY glad_debug_impl_glRasterPos2d(GLdouble arg0, GLdouble arg1) {    
    _pre_call_callback("glRasterPos2d", (void*)glRasterPos2d, 2, arg0, arg1);
     glad_glRasterPos2d(arg0, arg1);
    _post_call_callback("glRasterPos2d", (void*)glRasterPos2d, 2, arg0, arg1);
    
}
PFNGLRASTERPOS2DPROC glad_debug_glRasterPos2d = glad_debug_impl_glRasterPos2d;
PFNGLRASTERPOS2DVPROC glad_glRasterPos2dv;
void APIENTRY glad_debug_impl_glRasterPos2dv(const GLdouble * arg0) {    
    _pre_call_callback("glRasterPos2dv", (void*)glRasterPos2dv, 1, arg0);
     glad_glRasterPos2dv(arg0);
    _post_call_callback("glRasterPos2dv", (void*)glRasterPos2dv, 1, arg0);
    
}
PFNGLRASTERPOS2DVPROC glad_debug_glRasterPos2dv = glad_debug_impl_glRasterPos2dv;
PFNGLRASTERPOS2FPROC glad_glRasterPos2f;
void APIENTRY glad_debug_impl_glRasterPos2f(GLfloat arg0, GLfloat arg1) {    
    _pre_call_callback("glRasterPos2f", (void*)glRasterPos2f, 2, arg0, arg1);
     glad_glRasterPos2f(arg0, arg1);
    _post_call_callback("glRasterPos2f", (void*)glRasterPos2f, 2, arg0, arg1);
    
}
PFNGLRASTERPOS2FPROC glad_debug_glRasterPos2f = glad_debug_impl_glRasterPos2f;
PFNGLRASTERPOS2FVPROC glad_glRasterPos2fv;
void APIENTRY glad_debug_impl_glRasterPos2fv(const GLfloat * arg0) {    
    _pre_call_callback("glRasterPos2fv", (void*)glRasterPos2fv, 1, arg0);
     glad_glRasterPos2fv(arg0);
    _post_call_callback("glRasterPos2fv", (void*)glRasterPos2fv, 1, arg0);
    
}
PFNGLRASTERPOS2FVPROC glad_debug_glRasterPos2fv = glad_debug_impl_glRasterPos2fv;
PFNGLRASTERPOS2IPROC glad_glRasterPos2i;
void APIENTRY glad_debug_impl_glRasterPos2i(GLint arg0, GLint arg1) {    
    _pre_call_callback("glRasterPos2i", (void*)glRasterPos2i, 2, arg0, arg1);
     glad_glRasterPos2i(arg0, arg1);
    _post_call_callback("glRasterPos2i", (void*)glRasterPos2i, 2, arg0, arg1);
    
}
PFNGLRASTERPOS2IPROC glad_debug_glRasterPos2i = glad_debug_impl_glRasterPos2i;
PFNGLRASTERPOS2IVPROC glad_glRasterPos2iv;
void APIENTRY glad_debug_impl_glRasterPos2iv(const GLint * arg0) {    
    _pre_call_callback("glRasterPos2iv", (void*)glRasterPos2iv, 1, arg0);
     glad_glRasterPos2iv(arg0);
    _post_call_callback("glRasterPos2iv", (void*)glRasterPos2iv, 1, arg0);
    
}
PFNGLRASTERPOS2IVPROC glad_debug_glRasterPos2iv = glad_debug_impl_glRasterPos2iv;
PFNGLRASTERPOS2SPROC glad_glRasterPos2s;
void APIENTRY glad_debug_impl_glRasterPos2s(GLshort arg0, GLshort arg1) {    
    _pre_call_callback("glRasterPos2s", (void*)glRasterPos2s, 2, arg0, arg1);
     glad_glRasterPos2s(arg0, arg1);
    _post_call_callback("glRasterPos2s", (void*)glRasterPos2s, 2, arg0, arg1);
    
}
PFNGLRASTERPOS2SPROC glad_debug_glRasterPos2s = glad_debug_impl_glRasterPos2s;
PFNGLRASTERPOS2SVPROC glad_glRasterPos2sv;
void APIENTRY glad_debug_impl_glRasterPos2sv(const GLshort * arg0) {    
    _pre_call_callback("glRasterPos2sv", (void*)glRasterPos2sv, 1, arg0);
     glad_glRasterPos2sv(arg0);
    _post_call_callback("glRasterPos2sv", (void*)glRasterPos2sv, 1, arg0);
    
}
PFNGLRASTERPOS2SVPROC glad_debug_glRasterPos2sv = glad_debug_impl_glRasterPos2sv;
PFNGLRASTERPOS3DPROC glad_glRasterPos3d;
void APIENTRY glad_debug_impl_glRasterPos3d(GLdouble arg0, GLdouble arg1, GLdouble arg2) {    
    _pre_call_callback("glRasterPos3d", (void*)glRasterPos3d, 3, arg0, arg1, arg2);
     glad_glRasterPos3d(arg0, arg1, arg2);
    _post_call_callback("glRasterPos3d", (void*)glRasterPos3d, 3, arg0, arg1, arg2);
    
}
PFNGLRASTERPOS3DPROC glad_debug_glRasterPos3d = glad_debug_impl_glRasterPos3d;
PFNGLRASTERPOS3DVPROC glad_glRasterPos3dv;
void APIENTRY glad_debug_impl_glRasterPos3dv(const GLdouble * arg0) {    
    _pre_call_callback("glRasterPos3dv", (void*)glRasterPos3dv, 1, arg0);
     glad_glRasterPos3dv(arg0);
    _post_call_callback("glRasterPos3dv", (void*)glRasterPos3dv, 1, arg0);
    
}
PFNGLRASTERPOS3DVPROC glad_debug_glRasterPos3dv = glad_debug_impl_glRasterPos3dv;
PFNGLRASTERPOS3FPROC glad_glRasterPos3f;
void APIENTRY glad_debug_impl_glRasterPos3f(GLfloat arg0, GLfloat arg1, GLfloat arg2) {    
    _pre_call_callback("glRasterPos3f", (void*)glRasterPos3f, 3, arg0, arg1, arg2);
     glad_glRasterPos3f(arg0, arg1, arg2);
    _post_call_callback("glRasterPos3f", (void*)glRasterPos3f, 3, arg0, arg1, arg2);
    
}
PFNGLRASTERPOS3FPROC glad_debug_glRasterPos3f = glad_debug_impl_glRasterPos3f;
PFNGLRASTERPOS3FVPROC glad_glRasterPos3fv;
void APIENTRY glad_debug_impl_glRasterPos3fv(const GLfloat * arg0) {    
    _pre_call_callback("glRasterPos3fv", (void*)glRasterPos3fv, 1, arg0);
     glad_glRasterPos3fv(arg0);
    _post_call_callback("glRasterPos3fv", (void*)glRasterPos3fv, 1, arg0);
    
}
PFNGLRASTERPOS3FVPROC glad_debug_glRasterPos3fv = glad_debug_impl_glRasterPos3fv;
PFNGLRASTERPOS3IPROC glad_glRasterPos3i;
void APIENTRY glad_debug_impl_glRasterPos3i(GLint arg0, GLint arg1, GLint arg2) {    
    _pre_call_callback("glRasterPos3i", (void*)glRasterPos3i, 3, arg0, arg1, arg2);
     glad_glRasterPos3i(arg0, arg1, arg2);
    _post_call_callback("glRasterPos3i", (void*)glRasterPos3i, 3, arg0, arg1, arg2);
    
}
PFNGLRASTERPOS3IPROC glad_debug_glRasterPos3i = glad_debug_impl_glRasterPos3i;
PFNGLRASTERPOS3IVPROC glad_glRasterPos3iv;
void APIENTRY glad_debug_impl_glRasterPos3iv(const GLint * arg0) {    
    _pre_call_callback("glRasterPos3iv", (void*)glRasterPos3iv, 1, arg0);
     glad_glRasterPos3iv(arg0);
    _post_call_callback("glRasterPos3iv", (void*)glRasterPos3iv, 1, arg0);
    
}
PFNGLRASTERPOS3IVPROC glad_debug_glRasterPos3iv = glad_debug_impl_glRasterPos3iv;
PFNGLRASTERPOS3SPROC glad_glRasterPos3s;
void APIENTRY glad_debug_impl_glRasterPos3s(GLshort arg0, GLshort arg1, GLshort arg2) {    
    _pre_call_callback("glRasterPos3s", (void*)glRasterPos3s, 3, arg0, arg1, arg2);
     glad_glRasterPos3s(arg0, arg1, arg2);
    _post_call_callback("glRasterPos3s", (void*)glRasterPos3s, 3, arg0, arg1, arg2);
    
}
PFNGLRASTERPOS3SPROC glad_debug_glRasterPos3s = glad_debug_impl_glRasterPos3s;
PFNGLRASTERPOS3SVPROC glad_glRasterPos3sv;
void APIENTRY glad_debug_impl_glRasterPos3sv(const GLshort * arg0) {    
    _pre_call_callback("glRasterPos3sv", (void*)glRasterPos3sv, 1, arg0);
     glad_glRasterPos3sv(arg0);
    _post_call_callback("glRasterPos3sv", (void*)glRasterPos3sv, 1, arg0);
    
}
PFNGLRASTERPOS3SVPROC glad_debug_glRasterPos3sv = glad_debug_impl_glRasterPos3sv;
PFNGLRASTERPOS4DPROC glad_glRasterPos4d;
void APIENTRY glad_debug_impl_glRasterPos4d(GLdouble arg0, GLdouble arg1, GLdouble arg2, GLdouble arg3) {    
    _pre_call_callback("glRasterPos4d", (void*)glRasterPos4d, 4, arg0, arg1, arg2, arg3);
     glad_glRasterPos4d(arg0, arg1, arg2, arg3);
    _post_call_callback("glRasterPos4d", (void*)glRasterPos4d, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLRASTERPOS4DPROC glad_debug_glRasterPos4d = glad_debug_impl_glRasterPos4d;
PFNGLRASTERPOS4DVPROC glad_glRasterPos4dv;
void APIENTRY glad_debug_impl_glRasterPos4dv(const GLdouble * arg0) {    
    _pre_call_callback("glRasterPos4dv", (void*)glRasterPos4dv, 1, arg0);
     glad_glRasterPos4dv(arg0);
    _post_call_callback("glRasterPos4dv", (void*)glRasterPos4dv, 1, arg0);
    
}
PFNGLRASTERPOS4DVPROC glad_debug_glRasterPos4dv = glad_debug_impl_glRasterPos4dv;
PFNGLRASTERPOS4FPROC glad_glRasterPos4f;
void APIENTRY glad_debug_impl_glRasterPos4f(GLfloat arg0, GLfloat arg1, GLfloat arg2, GLfloat arg3) {    
    _pre_call_callback("glRasterPos4f", (void*)glRasterPos4f, 4, arg0, arg1, arg2, arg3);
     glad_glRasterPos4f(arg0, arg1, arg2, arg3);
    _post_call_callback("glRasterPos4f", (void*)glRasterPos4f, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLRASTERPOS4FPROC glad_debug_glRasterPos4f = glad_debug_impl_glRasterPos4f;
PFNGLRASTERPOS4FVPROC glad_glRasterPos4fv;
void APIENTRY glad_debug_impl_glRasterPos4fv(const GLfloat * arg0) {    
    _pre_call_callback("glRasterPos4fv", (void*)glRasterPos4fv, 1, arg0);
     glad_glRasterPos4fv(arg0);
    _post_call_callback("glRasterPos4fv", (void*)glRasterPos4fv, 1, arg0);
    
}
PFNGLRASTERPOS4FVPROC glad_debug_glRasterPos4fv = glad_debug_impl_glRasterPos4fv;
PFNGLRASTERPOS4IPROC glad_glRasterPos4i;
void APIENTRY glad_debug_impl_glRasterPos4i(GLint arg0, GLint arg1, GLint arg2, GLint arg3) {    
    _pre_call_callback("glRasterPos4i", (void*)glRasterPos4i, 4, arg0, arg1, arg2, arg3);
     glad_glRasterPos4i(arg0, arg1, arg2, arg3);
    _post_call_callback("glRasterPos4i", (void*)glRasterPos4i, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLRASTERPOS4IPROC glad_debug_glRasterPos4i = glad_debug_impl_glRasterPos4i;
PFNGLRASTERPOS4IVPROC glad_glRasterPos4iv;
void APIENTRY glad_debug_impl_glRasterPos4iv(const GLint * arg0) {    
    _pre_call_callback("glRasterPos4iv", (void*)glRasterPos4iv, 1, arg0);
     glad_glRasterPos4iv(arg0);
    _post_call_callback("glRasterPos4iv", (void*)glRasterPos4iv, 1, arg0);
    
}
PFNGLRASTERPOS4IVPROC glad_debug_glRasterPos4iv = glad_debug_impl_glRasterPos4iv;
PFNGLRASTERPOS4SPROC glad_glRasterPos4s;
void APIENTRY glad_debug_impl_glRasterPos4s(GLshort arg0, GLshort arg1, GLshort arg2, GLshort arg3) {    
    _pre_call_callback("glRasterPos4s", (void*)glRasterPos4s, 4, arg0, arg1, arg2, arg3);
     glad_glRasterPos4s(arg0, arg1, arg2, arg3);
    _post_call_callback("glRasterPos4s", (void*)glRasterPos4s, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLRASTERPOS4SPROC glad_debug_glRasterPos4s = glad_debug_impl_glRasterPos4s;
PFNGLRASTERPOS4SVPROC glad_glRasterPos4sv;
void APIENTRY glad_debug_impl_glRasterPos4sv(const GLshort * arg0) {    
    _pre_call_callback("glRasterPos4sv", (void*)glRasterPos4sv, 1, arg0);
     glad_glRasterPos4sv(arg0);
    _post_call_callback("glRasterPos4sv", (void*)glRasterPos4sv, 1, arg0);
    
}
PFNGLRASTERPOS4SVPROC glad_debug_glRasterPos4sv = glad_debug_impl_glRasterPos4sv;
PFNGLREADBUFFERPROC glad_glReadBuffer;
void APIENTRY glad_debug_impl_glReadBuffer(GLenum arg0) {    
    _pre_call_callback("glReadBuffer", (void*)glReadBuffer, 1, arg0);
     glad_glReadBuffer(arg0);
    _post_call_callback("glReadBuffer", (void*)glReadBuffer, 1, arg0);
    
}
PFNGLREADBUFFERPROC glad_debug_glReadBuffer = glad_debug_impl_glReadBuffer;
PFNGLREADPIXELSPROC glad_glReadPixels;
void APIENTRY glad_debug_impl_glReadPixels(GLint arg0, GLint arg1, GLsizei arg2, GLsizei arg3, GLenum arg4, GLenum arg5, void * arg6) {    
    _pre_call_callback("glReadPixels", (void*)glReadPixels, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
     glad_glReadPixels(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    _post_call_callback("glReadPixels", (void*)glReadPixels, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    
}
PFNGLREADPIXELSPROC glad_debug_glReadPixels = glad_debug_impl_glReadPixels;
PFNGLREADNPIXELSPROC glad_glReadnPixels;
void APIENTRY glad_debug_impl_glReadnPixels(GLint arg0, GLint arg1, GLsizei arg2, GLsizei arg3, GLenum arg4, GLenum arg5, GLsizei arg6, void * arg7) {    
    _pre_call_callback("glReadnPixels", (void*)glReadnPixels, 8, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
     glad_glReadnPixels(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    _post_call_callback("glReadnPixels", (void*)glReadnPixels, 8, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    
}
PFNGLREADNPIXELSPROC glad_debug_glReadnPixels = glad_debug_impl_glReadnPixels;
PFNGLRECTDPROC glad_glRectd;
void APIENTRY glad_debug_impl_glRectd(GLdouble arg0, GLdouble arg1, GLdouble arg2, GLdouble arg3) {    
    _pre_call_callback("glRectd", (void*)glRectd, 4, arg0, arg1, arg2, arg3);
     glad_glRectd(arg0, arg1, arg2, arg3);
    _post_call_callback("glRectd", (void*)glRectd, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLRECTDPROC glad_debug_glRectd = glad_debug_impl_glRectd;
PFNGLRECTDVPROC glad_glRectdv;
void APIENTRY glad_debug_impl_glRectdv(const GLdouble * arg0, const GLdouble * arg1) {    
    _pre_call_callback("glRectdv", (void*)glRectdv, 2, arg0, arg1);
     glad_glRectdv(arg0, arg1);
    _post_call_callback("glRectdv", (void*)glRectdv, 2, arg0, arg1);
    
}
PFNGLRECTDVPROC glad_debug_glRectdv = glad_debug_impl_glRectdv;
PFNGLRECTFPROC glad_glRectf;
void APIENTRY glad_debug_impl_glRectf(GLfloat arg0, GLfloat arg1, GLfloat arg2, GLfloat arg3) {    
    _pre_call_callback("glRectf", (void*)glRectf, 4, arg0, arg1, arg2, arg3);
     glad_glRectf(arg0, arg1, arg2, arg3);
    _post_call_callback("glRectf", (void*)glRectf, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLRECTFPROC glad_debug_glRectf = glad_debug_impl_glRectf;
PFNGLRECTFVPROC glad_glRectfv;
void APIENTRY glad_debug_impl_glRectfv(const GLfloat * arg0, const GLfloat * arg1) {    
    _pre_call_callback("glRectfv", (void*)glRectfv, 2, arg0, arg1);
     glad_glRectfv(arg0, arg1);
    _post_call_callback("glRectfv", (void*)glRectfv, 2, arg0, arg1);
    
}
PFNGLRECTFVPROC glad_debug_glRectfv = glad_debug_impl_glRectfv;
PFNGLRECTIPROC glad_glRecti;
void APIENTRY glad_debug_impl_glRecti(GLint arg0, GLint arg1, GLint arg2, GLint arg3) {    
    _pre_call_callback("glRecti", (void*)glRecti, 4, arg0, arg1, arg2, arg3);
     glad_glRecti(arg0, arg1, arg2, arg3);
    _post_call_callback("glRecti", (void*)glRecti, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLRECTIPROC glad_debug_glRecti = glad_debug_impl_glRecti;
PFNGLRECTIVPROC glad_glRectiv;
void APIENTRY glad_debug_impl_glRectiv(const GLint * arg0, const GLint * arg1) {    
    _pre_call_callback("glRectiv", (void*)glRectiv, 2, arg0, arg1);
     glad_glRectiv(arg0, arg1);
    _post_call_callback("glRectiv", (void*)glRectiv, 2, arg0, arg1);
    
}
PFNGLRECTIVPROC glad_debug_glRectiv = glad_debug_impl_glRectiv;
PFNGLRECTSPROC glad_glRects;
void APIENTRY glad_debug_impl_glRects(GLshort arg0, GLshort arg1, GLshort arg2, GLshort arg3) {    
    _pre_call_callback("glRects", (void*)glRects, 4, arg0, arg1, arg2, arg3);
     glad_glRects(arg0, arg1, arg2, arg3);
    _post_call_callback("glRects", (void*)glRects, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLRECTSPROC glad_debug_glRects = glad_debug_impl_glRects;
PFNGLRECTSVPROC glad_glRectsv;
void APIENTRY glad_debug_impl_glRectsv(const GLshort * arg0, const GLshort * arg1) {    
    _pre_call_callback("glRectsv", (void*)glRectsv, 2, arg0, arg1);
     glad_glRectsv(arg0, arg1);
    _post_call_callback("glRectsv", (void*)glRectsv, 2, arg0, arg1);
    
}
PFNGLRECTSVPROC glad_debug_glRectsv = glad_debug_impl_glRectsv;
PFNGLRELEASESHADERCOMPILERPROC glad_glReleaseShaderCompiler;
void APIENTRY glad_debug_impl_glReleaseShaderCompiler(void) {    
    _pre_call_callback("glReleaseShaderCompiler", (void*)glReleaseShaderCompiler, 0);
     glad_glReleaseShaderCompiler();
    _post_call_callback("glReleaseShaderCompiler", (void*)glReleaseShaderCompiler, 0);
    
}
PFNGLRELEASESHADERCOMPILERPROC glad_debug_glReleaseShaderCompiler = glad_debug_impl_glReleaseShaderCompiler;
PFNGLRENDERMODEPROC glad_glRenderMode;
GLint APIENTRY glad_debug_impl_glRenderMode(GLenum arg0) {    
    GLint ret;
    _pre_call_callback("glRenderMode", (void*)glRenderMode, 1, arg0);
    ret =  glad_glRenderMode(arg0);
    _post_call_callback("glRenderMode", (void*)glRenderMode, 1, arg0);
    return ret;
}
PFNGLRENDERMODEPROC glad_debug_glRenderMode = glad_debug_impl_glRenderMode;
PFNGLRENDERBUFFERSTORAGEPROC glad_glRenderbufferStorage;
void APIENTRY glad_debug_impl_glRenderbufferStorage(GLenum arg0, GLenum arg1, GLsizei arg2, GLsizei arg3) {    
    _pre_call_callback("glRenderbufferStorage", (void*)glRenderbufferStorage, 4, arg0, arg1, arg2, arg3);
     glad_glRenderbufferStorage(arg0, arg1, arg2, arg3);
    _post_call_callback("glRenderbufferStorage", (void*)glRenderbufferStorage, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLRENDERBUFFERSTORAGEPROC glad_debug_glRenderbufferStorage = glad_debug_impl_glRenderbufferStorage;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC glad_glRenderbufferStorageMultisample;
void APIENTRY glad_debug_impl_glRenderbufferStorageMultisample(GLenum arg0, GLsizei arg1, GLenum arg2, GLsizei arg3, GLsizei arg4) {    
    _pre_call_callback("glRenderbufferStorageMultisample", (void*)glRenderbufferStorageMultisample, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glRenderbufferStorageMultisample(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glRenderbufferStorageMultisample", (void*)glRenderbufferStorageMultisample, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC glad_debug_glRenderbufferStorageMultisample = glad_debug_impl_glRenderbufferStorageMultisample;
PFNGLRESUMETRANSFORMFEEDBACKPROC glad_glResumeTransformFeedback;
void APIENTRY glad_debug_impl_glResumeTransformFeedback(void) {    
    _pre_call_callback("glResumeTransformFeedback", (void*)glResumeTransformFeedback, 0);
     glad_glResumeTransformFeedback();
    _post_call_callback("glResumeTransformFeedback", (void*)glResumeTransformFeedback, 0);
    
}
PFNGLRESUMETRANSFORMFEEDBACKPROC glad_debug_glResumeTransformFeedback = glad_debug_impl_glResumeTransformFeedback;
PFNGLROTATEDPROC glad_glRotated;
void APIENTRY glad_debug_impl_glRotated(GLdouble arg0, GLdouble arg1, GLdouble arg2, GLdouble arg3) {    
    _pre_call_callback("glRotated", (void*)glRotated, 4, arg0, arg1, arg2, arg3);
     glad_glRotated(arg0, arg1, arg2, arg3);
    _post_call_callback("glRotated", (void*)glRotated, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLROTATEDPROC glad_debug_glRotated = glad_debug_impl_glRotated;
PFNGLROTATEFPROC glad_glRotatef;
void APIENTRY glad_debug_impl_glRotatef(GLfloat arg0, GLfloat arg1, GLfloat arg2, GLfloat arg3) {    
    _pre_call_callback("glRotatef", (void*)glRotatef, 4, arg0, arg1, arg2, arg3);
     glad_glRotatef(arg0, arg1, arg2, arg3);
    _post_call_callback("glRotatef", (void*)glRotatef, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLROTATEFPROC glad_debug_glRotatef = glad_debug_impl_glRotatef;
PFNGLSAMPLECOVERAGEPROC glad_glSampleCoverage;
void APIENTRY glad_debug_impl_glSampleCoverage(GLfloat arg0, GLboolean arg1) {    
    _pre_call_callback("glSampleCoverage", (void*)glSampleCoverage, 2, arg0, arg1);
     glad_glSampleCoverage(arg0, arg1);
    _post_call_callback("glSampleCoverage", (void*)glSampleCoverage, 2, arg0, arg1);
    
}
PFNGLSAMPLECOVERAGEPROC glad_debug_glSampleCoverage = glad_debug_impl_glSampleCoverage;
PFNGLSAMPLEMASKIPROC glad_glSampleMaski;
void APIENTRY glad_debug_impl_glSampleMaski(GLuint arg0, GLbitfield arg1) {    
    _pre_call_callback("glSampleMaski", (void*)glSampleMaski, 2, arg0, arg1);
     glad_glSampleMaski(arg0, arg1);
    _post_call_callback("glSampleMaski", (void*)glSampleMaski, 2, arg0, arg1);
    
}
PFNGLSAMPLEMASKIPROC glad_debug_glSampleMaski = glad_debug_impl_glSampleMaski;
PFNGLSAMPLERPARAMETERIIVPROC glad_glSamplerParameterIiv;
void APIENTRY glad_debug_impl_glSamplerParameterIiv(GLuint arg0, GLenum arg1, const GLint * arg2) {    
    _pre_call_callback("glSamplerParameterIiv", (void*)glSamplerParameterIiv, 3, arg0, arg1, arg2);
     glad_glSamplerParameterIiv(arg0, arg1, arg2);
    _post_call_callback("glSamplerParameterIiv", (void*)glSamplerParameterIiv, 3, arg0, arg1, arg2);
    
}
PFNGLSAMPLERPARAMETERIIVPROC glad_debug_glSamplerParameterIiv = glad_debug_impl_glSamplerParameterIiv;
PFNGLSAMPLERPARAMETERIUIVPROC glad_glSamplerParameterIuiv;
void APIENTRY glad_debug_impl_glSamplerParameterIuiv(GLuint arg0, GLenum arg1, const GLuint * arg2) {    
    _pre_call_callback("glSamplerParameterIuiv", (void*)glSamplerParameterIuiv, 3, arg0, arg1, arg2);
     glad_glSamplerParameterIuiv(arg0, arg1, arg2);
    _post_call_callback("glSamplerParameterIuiv", (void*)glSamplerParameterIuiv, 3, arg0, arg1, arg2);
    
}
PFNGLSAMPLERPARAMETERIUIVPROC glad_debug_glSamplerParameterIuiv = glad_debug_impl_glSamplerParameterIuiv;
PFNGLSAMPLERPARAMETERFPROC glad_glSamplerParameterf;
void APIENTRY glad_debug_impl_glSamplerParameterf(GLuint arg0, GLenum arg1, GLfloat arg2) {    
    _pre_call_callback("glSamplerParameterf", (void*)glSamplerParameterf, 3, arg0, arg1, arg2);
     glad_glSamplerParameterf(arg0, arg1, arg2);
    _post_call_callback("glSamplerParameterf", (void*)glSamplerParameterf, 3, arg0, arg1, arg2);
    
}
PFNGLSAMPLERPARAMETERFPROC glad_debug_glSamplerParameterf = glad_debug_impl_glSamplerParameterf;
PFNGLSAMPLERPARAMETERFVPROC glad_glSamplerParameterfv;
void APIENTRY glad_debug_impl_glSamplerParameterfv(GLuint arg0, GLenum arg1, const GLfloat * arg2) {    
    _pre_call_callback("glSamplerParameterfv", (void*)glSamplerParameterfv, 3, arg0, arg1, arg2);
     glad_glSamplerParameterfv(arg0, arg1, arg2);
    _post_call_callback("glSamplerParameterfv", (void*)glSamplerParameterfv, 3, arg0, arg1, arg2);
    
}
PFNGLSAMPLERPARAMETERFVPROC glad_debug_glSamplerParameterfv = glad_debug_impl_glSamplerParameterfv;
PFNGLSAMPLERPARAMETERIPROC glad_glSamplerParameteri;
void APIENTRY glad_debug_impl_glSamplerParameteri(GLuint arg0, GLenum arg1, GLint arg2) {    
    _pre_call_callback("glSamplerParameteri", (void*)glSamplerParameteri, 3, arg0, arg1, arg2);
     glad_glSamplerParameteri(arg0, arg1, arg2);
    _post_call_callback("glSamplerParameteri", (void*)glSamplerParameteri, 3, arg0, arg1, arg2);
    
}
PFNGLSAMPLERPARAMETERIPROC glad_debug_glSamplerParameteri = glad_debug_impl_glSamplerParameteri;
PFNGLSAMPLERPARAMETERIVPROC glad_glSamplerParameteriv;
void APIENTRY glad_debug_impl_glSamplerParameteriv(GLuint arg0, GLenum arg1, const GLint * arg2) {    
    _pre_call_callback("glSamplerParameteriv", (void*)glSamplerParameteriv, 3, arg0, arg1, arg2);
     glad_glSamplerParameteriv(arg0, arg1, arg2);
    _post_call_callback("glSamplerParameteriv", (void*)glSamplerParameteriv, 3, arg0, arg1, arg2);
    
}
PFNGLSAMPLERPARAMETERIVPROC glad_debug_glSamplerParameteriv = glad_debug_impl_glSamplerParameteriv;
PFNGLSCALEDPROC glad_glScaled;
void APIENTRY glad_debug_impl_glScaled(GLdouble arg0, GLdouble arg1, GLdouble arg2) {    
    _pre_call_callback("glScaled", (void*)glScaled, 3, arg0, arg1, arg2);
     glad_glScaled(arg0, arg1, arg2);
    _post_call_callback("glScaled", (void*)glScaled, 3, arg0, arg1, arg2);
    
}
PFNGLSCALEDPROC glad_debug_glScaled = glad_debug_impl_glScaled;
PFNGLSCALEFPROC glad_glScalef;
void APIENTRY glad_debug_impl_glScalef(GLfloat arg0, GLfloat arg1, GLfloat arg2) {    
    _pre_call_callback("glScalef", (void*)glScalef, 3, arg0, arg1, arg2);
     glad_glScalef(arg0, arg1, arg2);
    _post_call_callback("glScalef", (void*)glScalef, 3, arg0, arg1, arg2);
    
}
PFNGLSCALEFPROC glad_debug_glScalef = glad_debug_impl_glScalef;
PFNGLSCISSORPROC glad_glScissor;
void APIENTRY glad_debug_impl_glScissor(GLint arg0, GLint arg1, GLsizei arg2, GLsizei arg3) {    
    _pre_call_callback("glScissor", (void*)glScissor, 4, arg0, arg1, arg2, arg3);
     glad_glScissor(arg0, arg1, arg2, arg3);
    _post_call_callback("glScissor", (void*)glScissor, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLSCISSORPROC glad_debug_glScissor = glad_debug_impl_glScissor;
PFNGLSCISSORARRAYVPROC glad_glScissorArrayv;
void APIENTRY glad_debug_impl_glScissorArrayv(GLuint arg0, GLsizei arg1, const GLint * arg2) {    
    _pre_call_callback("glScissorArrayv", (void*)glScissorArrayv, 3, arg0, arg1, arg2);
     glad_glScissorArrayv(arg0, arg1, arg2);
    _post_call_callback("glScissorArrayv", (void*)glScissorArrayv, 3, arg0, arg1, arg2);
    
}
PFNGLSCISSORARRAYVPROC glad_debug_glScissorArrayv = glad_debug_impl_glScissorArrayv;
PFNGLSCISSORINDEXEDPROC glad_glScissorIndexed;
void APIENTRY glad_debug_impl_glScissorIndexed(GLuint arg0, GLint arg1, GLint arg2, GLsizei arg3, GLsizei arg4) {    
    _pre_call_callback("glScissorIndexed", (void*)glScissorIndexed, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glScissorIndexed(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glScissorIndexed", (void*)glScissorIndexed, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLSCISSORINDEXEDPROC glad_debug_glScissorIndexed = glad_debug_impl_glScissorIndexed;
PFNGLSCISSORINDEXEDVPROC glad_glScissorIndexedv;
void APIENTRY glad_debug_impl_glScissorIndexedv(GLuint arg0, const GLint * arg1) {    
    _pre_call_callback("glScissorIndexedv", (void*)glScissorIndexedv, 2, arg0, arg1);
     glad_glScissorIndexedv(arg0, arg1);
    _post_call_callback("glScissorIndexedv", (void*)glScissorIndexedv, 2, arg0, arg1);
    
}
PFNGLSCISSORINDEXEDVPROC glad_debug_glScissorIndexedv = glad_debug_impl_glScissorIndexedv;
PFNGLSECONDARYCOLOR3BPROC glad_glSecondaryColor3b;
void APIENTRY glad_debug_impl_glSecondaryColor3b(GLbyte arg0, GLbyte arg1, GLbyte arg2) {    
    _pre_call_callback("glSecondaryColor3b", (void*)glSecondaryColor3b, 3, arg0, arg1, arg2);
     glad_glSecondaryColor3b(arg0, arg1, arg2);
    _post_call_callback("glSecondaryColor3b", (void*)glSecondaryColor3b, 3, arg0, arg1, arg2);
    
}
PFNGLSECONDARYCOLOR3BPROC glad_debug_glSecondaryColor3b = glad_debug_impl_glSecondaryColor3b;
PFNGLSECONDARYCOLOR3BVPROC glad_glSecondaryColor3bv;
void APIENTRY glad_debug_impl_glSecondaryColor3bv(const GLbyte * arg0) {    
    _pre_call_callback("glSecondaryColor3bv", (void*)glSecondaryColor3bv, 1, arg0);
     glad_glSecondaryColor3bv(arg0);
    _post_call_callback("glSecondaryColor3bv", (void*)glSecondaryColor3bv, 1, arg0);
    
}
PFNGLSECONDARYCOLOR3BVPROC glad_debug_glSecondaryColor3bv = glad_debug_impl_glSecondaryColor3bv;
PFNGLSECONDARYCOLOR3DPROC glad_glSecondaryColor3d;
void APIENTRY glad_debug_impl_glSecondaryColor3d(GLdouble arg0, GLdouble arg1, GLdouble arg2) {    
    _pre_call_callback("glSecondaryColor3d", (void*)glSecondaryColor3d, 3, arg0, arg1, arg2);
     glad_glSecondaryColor3d(arg0, arg1, arg2);
    _post_call_callback("glSecondaryColor3d", (void*)glSecondaryColor3d, 3, arg0, arg1, arg2);
    
}
PFNGLSECONDARYCOLOR3DPROC glad_debug_glSecondaryColor3d = glad_debug_impl_glSecondaryColor3d;
PFNGLSECONDARYCOLOR3DVPROC glad_glSecondaryColor3dv;
void APIENTRY glad_debug_impl_glSecondaryColor3dv(const GLdouble * arg0) {    
    _pre_call_callback("glSecondaryColor3dv", (void*)glSecondaryColor3dv, 1, arg0);
     glad_glSecondaryColor3dv(arg0);
    _post_call_callback("glSecondaryColor3dv", (void*)glSecondaryColor3dv, 1, arg0);
    
}
PFNGLSECONDARYCOLOR3DVPROC glad_debug_glSecondaryColor3dv = glad_debug_impl_glSecondaryColor3dv;
PFNGLSECONDARYCOLOR3FPROC glad_glSecondaryColor3f;
void APIENTRY glad_debug_impl_glSecondaryColor3f(GLfloat arg0, GLfloat arg1, GLfloat arg2) {    
    _pre_call_callback("glSecondaryColor3f", (void*)glSecondaryColor3f, 3, arg0, arg1, arg2);
     glad_glSecondaryColor3f(arg0, arg1, arg2);
    _post_call_callback("glSecondaryColor3f", (void*)glSecondaryColor3f, 3, arg0, arg1, arg2);
    
}
PFNGLSECONDARYCOLOR3FPROC glad_debug_glSecondaryColor3f = glad_debug_impl_glSecondaryColor3f;
PFNGLSECONDARYCOLOR3FVPROC glad_glSecondaryColor3fv;
void APIENTRY glad_debug_impl_glSecondaryColor3fv(const GLfloat * arg0) {    
    _pre_call_callback("glSecondaryColor3fv", (void*)glSecondaryColor3fv, 1, arg0);
     glad_glSecondaryColor3fv(arg0);
    _post_call_callback("glSecondaryColor3fv", (void*)glSecondaryColor3fv, 1, arg0);
    
}
PFNGLSECONDARYCOLOR3FVPROC glad_debug_glSecondaryColor3fv = glad_debug_impl_glSecondaryColor3fv;
PFNGLSECONDARYCOLOR3IPROC glad_glSecondaryColor3i;
void APIENTRY glad_debug_impl_glSecondaryColor3i(GLint arg0, GLint arg1, GLint arg2) {    
    _pre_call_callback("glSecondaryColor3i", (void*)glSecondaryColor3i, 3, arg0, arg1, arg2);
     glad_glSecondaryColor3i(arg0, arg1, arg2);
    _post_call_callback("glSecondaryColor3i", (void*)glSecondaryColor3i, 3, arg0, arg1, arg2);
    
}
PFNGLSECONDARYCOLOR3IPROC glad_debug_glSecondaryColor3i = glad_debug_impl_glSecondaryColor3i;
PFNGLSECONDARYCOLOR3IVPROC glad_glSecondaryColor3iv;
void APIENTRY glad_debug_impl_glSecondaryColor3iv(const GLint * arg0) {    
    _pre_call_callback("glSecondaryColor3iv", (void*)glSecondaryColor3iv, 1, arg0);
     glad_glSecondaryColor3iv(arg0);
    _post_call_callback("glSecondaryColor3iv", (void*)glSecondaryColor3iv, 1, arg0);
    
}
PFNGLSECONDARYCOLOR3IVPROC glad_debug_glSecondaryColor3iv = glad_debug_impl_glSecondaryColor3iv;
PFNGLSECONDARYCOLOR3SPROC glad_glSecondaryColor3s;
void APIENTRY glad_debug_impl_glSecondaryColor3s(GLshort arg0, GLshort arg1, GLshort arg2) {    
    _pre_call_callback("glSecondaryColor3s", (void*)glSecondaryColor3s, 3, arg0, arg1, arg2);
     glad_glSecondaryColor3s(arg0, arg1, arg2);
    _post_call_callback("glSecondaryColor3s", (void*)glSecondaryColor3s, 3, arg0, arg1, arg2);
    
}
PFNGLSECONDARYCOLOR3SPROC glad_debug_glSecondaryColor3s = glad_debug_impl_glSecondaryColor3s;
PFNGLSECONDARYCOLOR3SVPROC glad_glSecondaryColor3sv;
void APIENTRY glad_debug_impl_glSecondaryColor3sv(const GLshort * arg0) {    
    _pre_call_callback("glSecondaryColor3sv", (void*)glSecondaryColor3sv, 1, arg0);
     glad_glSecondaryColor3sv(arg0);
    _post_call_callback("glSecondaryColor3sv", (void*)glSecondaryColor3sv, 1, arg0);
    
}
PFNGLSECONDARYCOLOR3SVPROC glad_debug_glSecondaryColor3sv = glad_debug_impl_glSecondaryColor3sv;
PFNGLSECONDARYCOLOR3UBPROC glad_glSecondaryColor3ub;
void APIENTRY glad_debug_impl_glSecondaryColor3ub(GLubyte arg0, GLubyte arg1, GLubyte arg2) {    
    _pre_call_callback("glSecondaryColor3ub", (void*)glSecondaryColor3ub, 3, arg0, arg1, arg2);
     glad_glSecondaryColor3ub(arg0, arg1, arg2);
    _post_call_callback("glSecondaryColor3ub", (void*)glSecondaryColor3ub, 3, arg0, arg1, arg2);
    
}
PFNGLSECONDARYCOLOR3UBPROC glad_debug_glSecondaryColor3ub = glad_debug_impl_glSecondaryColor3ub;
PFNGLSECONDARYCOLOR3UBVPROC glad_glSecondaryColor3ubv;
void APIENTRY glad_debug_impl_glSecondaryColor3ubv(const GLubyte * arg0) {    
    _pre_call_callback("glSecondaryColor3ubv", (void*)glSecondaryColor3ubv, 1, arg0);
     glad_glSecondaryColor3ubv(arg0);
    _post_call_callback("glSecondaryColor3ubv", (void*)glSecondaryColor3ubv, 1, arg0);
    
}
PFNGLSECONDARYCOLOR3UBVPROC glad_debug_glSecondaryColor3ubv = glad_debug_impl_glSecondaryColor3ubv;
PFNGLSECONDARYCOLOR3UIPROC glad_glSecondaryColor3ui;
void APIENTRY glad_debug_impl_glSecondaryColor3ui(GLuint arg0, GLuint arg1, GLuint arg2) {    
    _pre_call_callback("glSecondaryColor3ui", (void*)glSecondaryColor3ui, 3, arg0, arg1, arg2);
     glad_glSecondaryColor3ui(arg0, arg1, arg2);
    _post_call_callback("glSecondaryColor3ui", (void*)glSecondaryColor3ui, 3, arg0, arg1, arg2);
    
}
PFNGLSECONDARYCOLOR3UIPROC glad_debug_glSecondaryColor3ui = glad_debug_impl_glSecondaryColor3ui;
PFNGLSECONDARYCOLOR3UIVPROC glad_glSecondaryColor3uiv;
void APIENTRY glad_debug_impl_glSecondaryColor3uiv(const GLuint * arg0) {    
    _pre_call_callback("glSecondaryColor3uiv", (void*)glSecondaryColor3uiv, 1, arg0);
     glad_glSecondaryColor3uiv(arg0);
    _post_call_callback("glSecondaryColor3uiv", (void*)glSecondaryColor3uiv, 1, arg0);
    
}
PFNGLSECONDARYCOLOR3UIVPROC glad_debug_glSecondaryColor3uiv = glad_debug_impl_glSecondaryColor3uiv;
PFNGLSECONDARYCOLOR3USPROC glad_glSecondaryColor3us;
void APIENTRY glad_debug_impl_glSecondaryColor3us(GLushort arg0, GLushort arg1, GLushort arg2) {    
    _pre_call_callback("glSecondaryColor3us", (void*)glSecondaryColor3us, 3, arg0, arg1, arg2);
     glad_glSecondaryColor3us(arg0, arg1, arg2);
    _post_call_callback("glSecondaryColor3us", (void*)glSecondaryColor3us, 3, arg0, arg1, arg2);
    
}
PFNGLSECONDARYCOLOR3USPROC glad_debug_glSecondaryColor3us = glad_debug_impl_glSecondaryColor3us;
PFNGLSECONDARYCOLOR3USVPROC glad_glSecondaryColor3usv;
void APIENTRY glad_debug_impl_glSecondaryColor3usv(const GLushort * arg0) {    
    _pre_call_callback("glSecondaryColor3usv", (void*)glSecondaryColor3usv, 1, arg0);
     glad_glSecondaryColor3usv(arg0);
    _post_call_callback("glSecondaryColor3usv", (void*)glSecondaryColor3usv, 1, arg0);
    
}
PFNGLSECONDARYCOLOR3USVPROC glad_debug_glSecondaryColor3usv = glad_debug_impl_glSecondaryColor3usv;
PFNGLSECONDARYCOLORP3UIPROC glad_glSecondaryColorP3ui;
void APIENTRY glad_debug_impl_glSecondaryColorP3ui(GLenum arg0, GLuint arg1) {    
    _pre_call_callback("glSecondaryColorP3ui", (void*)glSecondaryColorP3ui, 2, arg0, arg1);
     glad_glSecondaryColorP3ui(arg0, arg1);
    _post_call_callback("glSecondaryColorP3ui", (void*)glSecondaryColorP3ui, 2, arg0, arg1);
    
}
PFNGLSECONDARYCOLORP3UIPROC glad_debug_glSecondaryColorP3ui = glad_debug_impl_glSecondaryColorP3ui;
PFNGLSECONDARYCOLORP3UIVPROC glad_glSecondaryColorP3uiv;
void APIENTRY glad_debug_impl_glSecondaryColorP3uiv(GLenum arg0, const GLuint * arg1) {    
    _pre_call_callback("glSecondaryColorP3uiv", (void*)glSecondaryColorP3uiv, 2, arg0, arg1);
     glad_glSecondaryColorP3uiv(arg0, arg1);
    _post_call_callback("glSecondaryColorP3uiv", (void*)glSecondaryColorP3uiv, 2, arg0, arg1);
    
}
PFNGLSECONDARYCOLORP3UIVPROC glad_debug_glSecondaryColorP3uiv = glad_debug_impl_glSecondaryColorP3uiv;
PFNGLSECONDARYCOLORPOINTERPROC glad_glSecondaryColorPointer;
void APIENTRY glad_debug_impl_glSecondaryColorPointer(GLint arg0, GLenum arg1, GLsizei arg2, const void * arg3) {    
    _pre_call_callback("glSecondaryColorPointer", (void*)glSecondaryColorPointer, 4, arg0, arg1, arg2, arg3);
     glad_glSecondaryColorPointer(arg0, arg1, arg2, arg3);
    _post_call_callback("glSecondaryColorPointer", (void*)glSecondaryColorPointer, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLSECONDARYCOLORPOINTERPROC glad_debug_glSecondaryColorPointer = glad_debug_impl_glSecondaryColorPointer;
PFNGLSELECTBUFFERPROC glad_glSelectBuffer;
void APIENTRY glad_debug_impl_glSelectBuffer(GLsizei arg0, GLuint * arg1) {    
    _pre_call_callback("glSelectBuffer", (void*)glSelectBuffer, 2, arg0, arg1);
     glad_glSelectBuffer(arg0, arg1);
    _post_call_callback("glSelectBuffer", (void*)glSelectBuffer, 2, arg0, arg1);
    
}
PFNGLSELECTBUFFERPROC glad_debug_glSelectBuffer = glad_debug_impl_glSelectBuffer;
PFNGLSHADEMODELPROC glad_glShadeModel;
void APIENTRY glad_debug_impl_glShadeModel(GLenum arg0) {    
    _pre_call_callback("glShadeModel", (void*)glShadeModel, 1, arg0);
     glad_glShadeModel(arg0);
    _post_call_callback("glShadeModel", (void*)glShadeModel, 1, arg0);
    
}
PFNGLSHADEMODELPROC glad_debug_glShadeModel = glad_debug_impl_glShadeModel;
PFNGLSHADERBINARYPROC glad_glShaderBinary;
void APIENTRY glad_debug_impl_glShaderBinary(GLsizei arg0, const GLuint * arg1, GLenum arg2, const void * arg3, GLsizei arg4) {    
    _pre_call_callback("glShaderBinary", (void*)glShaderBinary, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glShaderBinary(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glShaderBinary", (void*)glShaderBinary, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLSHADERBINARYPROC glad_debug_glShaderBinary = glad_debug_impl_glShaderBinary;
PFNGLSHADERSOURCEPROC glad_glShaderSource;
void APIENTRY glad_debug_impl_glShaderSource(GLuint arg0, GLsizei arg1, const GLchar *const* arg2, const GLint * arg3) {    
    _pre_call_callback("glShaderSource", (void*)glShaderSource, 4, arg0, arg1, arg2, arg3);
     glad_glShaderSource(arg0, arg1, arg2, arg3);
    _post_call_callback("glShaderSource", (void*)glShaderSource, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLSHADERSOURCEPROC glad_debug_glShaderSource = glad_debug_impl_glShaderSource;
PFNGLSHADERSTORAGEBLOCKBINDINGPROC glad_glShaderStorageBlockBinding;
void APIENTRY glad_debug_impl_glShaderStorageBlockBinding(GLuint arg0, GLuint arg1, GLuint arg2) {    
    _pre_call_callback("glShaderStorageBlockBinding", (void*)glShaderStorageBlockBinding, 3, arg0, arg1, arg2);
     glad_glShaderStorageBlockBinding(arg0, arg1, arg2);
    _post_call_callback("glShaderStorageBlockBinding", (void*)glShaderStorageBlockBinding, 3, arg0, arg1, arg2);
    
}
PFNGLSHADERSTORAGEBLOCKBINDINGPROC glad_debug_glShaderStorageBlockBinding = glad_debug_impl_glShaderStorageBlockBinding;
PFNGLSTENCILFUNCPROC glad_glStencilFunc;
void APIENTRY glad_debug_impl_glStencilFunc(GLenum arg0, GLint arg1, GLuint arg2) {    
    _pre_call_callback("glStencilFunc", (void*)glStencilFunc, 3, arg0, arg1, arg2);
     glad_glStencilFunc(arg0, arg1, arg2);
    _post_call_callback("glStencilFunc", (void*)glStencilFunc, 3, arg0, arg1, arg2);
    
}
PFNGLSTENCILFUNCPROC glad_debug_glStencilFunc = glad_debug_impl_glStencilFunc;
PFNGLSTENCILFUNCSEPARATEPROC glad_glStencilFuncSeparate;
void APIENTRY glad_debug_impl_glStencilFuncSeparate(GLenum arg0, GLenum arg1, GLint arg2, GLuint arg3) {    
    _pre_call_callback("glStencilFuncSeparate", (void*)glStencilFuncSeparate, 4, arg0, arg1, arg2, arg3);
     glad_glStencilFuncSeparate(arg0, arg1, arg2, arg3);
    _post_call_callback("glStencilFuncSeparate", (void*)glStencilFuncSeparate, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLSTENCILFUNCSEPARATEPROC glad_debug_glStencilFuncSeparate = glad_debug_impl_glStencilFuncSeparate;
PFNGLSTENCILMASKPROC glad_glStencilMask;
void APIENTRY glad_debug_impl_glStencilMask(GLuint arg0) {    
    _pre_call_callback("glStencilMask", (void*)glStencilMask, 1, arg0);
     glad_glStencilMask(arg0);
    _post_call_callback("glStencilMask", (void*)glStencilMask, 1, arg0);
    
}
PFNGLSTENCILMASKPROC glad_debug_glStencilMask = glad_debug_impl_glStencilMask;
PFNGLSTENCILMASKSEPARATEPROC glad_glStencilMaskSeparate;
void APIENTRY glad_debug_impl_glStencilMaskSeparate(GLenum arg0, GLuint arg1) {    
    _pre_call_callback("glStencilMaskSeparate", (void*)glStencilMaskSeparate, 2, arg0, arg1);
     glad_glStencilMaskSeparate(arg0, arg1);
    _post_call_callback("glStencilMaskSeparate", (void*)glStencilMaskSeparate, 2, arg0, arg1);
    
}
PFNGLSTENCILMASKSEPARATEPROC glad_debug_glStencilMaskSeparate = glad_debug_impl_glStencilMaskSeparate;
PFNGLSTENCILOPPROC glad_glStencilOp;
void APIENTRY glad_debug_impl_glStencilOp(GLenum arg0, GLenum arg1, GLenum arg2) {    
    _pre_call_callback("glStencilOp", (void*)glStencilOp, 3, arg0, arg1, arg2);
     glad_glStencilOp(arg0, arg1, arg2);
    _post_call_callback("glStencilOp", (void*)glStencilOp, 3, arg0, arg1, arg2);
    
}
PFNGLSTENCILOPPROC glad_debug_glStencilOp = glad_debug_impl_glStencilOp;
PFNGLSTENCILOPSEPARATEPROC glad_glStencilOpSeparate;
void APIENTRY glad_debug_impl_glStencilOpSeparate(GLenum arg0, GLenum arg1, GLenum arg2, GLenum arg3) {    
    _pre_call_callback("glStencilOpSeparate", (void*)glStencilOpSeparate, 4, arg0, arg1, arg2, arg3);
     glad_glStencilOpSeparate(arg0, arg1, arg2, arg3);
    _post_call_callback("glStencilOpSeparate", (void*)glStencilOpSeparate, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLSTENCILOPSEPARATEPROC glad_debug_glStencilOpSeparate = glad_debug_impl_glStencilOpSeparate;
PFNGLTEXBUFFERPROC glad_glTexBuffer;
void APIENTRY glad_debug_impl_glTexBuffer(GLenum arg0, GLenum arg1, GLuint arg2) {    
    _pre_call_callback("glTexBuffer", (void*)glTexBuffer, 3, arg0, arg1, arg2);
     glad_glTexBuffer(arg0, arg1, arg2);
    _post_call_callback("glTexBuffer", (void*)glTexBuffer, 3, arg0, arg1, arg2);
    
}
PFNGLTEXBUFFERPROC glad_debug_glTexBuffer = glad_debug_impl_glTexBuffer;
PFNGLTEXBUFFERRANGEPROC glad_glTexBufferRange;
void APIENTRY glad_debug_impl_glTexBufferRange(GLenum arg0, GLenum arg1, GLuint arg2, GLintptr arg3, GLsizeiptr arg4) {    
    _pre_call_callback("glTexBufferRange", (void*)glTexBufferRange, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glTexBufferRange(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glTexBufferRange", (void*)glTexBufferRange, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLTEXBUFFERRANGEPROC glad_debug_glTexBufferRange = glad_debug_impl_glTexBufferRange;
PFNGLTEXCOORD1DPROC glad_glTexCoord1d;
void APIENTRY glad_debug_impl_glTexCoord1d(GLdouble arg0) {    
    _pre_call_callback("glTexCoord1d", (void*)glTexCoord1d, 1, arg0);
     glad_glTexCoord1d(arg0);
    _post_call_callback("glTexCoord1d", (void*)glTexCoord1d, 1, arg0);
    
}
PFNGLTEXCOORD1DPROC glad_debug_glTexCoord1d = glad_debug_impl_glTexCoord1d;
PFNGLTEXCOORD1DVPROC glad_glTexCoord1dv;
void APIENTRY glad_debug_impl_glTexCoord1dv(const GLdouble * arg0) {    
    _pre_call_callback("glTexCoord1dv", (void*)glTexCoord1dv, 1, arg0);
     glad_glTexCoord1dv(arg0);
    _post_call_callback("glTexCoord1dv", (void*)glTexCoord1dv, 1, arg0);
    
}
PFNGLTEXCOORD1DVPROC glad_debug_glTexCoord1dv = glad_debug_impl_glTexCoord1dv;
PFNGLTEXCOORD1FPROC glad_glTexCoord1f;
void APIENTRY glad_debug_impl_glTexCoord1f(GLfloat arg0) {    
    _pre_call_callback("glTexCoord1f", (void*)glTexCoord1f, 1, arg0);
     glad_glTexCoord1f(arg0);
    _post_call_callback("glTexCoord1f", (void*)glTexCoord1f, 1, arg0);
    
}
PFNGLTEXCOORD1FPROC glad_debug_glTexCoord1f = glad_debug_impl_glTexCoord1f;
PFNGLTEXCOORD1FVPROC glad_glTexCoord1fv;
void APIENTRY glad_debug_impl_glTexCoord1fv(const GLfloat * arg0) {    
    _pre_call_callback("glTexCoord1fv", (void*)glTexCoord1fv, 1, arg0);
     glad_glTexCoord1fv(arg0);
    _post_call_callback("glTexCoord1fv", (void*)glTexCoord1fv, 1, arg0);
    
}
PFNGLTEXCOORD1FVPROC glad_debug_glTexCoord1fv = glad_debug_impl_glTexCoord1fv;
PFNGLTEXCOORD1IPROC glad_glTexCoord1i;
void APIENTRY glad_debug_impl_glTexCoord1i(GLint arg0) {    
    _pre_call_callback("glTexCoord1i", (void*)glTexCoord1i, 1, arg0);
     glad_glTexCoord1i(arg0);
    _post_call_callback("glTexCoord1i", (void*)glTexCoord1i, 1, arg0);
    
}
PFNGLTEXCOORD1IPROC glad_debug_glTexCoord1i = glad_debug_impl_glTexCoord1i;
PFNGLTEXCOORD1IVPROC glad_glTexCoord1iv;
void APIENTRY glad_debug_impl_glTexCoord1iv(const GLint * arg0) {    
    _pre_call_callback("glTexCoord1iv", (void*)glTexCoord1iv, 1, arg0);
     glad_glTexCoord1iv(arg0);
    _post_call_callback("glTexCoord1iv", (void*)glTexCoord1iv, 1, arg0);
    
}
PFNGLTEXCOORD1IVPROC glad_debug_glTexCoord1iv = glad_debug_impl_glTexCoord1iv;
PFNGLTEXCOORD1SPROC glad_glTexCoord1s;
void APIENTRY glad_debug_impl_glTexCoord1s(GLshort arg0) {    
    _pre_call_callback("glTexCoord1s", (void*)glTexCoord1s, 1, arg0);
     glad_glTexCoord1s(arg0);
    _post_call_callback("glTexCoord1s", (void*)glTexCoord1s, 1, arg0);
    
}
PFNGLTEXCOORD1SPROC glad_debug_glTexCoord1s = glad_debug_impl_glTexCoord1s;
PFNGLTEXCOORD1SVPROC glad_glTexCoord1sv;
void APIENTRY glad_debug_impl_glTexCoord1sv(const GLshort * arg0) {    
    _pre_call_callback("glTexCoord1sv", (void*)glTexCoord1sv, 1, arg0);
     glad_glTexCoord1sv(arg0);
    _post_call_callback("glTexCoord1sv", (void*)glTexCoord1sv, 1, arg0);
    
}
PFNGLTEXCOORD1SVPROC glad_debug_glTexCoord1sv = glad_debug_impl_glTexCoord1sv;
PFNGLTEXCOORD2DPROC glad_glTexCoord2d;
void APIENTRY glad_debug_impl_glTexCoord2d(GLdouble arg0, GLdouble arg1) {    
    _pre_call_callback("glTexCoord2d", (void*)glTexCoord2d, 2, arg0, arg1);
     glad_glTexCoord2d(arg0, arg1);
    _post_call_callback("glTexCoord2d", (void*)glTexCoord2d, 2, arg0, arg1);
    
}
PFNGLTEXCOORD2DPROC glad_debug_glTexCoord2d = glad_debug_impl_glTexCoord2d;
PFNGLTEXCOORD2DVPROC glad_glTexCoord2dv;
void APIENTRY glad_debug_impl_glTexCoord2dv(const GLdouble * arg0) {    
    _pre_call_callback("glTexCoord2dv", (void*)glTexCoord2dv, 1, arg0);
     glad_glTexCoord2dv(arg0);
    _post_call_callback("glTexCoord2dv", (void*)glTexCoord2dv, 1, arg0);
    
}
PFNGLTEXCOORD2DVPROC glad_debug_glTexCoord2dv = glad_debug_impl_glTexCoord2dv;
PFNGLTEXCOORD2FPROC glad_glTexCoord2f;
void APIENTRY glad_debug_impl_glTexCoord2f(GLfloat arg0, GLfloat arg1) {    
    _pre_call_callback("glTexCoord2f", (void*)glTexCoord2f, 2, arg0, arg1);
     glad_glTexCoord2f(arg0, arg1);
    _post_call_callback("glTexCoord2f", (void*)glTexCoord2f, 2, arg0, arg1);
    
}
PFNGLTEXCOORD2FPROC glad_debug_glTexCoord2f = glad_debug_impl_glTexCoord2f;
PFNGLTEXCOORD2FVPROC glad_glTexCoord2fv;
void APIENTRY glad_debug_impl_glTexCoord2fv(const GLfloat * arg0) {    
    _pre_call_callback("glTexCoord2fv", (void*)glTexCoord2fv, 1, arg0);
     glad_glTexCoord2fv(arg0);
    _post_call_callback("glTexCoord2fv", (void*)glTexCoord2fv, 1, arg0);
    
}
PFNGLTEXCOORD2FVPROC glad_debug_glTexCoord2fv = glad_debug_impl_glTexCoord2fv;
PFNGLTEXCOORD2IPROC glad_glTexCoord2i;
void APIENTRY glad_debug_impl_glTexCoord2i(GLint arg0, GLint arg1) {    
    _pre_call_callback("glTexCoord2i", (void*)glTexCoord2i, 2, arg0, arg1);
     glad_glTexCoord2i(arg0, arg1);
    _post_call_callback("glTexCoord2i", (void*)glTexCoord2i, 2, arg0, arg1);
    
}
PFNGLTEXCOORD2IPROC glad_debug_glTexCoord2i = glad_debug_impl_glTexCoord2i;
PFNGLTEXCOORD2IVPROC glad_glTexCoord2iv;
void APIENTRY glad_debug_impl_glTexCoord2iv(const GLint * arg0) {    
    _pre_call_callback("glTexCoord2iv", (void*)glTexCoord2iv, 1, arg0);
     glad_glTexCoord2iv(arg0);
    _post_call_callback("glTexCoord2iv", (void*)glTexCoord2iv, 1, arg0);
    
}
PFNGLTEXCOORD2IVPROC glad_debug_glTexCoord2iv = glad_debug_impl_glTexCoord2iv;
PFNGLTEXCOORD2SPROC glad_glTexCoord2s;
void APIENTRY glad_debug_impl_glTexCoord2s(GLshort arg0, GLshort arg1) {    
    _pre_call_callback("glTexCoord2s", (void*)glTexCoord2s, 2, arg0, arg1);
     glad_glTexCoord2s(arg0, arg1);
    _post_call_callback("glTexCoord2s", (void*)glTexCoord2s, 2, arg0, arg1);
    
}
PFNGLTEXCOORD2SPROC glad_debug_glTexCoord2s = glad_debug_impl_glTexCoord2s;
PFNGLTEXCOORD2SVPROC glad_glTexCoord2sv;
void APIENTRY glad_debug_impl_glTexCoord2sv(const GLshort * arg0) {    
    _pre_call_callback("glTexCoord2sv", (void*)glTexCoord2sv, 1, arg0);
     glad_glTexCoord2sv(arg0);
    _post_call_callback("glTexCoord2sv", (void*)glTexCoord2sv, 1, arg0);
    
}
PFNGLTEXCOORD2SVPROC glad_debug_glTexCoord2sv = glad_debug_impl_glTexCoord2sv;
PFNGLTEXCOORD3DPROC glad_glTexCoord3d;
void APIENTRY glad_debug_impl_glTexCoord3d(GLdouble arg0, GLdouble arg1, GLdouble arg2) {    
    _pre_call_callback("glTexCoord3d", (void*)glTexCoord3d, 3, arg0, arg1, arg2);
     glad_glTexCoord3d(arg0, arg1, arg2);
    _post_call_callback("glTexCoord3d", (void*)glTexCoord3d, 3, arg0, arg1, arg2);
    
}
PFNGLTEXCOORD3DPROC glad_debug_glTexCoord3d = glad_debug_impl_glTexCoord3d;
PFNGLTEXCOORD3DVPROC glad_glTexCoord3dv;
void APIENTRY glad_debug_impl_glTexCoord3dv(const GLdouble * arg0) {    
    _pre_call_callback("glTexCoord3dv", (void*)glTexCoord3dv, 1, arg0);
     glad_glTexCoord3dv(arg0);
    _post_call_callback("glTexCoord3dv", (void*)glTexCoord3dv, 1, arg0);
    
}
PFNGLTEXCOORD3DVPROC glad_debug_glTexCoord3dv = glad_debug_impl_glTexCoord3dv;
PFNGLTEXCOORD3FPROC glad_glTexCoord3f;
void APIENTRY glad_debug_impl_glTexCoord3f(GLfloat arg0, GLfloat arg1, GLfloat arg2) {    
    _pre_call_callback("glTexCoord3f", (void*)glTexCoord3f, 3, arg0, arg1, arg2);
     glad_glTexCoord3f(arg0, arg1, arg2);
    _post_call_callback("glTexCoord3f", (void*)glTexCoord3f, 3, arg0, arg1, arg2);
    
}
PFNGLTEXCOORD3FPROC glad_debug_glTexCoord3f = glad_debug_impl_glTexCoord3f;
PFNGLTEXCOORD3FVPROC glad_glTexCoord3fv;
void APIENTRY glad_debug_impl_glTexCoord3fv(const GLfloat * arg0) {    
    _pre_call_callback("glTexCoord3fv", (void*)glTexCoord3fv, 1, arg0);
     glad_glTexCoord3fv(arg0);
    _post_call_callback("glTexCoord3fv", (void*)glTexCoord3fv, 1, arg0);
    
}
PFNGLTEXCOORD3FVPROC glad_debug_glTexCoord3fv = glad_debug_impl_glTexCoord3fv;
PFNGLTEXCOORD3IPROC glad_glTexCoord3i;
void APIENTRY glad_debug_impl_glTexCoord3i(GLint arg0, GLint arg1, GLint arg2) {    
    _pre_call_callback("glTexCoord3i", (void*)glTexCoord3i, 3, arg0, arg1, arg2);
     glad_glTexCoord3i(arg0, arg1, arg2);
    _post_call_callback("glTexCoord3i", (void*)glTexCoord3i, 3, arg0, arg1, arg2);
    
}
PFNGLTEXCOORD3IPROC glad_debug_glTexCoord3i = glad_debug_impl_glTexCoord3i;
PFNGLTEXCOORD3IVPROC glad_glTexCoord3iv;
void APIENTRY glad_debug_impl_glTexCoord3iv(const GLint * arg0) {    
    _pre_call_callback("glTexCoord3iv", (void*)glTexCoord3iv, 1, arg0);
     glad_glTexCoord3iv(arg0);
    _post_call_callback("glTexCoord3iv", (void*)glTexCoord3iv, 1, arg0);
    
}
PFNGLTEXCOORD3IVPROC glad_debug_glTexCoord3iv = glad_debug_impl_glTexCoord3iv;
PFNGLTEXCOORD3SPROC glad_glTexCoord3s;
void APIENTRY glad_debug_impl_glTexCoord3s(GLshort arg0, GLshort arg1, GLshort arg2) {    
    _pre_call_callback("glTexCoord3s", (void*)glTexCoord3s, 3, arg0, arg1, arg2);
     glad_glTexCoord3s(arg0, arg1, arg2);
    _post_call_callback("glTexCoord3s", (void*)glTexCoord3s, 3, arg0, arg1, arg2);
    
}
PFNGLTEXCOORD3SPROC glad_debug_glTexCoord3s = glad_debug_impl_glTexCoord3s;
PFNGLTEXCOORD3SVPROC glad_glTexCoord3sv;
void APIENTRY glad_debug_impl_glTexCoord3sv(const GLshort * arg0) {    
    _pre_call_callback("glTexCoord3sv", (void*)glTexCoord3sv, 1, arg0);
     glad_glTexCoord3sv(arg0);
    _post_call_callback("glTexCoord3sv", (void*)glTexCoord3sv, 1, arg0);
    
}
PFNGLTEXCOORD3SVPROC glad_debug_glTexCoord3sv = glad_debug_impl_glTexCoord3sv;
PFNGLTEXCOORD4DPROC glad_glTexCoord4d;
void APIENTRY glad_debug_impl_glTexCoord4d(GLdouble arg0, GLdouble arg1, GLdouble arg2, GLdouble arg3) {    
    _pre_call_callback("glTexCoord4d", (void*)glTexCoord4d, 4, arg0, arg1, arg2, arg3);
     glad_glTexCoord4d(arg0, arg1, arg2, arg3);
    _post_call_callback("glTexCoord4d", (void*)glTexCoord4d, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLTEXCOORD4DPROC glad_debug_glTexCoord4d = glad_debug_impl_glTexCoord4d;
PFNGLTEXCOORD4DVPROC glad_glTexCoord4dv;
void APIENTRY glad_debug_impl_glTexCoord4dv(const GLdouble * arg0) {    
    _pre_call_callback("glTexCoord4dv", (void*)glTexCoord4dv, 1, arg0);
     glad_glTexCoord4dv(arg0);
    _post_call_callback("glTexCoord4dv", (void*)glTexCoord4dv, 1, arg0);
    
}
PFNGLTEXCOORD4DVPROC glad_debug_glTexCoord4dv = glad_debug_impl_glTexCoord4dv;
PFNGLTEXCOORD4FPROC glad_glTexCoord4f;
void APIENTRY glad_debug_impl_glTexCoord4f(GLfloat arg0, GLfloat arg1, GLfloat arg2, GLfloat arg3) {    
    _pre_call_callback("glTexCoord4f", (void*)glTexCoord4f, 4, arg0, arg1, arg2, arg3);
     glad_glTexCoord4f(arg0, arg1, arg2, arg3);
    _post_call_callback("glTexCoord4f", (void*)glTexCoord4f, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLTEXCOORD4FPROC glad_debug_glTexCoord4f = glad_debug_impl_glTexCoord4f;
PFNGLTEXCOORD4FVPROC glad_glTexCoord4fv;
void APIENTRY glad_debug_impl_glTexCoord4fv(const GLfloat * arg0) {    
    _pre_call_callback("glTexCoord4fv", (void*)glTexCoord4fv, 1, arg0);
     glad_glTexCoord4fv(arg0);
    _post_call_callback("glTexCoord4fv", (void*)glTexCoord4fv, 1, arg0);
    
}
PFNGLTEXCOORD4FVPROC glad_debug_glTexCoord4fv = glad_debug_impl_glTexCoord4fv;
PFNGLTEXCOORD4IPROC glad_glTexCoord4i;
void APIENTRY glad_debug_impl_glTexCoord4i(GLint arg0, GLint arg1, GLint arg2, GLint arg3) {    
    _pre_call_callback("glTexCoord4i", (void*)glTexCoord4i, 4, arg0, arg1, arg2, arg3);
     glad_glTexCoord4i(arg0, arg1, arg2, arg3);
    _post_call_callback("glTexCoord4i", (void*)glTexCoord4i, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLTEXCOORD4IPROC glad_debug_glTexCoord4i = glad_debug_impl_glTexCoord4i;
PFNGLTEXCOORD4IVPROC glad_glTexCoord4iv;
void APIENTRY glad_debug_impl_glTexCoord4iv(const GLint * arg0) {    
    _pre_call_callback("glTexCoord4iv", (void*)glTexCoord4iv, 1, arg0);
     glad_glTexCoord4iv(arg0);
    _post_call_callback("glTexCoord4iv", (void*)glTexCoord4iv, 1, arg0);
    
}
PFNGLTEXCOORD4IVPROC glad_debug_glTexCoord4iv = glad_debug_impl_glTexCoord4iv;
PFNGLTEXCOORD4SPROC glad_glTexCoord4s;
void APIENTRY glad_debug_impl_glTexCoord4s(GLshort arg0, GLshort arg1, GLshort arg2, GLshort arg3) {    
    _pre_call_callback("glTexCoord4s", (void*)glTexCoord4s, 4, arg0, arg1, arg2, arg3);
     glad_glTexCoord4s(arg0, arg1, arg2, arg3);
    _post_call_callback("glTexCoord4s", (void*)glTexCoord4s, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLTEXCOORD4SPROC glad_debug_glTexCoord4s = glad_debug_impl_glTexCoord4s;
PFNGLTEXCOORD4SVPROC glad_glTexCoord4sv;
void APIENTRY glad_debug_impl_glTexCoord4sv(const GLshort * arg0) {    
    _pre_call_callback("glTexCoord4sv", (void*)glTexCoord4sv, 1, arg0);
     glad_glTexCoord4sv(arg0);
    _post_call_callback("glTexCoord4sv", (void*)glTexCoord4sv, 1, arg0);
    
}
PFNGLTEXCOORD4SVPROC glad_debug_glTexCoord4sv = glad_debug_impl_glTexCoord4sv;
PFNGLTEXCOORDP1UIPROC glad_glTexCoordP1ui;
void APIENTRY glad_debug_impl_glTexCoordP1ui(GLenum arg0, GLuint arg1) {    
    _pre_call_callback("glTexCoordP1ui", (void*)glTexCoordP1ui, 2, arg0, arg1);
     glad_glTexCoordP1ui(arg0, arg1);
    _post_call_callback("glTexCoordP1ui", (void*)glTexCoordP1ui, 2, arg0, arg1);
    
}
PFNGLTEXCOORDP1UIPROC glad_debug_glTexCoordP1ui = glad_debug_impl_glTexCoordP1ui;
PFNGLTEXCOORDP1UIVPROC glad_glTexCoordP1uiv;
void APIENTRY glad_debug_impl_glTexCoordP1uiv(GLenum arg0, const GLuint * arg1) {    
    _pre_call_callback("glTexCoordP1uiv", (void*)glTexCoordP1uiv, 2, arg0, arg1);
     glad_glTexCoordP1uiv(arg0, arg1);
    _post_call_callback("glTexCoordP1uiv", (void*)glTexCoordP1uiv, 2, arg0, arg1);
    
}
PFNGLTEXCOORDP1UIVPROC glad_debug_glTexCoordP1uiv = glad_debug_impl_glTexCoordP1uiv;
PFNGLTEXCOORDP2UIPROC glad_glTexCoordP2ui;
void APIENTRY glad_debug_impl_glTexCoordP2ui(GLenum arg0, GLuint arg1) {    
    _pre_call_callback("glTexCoordP2ui", (void*)glTexCoordP2ui, 2, arg0, arg1);
     glad_glTexCoordP2ui(arg0, arg1);
    _post_call_callback("glTexCoordP2ui", (void*)glTexCoordP2ui, 2, arg0, arg1);
    
}
PFNGLTEXCOORDP2UIPROC glad_debug_glTexCoordP2ui = glad_debug_impl_glTexCoordP2ui;
PFNGLTEXCOORDP2UIVPROC glad_glTexCoordP2uiv;
void APIENTRY glad_debug_impl_glTexCoordP2uiv(GLenum arg0, const GLuint * arg1) {    
    _pre_call_callback("glTexCoordP2uiv", (void*)glTexCoordP2uiv, 2, arg0, arg1);
     glad_glTexCoordP2uiv(arg0, arg1);
    _post_call_callback("glTexCoordP2uiv", (void*)glTexCoordP2uiv, 2, arg0, arg1);
    
}
PFNGLTEXCOORDP2UIVPROC glad_debug_glTexCoordP2uiv = glad_debug_impl_glTexCoordP2uiv;
PFNGLTEXCOORDP3UIPROC glad_glTexCoordP3ui;
void APIENTRY glad_debug_impl_glTexCoordP3ui(GLenum arg0, GLuint arg1) {    
    _pre_call_callback("glTexCoordP3ui", (void*)glTexCoordP3ui, 2, arg0, arg1);
     glad_glTexCoordP3ui(arg0, arg1);
    _post_call_callback("glTexCoordP3ui", (void*)glTexCoordP3ui, 2, arg0, arg1);
    
}
PFNGLTEXCOORDP3UIPROC glad_debug_glTexCoordP3ui = glad_debug_impl_glTexCoordP3ui;
PFNGLTEXCOORDP3UIVPROC glad_glTexCoordP3uiv;
void APIENTRY glad_debug_impl_glTexCoordP3uiv(GLenum arg0, const GLuint * arg1) {    
    _pre_call_callback("glTexCoordP3uiv", (void*)glTexCoordP3uiv, 2, arg0, arg1);
     glad_glTexCoordP3uiv(arg0, arg1);
    _post_call_callback("glTexCoordP3uiv", (void*)glTexCoordP3uiv, 2, arg0, arg1);
    
}
PFNGLTEXCOORDP3UIVPROC glad_debug_glTexCoordP3uiv = glad_debug_impl_glTexCoordP3uiv;
PFNGLTEXCOORDP4UIPROC glad_glTexCoordP4ui;
void APIENTRY glad_debug_impl_glTexCoordP4ui(GLenum arg0, GLuint arg1) {    
    _pre_call_callback("glTexCoordP4ui", (void*)glTexCoordP4ui, 2, arg0, arg1);
     glad_glTexCoordP4ui(arg0, arg1);
    _post_call_callback("glTexCoordP4ui", (void*)glTexCoordP4ui, 2, arg0, arg1);
    
}
PFNGLTEXCOORDP4UIPROC glad_debug_glTexCoordP4ui = glad_debug_impl_glTexCoordP4ui;
PFNGLTEXCOORDP4UIVPROC glad_glTexCoordP4uiv;
void APIENTRY glad_debug_impl_glTexCoordP4uiv(GLenum arg0, const GLuint * arg1) {    
    _pre_call_callback("glTexCoordP4uiv", (void*)glTexCoordP4uiv, 2, arg0, arg1);
     glad_glTexCoordP4uiv(arg0, arg1);
    _post_call_callback("glTexCoordP4uiv", (void*)glTexCoordP4uiv, 2, arg0, arg1);
    
}
PFNGLTEXCOORDP4UIVPROC glad_debug_glTexCoordP4uiv = glad_debug_impl_glTexCoordP4uiv;
PFNGLTEXCOORDPOINTERPROC glad_glTexCoordPointer;
void APIENTRY glad_debug_impl_glTexCoordPointer(GLint arg0, GLenum arg1, GLsizei arg2, const void * arg3) {    
    _pre_call_callback("glTexCoordPointer", (void*)glTexCoordPointer, 4, arg0, arg1, arg2, arg3);
     glad_glTexCoordPointer(arg0, arg1, arg2, arg3);
    _post_call_callback("glTexCoordPointer", (void*)glTexCoordPointer, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLTEXCOORDPOINTERPROC glad_debug_glTexCoordPointer = glad_debug_impl_glTexCoordPointer;
PFNGLTEXENVFPROC glad_glTexEnvf;
void APIENTRY glad_debug_impl_glTexEnvf(GLenum arg0, GLenum arg1, GLfloat arg2) {    
    _pre_call_callback("glTexEnvf", (void*)glTexEnvf, 3, arg0, arg1, arg2);
     glad_glTexEnvf(arg0, arg1, arg2);
    _post_call_callback("glTexEnvf", (void*)glTexEnvf, 3, arg0, arg1, arg2);
    
}
PFNGLTEXENVFPROC glad_debug_glTexEnvf = glad_debug_impl_glTexEnvf;
PFNGLTEXENVFVPROC glad_glTexEnvfv;
void APIENTRY glad_debug_impl_glTexEnvfv(GLenum arg0, GLenum arg1, const GLfloat * arg2) {    
    _pre_call_callback("glTexEnvfv", (void*)glTexEnvfv, 3, arg0, arg1, arg2);
     glad_glTexEnvfv(arg0, arg1, arg2);
    _post_call_callback("glTexEnvfv", (void*)glTexEnvfv, 3, arg0, arg1, arg2);
    
}
PFNGLTEXENVFVPROC glad_debug_glTexEnvfv = glad_debug_impl_glTexEnvfv;
PFNGLTEXENVIPROC glad_glTexEnvi;
void APIENTRY glad_debug_impl_glTexEnvi(GLenum arg0, GLenum arg1, GLint arg2) {    
    _pre_call_callback("glTexEnvi", (void*)glTexEnvi, 3, arg0, arg1, arg2);
     glad_glTexEnvi(arg0, arg1, arg2);
    _post_call_callback("glTexEnvi", (void*)glTexEnvi, 3, arg0, arg1, arg2);
    
}
PFNGLTEXENVIPROC glad_debug_glTexEnvi = glad_debug_impl_glTexEnvi;
PFNGLTEXENVIVPROC glad_glTexEnviv;
void APIENTRY glad_debug_impl_glTexEnviv(GLenum arg0, GLenum arg1, const GLint * arg2) {    
    _pre_call_callback("glTexEnviv", (void*)glTexEnviv, 3, arg0, arg1, arg2);
     glad_glTexEnviv(arg0, arg1, arg2);
    _post_call_callback("glTexEnviv", (void*)glTexEnviv, 3, arg0, arg1, arg2);
    
}
PFNGLTEXENVIVPROC glad_debug_glTexEnviv = glad_debug_impl_glTexEnviv;
PFNGLTEXGENDPROC glad_glTexGend;
void APIENTRY glad_debug_impl_glTexGend(GLenum arg0, GLenum arg1, GLdouble arg2) {    
    _pre_call_callback("glTexGend", (void*)glTexGend, 3, arg0, arg1, arg2);
     glad_glTexGend(arg0, arg1, arg2);
    _post_call_callback("glTexGend", (void*)glTexGend, 3, arg0, arg1, arg2);
    
}
PFNGLTEXGENDPROC glad_debug_glTexGend = glad_debug_impl_glTexGend;
PFNGLTEXGENDVPROC glad_glTexGendv;
void APIENTRY glad_debug_impl_glTexGendv(GLenum arg0, GLenum arg1, const GLdouble * arg2) {    
    _pre_call_callback("glTexGendv", (void*)glTexGendv, 3, arg0, arg1, arg2);
     glad_glTexGendv(arg0, arg1, arg2);
    _post_call_callback("glTexGendv", (void*)glTexGendv, 3, arg0, arg1, arg2);
    
}
PFNGLTEXGENDVPROC glad_debug_glTexGendv = glad_debug_impl_glTexGendv;
PFNGLTEXGENFPROC glad_glTexGenf;
void APIENTRY glad_debug_impl_glTexGenf(GLenum arg0, GLenum arg1, GLfloat arg2) {    
    _pre_call_callback("glTexGenf", (void*)glTexGenf, 3, arg0, arg1, arg2);
     glad_glTexGenf(arg0, arg1, arg2);
    _post_call_callback("glTexGenf", (void*)glTexGenf, 3, arg0, arg1, arg2);
    
}
PFNGLTEXGENFPROC glad_debug_glTexGenf = glad_debug_impl_glTexGenf;
PFNGLTEXGENFVPROC glad_glTexGenfv;
void APIENTRY glad_debug_impl_glTexGenfv(GLenum arg0, GLenum arg1, const GLfloat * arg2) {    
    _pre_call_callback("glTexGenfv", (void*)glTexGenfv, 3, arg0, arg1, arg2);
     glad_glTexGenfv(arg0, arg1, arg2);
    _post_call_callback("glTexGenfv", (void*)glTexGenfv, 3, arg0, arg1, arg2);
    
}
PFNGLTEXGENFVPROC glad_debug_glTexGenfv = glad_debug_impl_glTexGenfv;
PFNGLTEXGENIPROC glad_glTexGeni;
void APIENTRY glad_debug_impl_glTexGeni(GLenum arg0, GLenum arg1, GLint arg2) {    
    _pre_call_callback("glTexGeni", (void*)glTexGeni, 3, arg0, arg1, arg2);
     glad_glTexGeni(arg0, arg1, arg2);
    _post_call_callback("glTexGeni", (void*)glTexGeni, 3, arg0, arg1, arg2);
    
}
PFNGLTEXGENIPROC glad_debug_glTexGeni = glad_debug_impl_glTexGeni;
PFNGLTEXGENIVPROC glad_glTexGeniv;
void APIENTRY glad_debug_impl_glTexGeniv(GLenum arg0, GLenum arg1, const GLint * arg2) {    
    _pre_call_callback("glTexGeniv", (void*)glTexGeniv, 3, arg0, arg1, arg2);
     glad_glTexGeniv(arg0, arg1, arg2);
    _post_call_callback("glTexGeniv", (void*)glTexGeniv, 3, arg0, arg1, arg2);
    
}
PFNGLTEXGENIVPROC glad_debug_glTexGeniv = glad_debug_impl_glTexGeniv;
PFNGLTEXIMAGE1DPROC glad_glTexImage1D;
void APIENTRY glad_debug_impl_glTexImage1D(GLenum arg0, GLint arg1, GLint arg2, GLsizei arg3, GLint arg4, GLenum arg5, GLenum arg6, const void * arg7) {    
    _pre_call_callback("glTexImage1D", (void*)glTexImage1D, 8, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
     glad_glTexImage1D(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    _post_call_callback("glTexImage1D", (void*)glTexImage1D, 8, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    
}
PFNGLTEXIMAGE1DPROC glad_debug_glTexImage1D = glad_debug_impl_glTexImage1D;
PFNGLTEXIMAGE2DPROC glad_glTexImage2D;
void APIENTRY glad_debug_impl_glTexImage2D(GLenum arg0, GLint arg1, GLint arg2, GLsizei arg3, GLsizei arg4, GLint arg5, GLenum arg6, GLenum arg7, const void * arg8) {    
    _pre_call_callback("glTexImage2D", (void*)glTexImage2D, 9, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
     glad_glTexImage2D(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
    _post_call_callback("glTexImage2D", (void*)glTexImage2D, 9, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
    
}
PFNGLTEXIMAGE2DPROC glad_debug_glTexImage2D = glad_debug_impl_glTexImage2D;
PFNGLTEXIMAGE2DMULTISAMPLEPROC glad_glTexImage2DMultisample;
void APIENTRY glad_debug_impl_glTexImage2DMultisample(GLenum arg0, GLsizei arg1, GLenum arg2, GLsizei arg3, GLsizei arg4, GLboolean arg5) {    
    _pre_call_callback("glTexImage2DMultisample", (void*)glTexImage2DMultisample, 6, arg0, arg1, arg2, arg3, arg4, arg5);
     glad_glTexImage2DMultisample(arg0, arg1, arg2, arg3, arg4, arg5);
    _post_call_callback("glTexImage2DMultisample", (void*)glTexImage2DMultisample, 6, arg0, arg1, arg2, arg3, arg4, arg5);
    
}
PFNGLTEXIMAGE2DMULTISAMPLEPROC glad_debug_glTexImage2DMultisample = glad_debug_impl_glTexImage2DMultisample;
PFNGLTEXIMAGE3DPROC glad_glTexImage3D;
void APIENTRY glad_debug_impl_glTexImage3D(GLenum arg0, GLint arg1, GLint arg2, GLsizei arg3, GLsizei arg4, GLsizei arg5, GLint arg6, GLenum arg7, GLenum arg8, const void * arg9) {    
    _pre_call_callback("glTexImage3D", (void*)glTexImage3D, 10, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
     glad_glTexImage3D(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
    _post_call_callback("glTexImage3D", (void*)glTexImage3D, 10, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
    
}
PFNGLTEXIMAGE3DPROC glad_debug_glTexImage3D = glad_debug_impl_glTexImage3D;
PFNGLTEXIMAGE3DMULTISAMPLEPROC glad_glTexImage3DMultisample;
void APIENTRY glad_debug_impl_glTexImage3DMultisample(GLenum arg0, GLsizei arg1, GLenum arg2, GLsizei arg3, GLsizei arg4, GLsizei arg5, GLboolean arg6) {    
    _pre_call_callback("glTexImage3DMultisample", (void*)glTexImage3DMultisample, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
     glad_glTexImage3DMultisample(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    _post_call_callback("glTexImage3DMultisample", (void*)glTexImage3DMultisample, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    
}
PFNGLTEXIMAGE3DMULTISAMPLEPROC glad_debug_glTexImage3DMultisample = glad_debug_impl_glTexImage3DMultisample;
PFNGLTEXPARAMETERIIVPROC glad_glTexParameterIiv;
void APIENTRY glad_debug_impl_glTexParameterIiv(GLenum arg0, GLenum arg1, const GLint * arg2) {    
    _pre_call_callback("glTexParameterIiv", (void*)glTexParameterIiv, 3, arg0, arg1, arg2);
     glad_glTexParameterIiv(arg0, arg1, arg2);
    _post_call_callback("glTexParameterIiv", (void*)glTexParameterIiv, 3, arg0, arg1, arg2);
    
}
PFNGLTEXPARAMETERIIVPROC glad_debug_glTexParameterIiv = glad_debug_impl_glTexParameterIiv;
PFNGLTEXPARAMETERIUIVPROC glad_glTexParameterIuiv;
void APIENTRY glad_debug_impl_glTexParameterIuiv(GLenum arg0, GLenum arg1, const GLuint * arg2) {    
    _pre_call_callback("glTexParameterIuiv", (void*)glTexParameterIuiv, 3, arg0, arg1, arg2);
     glad_glTexParameterIuiv(arg0, arg1, arg2);
    _post_call_callback("glTexParameterIuiv", (void*)glTexParameterIuiv, 3, arg0, arg1, arg2);
    
}
PFNGLTEXPARAMETERIUIVPROC glad_debug_glTexParameterIuiv = glad_debug_impl_glTexParameterIuiv;
PFNGLTEXPARAMETERFPROC glad_glTexParameterf;
void APIENTRY glad_debug_impl_glTexParameterf(GLenum arg0, GLenum arg1, GLfloat arg2) {    
    _pre_call_callback("glTexParameterf", (void*)glTexParameterf, 3, arg0, arg1, arg2);
     glad_glTexParameterf(arg0, arg1, arg2);
    _post_call_callback("glTexParameterf", (void*)glTexParameterf, 3, arg0, arg1, arg2);
    
}
PFNGLTEXPARAMETERFPROC glad_debug_glTexParameterf = glad_debug_impl_glTexParameterf;
PFNGLTEXPARAMETERFVPROC glad_glTexParameterfv;
void APIENTRY glad_debug_impl_glTexParameterfv(GLenum arg0, GLenum arg1, const GLfloat * arg2) {    
    _pre_call_callback("glTexParameterfv", (void*)glTexParameterfv, 3, arg0, arg1, arg2);
     glad_glTexParameterfv(arg0, arg1, arg2);
    _post_call_callback("glTexParameterfv", (void*)glTexParameterfv, 3, arg0, arg1, arg2);
    
}
PFNGLTEXPARAMETERFVPROC glad_debug_glTexParameterfv = glad_debug_impl_glTexParameterfv;
PFNGLTEXPARAMETERIPROC glad_glTexParameteri;
void APIENTRY glad_debug_impl_glTexParameteri(GLenum arg0, GLenum arg1, GLint arg2) {    
    _pre_call_callback("glTexParameteri", (void*)glTexParameteri, 3, arg0, arg1, arg2);
     glad_glTexParameteri(arg0, arg1, arg2);
    _post_call_callback("glTexParameteri", (void*)glTexParameteri, 3, arg0, arg1, arg2);
    
}
PFNGLTEXPARAMETERIPROC glad_debug_glTexParameteri = glad_debug_impl_glTexParameteri;
PFNGLTEXPARAMETERIVPROC glad_glTexParameteriv;
void APIENTRY glad_debug_impl_glTexParameteriv(GLenum arg0, GLenum arg1, const GLint * arg2) {    
    _pre_call_callback("glTexParameteriv", (void*)glTexParameteriv, 3, arg0, arg1, arg2);
     glad_glTexParameteriv(arg0, arg1, arg2);
    _post_call_callback("glTexParameteriv", (void*)glTexParameteriv, 3, arg0, arg1, arg2);
    
}
PFNGLTEXPARAMETERIVPROC glad_debug_glTexParameteriv = glad_debug_impl_glTexParameteriv;
PFNGLTEXSTORAGE1DPROC glad_glTexStorage1D;
void APIENTRY glad_debug_impl_glTexStorage1D(GLenum arg0, GLsizei arg1, GLenum arg2, GLsizei arg3) {    
    _pre_call_callback("glTexStorage1D", (void*)glTexStorage1D, 4, arg0, arg1, arg2, arg3);
     glad_glTexStorage1D(arg0, arg1, arg2, arg3);
    _post_call_callback("glTexStorage1D", (void*)glTexStorage1D, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLTEXSTORAGE1DPROC glad_debug_glTexStorage1D = glad_debug_impl_glTexStorage1D;
PFNGLTEXSTORAGE2DPROC glad_glTexStorage2D;
void APIENTRY glad_debug_impl_glTexStorage2D(GLenum arg0, GLsizei arg1, GLenum arg2, GLsizei arg3, GLsizei arg4) {    
    _pre_call_callback("glTexStorage2D", (void*)glTexStorage2D, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glTexStorage2D(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glTexStorage2D", (void*)glTexStorage2D, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLTEXSTORAGE2DPROC glad_debug_glTexStorage2D = glad_debug_impl_glTexStorage2D;
PFNGLTEXSTORAGE2DMULTISAMPLEPROC glad_glTexStorage2DMultisample;
void APIENTRY glad_debug_impl_glTexStorage2DMultisample(GLenum arg0, GLsizei arg1, GLenum arg2, GLsizei arg3, GLsizei arg4, GLboolean arg5) {    
    _pre_call_callback("glTexStorage2DMultisample", (void*)glTexStorage2DMultisample, 6, arg0, arg1, arg2, arg3, arg4, arg5);
     glad_glTexStorage2DMultisample(arg0, arg1, arg2, arg3, arg4, arg5);
    _post_call_callback("glTexStorage2DMultisample", (void*)glTexStorage2DMultisample, 6, arg0, arg1, arg2, arg3, arg4, arg5);
    
}
PFNGLTEXSTORAGE2DMULTISAMPLEPROC glad_debug_glTexStorage2DMultisample = glad_debug_impl_glTexStorage2DMultisample;
PFNGLTEXSTORAGE3DPROC glad_glTexStorage3D;
void APIENTRY glad_debug_impl_glTexStorage3D(GLenum arg0, GLsizei arg1, GLenum arg2, GLsizei arg3, GLsizei arg4, GLsizei arg5) {    
    _pre_call_callback("glTexStorage3D", (void*)glTexStorage3D, 6, arg0, arg1, arg2, arg3, arg4, arg5);
     glad_glTexStorage3D(arg0, arg1, arg2, arg3, arg4, arg5);
    _post_call_callback("glTexStorage3D", (void*)glTexStorage3D, 6, arg0, arg1, arg2, arg3, arg4, arg5);
    
}
PFNGLTEXSTORAGE3DPROC glad_debug_glTexStorage3D = glad_debug_impl_glTexStorage3D;
PFNGLTEXSTORAGE3DMULTISAMPLEPROC glad_glTexStorage3DMultisample;
void APIENTRY glad_debug_impl_glTexStorage3DMultisample(GLenum arg0, GLsizei arg1, GLenum arg2, GLsizei arg3, GLsizei arg4, GLsizei arg5, GLboolean arg6) {    
    _pre_call_callback("glTexStorage3DMultisample", (void*)glTexStorage3DMultisample, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
     glad_glTexStorage3DMultisample(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    _post_call_callback("glTexStorage3DMultisample", (void*)glTexStorage3DMultisample, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    
}
PFNGLTEXSTORAGE3DMULTISAMPLEPROC glad_debug_glTexStorage3DMultisample = glad_debug_impl_glTexStorage3DMultisample;
PFNGLTEXSUBIMAGE1DPROC glad_glTexSubImage1D;
void APIENTRY glad_debug_impl_glTexSubImage1D(GLenum arg0, GLint arg1, GLint arg2, GLsizei arg3, GLenum arg4, GLenum arg5, const void * arg6) {    
    _pre_call_callback("glTexSubImage1D", (void*)glTexSubImage1D, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
     glad_glTexSubImage1D(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    _post_call_callback("glTexSubImage1D", (void*)glTexSubImage1D, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    
}
PFNGLTEXSUBIMAGE1DPROC glad_debug_glTexSubImage1D = glad_debug_impl_glTexSubImage1D;
PFNGLTEXSUBIMAGE2DPROC glad_glTexSubImage2D;
void APIENTRY glad_debug_impl_glTexSubImage2D(GLenum arg0, GLint arg1, GLint arg2, GLint arg3, GLsizei arg4, GLsizei arg5, GLenum arg6, GLenum arg7, const void * arg8) {    
    _pre_call_callback("glTexSubImage2D", (void*)glTexSubImage2D, 9, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
     glad_glTexSubImage2D(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
    _post_call_callback("glTexSubImage2D", (void*)glTexSubImage2D, 9, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
    
}
PFNGLTEXSUBIMAGE2DPROC glad_debug_glTexSubImage2D = glad_debug_impl_glTexSubImage2D;
PFNGLTEXSUBIMAGE3DPROC glad_glTexSubImage3D;
void APIENTRY glad_debug_impl_glTexSubImage3D(GLenum arg0, GLint arg1, GLint arg2, GLint arg3, GLint arg4, GLsizei arg5, GLsizei arg6, GLsizei arg7, GLenum arg8, GLenum arg9, const void * arg10) {    
    _pre_call_callback("glTexSubImage3D", (void*)glTexSubImage3D, 11, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10);
     glad_glTexSubImage3D(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10);
    _post_call_callback("glTexSubImage3D", (void*)glTexSubImage3D, 11, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10);
    
}
PFNGLTEXSUBIMAGE3DPROC glad_debug_glTexSubImage3D = glad_debug_impl_glTexSubImage3D;
PFNGLTEXTUREBARRIERPROC glad_glTextureBarrier;
void APIENTRY glad_debug_impl_glTextureBarrier(void) {    
    _pre_call_callback("glTextureBarrier", (void*)glTextureBarrier, 0);
     glad_glTextureBarrier();
    _post_call_callback("glTextureBarrier", (void*)glTextureBarrier, 0);
    
}
PFNGLTEXTUREBARRIERPROC glad_debug_glTextureBarrier = glad_debug_impl_glTextureBarrier;
PFNGLTEXTUREBUFFERPROC glad_glTextureBuffer;
void APIENTRY glad_debug_impl_glTextureBuffer(GLuint arg0, GLenum arg1, GLuint arg2) {    
    _pre_call_callback("glTextureBuffer", (void*)glTextureBuffer, 3, arg0, arg1, arg2);
     glad_glTextureBuffer(arg0, arg1, arg2);
    _post_call_callback("glTextureBuffer", (void*)glTextureBuffer, 3, arg0, arg1, arg2);
    
}
PFNGLTEXTUREBUFFERPROC glad_debug_glTextureBuffer = glad_debug_impl_glTextureBuffer;
PFNGLTEXTUREBUFFERRANGEPROC glad_glTextureBufferRange;
void APIENTRY glad_debug_impl_glTextureBufferRange(GLuint arg0, GLenum arg1, GLuint arg2, GLintptr arg3, GLsizeiptr arg4) {    
    _pre_call_callback("glTextureBufferRange", (void*)glTextureBufferRange, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glTextureBufferRange(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glTextureBufferRange", (void*)glTextureBufferRange, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLTEXTUREBUFFERRANGEPROC glad_debug_glTextureBufferRange = glad_debug_impl_glTextureBufferRange;
PFNGLTEXTUREPARAMETERIIVPROC glad_glTextureParameterIiv;
void APIENTRY glad_debug_impl_glTextureParameterIiv(GLuint arg0, GLenum arg1, const GLint * arg2) {    
    _pre_call_callback("glTextureParameterIiv", (void*)glTextureParameterIiv, 3, arg0, arg1, arg2);
     glad_glTextureParameterIiv(arg0, arg1, arg2);
    _post_call_callback("glTextureParameterIiv", (void*)glTextureParameterIiv, 3, arg0, arg1, arg2);
    
}
PFNGLTEXTUREPARAMETERIIVPROC glad_debug_glTextureParameterIiv = glad_debug_impl_glTextureParameterIiv;
PFNGLTEXTUREPARAMETERIUIVPROC glad_glTextureParameterIuiv;
void APIENTRY glad_debug_impl_glTextureParameterIuiv(GLuint arg0, GLenum arg1, const GLuint * arg2) {    
    _pre_call_callback("glTextureParameterIuiv", (void*)glTextureParameterIuiv, 3, arg0, arg1, arg2);
     glad_glTextureParameterIuiv(arg0, arg1, arg2);
    _post_call_callback("glTextureParameterIuiv", (void*)glTextureParameterIuiv, 3, arg0, arg1, arg2);
    
}
PFNGLTEXTUREPARAMETERIUIVPROC glad_debug_glTextureParameterIuiv = glad_debug_impl_glTextureParameterIuiv;
PFNGLTEXTUREPARAMETERFPROC glad_glTextureParameterf;
void APIENTRY glad_debug_impl_glTextureParameterf(GLuint arg0, GLenum arg1, GLfloat arg2) {    
    _pre_call_callback("glTextureParameterf", (void*)glTextureParameterf, 3, arg0, arg1, arg2);
     glad_glTextureParameterf(arg0, arg1, arg2);
    _post_call_callback("glTextureParameterf", (void*)glTextureParameterf, 3, arg0, arg1, arg2);
    
}
PFNGLTEXTUREPARAMETERFPROC glad_debug_glTextureParameterf = glad_debug_impl_glTextureParameterf;
PFNGLTEXTUREPARAMETERFVPROC glad_glTextureParameterfv;
void APIENTRY glad_debug_impl_glTextureParameterfv(GLuint arg0, GLenum arg1, const GLfloat * arg2) {    
    _pre_call_callback("glTextureParameterfv", (void*)glTextureParameterfv, 3, arg0, arg1, arg2);
     glad_glTextureParameterfv(arg0, arg1, arg2);
    _post_call_callback("glTextureParameterfv", (void*)glTextureParameterfv, 3, arg0, arg1, arg2);
    
}
PFNGLTEXTUREPARAMETERFVPROC glad_debug_glTextureParameterfv = glad_debug_impl_glTextureParameterfv;
PFNGLTEXTUREPARAMETERIPROC glad_glTextureParameteri;
void APIENTRY glad_debug_impl_glTextureParameteri(GLuint arg0, GLenum arg1, GLint arg2) {    
    _pre_call_callback("glTextureParameteri", (void*)glTextureParameteri, 3, arg0, arg1, arg2);
     glad_glTextureParameteri(arg0, arg1, arg2);
    _post_call_callback("glTextureParameteri", (void*)glTextureParameteri, 3, arg0, arg1, arg2);
    
}
PFNGLTEXTUREPARAMETERIPROC glad_debug_glTextureParameteri = glad_debug_impl_glTextureParameteri;
PFNGLTEXTUREPARAMETERIVPROC glad_glTextureParameteriv;
void APIENTRY glad_debug_impl_glTextureParameteriv(GLuint arg0, GLenum arg1, const GLint * arg2) {    
    _pre_call_callback("glTextureParameteriv", (void*)glTextureParameteriv, 3, arg0, arg1, arg2);
     glad_glTextureParameteriv(arg0, arg1, arg2);
    _post_call_callback("glTextureParameteriv", (void*)glTextureParameteriv, 3, arg0, arg1, arg2);
    
}
PFNGLTEXTUREPARAMETERIVPROC glad_debug_glTextureParameteriv = glad_debug_impl_glTextureParameteriv;
PFNGLTEXTURESTORAGE1DPROC glad_glTextureStorage1D;
void APIENTRY glad_debug_impl_glTextureStorage1D(GLuint arg0, GLsizei arg1, GLenum arg2, GLsizei arg3) {    
    _pre_call_callback("glTextureStorage1D", (void*)glTextureStorage1D, 4, arg0, arg1, arg2, arg3);
     glad_glTextureStorage1D(arg0, arg1, arg2, arg3);
    _post_call_callback("glTextureStorage1D", (void*)glTextureStorage1D, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLTEXTURESTORAGE1DPROC glad_debug_glTextureStorage1D = glad_debug_impl_glTextureStorage1D;
PFNGLTEXTURESTORAGE2DPROC glad_glTextureStorage2D;
void APIENTRY glad_debug_impl_glTextureStorage2D(GLuint arg0, GLsizei arg1, GLenum arg2, GLsizei arg3, GLsizei arg4) {    
    _pre_call_callback("glTextureStorage2D", (void*)glTextureStorage2D, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glTextureStorage2D(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glTextureStorage2D", (void*)glTextureStorage2D, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLTEXTURESTORAGE2DPROC glad_debug_glTextureStorage2D = glad_debug_impl_glTextureStorage2D;
PFNGLTEXTURESTORAGE2DMULTISAMPLEPROC glad_glTextureStorage2DMultisample;
void APIENTRY glad_debug_impl_glTextureStorage2DMultisample(GLuint arg0, GLsizei arg1, GLenum arg2, GLsizei arg3, GLsizei arg4, GLboolean arg5) {    
    _pre_call_callback("glTextureStorage2DMultisample", (void*)glTextureStorage2DMultisample, 6, arg0, arg1, arg2, arg3, arg4, arg5);
     glad_glTextureStorage2DMultisample(arg0, arg1, arg2, arg3, arg4, arg5);
    _post_call_callback("glTextureStorage2DMultisample", (void*)glTextureStorage2DMultisample, 6, arg0, arg1, arg2, arg3, arg4, arg5);
    
}
PFNGLTEXTURESTORAGE2DMULTISAMPLEPROC glad_debug_glTextureStorage2DMultisample = glad_debug_impl_glTextureStorage2DMultisample;
PFNGLTEXTURESTORAGE3DPROC glad_glTextureStorage3D;
void APIENTRY glad_debug_impl_glTextureStorage3D(GLuint arg0, GLsizei arg1, GLenum arg2, GLsizei arg3, GLsizei arg4, GLsizei arg5) {    
    _pre_call_callback("glTextureStorage3D", (void*)glTextureStorage3D, 6, arg0, arg1, arg2, arg3, arg4, arg5);
     glad_glTextureStorage3D(arg0, arg1, arg2, arg3, arg4, arg5);
    _post_call_callback("glTextureStorage3D", (void*)glTextureStorage3D, 6, arg0, arg1, arg2, arg3, arg4, arg5);
    
}
PFNGLTEXTURESTORAGE3DPROC glad_debug_glTextureStorage3D = glad_debug_impl_glTextureStorage3D;
PFNGLTEXTURESTORAGE3DMULTISAMPLEPROC glad_glTextureStorage3DMultisample;
void APIENTRY glad_debug_impl_glTextureStorage3DMultisample(GLuint arg0, GLsizei arg1, GLenum arg2, GLsizei arg3, GLsizei arg4, GLsizei arg5, GLboolean arg6) {    
    _pre_call_callback("glTextureStorage3DMultisample", (void*)glTextureStorage3DMultisample, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
     glad_glTextureStorage3DMultisample(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    _post_call_callback("glTextureStorage3DMultisample", (void*)glTextureStorage3DMultisample, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    
}
PFNGLTEXTURESTORAGE3DMULTISAMPLEPROC glad_debug_glTextureStorage3DMultisample = glad_debug_impl_glTextureStorage3DMultisample;
PFNGLTEXTURESUBIMAGE1DPROC glad_glTextureSubImage1D;
void APIENTRY glad_debug_impl_glTextureSubImage1D(GLuint arg0, GLint arg1, GLint arg2, GLsizei arg3, GLenum arg4, GLenum arg5, const void * arg6) {    
    _pre_call_callback("glTextureSubImage1D", (void*)glTextureSubImage1D, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
     glad_glTextureSubImage1D(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    _post_call_callback("glTextureSubImage1D", (void*)glTextureSubImage1D, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    
}
PFNGLTEXTURESUBIMAGE1DPROC glad_debug_glTextureSubImage1D = glad_debug_impl_glTextureSubImage1D;
PFNGLTEXTURESUBIMAGE2DPROC glad_glTextureSubImage2D;
void APIENTRY glad_debug_impl_glTextureSubImage2D(GLuint arg0, GLint arg1, GLint arg2, GLint arg3, GLsizei arg4, GLsizei arg5, GLenum arg6, GLenum arg7, const void * arg8) {    
    _pre_call_callback("glTextureSubImage2D", (void*)glTextureSubImage2D, 9, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
     glad_glTextureSubImage2D(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
    _post_call_callback("glTextureSubImage2D", (void*)glTextureSubImage2D, 9, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
    
}
PFNGLTEXTURESUBIMAGE2DPROC glad_debug_glTextureSubImage2D = glad_debug_impl_glTextureSubImage2D;
PFNGLTEXTURESUBIMAGE3DPROC glad_glTextureSubImage3D;
void APIENTRY glad_debug_impl_glTextureSubImage3D(GLuint arg0, GLint arg1, GLint arg2, GLint arg3, GLint arg4, GLsizei arg5, GLsizei arg6, GLsizei arg7, GLenum arg8, GLenum arg9, const void * arg10) {    
    _pre_call_callback("glTextureSubImage3D", (void*)glTextureSubImage3D, 11, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10);
     glad_glTextureSubImage3D(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10);
    _post_call_callback("glTextureSubImage3D", (void*)glTextureSubImage3D, 11, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10);
    
}
PFNGLTEXTURESUBIMAGE3DPROC glad_debug_glTextureSubImage3D = glad_debug_impl_glTextureSubImage3D;
PFNGLTEXTUREVIEWPROC glad_glTextureView;
void APIENTRY glad_debug_impl_glTextureView(GLuint arg0, GLenum arg1, GLuint arg2, GLenum arg3, GLuint arg4, GLuint arg5, GLuint arg6, GLuint arg7) {    
    _pre_call_callback("glTextureView", (void*)glTextureView, 8, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
     glad_glTextureView(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    _post_call_callback("glTextureView", (void*)glTextureView, 8, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    
}
PFNGLTEXTUREVIEWPROC glad_debug_glTextureView = glad_debug_impl_glTextureView;
PFNGLTRANSFORMFEEDBACKBUFFERBASEPROC glad_glTransformFeedbackBufferBase;
void APIENTRY glad_debug_impl_glTransformFeedbackBufferBase(GLuint arg0, GLuint arg1, GLuint arg2) {    
    _pre_call_callback("glTransformFeedbackBufferBase", (void*)glTransformFeedbackBufferBase, 3, arg0, arg1, arg2);
     glad_glTransformFeedbackBufferBase(arg0, arg1, arg2);
    _post_call_callback("glTransformFeedbackBufferBase", (void*)glTransformFeedbackBufferBase, 3, arg0, arg1, arg2);
    
}
PFNGLTRANSFORMFEEDBACKBUFFERBASEPROC glad_debug_glTransformFeedbackBufferBase = glad_debug_impl_glTransformFeedbackBufferBase;
PFNGLTRANSFORMFEEDBACKBUFFERRANGEPROC glad_glTransformFeedbackBufferRange;
void APIENTRY glad_debug_impl_glTransformFeedbackBufferRange(GLuint arg0, GLuint arg1, GLuint arg2, GLintptr arg3, GLsizeiptr arg4) {    
    _pre_call_callback("glTransformFeedbackBufferRange", (void*)glTransformFeedbackBufferRange, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glTransformFeedbackBufferRange(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glTransformFeedbackBufferRange", (void*)glTransformFeedbackBufferRange, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLTRANSFORMFEEDBACKBUFFERRANGEPROC glad_debug_glTransformFeedbackBufferRange = glad_debug_impl_glTransformFeedbackBufferRange;
PFNGLTRANSFORMFEEDBACKVARYINGSPROC glad_glTransformFeedbackVaryings;
void APIENTRY glad_debug_impl_glTransformFeedbackVaryings(GLuint arg0, GLsizei arg1, const GLchar *const* arg2, GLenum arg3) {    
    _pre_call_callback("glTransformFeedbackVaryings", (void*)glTransformFeedbackVaryings, 4, arg0, arg1, arg2, arg3);
     glad_glTransformFeedbackVaryings(arg0, arg1, arg2, arg3);
    _post_call_callback("glTransformFeedbackVaryings", (void*)glTransformFeedbackVaryings, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLTRANSFORMFEEDBACKVARYINGSPROC glad_debug_glTransformFeedbackVaryings = glad_debug_impl_glTransformFeedbackVaryings;
PFNGLTRANSLATEDPROC glad_glTranslated;
void APIENTRY glad_debug_impl_glTranslated(GLdouble arg0, GLdouble arg1, GLdouble arg2) {    
    _pre_call_callback("glTranslated", (void*)glTranslated, 3, arg0, arg1, arg2);
     glad_glTranslated(arg0, arg1, arg2);
    _post_call_callback("glTranslated", (void*)glTranslated, 3, arg0, arg1, arg2);
    
}
PFNGLTRANSLATEDPROC glad_debug_glTranslated = glad_debug_impl_glTranslated;
PFNGLTRANSLATEFPROC glad_glTranslatef;
void APIENTRY glad_debug_impl_glTranslatef(GLfloat arg0, GLfloat arg1, GLfloat arg2) {    
    _pre_call_callback("glTranslatef", (void*)glTranslatef, 3, arg0, arg1, arg2);
     glad_glTranslatef(arg0, arg1, arg2);
    _post_call_callback("glTranslatef", (void*)glTranslatef, 3, arg0, arg1, arg2);
    
}
PFNGLTRANSLATEFPROC glad_debug_glTranslatef = glad_debug_impl_glTranslatef;
PFNGLUNIFORM1DPROC glad_glUniform1d;
void APIENTRY glad_debug_impl_glUniform1d(GLint arg0, GLdouble arg1) {    
    _pre_call_callback("glUniform1d", (void*)glUniform1d, 2, arg0, arg1);
     glad_glUniform1d(arg0, arg1);
    _post_call_callback("glUniform1d", (void*)glUniform1d, 2, arg0, arg1);
    
}
PFNGLUNIFORM1DPROC glad_debug_glUniform1d = glad_debug_impl_glUniform1d;
PFNGLUNIFORM1DVPROC glad_glUniform1dv;
void APIENTRY glad_debug_impl_glUniform1dv(GLint arg0, GLsizei arg1, const GLdouble * arg2) {    
    _pre_call_callback("glUniform1dv", (void*)glUniform1dv, 3, arg0, arg1, arg2);
     glad_glUniform1dv(arg0, arg1, arg2);
    _post_call_callback("glUniform1dv", (void*)glUniform1dv, 3, arg0, arg1, arg2);
    
}
PFNGLUNIFORM1DVPROC glad_debug_glUniform1dv = glad_debug_impl_glUniform1dv;
PFNGLUNIFORM1FPROC glad_glUniform1f;
void APIENTRY glad_debug_impl_glUniform1f(GLint arg0, GLfloat arg1) {    
    _pre_call_callback("glUniform1f", (void*)glUniform1f, 2, arg0, arg1);
     glad_glUniform1f(arg0, arg1);
    _post_call_callback("glUniform1f", (void*)glUniform1f, 2, arg0, arg1);
    
}
PFNGLUNIFORM1FPROC glad_debug_glUniform1f = glad_debug_impl_glUniform1f;
PFNGLUNIFORM1FVPROC glad_glUniform1fv;
void APIENTRY glad_debug_impl_glUniform1fv(GLint arg0, GLsizei arg1, const GLfloat * arg2) {    
    _pre_call_callback("glUniform1fv", (void*)glUniform1fv, 3, arg0, arg1, arg2);
     glad_glUniform1fv(arg0, arg1, arg2);
    _post_call_callback("glUniform1fv", (void*)glUniform1fv, 3, arg0, arg1, arg2);
    
}
PFNGLUNIFORM1FVPROC glad_debug_glUniform1fv = glad_debug_impl_glUniform1fv;
PFNGLUNIFORM1IPROC glad_glUniform1i;
void APIENTRY glad_debug_impl_glUniform1i(GLint arg0, GLint arg1) {    
    _pre_call_callback("glUniform1i", (void*)glUniform1i, 2, arg0, arg1);
     glad_glUniform1i(arg0, arg1);
    _post_call_callback("glUniform1i", (void*)glUniform1i, 2, arg0, arg1);
    
}
PFNGLUNIFORM1IPROC glad_debug_glUniform1i = glad_debug_impl_glUniform1i;
PFNGLUNIFORM1IVPROC glad_glUniform1iv;
void APIENTRY glad_debug_impl_glUniform1iv(GLint arg0, GLsizei arg1, const GLint * arg2) {    
    _pre_call_callback("glUniform1iv", (void*)glUniform1iv, 3, arg0, arg1, arg2);
     glad_glUniform1iv(arg0, arg1, arg2);
    _post_call_callback("glUniform1iv", (void*)glUniform1iv, 3, arg0, arg1, arg2);
    
}
PFNGLUNIFORM1IVPROC glad_debug_glUniform1iv = glad_debug_impl_glUniform1iv;
PFNGLUNIFORM1UIPROC glad_glUniform1ui;
void APIENTRY glad_debug_impl_glUniform1ui(GLint arg0, GLuint arg1) {    
    _pre_call_callback("glUniform1ui", (void*)glUniform1ui, 2, arg0, arg1);
     glad_glUniform1ui(arg0, arg1);
    _post_call_callback("glUniform1ui", (void*)glUniform1ui, 2, arg0, arg1);
    
}
PFNGLUNIFORM1UIPROC glad_debug_glUniform1ui = glad_debug_impl_glUniform1ui;
PFNGLUNIFORM1UIVPROC glad_glUniform1uiv;
void APIENTRY glad_debug_impl_glUniform1uiv(GLint arg0, GLsizei arg1, const GLuint * arg2) {    
    _pre_call_callback("glUniform1uiv", (void*)glUniform1uiv, 3, arg0, arg1, arg2);
     glad_glUniform1uiv(arg0, arg1, arg2);
    _post_call_callback("glUniform1uiv", (void*)glUniform1uiv, 3, arg0, arg1, arg2);
    
}
PFNGLUNIFORM1UIVPROC glad_debug_glUniform1uiv = glad_debug_impl_glUniform1uiv;
PFNGLUNIFORM2DPROC glad_glUniform2d;
void APIENTRY glad_debug_impl_glUniform2d(GLint arg0, GLdouble arg1, GLdouble arg2) {    
    _pre_call_callback("glUniform2d", (void*)glUniform2d, 3, arg0, arg1, arg2);
     glad_glUniform2d(arg0, arg1, arg2);
    _post_call_callback("glUniform2d", (void*)glUniform2d, 3, arg0, arg1, arg2);
    
}
PFNGLUNIFORM2DPROC glad_debug_glUniform2d = glad_debug_impl_glUniform2d;
PFNGLUNIFORM2DVPROC glad_glUniform2dv;
void APIENTRY glad_debug_impl_glUniform2dv(GLint arg0, GLsizei arg1, const GLdouble * arg2) {    
    _pre_call_callback("glUniform2dv", (void*)glUniform2dv, 3, arg0, arg1, arg2);
     glad_glUniform2dv(arg0, arg1, arg2);
    _post_call_callback("glUniform2dv", (void*)glUniform2dv, 3, arg0, arg1, arg2);
    
}
PFNGLUNIFORM2DVPROC glad_debug_glUniform2dv = glad_debug_impl_glUniform2dv;
PFNGLUNIFORM2FPROC glad_glUniform2f;
void APIENTRY glad_debug_impl_glUniform2f(GLint arg0, GLfloat arg1, GLfloat arg2) {    
    _pre_call_callback("glUniform2f", (void*)glUniform2f, 3, arg0, arg1, arg2);
     glad_glUniform2f(arg0, arg1, arg2);
    _post_call_callback("glUniform2f", (void*)glUniform2f, 3, arg0, arg1, arg2);
    
}
PFNGLUNIFORM2FPROC glad_debug_glUniform2f = glad_debug_impl_glUniform2f;
PFNGLUNIFORM2FVPROC glad_glUniform2fv;
void APIENTRY glad_debug_impl_glUniform2fv(GLint arg0, GLsizei arg1, const GLfloat * arg2) {    
    _pre_call_callback("glUniform2fv", (void*)glUniform2fv, 3, arg0, arg1, arg2);
     glad_glUniform2fv(arg0, arg1, arg2);
    _post_call_callback("glUniform2fv", (void*)glUniform2fv, 3, arg0, arg1, arg2);
    
}
PFNGLUNIFORM2FVPROC glad_debug_glUniform2fv = glad_debug_impl_glUniform2fv;
PFNGLUNIFORM2IPROC glad_glUniform2i;
void APIENTRY glad_debug_impl_glUniform2i(GLint arg0, GLint arg1, GLint arg2) {    
    _pre_call_callback("glUniform2i", (void*)glUniform2i, 3, arg0, arg1, arg2);
     glad_glUniform2i(arg0, arg1, arg2);
    _post_call_callback("glUniform2i", (void*)glUniform2i, 3, arg0, arg1, arg2);
    
}
PFNGLUNIFORM2IPROC glad_debug_glUniform2i = glad_debug_impl_glUniform2i;
PFNGLUNIFORM2IVPROC glad_glUniform2iv;
void APIENTRY glad_debug_impl_glUniform2iv(GLint arg0, GLsizei arg1, const GLint * arg2) {    
    _pre_call_callback("glUniform2iv", (void*)glUniform2iv, 3, arg0, arg1, arg2);
     glad_glUniform2iv(arg0, arg1, arg2);
    _post_call_callback("glUniform2iv", (void*)glUniform2iv, 3, arg0, arg1, arg2);
    
}
PFNGLUNIFORM2IVPROC glad_debug_glUniform2iv = glad_debug_impl_glUniform2iv;
PFNGLUNIFORM2UIPROC glad_glUniform2ui;
void APIENTRY glad_debug_impl_glUniform2ui(GLint arg0, GLuint arg1, GLuint arg2) {    
    _pre_call_callback("glUniform2ui", (void*)glUniform2ui, 3, arg0, arg1, arg2);
     glad_glUniform2ui(arg0, arg1, arg2);
    _post_call_callback("glUniform2ui", (void*)glUniform2ui, 3, arg0, arg1, arg2);
    
}
PFNGLUNIFORM2UIPROC glad_debug_glUniform2ui = glad_debug_impl_glUniform2ui;
PFNGLUNIFORM2UIVPROC glad_glUniform2uiv;
void APIENTRY glad_debug_impl_glUniform2uiv(GLint arg0, GLsizei arg1, const GLuint * arg2) {    
    _pre_call_callback("glUniform2uiv", (void*)glUniform2uiv, 3, arg0, arg1, arg2);
     glad_glUniform2uiv(arg0, arg1, arg2);
    _post_call_callback("glUniform2uiv", (void*)glUniform2uiv, 3, arg0, arg1, arg2);
    
}
PFNGLUNIFORM2UIVPROC glad_debug_glUniform2uiv = glad_debug_impl_glUniform2uiv;
PFNGLUNIFORM3DPROC glad_glUniform3d;
void APIENTRY glad_debug_impl_glUniform3d(GLint arg0, GLdouble arg1, GLdouble arg2, GLdouble arg3) {    
    _pre_call_callback("glUniform3d", (void*)glUniform3d, 4, arg0, arg1, arg2, arg3);
     glad_glUniform3d(arg0, arg1, arg2, arg3);
    _post_call_callback("glUniform3d", (void*)glUniform3d, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLUNIFORM3DPROC glad_debug_glUniform3d = glad_debug_impl_glUniform3d;
PFNGLUNIFORM3DVPROC glad_glUniform3dv;
void APIENTRY glad_debug_impl_glUniform3dv(GLint arg0, GLsizei arg1, const GLdouble * arg2) {    
    _pre_call_callback("glUniform3dv", (void*)glUniform3dv, 3, arg0, arg1, arg2);
     glad_glUniform3dv(arg0, arg1, arg2);
    _post_call_callback("glUniform3dv", (void*)glUniform3dv, 3, arg0, arg1, arg2);
    
}
PFNGLUNIFORM3DVPROC glad_debug_glUniform3dv = glad_debug_impl_glUniform3dv;
PFNGLUNIFORM3FPROC glad_glUniform3f;
void APIENTRY glad_debug_impl_glUniform3f(GLint arg0, GLfloat arg1, GLfloat arg2, GLfloat arg3) {    
    _pre_call_callback("glUniform3f", (void*)glUniform3f, 4, arg0, arg1, arg2, arg3);
     glad_glUniform3f(arg0, arg1, arg2, arg3);
    _post_call_callback("glUniform3f", (void*)glUniform3f, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLUNIFORM3FPROC glad_debug_glUniform3f = glad_debug_impl_glUniform3f;
PFNGLUNIFORM3FVPROC glad_glUniform3fv;
void APIENTRY glad_debug_impl_glUniform3fv(GLint arg0, GLsizei arg1, const GLfloat * arg2) {    
    _pre_call_callback("glUniform3fv", (void*)glUniform3fv, 3, arg0, arg1, arg2);
     glad_glUniform3fv(arg0, arg1, arg2);
    _post_call_callback("glUniform3fv", (void*)glUniform3fv, 3, arg0, arg1, arg2);
    
}
PFNGLUNIFORM3FVPROC glad_debug_glUniform3fv = glad_debug_impl_glUniform3fv;
PFNGLUNIFORM3IPROC glad_glUniform3i;
void APIENTRY glad_debug_impl_glUniform3i(GLint arg0, GLint arg1, GLint arg2, GLint arg3) {    
    _pre_call_callback("glUniform3i", (void*)glUniform3i, 4, arg0, arg1, arg2, arg3);
     glad_glUniform3i(arg0, arg1, arg2, arg3);
    _post_call_callback("glUniform3i", (void*)glUniform3i, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLUNIFORM3IPROC glad_debug_glUniform3i = glad_debug_impl_glUniform3i;
PFNGLUNIFORM3IVPROC glad_glUniform3iv;
void APIENTRY glad_debug_impl_glUniform3iv(GLint arg0, GLsizei arg1, const GLint * arg2) {    
    _pre_call_callback("glUniform3iv", (void*)glUniform3iv, 3, arg0, arg1, arg2);
     glad_glUniform3iv(arg0, arg1, arg2);
    _post_call_callback("glUniform3iv", (void*)glUniform3iv, 3, arg0, arg1, arg2);
    
}
PFNGLUNIFORM3IVPROC glad_debug_glUniform3iv = glad_debug_impl_glUniform3iv;
PFNGLUNIFORM3UIPROC glad_glUniform3ui;
void APIENTRY glad_debug_impl_glUniform3ui(GLint arg0, GLuint arg1, GLuint arg2, GLuint arg3) {    
    _pre_call_callback("glUniform3ui", (void*)glUniform3ui, 4, arg0, arg1, arg2, arg3);
     glad_glUniform3ui(arg0, arg1, arg2, arg3);
    _post_call_callback("glUniform3ui", (void*)glUniform3ui, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLUNIFORM3UIPROC glad_debug_glUniform3ui = glad_debug_impl_glUniform3ui;
PFNGLUNIFORM3UIVPROC glad_glUniform3uiv;
void APIENTRY glad_debug_impl_glUniform3uiv(GLint arg0, GLsizei arg1, const GLuint * arg2) {    
    _pre_call_callback("glUniform3uiv", (void*)glUniform3uiv, 3, arg0, arg1, arg2);
     glad_glUniform3uiv(arg0, arg1, arg2);
    _post_call_callback("glUniform3uiv", (void*)glUniform3uiv, 3, arg0, arg1, arg2);
    
}
PFNGLUNIFORM3UIVPROC glad_debug_glUniform3uiv = glad_debug_impl_glUniform3uiv;
PFNGLUNIFORM4DPROC glad_glUniform4d;
void APIENTRY glad_debug_impl_glUniform4d(GLint arg0, GLdouble arg1, GLdouble arg2, GLdouble arg3, GLdouble arg4) {    
    _pre_call_callback("glUniform4d", (void*)glUniform4d, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glUniform4d(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glUniform4d", (void*)glUniform4d, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLUNIFORM4DPROC glad_debug_glUniform4d = glad_debug_impl_glUniform4d;
PFNGLUNIFORM4DVPROC glad_glUniform4dv;
void APIENTRY glad_debug_impl_glUniform4dv(GLint arg0, GLsizei arg1, const GLdouble * arg2) {    
    _pre_call_callback("glUniform4dv", (void*)glUniform4dv, 3, arg0, arg1, arg2);
     glad_glUniform4dv(arg0, arg1, arg2);
    _post_call_callback("glUniform4dv", (void*)glUniform4dv, 3, arg0, arg1, arg2);
    
}
PFNGLUNIFORM4DVPROC glad_debug_glUniform4dv = glad_debug_impl_glUniform4dv;
PFNGLUNIFORM4FPROC glad_glUniform4f;
void APIENTRY glad_debug_impl_glUniform4f(GLint arg0, GLfloat arg1, GLfloat arg2, GLfloat arg3, GLfloat arg4) {    
    _pre_call_callback("glUniform4f", (void*)glUniform4f, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glUniform4f(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glUniform4f", (void*)glUniform4f, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLUNIFORM4FPROC glad_debug_glUniform4f = glad_debug_impl_glUniform4f;
PFNGLUNIFORM4FVPROC glad_glUniform4fv;
void APIENTRY glad_debug_impl_glUniform4fv(GLint arg0, GLsizei arg1, const GLfloat * arg2) {    
    _pre_call_callback("glUniform4fv", (void*)glUniform4fv, 3, arg0, arg1, arg2);
     glad_glUniform4fv(arg0, arg1, arg2);
    _post_call_callback("glUniform4fv", (void*)glUniform4fv, 3, arg0, arg1, arg2);
    
}
PFNGLUNIFORM4FVPROC glad_debug_glUniform4fv = glad_debug_impl_glUniform4fv;
PFNGLUNIFORM4IPROC glad_glUniform4i;
void APIENTRY glad_debug_impl_glUniform4i(GLint arg0, GLint arg1, GLint arg2, GLint arg3, GLint arg4) {    
    _pre_call_callback("glUniform4i", (void*)glUniform4i, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glUniform4i(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glUniform4i", (void*)glUniform4i, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLUNIFORM4IPROC glad_debug_glUniform4i = glad_debug_impl_glUniform4i;
PFNGLUNIFORM4IVPROC glad_glUniform4iv;
void APIENTRY glad_debug_impl_glUniform4iv(GLint arg0, GLsizei arg1, const GLint * arg2) {    
    _pre_call_callback("glUniform4iv", (void*)glUniform4iv, 3, arg0, arg1, arg2);
     glad_glUniform4iv(arg0, arg1, arg2);
    _post_call_callback("glUniform4iv", (void*)glUniform4iv, 3, arg0, arg1, arg2);
    
}
PFNGLUNIFORM4IVPROC glad_debug_glUniform4iv = glad_debug_impl_glUniform4iv;
PFNGLUNIFORM4UIPROC glad_glUniform4ui;
void APIENTRY glad_debug_impl_glUniform4ui(GLint arg0, GLuint arg1, GLuint arg2, GLuint arg3, GLuint arg4) {    
    _pre_call_callback("glUniform4ui", (void*)glUniform4ui, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glUniform4ui(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glUniform4ui", (void*)glUniform4ui, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLUNIFORM4UIPROC glad_debug_glUniform4ui = glad_debug_impl_glUniform4ui;
PFNGLUNIFORM4UIVPROC glad_glUniform4uiv;
void APIENTRY glad_debug_impl_glUniform4uiv(GLint arg0, GLsizei arg1, const GLuint * arg2) {    
    _pre_call_callback("glUniform4uiv", (void*)glUniform4uiv, 3, arg0, arg1, arg2);
     glad_glUniform4uiv(arg0, arg1, arg2);
    _post_call_callback("glUniform4uiv", (void*)glUniform4uiv, 3, arg0, arg1, arg2);
    
}
PFNGLUNIFORM4UIVPROC glad_debug_glUniform4uiv = glad_debug_impl_glUniform4uiv;
PFNGLUNIFORMBLOCKBINDINGPROC glad_glUniformBlockBinding;
void APIENTRY glad_debug_impl_glUniformBlockBinding(GLuint arg0, GLuint arg1, GLuint arg2) {    
    _pre_call_callback("glUniformBlockBinding", (void*)glUniformBlockBinding, 3, arg0, arg1, arg2);
     glad_glUniformBlockBinding(arg0, arg1, arg2);
    _post_call_callback("glUniformBlockBinding", (void*)glUniformBlockBinding, 3, arg0, arg1, arg2);
    
}
PFNGLUNIFORMBLOCKBINDINGPROC glad_debug_glUniformBlockBinding = glad_debug_impl_glUniformBlockBinding;
PFNGLUNIFORMMATRIX2DVPROC glad_glUniformMatrix2dv;
void APIENTRY glad_debug_impl_glUniformMatrix2dv(GLint arg0, GLsizei arg1, GLboolean arg2, const GLdouble * arg3) {    
    _pre_call_callback("glUniformMatrix2dv", (void*)glUniformMatrix2dv, 4, arg0, arg1, arg2, arg3);
     glad_glUniformMatrix2dv(arg0, arg1, arg2, arg3);
    _post_call_callback("glUniformMatrix2dv", (void*)glUniformMatrix2dv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLUNIFORMMATRIX2DVPROC glad_debug_glUniformMatrix2dv = glad_debug_impl_glUniformMatrix2dv;
PFNGLUNIFORMMATRIX2FVPROC glad_glUniformMatrix2fv;
void APIENTRY glad_debug_impl_glUniformMatrix2fv(GLint arg0, GLsizei arg1, GLboolean arg2, const GLfloat * arg3) {    
    _pre_call_callback("glUniformMatrix2fv", (void*)glUniformMatrix2fv, 4, arg0, arg1, arg2, arg3);
     glad_glUniformMatrix2fv(arg0, arg1, arg2, arg3);
    _post_call_callback("glUniformMatrix2fv", (void*)glUniformMatrix2fv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLUNIFORMMATRIX2FVPROC glad_debug_glUniformMatrix2fv = glad_debug_impl_glUniformMatrix2fv;
PFNGLUNIFORMMATRIX2X3DVPROC glad_glUniformMatrix2x3dv;
void APIENTRY glad_debug_impl_glUniformMatrix2x3dv(GLint arg0, GLsizei arg1, GLboolean arg2, const GLdouble * arg3) {    
    _pre_call_callback("glUniformMatrix2x3dv", (void*)glUniformMatrix2x3dv, 4, arg0, arg1, arg2, arg3);
     glad_glUniformMatrix2x3dv(arg0, arg1, arg2, arg3);
    _post_call_callback("glUniformMatrix2x3dv", (void*)glUniformMatrix2x3dv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLUNIFORMMATRIX2X3DVPROC glad_debug_glUniformMatrix2x3dv = glad_debug_impl_glUniformMatrix2x3dv;
PFNGLUNIFORMMATRIX2X3FVPROC glad_glUniformMatrix2x3fv;
void APIENTRY glad_debug_impl_glUniformMatrix2x3fv(GLint arg0, GLsizei arg1, GLboolean arg2, const GLfloat * arg3) {    
    _pre_call_callback("glUniformMatrix2x3fv", (void*)glUniformMatrix2x3fv, 4, arg0, arg1, arg2, arg3);
     glad_glUniformMatrix2x3fv(arg0, arg1, arg2, arg3);
    _post_call_callback("glUniformMatrix2x3fv", (void*)glUniformMatrix2x3fv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLUNIFORMMATRIX2X3FVPROC glad_debug_glUniformMatrix2x3fv = glad_debug_impl_glUniformMatrix2x3fv;
PFNGLUNIFORMMATRIX2X4DVPROC glad_glUniformMatrix2x4dv;
void APIENTRY glad_debug_impl_glUniformMatrix2x4dv(GLint arg0, GLsizei arg1, GLboolean arg2, const GLdouble * arg3) {    
    _pre_call_callback("glUniformMatrix2x4dv", (void*)glUniformMatrix2x4dv, 4, arg0, arg1, arg2, arg3);
     glad_glUniformMatrix2x4dv(arg0, arg1, arg2, arg3);
    _post_call_callback("glUniformMatrix2x4dv", (void*)glUniformMatrix2x4dv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLUNIFORMMATRIX2X4DVPROC glad_debug_glUniformMatrix2x4dv = glad_debug_impl_glUniformMatrix2x4dv;
PFNGLUNIFORMMATRIX2X4FVPROC glad_glUniformMatrix2x4fv;
void APIENTRY glad_debug_impl_glUniformMatrix2x4fv(GLint arg0, GLsizei arg1, GLboolean arg2, const GLfloat * arg3) {    
    _pre_call_callback("glUniformMatrix2x4fv", (void*)glUniformMatrix2x4fv, 4, arg0, arg1, arg2, arg3);
     glad_glUniformMatrix2x4fv(arg0, arg1, arg2, arg3);
    _post_call_callback("glUniformMatrix2x4fv", (void*)glUniformMatrix2x4fv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLUNIFORMMATRIX2X4FVPROC glad_debug_glUniformMatrix2x4fv = glad_debug_impl_glUniformMatrix2x4fv;
PFNGLUNIFORMMATRIX3DVPROC glad_glUniformMatrix3dv;
void APIENTRY glad_debug_impl_glUniformMatrix3dv(GLint arg0, GLsizei arg1, GLboolean arg2, const GLdouble * arg3) {    
    _pre_call_callback("glUniformMatrix3dv", (void*)glUniformMatrix3dv, 4, arg0, arg1, arg2, arg3);
     glad_glUniformMatrix3dv(arg0, arg1, arg2, arg3);
    _post_call_callback("glUniformMatrix3dv", (void*)glUniformMatrix3dv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLUNIFORMMATRIX3DVPROC glad_debug_glUniformMatrix3dv = glad_debug_impl_glUniformMatrix3dv;
PFNGLUNIFORMMATRIX3FVPROC glad_glUniformMatrix3fv;
void APIENTRY glad_debug_impl_glUniformMatrix3fv(GLint arg0, GLsizei arg1, GLboolean arg2, const GLfloat * arg3) {    
    _pre_call_callback("glUniformMatrix3fv", (void*)glUniformMatrix3fv, 4, arg0, arg1, arg2, arg3);
     glad_glUniformMatrix3fv(arg0, arg1, arg2, arg3);
    _post_call_callback("glUniformMatrix3fv", (void*)glUniformMatrix3fv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLUNIFORMMATRIX3FVPROC glad_debug_glUniformMatrix3fv = glad_debug_impl_glUniformMatrix3fv;
PFNGLUNIFORMMATRIX3X2DVPROC glad_glUniformMatrix3x2dv;
void APIENTRY glad_debug_impl_glUniformMatrix3x2dv(GLint arg0, GLsizei arg1, GLboolean arg2, const GLdouble * arg3) {    
    _pre_call_callback("glUniformMatrix3x2dv", (void*)glUniformMatrix3x2dv, 4, arg0, arg1, arg2, arg3);
     glad_glUniformMatrix3x2dv(arg0, arg1, arg2, arg3);
    _post_call_callback("glUniformMatrix3x2dv", (void*)glUniformMatrix3x2dv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLUNIFORMMATRIX3X2DVPROC glad_debug_glUniformMatrix3x2dv = glad_debug_impl_glUniformMatrix3x2dv;
PFNGLUNIFORMMATRIX3X2FVPROC glad_glUniformMatrix3x2fv;
void APIENTRY glad_debug_impl_glUniformMatrix3x2fv(GLint arg0, GLsizei arg1, GLboolean arg2, const GLfloat * arg3) {    
    _pre_call_callback("glUniformMatrix3x2fv", (void*)glUniformMatrix3x2fv, 4, arg0, arg1, arg2, arg3);
     glad_glUniformMatrix3x2fv(arg0, arg1, arg2, arg3);
    _post_call_callback("glUniformMatrix3x2fv", (void*)glUniformMatrix3x2fv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLUNIFORMMATRIX3X2FVPROC glad_debug_glUniformMatrix3x2fv = glad_debug_impl_glUniformMatrix3x2fv;
PFNGLUNIFORMMATRIX3X4DVPROC glad_glUniformMatrix3x4dv;
void APIENTRY glad_debug_impl_glUniformMatrix3x4dv(GLint arg0, GLsizei arg1, GLboolean arg2, const GLdouble * arg3) {    
    _pre_call_callback("glUniformMatrix3x4dv", (void*)glUniformMatrix3x4dv, 4, arg0, arg1, arg2, arg3);
     glad_glUniformMatrix3x4dv(arg0, arg1, arg2, arg3);
    _post_call_callback("glUniformMatrix3x4dv", (void*)glUniformMatrix3x4dv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLUNIFORMMATRIX3X4DVPROC glad_debug_glUniformMatrix3x4dv = glad_debug_impl_glUniformMatrix3x4dv;
PFNGLUNIFORMMATRIX3X4FVPROC glad_glUniformMatrix3x4fv;
void APIENTRY glad_debug_impl_glUniformMatrix3x4fv(GLint arg0, GLsizei arg1, GLboolean arg2, const GLfloat * arg3) {    
    _pre_call_callback("glUniformMatrix3x4fv", (void*)glUniformMatrix3x4fv, 4, arg0, arg1, arg2, arg3);
     glad_glUniformMatrix3x4fv(arg0, arg1, arg2, arg3);
    _post_call_callback("glUniformMatrix3x4fv", (void*)glUniformMatrix3x4fv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLUNIFORMMATRIX3X4FVPROC glad_debug_glUniformMatrix3x4fv = glad_debug_impl_glUniformMatrix3x4fv;
PFNGLUNIFORMMATRIX4DVPROC glad_glUniformMatrix4dv;
void APIENTRY glad_debug_impl_glUniformMatrix4dv(GLint arg0, GLsizei arg1, GLboolean arg2, const GLdouble * arg3) {    
    _pre_call_callback("glUniformMatrix4dv", (void*)glUniformMatrix4dv, 4, arg0, arg1, arg2, arg3);
     glad_glUniformMatrix4dv(arg0, arg1, arg2, arg3);
    _post_call_callback("glUniformMatrix4dv", (void*)glUniformMatrix4dv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLUNIFORMMATRIX4DVPROC glad_debug_glUniformMatrix4dv = glad_debug_impl_glUniformMatrix4dv;
PFNGLUNIFORMMATRIX4FVPROC glad_glUniformMatrix4fv;
void APIENTRY glad_debug_impl_glUniformMatrix4fv(GLint arg0, GLsizei arg1, GLboolean arg2, const GLfloat * arg3) {    
    _pre_call_callback("glUniformMatrix4fv", (void*)glUniformMatrix4fv, 4, arg0, arg1, arg2, arg3);
     glad_glUniformMatrix4fv(arg0, arg1, arg2, arg3);
    _post_call_callback("glUniformMatrix4fv", (void*)glUniformMatrix4fv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLUNIFORMMATRIX4FVPROC glad_debug_glUniformMatrix4fv = glad_debug_impl_glUniformMatrix4fv;
PFNGLUNIFORMMATRIX4X2DVPROC glad_glUniformMatrix4x2dv;
void APIENTRY glad_debug_impl_glUniformMatrix4x2dv(GLint arg0, GLsizei arg1, GLboolean arg2, const GLdouble * arg3) {    
    _pre_call_callback("glUniformMatrix4x2dv", (void*)glUniformMatrix4x2dv, 4, arg0, arg1, arg2, arg3);
     glad_glUniformMatrix4x2dv(arg0, arg1, arg2, arg3);
    _post_call_callback("glUniformMatrix4x2dv", (void*)glUniformMatrix4x2dv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLUNIFORMMATRIX4X2DVPROC glad_debug_glUniformMatrix4x2dv = glad_debug_impl_glUniformMatrix4x2dv;
PFNGLUNIFORMMATRIX4X2FVPROC glad_glUniformMatrix4x2fv;
void APIENTRY glad_debug_impl_glUniformMatrix4x2fv(GLint arg0, GLsizei arg1, GLboolean arg2, const GLfloat * arg3) {    
    _pre_call_callback("glUniformMatrix4x2fv", (void*)glUniformMatrix4x2fv, 4, arg0, arg1, arg2, arg3);
     glad_glUniformMatrix4x2fv(arg0, arg1, arg2, arg3);
    _post_call_callback("glUniformMatrix4x2fv", (void*)glUniformMatrix4x2fv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLUNIFORMMATRIX4X2FVPROC glad_debug_glUniformMatrix4x2fv = glad_debug_impl_glUniformMatrix4x2fv;
PFNGLUNIFORMMATRIX4X3DVPROC glad_glUniformMatrix4x3dv;
void APIENTRY glad_debug_impl_glUniformMatrix4x3dv(GLint arg0, GLsizei arg1, GLboolean arg2, const GLdouble * arg3) {    
    _pre_call_callback("glUniformMatrix4x3dv", (void*)glUniformMatrix4x3dv, 4, arg0, arg1, arg2, arg3);
     glad_glUniformMatrix4x3dv(arg0, arg1, arg2, arg3);
    _post_call_callback("glUniformMatrix4x3dv", (void*)glUniformMatrix4x3dv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLUNIFORMMATRIX4X3DVPROC glad_debug_glUniformMatrix4x3dv = glad_debug_impl_glUniformMatrix4x3dv;
PFNGLUNIFORMMATRIX4X3FVPROC glad_glUniformMatrix4x3fv;
void APIENTRY glad_debug_impl_glUniformMatrix4x3fv(GLint arg0, GLsizei arg1, GLboolean arg2, const GLfloat * arg3) {    
    _pre_call_callback("glUniformMatrix4x3fv", (void*)glUniformMatrix4x3fv, 4, arg0, arg1, arg2, arg3);
     glad_glUniformMatrix4x3fv(arg0, arg1, arg2, arg3);
    _post_call_callback("glUniformMatrix4x3fv", (void*)glUniformMatrix4x3fv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLUNIFORMMATRIX4X3FVPROC glad_debug_glUniformMatrix4x3fv = glad_debug_impl_glUniformMatrix4x3fv;
PFNGLUNIFORMSUBROUTINESUIVPROC glad_glUniformSubroutinesuiv;
void APIENTRY glad_debug_impl_glUniformSubroutinesuiv(GLenum arg0, GLsizei arg1, const GLuint * arg2) {    
    _pre_call_callback("glUniformSubroutinesuiv", (void*)glUniformSubroutinesuiv, 3, arg0, arg1, arg2);
     glad_glUniformSubroutinesuiv(arg0, arg1, arg2);
    _post_call_callback("glUniformSubroutinesuiv", (void*)glUniformSubroutinesuiv, 3, arg0, arg1, arg2);
    
}
PFNGLUNIFORMSUBROUTINESUIVPROC glad_debug_glUniformSubroutinesuiv = glad_debug_impl_glUniformSubroutinesuiv;
PFNGLUNMAPBUFFERPROC glad_glUnmapBuffer;
GLboolean APIENTRY glad_debug_impl_glUnmapBuffer(GLenum arg0) {    
    GLboolean ret;
    _pre_call_callback("glUnmapBuffer", (void*)glUnmapBuffer, 1, arg0);
    ret =  glad_glUnmapBuffer(arg0);
    _post_call_callback("glUnmapBuffer", (void*)glUnmapBuffer, 1, arg0);
    return ret;
}
PFNGLUNMAPBUFFERPROC glad_debug_glUnmapBuffer = glad_debug_impl_glUnmapBuffer;
PFNGLUNMAPNAMEDBUFFERPROC glad_glUnmapNamedBuffer;
GLboolean APIENTRY glad_debug_impl_glUnmapNamedBuffer(GLuint arg0) {    
    GLboolean ret;
    _pre_call_callback("glUnmapNamedBuffer", (void*)glUnmapNamedBuffer, 1, arg0);
    ret =  glad_glUnmapNamedBuffer(arg0);
    _post_call_callback("glUnmapNamedBuffer", (void*)glUnmapNamedBuffer, 1, arg0);
    return ret;
}
PFNGLUNMAPNAMEDBUFFERPROC glad_debug_glUnmapNamedBuffer = glad_debug_impl_glUnmapNamedBuffer;
PFNGLUSEPROGRAMPROC glad_glUseProgram;
void APIENTRY glad_debug_impl_glUseProgram(GLuint arg0) {    
    _pre_call_callback("glUseProgram", (void*)glUseProgram, 1, arg0);
     glad_glUseProgram(arg0);
    _post_call_callback("glUseProgram", (void*)glUseProgram, 1, arg0);
    
}
PFNGLUSEPROGRAMPROC glad_debug_glUseProgram = glad_debug_impl_glUseProgram;
PFNGLUSEPROGRAMSTAGESPROC glad_glUseProgramStages;
void APIENTRY glad_debug_impl_glUseProgramStages(GLuint arg0, GLbitfield arg1, GLuint arg2) {    
    _pre_call_callback("glUseProgramStages", (void*)glUseProgramStages, 3, arg0, arg1, arg2);
     glad_glUseProgramStages(arg0, arg1, arg2);
    _post_call_callback("glUseProgramStages", (void*)glUseProgramStages, 3, arg0, arg1, arg2);
    
}
PFNGLUSEPROGRAMSTAGESPROC glad_debug_glUseProgramStages = glad_debug_impl_glUseProgramStages;
PFNGLVALIDATEPROGRAMPROC glad_glValidateProgram;
void APIENTRY glad_debug_impl_glValidateProgram(GLuint arg0) {    
    _pre_call_callback("glValidateProgram", (void*)glValidateProgram, 1, arg0);
     glad_glValidateProgram(arg0);
    _post_call_callback("glValidateProgram", (void*)glValidateProgram, 1, arg0);
    
}
PFNGLVALIDATEPROGRAMPROC glad_debug_glValidateProgram = glad_debug_impl_glValidateProgram;
PFNGLVALIDATEPROGRAMPIPELINEPROC glad_glValidateProgramPipeline;
void APIENTRY glad_debug_impl_glValidateProgramPipeline(GLuint arg0) {    
    _pre_call_callback("glValidateProgramPipeline", (void*)glValidateProgramPipeline, 1, arg0);
     glad_glValidateProgramPipeline(arg0);
    _post_call_callback("glValidateProgramPipeline", (void*)glValidateProgramPipeline, 1, arg0);
    
}
PFNGLVALIDATEPROGRAMPIPELINEPROC glad_debug_glValidateProgramPipeline = glad_debug_impl_glValidateProgramPipeline;
PFNGLVERTEX2DPROC glad_glVertex2d;
void APIENTRY glad_debug_impl_glVertex2d(GLdouble arg0, GLdouble arg1) {    
    _pre_call_callback("glVertex2d", (void*)glVertex2d, 2, arg0, arg1);
     glad_glVertex2d(arg0, arg1);
    _post_call_callback("glVertex2d", (void*)glVertex2d, 2, arg0, arg1);
    
}
PFNGLVERTEX2DPROC glad_debug_glVertex2d = glad_debug_impl_glVertex2d;
PFNGLVERTEX2DVPROC glad_glVertex2dv;
void APIENTRY glad_debug_impl_glVertex2dv(const GLdouble * arg0) {    
    _pre_call_callback("glVertex2dv", (void*)glVertex2dv, 1, arg0);
     glad_glVertex2dv(arg0);
    _post_call_callback("glVertex2dv", (void*)glVertex2dv, 1, arg0);
    
}
PFNGLVERTEX2DVPROC glad_debug_glVertex2dv = glad_debug_impl_glVertex2dv;
PFNGLVERTEX2FPROC glad_glVertex2f;
void APIENTRY glad_debug_impl_glVertex2f(GLfloat arg0, GLfloat arg1) {    
    _pre_call_callback("glVertex2f", (void*)glVertex2f, 2, arg0, arg1);
     glad_glVertex2f(arg0, arg1);
    _post_call_callback("glVertex2f", (void*)glVertex2f, 2, arg0, arg1);
    
}
PFNGLVERTEX2FPROC glad_debug_glVertex2f = glad_debug_impl_glVertex2f;
PFNGLVERTEX2FVPROC glad_glVertex2fv;
void APIENTRY glad_debug_impl_glVertex2fv(const GLfloat * arg0) {    
    _pre_call_callback("glVertex2fv", (void*)glVertex2fv, 1, arg0);
     glad_glVertex2fv(arg0);
    _post_call_callback("glVertex2fv", (void*)glVertex2fv, 1, arg0);
    
}
PFNGLVERTEX2FVPROC glad_debug_glVertex2fv = glad_debug_impl_glVertex2fv;
PFNGLVERTEX2IPROC glad_glVertex2i;
void APIENTRY glad_debug_impl_glVertex2i(GLint arg0, GLint arg1) {    
    _pre_call_callback("glVertex2i", (void*)glVertex2i, 2, arg0, arg1);
     glad_glVertex2i(arg0, arg1);
    _post_call_callback("glVertex2i", (void*)glVertex2i, 2, arg0, arg1);
    
}
PFNGLVERTEX2IPROC glad_debug_glVertex2i = glad_debug_impl_glVertex2i;
PFNGLVERTEX2IVPROC glad_glVertex2iv;
void APIENTRY glad_debug_impl_glVertex2iv(const GLint * arg0) {    
    _pre_call_callback("glVertex2iv", (void*)glVertex2iv, 1, arg0);
     glad_glVertex2iv(arg0);
    _post_call_callback("glVertex2iv", (void*)glVertex2iv, 1, arg0);
    
}
PFNGLVERTEX2IVPROC glad_debug_glVertex2iv = glad_debug_impl_glVertex2iv;
PFNGLVERTEX2SPROC glad_glVertex2s;
void APIENTRY glad_debug_impl_glVertex2s(GLshort arg0, GLshort arg1) {    
    _pre_call_callback("glVertex2s", (void*)glVertex2s, 2, arg0, arg1);
     glad_glVertex2s(arg0, arg1);
    _post_call_callback("glVertex2s", (void*)glVertex2s, 2, arg0, arg1);
    
}
PFNGLVERTEX2SPROC glad_debug_glVertex2s = glad_debug_impl_glVertex2s;
PFNGLVERTEX2SVPROC glad_glVertex2sv;
void APIENTRY glad_debug_impl_glVertex2sv(const GLshort * arg0) {    
    _pre_call_callback("glVertex2sv", (void*)glVertex2sv, 1, arg0);
     glad_glVertex2sv(arg0);
    _post_call_callback("glVertex2sv", (void*)glVertex2sv, 1, arg0);
    
}
PFNGLVERTEX2SVPROC glad_debug_glVertex2sv = glad_debug_impl_glVertex2sv;
PFNGLVERTEX3DPROC glad_glVertex3d;
void APIENTRY glad_debug_impl_glVertex3d(GLdouble arg0, GLdouble arg1, GLdouble arg2) {    
    _pre_call_callback("glVertex3d", (void*)glVertex3d, 3, arg0, arg1, arg2);
     glad_glVertex3d(arg0, arg1, arg2);
    _post_call_callback("glVertex3d", (void*)glVertex3d, 3, arg0, arg1, arg2);
    
}
PFNGLVERTEX3DPROC glad_debug_glVertex3d = glad_debug_impl_glVertex3d;
PFNGLVERTEX3DVPROC glad_glVertex3dv;
void APIENTRY glad_debug_impl_glVertex3dv(const GLdouble * arg0) {    
    _pre_call_callback("glVertex3dv", (void*)glVertex3dv, 1, arg0);
     glad_glVertex3dv(arg0);
    _post_call_callback("glVertex3dv", (void*)glVertex3dv, 1, arg0);
    
}
PFNGLVERTEX3DVPROC glad_debug_glVertex3dv = glad_debug_impl_glVertex3dv;
PFNGLVERTEX3FPROC glad_glVertex3f;
void APIENTRY glad_debug_impl_glVertex3f(GLfloat arg0, GLfloat arg1, GLfloat arg2) {    
    _pre_call_callback("glVertex3f", (void*)glVertex3f, 3, arg0, arg1, arg2);
     glad_glVertex3f(arg0, arg1, arg2);
    _post_call_callback("glVertex3f", (void*)glVertex3f, 3, arg0, arg1, arg2);
    
}
PFNGLVERTEX3FPROC glad_debug_glVertex3f = glad_debug_impl_glVertex3f;
PFNGLVERTEX3FVPROC glad_glVertex3fv;
void APIENTRY glad_debug_impl_glVertex3fv(const GLfloat * arg0) {    
    _pre_call_callback("glVertex3fv", (void*)glVertex3fv, 1, arg0);
     glad_glVertex3fv(arg0);
    _post_call_callback("glVertex3fv", (void*)glVertex3fv, 1, arg0);
    
}
PFNGLVERTEX3FVPROC glad_debug_glVertex3fv = glad_debug_impl_glVertex3fv;
PFNGLVERTEX3IPROC glad_glVertex3i;
void APIENTRY glad_debug_impl_glVertex3i(GLint arg0, GLint arg1, GLint arg2) {    
    _pre_call_callback("glVertex3i", (void*)glVertex3i, 3, arg0, arg1, arg2);
     glad_glVertex3i(arg0, arg1, arg2);
    _post_call_callback("glVertex3i", (void*)glVertex3i, 3, arg0, arg1, arg2);
    
}
PFNGLVERTEX3IPROC glad_debug_glVertex3i = glad_debug_impl_glVertex3i;
PFNGLVERTEX3IVPROC glad_glVertex3iv;
void APIENTRY glad_debug_impl_glVertex3iv(const GLint * arg0) {    
    _pre_call_callback("glVertex3iv", (void*)glVertex3iv, 1, arg0);
     glad_glVertex3iv(arg0);
    _post_call_callback("glVertex3iv", (void*)glVertex3iv, 1, arg0);
    
}
PFNGLVERTEX3IVPROC glad_debug_glVertex3iv = glad_debug_impl_glVertex3iv;
PFNGLVERTEX3SPROC glad_glVertex3s;
void APIENTRY glad_debug_impl_glVertex3s(GLshort arg0, GLshort arg1, GLshort arg2) {    
    _pre_call_callback("glVertex3s", (void*)glVertex3s, 3, arg0, arg1, arg2);
     glad_glVertex3s(arg0, arg1, arg2);
    _post_call_callback("glVertex3s", (void*)glVertex3s, 3, arg0, arg1, arg2);
    
}
PFNGLVERTEX3SPROC glad_debug_glVertex3s = glad_debug_impl_glVertex3s;
PFNGLVERTEX3SVPROC glad_glVertex3sv;
void APIENTRY glad_debug_impl_glVertex3sv(const GLshort * arg0) {    
    _pre_call_callback("glVertex3sv", (void*)glVertex3sv, 1, arg0);
     glad_glVertex3sv(arg0);
    _post_call_callback("glVertex3sv", (void*)glVertex3sv, 1, arg0);
    
}
PFNGLVERTEX3SVPROC glad_debug_glVertex3sv = glad_debug_impl_glVertex3sv;
PFNGLVERTEX4DPROC glad_glVertex4d;
void APIENTRY glad_debug_impl_glVertex4d(GLdouble arg0, GLdouble arg1, GLdouble arg2, GLdouble arg3) {    
    _pre_call_callback("glVertex4d", (void*)glVertex4d, 4, arg0, arg1, arg2, arg3);
     glad_glVertex4d(arg0, arg1, arg2, arg3);
    _post_call_callback("glVertex4d", (void*)glVertex4d, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLVERTEX4DPROC glad_debug_glVertex4d = glad_debug_impl_glVertex4d;
PFNGLVERTEX4DVPROC glad_glVertex4dv;
void APIENTRY glad_debug_impl_glVertex4dv(const GLdouble * arg0) {    
    _pre_call_callback("glVertex4dv", (void*)glVertex4dv, 1, arg0);
     glad_glVertex4dv(arg0);
    _post_call_callback("glVertex4dv", (void*)glVertex4dv, 1, arg0);
    
}
PFNGLVERTEX4DVPROC glad_debug_glVertex4dv = glad_debug_impl_glVertex4dv;
PFNGLVERTEX4FPROC glad_glVertex4f;
void APIENTRY glad_debug_impl_glVertex4f(GLfloat arg0, GLfloat arg1, GLfloat arg2, GLfloat arg3) {    
    _pre_call_callback("glVertex4f", (void*)glVertex4f, 4, arg0, arg1, arg2, arg3);
     glad_glVertex4f(arg0, arg1, arg2, arg3);
    _post_call_callback("glVertex4f", (void*)glVertex4f, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLVERTEX4FPROC glad_debug_glVertex4f = glad_debug_impl_glVertex4f;
PFNGLVERTEX4FVPROC glad_glVertex4fv;
void APIENTRY glad_debug_impl_glVertex4fv(const GLfloat * arg0) {    
    _pre_call_callback("glVertex4fv", (void*)glVertex4fv, 1, arg0);
     glad_glVertex4fv(arg0);
    _post_call_callback("glVertex4fv", (void*)glVertex4fv, 1, arg0);
    
}
PFNGLVERTEX4FVPROC glad_debug_glVertex4fv = glad_debug_impl_glVertex4fv;
PFNGLVERTEX4IPROC glad_glVertex4i;
void APIENTRY glad_debug_impl_glVertex4i(GLint arg0, GLint arg1, GLint arg2, GLint arg3) {    
    _pre_call_callback("glVertex4i", (void*)glVertex4i, 4, arg0, arg1, arg2, arg3);
     glad_glVertex4i(arg0, arg1, arg2, arg3);
    _post_call_callback("glVertex4i", (void*)glVertex4i, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLVERTEX4IPROC glad_debug_glVertex4i = glad_debug_impl_glVertex4i;
PFNGLVERTEX4IVPROC glad_glVertex4iv;
void APIENTRY glad_debug_impl_glVertex4iv(const GLint * arg0) {    
    _pre_call_callback("glVertex4iv", (void*)glVertex4iv, 1, arg0);
     glad_glVertex4iv(arg0);
    _post_call_callback("glVertex4iv", (void*)glVertex4iv, 1, arg0);
    
}
PFNGLVERTEX4IVPROC glad_debug_glVertex4iv = glad_debug_impl_glVertex4iv;
PFNGLVERTEX4SPROC glad_glVertex4s;
void APIENTRY glad_debug_impl_glVertex4s(GLshort arg0, GLshort arg1, GLshort arg2, GLshort arg3) {    
    _pre_call_callback("glVertex4s", (void*)glVertex4s, 4, arg0, arg1, arg2, arg3);
     glad_glVertex4s(arg0, arg1, arg2, arg3);
    _post_call_callback("glVertex4s", (void*)glVertex4s, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLVERTEX4SPROC glad_debug_glVertex4s = glad_debug_impl_glVertex4s;
PFNGLVERTEX4SVPROC glad_glVertex4sv;
void APIENTRY glad_debug_impl_glVertex4sv(const GLshort * arg0) {    
    _pre_call_callback("glVertex4sv", (void*)glVertex4sv, 1, arg0);
     glad_glVertex4sv(arg0);
    _post_call_callback("glVertex4sv", (void*)glVertex4sv, 1, arg0);
    
}
PFNGLVERTEX4SVPROC glad_debug_glVertex4sv = glad_debug_impl_glVertex4sv;
PFNGLVERTEXARRAYATTRIBBINDINGPROC glad_glVertexArrayAttribBinding;
void APIENTRY glad_debug_impl_glVertexArrayAttribBinding(GLuint arg0, GLuint arg1, GLuint arg2) {    
    _pre_call_callback("glVertexArrayAttribBinding", (void*)glVertexArrayAttribBinding, 3, arg0, arg1, arg2);
     glad_glVertexArrayAttribBinding(arg0, arg1, arg2);
    _post_call_callback("glVertexArrayAttribBinding", (void*)glVertexArrayAttribBinding, 3, arg0, arg1, arg2);
    
}
PFNGLVERTEXARRAYATTRIBBINDINGPROC glad_debug_glVertexArrayAttribBinding = glad_debug_impl_glVertexArrayAttribBinding;
PFNGLVERTEXARRAYATTRIBFORMATPROC glad_glVertexArrayAttribFormat;
void APIENTRY glad_debug_impl_glVertexArrayAttribFormat(GLuint arg0, GLuint arg1, GLint arg2, GLenum arg3, GLboolean arg4, GLuint arg5) {    
    _pre_call_callback("glVertexArrayAttribFormat", (void*)glVertexArrayAttribFormat, 6, arg0, arg1, arg2, arg3, arg4, arg5);
     glad_glVertexArrayAttribFormat(arg0, arg1, arg2, arg3, arg4, arg5);
    _post_call_callback("glVertexArrayAttribFormat", (void*)glVertexArrayAttribFormat, 6, arg0, arg1, arg2, arg3, arg4, arg5);
    
}
PFNGLVERTEXARRAYATTRIBFORMATPROC glad_debug_glVertexArrayAttribFormat = glad_debug_impl_glVertexArrayAttribFormat;
PFNGLVERTEXARRAYATTRIBIFORMATPROC glad_glVertexArrayAttribIFormat;
void APIENTRY glad_debug_impl_glVertexArrayAttribIFormat(GLuint arg0, GLuint arg1, GLint arg2, GLenum arg3, GLuint arg4) {    
    _pre_call_callback("glVertexArrayAttribIFormat", (void*)glVertexArrayAttribIFormat, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glVertexArrayAttribIFormat(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glVertexArrayAttribIFormat", (void*)glVertexArrayAttribIFormat, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLVERTEXARRAYATTRIBIFORMATPROC glad_debug_glVertexArrayAttribIFormat = glad_debug_impl_glVertexArrayAttribIFormat;
PFNGLVERTEXARRAYATTRIBLFORMATPROC glad_glVertexArrayAttribLFormat;
void APIENTRY glad_debug_impl_glVertexArrayAttribLFormat(GLuint arg0, GLuint arg1, GLint arg2, GLenum arg3, GLuint arg4) {    
    _pre_call_callback("glVertexArrayAttribLFormat", (void*)glVertexArrayAttribLFormat, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glVertexArrayAttribLFormat(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glVertexArrayAttribLFormat", (void*)glVertexArrayAttribLFormat, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLVERTEXARRAYATTRIBLFORMATPROC glad_debug_glVertexArrayAttribLFormat = glad_debug_impl_glVertexArrayAttribLFormat;
PFNGLVERTEXARRAYBINDINGDIVISORPROC glad_glVertexArrayBindingDivisor;
void APIENTRY glad_debug_impl_glVertexArrayBindingDivisor(GLuint arg0, GLuint arg1, GLuint arg2) {    
    _pre_call_callback("glVertexArrayBindingDivisor", (void*)glVertexArrayBindingDivisor, 3, arg0, arg1, arg2);
     glad_glVertexArrayBindingDivisor(arg0, arg1, arg2);
    _post_call_callback("glVertexArrayBindingDivisor", (void*)glVertexArrayBindingDivisor, 3, arg0, arg1, arg2);
    
}
PFNGLVERTEXARRAYBINDINGDIVISORPROC glad_debug_glVertexArrayBindingDivisor = glad_debug_impl_glVertexArrayBindingDivisor;
PFNGLVERTEXARRAYELEMENTBUFFERPROC glad_glVertexArrayElementBuffer;
void APIENTRY glad_debug_impl_glVertexArrayElementBuffer(GLuint arg0, GLuint arg1) {    
    _pre_call_callback("glVertexArrayElementBuffer", (void*)glVertexArrayElementBuffer, 2, arg0, arg1);
     glad_glVertexArrayElementBuffer(arg0, arg1);
    _post_call_callback("glVertexArrayElementBuffer", (void*)glVertexArrayElementBuffer, 2, arg0, arg1);
    
}
PFNGLVERTEXARRAYELEMENTBUFFERPROC glad_debug_glVertexArrayElementBuffer = glad_debug_impl_glVertexArrayElementBuffer;
PFNGLVERTEXARRAYVERTEXBUFFERPROC glad_glVertexArrayVertexBuffer;
void APIENTRY glad_debug_impl_glVertexArrayVertexBuffer(GLuint arg0, GLuint arg1, GLuint arg2, GLintptr arg3, GLsizei arg4) {    
    _pre_call_callback("glVertexArrayVertexBuffer", (void*)glVertexArrayVertexBuffer, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glVertexArrayVertexBuffer(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glVertexArrayVertexBuffer", (void*)glVertexArrayVertexBuffer, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLVERTEXARRAYVERTEXBUFFERPROC glad_debug_glVertexArrayVertexBuffer = glad_debug_impl_glVertexArrayVertexBuffer;
PFNGLVERTEXARRAYVERTEXBUFFERSPROC glad_glVertexArrayVertexBuffers;
void APIENTRY glad_debug_impl_glVertexArrayVertexBuffers(GLuint arg0, GLuint arg1, GLsizei arg2, const GLuint * arg3, const GLintptr * arg4, const GLsizei * arg5) {    
    _pre_call_callback("glVertexArrayVertexBuffers", (void*)glVertexArrayVertexBuffers, 6, arg0, arg1, arg2, arg3, arg4, arg5);
     glad_glVertexArrayVertexBuffers(arg0, arg1, arg2, arg3, arg4, arg5);
    _post_call_callback("glVertexArrayVertexBuffers", (void*)glVertexArrayVertexBuffers, 6, arg0, arg1, arg2, arg3, arg4, arg5);
    
}
PFNGLVERTEXARRAYVERTEXBUFFERSPROC glad_debug_glVertexArrayVertexBuffers = glad_debug_impl_glVertexArrayVertexBuffers;
PFNGLVERTEXATTRIB1DPROC glad_glVertexAttrib1d;
void APIENTRY glad_debug_impl_glVertexAttrib1d(GLuint arg0, GLdouble arg1) {    
    _pre_call_callback("glVertexAttrib1d", (void*)glVertexAttrib1d, 2, arg0, arg1);
     glad_glVertexAttrib1d(arg0, arg1);
    _post_call_callback("glVertexAttrib1d", (void*)glVertexAttrib1d, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIB1DPROC glad_debug_glVertexAttrib1d = glad_debug_impl_glVertexAttrib1d;
PFNGLVERTEXATTRIB1DVPROC glad_glVertexAttrib1dv;
void APIENTRY glad_debug_impl_glVertexAttrib1dv(GLuint arg0, const GLdouble * arg1) {    
    _pre_call_callback("glVertexAttrib1dv", (void*)glVertexAttrib1dv, 2, arg0, arg1);
     glad_glVertexAttrib1dv(arg0, arg1);
    _post_call_callback("glVertexAttrib1dv", (void*)glVertexAttrib1dv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIB1DVPROC glad_debug_glVertexAttrib1dv = glad_debug_impl_glVertexAttrib1dv;
PFNGLVERTEXATTRIB1FPROC glad_glVertexAttrib1f;
void APIENTRY glad_debug_impl_glVertexAttrib1f(GLuint arg0, GLfloat arg1) {    
    _pre_call_callback("glVertexAttrib1f", (void*)glVertexAttrib1f, 2, arg0, arg1);
     glad_glVertexAttrib1f(arg0, arg1);
    _post_call_callback("glVertexAttrib1f", (void*)glVertexAttrib1f, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIB1FPROC glad_debug_glVertexAttrib1f = glad_debug_impl_glVertexAttrib1f;
PFNGLVERTEXATTRIB1FVPROC glad_glVertexAttrib1fv;
void APIENTRY glad_debug_impl_glVertexAttrib1fv(GLuint arg0, const GLfloat * arg1) {    
    _pre_call_callback("glVertexAttrib1fv", (void*)glVertexAttrib1fv, 2, arg0, arg1);
     glad_glVertexAttrib1fv(arg0, arg1);
    _post_call_callback("glVertexAttrib1fv", (void*)glVertexAttrib1fv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIB1FVPROC glad_debug_glVertexAttrib1fv = glad_debug_impl_glVertexAttrib1fv;
PFNGLVERTEXATTRIB1SPROC glad_glVertexAttrib1s;
void APIENTRY glad_debug_impl_glVertexAttrib1s(GLuint arg0, GLshort arg1) {    
    _pre_call_callback("glVertexAttrib1s", (void*)glVertexAttrib1s, 2, arg0, arg1);
     glad_glVertexAttrib1s(arg0, arg1);
    _post_call_callback("glVertexAttrib1s", (void*)glVertexAttrib1s, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIB1SPROC glad_debug_glVertexAttrib1s = glad_debug_impl_glVertexAttrib1s;
PFNGLVERTEXATTRIB1SVPROC glad_glVertexAttrib1sv;
void APIENTRY glad_debug_impl_glVertexAttrib1sv(GLuint arg0, const GLshort * arg1) {    
    _pre_call_callback("glVertexAttrib1sv", (void*)glVertexAttrib1sv, 2, arg0, arg1);
     glad_glVertexAttrib1sv(arg0, arg1);
    _post_call_callback("glVertexAttrib1sv", (void*)glVertexAttrib1sv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIB1SVPROC glad_debug_glVertexAttrib1sv = glad_debug_impl_glVertexAttrib1sv;
PFNGLVERTEXATTRIB2DPROC glad_glVertexAttrib2d;
void APIENTRY glad_debug_impl_glVertexAttrib2d(GLuint arg0, GLdouble arg1, GLdouble arg2) {    
    _pre_call_callback("glVertexAttrib2d", (void*)glVertexAttrib2d, 3, arg0, arg1, arg2);
     glad_glVertexAttrib2d(arg0, arg1, arg2);
    _post_call_callback("glVertexAttrib2d", (void*)glVertexAttrib2d, 3, arg0, arg1, arg2);
    
}
PFNGLVERTEXATTRIB2DPROC glad_debug_glVertexAttrib2d = glad_debug_impl_glVertexAttrib2d;
PFNGLVERTEXATTRIB2DVPROC glad_glVertexAttrib2dv;
void APIENTRY glad_debug_impl_glVertexAttrib2dv(GLuint arg0, const GLdouble * arg1) {    
    _pre_call_callback("glVertexAttrib2dv", (void*)glVertexAttrib2dv, 2, arg0, arg1);
     glad_glVertexAttrib2dv(arg0, arg1);
    _post_call_callback("glVertexAttrib2dv", (void*)glVertexAttrib2dv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIB2DVPROC glad_debug_glVertexAttrib2dv = glad_debug_impl_glVertexAttrib2dv;
PFNGLVERTEXATTRIB2FPROC glad_glVertexAttrib2f;
void APIENTRY glad_debug_impl_glVertexAttrib2f(GLuint arg0, GLfloat arg1, GLfloat arg2) {    
    _pre_call_callback("glVertexAttrib2f", (void*)glVertexAttrib2f, 3, arg0, arg1, arg2);
     glad_glVertexAttrib2f(arg0, arg1, arg2);
    _post_call_callback("glVertexAttrib2f", (void*)glVertexAttrib2f, 3, arg0, arg1, arg2);
    
}
PFNGLVERTEXATTRIB2FPROC glad_debug_glVertexAttrib2f = glad_debug_impl_glVertexAttrib2f;
PFNGLVERTEXATTRIB2FVPROC glad_glVertexAttrib2fv;
void APIENTRY glad_debug_impl_glVertexAttrib2fv(GLuint arg0, const GLfloat * arg1) {    
    _pre_call_callback("glVertexAttrib2fv", (void*)glVertexAttrib2fv, 2, arg0, arg1);
     glad_glVertexAttrib2fv(arg0, arg1);
    _post_call_callback("glVertexAttrib2fv", (void*)glVertexAttrib2fv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIB2FVPROC glad_debug_glVertexAttrib2fv = glad_debug_impl_glVertexAttrib2fv;
PFNGLVERTEXATTRIB2SPROC glad_glVertexAttrib2s;
void APIENTRY glad_debug_impl_glVertexAttrib2s(GLuint arg0, GLshort arg1, GLshort arg2) {    
    _pre_call_callback("glVertexAttrib2s", (void*)glVertexAttrib2s, 3, arg0, arg1, arg2);
     glad_glVertexAttrib2s(arg0, arg1, arg2);
    _post_call_callback("glVertexAttrib2s", (void*)glVertexAttrib2s, 3, arg0, arg1, arg2);
    
}
PFNGLVERTEXATTRIB2SPROC glad_debug_glVertexAttrib2s = glad_debug_impl_glVertexAttrib2s;
PFNGLVERTEXATTRIB2SVPROC glad_glVertexAttrib2sv;
void APIENTRY glad_debug_impl_glVertexAttrib2sv(GLuint arg0, const GLshort * arg1) {    
    _pre_call_callback("glVertexAttrib2sv", (void*)glVertexAttrib2sv, 2, arg0, arg1);
     glad_glVertexAttrib2sv(arg0, arg1);
    _post_call_callback("glVertexAttrib2sv", (void*)glVertexAttrib2sv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIB2SVPROC glad_debug_glVertexAttrib2sv = glad_debug_impl_glVertexAttrib2sv;
PFNGLVERTEXATTRIB3DPROC glad_glVertexAttrib3d;
void APIENTRY glad_debug_impl_glVertexAttrib3d(GLuint arg0, GLdouble arg1, GLdouble arg2, GLdouble arg3) {    
    _pre_call_callback("glVertexAttrib3d", (void*)glVertexAttrib3d, 4, arg0, arg1, arg2, arg3);
     glad_glVertexAttrib3d(arg0, arg1, arg2, arg3);
    _post_call_callback("glVertexAttrib3d", (void*)glVertexAttrib3d, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLVERTEXATTRIB3DPROC glad_debug_glVertexAttrib3d = glad_debug_impl_glVertexAttrib3d;
PFNGLVERTEXATTRIB3DVPROC glad_glVertexAttrib3dv;
void APIENTRY glad_debug_impl_glVertexAttrib3dv(GLuint arg0, const GLdouble * arg1) {    
    _pre_call_callback("glVertexAttrib3dv", (void*)glVertexAttrib3dv, 2, arg0, arg1);
     glad_glVertexAttrib3dv(arg0, arg1);
    _post_call_callback("glVertexAttrib3dv", (void*)glVertexAttrib3dv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIB3DVPROC glad_debug_glVertexAttrib3dv = glad_debug_impl_glVertexAttrib3dv;
PFNGLVERTEXATTRIB3FPROC glad_glVertexAttrib3f;
void APIENTRY glad_debug_impl_glVertexAttrib3f(GLuint arg0, GLfloat arg1, GLfloat arg2, GLfloat arg3) {    
    _pre_call_callback("glVertexAttrib3f", (void*)glVertexAttrib3f, 4, arg0, arg1, arg2, arg3);
     glad_glVertexAttrib3f(arg0, arg1, arg2, arg3);
    _post_call_callback("glVertexAttrib3f", (void*)glVertexAttrib3f, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLVERTEXATTRIB3FPROC glad_debug_glVertexAttrib3f = glad_debug_impl_glVertexAttrib3f;
PFNGLVERTEXATTRIB3FVPROC glad_glVertexAttrib3fv;
void APIENTRY glad_debug_impl_glVertexAttrib3fv(GLuint arg0, const GLfloat * arg1) {    
    _pre_call_callback("glVertexAttrib3fv", (void*)glVertexAttrib3fv, 2, arg0, arg1);
     glad_glVertexAttrib3fv(arg0, arg1);
    _post_call_callback("glVertexAttrib3fv", (void*)glVertexAttrib3fv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIB3FVPROC glad_debug_glVertexAttrib3fv = glad_debug_impl_glVertexAttrib3fv;
PFNGLVERTEXATTRIB3SPROC glad_glVertexAttrib3s;
void APIENTRY glad_debug_impl_glVertexAttrib3s(GLuint arg0, GLshort arg1, GLshort arg2, GLshort arg3) {    
    _pre_call_callback("glVertexAttrib3s", (void*)glVertexAttrib3s, 4, arg0, arg1, arg2, arg3);
     glad_glVertexAttrib3s(arg0, arg1, arg2, arg3);
    _post_call_callback("glVertexAttrib3s", (void*)glVertexAttrib3s, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLVERTEXATTRIB3SPROC glad_debug_glVertexAttrib3s = glad_debug_impl_glVertexAttrib3s;
PFNGLVERTEXATTRIB3SVPROC glad_glVertexAttrib3sv;
void APIENTRY glad_debug_impl_glVertexAttrib3sv(GLuint arg0, const GLshort * arg1) {    
    _pre_call_callback("glVertexAttrib3sv", (void*)glVertexAttrib3sv, 2, arg0, arg1);
     glad_glVertexAttrib3sv(arg0, arg1);
    _post_call_callback("glVertexAttrib3sv", (void*)glVertexAttrib3sv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIB3SVPROC glad_debug_glVertexAttrib3sv = glad_debug_impl_glVertexAttrib3sv;
PFNGLVERTEXATTRIB4NBVPROC glad_glVertexAttrib4Nbv;
void APIENTRY glad_debug_impl_glVertexAttrib4Nbv(GLuint arg0, const GLbyte * arg1) {    
    _pre_call_callback("glVertexAttrib4Nbv", (void*)glVertexAttrib4Nbv, 2, arg0, arg1);
     glad_glVertexAttrib4Nbv(arg0, arg1);
    _post_call_callback("glVertexAttrib4Nbv", (void*)glVertexAttrib4Nbv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIB4NBVPROC glad_debug_glVertexAttrib4Nbv = glad_debug_impl_glVertexAttrib4Nbv;
PFNGLVERTEXATTRIB4NIVPROC glad_glVertexAttrib4Niv;
void APIENTRY glad_debug_impl_glVertexAttrib4Niv(GLuint arg0, const GLint * arg1) {    
    _pre_call_callback("glVertexAttrib4Niv", (void*)glVertexAttrib4Niv, 2, arg0, arg1);
     glad_glVertexAttrib4Niv(arg0, arg1);
    _post_call_callback("glVertexAttrib4Niv", (void*)glVertexAttrib4Niv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIB4NIVPROC glad_debug_glVertexAttrib4Niv = glad_debug_impl_glVertexAttrib4Niv;
PFNGLVERTEXATTRIB4NSVPROC glad_glVertexAttrib4Nsv;
void APIENTRY glad_debug_impl_glVertexAttrib4Nsv(GLuint arg0, const GLshort * arg1) {    
    _pre_call_callback("glVertexAttrib4Nsv", (void*)glVertexAttrib4Nsv, 2, arg0, arg1);
     glad_glVertexAttrib4Nsv(arg0, arg1);
    _post_call_callback("glVertexAttrib4Nsv", (void*)glVertexAttrib4Nsv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIB4NSVPROC glad_debug_glVertexAttrib4Nsv = glad_debug_impl_glVertexAttrib4Nsv;
PFNGLVERTEXATTRIB4NUBPROC glad_glVertexAttrib4Nub;
void APIENTRY glad_debug_impl_glVertexAttrib4Nub(GLuint arg0, GLubyte arg1, GLubyte arg2, GLubyte arg3, GLubyte arg4) {    
    _pre_call_callback("glVertexAttrib4Nub", (void*)glVertexAttrib4Nub, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glVertexAttrib4Nub(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glVertexAttrib4Nub", (void*)glVertexAttrib4Nub, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLVERTEXATTRIB4NUBPROC glad_debug_glVertexAttrib4Nub = glad_debug_impl_glVertexAttrib4Nub;
PFNGLVERTEXATTRIB4NUBVPROC glad_glVertexAttrib4Nubv;
void APIENTRY glad_debug_impl_glVertexAttrib4Nubv(GLuint arg0, const GLubyte * arg1) {    
    _pre_call_callback("glVertexAttrib4Nubv", (void*)glVertexAttrib4Nubv, 2, arg0, arg1);
     glad_glVertexAttrib4Nubv(arg0, arg1);
    _post_call_callback("glVertexAttrib4Nubv", (void*)glVertexAttrib4Nubv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIB4NUBVPROC glad_debug_glVertexAttrib4Nubv = glad_debug_impl_glVertexAttrib4Nubv;
PFNGLVERTEXATTRIB4NUIVPROC glad_glVertexAttrib4Nuiv;
void APIENTRY glad_debug_impl_glVertexAttrib4Nuiv(GLuint arg0, const GLuint * arg1) {    
    _pre_call_callback("glVertexAttrib4Nuiv", (void*)glVertexAttrib4Nuiv, 2, arg0, arg1);
     glad_glVertexAttrib4Nuiv(arg0, arg1);
    _post_call_callback("glVertexAttrib4Nuiv", (void*)glVertexAttrib4Nuiv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIB4NUIVPROC glad_debug_glVertexAttrib4Nuiv = glad_debug_impl_glVertexAttrib4Nuiv;
PFNGLVERTEXATTRIB4NUSVPROC glad_glVertexAttrib4Nusv;
void APIENTRY glad_debug_impl_glVertexAttrib4Nusv(GLuint arg0, const GLushort * arg1) {    
    _pre_call_callback("glVertexAttrib4Nusv", (void*)glVertexAttrib4Nusv, 2, arg0, arg1);
     glad_glVertexAttrib4Nusv(arg0, arg1);
    _post_call_callback("glVertexAttrib4Nusv", (void*)glVertexAttrib4Nusv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIB4NUSVPROC glad_debug_glVertexAttrib4Nusv = glad_debug_impl_glVertexAttrib4Nusv;
PFNGLVERTEXATTRIB4BVPROC glad_glVertexAttrib4bv;
void APIENTRY glad_debug_impl_glVertexAttrib4bv(GLuint arg0, const GLbyte * arg1) {    
    _pre_call_callback("glVertexAttrib4bv", (void*)glVertexAttrib4bv, 2, arg0, arg1);
     glad_glVertexAttrib4bv(arg0, arg1);
    _post_call_callback("glVertexAttrib4bv", (void*)glVertexAttrib4bv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIB4BVPROC glad_debug_glVertexAttrib4bv = glad_debug_impl_glVertexAttrib4bv;
PFNGLVERTEXATTRIB4DPROC glad_glVertexAttrib4d;
void APIENTRY glad_debug_impl_glVertexAttrib4d(GLuint arg0, GLdouble arg1, GLdouble arg2, GLdouble arg3, GLdouble arg4) {    
    _pre_call_callback("glVertexAttrib4d", (void*)glVertexAttrib4d, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glVertexAttrib4d(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glVertexAttrib4d", (void*)glVertexAttrib4d, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLVERTEXATTRIB4DPROC glad_debug_glVertexAttrib4d = glad_debug_impl_glVertexAttrib4d;
PFNGLVERTEXATTRIB4DVPROC glad_glVertexAttrib4dv;
void APIENTRY glad_debug_impl_glVertexAttrib4dv(GLuint arg0, const GLdouble * arg1) {    
    _pre_call_callback("glVertexAttrib4dv", (void*)glVertexAttrib4dv, 2, arg0, arg1);
     glad_glVertexAttrib4dv(arg0, arg1);
    _post_call_callback("glVertexAttrib4dv", (void*)glVertexAttrib4dv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIB4DVPROC glad_debug_glVertexAttrib4dv = glad_debug_impl_glVertexAttrib4dv;
PFNGLVERTEXATTRIB4FPROC glad_glVertexAttrib4f;
void APIENTRY glad_debug_impl_glVertexAttrib4f(GLuint arg0, GLfloat arg1, GLfloat arg2, GLfloat arg3, GLfloat arg4) {    
    _pre_call_callback("glVertexAttrib4f", (void*)glVertexAttrib4f, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glVertexAttrib4f(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glVertexAttrib4f", (void*)glVertexAttrib4f, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLVERTEXATTRIB4FPROC glad_debug_glVertexAttrib4f = glad_debug_impl_glVertexAttrib4f;
PFNGLVERTEXATTRIB4FVPROC glad_glVertexAttrib4fv;
void APIENTRY glad_debug_impl_glVertexAttrib4fv(GLuint arg0, const GLfloat * arg1) {    
    _pre_call_callback("glVertexAttrib4fv", (void*)glVertexAttrib4fv, 2, arg0, arg1);
     glad_glVertexAttrib4fv(arg0, arg1);
    _post_call_callback("glVertexAttrib4fv", (void*)glVertexAttrib4fv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIB4FVPROC glad_debug_glVertexAttrib4fv = glad_debug_impl_glVertexAttrib4fv;
PFNGLVERTEXATTRIB4IVPROC glad_glVertexAttrib4iv;
void APIENTRY glad_debug_impl_glVertexAttrib4iv(GLuint arg0, const GLint * arg1) {    
    _pre_call_callback("glVertexAttrib4iv", (void*)glVertexAttrib4iv, 2, arg0, arg1);
     glad_glVertexAttrib4iv(arg0, arg1);
    _post_call_callback("glVertexAttrib4iv", (void*)glVertexAttrib4iv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIB4IVPROC glad_debug_glVertexAttrib4iv = glad_debug_impl_glVertexAttrib4iv;
PFNGLVERTEXATTRIB4SPROC glad_glVertexAttrib4s;
void APIENTRY glad_debug_impl_glVertexAttrib4s(GLuint arg0, GLshort arg1, GLshort arg2, GLshort arg3, GLshort arg4) {    
    _pre_call_callback("glVertexAttrib4s", (void*)glVertexAttrib4s, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glVertexAttrib4s(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glVertexAttrib4s", (void*)glVertexAttrib4s, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLVERTEXATTRIB4SPROC glad_debug_glVertexAttrib4s = glad_debug_impl_glVertexAttrib4s;
PFNGLVERTEXATTRIB4SVPROC glad_glVertexAttrib4sv;
void APIENTRY glad_debug_impl_glVertexAttrib4sv(GLuint arg0, const GLshort * arg1) {    
    _pre_call_callback("glVertexAttrib4sv", (void*)glVertexAttrib4sv, 2, arg0, arg1);
     glad_glVertexAttrib4sv(arg0, arg1);
    _post_call_callback("glVertexAttrib4sv", (void*)glVertexAttrib4sv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIB4SVPROC glad_debug_glVertexAttrib4sv = glad_debug_impl_glVertexAttrib4sv;
PFNGLVERTEXATTRIB4UBVPROC glad_glVertexAttrib4ubv;
void APIENTRY glad_debug_impl_glVertexAttrib4ubv(GLuint arg0, const GLubyte * arg1) {    
    _pre_call_callback("glVertexAttrib4ubv", (void*)glVertexAttrib4ubv, 2, arg0, arg1);
     glad_glVertexAttrib4ubv(arg0, arg1);
    _post_call_callback("glVertexAttrib4ubv", (void*)glVertexAttrib4ubv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIB4UBVPROC glad_debug_glVertexAttrib4ubv = glad_debug_impl_glVertexAttrib4ubv;
PFNGLVERTEXATTRIB4UIVPROC glad_glVertexAttrib4uiv;
void APIENTRY glad_debug_impl_glVertexAttrib4uiv(GLuint arg0, const GLuint * arg1) {    
    _pre_call_callback("glVertexAttrib4uiv", (void*)glVertexAttrib4uiv, 2, arg0, arg1);
     glad_glVertexAttrib4uiv(arg0, arg1);
    _post_call_callback("glVertexAttrib4uiv", (void*)glVertexAttrib4uiv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIB4UIVPROC glad_debug_glVertexAttrib4uiv = glad_debug_impl_glVertexAttrib4uiv;
PFNGLVERTEXATTRIB4USVPROC glad_glVertexAttrib4usv;
void APIENTRY glad_debug_impl_glVertexAttrib4usv(GLuint arg0, const GLushort * arg1) {    
    _pre_call_callback("glVertexAttrib4usv", (void*)glVertexAttrib4usv, 2, arg0, arg1);
     glad_glVertexAttrib4usv(arg0, arg1);
    _post_call_callback("glVertexAttrib4usv", (void*)glVertexAttrib4usv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIB4USVPROC glad_debug_glVertexAttrib4usv = glad_debug_impl_glVertexAttrib4usv;
PFNGLVERTEXATTRIBBINDINGPROC glad_glVertexAttribBinding;
void APIENTRY glad_debug_impl_glVertexAttribBinding(GLuint arg0, GLuint arg1) {    
    _pre_call_callback("glVertexAttribBinding", (void*)glVertexAttribBinding, 2, arg0, arg1);
     glad_glVertexAttribBinding(arg0, arg1);
    _post_call_callback("glVertexAttribBinding", (void*)glVertexAttribBinding, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIBBINDINGPROC glad_debug_glVertexAttribBinding = glad_debug_impl_glVertexAttribBinding;
PFNGLVERTEXATTRIBDIVISORPROC glad_glVertexAttribDivisor;
void APIENTRY glad_debug_impl_glVertexAttribDivisor(GLuint arg0, GLuint arg1) {    
    _pre_call_callback("glVertexAttribDivisor", (void*)glVertexAttribDivisor, 2, arg0, arg1);
     glad_glVertexAttribDivisor(arg0, arg1);
    _post_call_callback("glVertexAttribDivisor", (void*)glVertexAttribDivisor, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIBDIVISORPROC glad_debug_glVertexAttribDivisor = glad_debug_impl_glVertexAttribDivisor;
PFNGLVERTEXATTRIBFORMATPROC glad_glVertexAttribFormat;
void APIENTRY glad_debug_impl_glVertexAttribFormat(GLuint arg0, GLint arg1, GLenum arg2, GLboolean arg3, GLuint arg4) {    
    _pre_call_callback("glVertexAttribFormat", (void*)glVertexAttribFormat, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glVertexAttribFormat(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glVertexAttribFormat", (void*)glVertexAttribFormat, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLVERTEXATTRIBFORMATPROC glad_debug_glVertexAttribFormat = glad_debug_impl_glVertexAttribFormat;
PFNGLVERTEXATTRIBI1IPROC glad_glVertexAttribI1i;
void APIENTRY glad_debug_impl_glVertexAttribI1i(GLuint arg0, GLint arg1) {    
    _pre_call_callback("glVertexAttribI1i", (void*)glVertexAttribI1i, 2, arg0, arg1);
     glad_glVertexAttribI1i(arg0, arg1);
    _post_call_callback("glVertexAttribI1i", (void*)glVertexAttribI1i, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIBI1IPROC glad_debug_glVertexAttribI1i = glad_debug_impl_glVertexAttribI1i;
PFNGLVERTEXATTRIBI1IVPROC glad_glVertexAttribI1iv;
void APIENTRY glad_debug_impl_glVertexAttribI1iv(GLuint arg0, const GLint * arg1) {    
    _pre_call_callback("glVertexAttribI1iv", (void*)glVertexAttribI1iv, 2, arg0, arg1);
     glad_glVertexAttribI1iv(arg0, arg1);
    _post_call_callback("glVertexAttribI1iv", (void*)glVertexAttribI1iv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIBI1IVPROC glad_debug_glVertexAttribI1iv = glad_debug_impl_glVertexAttribI1iv;
PFNGLVERTEXATTRIBI1UIPROC glad_glVertexAttribI1ui;
void APIENTRY glad_debug_impl_glVertexAttribI1ui(GLuint arg0, GLuint arg1) {    
    _pre_call_callback("glVertexAttribI1ui", (void*)glVertexAttribI1ui, 2, arg0, arg1);
     glad_glVertexAttribI1ui(arg0, arg1);
    _post_call_callback("glVertexAttribI1ui", (void*)glVertexAttribI1ui, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIBI1UIPROC glad_debug_glVertexAttribI1ui = glad_debug_impl_glVertexAttribI1ui;
PFNGLVERTEXATTRIBI1UIVPROC glad_glVertexAttribI1uiv;
void APIENTRY glad_debug_impl_glVertexAttribI1uiv(GLuint arg0, const GLuint * arg1) {    
    _pre_call_callback("glVertexAttribI1uiv", (void*)glVertexAttribI1uiv, 2, arg0, arg1);
     glad_glVertexAttribI1uiv(arg0, arg1);
    _post_call_callback("glVertexAttribI1uiv", (void*)glVertexAttribI1uiv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIBI1UIVPROC glad_debug_glVertexAttribI1uiv = glad_debug_impl_glVertexAttribI1uiv;
PFNGLVERTEXATTRIBI2IPROC glad_glVertexAttribI2i;
void APIENTRY glad_debug_impl_glVertexAttribI2i(GLuint arg0, GLint arg1, GLint arg2) {    
    _pre_call_callback("glVertexAttribI2i", (void*)glVertexAttribI2i, 3, arg0, arg1, arg2);
     glad_glVertexAttribI2i(arg0, arg1, arg2);
    _post_call_callback("glVertexAttribI2i", (void*)glVertexAttribI2i, 3, arg0, arg1, arg2);
    
}
PFNGLVERTEXATTRIBI2IPROC glad_debug_glVertexAttribI2i = glad_debug_impl_glVertexAttribI2i;
PFNGLVERTEXATTRIBI2IVPROC glad_glVertexAttribI2iv;
void APIENTRY glad_debug_impl_glVertexAttribI2iv(GLuint arg0, const GLint * arg1) {    
    _pre_call_callback("glVertexAttribI2iv", (void*)glVertexAttribI2iv, 2, arg0, arg1);
     glad_glVertexAttribI2iv(arg0, arg1);
    _post_call_callback("glVertexAttribI2iv", (void*)glVertexAttribI2iv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIBI2IVPROC glad_debug_glVertexAttribI2iv = glad_debug_impl_glVertexAttribI2iv;
PFNGLVERTEXATTRIBI2UIPROC glad_glVertexAttribI2ui;
void APIENTRY glad_debug_impl_glVertexAttribI2ui(GLuint arg0, GLuint arg1, GLuint arg2) {    
    _pre_call_callback("glVertexAttribI2ui", (void*)glVertexAttribI2ui, 3, arg0, arg1, arg2);
     glad_glVertexAttribI2ui(arg0, arg1, arg2);
    _post_call_callback("glVertexAttribI2ui", (void*)glVertexAttribI2ui, 3, arg0, arg1, arg2);
    
}
PFNGLVERTEXATTRIBI2UIPROC glad_debug_glVertexAttribI2ui = glad_debug_impl_glVertexAttribI2ui;
PFNGLVERTEXATTRIBI2UIVPROC glad_glVertexAttribI2uiv;
void APIENTRY glad_debug_impl_glVertexAttribI2uiv(GLuint arg0, const GLuint * arg1) {    
    _pre_call_callback("glVertexAttribI2uiv", (void*)glVertexAttribI2uiv, 2, arg0, arg1);
     glad_glVertexAttribI2uiv(arg0, arg1);
    _post_call_callback("glVertexAttribI2uiv", (void*)glVertexAttribI2uiv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIBI2UIVPROC glad_debug_glVertexAttribI2uiv = glad_debug_impl_glVertexAttribI2uiv;
PFNGLVERTEXATTRIBI3IPROC glad_glVertexAttribI3i;
void APIENTRY glad_debug_impl_glVertexAttribI3i(GLuint arg0, GLint arg1, GLint arg2, GLint arg3) {    
    _pre_call_callback("glVertexAttribI3i", (void*)glVertexAttribI3i, 4, arg0, arg1, arg2, arg3);
     glad_glVertexAttribI3i(arg0, arg1, arg2, arg3);
    _post_call_callback("glVertexAttribI3i", (void*)glVertexAttribI3i, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLVERTEXATTRIBI3IPROC glad_debug_glVertexAttribI3i = glad_debug_impl_glVertexAttribI3i;
PFNGLVERTEXATTRIBI3IVPROC glad_glVertexAttribI3iv;
void APIENTRY glad_debug_impl_glVertexAttribI3iv(GLuint arg0, const GLint * arg1) {    
    _pre_call_callback("glVertexAttribI3iv", (void*)glVertexAttribI3iv, 2, arg0, arg1);
     glad_glVertexAttribI3iv(arg0, arg1);
    _post_call_callback("glVertexAttribI3iv", (void*)glVertexAttribI3iv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIBI3IVPROC glad_debug_glVertexAttribI3iv = glad_debug_impl_glVertexAttribI3iv;
PFNGLVERTEXATTRIBI3UIPROC glad_glVertexAttribI3ui;
void APIENTRY glad_debug_impl_glVertexAttribI3ui(GLuint arg0, GLuint arg1, GLuint arg2, GLuint arg3) {    
    _pre_call_callback("glVertexAttribI3ui", (void*)glVertexAttribI3ui, 4, arg0, arg1, arg2, arg3);
     glad_glVertexAttribI3ui(arg0, arg1, arg2, arg3);
    _post_call_callback("glVertexAttribI3ui", (void*)glVertexAttribI3ui, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLVERTEXATTRIBI3UIPROC glad_debug_glVertexAttribI3ui = glad_debug_impl_glVertexAttribI3ui;
PFNGLVERTEXATTRIBI3UIVPROC glad_glVertexAttribI3uiv;
void APIENTRY glad_debug_impl_glVertexAttribI3uiv(GLuint arg0, const GLuint * arg1) {    
    _pre_call_callback("glVertexAttribI3uiv", (void*)glVertexAttribI3uiv, 2, arg0, arg1);
     glad_glVertexAttribI3uiv(arg0, arg1);
    _post_call_callback("glVertexAttribI3uiv", (void*)glVertexAttribI3uiv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIBI3UIVPROC glad_debug_glVertexAttribI3uiv = glad_debug_impl_glVertexAttribI3uiv;
PFNGLVERTEXATTRIBI4BVPROC glad_glVertexAttribI4bv;
void APIENTRY glad_debug_impl_glVertexAttribI4bv(GLuint arg0, const GLbyte * arg1) {    
    _pre_call_callback("glVertexAttribI4bv", (void*)glVertexAttribI4bv, 2, arg0, arg1);
     glad_glVertexAttribI4bv(arg0, arg1);
    _post_call_callback("glVertexAttribI4bv", (void*)glVertexAttribI4bv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIBI4BVPROC glad_debug_glVertexAttribI4bv = glad_debug_impl_glVertexAttribI4bv;
PFNGLVERTEXATTRIBI4IPROC glad_glVertexAttribI4i;
void APIENTRY glad_debug_impl_glVertexAttribI4i(GLuint arg0, GLint arg1, GLint arg2, GLint arg3, GLint arg4) {    
    _pre_call_callback("glVertexAttribI4i", (void*)glVertexAttribI4i, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glVertexAttribI4i(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glVertexAttribI4i", (void*)glVertexAttribI4i, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLVERTEXATTRIBI4IPROC glad_debug_glVertexAttribI4i = glad_debug_impl_glVertexAttribI4i;
PFNGLVERTEXATTRIBI4IVPROC glad_glVertexAttribI4iv;
void APIENTRY glad_debug_impl_glVertexAttribI4iv(GLuint arg0, const GLint * arg1) {    
    _pre_call_callback("glVertexAttribI4iv", (void*)glVertexAttribI4iv, 2, arg0, arg1);
     glad_glVertexAttribI4iv(arg0, arg1);
    _post_call_callback("glVertexAttribI4iv", (void*)glVertexAttribI4iv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIBI4IVPROC glad_debug_glVertexAttribI4iv = glad_debug_impl_glVertexAttribI4iv;
PFNGLVERTEXATTRIBI4SVPROC glad_glVertexAttribI4sv;
void APIENTRY glad_debug_impl_glVertexAttribI4sv(GLuint arg0, const GLshort * arg1) {    
    _pre_call_callback("glVertexAttribI4sv", (void*)glVertexAttribI4sv, 2, arg0, arg1);
     glad_glVertexAttribI4sv(arg0, arg1);
    _post_call_callback("glVertexAttribI4sv", (void*)glVertexAttribI4sv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIBI4SVPROC glad_debug_glVertexAttribI4sv = glad_debug_impl_glVertexAttribI4sv;
PFNGLVERTEXATTRIBI4UBVPROC glad_glVertexAttribI4ubv;
void APIENTRY glad_debug_impl_glVertexAttribI4ubv(GLuint arg0, const GLubyte * arg1) {    
    _pre_call_callback("glVertexAttribI4ubv", (void*)glVertexAttribI4ubv, 2, arg0, arg1);
     glad_glVertexAttribI4ubv(arg0, arg1);
    _post_call_callback("glVertexAttribI4ubv", (void*)glVertexAttribI4ubv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIBI4UBVPROC glad_debug_glVertexAttribI4ubv = glad_debug_impl_glVertexAttribI4ubv;
PFNGLVERTEXATTRIBI4UIPROC glad_glVertexAttribI4ui;
void APIENTRY glad_debug_impl_glVertexAttribI4ui(GLuint arg0, GLuint arg1, GLuint arg2, GLuint arg3, GLuint arg4) {    
    _pre_call_callback("glVertexAttribI4ui", (void*)glVertexAttribI4ui, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glVertexAttribI4ui(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glVertexAttribI4ui", (void*)glVertexAttribI4ui, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLVERTEXATTRIBI4UIPROC glad_debug_glVertexAttribI4ui = glad_debug_impl_glVertexAttribI4ui;
PFNGLVERTEXATTRIBI4UIVPROC glad_glVertexAttribI4uiv;
void APIENTRY glad_debug_impl_glVertexAttribI4uiv(GLuint arg0, const GLuint * arg1) {    
    _pre_call_callback("glVertexAttribI4uiv", (void*)glVertexAttribI4uiv, 2, arg0, arg1);
     glad_glVertexAttribI4uiv(arg0, arg1);
    _post_call_callback("glVertexAttribI4uiv", (void*)glVertexAttribI4uiv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIBI4UIVPROC glad_debug_glVertexAttribI4uiv = glad_debug_impl_glVertexAttribI4uiv;
PFNGLVERTEXATTRIBI4USVPROC glad_glVertexAttribI4usv;
void APIENTRY glad_debug_impl_glVertexAttribI4usv(GLuint arg0, const GLushort * arg1) {    
    _pre_call_callback("glVertexAttribI4usv", (void*)glVertexAttribI4usv, 2, arg0, arg1);
     glad_glVertexAttribI4usv(arg0, arg1);
    _post_call_callback("glVertexAttribI4usv", (void*)glVertexAttribI4usv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIBI4USVPROC glad_debug_glVertexAttribI4usv = glad_debug_impl_glVertexAttribI4usv;
PFNGLVERTEXATTRIBIFORMATPROC glad_glVertexAttribIFormat;
void APIENTRY glad_debug_impl_glVertexAttribIFormat(GLuint arg0, GLint arg1, GLenum arg2, GLuint arg3) {    
    _pre_call_callback("glVertexAttribIFormat", (void*)glVertexAttribIFormat, 4, arg0, arg1, arg2, arg3);
     glad_glVertexAttribIFormat(arg0, arg1, arg2, arg3);
    _post_call_callback("glVertexAttribIFormat", (void*)glVertexAttribIFormat, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLVERTEXATTRIBIFORMATPROC glad_debug_glVertexAttribIFormat = glad_debug_impl_glVertexAttribIFormat;
PFNGLVERTEXATTRIBIPOINTERPROC glad_glVertexAttribIPointer;
void APIENTRY glad_debug_impl_glVertexAttribIPointer(GLuint arg0, GLint arg1, GLenum arg2, GLsizei arg3, const void * arg4) {    
    _pre_call_callback("glVertexAttribIPointer", (void*)glVertexAttribIPointer, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glVertexAttribIPointer(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glVertexAttribIPointer", (void*)glVertexAttribIPointer, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLVERTEXATTRIBIPOINTERPROC glad_debug_glVertexAttribIPointer = glad_debug_impl_glVertexAttribIPointer;
PFNGLVERTEXATTRIBL1DPROC glad_glVertexAttribL1d;
void APIENTRY glad_debug_impl_glVertexAttribL1d(GLuint arg0, GLdouble arg1) {    
    _pre_call_callback("glVertexAttribL1d", (void*)glVertexAttribL1d, 2, arg0, arg1);
     glad_glVertexAttribL1d(arg0, arg1);
    _post_call_callback("glVertexAttribL1d", (void*)glVertexAttribL1d, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIBL1DPROC glad_debug_glVertexAttribL1d = glad_debug_impl_glVertexAttribL1d;
PFNGLVERTEXATTRIBL1DVPROC glad_glVertexAttribL1dv;
void APIENTRY glad_debug_impl_glVertexAttribL1dv(GLuint arg0, const GLdouble * arg1) {    
    _pre_call_callback("glVertexAttribL1dv", (void*)glVertexAttribL1dv, 2, arg0, arg1);
     glad_glVertexAttribL1dv(arg0, arg1);
    _post_call_callback("glVertexAttribL1dv", (void*)glVertexAttribL1dv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIBL1DVPROC glad_debug_glVertexAttribL1dv = glad_debug_impl_glVertexAttribL1dv;
PFNGLVERTEXATTRIBL2DPROC glad_glVertexAttribL2d;
void APIENTRY glad_debug_impl_glVertexAttribL2d(GLuint arg0, GLdouble arg1, GLdouble arg2) {    
    _pre_call_callback("glVertexAttribL2d", (void*)glVertexAttribL2d, 3, arg0, arg1, arg2);
     glad_glVertexAttribL2d(arg0, arg1, arg2);
    _post_call_callback("glVertexAttribL2d", (void*)glVertexAttribL2d, 3, arg0, arg1, arg2);
    
}
PFNGLVERTEXATTRIBL2DPROC glad_debug_glVertexAttribL2d = glad_debug_impl_glVertexAttribL2d;
PFNGLVERTEXATTRIBL2DVPROC glad_glVertexAttribL2dv;
void APIENTRY glad_debug_impl_glVertexAttribL2dv(GLuint arg0, const GLdouble * arg1) {    
    _pre_call_callback("glVertexAttribL2dv", (void*)glVertexAttribL2dv, 2, arg0, arg1);
     glad_glVertexAttribL2dv(arg0, arg1);
    _post_call_callback("glVertexAttribL2dv", (void*)glVertexAttribL2dv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIBL2DVPROC glad_debug_glVertexAttribL2dv = glad_debug_impl_glVertexAttribL2dv;
PFNGLVERTEXATTRIBL3DPROC glad_glVertexAttribL3d;
void APIENTRY glad_debug_impl_glVertexAttribL3d(GLuint arg0, GLdouble arg1, GLdouble arg2, GLdouble arg3) {    
    _pre_call_callback("glVertexAttribL3d", (void*)glVertexAttribL3d, 4, arg0, arg1, arg2, arg3);
     glad_glVertexAttribL3d(arg0, arg1, arg2, arg3);
    _post_call_callback("glVertexAttribL3d", (void*)glVertexAttribL3d, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLVERTEXATTRIBL3DPROC glad_debug_glVertexAttribL3d = glad_debug_impl_glVertexAttribL3d;
PFNGLVERTEXATTRIBL3DVPROC glad_glVertexAttribL3dv;
void APIENTRY glad_debug_impl_glVertexAttribL3dv(GLuint arg0, const GLdouble * arg1) {    
    _pre_call_callback("glVertexAttribL3dv", (void*)glVertexAttribL3dv, 2, arg0, arg1);
     glad_glVertexAttribL3dv(arg0, arg1);
    _post_call_callback("glVertexAttribL3dv", (void*)glVertexAttribL3dv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIBL3DVPROC glad_debug_glVertexAttribL3dv = glad_debug_impl_glVertexAttribL3dv;
PFNGLVERTEXATTRIBL4DPROC glad_glVertexAttribL4d;
void APIENTRY glad_debug_impl_glVertexAttribL4d(GLuint arg0, GLdouble arg1, GLdouble arg2, GLdouble arg3, GLdouble arg4) {    
    _pre_call_callback("glVertexAttribL4d", (void*)glVertexAttribL4d, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glVertexAttribL4d(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glVertexAttribL4d", (void*)glVertexAttribL4d, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLVERTEXATTRIBL4DPROC glad_debug_glVertexAttribL4d = glad_debug_impl_glVertexAttribL4d;
PFNGLVERTEXATTRIBL4DVPROC glad_glVertexAttribL4dv;
void APIENTRY glad_debug_impl_glVertexAttribL4dv(GLuint arg0, const GLdouble * arg1) {    
    _pre_call_callback("glVertexAttribL4dv", (void*)glVertexAttribL4dv, 2, arg0, arg1);
     glad_glVertexAttribL4dv(arg0, arg1);
    _post_call_callback("glVertexAttribL4dv", (void*)glVertexAttribL4dv, 2, arg0, arg1);
    
}
PFNGLVERTEXATTRIBL4DVPROC glad_debug_glVertexAttribL4dv = glad_debug_impl_glVertexAttribL4dv;
PFNGLVERTEXATTRIBLFORMATPROC glad_glVertexAttribLFormat;
void APIENTRY glad_debug_impl_glVertexAttribLFormat(GLuint arg0, GLint arg1, GLenum arg2, GLuint arg3) {    
    _pre_call_callback("glVertexAttribLFormat", (void*)glVertexAttribLFormat, 4, arg0, arg1, arg2, arg3);
     glad_glVertexAttribLFormat(arg0, arg1, arg2, arg3);
    _post_call_callback("glVertexAttribLFormat", (void*)glVertexAttribLFormat, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLVERTEXATTRIBLFORMATPROC glad_debug_glVertexAttribLFormat = glad_debug_impl_glVertexAttribLFormat;
PFNGLVERTEXATTRIBLPOINTERPROC glad_glVertexAttribLPointer;
void APIENTRY glad_debug_impl_glVertexAttribLPointer(GLuint arg0, GLint arg1, GLenum arg2, GLsizei arg3, const void * arg4) {    
    _pre_call_callback("glVertexAttribLPointer", (void*)glVertexAttribLPointer, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glVertexAttribLPointer(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glVertexAttribLPointer", (void*)glVertexAttribLPointer, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLVERTEXATTRIBLPOINTERPROC glad_debug_glVertexAttribLPointer = glad_debug_impl_glVertexAttribLPointer;
PFNGLVERTEXATTRIBP1UIPROC glad_glVertexAttribP1ui;
void APIENTRY glad_debug_impl_glVertexAttribP1ui(GLuint arg0, GLenum arg1, GLboolean arg2, GLuint arg3) {    
    _pre_call_callback("glVertexAttribP1ui", (void*)glVertexAttribP1ui, 4, arg0, arg1, arg2, arg3);
     glad_glVertexAttribP1ui(arg0, arg1, arg2, arg3);
    _post_call_callback("glVertexAttribP1ui", (void*)glVertexAttribP1ui, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLVERTEXATTRIBP1UIPROC glad_debug_glVertexAttribP1ui = glad_debug_impl_glVertexAttribP1ui;
PFNGLVERTEXATTRIBP1UIVPROC glad_glVertexAttribP1uiv;
void APIENTRY glad_debug_impl_glVertexAttribP1uiv(GLuint arg0, GLenum arg1, GLboolean arg2, const GLuint * arg3) {    
    _pre_call_callback("glVertexAttribP1uiv", (void*)glVertexAttribP1uiv, 4, arg0, arg1, arg2, arg3);
     glad_glVertexAttribP1uiv(arg0, arg1, arg2, arg3);
    _post_call_callback("glVertexAttribP1uiv", (void*)glVertexAttribP1uiv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLVERTEXATTRIBP1UIVPROC glad_debug_glVertexAttribP1uiv = glad_debug_impl_glVertexAttribP1uiv;
PFNGLVERTEXATTRIBP2UIPROC glad_glVertexAttribP2ui;
void APIENTRY glad_debug_impl_glVertexAttribP2ui(GLuint arg0, GLenum arg1, GLboolean arg2, GLuint arg3) {    
    _pre_call_callback("glVertexAttribP2ui", (void*)glVertexAttribP2ui, 4, arg0, arg1, arg2, arg3);
     glad_glVertexAttribP2ui(arg0, arg1, arg2, arg3);
    _post_call_callback("glVertexAttribP2ui", (void*)glVertexAttribP2ui, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLVERTEXATTRIBP2UIPROC glad_debug_glVertexAttribP2ui = glad_debug_impl_glVertexAttribP2ui;
PFNGLVERTEXATTRIBP2UIVPROC glad_glVertexAttribP2uiv;
void APIENTRY glad_debug_impl_glVertexAttribP2uiv(GLuint arg0, GLenum arg1, GLboolean arg2, const GLuint * arg3) {    
    _pre_call_callback("glVertexAttribP2uiv", (void*)glVertexAttribP2uiv, 4, arg0, arg1, arg2, arg3);
     glad_glVertexAttribP2uiv(arg0, arg1, arg2, arg3);
    _post_call_callback("glVertexAttribP2uiv", (void*)glVertexAttribP2uiv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLVERTEXATTRIBP2UIVPROC glad_debug_glVertexAttribP2uiv = glad_debug_impl_glVertexAttribP2uiv;
PFNGLVERTEXATTRIBP3UIPROC glad_glVertexAttribP3ui;
void APIENTRY glad_debug_impl_glVertexAttribP3ui(GLuint arg0, GLenum arg1, GLboolean arg2, GLuint arg3) {    
    _pre_call_callback("glVertexAttribP3ui", (void*)glVertexAttribP3ui, 4, arg0, arg1, arg2, arg3);
     glad_glVertexAttribP3ui(arg0, arg1, arg2, arg3);
    _post_call_callback("glVertexAttribP3ui", (void*)glVertexAttribP3ui, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLVERTEXATTRIBP3UIPROC glad_debug_glVertexAttribP3ui = glad_debug_impl_glVertexAttribP3ui;
PFNGLVERTEXATTRIBP3UIVPROC glad_glVertexAttribP3uiv;
void APIENTRY glad_debug_impl_glVertexAttribP3uiv(GLuint arg0, GLenum arg1, GLboolean arg2, const GLuint * arg3) {    
    _pre_call_callback("glVertexAttribP3uiv", (void*)glVertexAttribP3uiv, 4, arg0, arg1, arg2, arg3);
     glad_glVertexAttribP3uiv(arg0, arg1, arg2, arg3);
    _post_call_callback("glVertexAttribP3uiv", (void*)glVertexAttribP3uiv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLVERTEXATTRIBP3UIVPROC glad_debug_glVertexAttribP3uiv = glad_debug_impl_glVertexAttribP3uiv;
PFNGLVERTEXATTRIBP4UIPROC glad_glVertexAttribP4ui;
void APIENTRY glad_debug_impl_glVertexAttribP4ui(GLuint arg0, GLenum arg1, GLboolean arg2, GLuint arg3) {    
    _pre_call_callback("glVertexAttribP4ui", (void*)glVertexAttribP4ui, 4, arg0, arg1, arg2, arg3);
     glad_glVertexAttribP4ui(arg0, arg1, arg2, arg3);
    _post_call_callback("glVertexAttribP4ui", (void*)glVertexAttribP4ui, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLVERTEXATTRIBP4UIPROC glad_debug_glVertexAttribP4ui = glad_debug_impl_glVertexAttribP4ui;
PFNGLVERTEXATTRIBP4UIVPROC glad_glVertexAttribP4uiv;
void APIENTRY glad_debug_impl_glVertexAttribP4uiv(GLuint arg0, GLenum arg1, GLboolean arg2, const GLuint * arg3) {    
    _pre_call_callback("glVertexAttribP4uiv", (void*)glVertexAttribP4uiv, 4, arg0, arg1, arg2, arg3);
     glad_glVertexAttribP4uiv(arg0, arg1, arg2, arg3);
    _post_call_callback("glVertexAttribP4uiv", (void*)glVertexAttribP4uiv, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLVERTEXATTRIBP4UIVPROC glad_debug_glVertexAttribP4uiv = glad_debug_impl_glVertexAttribP4uiv;
PFNGLVERTEXATTRIBPOINTERPROC glad_glVertexAttribPointer;
void APIENTRY glad_debug_impl_glVertexAttribPointer(GLuint arg0, GLint arg1, GLenum arg2, GLboolean arg3, GLsizei arg4, const void * arg5) {    
    _pre_call_callback("glVertexAttribPointer", (void*)glVertexAttribPointer, 6, arg0, arg1, arg2, arg3, arg4, arg5);
     glad_glVertexAttribPointer(arg0, arg1, arg2, arg3, arg4, arg5);
    _post_call_callback("glVertexAttribPointer", (void*)glVertexAttribPointer, 6, arg0, arg1, arg2, arg3, arg4, arg5);
    
}
PFNGLVERTEXATTRIBPOINTERPROC glad_debug_glVertexAttribPointer = glad_debug_impl_glVertexAttribPointer;
PFNGLVERTEXBINDINGDIVISORPROC glad_glVertexBindingDivisor;
void APIENTRY glad_debug_impl_glVertexBindingDivisor(GLuint arg0, GLuint arg1) {    
    _pre_call_callback("glVertexBindingDivisor", (void*)glVertexBindingDivisor, 2, arg0, arg1);
     glad_glVertexBindingDivisor(arg0, arg1);
    _post_call_callback("glVertexBindingDivisor", (void*)glVertexBindingDivisor, 2, arg0, arg1);
    
}
PFNGLVERTEXBINDINGDIVISORPROC glad_debug_glVertexBindingDivisor = glad_debug_impl_glVertexBindingDivisor;
PFNGLVERTEXP2UIPROC glad_glVertexP2ui;
void APIENTRY glad_debug_impl_glVertexP2ui(GLenum arg0, GLuint arg1) {    
    _pre_call_callback("glVertexP2ui", (void*)glVertexP2ui, 2, arg0, arg1);
     glad_glVertexP2ui(arg0, arg1);
    _post_call_callback("glVertexP2ui", (void*)glVertexP2ui, 2, arg0, arg1);
    
}
PFNGLVERTEXP2UIPROC glad_debug_glVertexP2ui = glad_debug_impl_glVertexP2ui;
PFNGLVERTEXP2UIVPROC glad_glVertexP2uiv;
void APIENTRY glad_debug_impl_glVertexP2uiv(GLenum arg0, const GLuint * arg1) {    
    _pre_call_callback("glVertexP2uiv", (void*)glVertexP2uiv, 2, arg0, arg1);
     glad_glVertexP2uiv(arg0, arg1);
    _post_call_callback("glVertexP2uiv", (void*)glVertexP2uiv, 2, arg0, arg1);
    
}
PFNGLVERTEXP2UIVPROC glad_debug_glVertexP2uiv = glad_debug_impl_glVertexP2uiv;
PFNGLVERTEXP3UIPROC glad_glVertexP3ui;
void APIENTRY glad_debug_impl_glVertexP3ui(GLenum arg0, GLuint arg1) {    
    _pre_call_callback("glVertexP3ui", (void*)glVertexP3ui, 2, arg0, arg1);
     glad_glVertexP3ui(arg0, arg1);
    _post_call_callback("glVertexP3ui", (void*)glVertexP3ui, 2, arg0, arg1);
    
}
PFNGLVERTEXP3UIPROC glad_debug_glVertexP3ui = glad_debug_impl_glVertexP3ui;
PFNGLVERTEXP3UIVPROC glad_glVertexP3uiv;
void APIENTRY glad_debug_impl_glVertexP3uiv(GLenum arg0, const GLuint * arg1) {    
    _pre_call_callback("glVertexP3uiv", (void*)glVertexP3uiv, 2, arg0, arg1);
     glad_glVertexP3uiv(arg0, arg1);
    _post_call_callback("glVertexP3uiv", (void*)glVertexP3uiv, 2, arg0, arg1);
    
}
PFNGLVERTEXP3UIVPROC glad_debug_glVertexP3uiv = glad_debug_impl_glVertexP3uiv;
PFNGLVERTEXP4UIPROC glad_glVertexP4ui;
void APIENTRY glad_debug_impl_glVertexP4ui(GLenum arg0, GLuint arg1) {    
    _pre_call_callback("glVertexP4ui", (void*)glVertexP4ui, 2, arg0, arg1);
     glad_glVertexP4ui(arg0, arg1);
    _post_call_callback("glVertexP4ui", (void*)glVertexP4ui, 2, arg0, arg1);
    
}
PFNGLVERTEXP4UIPROC glad_debug_glVertexP4ui = glad_debug_impl_glVertexP4ui;
PFNGLVERTEXP4UIVPROC glad_glVertexP4uiv;
void APIENTRY glad_debug_impl_glVertexP4uiv(GLenum arg0, const GLuint * arg1) {    
    _pre_call_callback("glVertexP4uiv", (void*)glVertexP4uiv, 2, arg0, arg1);
     glad_glVertexP4uiv(arg0, arg1);
    _post_call_callback("glVertexP4uiv", (void*)glVertexP4uiv, 2, arg0, arg1);
    
}
PFNGLVERTEXP4UIVPROC glad_debug_glVertexP4uiv = glad_debug_impl_glVertexP4uiv;
PFNGLVERTEXPOINTERPROC glad_glVertexPointer;
void APIENTRY glad_debug_impl_glVertexPointer(GLint arg0, GLenum arg1, GLsizei arg2, const void * arg3) {    
    _pre_call_callback("glVertexPointer", (void*)glVertexPointer, 4, arg0, arg1, arg2, arg3);
     glad_glVertexPointer(arg0, arg1, arg2, arg3);
    _post_call_callback("glVertexPointer", (void*)glVertexPointer, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLVERTEXPOINTERPROC glad_debug_glVertexPointer = glad_debug_impl_glVertexPointer;
PFNGLVIEWPORTPROC glad_glViewport;
void APIENTRY glad_debug_impl_glViewport(GLint arg0, GLint arg1, GLsizei arg2, GLsizei arg3) {    
    _pre_call_callback("glViewport", (void*)glViewport, 4, arg0, arg1, arg2, arg3);
     glad_glViewport(arg0, arg1, arg2, arg3);
    _post_call_callback("glViewport", (void*)glViewport, 4, arg0, arg1, arg2, arg3);
    
}
PFNGLVIEWPORTPROC glad_debug_glViewport = glad_debug_impl_glViewport;
PFNGLVIEWPORTARRAYVPROC glad_glViewportArrayv;
void APIENTRY glad_debug_impl_glViewportArrayv(GLuint arg0, GLsizei arg1, const GLfloat * arg2) {    
    _pre_call_callback("glViewportArrayv", (void*)glViewportArrayv, 3, arg0, arg1, arg2);
     glad_glViewportArrayv(arg0, arg1, arg2);
    _post_call_callback("glViewportArrayv", (void*)glViewportArrayv, 3, arg0, arg1, arg2);
    
}
PFNGLVIEWPORTARRAYVPROC glad_debug_glViewportArrayv = glad_debug_impl_glViewportArrayv;
PFNGLVIEWPORTINDEXEDFPROC glad_glViewportIndexedf;
void APIENTRY glad_debug_impl_glViewportIndexedf(GLuint arg0, GLfloat arg1, GLfloat arg2, GLfloat arg3, GLfloat arg4) {    
    _pre_call_callback("glViewportIndexedf", (void*)glViewportIndexedf, 5, arg0, arg1, arg2, arg3, arg4);
     glad_glViewportIndexedf(arg0, arg1, arg2, arg3, arg4);
    _post_call_callback("glViewportIndexedf", (void*)glViewportIndexedf, 5, arg0, arg1, arg2, arg3, arg4);
    
}
PFNGLVIEWPORTINDEXEDFPROC glad_debug_glViewportIndexedf = glad_debug_impl_glViewportIndexedf;
PFNGLVIEWPORTINDEXEDFVPROC glad_glViewportIndexedfv;
void APIENTRY glad_debug_impl_glViewportIndexedfv(GLuint arg0, const GLfloat * arg1) {    
    _pre_call_callback("glViewportIndexedfv", (void*)glViewportIndexedfv, 2, arg0, arg1);
     glad_glViewportIndexedfv(arg0, arg1);
    _post_call_callback("glViewportIndexedfv", (void*)glViewportIndexedfv, 2, arg0, arg1);
    
}
PFNGLVIEWPORTINDEXEDFVPROC glad_debug_glViewportIndexedfv = glad_debug_impl_glViewportIndexedfv;
PFNGLWAITSYNCPROC glad_glWaitSync;
void APIENTRY glad_debug_impl_glWaitSync(GLsync arg0, GLbitfield arg1, GLuint64 arg2) {    
    _pre_call_callback("glWaitSync", (void*)glWaitSync, 3, arg0, arg1, arg2);
     glad_glWaitSync(arg0, arg1, arg2);
    _post_call_callback("glWaitSync", (void*)glWaitSync, 3, arg0, arg1, arg2);
    
}
PFNGLWAITSYNCPROC glad_debug_glWaitSync = glad_debug_impl_glWaitSync;
PFNGLWINDOWPOS2DPROC glad_glWindowPos2d;
void APIENTRY glad_debug_impl_glWindowPos2d(GLdouble arg0, GLdouble arg1) {    
    _pre_call_callback("glWindowPos2d", (void*)glWindowPos2d, 2, arg0, arg1);
     glad_glWindowPos2d(arg0, arg1);
    _post_call_callback("glWindowPos2d", (void*)glWindowPos2d, 2, arg0, arg1);
    
}
PFNGLWINDOWPOS2DPROC glad_debug_glWindowPos2d = glad_debug_impl_glWindowPos2d;
PFNGLWINDOWPOS2DVPROC glad_glWindowPos2dv;
void APIENTRY glad_debug_impl_glWindowPos2dv(const GLdouble * arg0) {    
    _pre_call_callback("glWindowPos2dv", (void*)glWindowPos2dv, 1, arg0);
     glad_glWindowPos2dv(arg0);
    _post_call_callback("glWindowPos2dv", (void*)glWindowPos2dv, 1, arg0);
    
}
PFNGLWINDOWPOS2DVPROC glad_debug_glWindowPos2dv = glad_debug_impl_glWindowPos2dv;
PFNGLWINDOWPOS2FPROC glad_glWindowPos2f;
void APIENTRY glad_debug_impl_glWindowPos2f(GLfloat arg0, GLfloat arg1) {    
    _pre_call_callback("glWindowPos2f", (void*)glWindowPos2f, 2, arg0, arg1);
     glad_glWindowPos2f(arg0, arg1);
    _post_call_callback("glWindowPos2f", (void*)glWindowPos2f, 2, arg0, arg1);
    
}
PFNGLWINDOWPOS2FPROC glad_debug_glWindowPos2f = glad_debug_impl_glWindowPos2f;
PFNGLWINDOWPOS2FVPROC glad_glWindowPos2fv;
void APIENTRY glad_debug_impl_glWindowPos2fv(const GLfloat * arg0) {    
    _pre_call_callback("glWindowPos2fv", (void*)glWindowPos2fv, 1, arg0);
     glad_glWindowPos2fv(arg0);
    _post_call_callback("glWindowPos2fv", (void*)glWindowPos2fv, 1, arg0);
    
}
PFNGLWINDOWPOS2FVPROC glad_debug_glWindowPos2fv = glad_debug_impl_glWindowPos2fv;
PFNGLWINDOWPOS2IPROC glad_glWindowPos2i;
void APIENTRY glad_debug_impl_glWindowPos2i(GLint arg0, GLint arg1) {    
    _pre_call_callback("glWindowPos2i", (void*)glWindowPos2i, 2, arg0, arg1);
     glad_glWindowPos2i(arg0, arg1);
    _post_call_callback("glWindowPos2i", (void*)glWindowPos2i, 2, arg0, arg1);
    
}
PFNGLWINDOWPOS2IPROC glad_debug_glWindowPos2i = glad_debug_impl_glWindowPos2i;
PFNGLWINDOWPOS2IVPROC glad_glWindowPos2iv;
void APIENTRY glad_debug_impl_glWindowPos2iv(const GLint * arg0) {    
    _pre_call_callback("glWindowPos2iv", (void*)glWindowPos2iv, 1, arg0);
     glad_glWindowPos2iv(arg0);
    _post_call_callback("glWindowPos2iv", (void*)glWindowPos2iv, 1, arg0);
    
}
PFNGLWINDOWPOS2IVPROC glad_debug_glWindowPos2iv = glad_debug_impl_glWindowPos2iv;
PFNGLWINDOWPOS2SPROC glad_glWindowPos2s;
void APIENTRY glad_debug_impl_glWindowPos2s(GLshort arg0, GLshort arg1) {    
    _pre_call_callback("glWindowPos2s", (void*)glWindowPos2s, 2, arg0, arg1);
     glad_glWindowPos2s(arg0, arg1);
    _post_call_callback("glWindowPos2s", (void*)glWindowPos2s, 2, arg0, arg1);
    
}
PFNGLWINDOWPOS2SPROC glad_debug_glWindowPos2s = glad_debug_impl_glWindowPos2s;
PFNGLWINDOWPOS2SVPROC glad_glWindowPos2sv;
void APIENTRY glad_debug_impl_glWindowPos2sv(const GLshort * arg0) {    
    _pre_call_callback("glWindowPos2sv", (void*)glWindowPos2sv, 1, arg0);
     glad_glWindowPos2sv(arg0);
    _post_call_callback("glWindowPos2sv", (void*)glWindowPos2sv, 1, arg0);
    
}
PFNGLWINDOWPOS2SVPROC glad_debug_glWindowPos2sv = glad_debug_impl_glWindowPos2sv;
PFNGLWINDOWPOS3DPROC glad_glWindowPos3d;
void APIENTRY glad_debug_impl_glWindowPos3d(GLdouble arg0, GLdouble arg1, GLdouble arg2) {    
    _pre_call_callback("glWindowPos3d", (void*)glWindowPos3d, 3, arg0, arg1, arg2);
     glad_glWindowPos3d(arg0, arg1, arg2);
    _post_call_callback("glWindowPos3d", (void*)glWindowPos3d, 3, arg0, arg1, arg2);
    
}
PFNGLWINDOWPOS3DPROC glad_debug_glWindowPos3d = glad_debug_impl_glWindowPos3d;
PFNGLWINDOWPOS3DVPROC glad_glWindowPos3dv;
void APIENTRY glad_debug_impl_glWindowPos3dv(const GLdouble * arg0) {    
    _pre_call_callback("glWindowPos3dv", (void*)glWindowPos3dv, 1, arg0);
     glad_glWindowPos3dv(arg0);
    _post_call_callback("glWindowPos3dv", (void*)glWindowPos3dv, 1, arg0);
    
}
PFNGLWINDOWPOS3DVPROC glad_debug_glWindowPos3dv = glad_debug_impl_glWindowPos3dv;
PFNGLWINDOWPOS3FPROC glad_glWindowPos3f;
void APIENTRY glad_debug_impl_glWindowPos3f(GLfloat arg0, GLfloat arg1, GLfloat arg2) {    
    _pre_call_callback("glWindowPos3f", (void*)glWindowPos3f, 3, arg0, arg1, arg2);
     glad_glWindowPos3f(arg0, arg1, arg2);
    _post_call_callback("glWindowPos3f", (void*)glWindowPos3f, 3, arg0, arg1, arg2);
    
}
PFNGLWINDOWPOS3FPROC glad_debug_glWindowPos3f = glad_debug_impl_glWindowPos3f;
PFNGLWINDOWPOS3FVPROC glad_glWindowPos3fv;
void APIENTRY glad_debug_impl_glWindowPos3fv(const GLfloat * arg0) {    
    _pre_call_callback("glWindowPos3fv", (void*)glWindowPos3fv, 1, arg0);
     glad_glWindowPos3fv(arg0);
    _post_call_callback("glWindowPos3fv", (void*)glWindowPos3fv, 1, arg0);
    
}
PFNGLWINDOWPOS3FVPROC glad_debug_glWindowPos3fv = glad_debug_impl_glWindowPos3fv;
PFNGLWINDOWPOS3IPROC glad_glWindowPos3i;
void APIENTRY glad_debug_impl_glWindowPos3i(GLint arg0, GLint arg1, GLint arg2) {    
    _pre_call_callback("glWindowPos3i", (void*)glWindowPos3i, 3, arg0, arg1, arg2);
     glad_glWindowPos3i(arg0, arg1, arg2);
    _post_call_callback("glWindowPos3i", (void*)glWindowPos3i, 3, arg0, arg1, arg2);
    
}
PFNGLWINDOWPOS3IPROC glad_debug_glWindowPos3i = glad_debug_impl_glWindowPos3i;
PFNGLWINDOWPOS3IVPROC glad_glWindowPos3iv;
void APIENTRY glad_debug_impl_glWindowPos3iv(const GLint * arg0) {    
    _pre_call_callback("glWindowPos3iv", (void*)glWindowPos3iv, 1, arg0);
     glad_glWindowPos3iv(arg0);
    _post_call_callback("glWindowPos3iv", (void*)glWindowPos3iv, 1, arg0);
    
}
PFNGLWINDOWPOS3IVPROC glad_debug_glWindowPos3iv = glad_debug_impl_glWindowPos3iv;
PFNGLWINDOWPOS3SPROC glad_glWindowPos3s;
void APIENTRY glad_debug_impl_glWindowPos3s(GLshort arg0, GLshort arg1, GLshort arg2) {    
    _pre_call_callback("glWindowPos3s", (void*)glWindowPos3s, 3, arg0, arg1, arg2);
     glad_glWindowPos3s(arg0, arg1, arg2);
    _post_call_callback("glWindowPos3s", (void*)glWindowPos3s, 3, arg0, arg1, arg2);
    
}
PFNGLWINDOWPOS3SPROC glad_debug_glWindowPos3s = glad_debug_impl_glWindowPos3s;
PFNGLWINDOWPOS3SVPROC glad_glWindowPos3sv;
void APIENTRY glad_debug_impl_glWindowPos3sv(const GLshort * arg0) {    
    _pre_call_callback("glWindowPos3sv", (void*)glWindowPos3sv, 1, arg0);
     glad_glWindowPos3sv(arg0);
    _post_call_callback("glWindowPos3sv", (void*)glWindowPos3sv, 1, arg0);
    
}
PFNGLWINDOWPOS3SVPROC glad_debug_glWindowPos3sv = glad_debug_impl_glWindowPos3sv;
static void load_GL_VERSION_1_0(GLADloadproc load) {
    if(!GLAD_GL_VERSION_1_0) return;
    glad_glCullFace = (PFNGLCULLFACEPROC)load("glCullFace");
    glad_glFrontFace = (PFNGLFRONTFACEPROC)load("glFrontFace");
    glad_glHint = (PFNGLHINTPROC)load("glHint");
    glad_glLineWidth = (PFNGLLINEWIDTHPROC)load("glLineWidth");
    glad_glPointSize = (PFNGLPOINTSIZEPROC)load("glPointSize");
    glad_glPolygonMode = (PFNGLPOLYGONMODEPROC)load("glPolygonMode");
    glad_glScissor = (PFNGLSCISSORPROC)load("glScissor");
    glad_glTexParameterf = (PFNGLTEXPARAMETERFPROC)load("glTexParameterf");
    glad_glTexParameterfv = (PFNGLTEXPARAMETERFVPROC)load("glTexParameterfv");
    glad_glTexParameteri = (PFNGLTEXPARAMETERIPROC)load("glTexParameteri");
    glad_glTexParameteriv = (PFNGLTEXPARAMETERIVPROC)load("glTexParameteriv");
    glad_glTexImage1D = (PFNGLTEXIMAGE1DPROC)load("glTexImage1D");
    glad_glTexImage2D = (PFNGLTEXIMAGE2DPROC)load("glTexImage2D");
    glad_glDrawBuffer = (PFNGLDRAWBUFFERPROC)load("glDrawBuffer");
    glad_glClear = (PFNGLCLEARPROC)load("glClear");
    glad_glClearColor = (PFNGLCLEARCOLORPROC)load("glClearColor");
    glad_glClearStencil = (PFNGLCLEARSTENCILPROC)load("glClearStencil");
    glad_glClearDepth = (PFNGLCLEARDEPTHPROC)load("glClearDepth");
    glad_glStencilMask = (PFNGLSTENCILMASKPROC)load("glStencilMask");
    glad_glColorMask = (PFNGLCOLORMASKPROC)load("glColorMask");
    glad_glDepthMask = (PFNGLDEPTHMASKPROC)load("glDepthMask");
    glad_glDisable = (PFNGLDISABLEPROC)load("glDisable");
    glad_glEnable = (PFNGLENABLEPROC)load("glEnable");
    glad_glFinish = (PFNGLFINISHPROC)load("glFinish");
    glad_glFlush = (PFNGLFLUSHPROC)load("glFlush");
    glad_glBlendFunc = (PFNGLBLENDFUNCPROC)load("glBlendFunc");
    glad_glLogicOp = (PFNGLLOGICOPPROC)load("glLogicOp");
    glad_glStencilFunc = (PFNGLSTENCILFUNCPROC)load("glStencilFunc");
    glad_glStencilOp = (PFNGLSTENCILOPPROC)load("glStencilOp");
    glad_glDepthFunc = (PFNGLDEPTHFUNCPROC)load("glDepthFunc");
    glad_glPixelStoref = (PFNGLPIXELSTOREFPROC)load("glPixelStoref");
    glad_glPixelStorei = (PFNGLPIXELSTOREIPROC)load("glPixelStorei");
    glad_glReadBuffer = (PFNGLREADBUFFERPROC)load("glReadBuffer");
    glad_glReadPixels = (PFNGLREADPIXELSPROC)load("glReadPixels");
    glad_glGetBooleanv = (PFNGLGETBOOLEANVPROC)load("glGetBooleanv");
    glad_glGetDoublev = (PFNGLGETDOUBLEVPROC)load("glGetDoublev");
    glad_glGetError = (PFNGLGETERRORPROC)load("glGetError");
    glad_glGetFloatv = (PFNGLGETFLOATVPROC)load("glGetFloatv");
    glad_glGetIntegerv = (PFNGLGETINTEGERVPROC)load("glGetIntegerv");
    glad_glGetString = (PFNGLGETSTRINGPROC)load("glGetString");
    glad_glGetTexImage = (PFNGLGETTEXIMAGEPROC)load("glGetTexImage");
    glad_glGetTexParameterfv = (PFNGLGETTEXPARAMETERFVPROC)load("glGetTexParameterfv");
    glad_glGetTexParameteriv = (PFNGLGETTEXPARAMETERIVPROC)load("glGetTexParameteriv");
    glad_glGetTexLevelParameterfv = (PFNGLGETTEXLEVELPARAMETERFVPROC)load("glGetTexLevelParameterfv");
    glad_glGetTexLevelParameteriv = (PFNGLGETTEXLEVELPARAMETERIVPROC)load("glGetTexLevelParameteriv");
    glad_glIsEnabled = (PFNGLISENABLEDPROC)load("glIsEnabled");
    glad_glDepthRange = (PFNGLDEPTHRANGEPROC)load("glDepthRange");
    glad_glViewport = (PFNGLVIEWPORTPROC)load("glViewport");
    glad_glNewList = (PFNGLNEWLISTPROC)load("glNewList");
    glad_glEndList = (PFNGLENDLISTPROC)load("glEndList");
    glad_glCallList = (PFNGLCALLLISTPROC)load("glCallList");
    glad_glCallLists = (PFNGLCALLLISTSPROC)load("glCallLists");
    glad_glDeleteLists = (PFNGLDELETELISTSPROC)load("glDeleteLists");
    glad_glGenLists = (PFNGLGENLISTSPROC)load("glGenLists");
    glad_glListBase = (PFNGLLISTBASEPROC)load("glListBase");
    glad_glBegin = (PFNGLBEGINPROC)load("glBegin");
    glad_glBitmap = (PFNGLBITMAPPROC)load("glBitmap");
    glad_glColor3b = (PFNGLCOLOR3BPROC)load("glColor3b");
    glad_glColor3bv = (PFNGLCOLOR3BVPROC)load("glColor3bv");
    glad_glColor3d = (PFNGLCOLOR3DPROC)load("glColor3d");
    glad_glColor3dv = (PFNGLCOLOR3DVPROC)load("glColor3dv");
    glad_glColor3f = (PFNGLCOLOR3FPROC)load("glColor3f");
    glad_glColor3fv = (PFNGLCOLOR3FVPROC)load("glColor3fv");
    glad_glColor3i = (PFNGLCOLOR3IPROC)load("glColor3i");
    glad_glColor3iv = (PFNGLCOLOR3IVPROC)load("glColor3iv");
    glad_glColor3s = (PFNGLCOLOR3SPROC)load("glColor3s");
    glad_glColor3sv = (PFNGLCOLOR3SVPROC)load("glColor3sv");
    glad_glColor3ub = (PFNGLCOLOR3UBPROC)load("glColor3ub");
    glad_glColor3ubv = (PFNGLCOLOR3UBVPROC)load("glColor3ubv");
    glad_glColor3ui = (PFNGLCOLOR3UIPROC)load("glColor3ui");
    glad_glColor3uiv = (PFNGLCOLOR3UIVPROC)load("glColor3uiv");
    glad_glColor3us = (PFNGLCOLOR3USPROC)load("glColor3us");
    glad_glColor3usv = (PFNGLCOLOR3USVPROC)load("glColor3usv");
    glad_glColor4b = (PFNGLCOLOR4BPROC)load("glColor4b");
    glad_glColor4bv = (PFNGLCOLOR4BVPROC)load("glColor4bv");
    glad_glColor4d = (PFNGLCOLOR4DPROC)load("glColor4d");
    glad_glColor4dv = (PFNGLCOLOR4DVPROC)load("glColor4dv");
    glad_glColor4f = (PFNGLCOLOR4FPROC)load("glColor4f");
    glad_glColor4fv = (PFNGLCOLOR4FVPROC)load("glColor4fv");
    glad_glColor4i = (PFNGLCOLOR4IPROC)load("glColor4i");
    glad_glColor4iv = (PFNGLCOLOR4IVPROC)load("glColor4iv");
    glad_glColor4s = (PFNGLCOLOR4SPROC)load("glColor4s");
    glad_glColor4sv = (PFNGLCOLOR4SVPROC)load("glColor4sv");
    glad_glColor4ub = (PFNGLCOLOR4UBPROC)load("glColor4ub");
    glad_glColor4ubv = (PFNGLCOLOR4UBVPROC)load("glColor4ubv");
    glad_glColor4ui = (PFNGLCOLOR4UIPROC)load("glColor4ui");
    glad_glColor4uiv = (PFNGLCOLOR4UIVPROC)load("glColor4uiv");
    glad_glColor4us = (PFNGLCOLOR4USPROC)load("glColor4us");
    glad_glColor4usv = (PFNGLCOLOR4USVPROC)load("glColor4usv");
    glad_glEdgeFlag = (PFNGLEDGEFLAGPROC)load("glEdgeFlag");
    glad_glEdgeFlagv = (PFNGLEDGEFLAGVPROC)load("glEdgeFlagv");
    glad_glEnd = (PFNGLENDPROC)load("glEnd");
    glad_glIndexd = (PFNGLINDEXDPROC)load("glIndexd");
    glad_glIndexdv = (PFNGLINDEXDVPROC)load("glIndexdv");
    glad_glIndexf = (PFNGLINDEXFPROC)load("glIndexf");
    glad_glIndexfv = (PFNGLINDEXFVPROC)load("glIndexfv");
    glad_glIndexi = (PFNGLINDEXIPROC)load("glIndexi");
    glad_glIndexiv = (PFNGLINDEXIVPROC)load("glIndexiv");
    glad_glIndexs = (PFNGLINDEXSPROC)load("glIndexs");
    glad_glIndexsv = (PFNGLINDEXSVPROC)load("glIndexsv");
    glad_glNormal3b = (PFNGLNORMAL3BPROC)load("glNormal3b");
    glad_glNormal3bv = (PFNGLNORMAL3BVPROC)load("glNormal3bv");
    glad_glNormal3d = (PFNGLNORMAL3DPROC)load("glNormal3d");
    glad_glNormal3dv = (PFNGLNORMAL3DVPROC)load("glNormal3dv");
    glad_glNormal3f = (PFNGLNORMAL3FPROC)load("glNormal3f");
    glad_glNormal3fv = (PFNGLNORMAL3FVPROC)load("glNormal3fv");
    glad_glNormal3i = (PFNGLNORMAL3IPROC)load("glNormal3i");
    glad_glNormal3iv = (PFNGLNORMAL3IVPROC)load("glNormal3iv");
    glad_glNormal3s = (PFNGLNORMAL3SPROC)load("glNormal3s");
    glad_glNormal3sv = (PFNGLNORMAL3SVPROC)load("glNormal3sv");
    glad_glRasterPos2d = (PFNGLRASTERPOS2DPROC)load("glRasterPos2d");
    glad_glRasterPos2dv = (PFNGLRASTERPOS2DVPROC)load("glRasterPos2dv");
    glad_glRasterPos2f = (PFNGLRASTERPOS2FPROC)load("glRasterPos2f");
    glad_glRasterPos2fv = (PFNGLRASTERPOS2FVPROC)load("glRasterPos2fv");
    glad_glRasterPos2i = (PFNGLRASTERPOS2IPROC)load("glRasterPos2i");
    glad_glRasterPos2iv = (PFNGLRASTERPOS2IVPROC)load("glRasterPos2iv");
    glad_glRasterPos2s = (PFNGLRASTERPOS2SPROC)load("glRasterPos2s");
    glad_glRasterPos2sv = (PFNGLRASTERPOS2SVPROC)load("glRasterPos2sv");
    glad_glRasterPos3d = (PFNGLRASTERPOS3DPROC)load("glRasterPos3d");
    glad_glRasterPos3dv = (PFNGLRASTERPOS3DVPROC)load("glRasterPos3dv");
    glad_glRasterPos3f = (PFNGLRASTERPOS3FPROC)load("glRasterPos3f");
    glad_glRasterPos3fv = (PFNGLRASTERPOS3FVPROC)load("glRasterPos3fv");
    glad_glRasterPos3i = (PFNGLRASTERPOS3IPROC)load("glRasterPos3i");
    glad_glRasterPos3iv = (PFNGLRASTERPOS3IVPROC)load("glRasterPos3iv");
    glad_glRasterPos3s = (PFNGLRASTERPOS3SPROC)load("glRasterPos3s");
    glad_glRasterPos3sv = (PFNGLRASTERPOS3SVPROC)load("glRasterPos3sv");
    glad_glRasterPos4d = (PFNGLRASTERPOS4DPROC)load("glRasterPos4d");
    glad_glRasterPos4dv = (PFNGLRASTERPOS4DVPROC)load("glRasterPos4dv");
    glad_glRasterPos4f = (PFNGLRASTERPOS4FPROC)load("glRasterPos4f");
    glad_glRasterPos4fv = (PFNGLRASTERPOS4FVPROC)load("glRasterPos4fv");
    glad_glRasterPos4i = (PFNGLRASTERPOS4IPROC)load("glRasterPos4i");
    glad_glRasterPos4iv = (PFNGLRASTERPOS4IVPROC)load("glRasterPos4iv");
    glad_glRasterPos4s = (PFNGLRASTERPOS4SPROC)load("glRasterPos4s");
    glad_glRasterPos4sv = (PFNGLRASTERPOS4SVPROC)load("glRasterPos4sv");
    glad_glRectd = (PFNGLRECTDPROC)load("glRectd");
    glad_glRectdv = (PFNGLRECTDVPROC)load("glRectdv");
    glad_glRectf = (PFNGLRECTFPROC)load("glRectf");
    glad_glRectfv = (PFNGLRECTFVPROC)load("glRectfv");
    glad_glRecti = (PFNGLRECTIPROC)load("glRecti");
    glad_glRectiv = (PFNGLRECTIVPROC)load("glRectiv");
    glad_glRects = (PFNGLRECTSPROC)load("glRects");
    glad_glRectsv = (PFNGLRECTSVPROC)load("glRectsv");
    glad_glTexCoord1d = (PFNGLTEXCOORD1DPROC)load("glTexCoord1d");
    glad_glTexCoord1dv = (PFNGLTEXCOORD1DVPROC)load("glTexCoord1dv");
    glad_glTexCoord1f = (PFNGLTEXCOORD1FPROC)load("glTexCoord1f");
    glad_glTexCoord1fv = (PFNGLTEXCOORD1FVPROC)load("glTexCoord1fv");
    glad_glTexCoord1i = (PFNGLTEXCOORD1IPROC)load("glTexCoord1i");
    glad_glTexCoord1iv = (PFNGLTEXCOORD1IVPROC)load("glTexCoord1iv");
    glad_glTexCoord1s = (PFNGLTEXCOORD1SPROC)load("glTexCoord1s");
    glad_glTexCoord1sv = (PFNGLTEXCOORD1SVPROC)load("glTexCoord1sv");
    glad_glTexCoord2d = (PFNGLTEXCOORD2DPROC)load("glTexCoord2d");
    glad_glTexCoord2dv = (PFNGLTEXCOORD2DVPROC)load("glTexCoord2dv");
    glad_glTexCoord2f = (PFNGLTEXCOORD2FPROC)load("glTexCoord2f");
    glad_glTexCoord2fv = (PFNGLTEXCOORD2FVPROC)load("glTexCoord2fv");
    glad_glTexCoord2i = (PFNGLTEXCOORD2IPROC)load("glTexCoord2i");
    glad_glTexCoord2iv = (PFNGLTEXCOORD2IVPROC)load("glTexCoord2iv");
    glad_glTexCoord2s = (PFNGLTEXCOORD2SPROC)load("glTexCoord2s");
    glad_glTexCoord2sv = (PFNGLTEXCOORD2SVPROC)load("glTexCoord2sv");
    glad_glTexCoord3d = (PFNGLTEXCOORD3DPROC)load("glTexCoord3d");
    glad_glTexCoord3dv = (PFNGLTEXCOORD3DVPROC)load("glTexCoord3dv");
    glad_glTexCoord3f = (PFNGLTEXCOORD3FPROC)load("glTexCoord3f");
    glad_glTexCoord3fv = (PFNGLTEXCOORD3FVPROC)load("glTexCoord3fv");
    glad_glTexCoord3i = (PFNGLTEXCOORD3IPROC)load("glTexCoord3i");
    glad_glTexCoord3iv = (PFNGLTEXCOORD3IVPROC)load("glTexCoord3iv");
    glad_glTexCoord3s = (PFNGLTEXCOORD3SPROC)load("glTexCoord3s");
    glad_glTexCoord3sv = (PFNGLTEXCOORD3SVPROC)load("glTexCoord3sv");
    glad_glTexCoord4d = (PFNGLTEXCOORD4DPROC)load("glTexCoord4d");
    glad_glTexCoord4dv = (PFNGLTEXCOORD4DVPROC)load("glTexCoord4dv");
    glad_glTexCoord4f = (PFNGLTEXCOORD4FPROC)load("glTexCoord4f");
    glad_glTexCoord4fv = (PFNGLTEXCOORD4FVPROC)load("glTexCoord4fv");
    glad_glTexCoord4i = (PFNGLTEXCOORD4IPROC)load("glTexCoord4i");
    glad_glTexCoord4iv = (PFNGLTEXCOORD4IVPROC)load("glTexCoord4iv");
    glad_glTexCoord4s = (PFNGLTEXCOORD4SPROC)load("glTexCoord4s");
    glad_glTexCoord4sv = (PFNGLTEXCOORD4SVPROC)load("glTexCoord4sv");
    glad_glVertex2d = (PFNGLVERTEX2DPROC)load("glVertex2d");
    glad_glVertex2dv = (PFNGLVERTEX2DVPROC)load("glVertex2dv");
    glad_glVertex2f = (PFNGLVERTEX2FPROC)load("glVertex2f");
    glad_glVertex2fv = (PFNGLVERTEX2FVPROC)load("glVertex2fv");
    glad_glVertex2i = (PFNGLVERTEX2IPROC)load("glVertex2i");
    glad_glVertex2iv = (PFNGLVERTEX2IVPROC)load("glVertex2iv");
    glad_glVertex2s = (PFNGLVERTEX2SPROC)load("glVertex2s");
    glad_glVertex2sv = (PFNGLVERTEX2SVPROC)load("glVertex2sv");
    glad_glVertex3d = (PFNGLVERTEX3DPROC)load("glVertex3d");
    glad_glVertex3dv = (PFNGLVERTEX3DVPROC)load("glVertex3dv");
    glad_glVertex3f = (PFNGLVERTEX3FPROC)load("glVertex3f");
    glad_glVertex3fv = (PFNGLVERTEX3FVPROC)load("glVertex3fv");
    glad_glVertex3i = (PFNGLVERTEX3IPROC)load("glVertex3i");
    glad_glVertex3iv = (PFNGLVERTEX3IVPROC)load("glVertex3iv");
    glad_glVertex3s = (PFNGLVERTEX3SPROC)load("glVertex3s");
    glad_glVertex3sv = (PFNGLVERTEX3SVPROC)load("glVertex3sv");
    glad_glVertex4d = (PFNGLVERTEX4DPROC)load("glVertex4d");
    glad_glVertex4dv = (PFNGLVERTEX4DVPROC)load("glVertex4dv");
    glad_glVertex4f = (PFNGLVERTEX4FPROC)load("glVertex4f");
    glad_glVertex4fv = (PFNGLVERTEX4FVPROC)load("glVertex4fv");
    glad_glVertex4i = (PFNGLVERTEX4IPROC)load("glVertex4i");
    glad_glVertex4iv = (PFNGLVERTEX4IVPROC)load("glVertex4iv");
    glad_glVertex4s = (PFNGLVERTEX4SPROC)load("glVertex4s");
    glad_glVertex4sv = (PFNGLVERTEX4SVPROC)load("glVertex4sv");
    glad_glClipPlane = (PFNGLCLIPPLANEPROC)load("glClipPlane");
    glad_glColorMaterial = (PFNGLCOLORMATERIALPROC)load("glColorMaterial");
    glad_glFogf = (PFNGLFOGFPROC)load("glFogf");
    glad_glFogfv = (PFNGLFOGFVPROC)load("glFogfv");
    glad_glFogi = (PFNGLFOGIPROC)load("glFogi");
    glad_glFogiv = (PFNGLFOGIVPROC)load("glFogiv");
    glad_glLightf = (PFNGLLIGHTFPROC)load("glLightf");
    glad_glLightfv = (PFNGLLIGHTFVPROC)load("glLightfv");
    glad_glLighti = (PFNGLLIGHTIPROC)load("glLighti");
    glad_glLightiv = (PFNGLLIGHTIVPROC)load("glLightiv");
    glad_glLightModelf = (PFNGLLIGHTMODELFPROC)load("glLightModelf");
    glad_glLightModelfv = (PFNGLLIGHTMODELFVPROC)load("glLightModelfv");
    glad_glLightModeli = (PFNGLLIGHTMODELIPROC)load("glLightModeli");
    glad_glLightModeliv = (PFNGLLIGHTMODELIVPROC)load("glLightModeliv");
    glad_glLineStipple = (PFNGLLINESTIPPLEPROC)load("glLineStipple");
    glad_glMaterialf = (PFNGLMATERIALFPROC)load("glMaterialf");
    glad_glMaterialfv = (PFNGLMATERIALFVPROC)load("glMaterialfv");
    glad_glMateriali = (PFNGLMATERIALIPROC)load("glMateriali");
    glad_glMaterialiv = (PFNGLMATERIALIVPROC)load("glMaterialiv");
    glad_glPolygonStipple = (PFNGLPOLYGONSTIPPLEPROC)load("glPolygonStipple");
    glad_glShadeModel = (PFNGLSHADEMODELPROC)load("glShadeModel");
    glad_glTexEnvf = (PFNGLTEXENVFPROC)load("glTexEnvf");
    glad_glTexEnvfv = (PFNGLTEXENVFVPROC)load("glTexEnvfv");
    glad_glTexEnvi = (PFNGLTEXENVIPROC)load("glTexEnvi");
    glad_glTexEnviv = (PFNGLTEXENVIVPROC)load("glTexEnviv");
    glad_glTexGend = (PFNGLTEXGENDPROC)load("glTexGend");
    glad_glTexGendv = (PFNGLTEXGENDVPROC)load("glTexGendv");
    glad_glTexGenf = (PFNGLTEXGENFPROC)load("glTexGenf");
    glad_glTexGenfv = (PFNGLTEXGENFVPROC)load("glTexGenfv");
    glad_glTexGeni = (PFNGLTEXGENIPROC)load("glTexGeni");
    glad_glTexGeniv = (PFNGLTEXGENIVPROC)load("glTexGeniv");
    glad_glFeedbackBuffer = (PFNGLFEEDBACKBUFFERPROC)load("glFeedbackBuffer");
    glad_glSelectBuffer = (PFNGLSELECTBUFFERPROC)load("glSelectBuffer");
    glad_glRenderMode = (PFNGLRENDERMODEPROC)load("glRenderMode");
    glad_glInitNames = (PFNGLINITNAMESPROC)load("glInitNames");
    glad_glLoadName = (PFNGLLOADNAMEPROC)load("glLoadName");
    glad_glPassThrough = (PFNGLPASSTHROUGHPROC)load("glPassThrough");
    glad_glPopName = (PFNGLPOPNAMEPROC)load("glPopName");
    glad_glPushName = (PFNGLPUSHNAMEPROC)load("glPushName");
    glad_glClearAccum = (PFNGLCLEARACCUMPROC)load("glClearAccum");
    glad_glClearIndex = (PFNGLCLEARINDEXPROC)load("glClearIndex");
    glad_glIndexMask = (PFNGLINDEXMASKPROC)load("glIndexMask");
    glad_glAccum = (PFNGLACCUMPROC)load("glAccum");
    glad_glPopAttrib = (PFNGLPOPATTRIBPROC)load("glPopAttrib");
    glad_glPushAttrib = (PFNGLPUSHATTRIBPROC)load("glPushAttrib");
    glad_glMap1d = (PFNGLMAP1DPROC)load("glMap1d");
    glad_glMap1f = (PFNGLMAP1FPROC)load("glMap1f");
    glad_glMap2d = (PFNGLMAP2DPROC)load("glMap2d");
    glad_glMap2f = (PFNGLMAP2FPROC)load("glMap2f");
    glad_glMapGrid1d = (PFNGLMAPGRID1DPROC)load("glMapGrid1d");
    glad_glMapGrid1f = (PFNGLMAPGRID1FPROC)load("glMapGrid1f");
    glad_glMapGrid2d = (PFNGLMAPGRID2DPROC)load("glMapGrid2d");
    glad_glMapGrid2f = (PFNGLMAPGRID2FPROC)load("glMapGrid2f");
    glad_glEvalCoord1d = (PFNGLEVALCOORD1DPROC)load("glEvalCoord1d");
    glad_glEvalCoord1dv = (PFNGLEVALCOORD1DVPROC)load("glEvalCoord1dv");
    glad_glEvalCoord1f = (PFNGLEVALCOORD1FPROC)load("glEvalCoord1f");
    glad_glEvalCoord1fv = (PFNGLEVALCOORD1FVPROC)load("glEvalCoord1fv");
    glad_glEvalCoord2d = (PFNGLEVALCOORD2DPROC)load("glEvalCoord2d");
    glad_glEvalCoord2dv = (PFNGLEVALCOORD2DVPROC)load("glEvalCoord2dv");
    glad_glEvalCoord2f = (PFNGLEVALCOORD2FPROC)load("glEvalCoord2f");
    glad_glEvalCoord2fv = (PFNGLEVALCOORD2FVPROC)load("glEvalCoord2fv");
    glad_glEvalMesh1 = (PFNGLEVALMESH1PROC)load("glEvalMesh1");
    glad_glEvalPoint1 = (PFNGLEVALPOINT1PROC)load("glEvalPoint1");
    glad_glEvalMesh2 = (PFNGLEVALMESH2PROC)load("glEvalMesh2");
    glad_glEvalPoint2 = (PFNGLEVALPOINT2PROC)load("glEvalPoint2");
    glad_glAlphaFunc = (PFNGLALPHAFUNCPROC)load("glAlphaFunc");
    glad_glPixelZoom = (PFNGLPIXELZOOMPROC)load("glPixelZoom");
    glad_glPixelTransferf = (PFNGLPIXELTRANSFERFPROC)load("glPixelTransferf");
    glad_glPixelTransferi = (PFNGLPIXELTRANSFERIPROC)load("glPixelTransferi");
    glad_glPixelMapfv = (PFNGLPIXELMAPFVPROC)load("glPixelMapfv");
    glad_glPixelMapuiv = (PFNGLPIXELMAPUIVPROC)load("glPixelMapuiv");
    glad_glPixelMapusv = (PFNGLPIXELMAPUSVPROC)load("glPixelMapusv");
    glad_glCopyPixels = (PFNGLCOPYPIXELSPROC)load("glCopyPixels");
    glad_glDrawPixels = (PFNGLDRAWPIXELSPROC)load("glDrawPixels");
    glad_glGetClipPlane = (PFNGLGETCLIPPLANEPROC)load("glGetClipPlane");
    glad_glGetLightfv = (PFNGLGETLIGHTFVPROC)load("glGetLightfv");
    glad_glGetLightiv = (PFNGLGETLIGHTIVPROC)load("glGetLightiv");
    glad_glGetMapdv = (PFNGLGETMAPDVPROC)load("glGetMapdv");
    glad_glGetMapfv = (PFNGLGETMAPFVPROC)load("glGetMapfv");
    glad_glGetMapiv = (PFNGLGETMAPIVPROC)load("glGetMapiv");
    glad_glGetMaterialfv = (PFNGLGETMATERIALFVPROC)load("glGetMaterialfv");
    glad_glGetMaterialiv = (PFNGLGETMATERIALIVPROC)load("glGetMaterialiv");
    glad_glGetPixelMapfv = (PFNGLGETPIXELMAPFVPROC)load("glGetPixelMapfv");
    glad_glGetPixelMapuiv = (PFNGLGETPIXELMAPUIVPROC)load("glGetPixelMapuiv");
    glad_glGetPixelMapusv = (PFNGLGETPIXELMAPUSVPROC)load("glGetPixelMapusv");
    glad_glGetPolygonStipple = (PFNGLGETPOLYGONSTIPPLEPROC)load("glGetPolygonStipple");
    glad_glGetTexEnvfv = (PFNGLGETTEXENVFVPROC)load("glGetTexEnvfv");
    glad_glGetTexEnviv = (PFNGLGETTEXENVIVPROC)load("glGetTexEnviv");
    glad_glGetTexGendv = (PFNGLGETTEXGENDVPROC)load("glGetTexGendv");
    glad_glGetTexGenfv = (PFNGLGETTEXGENFVPROC)load("glGetTexGenfv");
    glad_glGetTexGeniv = (PFNGLGETTEXGENIVPROC)load("glGetTexGeniv");
    glad_glIsList = (PFNGLISLISTPROC)load("glIsList");
    glad_glFrustum = (PFNGLFRUSTUMPROC)load("glFrustum");
    glad_glLoadIdentity = (PFNGLLOADIDENTITYPROC)load("glLoadIdentity");
    glad_glLoadMatrixf = (PFNGLLOADMATRIXFPROC)load("glLoadMatrixf");
    glad_glLoadMatrixd = (PFNGLLOADMATRIXDPROC)load("glLoadMatrixd");
    glad_glMatrixMode = (PFNGLMATRIXMODEPROC)load("glMatrixMode");
    glad_glMultMatrixf = (PFNGLMULTMATRIXFPROC)load("glMultMatrixf");
    glad_glMultMatrixd = (PFNGLMULTMATRIXDPROC)load("glMultMatrixd");
    glad_glOrtho = (PFNGLORTHOPROC)load("glOrtho");
    glad_glPopMatrix = (PFNGLPOPMATRIXPROC)load("glPopMatrix");
    glad_glPushMatrix = (PFNGLPUSHMATRIXPROC)load("glPushMatrix");
    glad_glRotated = (PFNGLROTATEDPROC)load("glRotated");
    glad_glRotatef = (PFNGLROTATEFPROC)load("glRotatef");
    glad_glScaled = (PFNGLSCALEDPROC)load("glScaled");
    glad_glScalef = (PFNGLSCALEFPROC)load("glScalef");
    glad_glTranslated = (PFNGLTRANSLATEDPROC)load("glTranslated");
    glad_glTranslatef = (PFNGLTRANSLATEFPROC)load("glTranslatef");
}
static void load_GL_VERSION_1_1(GLADloadproc load) {
    if(!GLAD_GL_VERSION_1_1) return;
    glad_glDrawArrays = (PFNGLDRAWARRAYSPROC)load("glDrawArrays");
    glad_glDrawElements = (PFNGLDRAWELEMENTSPROC)load("glDrawElements");
    glad_glGetPointerv = (PFNGLGETPOINTERVPROC)load("glGetPointerv");
    glad_glPolygonOffset = (PFNGLPOLYGONOFFSETPROC)load("glPolygonOffset");
    glad_glCopyTexImage1D = (PFNGLCOPYTEXIMAGE1DPROC)load("glCopyTexImage1D");
    glad_glCopyTexImage2D = (PFNGLCOPYTEXIMAGE2DPROC)load("glCopyTexImage2D");
    glad_glCopyTexSubImage1D = (PFNGLCOPYTEXSUBIMAGE1DPROC)load("glCopyTexSubImage1D");
    glad_glCopyTexSubImage2D = (PFNGLCOPYTEXSUBIMAGE2DPROC)load("glCopyTexSubImage2D");
    glad_glTexSubImage1D = (PFNGLTEXSUBIMAGE1DPROC)load("glTexSubImage1D");
    glad_glTexSubImage2D = (PFNGLTEXSUBIMAGE2DPROC)load("glTexSubImage2D");
    glad_glBindTexture = (PFNGLBINDTEXTUREPROC)load("glBindTexture");
    glad_glDeleteTextures = (PFNGLDELETETEXTURESPROC)load("glDeleteTextures");
    glad_glGenTextures = (PFNGLGENTEXTURESPROC)load("glGenTextures");
    glad_glIsTexture = (PFNGLISTEXTUREPROC)load("glIsTexture");
    glad_glArrayElement = (PFNGLARRAYELEMENTPROC)load("glArrayElement");
    glad_glColorPointer = (PFNGLCOLORPOINTERPROC)load("glColorPointer");
    glad_glDisableClientState = (PFNGLDISABLECLIENTSTATEPROC)load("glDisableClientState");
    glad_glEdgeFlagPointer = (PFNGLEDGEFLAGPOINTERPROC)load("glEdgeFlagPointer");
    glad_glEnableClientState = (PFNGLENABLECLIENTSTATEPROC)load("glEnableClientState");
    glad_glIndexPointer = (PFNGLINDEXPOINTERPROC)load("glIndexPointer");
    glad_glInterleavedArrays = (PFNGLINTERLEAVEDARRAYSPROC)load("glInterleavedArrays");
    glad_glNormalPointer = (PFNGLNORMALPOINTERPROC)load("glNormalPointer");
    glad_glTexCoordPointer = (PFNGLTEXCOORDPOINTERPROC)load("glTexCoordPointer");
    glad_glVertexPointer = (PFNGLVERTEXPOINTERPROC)load("glVertexPointer");
    glad_glAreTexturesResident = (PFNGLARETEXTURESRESIDENTPROC)load("glAreTexturesResident");
    glad_glPrioritizeTextures = (PFNGLPRIORITIZETEXTURESPROC)load("glPrioritizeTextures");
    glad_glIndexub = (PFNGLINDEXUBPROC)load("glIndexub");
    glad_glIndexubv = (PFNGLINDEXUBVPROC)load("glIndexubv");
    glad_glPopClientAttrib = (PFNGLPOPCLIENTATTRIBPROC)load("glPopClientAttrib");
    glad_glPushClientAttrib = (PFNGLPUSHCLIENTATTRIBPROC)load("glPushClientAttrib");
}
static void load_GL_VERSION_1_2(GLADloadproc load) {
    if(!GLAD_GL_VERSION_1_2) return;
    glad_glDrawRangeElements = (PFNGLDRAWRANGEELEMENTSPROC)load("glDrawRangeElements");
    glad_glTexImage3D = (PFNGLTEXIMAGE3DPROC)load("glTexImage3D");
    glad_glTexSubImage3D = (PFNGLTEXSUBIMAGE3DPROC)load("glTexSubImage3D");
    glad_glCopyTexSubImage3D = (PFNGLCOPYTEXSUBIMAGE3DPROC)load("glCopyTexSubImage3D");
}
static void load_GL_VERSION_1_3(GLADloadproc load) {
    if(!GLAD_GL_VERSION_1_3) return;
    glad_glActiveTexture = (PFNGLACTIVETEXTUREPROC)load("glActiveTexture");
    glad_glSampleCoverage = (PFNGLSAMPLECOVERAGEPROC)load("glSampleCoverage");
    glad_glCompressedTexImage3D = (PFNGLCOMPRESSEDTEXIMAGE3DPROC)load("glCompressedTexImage3D");
    glad_glCompressedTexImage2D = (PFNGLCOMPRESSEDTEXIMAGE2DPROC)load("glCompressedTexImage2D");
    glad_glCompressedTexImage1D = (PFNGLCOMPRESSEDTEXIMAGE1DPROC)load("glCompressedTexImage1D");
    glad_glCompressedTexSubImage3D = (PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC)load("glCompressedTexSubImage3D");
    glad_glCompressedTexSubImage2D = (PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC)load("glCompressedTexSubImage2D");
    glad_glCompressedTexSubImage1D = (PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC)load("glCompressedTexSubImage1D");
    glad_glGetCompressedTexImage = (PFNGLGETCOMPRESSEDTEXIMAGEPROC)load("glGetCompressedTexImage");
    glad_glClientActiveTexture = (PFNGLCLIENTACTIVETEXTUREPROC)load("glClientActiveTexture");
    glad_glMultiTexCoord1d = (PFNGLMULTITEXCOORD1DPROC)load("glMultiTexCoord1d");
    glad_glMultiTexCoord1dv = (PFNGLMULTITEXCOORD1DVPROC)load("glMultiTexCoord1dv");
    glad_glMultiTexCoord1f = (PFNGLMULTITEXCOORD1FPROC)load("glMultiTexCoord1f");
    glad_glMultiTexCoord1fv = (PFNGLMULTITEXCOORD1FVPROC)load("glMultiTexCoord1fv");
    glad_glMultiTexCoord1i = (PFNGLMULTITEXCOORD1IPROC)load("glMultiTexCoord1i");
    glad_glMultiTexCoord1iv = (PFNGLMULTITEXCOORD1IVPROC)load("glMultiTexCoord1iv");
    glad_glMultiTexCoord1s = (PFNGLMULTITEXCOORD1SPROC)load("glMultiTexCoord1s");
    glad_glMultiTexCoord1sv = (PFNGLMULTITEXCOORD1SVPROC)load("glMultiTexCoord1sv");
    glad_glMultiTexCoord2d = (PFNGLMULTITEXCOORD2DPROC)load("glMultiTexCoord2d");
    glad_glMultiTexCoord2dv = (PFNGLMULTITEXCOORD2DVPROC)load("glMultiTexCoord2dv");
    glad_glMultiTexCoord2f = (PFNGLMULTITEXCOORD2FPROC)load("glMultiTexCoord2f");
    glad_glMultiTexCoord2fv = (PFNGLMULTITEXCOORD2FVPROC)load("glMultiTexCoord2fv");
    glad_glMultiTexCoord2i = (PFNGLMULTITEXCOORD2IPROC)load("glMultiTexCoord2i");
    glad_glMultiTexCoord2iv = (PFNGLMULTITEXCOORD2IVPROC)load("glMultiTexCoord2iv");
    glad_glMultiTexCoord2s = (PFNGLMULTITEXCOORD2SPROC)load("glMultiTexCoord2s");
    glad_glMultiTexCoord2sv = (PFNGLMULTITEXCOORD2SVPROC)load("glMultiTexCoord2sv");
    glad_glMultiTexCoord3d = (PFNGLMULTITEXCOORD3DPROC)load("glMultiTexCoord3d");
    glad_glMultiTexCoord3dv = (PFNGLMULTITEXCOORD3DVPROC)load("glMultiTexCoord3dv");
    glad_glMultiTexCoord3f = (PFNGLMULTITEXCOORD3FPROC)load("glMultiTexCoord3f");
    glad_glMultiTexCoord3fv = (PFNGLMULTITEXCOORD3FVPROC)load("glMultiTexCoord3fv");
    glad_glMultiTexCoord3i = (PFNGLMULTITEXCOORD3IPROC)load("glMultiTexCoord3i");
    glad_glMultiTexCoord3iv = (PFNGLMULTITEXCOORD3IVPROC)load("glMultiTexCoord3iv");
    glad_glMultiTexCoord3s = (PFNGLMULTITEXCOORD3SPROC)load("glMultiTexCoord3s");
    glad_glMultiTexCoord3sv = (PFNGLMULTITEXCOORD3SVPROC)load("glMultiTexCoord3sv");
    glad_glMultiTexCoord4d = (PFNGLMULTITEXCOORD4DPROC)load("glMultiTexCoord4d");
    glad_glMultiTexCoord4dv = (PFNGLMULTITEXCOORD4DVPROC)load("glMultiTexCoord4dv");
    glad_glMultiTexCoord4f = (PFNGLMULTITEXCOORD4FPROC)load("glMultiTexCoord4f");
    glad_glMultiTexCoord4fv = (PFNGLMULTITEXCOORD4FVPROC)load("glMultiTexCoord4fv");
    glad_glMultiTexCoord4i = (PFNGLMULTITEXCOORD4IPROC)load("glMultiTexCoord4i");
    glad_glMultiTexCoord4iv = (PFNGLMULTITEXCOORD4IVPROC)load("glMultiTexCoord4iv");
    glad_glMultiTexCoord4s = (PFNGLMULTITEXCOORD4SPROC)load("glMultiTexCoord4s");
    glad_glMultiTexCoord4sv = (PFNGLMULTITEXCOORD4SVPROC)load("glMultiTexCoord4sv");
    glad_glLoadTransposeMatrixf = (PFNGLLOADTRANSPOSEMATRIXFPROC)load("glLoadTransposeMatrixf");
    glad_glLoadTransposeMatrixd = (PFNGLLOADTRANSPOSEMATRIXDPROC)load("glLoadTransposeMatrixd");
    glad_glMultTransposeMatrixf = (PFNGLMULTTRANSPOSEMATRIXFPROC)load("glMultTransposeMatrixf");
    glad_glMultTransposeMatrixd = (PFNGLMULTTRANSPOSEMATRIXDPROC)load("glMultTransposeMatrixd");
}
static void load_GL_VERSION_1_4(GLADloadproc load) {
    if(!GLAD_GL_VERSION_1_4) return;
    glad_glBlendFuncSeparate = (PFNGLBLENDFUNCSEPARATEPROC)load("glBlendFuncSeparate");
    glad_glMultiDrawArrays = (PFNGLMULTIDRAWARRAYSPROC)load("glMultiDrawArrays");
    glad_glMultiDrawElements = (PFNGLMULTIDRAWELEMENTSPROC)load("glMultiDrawElements");
    glad_glPointParameterf = (PFNGLPOINTPARAMETERFPROC)load("glPointParameterf");
    glad_glPointParameterfv = (PFNGLPOINTPARAMETERFVPROC)load("glPointParameterfv");
    glad_glPointParameteri = (PFNGLPOINTPARAMETERIPROC)load("glPointParameteri");
    glad_glPointParameteriv = (PFNGLPOINTPARAMETERIVPROC)load("glPointParameteriv");
    glad_glFogCoordf = (PFNGLFOGCOORDFPROC)load("glFogCoordf");
    glad_glFogCoordfv = (PFNGLFOGCOORDFVPROC)load("glFogCoordfv");
    glad_glFogCoordd = (PFNGLFOGCOORDDPROC)load("glFogCoordd");
    glad_glFogCoorddv = (PFNGLFOGCOORDDVPROC)load("glFogCoorddv");
    glad_glFogCoordPointer = (PFNGLFOGCOORDPOINTERPROC)load("glFogCoordPointer");
    glad_glSecondaryColor3b = (PFNGLSECONDARYCOLOR3BPROC)load("glSecondaryColor3b");
    glad_glSecondaryColor3bv = (PFNGLSECONDARYCOLOR3BVPROC)load("glSecondaryColor3bv");
    glad_glSecondaryColor3d = (PFNGLSECONDARYCOLOR3DPROC)load("glSecondaryColor3d");
    glad_glSecondaryColor3dv = (PFNGLSECONDARYCOLOR3DVPROC)load("glSecondaryColor3dv");
    glad_glSecondaryColor3f = (PFNGLSECONDARYCOLOR3FPROC)load("glSecondaryColor3f");
    glad_glSecondaryColor3fv = (PFNGLSECONDARYCOLOR3FVPROC)load("glSecondaryColor3fv");
    glad_glSecondaryColor3i = (PFNGLSECONDARYCOLOR3IPROC)load("glSecondaryColor3i");
    glad_glSecondaryColor3iv = (PFNGLSECONDARYCOLOR3IVPROC)load("glSecondaryColor3iv");
    glad_glSecondaryColor3s = (PFNGLSECONDARYCOLOR3SPROC)load("glSecondaryColor3s");
    glad_glSecondaryColor3sv = (PFNGLSECONDARYCOLOR3SVPROC)load("glSecondaryColor3sv");
    glad_glSecondaryColor3ub = (PFNGLSECONDARYCOLOR3UBPROC)load("glSecondaryColor3ub");
    glad_glSecondaryColor3ubv = (PFNGLSECONDARYCOLOR3UBVPROC)load("glSecondaryColor3ubv");
    glad_glSecondaryColor3ui = (PFNGLSECONDARYCOLOR3UIPROC)load("glSecondaryColor3ui");
    glad_glSecondaryColor3uiv = (PFNGLSECONDARYCOLOR3UIVPROC)load("glSecondaryColor3uiv");
    glad_glSecondaryColor3us = (PFNGLSECONDARYCOLOR3USPROC)load("glSecondaryColor3us");
    glad_glSecondaryColor3usv = (PFNGLSECONDARYCOLOR3USVPROC)load("glSecondaryColor3usv");
    glad_glSecondaryColorPointer = (PFNGLSECONDARYCOLORPOINTERPROC)load("glSecondaryColorPointer");
    glad_glWindowPos2d = (PFNGLWINDOWPOS2DPROC)load("glWindowPos2d");
    glad_glWindowPos2dv = (PFNGLWINDOWPOS2DVPROC)load("glWindowPos2dv");
    glad_glWindowPos2f = (PFNGLWINDOWPOS2FPROC)load("glWindowPos2f");
    glad_glWindowPos2fv = (PFNGLWINDOWPOS2FVPROC)load("glWindowPos2fv");
    glad_glWindowPos2i = (PFNGLWINDOWPOS2IPROC)load("glWindowPos2i");
    glad_glWindowPos2iv = (PFNGLWINDOWPOS2IVPROC)load("glWindowPos2iv");
    glad_glWindowPos2s = (PFNGLWINDOWPOS2SPROC)load("glWindowPos2s");
    glad_glWindowPos2sv = (PFNGLWINDOWPOS2SVPROC)load("glWindowPos2sv");
    glad_glWindowPos3d = (PFNGLWINDOWPOS3DPROC)load("glWindowPos3d");
    glad_glWindowPos3dv = (PFNGLWINDOWPOS3DVPROC)load("glWindowPos3dv");
    glad_glWindowPos3f = (PFNGLWINDOWPOS3FPROC)load("glWindowPos3f");
    glad_glWindowPos3fv = (PFNGLWINDOWPOS3FVPROC)load("glWindowPos3fv");
    glad_glWindowPos3i = (PFNGLWINDOWPOS3IPROC)load("glWindowPos3i");
    glad_glWindowPos3iv = (PFNGLWINDOWPOS3IVPROC)load("glWindowPos3iv");
    glad_glWindowPos3s = (PFNGLWINDOWPOS3SPROC)load("glWindowPos3s");
    glad_glWindowPos3sv = (PFNGLWINDOWPOS3SVPROC)load("glWindowPos3sv");
    glad_glBlendColor = (PFNGLBLENDCOLORPROC)load("glBlendColor");
    glad_glBlendEquation = (PFNGLBLENDEQUATIONPROC)load("glBlendEquation");
}
static void load_GL_VERSION_1_5(GLADloadproc load) {
    if(!GLAD_GL_VERSION_1_5) return;
    glad_glGenQueries = (PFNGLGENQUERIESPROC)load("glGenQueries");
    glad_glDeleteQueries = (PFNGLDELETEQUERIESPROC)load("glDeleteQueries");
    glad_glIsQuery = (PFNGLISQUERYPROC)load("glIsQuery");
    glad_glBeginQuery = (PFNGLBEGINQUERYPROC)load("glBeginQuery");
    glad_glEndQuery = (PFNGLENDQUERYPROC)load("glEndQuery");
    glad_glGetQueryiv = (PFNGLGETQUERYIVPROC)load("glGetQueryiv");
    glad_glGetQueryObjectiv = (PFNGLGETQUERYOBJECTIVPROC)load("glGetQueryObjectiv");
    glad_glGetQueryObjectuiv = (PFNGLGETQUERYOBJECTUIVPROC)load("glGetQueryObjectuiv");
    glad_glBindBuffer = (PFNGLBINDBUFFERPROC)load("glBindBuffer");
    glad_glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)load("glDeleteBuffers");
    glad_glGenBuffers = (PFNGLGENBUFFERSPROC)load("glGenBuffers");
    glad_glIsBuffer = (PFNGLISBUFFERPROC)load("glIsBuffer");
    glad_glBufferData = (PFNGLBUFFERDATAPROC)load("glBufferData");
    glad_glBufferSubData = (PFNGLBUFFERSUBDATAPROC)load("glBufferSubData");
    glad_glGetBufferSubData = (PFNGLGETBUFFERSUBDATAPROC)load("glGetBufferSubData");
    glad_glMapBuffer = (PFNGLMAPBUFFERPROC)load("glMapBuffer");
    glad_glUnmapBuffer = (PFNGLUNMAPBUFFERPROC)load("glUnmapBuffer");
    glad_glGetBufferParameteriv = (PFNGLGETBUFFERPARAMETERIVPROC)load("glGetBufferParameteriv");
    glad_glGetBufferPointerv = (PFNGLGETBUFFERPOINTERVPROC)load("glGetBufferPointerv");
}
static void load_GL_VERSION_2_0(GLADloadproc load) {
    if(!GLAD_GL_VERSION_2_0) return;
    glad_glBlendEquationSeparate = (PFNGLBLENDEQUATIONSEPARATEPROC)load("glBlendEquationSeparate");
    glad_glDrawBuffers = (PFNGLDRAWBUFFERSPROC)load("glDrawBuffers");
    glad_glStencilOpSeparate = (PFNGLSTENCILOPSEPARATEPROC)load("glStencilOpSeparate");
    glad_glStencilFuncSeparate = (PFNGLSTENCILFUNCSEPARATEPROC)load("glStencilFuncSeparate");
    glad_glStencilMaskSeparate = (PFNGLSTENCILMASKSEPARATEPROC)load("glStencilMaskSeparate");
    glad_glAttachShader = (PFNGLATTACHSHADERPROC)load("glAttachShader");
    glad_glBindAttribLocation = (PFNGLBINDATTRIBLOCATIONPROC)load("glBindAttribLocation");
    glad_glCompileShader = (PFNGLCOMPILESHADERPROC)load("glCompileShader");
    glad_glCreateProgram = (PFNGLCREATEPROGRAMPROC)load("glCreateProgram");
    glad_glCreateShader = (PFNGLCREATESHADERPROC)load("glCreateShader");
    glad_glDeleteProgram = (PFNGLDELETEPROGRAMPROC)load("glDeleteProgram");
    glad_glDeleteShader = (PFNGLDELETESHADERPROC)load("glDeleteShader");
    glad_glDetachShader = (PFNGLDETACHSHADERPROC)load("glDetachShader");
    glad_glDisableVertexAttribArray = (PFNGLDISABLEVERTEXATTRIBARRAYPROC)load("glDisableVertexAttribArray");
    glad_glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)load("glEnableVertexAttribArray");
    glad_glGetActiveAttrib = (PFNGLGETACTIVEATTRIBPROC)load("glGetActiveAttrib");
    glad_glGetActiveUniform = (PFNGLGETACTIVEUNIFORMPROC)load("glGetActiveUniform");
    glad_glGetAttachedShaders = (PFNGLGETATTACHEDSHADERSPROC)load("glGetAttachedShaders");
    glad_glGetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC)load("glGetAttribLocation");
    glad_glGetProgramiv = (PFNGLGETPROGRAMIVPROC)load("glGetProgramiv");
    glad_glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)load("glGetProgramInfoLog");
    glad_glGetShaderiv = (PFNGLGETSHADERIVPROC)load("glGetShaderiv");
    glad_glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)load("glGetShaderInfoLog");
    glad_glGetShaderSource = (PFNGLGETSHADERSOURCEPROC)load("glGetShaderSource");
    glad_glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)load("glGetUniformLocation");
    glad_glGetUniformfv = (PFNGLGETUNIFORMFVPROC)load("glGetUniformfv");
    glad_glGetUniformiv = (PFNGLGETUNIFORMIVPROC)load("glGetUniformiv");
    glad_glGetVertexAttribdv = (PFNGLGETVERTEXATTRIBDVPROC)load("glGetVertexAttribdv");
    glad_glGetVertexAttribfv = (PFNGLGETVERTEXATTRIBFVPROC)load("glGetVertexAttribfv");
    glad_glGetVertexAttribiv = (PFNGLGETVERTEXATTRIBIVPROC)load("glGetVertexAttribiv");
    glad_glGetVertexAttribPointerv = (PFNGLGETVERTEXATTRIBPOINTERVPROC)load("glGetVertexAttribPointerv");
    glad_glIsProgram = (PFNGLISPROGRAMPROC)load("glIsProgram");
    glad_glIsShader = (PFNGLISSHADERPROC)load("glIsShader");
    glad_glLinkProgram = (PFNGLLINKPROGRAMPROC)load("glLinkProgram");
    glad_glShaderSource = (PFNGLSHADERSOURCEPROC)load("glShaderSource");
    glad_glUseProgram = (PFNGLUSEPROGRAMPROC)load("glUseProgram");
    glad_glUniform1f = (PFNGLUNIFORM1FPROC)load("glUniform1f");
    glad_glUniform2f = (PFNGLUNIFORM2FPROC)load("glUniform2f");
    glad_glUniform3f = (PFNGLUNIFORM3FPROC)load("glUniform3f");
    glad_glUniform4f = (PFNGLUNIFORM4FPROC)load("glUniform4f");
    glad_glUniform1i = (PFNGLUNIFORM1IPROC)load("glUniform1i");
    glad_glUniform2i = (PFNGLUNIFORM2IPROC)load("glUniform2i");
    glad_glUniform3i = (PFNGLUNIFORM3IPROC)load("glUniform3i");
    glad_glUniform4i = (PFNGLUNIFORM4IPROC)load("glUniform4i");
    glad_glUniform1fv = (PFNGLUNIFORM1FVPROC)load("glUniform1fv");
    glad_glUniform2fv = (PFNGLUNIFORM2FVPROC)load("glUniform2fv");
    glad_glUniform3fv = (PFNGLUNIFORM3FVPROC)load("glUniform3fv");
    glad_glUniform4fv = (PFNGLUNIFORM4FVPROC)load("glUniform4fv");
    glad_glUniform1iv = (PFNGLUNIFORM1IVPROC)load("glUniform1iv");
    glad_glUniform2iv = (PFNGLUNIFORM2IVPROC)load("glUniform2iv");
    glad_glUniform3iv = (PFNGLUNIFORM3IVPROC)load("glUniform3iv");
    glad_glUniform4iv = (PFNGLUNIFORM4IVPROC)load("glUniform4iv");
    glad_glUniformMatrix2fv = (PFNGLUNIFORMMATRIX2FVPROC)load("glUniformMatrix2fv");
    glad_glUniformMatrix3fv = (PFNGLUNIFORMMATRIX3FVPROC)load("glUniformMatrix3fv");
    glad_glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC)load("glUniformMatrix4fv");
    glad_glValidateProgram = (PFNGLVALIDATEPROGRAMPROC)load("glValidateProgram");
    glad_glVertexAttrib1d = (PFNGLVERTEXATTRIB1DPROC)load("glVertexAttrib1d");
    glad_glVertexAttrib1dv = (PFNGLVERTEXATTRIB1DVPROC)load("glVertexAttrib1dv");
    glad_glVertexAttrib1f = (PFNGLVERTEXATTRIB1FPROC)load("glVertexAttrib1f");
    glad_glVertexAttrib1fv = (PFNGLVERTEXATTRIB1FVPROC)load("glVertexAttrib1fv");
    glad_glVertexAttrib1s = (PFNGLVERTEXATTRIB1SPROC)load("glVertexAttrib1s");
    glad_glVertexAttrib1sv = (PFNGLVERTEXATTRIB1SVPROC)load("glVertexAttrib1sv");
    glad_glVertexAttrib2d = (PFNGLVERTEXATTRIB2DPROC)load("glVertexAttrib2d");
    glad_glVertexAttrib2dv = (PFNGLVERTEXATTRIB2DVPROC)load("glVertexAttrib2dv");
    glad_glVertexAttrib2f = (PFNGLVERTEXATTRIB2FPROC)load("glVertexAttrib2f");
    glad_glVertexAttrib2fv = (PFNGLVERTEXATTRIB2FVPROC)load("glVertexAttrib2fv");
    glad_glVertexAttrib2s = (PFNGLVERTEXATTRIB2SPROC)load("glVertexAttrib2s");
    glad_glVertexAttrib2sv = (PFNGLVERTEXATTRIB2SVPROC)load("glVertexAttrib2sv");
    glad_glVertexAttrib3d = (PFNGLVERTEXATTRIB3DPROC)load("glVertexAttrib3d");
    glad_glVertexAttrib3dv = (PFNGLVERTEXATTRIB3DVPROC)load("glVertexAttrib3dv");
    glad_glVertexAttrib3f = (PFNGLVERTEXATTRIB3FPROC)load("glVertexAttrib3f");
    glad_glVertexAttrib3fv = (PFNGLVERTEXATTRIB3FVPROC)load("glVertexAttrib3fv");
    glad_glVertexAttrib3s = (PFNGLVERTEXATTRIB3SPROC)load("glVertexAttrib3s");
    glad_glVertexAttrib3sv = (PFNGLVERTEXATTRIB3SVPROC)load("glVertexAttrib3sv");
    glad_glVertexAttrib4Nbv = (PFNGLVERTEXATTRIB4NBVPROC)load("glVertexAttrib4Nbv");
    glad_glVertexAttrib4Niv = (PFNGLVERTEXATTRIB4NIVPROC)load("glVertexAttrib4Niv");
    glad_glVertexAttrib4Nsv = (PFNGLVERTEXATTRIB4NSVPROC)load("glVertexAttrib4Nsv");
    glad_glVertexAttrib4Nub = (PFNGLVERTEXATTRIB4NUBPROC)load("glVertexAttrib4Nub");
    glad_glVertexAttrib4Nubv = (PFNGLVERTEXATTRIB4NUBVPROC)load("glVertexAttrib4Nubv");
    glad_glVertexAttrib4Nuiv = (PFNGLVERTEXATTRIB4NUIVPROC)load("glVertexAttrib4Nuiv");
    glad_glVertexAttrib4Nusv = (PFNGLVERTEXATTRIB4NUSVPROC)load("glVertexAttrib4Nusv");
    glad_glVertexAttrib4bv = (PFNGLVERTEXATTRIB4BVPROC)load("glVertexAttrib4bv");
    glad_glVertexAttrib4d = (PFNGLVERTEXATTRIB4DPROC)load("glVertexAttrib4d");
    glad_glVertexAttrib4dv = (PFNGLVERTEXATTRIB4DVPROC)load("glVertexAttrib4dv");
    glad_glVertexAttrib4f = (PFNGLVERTEXATTRIB4FPROC)load("glVertexAttrib4f");
    glad_glVertexAttrib4fv = (PFNGLVERTEXATTRIB4FVPROC)load("glVertexAttrib4fv");
    glad_glVertexAttrib4iv = (PFNGLVERTEXATTRIB4IVPROC)load("glVertexAttrib4iv");
    glad_glVertexAttrib4s = (PFNGLVERTEXATTRIB4SPROC)load("glVertexAttrib4s");
    glad_glVertexAttrib4sv = (PFNGLVERTEXATTRIB4SVPROC)load("glVertexAttrib4sv");
    glad_glVertexAttrib4ubv = (PFNGLVERTEXATTRIB4UBVPROC)load("glVertexAttrib4ubv");
    glad_glVertexAttrib4uiv = (PFNGLVERTEXATTRIB4UIVPROC)load("glVertexAttrib4uiv");
    glad_glVertexAttrib4usv = (PFNGLVERTEXATTRIB4USVPROC)load("glVertexAttrib4usv");
    glad_glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)load("glVertexAttribPointer");
}
static void load_GL_VERSION_2_1(GLADloadproc load) {
    if(!GLAD_GL_VERSION_2_1) return;
    glad_glUniformMatrix2x3fv = (PFNGLUNIFORMMATRIX2X3FVPROC)load("glUniformMatrix2x3fv");
    glad_glUniformMatrix3x2fv = (PFNGLUNIFORMMATRIX3X2FVPROC)load("glUniformMatrix3x2fv");
    glad_glUniformMatrix2x4fv = (PFNGLUNIFORMMATRIX2X4FVPROC)load("glUniformMatrix2x4fv");
    glad_glUniformMatrix4x2fv = (PFNGLUNIFORMMATRIX4X2FVPROC)load("glUniformMatrix4x2fv");
    glad_glUniformMatrix3x4fv = (PFNGLUNIFORMMATRIX3X4FVPROC)load("glUniformMatrix3x4fv");
    glad_glUniformMatrix4x3fv = (PFNGLUNIFORMMATRIX4X3FVPROC)load("glUniformMatrix4x3fv");
}
static void load_GL_VERSION_3_0(GLADloadproc load) {
    if(!GLAD_GL_VERSION_3_0) return;
    glad_glColorMaski = (PFNGLCOLORMASKIPROC)load("glColorMaski");
    glad_glGetBooleani_v = (PFNGLGETBOOLEANI_VPROC)load("glGetBooleani_v");
    glad_glGetIntegeri_v = (PFNGLGETINTEGERI_VPROC)load("glGetIntegeri_v");
    glad_glEnablei = (PFNGLENABLEIPROC)load("glEnablei");
    glad_glDisablei = (PFNGLDISABLEIPROC)load("glDisablei");
    glad_glIsEnabledi = (PFNGLISENABLEDIPROC)load("glIsEnabledi");
    glad_glBeginTransformFeedback = (PFNGLBEGINTRANSFORMFEEDBACKPROC)load("glBeginTransformFeedback");
    glad_glEndTransformFeedback = (PFNGLENDTRANSFORMFEEDBACKPROC)load("glEndTransformFeedback");
    glad_glBindBufferRange = (PFNGLBINDBUFFERRANGEPROC)load("glBindBufferRange");
    glad_glBindBufferBase = (PFNGLBINDBUFFERBASEPROC)load("glBindBufferBase");
    glad_glTransformFeedbackVaryings = (PFNGLTRANSFORMFEEDBACKVARYINGSPROC)load("glTransformFeedbackVaryings");
    glad_glGetTransformFeedbackVarying = (PFNGLGETTRANSFORMFEEDBACKVARYINGPROC)load("glGetTransformFeedbackVarying");
    glad_glClampColor = (PFNGLCLAMPCOLORPROC)load("glClampColor");
    glad_glBeginConditionalRender = (PFNGLBEGINCONDITIONALRENDERPROC)load("glBeginConditionalRender");
    glad_glEndConditionalRender = (PFNGLENDCONDITIONALRENDERPROC)load("glEndConditionalRender");
    glad_glVertexAttribIPointer = (PFNGLVERTEXATTRIBIPOINTERPROC)load("glVertexAttribIPointer");
    glad_glGetVertexAttribIiv = (PFNGLGETVERTEXATTRIBIIVPROC)load("glGetVertexAttribIiv");
    glad_glGetVertexAttribIuiv = (PFNGLGETVERTEXATTRIBIUIVPROC)load("glGetVertexAttribIuiv");
    glad_glVertexAttribI1i = (PFNGLVERTEXATTRIBI1IPROC)load("glVertexAttribI1i");
    glad_glVertexAttribI2i = (PFNGLVERTEXATTRIBI2IPROC)load("glVertexAttribI2i");
    glad_glVertexAttribI3i = (PFNGLVERTEXATTRIBI3IPROC)load("glVertexAttribI3i");
    glad_glVertexAttribI4i = (PFNGLVERTEXATTRIBI4IPROC)load("glVertexAttribI4i");
    glad_glVertexAttribI1ui = (PFNGLVERTEXATTRIBI1UIPROC)load("glVertexAttribI1ui");
    glad_glVertexAttribI2ui = (PFNGLVERTEXATTRIBI2UIPROC)load("glVertexAttribI2ui");
    glad_glVertexAttribI3ui = (PFNGLVERTEXATTRIBI3UIPROC)load("glVertexAttribI3ui");
    glad_glVertexAttribI4ui = (PFNGLVERTEXATTRIBI4UIPROC)load("glVertexAttribI4ui");
    glad_glVertexAttribI1iv = (PFNGLVERTEXATTRIBI1IVPROC)load("glVertexAttribI1iv");
    glad_glVertexAttribI2iv = (PFNGLVERTEXATTRIBI2IVPROC)load("glVertexAttribI2iv");
    glad_glVertexAttribI3iv = (PFNGLVERTEXATTRIBI3IVPROC)load("glVertexAttribI3iv");
    glad_glVertexAttribI4iv = (PFNGLVERTEXATTRIBI4IVPROC)load("glVertexAttribI4iv");
    glad_glVertexAttribI1uiv = (PFNGLVERTEXATTRIBI1UIVPROC)load("glVertexAttribI1uiv");
    glad_glVertexAttribI2uiv = (PFNGLVERTEXATTRIBI2UIVPROC)load("glVertexAttribI2uiv");
    glad_glVertexAttribI3uiv = (PFNGLVERTEXATTRIBI3UIVPROC)load("glVertexAttribI3uiv");
    glad_glVertexAttribI4uiv = (PFNGLVERTEXATTRIBI4UIVPROC)load("glVertexAttribI4uiv");
    glad_glVertexAttribI4bv = (PFNGLVERTEXATTRIBI4BVPROC)load("glVertexAttribI4bv");
    glad_glVertexAttribI4sv = (PFNGLVERTEXATTRIBI4SVPROC)load("glVertexAttribI4sv");
    glad_glVertexAttribI4ubv = (PFNGLVERTEXATTRIBI4UBVPROC)load("glVertexAttribI4ubv");
    glad_glVertexAttribI4usv = (PFNGLVERTEXATTRIBI4USVPROC)load("glVertexAttribI4usv");
    glad_glGetUniformuiv = (PFNGLGETUNIFORMUIVPROC)load("glGetUniformuiv");
    glad_glBindFragDataLocation = (PFNGLBINDFRAGDATALOCATIONPROC)load("glBindFragDataLocation");
    glad_glGetFragDataLocation = (PFNGLGETFRAGDATALOCATIONPROC)load("glGetFragDataLocation");
    glad_glUniform1ui = (PFNGLUNIFORM1UIPROC)load("glUniform1ui");
    glad_glUniform2ui = (PFNGLUNIFORM2UIPROC)load("glUniform2ui");
    glad_glUniform3ui = (PFNGLUNIFORM3UIPROC)load("glUniform3ui");
    glad_glUniform4ui = (PFNGLUNIFORM4UIPROC)load("glUniform4ui");
    glad_glUniform1uiv = (PFNGLUNIFORM1UIVPROC)load("glUniform1uiv");
    glad_glUniform2uiv = (PFNGLUNIFORM2UIVPROC)load("glUniform2uiv");
    glad_glUniform3uiv = (PFNGLUNIFORM3UIVPROC)load("glUniform3uiv");
    glad_glUniform4uiv = (PFNGLUNIFORM4UIVPROC)load("glUniform4uiv");
    glad_glTexParameterIiv = (PFNGLTEXPARAMETERIIVPROC)load("glTexParameterIiv");
    glad_glTexParameterIuiv = (PFNGLTEXPARAMETERIUIVPROC)load("glTexParameterIuiv");
    glad_glGetTexParameterIiv = (PFNGLGETTEXPARAMETERIIVPROC)load("glGetTexParameterIiv");
    glad_glGetTexParameterIuiv = (PFNGLGETTEXPARAMETERIUIVPROC)load("glGetTexParameterIuiv");
    glad_glClearBufferiv = (PFNGLCLEARBUFFERIVPROC)load("glClearBufferiv");
    glad_glClearBufferuiv = (PFNGLCLEARBUFFERUIVPROC)load("glClearBufferuiv");
    glad_glClearBufferfv = (PFNGLCLEARBUFFERFVPROC)load("glClearBufferfv");
    glad_glClearBufferfi = (PFNGLCLEARBUFFERFIPROC)load("glClearBufferfi");
    glad_glGetStringi = (PFNGLGETSTRINGIPROC)load("glGetStringi");
    glad_glIsRenderbuffer = (PFNGLISRENDERBUFFERPROC)load("glIsRenderbuffer");
    glad_glBindRenderbuffer = (PFNGLBINDRENDERBUFFERPROC)load("glBindRenderbuffer");
    glad_glDeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSPROC)load("glDeleteRenderbuffers");
    glad_glGenRenderbuffers = (PFNGLGENRENDERBUFFERSPROC)load("glGenRenderbuffers");
    glad_glRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC)load("glRenderbufferStorage");
    glad_glGetRenderbufferParameteriv = (PFNGLGETRENDERBUFFERPARAMETERIVPROC)load("glGetRenderbufferParameteriv");
    glad_glIsFramebuffer = (PFNGLISFRAMEBUFFERPROC)load("glIsFramebuffer");
    glad_glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)load("glBindFramebuffer");
    glad_glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)load("glDeleteFramebuffers");
    glad_glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)load("glGenFramebuffers");
    glad_glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)load("glCheckFramebufferStatus");
    glad_glFramebufferTexture1D = (PFNGLFRAMEBUFFERTEXTURE1DPROC)load("glFramebufferTexture1D");
    glad_glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)load("glFramebufferTexture2D");
    glad_glFramebufferTexture3D = (PFNGLFRAMEBUFFERTEXTURE3DPROC)load("glFramebufferTexture3D");
    glad_glFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFERPROC)load("glFramebufferRenderbuffer");
    glad_glGetFramebufferAttachmentParameteriv = (PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC)load("glGetFramebufferAttachmentParameteriv");
    glad_glGenerateMipmap = (PFNGLGENERATEMIPMAPPROC)load("glGenerateMipmap");
    glad_glBlitFramebuffer = (PFNGLBLITFRAMEBUFFERPROC)load("glBlitFramebuffer");
    glad_glRenderbufferStorageMultisample = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC)load("glRenderbufferStorageMultisample");
    glad_glFramebufferTextureLayer = (PFNGLFRAMEBUFFERTEXTURELAYERPROC)load("glFramebufferTextureLayer");
    glad_glMapBufferRange = (PFNGLMAPBUFFERRANGEPROC)load("glMapBufferRange");
    glad_glFlushMappedBufferRange = (PFNGLFLUSHMAPPEDBUFFERRANGEPROC)load("glFlushMappedBufferRange");
    glad_glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)load("glBindVertexArray");
    glad_glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)load("glDeleteVertexArrays");
    glad_glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)load("glGenVertexArrays");
    glad_glIsVertexArray = (PFNGLISVERTEXARRAYPROC)load("glIsVertexArray");
}
static void load_GL_VERSION_3_1(GLADloadproc load) {
    if(!GLAD_GL_VERSION_3_1) return;
    glad_glDrawArraysInstanced = (PFNGLDRAWARRAYSINSTANCEDPROC)load("glDrawArraysInstanced");
    glad_glDrawElementsInstanced = (PFNGLDRAWELEMENTSINSTANCEDPROC)load("glDrawElementsInstanced");
    glad_glTexBuffer = (PFNGLTEXBUFFERPROC)load("glTexBuffer");
    glad_glPrimitiveRestartIndex = (PFNGLPRIMITIVERESTARTINDEXPROC)load("glPrimitiveRestartIndex");
    glad_glCopyBufferSubData = (PFNGLCOPYBUFFERSUBDATAPROC)load("glCopyBufferSubData");
    glad_glGetUniformIndices = (PFNGLGETUNIFORMINDICESPROC)load("glGetUniformIndices");
    glad_glGetActiveUniformsiv = (PFNGLGETACTIVEUNIFORMSIVPROC)load("glGetActiveUniformsiv");
    glad_glGetActiveUniformName = (PFNGLGETACTIVEUNIFORMNAMEPROC)load("glGetActiveUniformName");
    glad_glGetUniformBlockIndex = (PFNGLGETUNIFORMBLOCKINDEXPROC)load("glGetUniformBlockIndex");
    glad_glGetActiveUniformBlockiv = (PFNGLGETACTIVEUNIFORMBLOCKIVPROC)load("glGetActiveUniformBlockiv");
    glad_glGetActiveUniformBlockName = (PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC)load("glGetActiveUniformBlockName");
    glad_glUniformBlockBinding = (PFNGLUNIFORMBLOCKBINDINGPROC)load("glUniformBlockBinding");
    glad_glBindBufferRange = (PFNGLBINDBUFFERRANGEPROC)load("glBindBufferRange");
    glad_glBindBufferBase = (PFNGLBINDBUFFERBASEPROC)load("glBindBufferBase");
    glad_glGetIntegeri_v = (PFNGLGETINTEGERI_VPROC)load("glGetIntegeri_v");
}
static void load_GL_VERSION_3_2(GLADloadproc load) {
    if(!GLAD_GL_VERSION_3_2) return;
    glad_glDrawElementsBaseVertex = (PFNGLDRAWELEMENTSBASEVERTEXPROC)load("glDrawElementsBaseVertex");
    glad_glDrawRangeElementsBaseVertex = (PFNGLDRAWRANGEELEMENTSBASEVERTEXPROC)load("glDrawRangeElementsBaseVertex");
    glad_glDrawElementsInstancedBaseVertex = (PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC)load("glDrawElementsInstancedBaseVertex");
    glad_glMultiDrawElementsBaseVertex = (PFNGLMULTIDRAWELEMENTSBASEVERTEXPROC)load("glMultiDrawElementsBaseVertex");
    glad_glProvokingVertex = (PFNGLPROVOKINGVERTEXPROC)load("glProvokingVertex");
    glad_glFenceSync = (PFNGLFENCESYNCPROC)load("glFenceSync");
    glad_glIsSync = (PFNGLISSYNCPROC)load("glIsSync");
    glad_glDeleteSync = (PFNGLDELETESYNCPROC)load("glDeleteSync");
    glad_glClientWaitSync = (PFNGLCLIENTWAITSYNCPROC)load("glClientWaitSync");
    glad_glWaitSync = (PFNGLWAITSYNCPROC)load("glWaitSync");
    glad_glGetInteger64v = (PFNGLGETINTEGER64VPROC)load("glGetInteger64v");
    glad_glGetSynciv = (PFNGLGETSYNCIVPROC)load("glGetSynciv");
    glad_glGetInteger64i_v = (PFNGLGETINTEGER64I_VPROC)load("glGetInteger64i_v");
    glad_glGetBufferParameteri64v = (PFNGLGETBUFFERPARAMETERI64VPROC)load("glGetBufferParameteri64v");
    glad_glFramebufferTexture = (PFNGLFRAMEBUFFERTEXTUREPROC)load("glFramebufferTexture");
    glad_glTexImage2DMultisample = (PFNGLTEXIMAGE2DMULTISAMPLEPROC)load("glTexImage2DMultisample");
    glad_glTexImage3DMultisample = (PFNGLTEXIMAGE3DMULTISAMPLEPROC)load("glTexImage3DMultisample");
    glad_glGetMultisamplefv = (PFNGLGETMULTISAMPLEFVPROC)load("glGetMultisamplefv");
    glad_glSampleMaski = (PFNGLSAMPLEMASKIPROC)load("glSampleMaski");
}
static void load_GL_VERSION_3_3(GLADloadproc load) {
    if(!GLAD_GL_VERSION_3_3) return;
    glad_glBindFragDataLocationIndexed = (PFNGLBINDFRAGDATALOCATIONINDEXEDPROC)load("glBindFragDataLocationIndexed");
    glad_glGetFragDataIndex = (PFNGLGETFRAGDATAINDEXPROC)load("glGetFragDataIndex");
    glad_glGenSamplers = (PFNGLGENSAMPLERSPROC)load("glGenSamplers");
    glad_glDeleteSamplers = (PFNGLDELETESAMPLERSPROC)load("glDeleteSamplers");
    glad_glIsSampler = (PFNGLISSAMPLERPROC)load("glIsSampler");
    glad_glBindSampler = (PFNGLBINDSAMPLERPROC)load("glBindSampler");
    glad_glSamplerParameteri = (PFNGLSAMPLERPARAMETERIPROC)load("glSamplerParameteri");
    glad_glSamplerParameteriv = (PFNGLSAMPLERPARAMETERIVPROC)load("glSamplerParameteriv");
    glad_glSamplerParameterf = (PFNGLSAMPLERPARAMETERFPROC)load("glSamplerParameterf");
    glad_glSamplerParameterfv = (PFNGLSAMPLERPARAMETERFVPROC)load("glSamplerParameterfv");
    glad_glSamplerParameterIiv = (PFNGLSAMPLERPARAMETERIIVPROC)load("glSamplerParameterIiv");
    glad_glSamplerParameterIuiv = (PFNGLSAMPLERPARAMETERIUIVPROC)load("glSamplerParameterIuiv");
    glad_glGetSamplerParameteriv = (PFNGLGETSAMPLERPARAMETERIVPROC)load("glGetSamplerParameteriv");
    glad_glGetSamplerParameterIiv = (PFNGLGETSAMPLERPARAMETERIIVPROC)load("glGetSamplerParameterIiv");
    glad_glGetSamplerParameterfv = (PFNGLGETSAMPLERPARAMETERFVPROC)load("glGetSamplerParameterfv");
    glad_glGetSamplerParameterIuiv = (PFNGLGETSAMPLERPARAMETERIUIVPROC)load("glGetSamplerParameterIuiv");
    glad_glQueryCounter = (PFNGLQUERYCOUNTERPROC)load("glQueryCounter");
    glad_glGetQueryObjecti64v = (PFNGLGETQUERYOBJECTI64VPROC)load("glGetQueryObjecti64v");
    glad_glGetQueryObjectui64v = (PFNGLGETQUERYOBJECTUI64VPROC)load("glGetQueryObjectui64v");
    glad_glVertexAttribDivisor = (PFNGLVERTEXATTRIBDIVISORPROC)load("glVertexAttribDivisor");
    glad_glVertexAttribP1ui = (PFNGLVERTEXATTRIBP1UIPROC)load("glVertexAttribP1ui");
    glad_glVertexAttribP1uiv = (PFNGLVERTEXATTRIBP1UIVPROC)load("glVertexAttribP1uiv");
    glad_glVertexAttribP2ui = (PFNGLVERTEXATTRIBP2UIPROC)load("glVertexAttribP2ui");
    glad_glVertexAttribP2uiv = (PFNGLVERTEXATTRIBP2UIVPROC)load("glVertexAttribP2uiv");
    glad_glVertexAttribP3ui = (PFNGLVERTEXATTRIBP3UIPROC)load("glVertexAttribP3ui");
    glad_glVertexAttribP3uiv = (PFNGLVERTEXATTRIBP3UIVPROC)load("glVertexAttribP3uiv");
    glad_glVertexAttribP4ui = (PFNGLVERTEXATTRIBP4UIPROC)load("glVertexAttribP4ui");
    glad_glVertexAttribP4uiv = (PFNGLVERTEXATTRIBP4UIVPROC)load("glVertexAttribP4uiv");
    glad_glVertexP2ui = (PFNGLVERTEXP2UIPROC)load("glVertexP2ui");
    glad_glVertexP2uiv = (PFNGLVERTEXP2UIVPROC)load("glVertexP2uiv");
    glad_glVertexP3ui = (PFNGLVERTEXP3UIPROC)load("glVertexP3ui");
    glad_glVertexP3uiv = (PFNGLVERTEXP3UIVPROC)load("glVertexP3uiv");
    glad_glVertexP4ui = (PFNGLVERTEXP4UIPROC)load("glVertexP4ui");
    glad_glVertexP4uiv = (PFNGLVERTEXP4UIVPROC)load("glVertexP4uiv");
    glad_glTexCoordP1ui = (PFNGLTEXCOORDP1UIPROC)load("glTexCoordP1ui");
    glad_glTexCoordP1uiv = (PFNGLTEXCOORDP1UIVPROC)load("glTexCoordP1uiv");
    glad_glTexCoordP2ui = (PFNGLTEXCOORDP2UIPROC)load("glTexCoordP2ui");
    glad_glTexCoordP2uiv = (PFNGLTEXCOORDP2UIVPROC)load("glTexCoordP2uiv");
    glad_glTexCoordP3ui = (PFNGLTEXCOORDP3UIPROC)load("glTexCoordP3ui");
    glad_glTexCoordP3uiv = (PFNGLTEXCOORDP3UIVPROC)load("glTexCoordP3uiv");
    glad_glTexCoordP4ui = (PFNGLTEXCOORDP4UIPROC)load("glTexCoordP4ui");
    glad_glTexCoordP4uiv = (PFNGLTEXCOORDP4UIVPROC)load("glTexCoordP4uiv");
    glad_glMultiTexCoordP1ui = (PFNGLMULTITEXCOORDP1UIPROC)load("glMultiTexCoordP1ui");
    glad_glMultiTexCoordP1uiv = (PFNGLMULTITEXCOORDP1UIVPROC)load("glMultiTexCoordP1uiv");
    glad_glMultiTexCoordP2ui = (PFNGLMULTITEXCOORDP2UIPROC)load("glMultiTexCoordP2ui");
    glad_glMultiTexCoordP2uiv = (PFNGLMULTITEXCOORDP2UIVPROC)load("glMultiTexCoordP2uiv");
    glad_glMultiTexCoordP3ui = (PFNGLMULTITEXCOORDP3UIPROC)load("glMultiTexCoordP3ui");
    glad_glMultiTexCoordP3uiv = (PFNGLMULTITEXCOORDP3UIVPROC)load("glMultiTexCoordP3uiv");
    glad_glMultiTexCoordP4ui = (PFNGLMULTITEXCOORDP4UIPROC)load("glMultiTexCoordP4ui");
    glad_glMultiTexCoordP4uiv = (PFNGLMULTITEXCOORDP4UIVPROC)load("glMultiTexCoordP4uiv");
    glad_glNormalP3ui = (PFNGLNORMALP3UIPROC)load("glNormalP3ui");
    glad_glNormalP3uiv = (PFNGLNORMALP3UIVPROC)load("glNormalP3uiv");
    glad_glColorP3ui = (PFNGLCOLORP3UIPROC)load("glColorP3ui");
    glad_glColorP3uiv = (PFNGLCOLORP3UIVPROC)load("glColorP3uiv");
    glad_glColorP4ui = (PFNGLCOLORP4UIPROC)load("glColorP4ui");
    glad_glColorP4uiv = (PFNGLCOLORP4UIVPROC)load("glColorP4uiv");
    glad_glSecondaryColorP3ui = (PFNGLSECONDARYCOLORP3UIPROC)load("glSecondaryColorP3ui");
    glad_glSecondaryColorP3uiv = (PFNGLSECONDARYCOLORP3UIVPROC)load("glSecondaryColorP3uiv");
}
static void load_GL_VERSION_4_0(GLADloadproc load) {
    if(!GLAD_GL_VERSION_4_0) return;
    glad_glMinSampleShading = (PFNGLMINSAMPLESHADINGPROC)load("glMinSampleShading");
    glad_glBlendEquationi = (PFNGLBLENDEQUATIONIPROC)load("glBlendEquationi");
    glad_glBlendEquationSeparatei = (PFNGLBLENDEQUATIONSEPARATEIPROC)load("glBlendEquationSeparatei");
    glad_glBlendFunci = (PFNGLBLENDFUNCIPROC)load("glBlendFunci");
    glad_glBlendFuncSeparatei = (PFNGLBLENDFUNCSEPARATEIPROC)load("glBlendFuncSeparatei");
    glad_glDrawArraysIndirect = (PFNGLDRAWARRAYSINDIRECTPROC)load("glDrawArraysIndirect");
    glad_glDrawElementsIndirect = (PFNGLDRAWELEMENTSINDIRECTPROC)load("glDrawElementsIndirect");
    glad_glUniform1d = (PFNGLUNIFORM1DPROC)load("glUniform1d");
    glad_glUniform2d = (PFNGLUNIFORM2DPROC)load("glUniform2d");
    glad_glUniform3d = (PFNGLUNIFORM3DPROC)load("glUniform3d");
    glad_glUniform4d = (PFNGLUNIFORM4DPROC)load("glUniform4d");
    glad_glUniform1dv = (PFNGLUNIFORM1DVPROC)load("glUniform1dv");
    glad_glUniform2dv = (PFNGLUNIFORM2DVPROC)load("glUniform2dv");
    glad_glUniform3dv = (PFNGLUNIFORM3DVPROC)load("glUniform3dv");
    glad_glUniform4dv = (PFNGLUNIFORM4DVPROC)load("glUniform4dv");
    glad_glUniformMatrix2dv = (PFNGLUNIFORMMATRIX2DVPROC)load("glUniformMatrix2dv");
    glad_glUniformMatrix3dv = (PFNGLUNIFORMMATRIX3DVPROC)load("glUniformMatrix3dv");
    glad_glUniformMatrix4dv = (PFNGLUNIFORMMATRIX4DVPROC)load("glUniformMatrix4dv");
    glad_glUniformMatrix2x3dv = (PFNGLUNIFORMMATRIX2X3DVPROC)load("glUniformMatrix2x3dv");
    glad_glUniformMatrix2x4dv = (PFNGLUNIFORMMATRIX2X4DVPROC)load("glUniformMatrix2x4dv");
    glad_glUniformMatrix3x2dv = (PFNGLUNIFORMMATRIX3X2DVPROC)load("glUniformMatrix3x2dv");
    glad_glUniformMatrix3x4dv = (PFNGLUNIFORMMATRIX3X4DVPROC)load("glUniformMatrix3x4dv");
    glad_glUniformMatrix4x2dv = (PFNGLUNIFORMMATRIX4X2DVPROC)load("glUniformMatrix4x2dv");
    glad_glUniformMatrix4x3dv = (PFNGLUNIFORMMATRIX4X3DVPROC)load("glUniformMatrix4x3dv");
    glad_glGetUniformdv = (PFNGLGETUNIFORMDVPROC)load("glGetUniformdv");
    glad_glGetSubroutineUniformLocation = (PFNGLGETSUBROUTINEUNIFORMLOCATIONPROC)load("glGetSubroutineUniformLocation");
    glad_glGetSubroutineIndex = (PFNGLGETSUBROUTINEINDEXPROC)load("glGetSubroutineIndex");
    glad_glGetActiveSubroutineUniformiv = (PFNGLGETACTIVESUBROUTINEUNIFORMIVPROC)load("glGetActiveSubroutineUniformiv");
    glad_glGetActiveSubroutineUniformName = (PFNGLGETACTIVESUBROUTINEUNIFORMNAMEPROC)load("glGetActiveSubroutineUniformName");
    glad_glGetActiveSubroutineName = (PFNGLGETACTIVESUBROUTINENAMEPROC)load("glGetActiveSubroutineName");
    glad_glUniformSubroutinesuiv = (PFNGLUNIFORMSUBROUTINESUIVPROC)load("glUniformSubroutinesuiv");
    glad_glGetUniformSubroutineuiv = (PFNGLGETUNIFORMSUBROUTINEUIVPROC)load("glGetUniformSubroutineuiv");
    glad_glGetProgramStageiv = (PFNGLGETPROGRAMSTAGEIVPROC)load("glGetProgramStageiv");
    glad_glPatchParameteri = (PFNGLPATCHPARAMETERIPROC)load("glPatchParameteri");
    glad_glPatchParameterfv = (PFNGLPATCHPARAMETERFVPROC)load("glPatchParameterfv");
    glad_glBindTransformFeedback = (PFNGLBINDTRANSFORMFEEDBACKPROC)load("glBindTransformFeedback");
    glad_glDeleteTransformFeedbacks = (PFNGLDELETETRANSFORMFEEDBACKSPROC)load("glDeleteTransformFeedbacks");
    glad_glGenTransformFeedbacks = (PFNGLGENTRANSFORMFEEDBACKSPROC)load("glGenTransformFeedbacks");
    glad_glIsTransformFeedback = (PFNGLISTRANSFORMFEEDBACKPROC)load("glIsTransformFeedback");
    glad_glPauseTransformFeedback = (PFNGLPAUSETRANSFORMFEEDBACKPROC)load("glPauseTransformFeedback");
    glad_glResumeTransformFeedback = (PFNGLRESUMETRANSFORMFEEDBACKPROC)load("glResumeTransformFeedback");
    glad_glDrawTransformFeedback = (PFNGLDRAWTRANSFORMFEEDBACKPROC)load("glDrawTransformFeedback");
    glad_glDrawTransformFeedbackStream = (PFNGLDRAWTRANSFORMFEEDBACKSTREAMPROC)load("glDrawTransformFeedbackStream");
    glad_glBeginQueryIndexed = (PFNGLBEGINQUERYINDEXEDPROC)load("glBeginQueryIndexed");
    glad_glEndQueryIndexed = (PFNGLENDQUERYINDEXEDPROC)load("glEndQueryIndexed");
    glad_glGetQueryIndexediv = (PFNGLGETQUERYINDEXEDIVPROC)load("glGetQueryIndexediv");
}
static void load_GL_VERSION_4_1(GLADloadproc load) {
    if(!GLAD_GL_VERSION_4_1) return;
    glad_glReleaseShaderCompiler = (PFNGLRELEASESHADERCOMPILERPROC)load("glReleaseShaderCompiler");
    glad_glShaderBinary = (PFNGLSHADERBINARYPROC)load("glShaderBinary");
    glad_glGetShaderPrecisionFormat = (PFNGLGETSHADERPRECISIONFORMATPROC)load("glGetShaderPrecisionFormat");
    glad_glDepthRangef = (PFNGLDEPTHRANGEFPROC)load("glDepthRangef");
    glad_glClearDepthf = (PFNGLCLEARDEPTHFPROC)load("glClearDepthf");
    glad_glGetProgramBinary = (PFNGLGETPROGRAMBINARYPROC)load("glGetProgramBinary");
    glad_glProgramBinary = (PFNGLPROGRAMBINARYPROC)load("glProgramBinary");
    glad_glProgramParameteri = (PFNGLPROGRAMPARAMETERIPROC)load("glProgramParameteri");
    glad_glUseProgramStages = (PFNGLUSEPROGRAMSTAGESPROC)load("glUseProgramStages");
    glad_glActiveShaderProgram = (PFNGLACTIVESHADERPROGRAMPROC)load("glActiveShaderProgram");
    glad_glCreateShaderProgramv = (PFNGLCREATESHADERPROGRAMVPROC)load("glCreateShaderProgramv");
    glad_glBindProgramPipeline = (PFNGLBINDPROGRAMPIPELINEPROC)load("glBindProgramPipeline");
    glad_glDeleteProgramPipelines = (PFNGLDELETEPROGRAMPIPELINESPROC)load("glDeleteProgramPipelines");
    glad_glGenProgramPipelines = (PFNGLGENPROGRAMPIPELINESPROC)load("glGenProgramPipelines");
    glad_glIsProgramPipeline = (PFNGLISPROGRAMPIPELINEPROC)load("glIsProgramPipeline");
    glad_glGetProgramPipelineiv = (PFNGLGETPROGRAMPIPELINEIVPROC)load("glGetProgramPipelineiv");
    glad_glProgramParameteri = (PFNGLPROGRAMPARAMETERIPROC)load("glProgramParameteri");
    glad_glProgramUniform1i = (PFNGLPROGRAMUNIFORM1IPROC)load("glProgramUniform1i");
    glad_glProgramUniform1iv = (PFNGLPROGRAMUNIFORM1IVPROC)load("glProgramUniform1iv");
    glad_glProgramUniform1f = (PFNGLPROGRAMUNIFORM1FPROC)load("glProgramUniform1f");
    glad_glProgramUniform1fv = (PFNGLPROGRAMUNIFORM1FVPROC)load("glProgramUniform1fv");
    glad_glProgramUniform1d = (PFNGLPROGRAMUNIFORM1DPROC)load("glProgramUniform1d");
    glad_glProgramUniform1dv = (PFNGLPROGRAMUNIFORM1DVPROC)load("glProgramUniform1dv");
    glad_glProgramUniform1ui = (PFNGLPROGRAMUNIFORM1UIPROC)load("glProgramUniform1ui");
    glad_glProgramUniform1uiv = (PFNGLPROGRAMUNIFORM1UIVPROC)load("glProgramUniform1uiv");
    glad_glProgramUniform2i = (PFNGLPROGRAMUNIFORM2IPROC)load("glProgramUniform2i");
    glad_glProgramUniform2iv = (PFNGLPROGRAMUNIFORM2IVPROC)load("glProgramUniform2iv");
    glad_glProgramUniform2f = (PFNGLPROGRAMUNIFORM2FPROC)load("glProgramUniform2f");
    glad_glProgramUniform2fv = (PFNGLPROGRAMUNIFORM2FVPROC)load("glProgramUniform2fv");
    glad_glProgramUniform2d = (PFNGLPROGRAMUNIFORM2DPROC)load("glProgramUniform2d");
    glad_glProgramUniform2dv = (PFNGLPROGRAMUNIFORM2DVPROC)load("glProgramUniform2dv");
    glad_glProgramUniform2ui = (PFNGLPROGRAMUNIFORM2UIPROC)load("glProgramUniform2ui");
    glad_glProgramUniform2uiv = (PFNGLPROGRAMUNIFORM2UIVPROC)load("glProgramUniform2uiv");
    glad_glProgramUniform3i = (PFNGLPROGRAMUNIFORM3IPROC)load("glProgramUniform3i");
    glad_glProgramUniform3iv = (PFNGLPROGRAMUNIFORM3IVPROC)load("glProgramUniform3iv");
    glad_glProgramUniform3f = (PFNGLPROGRAMUNIFORM3FPROC)load("glProgramUniform3f");
    glad_glProgramUniform3fv = (PFNGLPROGRAMUNIFORM3FVPROC)load("glProgramUniform3fv");
    glad_glProgramUniform3d = (PFNGLPROGRAMUNIFORM3DPROC)load("glProgramUniform3d");
    glad_glProgramUniform3dv = (PFNGLPROGRAMUNIFORM3DVPROC)load("glProgramUniform3dv");
    glad_glProgramUniform3ui = (PFNGLPROGRAMUNIFORM3UIPROC)load("glProgramUniform3ui");
    glad_glProgramUniform3uiv = (PFNGLPROGRAMUNIFORM3UIVPROC)load("glProgramUniform3uiv");
    glad_glProgramUniform4i = (PFNGLPROGRAMUNIFORM4IPROC)load("glProgramUniform4i");
    glad_glProgramUniform4iv = (PFNGLPROGRAMUNIFORM4IVPROC)load("glProgramUniform4iv");
    glad_glProgramUniform4f = (PFNGLPROGRAMUNIFORM4FPROC)load("glProgramUniform4f");
    glad_glProgramUniform4fv = (PFNGLPROGRAMUNIFORM4FVPROC)load("glProgramUniform4fv");
    glad_glProgramUniform4d = (PFNGLPROGRAMUNIFORM4DPROC)load("glProgramUniform4d");
    glad_glProgramUniform4dv = (PFNGLPROGRAMUNIFORM4DVPROC)load("glProgramUniform4dv");
    glad_glProgramUniform4ui = (PFNGLPROGRAMUNIFORM4UIPROC)load("glProgramUniform4ui");
    glad_glProgramUniform4uiv = (PFNGLPROGRAMUNIFORM4UIVPROC)load("glProgramUniform4uiv");
    glad_glProgramUniformMatrix2fv = (PFNGLPROGRAMUNIFORMMATRIX2FVPROC)load("glProgramUniformMatrix2fv");
    glad_glProgramUniformMatrix3fv = (PFNGLPROGRAMUNIFORMMATRIX3FVPROC)load("glProgramUniformMatrix3fv");
    glad_glProgramUniformMatrix4fv = (PFNGLPROGRAMUNIFORMMATRIX4FVPROC)load("glProgramUniformMatrix4fv");
    glad_glProgramUniformMatrix2dv = (PFNGLPROGRAMUNIFORMMATRIX2DVPROC)load("glProgramUniformMatrix2dv");
    glad_glProgramUniformMatrix3dv = (PFNGLPROGRAMUNIFORMMATRIX3DVPROC)load("glProgramUniformMatrix3dv");
    glad_glProgramUniformMatrix4dv = (PFNGLPROGRAMUNIFORMMATRIX4DVPROC)load("glProgramUniformMatrix4dv");
    glad_glProgramUniformMatrix2x3fv = (PFNGLPROGRAMUNIFORMMATRIX2X3FVPROC)load("glProgramUniformMatrix2x3fv");
    glad_glProgramUniformMatrix3x2fv = (PFNGLPROGRAMUNIFORMMATRIX3X2FVPROC)load("glProgramUniformMatrix3x2fv");
    glad_glProgramUniformMatrix2x4fv = (PFNGLPROGRAMUNIFORMMATRIX2X4FVPROC)load("glProgramUniformMatrix2x4fv");
    glad_glProgramUniformMatrix4x2fv = (PFNGLPROGRAMUNIFORMMATRIX4X2FVPROC)load("glProgramUniformMatrix4x2fv");
    glad_glProgramUniformMatrix3x4fv = (PFNGLPROGRAMUNIFORMMATRIX3X4FVPROC)load("glProgramUniformMatrix3x4fv");
    glad_glProgramUniformMatrix4x3fv = (PFNGLPROGRAMUNIFORMMATRIX4X3FVPROC)load("glProgramUniformMatrix4x3fv");
    glad_glProgramUniformMatrix2x3dv = (PFNGLPROGRAMUNIFORMMATRIX2X3DVPROC)load("glProgramUniformMatrix2x3dv");
    glad_glProgramUniformMatrix3x2dv = (PFNGLPROGRAMUNIFORMMATRIX3X2DVPROC)load("glProgramUniformMatrix3x2dv");
    glad_glProgramUniformMatrix2x4dv = (PFNGLPROGRAMUNIFORMMATRIX2X4DVPROC)load("glProgramUniformMatrix2x4dv");
    glad_glProgramUniformMatrix4x2dv = (PFNGLPROGRAMUNIFORMMATRIX4X2DVPROC)load("glProgramUniformMatrix4x2dv");
    glad_glProgramUniformMatrix3x4dv = (PFNGLPROGRAMUNIFORMMATRIX3X4DVPROC)load("glProgramUniformMatrix3x4dv");
    glad_glProgramUniformMatrix4x3dv = (PFNGLPROGRAMUNIFORMMATRIX4X3DVPROC)load("glProgramUniformMatrix4x3dv");
    glad_glValidateProgramPipeline = (PFNGLVALIDATEPROGRAMPIPELINEPROC)load("glValidateProgramPipeline");
    glad_glGetProgramPipelineInfoLog = (PFNGLGETPROGRAMPIPELINEINFOLOGPROC)load("glGetProgramPipelineInfoLog");
    glad_glVertexAttribL1d = (PFNGLVERTEXATTRIBL1DPROC)load("glVertexAttribL1d");
    glad_glVertexAttribL2d = (PFNGLVERTEXATTRIBL2DPROC)load("glVertexAttribL2d");
    glad_glVertexAttribL3d = (PFNGLVERTEXATTRIBL3DPROC)load("glVertexAttribL3d");
    glad_glVertexAttribL4d = (PFNGLVERTEXATTRIBL4DPROC)load("glVertexAttribL4d");
    glad_glVertexAttribL1dv = (PFNGLVERTEXATTRIBL1DVPROC)load("glVertexAttribL1dv");
    glad_glVertexAttribL2dv = (PFNGLVERTEXATTRIBL2DVPROC)load("glVertexAttribL2dv");
    glad_glVertexAttribL3dv = (PFNGLVERTEXATTRIBL3DVPROC)load("glVertexAttribL3dv");
    glad_glVertexAttribL4dv = (PFNGLVERTEXATTRIBL4DVPROC)load("glVertexAttribL4dv");
    glad_glVertexAttribLPointer = (PFNGLVERTEXATTRIBLPOINTERPROC)load("glVertexAttribLPointer");
    glad_glGetVertexAttribLdv = (PFNGLGETVERTEXATTRIBLDVPROC)load("glGetVertexAttribLdv");
    glad_glViewportArrayv = (PFNGLVIEWPORTARRAYVPROC)load("glViewportArrayv");
    glad_glViewportIndexedf = (PFNGLVIEWPORTINDEXEDFPROC)load("glViewportIndexedf");
    glad_glViewportIndexedfv = (PFNGLVIEWPORTINDEXEDFVPROC)load("glViewportIndexedfv");
    glad_glScissorArrayv = (PFNGLSCISSORARRAYVPROC)load("glScissorArrayv");
    glad_glScissorIndexed = (PFNGLSCISSORINDEXEDPROC)load("glScissorIndexed");
    glad_glScissorIndexedv = (PFNGLSCISSORINDEXEDVPROC)load("glScissorIndexedv");
    glad_glDepthRangeArrayv = (PFNGLDEPTHRANGEARRAYVPROC)load("glDepthRangeArrayv");
    glad_glDepthRangeIndexed = (PFNGLDEPTHRANGEINDEXEDPROC)load("glDepthRangeIndexed");
    glad_glGetFloati_v = (PFNGLGETFLOATI_VPROC)load("glGetFloati_v");
    glad_glGetDoublei_v = (PFNGLGETDOUBLEI_VPROC)load("glGetDoublei_v");
}
static void load_GL_VERSION_4_2(GLADloadproc load) {
    if(!GLAD_GL_VERSION_4_2) return;
    glad_glDrawArraysInstancedBaseInstance = (PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEPROC)load("glDrawArraysInstancedBaseInstance");
    glad_glDrawElementsInstancedBaseInstance = (PFNGLDRAWELEMENTSINSTANCEDBASEINSTANCEPROC)load("glDrawElementsInstancedBaseInstance");
    glad_glDrawElementsInstancedBaseVertexBaseInstance = (PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEPROC)load("glDrawElementsInstancedBaseVertexBaseInstance");
    glad_glGetInternalformativ = (PFNGLGETINTERNALFORMATIVPROC)load("glGetInternalformativ");
    glad_glGetActiveAtomicCounterBufferiv = (PFNGLGETACTIVEATOMICCOUNTERBUFFERIVPROC)load("glGetActiveAtomicCounterBufferiv");
    glad_glBindImageTexture = (PFNGLBINDIMAGETEXTUREPROC)load("glBindImageTexture");
    glad_glMemoryBarrier = (PFNGLMEMORYBARRIERPROC)load("glMemoryBarrier");
    glad_glTexStorage1D = (PFNGLTEXSTORAGE1DPROC)load("glTexStorage1D");
    glad_glTexStorage2D = (PFNGLTEXSTORAGE2DPROC)load("glTexStorage2D");
    glad_glTexStorage3D = (PFNGLTEXSTORAGE3DPROC)load("glTexStorage3D");
    glad_glDrawTransformFeedbackInstanced = (PFNGLDRAWTRANSFORMFEEDBACKINSTANCEDPROC)load("glDrawTransformFeedbackInstanced");
    glad_glDrawTransformFeedbackStreamInstanced = (PFNGLDRAWTRANSFORMFEEDBACKSTREAMINSTANCEDPROC)load("glDrawTransformFeedbackStreamInstanced");
}
static void load_GL_VERSION_4_3(GLADloadproc load) {
    if(!GLAD_GL_VERSION_4_3) return;
    glad_glClearBufferData = (PFNGLCLEARBUFFERDATAPROC)load("glClearBufferData");
    glad_glClearBufferSubData = (PFNGLCLEARBUFFERSUBDATAPROC)load("glClearBufferSubData");
    glad_glDispatchCompute = (PFNGLDISPATCHCOMPUTEPROC)load("glDispatchCompute");
    glad_glDispatchComputeIndirect = (PFNGLDISPATCHCOMPUTEINDIRECTPROC)load("glDispatchComputeIndirect");
    glad_glCopyImageSubData = (PFNGLCOPYIMAGESUBDATAPROC)load("glCopyImageSubData");
    glad_glFramebufferParameteri = (PFNGLFRAMEBUFFERPARAMETERIPROC)load("glFramebufferParameteri");
    glad_glGetFramebufferParameteriv = (PFNGLGETFRAMEBUFFERPARAMETERIVPROC)load("glGetFramebufferParameteriv");
    glad_glGetInternalformati64v = (PFNGLGETINTERNALFORMATI64VPROC)load("glGetInternalformati64v");
    glad_glInvalidateTexSubImage = (PFNGLINVALIDATETEXSUBIMAGEPROC)load("glInvalidateTexSubImage");
    glad_glInvalidateTexImage = (PFNGLINVALIDATETEXIMAGEPROC)load("glInvalidateTexImage");
    glad_glInvalidateBufferSubData = (PFNGLINVALIDATEBUFFERSUBDATAPROC)load("glInvalidateBufferSubData");
    glad_glInvalidateBufferData = (PFNGLINVALIDATEBUFFERDATAPROC)load("glInvalidateBufferData");
    glad_glInvalidateFramebuffer = (PFNGLINVALIDATEFRAMEBUFFERPROC)load("glInvalidateFramebuffer");
    glad_glInvalidateSubFramebuffer = (PFNGLINVALIDATESUBFRAMEBUFFERPROC)load("glInvalidateSubFramebuffer");
    glad_glMultiDrawArraysIndirect = (PFNGLMULTIDRAWARRAYSINDIRECTPROC)load("glMultiDrawArraysIndirect");
    glad_glMultiDrawElementsIndirect = (PFNGLMULTIDRAWELEMENTSINDIRECTPROC)load("glMultiDrawElementsIndirect");
    glad_glGetProgramInterfaceiv = (PFNGLGETPROGRAMINTERFACEIVPROC)load("glGetProgramInterfaceiv");
    glad_glGetProgramResourceIndex = (PFNGLGETPROGRAMRESOURCEINDEXPROC)load("glGetProgramResourceIndex");
    glad_glGetProgramResourceName = (PFNGLGETPROGRAMRESOURCENAMEPROC)load("glGetProgramResourceName");
    glad_glGetProgramResourceiv = (PFNGLGETPROGRAMRESOURCEIVPROC)load("glGetProgramResourceiv");
    glad_glGetProgramResourceLocation = (PFNGLGETPROGRAMRESOURCELOCATIONPROC)load("glGetProgramResourceLocation");
    glad_glGetProgramResourceLocationIndex = (PFNGLGETPROGRAMRESOURCELOCATIONINDEXPROC)load("glGetProgramResourceLocationIndex");
    glad_glShaderStorageBlockBinding = (PFNGLSHADERSTORAGEBLOCKBINDINGPROC)load("glShaderStorageBlockBinding");
    glad_glTexBufferRange = (PFNGLTEXBUFFERRANGEPROC)load("glTexBufferRange");
    glad_glTexStorage2DMultisample = (PFNGLTEXSTORAGE2DMULTISAMPLEPROC)load("glTexStorage2DMultisample");
    glad_glTexStorage3DMultisample = (PFNGLTEXSTORAGE3DMULTISAMPLEPROC)load("glTexStorage3DMultisample");
    glad_glTextureView = (PFNGLTEXTUREVIEWPROC)load("glTextureView");
    glad_glBindVertexBuffer = (PFNGLBINDVERTEXBUFFERPROC)load("glBindVertexBuffer");
    glad_glVertexAttribFormat = (PFNGLVERTEXATTRIBFORMATPROC)load("glVertexAttribFormat");
    glad_glVertexAttribIFormat = (PFNGLVERTEXATTRIBIFORMATPROC)load("glVertexAttribIFormat");
    glad_glVertexAttribLFormat = (PFNGLVERTEXATTRIBLFORMATPROC)load("glVertexAttribLFormat");
    glad_glVertexAttribBinding = (PFNGLVERTEXATTRIBBINDINGPROC)load("glVertexAttribBinding");
    glad_glVertexBindingDivisor = (PFNGLVERTEXBINDINGDIVISORPROC)load("glVertexBindingDivisor");
    glad_glDebugMessageControl = (PFNGLDEBUGMESSAGECONTROLPROC)load("glDebugMessageControl");
    glad_glDebugMessageInsert = (PFNGLDEBUGMESSAGEINSERTPROC)load("glDebugMessageInsert");
    glad_glDebugMessageCallback = (PFNGLDEBUGMESSAGECALLBACKPROC)load("glDebugMessageCallback");
    glad_glGetDebugMessageLog = (PFNGLGETDEBUGMESSAGELOGPROC)load("glGetDebugMessageLog");
    glad_glPushDebugGroup = (PFNGLPUSHDEBUGGROUPPROC)load("glPushDebugGroup");
    glad_glPopDebugGroup = (PFNGLPOPDEBUGGROUPPROC)load("glPopDebugGroup");
    glad_glObjectLabel = (PFNGLOBJECTLABELPROC)load("glObjectLabel");
    glad_glGetObjectLabel = (PFNGLGETOBJECTLABELPROC)load("glGetObjectLabel");
    glad_glObjectPtrLabel = (PFNGLOBJECTPTRLABELPROC)load("glObjectPtrLabel");
    glad_glGetObjectPtrLabel = (PFNGLGETOBJECTPTRLABELPROC)load("glGetObjectPtrLabel");
    glad_glGetPointerv = (PFNGLGETPOINTERVPROC)load("glGetPointerv");
}
static void load_GL_VERSION_4_4(GLADloadproc load) {
    if(!GLAD_GL_VERSION_4_4) return;
    glad_glBufferStorage = (PFNGLBUFFERSTORAGEPROC)load("glBufferStorage");
    glad_glClearTexImage = (PFNGLCLEARTEXIMAGEPROC)load("glClearTexImage");
    glad_glClearTexSubImage = (PFNGLCLEARTEXSUBIMAGEPROC)load("glClearTexSubImage");
    glad_glBindBuffersBase = (PFNGLBINDBUFFERSBASEPROC)load("glBindBuffersBase");
    glad_glBindBuffersRange = (PFNGLBINDBUFFERSRANGEPROC)load("glBindBuffersRange");
    glad_glBindTextures = (PFNGLBINDTEXTURESPROC)load("glBindTextures");
    glad_glBindSamplers = (PFNGLBINDSAMPLERSPROC)load("glBindSamplers");
    glad_glBindImageTextures = (PFNGLBINDIMAGETEXTURESPROC)load("glBindImageTextures");
    glad_glBindVertexBuffers = (PFNGLBINDVERTEXBUFFERSPROC)load("glBindVertexBuffers");
}
static void load_GL_VERSION_4_5(GLADloadproc load) {
    if(!GLAD_GL_VERSION_4_5) return;
    glad_glClipControl = (PFNGLCLIPCONTROLPROC)load("glClipControl");
    glad_glCreateTransformFeedbacks = (PFNGLCREATETRANSFORMFEEDBACKSPROC)load("glCreateTransformFeedbacks");
    glad_glTransformFeedbackBufferBase = (PFNGLTRANSFORMFEEDBACKBUFFERBASEPROC)load("glTransformFeedbackBufferBase");
    glad_glTransformFeedbackBufferRange = (PFNGLTRANSFORMFEEDBACKBUFFERRANGEPROC)load("glTransformFeedbackBufferRange");
    glad_glGetTransformFeedbackiv = (PFNGLGETTRANSFORMFEEDBACKIVPROC)load("glGetTransformFeedbackiv");
    glad_glGetTransformFeedbacki_v = (PFNGLGETTRANSFORMFEEDBACKI_VPROC)load("glGetTransformFeedbacki_v");
    glad_glGetTransformFeedbacki64_v = (PFNGLGETTRANSFORMFEEDBACKI64_VPROC)load("glGetTransformFeedbacki64_v");
    glad_glCreateBuffers = (PFNGLCREATEBUFFERSPROC)load("glCreateBuffers");
    glad_glNamedBufferStorage = (PFNGLNAMEDBUFFERSTORAGEPROC)load("glNamedBufferStorage");
    glad_glNamedBufferData = (PFNGLNAMEDBUFFERDATAPROC)load("glNamedBufferData");
    glad_glNamedBufferSubData = (PFNGLNAMEDBUFFERSUBDATAPROC)load("glNamedBufferSubData");
    glad_glCopyNamedBufferSubData = (PFNGLCOPYNAMEDBUFFERSUBDATAPROC)load("glCopyNamedBufferSubData");
    glad_glClearNamedBufferData = (PFNGLCLEARNAMEDBUFFERDATAPROC)load("glClearNamedBufferData");
    glad_glClearNamedBufferSubData = (PFNGLCLEARNAMEDBUFFERSUBDATAPROC)load("glClearNamedBufferSubData");
    glad_glMapNamedBuffer = (PFNGLMAPNAMEDBUFFERPROC)load("glMapNamedBuffer");
    glad_glMapNamedBufferRange = (PFNGLMAPNAMEDBUFFERRANGEPROC)load("glMapNamedBufferRange");
    glad_glUnmapNamedBuffer = (PFNGLUNMAPNAMEDBUFFERPROC)load("glUnmapNamedBuffer");
    glad_glFlushMappedNamedBufferRange = (PFNGLFLUSHMAPPEDNAMEDBUFFERRANGEPROC)load("glFlushMappedNamedBufferRange");
    glad_glGetNamedBufferParameteriv = (PFNGLGETNAMEDBUFFERPARAMETERIVPROC)load("glGetNamedBufferParameteriv");
    glad_glGetNamedBufferParameteri64v = (PFNGLGETNAMEDBUFFERPARAMETERI64VPROC)load("glGetNamedBufferParameteri64v");
    glad_glGetNamedBufferPointerv = (PFNGLGETNAMEDBUFFERPOINTERVPROC)load("glGetNamedBufferPointerv");
    glad_glGetNamedBufferSubData = (PFNGLGETNAMEDBUFFERSUBDATAPROC)load("glGetNamedBufferSubData");
    glad_glCreateFramebuffers = (PFNGLCREATEFRAMEBUFFERSPROC)load("glCreateFramebuffers");
    glad_glNamedFramebufferRenderbuffer = (PFNGLNAMEDFRAMEBUFFERRENDERBUFFERPROC)load("glNamedFramebufferRenderbuffer");
    glad_glNamedFramebufferParameteri = (PFNGLNAMEDFRAMEBUFFERPARAMETERIPROC)load("glNamedFramebufferParameteri");
    glad_glNamedFramebufferTexture = (PFNGLNAMEDFRAMEBUFFERTEXTUREPROC)load("glNamedFramebufferTexture");
    glad_glNamedFramebufferTextureLayer = (PFNGLNAMEDFRAMEBUFFERTEXTURELAYERPROC)load("glNamedFramebufferTextureLayer");
    glad_glNamedFramebufferDrawBuffer = (PFNGLNAMEDFRAMEBUFFERDRAWBUFFERPROC)load("glNamedFramebufferDrawBuffer");
    glad_glNamedFramebufferDrawBuffers = (PFNGLNAMEDFRAMEBUFFERDRAWBUFFERSPROC)load("glNamedFramebufferDrawBuffers");
    glad_glNamedFramebufferReadBuffer = (PFNGLNAMEDFRAMEBUFFERREADBUFFERPROC)load("glNamedFramebufferReadBuffer");
    glad_glInvalidateNamedFramebufferData = (PFNGLINVALIDATENAMEDFRAMEBUFFERDATAPROC)load("glInvalidateNamedFramebufferData");
    glad_glInvalidateNamedFramebufferSubData = (PFNGLINVALIDATENAMEDFRAMEBUFFERSUBDATAPROC)load("glInvalidateNamedFramebufferSubData");
    glad_glClearNamedFramebufferiv = (PFNGLCLEARNAMEDFRAMEBUFFERIVPROC)load("glClearNamedFramebufferiv");
    glad_glClearNamedFramebufferuiv = (PFNGLCLEARNAMEDFRAMEBUFFERUIVPROC)load("glClearNamedFramebufferuiv");
    glad_glClearNamedFramebufferfv = (PFNGLCLEARNAMEDFRAMEBUFFERFVPROC)load("glClearNamedFramebufferfv");
    glad_glClearNamedFramebufferfi = (PFNGLCLEARNAMEDFRAMEBUFFERFIPROC)load("glClearNamedFramebufferfi");
    glad_glBlitNamedFramebuffer = (PFNGLBLITNAMEDFRAMEBUFFERPROC)load("glBlitNamedFramebuffer");
    glad_glCheckNamedFramebufferStatus = (PFNGLCHECKNAMEDFRAMEBUFFERSTATUSPROC)load("glCheckNamedFramebufferStatus");
    glad_glGetNamedFramebufferParameteriv = (PFNGLGETNAMEDFRAMEBUFFERPARAMETERIVPROC)load("glGetNamedFramebufferParameteriv");
    glad_glGetNamedFramebufferAttachmentParameteriv = (PFNGLGETNAMEDFRAMEBUFFERATTACHMENTPARAMETERIVPROC)load("glGetNamedFramebufferAttachmentParameteriv");
    glad_glCreateRenderbuffers = (PFNGLCREATERENDERBUFFERSPROC)load("glCreateRenderbuffers");
    glad_glNamedRenderbufferStorage = (PFNGLNAMEDRENDERBUFFERSTORAGEPROC)load("glNamedRenderbufferStorage");
    glad_glNamedRenderbufferStorageMultisample = (PFNGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLEPROC)load("glNamedRenderbufferStorageMultisample");
    glad_glGetNamedRenderbufferParameteriv = (PFNGLGETNAMEDRENDERBUFFERPARAMETERIVPROC)load("glGetNamedRenderbufferParameteriv");
    glad_glCreateTextures = (PFNGLCREATETEXTURESPROC)load("glCreateTextures");
    glad_glTextureBuffer = (PFNGLTEXTUREBUFFERPROC)load("glTextureBuffer");
    glad_glTextureBufferRange = (PFNGLTEXTUREBUFFERRANGEPROC)load("glTextureBufferRange");
    glad_glTextureStorage1D = (PFNGLTEXTURESTORAGE1DPROC)load("glTextureStorage1D");
    glad_glTextureStorage2D = (PFNGLTEXTURESTORAGE2DPROC)load("glTextureStorage2D");
    glad_glTextureStorage3D = (PFNGLTEXTURESTORAGE3DPROC)load("glTextureStorage3D");
    glad_glTextureStorage2DMultisample = (PFNGLTEXTURESTORAGE2DMULTISAMPLEPROC)load("glTextureStorage2DMultisample");
    glad_glTextureStorage3DMultisample = (PFNGLTEXTURESTORAGE3DMULTISAMPLEPROC)load("glTextureStorage3DMultisample");
    glad_glTextureSubImage1D = (PFNGLTEXTURESUBIMAGE1DPROC)load("glTextureSubImage1D");
    glad_glTextureSubImage2D = (PFNGLTEXTURESUBIMAGE2DPROC)load("glTextureSubImage2D");
    glad_glTextureSubImage3D = (PFNGLTEXTURESUBIMAGE3DPROC)load("glTextureSubImage3D");
    glad_glCompressedTextureSubImage1D = (PFNGLCOMPRESSEDTEXTURESUBIMAGE1DPROC)load("glCompressedTextureSubImage1D");
    glad_glCompressedTextureSubImage2D = (PFNGLCOMPRESSEDTEXTURESUBIMAGE2DPROC)load("glCompressedTextureSubImage2D");
    glad_glCompressedTextureSubImage3D = (PFNGLCOMPRESSEDTEXTURESUBIMAGE3DPROC)load("glCompressedTextureSubImage3D");
    glad_glCopyTextureSubImage1D = (PFNGLCOPYTEXTURESUBIMAGE1DPROC)load("glCopyTextureSubImage1D");
    glad_glCopyTextureSubImage2D = (PFNGLCOPYTEXTURESUBIMAGE2DPROC)load("glCopyTextureSubImage2D");
    glad_glCopyTextureSubImage3D = (PFNGLCOPYTEXTURESUBIMAGE3DPROC)load("glCopyTextureSubImage3D");
    glad_glTextureParameterf = (PFNGLTEXTUREPARAMETERFPROC)load("glTextureParameterf");
    glad_glTextureParameterfv = (PFNGLTEXTUREPARAMETERFVPROC)load("glTextureParameterfv");
    glad_glTextureParameteri = (PFNGLTEXTUREPARAMETERIPROC)load("glTextureParameteri");
    glad_glTextureParameterIiv = (PFNGLTEXTUREPARAMETERIIVPROC)load("glTextureParameterIiv");
    glad_glTextureParameterIuiv = (PFNGLTEXTUREPARAMETERIUIVPROC)load("glTextureParameterIuiv");
    glad_glTextureParameteriv = (PFNGLTEXTUREPARAMETERIVPROC)load("glTextureParameteriv");
    glad_glGenerateTextureMipmap = (PFNGLGENERATETEXTUREMIPMAPPROC)load("glGenerateTextureMipmap");
    glad_glBindTextureUnit = (PFNGLBINDTEXTUREUNITPROC)load("glBindTextureUnit");
    glad_glGetTextureImage = (PFNGLGETTEXTUREIMAGEPROC)load("glGetTextureImage");
    glad_glGetCompressedTextureImage = (PFNGLGETCOMPRESSEDTEXTUREIMAGEPROC)load("glGetCompressedTextureImage");
    glad_glGetTextureLevelParameterfv = (PFNGLGETTEXTURELEVELPARAMETERFVPROC)load("glGetTextureLevelParameterfv");
    glad_glGetTextureLevelParameteriv = (PFNGLGETTEXTURELEVELPARAMETERIVPROC)load("glGetTextureLevelParameteriv");
    glad_glGetTextureParameterfv = (PFNGLGETTEXTUREPARAMETERFVPROC)load("glGetTextureParameterfv");
    glad_glGetTextureParameterIiv = (PFNGLGETTEXTUREPARAMETERIIVPROC)load("glGetTextureParameterIiv");
    glad_glGetTextureParameterIuiv = (PFNGLGETTEXTUREPARAMETERIUIVPROC)load("glGetTextureParameterIuiv");
    glad_glGetTextureParameteriv = (PFNGLGETTEXTUREPARAMETERIVPROC)load("glGetTextureParameteriv");
    glad_glCreateVertexArrays = (PFNGLCREATEVERTEXARRAYSPROC)load("glCreateVertexArrays");
    glad_glDisableVertexArrayAttrib = (PFNGLDISABLEVERTEXARRAYATTRIBPROC)load("glDisableVertexArrayAttrib");
    glad_glEnableVertexArrayAttrib = (PFNGLENABLEVERTEXARRAYATTRIBPROC)load("glEnableVertexArrayAttrib");
    glad_glVertexArrayElementBuffer = (PFNGLVERTEXARRAYELEMENTBUFFERPROC)load("glVertexArrayElementBuffer");
    glad_glVertexArrayVertexBuffer = (PFNGLVERTEXARRAYVERTEXBUFFERPROC)load("glVertexArrayVertexBuffer");
    glad_glVertexArrayVertexBuffers = (PFNGLVERTEXARRAYVERTEXBUFFERSPROC)load("glVertexArrayVertexBuffers");
    glad_glVertexArrayAttribBinding = (PFNGLVERTEXARRAYATTRIBBINDINGPROC)load("glVertexArrayAttribBinding");
    glad_glVertexArrayAttribFormat = (PFNGLVERTEXARRAYATTRIBFORMATPROC)load("glVertexArrayAttribFormat");
    glad_glVertexArrayAttribIFormat = (PFNGLVERTEXARRAYATTRIBIFORMATPROC)load("glVertexArrayAttribIFormat");
    glad_glVertexArrayAttribLFormat = (PFNGLVERTEXARRAYATTRIBLFORMATPROC)load("glVertexArrayAttribLFormat");
    glad_glVertexArrayBindingDivisor = (PFNGLVERTEXARRAYBINDINGDIVISORPROC)load("glVertexArrayBindingDivisor");
    glad_glGetVertexArrayiv = (PFNGLGETVERTEXARRAYIVPROC)load("glGetVertexArrayiv");
    glad_glGetVertexArrayIndexediv = (PFNGLGETVERTEXARRAYINDEXEDIVPROC)load("glGetVertexArrayIndexediv");
    glad_glGetVertexArrayIndexed64iv = (PFNGLGETVERTEXARRAYINDEXED64IVPROC)load("glGetVertexArrayIndexed64iv");
    glad_glCreateSamplers = (PFNGLCREATESAMPLERSPROC)load("glCreateSamplers");
    glad_glCreateProgramPipelines = (PFNGLCREATEPROGRAMPIPELINESPROC)load("glCreateProgramPipelines");
    glad_glCreateQueries = (PFNGLCREATEQUERIESPROC)load("glCreateQueries");
    glad_glGetQueryBufferObjecti64v = (PFNGLGETQUERYBUFFEROBJECTI64VPROC)load("glGetQueryBufferObjecti64v");
    glad_glGetQueryBufferObjectiv = (PFNGLGETQUERYBUFFEROBJECTIVPROC)load("glGetQueryBufferObjectiv");
    glad_glGetQueryBufferObjectui64v = (PFNGLGETQUERYBUFFEROBJECTUI64VPROC)load("glGetQueryBufferObjectui64v");
    glad_glGetQueryBufferObjectuiv = (PFNGLGETQUERYBUFFEROBJECTUIVPROC)load("glGetQueryBufferObjectuiv");
    glad_glMemoryBarrierByRegion = (PFNGLMEMORYBARRIERBYREGIONPROC)load("glMemoryBarrierByRegion");
    glad_glGetTextureSubImage = (PFNGLGETTEXTURESUBIMAGEPROC)load("glGetTextureSubImage");
    glad_glGetCompressedTextureSubImage = (PFNGLGETCOMPRESSEDTEXTURESUBIMAGEPROC)load("glGetCompressedTextureSubImage");
    glad_glGetGraphicsResetStatus = (PFNGLGETGRAPHICSRESETSTATUSPROC)load("glGetGraphicsResetStatus");
    glad_glGetnCompressedTexImage = (PFNGLGETNCOMPRESSEDTEXIMAGEPROC)load("glGetnCompressedTexImage");
    glad_glGetnTexImage = (PFNGLGETNTEXIMAGEPROC)load("glGetnTexImage");
    glad_glGetnUniformdv = (PFNGLGETNUNIFORMDVPROC)load("glGetnUniformdv");
    glad_glGetnUniformfv = (PFNGLGETNUNIFORMFVPROC)load("glGetnUniformfv");
    glad_glGetnUniformiv = (PFNGLGETNUNIFORMIVPROC)load("glGetnUniformiv");
    glad_glGetnUniformuiv = (PFNGLGETNUNIFORMUIVPROC)load("glGetnUniformuiv");
    glad_glReadnPixels = (PFNGLREADNPIXELSPROC)load("glReadnPixels");
    glad_glGetnMapdv = (PFNGLGETNMAPDVPROC)load("glGetnMapdv");
    glad_glGetnMapfv = (PFNGLGETNMAPFVPROC)load("glGetnMapfv");
    glad_glGetnMapiv = (PFNGLGETNMAPIVPROC)load("glGetnMapiv");
    glad_glGetnPixelMapfv = (PFNGLGETNPIXELMAPFVPROC)load("glGetnPixelMapfv");
    glad_glGetnPixelMapuiv = (PFNGLGETNPIXELMAPUIVPROC)load("glGetnPixelMapuiv");
    glad_glGetnPixelMapusv = (PFNGLGETNPIXELMAPUSVPROC)load("glGetnPixelMapusv");
    glad_glGetnPolygonStipple = (PFNGLGETNPOLYGONSTIPPLEPROC)load("glGetnPolygonStipple");
    glad_glGetnColorTable = (PFNGLGETNCOLORTABLEPROC)load("glGetnColorTable");
    glad_glGetnConvolutionFilter = (PFNGLGETNCONVOLUTIONFILTERPROC)load("glGetnConvolutionFilter");
    glad_glGetnSeparableFilter = (PFNGLGETNSEPARABLEFILTERPROC)load("glGetnSeparableFilter");
    glad_glGetnHistogram = (PFNGLGETNHISTOGRAMPROC)load("glGetnHistogram");
    glad_glGetnMinmax = (PFNGLGETNMINMAXPROC)load("glGetnMinmax");
    glad_glTextureBarrier = (PFNGLTEXTUREBARRIERPROC)load("glTextureBarrier");
}
static int find_extensionsGL(void) {
    if (!get_exts()) return 0;
    (void)&has_ext;
    free_exts();
    return 1;
}

static void find_coreGL(void) {

    /* Thank you @elmindreda
     * https://github.com/elmindreda/greg/blob/master/templates/greg.c.in#L176
     * https://github.com/glfw/glfw/blob/master/src/context.c#L36
     */
    int i, major, minor;

    const char* version;
    const char* prefixes[] = {
        "OpenGL ES-CM ",
        "OpenGL ES-CL ",
        "OpenGL ES ",
        NULL
    };

    version = (const char*) glGetString(GL_VERSION);
    if (!version) return;

    for (i = 0;  prefixes[i];  i++) {
        const size_t length = strlen(prefixes[i]);
        if (strncmp(version, prefixes[i], length) == 0) {
            version += length;
            break;
        }
    }

/* PR #18 */
#ifdef _MSC_VER
    sscanf_s(version, "%d.%d", &major, &minor);
#else
    sscanf(version, "%d.%d", &major, &minor);
#endif

    GLVersion.major = major; GLVersion.minor = minor;
    max_loaded_major = major; max_loaded_minor = minor;
    GLAD_GL_VERSION_1_0 = (major == 1 && minor >= 0) || major > 1;
    GLAD_GL_VERSION_1_1 = (major == 1 && minor >= 1) || major > 1;
    GLAD_GL_VERSION_1_2 = (major == 1 && minor >= 2) || major > 1;
    GLAD_GL_VERSION_1_3 = (major == 1 && minor >= 3) || major > 1;
    GLAD_GL_VERSION_1_4 = (major == 1 && minor >= 4) || major > 1;
    GLAD_GL_VERSION_1_5 = (major == 1 && minor >= 5) || major > 1;
    GLAD_GL_VERSION_2_0 = (major == 2 && minor >= 0) || major > 2;
    GLAD_GL_VERSION_2_1 = (major == 2 && minor >= 1) || major > 2;
    GLAD_GL_VERSION_3_0 = (major == 3 && minor >= 0) || major > 3;
    GLAD_GL_VERSION_3_1 = (major == 3 && minor >= 1) || major > 3;
    GLAD_GL_VERSION_3_2 = (major == 3 && minor >= 2) || major > 3;
    GLAD_GL_VERSION_3_3 = (major == 3 && minor >= 3) || major > 3;
    GLAD_GL_VERSION_4_0 = (major == 4 && minor >= 0) || major > 4;
    GLAD_GL_VERSION_4_1 = (major == 4 && minor >= 1) || major > 4;
    GLAD_GL_VERSION_4_2 = (major == 4 && minor >= 2) || major > 4;
    GLAD_GL_VERSION_4_3 = (major == 4 && minor >= 3) || major > 4;
    GLAD_GL_VERSION_4_4 = (major == 4 && minor >= 4) || major > 4;
    GLAD_GL_VERSION_4_5 = (major == 4 && minor >= 5) || major > 4;
    if (GLVersion.major > 4 || (GLVersion.major >= 4 && GLVersion.minor >= 5)) {
        max_loaded_major = 4;
        max_loaded_minor = 5;
    }
}

int gladLoadGLLoader(GLADloadproc load) {
    GLVersion.major = 0; GLVersion.minor = 0;
    glGetString = (PFNGLGETSTRINGPROC)load("glGetString");
    if(glGetString == NULL) return 0;
    if(glGetString(GL_VERSION) == NULL) return 0;
    find_coreGL();
    load_GL_VERSION_1_0(load);
    load_GL_VERSION_1_1(load);
    load_GL_VERSION_1_2(load);
    load_GL_VERSION_1_3(load);
    load_GL_VERSION_1_4(load);
    load_GL_VERSION_1_5(load);
    load_GL_VERSION_2_0(load);
    load_GL_VERSION_2_1(load);
    load_GL_VERSION_3_0(load);
    load_GL_VERSION_3_1(load);
    load_GL_VERSION_3_2(load);
    load_GL_VERSION_3_3(load);
    load_GL_VERSION_4_0(load);
    load_GL_VERSION_4_1(load);
    load_GL_VERSION_4_2(load);
    load_GL_VERSION_4_3(load);
    load_GL_VERSION_4_4(load);
    load_GL_VERSION_4_5(load);

    if (!find_extensionsGL()) return 0;
    return GLVersion.major != 0 || GLVersion.minor != 0;
}

#endif