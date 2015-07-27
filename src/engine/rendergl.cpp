// rendergl.cpp: core opengl rendering stuff

#include "engine.h"

bool hasTR = false, hasFBO = false, hasAFBO = false, hasDS = false, hasTF = false, hasTRG = false, hasS3TC = false, hasFXT1 = false, hasAF = false, hasNVFB = false, hasFBB = false, hasUBO = false, hasMBR = false;
int hasstencil = 0;

VAR(IDF_READONLY, glversion, 1, 0, 0);
VAR(IDF_READONLY, glslversion, 1, 0, 0);

// OpenGL 1.3
#ifdef WIN32
PFNGLACTIVETEXTUREPROC       glActiveTexture_       = NULL;
PFNGLCLIENTACTIVETEXTUREPROC glClientActiveTexture_ = NULL;

PFNGLBLENDEQUATIONEXTPROC glBlendEquation_ = NULL;
PFNGLBLENDCOLOREXTPROC    glBlendColor_    = NULL;

PFNGLTEXIMAGE3DPROC        glTexImage3D_        = NULL;
PFNGLTEXSUBIMAGE3DPROC     glTexSubImage3D_     = NULL;
PFNGLCOPYTEXSUBIMAGE3DPROC glCopyTexSubImage3D_ = NULL;

PFNGLCOMPRESSEDTEXIMAGE3DPROC    glCompressedTexImage3D_    = NULL;
PFNGLCOMPRESSEDTEXIMAGE2DPROC    glCompressedTexImage2D_    = NULL;
PFNGLCOMPRESSEDTEXIMAGE1DPROC    glCompressedTexImage1D_    = NULL;
PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC glCompressedTexSubImage3D_ = NULL;
PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC glCompressedTexSubImage2D_ = NULL;
PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC glCompressedTexSubImage1D_ = NULL;
PFNGLGETCOMPRESSEDTEXIMAGEPROC   glGetCompressedTexImage_   = NULL;

PFNGLDRAWRANGEELEMENTSPROC glDrawRangeElements_ = NULL;
#endif

// OpenGL 2.1
#ifndef __APPLE__
PFNGLMULTIDRAWARRAYSPROC   glMultiDrawArrays_   = NULL;
PFNGLMULTIDRAWELEMENTSPROC glMultiDrawElements_ = NULL;

PFNGLBLENDFUNCSEPARATEPROC     glBlendFuncSeparate_     = NULL;
PFNGLBLENDEQUATIONSEPARATEPROC glBlendEquationSeparate_ = NULL;
PFNGLSTENCILOPSEPARATEPROC     glStencilOpSeparate_     = NULL;
PFNGLSTENCILFUNCSEPARATEPROC   glStencilFuncSeparate_   = NULL;
PFNGLSTENCILMASKSEPARATEPROC   glStencilMaskSeparate_   = NULL;

PFNGLGENBUFFERSPROC       glGenBuffers_       = NULL;
PFNGLBINDBUFFERPROC       glBindBuffer_       = NULL;
PFNGLMAPBUFFERPROC        glMapBuffer_        = NULL;
PFNGLUNMAPBUFFERPROC      glUnmapBuffer_      = NULL;
PFNGLBUFFERDATAPROC       glBufferData_       = NULL;
PFNGLBUFFERSUBDATAPROC    glBufferSubData_    = NULL;
PFNGLDELETEBUFFERSPROC    glDeleteBuffers_    = NULL;
PFNGLGETBUFFERSUBDATAPROC glGetBufferSubData_ = NULL;

PFNGLGENQUERIESPROC        glGenQueries_        = NULL;
PFNGLDELETEQUERIESPROC     glDeleteQueries_     = NULL;
PFNGLBEGINQUERYPROC        glBeginQuery_        = NULL;
PFNGLENDQUERYPROC          glEndQuery_          = NULL;
PFNGLGETQUERYIVPROC        glGetQueryiv_        = NULL;
PFNGLGETQUERYOBJECTIVPROC  glGetQueryObjectiv_  = NULL;
PFNGLGETQUERYOBJECTUIVPROC glGetQueryObjectuiv_ = NULL;

PFNGLCREATEPROGRAMPROC            glCreateProgram_            = NULL;
PFNGLDELETEPROGRAMPROC            glDeleteProgram_            = NULL;
PFNGLUSEPROGRAMPROC               glUseProgram_               = NULL;
PFNGLCREATESHADERPROC             glCreateShader_             = NULL;
PFNGLDELETESHADERPROC             glDeleteShader_             = NULL;
PFNGLSHADERSOURCEPROC             glShaderSource_             = NULL;
PFNGLCOMPILESHADERPROC            glCompileShader_            = NULL;
PFNGLGETSHADERIVPROC              glGetShaderiv_              = NULL;
PFNGLGETPROGRAMIVPROC             glGetProgramiv_             = NULL;
PFNGLATTACHSHADERPROC             glAttachShader_             = NULL;
PFNGLGETPROGRAMINFOLOGPROC        glGetProgramInfoLog_        = NULL;
PFNGLGETSHADERINFOLOGPROC         glGetShaderInfoLog_         = NULL;
PFNGLLINKPROGRAMPROC              glLinkProgram_              = NULL;
PFNGLGETUNIFORMLOCATIONPROC       glGetUniformLocation_       = NULL;
PFNGLUNIFORM1FPROC                glUniform1f_                = NULL;
PFNGLUNIFORM2FPROC                glUniform2f_                = NULL;
PFNGLUNIFORM3FPROC                glUniform3f_                = NULL;
PFNGLUNIFORM4FPROC                glUniform4f_                = NULL;
PFNGLUNIFORM1FVPROC               glUniform1fv_               = NULL;
PFNGLUNIFORM2FVPROC               glUniform2fv_               = NULL;
PFNGLUNIFORM3FVPROC               glUniform3fv_               = NULL;
PFNGLUNIFORM4FVPROC               glUniform4fv_               = NULL;
PFNGLUNIFORM1IPROC                glUniform1i_                = NULL;
PFNGLUNIFORM2IPROC                glUniform2i_                = NULL;
PFNGLUNIFORM3IPROC                glUniform3i_                = NULL;
PFNGLUNIFORM4IPROC                glUniform4i_                = NULL;
PFNGLUNIFORM1IVPROC               glUniform1iv_               = NULL;
PFNGLUNIFORM2IVPROC               glUniform2iv_               = NULL;
PFNGLUNIFORM3IVPROC               glUniform3iv_               = NULL;
PFNGLUNIFORM4IVPROC               glUniform4iv_               = NULL;
PFNGLUNIFORMMATRIX2FVPROC         glUniformMatrix2fv_         = NULL;
PFNGLUNIFORMMATRIX3FVPROC         glUniformMatrix3fv_         = NULL;
PFNGLUNIFORMMATRIX4FVPROC         glUniformMatrix4fv_         = NULL;
PFNGLBINDATTRIBLOCATIONPROC       glBindAttribLocation_       = NULL;
PFNGLGETACTIVEUNIFORMPROC         glGetActiveUniform_         = NULL;
PFNGLENABLEVERTEXATTRIBARRAYPROC  glEnableVertexAttribArray_  = NULL;
PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray_ = NULL;

PFNGLVERTEXATTRIB1FPROC           glVertexAttrib1f_           = NULL;
PFNGLVERTEXATTRIB1FVPROC          glVertexAttrib1fv_          = NULL;
PFNGLVERTEXATTRIB1SPROC           glVertexAttrib1s_           = NULL;
PFNGLVERTEXATTRIB1SVPROC          glVertexAttrib1sv_          = NULL;
PFNGLVERTEXATTRIB2FPROC           glVertexAttrib2f_           = NULL;
PFNGLVERTEXATTRIB2FVPROC          glVertexAttrib2fv_          = NULL;
PFNGLVERTEXATTRIB2SPROC           glVertexAttrib2s_           = NULL;
PFNGLVERTEXATTRIB2SVPROC          glVertexAttrib2sv_          = NULL;
PFNGLVERTEXATTRIB3FPROC           glVertexAttrib3f_           = NULL;
PFNGLVERTEXATTRIB3FVPROC          glVertexAttrib3fv_          = NULL;
PFNGLVERTEXATTRIB3SPROC           glVertexAttrib3s_           = NULL;
PFNGLVERTEXATTRIB3SVPROC          glVertexAttrib3sv_          = NULL;
PFNGLVERTEXATTRIB4FPROC           glVertexAttrib4f_           = NULL;
PFNGLVERTEXATTRIB4FVPROC          glVertexAttrib4fv_          = NULL;
PFNGLVERTEXATTRIB4SPROC           glVertexAttrib4s_           = NULL;
PFNGLVERTEXATTRIB4SVPROC          glVertexAttrib4sv_          = NULL;
PFNGLVERTEXATTRIB4BVPROC          glVertexAttrib4bv_          = NULL;
PFNGLVERTEXATTRIB4IVPROC          glVertexAttrib4iv_          = NULL;
PFNGLVERTEXATTRIB4UBVPROC         glVertexAttrib4ubv_         = NULL;
PFNGLVERTEXATTRIB4UIVPROC         glVertexAttrib4uiv_         = NULL;
PFNGLVERTEXATTRIB4USVPROC         glVertexAttrib4usv_         = NULL;
PFNGLVERTEXATTRIB4NBVPROC         glVertexAttrib4Nbv_         = NULL;
PFNGLVERTEXATTRIB4NIVPROC         glVertexAttrib4Niv_         = NULL;
PFNGLVERTEXATTRIB4NUBPROC         glVertexAttrib4Nub_         = NULL;
PFNGLVERTEXATTRIB4NUBVPROC        glVertexAttrib4Nubv_        = NULL;
PFNGLVERTEXATTRIB4NUIVPROC        glVertexAttrib4Nuiv_        = NULL;
PFNGLVERTEXATTRIB4NUSVPROC        glVertexAttrib4Nusv_        = NULL;
PFNGLVERTEXATTRIBPOINTERPROC      glVertexAttribPointer_      = NULL;

PFNGLUNIFORMMATRIX2X3FVPROC       glUniformMatrix2x3fv_       = NULL;
PFNGLUNIFORMMATRIX3X2FVPROC       glUniformMatrix3x2fv_       = NULL;
PFNGLUNIFORMMATRIX2X4FVPROC       glUniformMatrix2x4fv_       = NULL;
PFNGLUNIFORMMATRIX4X2FVPROC       glUniformMatrix4x2fv_       = NULL;
PFNGLUNIFORMMATRIX3X4FVPROC       glUniformMatrix3x4fv_       = NULL;
PFNGLUNIFORMMATRIX4X3FVPROC       glUniformMatrix4x3fv_       = NULL;

PFNGLDRAWBUFFERSPROC glDrawBuffers_ = NULL;
#endif

// GL_EXT_framebuffer_object
PFNGLBINDRENDERBUFFERPROC        glBindRenderbuffer_     = NULL;
PFNGLDELETERENDERBUFFERSPROC  glDeleteRenderbuffers_  = NULL;
PFNGLGENFRAMEBUFFERSPROC      glGenRenderbuffers_        = NULL;
PFNGLRENDERBUFFERSTORAGEPROC  glRenderbufferStorage_  = NULL;
PFNGLCHECKFRAMEBUFFERSTATUSPROC  glCheckFramebufferStatus_  = NULL;
PFNGLBINDFRAMEBUFFERPROC      glBindFramebuffer_      = NULL;
PFNGLDELETEFRAMEBUFFERSPROC    glDeleteFramebuffers_   = NULL;
PFNGLGENFRAMEBUFFERSPROC      glGenFramebuffers_      = NULL;
PFNGLFRAMEBUFFERTEXTURE2DPROC    glFramebufferTexture2D_ = NULL;
PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer_ = NULL;
PFNGLGENERATEMIPMAPPROC        glGenerateMipmap_       = NULL;

// GL_EXT_framebuffer_blit
PFNGLBLITFRAMEBUFFERPROC         glBlitFramebuffer_         = NULL;

// GL_ARB_uniform_buffer_object
PFNGLGETUNIFORMINDICESPROC       glGetUniformIndices_       = NULL;
PFNGLGETACTIVEUNIFORMSIVPROC     glGetActiveUniformsiv_     = NULL;
PFNGLGETUNIFORMBLOCKINDEXPROC    glGetUniformBlockIndex_    = NULL;
PFNGLGETACTIVEUNIFORMBLOCKIVPROC glGetActiveUniformBlockiv_ = NULL;
PFNGLUNIFORMBLOCKBINDINGPROC     glUniformBlockBinding_     = NULL;
PFNGLBINDBUFFERBASEPROC          glBindBufferBase_          = NULL;
PFNGLBINDBUFFERRANGEPROC         glBindBufferRange_         = NULL;

// GL_ARB_map_buffer_range
PFNGLMAPBUFFERRANGEPROC         glMapBufferRange_         = NULL;
PFNGLFLUSHMAPPEDBUFFERRANGEPROC glFlushMappedBufferRange_ = NULL;

void *getprocaddress(const char *name)
{
    return SDL_GL_GetProcAddress(name);
}

VAR(IDF_PERSIST, ati_skybox_bug, 0, 0, 1);
VAR(0, ati_minmax_bug, 0, 0, 1);
VAR(0, ati_cubemap_bug, 0, 0, 1);
VAR(0, intel_immediate_bug, 0, 0, 1);
VAR(0, intel_vertexarray_bug, 0, 0, 1);
VAR(0, sdl_backingstore_bug, -1, 0, 1);
VAR(0, minimizetcusage, 1, 0, 0);
VAR(0, useubo, 1, 0, 0);
VAR(0, usetexcompress, 1, 0, 0);
VAR(0, rtscissor, 0, 1, 1);
VAR(0, blurtile, 0, 1, 1);
VAR(0, rtsharefb, 0, 1, 1);

VAR(0, dbgexts, 0, 0, 1);

bool hasext(const char *exts, const char *ext)
{
    int len = strlen(ext);
    if(len) for(const char *cur = exts; (cur = strstr(cur, ext)); cur += len)
    {
        if((cur == exts || cur[-1] == ' ') && (cur[len] == ' ' || !cur[len])) return true;
    }
    return false;
}

SVAR(IDF_READONLY, gfxvendor, "");
SVAR(IDF_READONLY, gfxexts, "");
SVAR(IDF_READONLY, gfxrenderer, "");
SVAR(IDF_READONLY, gfxversion, "");

void gl_checkextensions()
{
    setsvar("gfxvendor", (const char *)glGetString(GL_VENDOR));
    setsvar("gfxexts", (const char *)glGetString(GL_EXTENSIONS));
    setsvar("gfxrenderer", (const char *)glGetString(GL_RENDERER));
    setsvar("gfxversion", (const char *)glGetString(GL_VERSION));

    conoutf("renderer: %s (%s)", gfxrenderer, gfxvendor);
    conoutf("driver: %s", gfxversion);

#ifdef __APPLE__
    extern int mac_osversion();
    int osversion = mac_osversion();  /* 0x0A0500 = 10.5 (Leopard) */
    sdl_backingstore_bug = -1;
#endif

    bool mesa = false, intel = false, ati = false, nvidia = false;
    if(strstr(gfxrenderer, "Mesa") || strstr(gfxversion, "Mesa"))
    {
        mesa = true;
        if(strstr(gfxrenderer, "Intel")) intel = true;
    }
    else if(strstr(gfxvendor, "NVIDIA"))
        nvidia = true;
    else if(strstr(gfxvendor, "ATI") || strstr(gfxvendor, "Advanced Micro Devices"))
        ati = true;
    else if(strstr(gfxvendor, "Intel"))
        intel = true;

    uint glmajorversion, glminorversion;
    if(sscanf(gfxversion, " %u.%u", &glmajorversion, &glminorversion) != 2) glversion = 100;
    else glversion = glmajorversion*100 + glminorversion*10;

    if(glversion < 210) fatal("OpenGL 2.1 or greater is required!");

    const char *glslstr = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
    uint glslmajorversion, glslminorversion;
    if(glslstr && sscanf(glslstr, " %u.%u", &glslmajorversion, &glslminorversion) == 2) glslversion = glslmajorversion*100 + glslminorversion;

    if(glslversion < 120) fatal("GLSL 1.20 or greater is required!");

    GLint val;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &val);
    hwtexsize = val;
    glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &val);
    hwcubetexsize = val;

#ifdef WIN32
    glActiveTexture_ =            (PFNGLACTIVETEXTUREPROC)            getprocaddress("glActiveTexture");
    glClientActiveTexture_ =      (PFNGLCLIENTACTIVETEXTUREPROC)      getprocaddress("glClientActiveTexture");

    glBlendEquation_ =            (PFNGLBLENDEQUATIONPROC)            getprocaddress("glBlendEquation");
    glBlendColor_ =               (PFNGLBLENDCOLORPROC)               getprocaddress("glBlendColor");

    glTexImage3D_ =               (PFNGLTEXIMAGE3DPROC)               getprocaddress("glTexImage3D");
    glTexSubImage3D_ =            (PFNGLTEXSUBIMAGE3DPROC)            getprocaddress("glTexSubImage3D");
    glCopyTexSubImage3D_ =        (PFNGLCOPYTEXSUBIMAGE3DPROC)        getprocaddress("glCopyTexSubImage3D");

    glCompressedTexImage3D_ =     (PFNGLCOMPRESSEDTEXIMAGE3DPROC)     getprocaddress("glCompressedTexImage3D");
    glCompressedTexImage2D_ =     (PFNGLCOMPRESSEDTEXIMAGE2DPROC)     getprocaddress("glCompressedTexImage2D");
    glCompressedTexImage1D_ =     (PFNGLCOMPRESSEDTEXIMAGE1DPROC)     getprocaddress("glCompressedTexImage1D");
    glCompressedTexSubImage3D_ =  (PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC)  getprocaddress("glCompressedTexSubImage3D");
    glCompressedTexSubImage2D_ =  (PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC)  getprocaddress("glCompressedTexSubImage2D");
    glCompressedTexSubImage1D_ =  (PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC)  getprocaddress("glCompressedTexSubImage1D");
    glGetCompressedTexImage_ =    (PFNGLGETCOMPRESSEDTEXIMAGEPROC)    getprocaddress("glGetCompressedTexImage");

    glDrawRangeElements_ =        (PFNGLDRAWRANGEELEMENTSPROC)        getprocaddress("glDrawRangeElements");
#endif

#ifndef __APPLE__
    glMultiDrawArrays_ =          (PFNGLMULTIDRAWARRAYSPROC)          getprocaddress("glMultiDrawArrays");
    glMultiDrawElements_ =        (PFNGLMULTIDRAWELEMENTSPROC)        getprocaddress("glMultiDrawElements");

    glBlendFuncSeparate_ =        (PFNGLBLENDFUNCSEPARATEPROC)        getprocaddress("glBlendFuncSeparate");
    glBlendEquationSeparate_ =    (PFNGLBLENDEQUATIONSEPARATEPROC)    getprocaddress("glBlendEquationSeparate");
    glStencilOpSeparate_ =        (PFNGLSTENCILOPSEPARATEPROC)        getprocaddress("glStencilOpSeparate");
    glStencilFuncSeparate_ =      (PFNGLSTENCILFUNCSEPARATEPROC)      getprocaddress("glStencilFuncSeparate");
    glStencilMaskSeparate_ =      (PFNGLSTENCILMASKSEPARATEPROC)      getprocaddress("glStencilMaskSeparate");

    glGenBuffers_ =               (PFNGLGENBUFFERSPROC)               getprocaddress("glGenBuffers");
    glBindBuffer_ =               (PFNGLBINDBUFFERPROC)               getprocaddress("glBindBuffer");
    glMapBuffer_ =                (PFNGLMAPBUFFERPROC)                getprocaddress("glMapBuffer");
    glUnmapBuffer_ =              (PFNGLUNMAPBUFFERPROC)              getprocaddress("glUnmapBuffer");
    glBufferData_ =               (PFNGLBUFFERDATAPROC)               getprocaddress("glBufferData");
    glBufferSubData_ =            (PFNGLBUFFERSUBDATAPROC)            getprocaddress("glBufferSubData");
    glDeleteBuffers_ =            (PFNGLDELETEBUFFERSPROC)            getprocaddress("glDeleteBuffers");
    glGetBufferSubData_ =         (PFNGLGETBUFFERSUBDATAPROC)         getprocaddress("glGetBufferSubData");

    glGetQueryiv_ =               (PFNGLGETQUERYIVPROC)               getprocaddress("glGetQueryiv");
    glGenQueries_ =               (PFNGLGENQUERIESPROC)               getprocaddress("glGenQueries");
    glDeleteQueries_ =            (PFNGLDELETEQUERIESPROC)            getprocaddress("glDeleteQueries");
    glBeginQuery_ =               (PFNGLBEGINQUERYPROC)               getprocaddress("glBeginQuery");
    glEndQuery_ =                 (PFNGLENDQUERYPROC)                 getprocaddress("glEndQuery");
    glGetQueryObjectiv_ =         (PFNGLGETQUERYOBJECTIVPROC)         getprocaddress("glGetQueryObjectiv");
    glGetQueryObjectuiv_ =        (PFNGLGETQUERYOBJECTUIVPROC)        getprocaddress("glGetQueryObjectuiv");

    glCreateProgram_ =            (PFNGLCREATEPROGRAMPROC)            getprocaddress("glCreateProgram");
    glDeleteProgram_ =            (PFNGLDELETEPROGRAMPROC)            getprocaddress("glDeleteProgram");
    glUseProgram_ =               (PFNGLUSEPROGRAMPROC)               getprocaddress("glUseProgram");
    glCreateShader_ =             (PFNGLCREATESHADERPROC)             getprocaddress("glCreateShader");
    glDeleteShader_ =             (PFNGLDELETESHADERPROC)             getprocaddress("glDeleteShader");
    glShaderSource_ =             (PFNGLSHADERSOURCEPROC)             getprocaddress("glShaderSource");
    glCompileShader_ =            (PFNGLCOMPILESHADERPROC)            getprocaddress("glCompileShader");
    glGetShaderiv_ =              (PFNGLGETSHADERIVPROC)              getprocaddress("glGetShaderiv");
    glGetProgramiv_ =             (PFNGLGETPROGRAMIVPROC)             getprocaddress("glGetProgramiv");
    glAttachShader_ =             (PFNGLATTACHSHADERPROC)             getprocaddress("glAttachShader");
    glGetProgramInfoLog_ =        (PFNGLGETPROGRAMINFOLOGPROC)        getprocaddress("glGetProgramInfoLog");
    glGetShaderInfoLog_ =         (PFNGLGETSHADERINFOLOGPROC)         getprocaddress("glGetShaderInfoLog");
    glLinkProgram_ =              (PFNGLLINKPROGRAMPROC)              getprocaddress("glLinkProgram");
    glGetUniformLocation_ =       (PFNGLGETUNIFORMLOCATIONPROC)       getprocaddress("glGetUniformLocation");
    glUniform1f_ =                (PFNGLUNIFORM1FPROC)                getprocaddress("glUniform1f");
    glUniform2f_ =                (PFNGLUNIFORM2FPROC)                getprocaddress("glUniform2f");
    glUniform3f_ =                (PFNGLUNIFORM3FPROC)                getprocaddress("glUniform3f");
    glUniform4f_ =                (PFNGLUNIFORM4FPROC)                getprocaddress("glUniform4f");
    glUniform1fv_ =               (PFNGLUNIFORM1FVPROC)               getprocaddress("glUniform1fv");
    glUniform2fv_ =               (PFNGLUNIFORM2FVPROC)               getprocaddress("glUniform2fv");
    glUniform3fv_ =               (PFNGLUNIFORM3FVPROC)               getprocaddress("glUniform3fv");
    glUniform4fv_ =               (PFNGLUNIFORM4FVPROC)               getprocaddress("glUniform4fv");
    glUniform1i_ =                (PFNGLUNIFORM1IPROC)                getprocaddress("glUniform1i");
    glUniform2i_ =                (PFNGLUNIFORM2IPROC)                getprocaddress("glUniform2i");
    glUniform3i_ =                (PFNGLUNIFORM3IPROC)                getprocaddress("glUniform3i");
    glUniform4i_ =                (PFNGLUNIFORM4IPROC)                getprocaddress("glUniform4i");
    glUniform1iv_ =               (PFNGLUNIFORM1IVPROC)               getprocaddress("glUniform1iv");
    glUniform2iv_ =               (PFNGLUNIFORM2IVPROC)               getprocaddress("glUniform2iv");
    glUniform3iv_ =               (PFNGLUNIFORM3IVPROC)               getprocaddress("glUniform3iv");
    glUniform4iv_ =               (PFNGLUNIFORM4IVPROC)               getprocaddress("glUniform4iv");
    glUniformMatrix2fv_ =         (PFNGLUNIFORMMATRIX2FVPROC)         getprocaddress("glUniformMatrix2fv");
    glUniformMatrix3fv_ =         (PFNGLUNIFORMMATRIX3FVPROC)         getprocaddress("glUniformMatrix3fv");
    glUniformMatrix4fv_ =         (PFNGLUNIFORMMATRIX4FVPROC)         getprocaddress("glUniformMatrix4fv");
    glBindAttribLocation_ =       (PFNGLBINDATTRIBLOCATIONPROC)       getprocaddress("glBindAttribLocation");
    glGetActiveUniform_ =         (PFNGLGETACTIVEUNIFORMPROC)         getprocaddress("glGetActiveUniform");
    glEnableVertexAttribArray_ =  (PFNGLENABLEVERTEXATTRIBARRAYPROC)  getprocaddress("glEnableVertexAttribArray");
    glDisableVertexAttribArray_ = (PFNGLDISABLEVERTEXATTRIBARRAYPROC) getprocaddress("glDisableVertexAttribArray");

    glVertexAttrib1f_ =           (PFNGLVERTEXATTRIB1FPROC)           getprocaddress("glVertexAttrib1f");
    glVertexAttrib1fv_ =          (PFNGLVERTEXATTRIB1FVPROC)          getprocaddress("glVertexAttrib1fv");
    glVertexAttrib1s_ =           (PFNGLVERTEXATTRIB1SPROC)           getprocaddress("glVertexAttrib1s");
    glVertexAttrib1sv_ =          (PFNGLVERTEXATTRIB1SVPROC)          getprocaddress("glVertexAttrib1sv");
    glVertexAttrib2f_ =           (PFNGLVERTEXATTRIB2FPROC)           getprocaddress("glVertexAttrib2f");
    glVertexAttrib2fv_ =          (PFNGLVERTEXATTRIB2FVPROC)          getprocaddress("glVertexAttrib2fv");
    glVertexAttrib2s_ =           (PFNGLVERTEXATTRIB2SPROC)           getprocaddress("glVertexAttrib2s");
    glVertexAttrib2sv_ =          (PFNGLVERTEXATTRIB2SVPROC)          getprocaddress("glVertexAttrib2sv");
    glVertexAttrib3f_ =           (PFNGLVERTEXATTRIB3FPROC)           getprocaddress("glVertexAttrib3f");
    glVertexAttrib3fv_ =          (PFNGLVERTEXATTRIB3FVPROC)          getprocaddress("glVertexAttrib3fv");
    glVertexAttrib3s_ =           (PFNGLVERTEXATTRIB3SPROC)           getprocaddress("glVertexAttrib3s");
    glVertexAttrib3sv_ =          (PFNGLVERTEXATTRIB3SVPROC)          getprocaddress("glVertexAttrib3sv");
    glVertexAttrib4f_ =           (PFNGLVERTEXATTRIB4FPROC)           getprocaddress("glVertexAttrib4f");
    glVertexAttrib4fv_ =          (PFNGLVERTEXATTRIB4FVPROC)          getprocaddress("glVertexAttrib4fv");
    glVertexAttrib4s_ =           (PFNGLVERTEXATTRIB4SPROC)           getprocaddress("glVertexAttrib4s");
    glVertexAttrib4sv_ =          (PFNGLVERTEXATTRIB4SVPROC)          getprocaddress("glVertexAttrib4sv");
    glVertexAttrib4bv_ =          (PFNGLVERTEXATTRIB4BVPROC)          getprocaddress("glVertexAttrib4bv");
    glVertexAttrib4iv_ =          (PFNGLVERTEXATTRIB4IVPROC)          getprocaddress("glVertexAttrib4iv");
    glVertexAttrib4ubv_ =         (PFNGLVERTEXATTRIB4UBVPROC)         getprocaddress("glVertexAttrib4ubv");
    glVertexAttrib4uiv_ =         (PFNGLVERTEXATTRIB4UIVPROC)         getprocaddress("glVertexAttrib4uiv");
    glVertexAttrib4usv_ =         (PFNGLVERTEXATTRIB4USVPROC)         getprocaddress("glVertexAttrib4usv");
    glVertexAttrib4Nbv_ =         (PFNGLVERTEXATTRIB4NBVPROC)         getprocaddress("glVertexAttrib4Nbv");
    glVertexAttrib4Niv_ =         (PFNGLVERTEXATTRIB4NIVPROC)         getprocaddress("glVertexAttrib4Niv");
    glVertexAttrib4Nub_ =         (PFNGLVERTEXATTRIB4NUBPROC)         getprocaddress("glVertexAttrib4Nub");
    glVertexAttrib4Nubv_ =        (PFNGLVERTEXATTRIB4NUBVPROC)        getprocaddress("glVertexAttrib4Nubv");
    glVertexAttrib4Nuiv_ =        (PFNGLVERTEXATTRIB4NUIVPROC)        getprocaddress("glVertexAttrib4Nuiv");
    glVertexAttrib4Nusv_ =        (PFNGLVERTEXATTRIB4NUSVPROC)        getprocaddress("glVertexAttrib4Nusv");
    glVertexAttribPointer_ =      (PFNGLVERTEXATTRIBPOINTERPROC)      getprocaddress("glVertexAttribPointer");

    glUniformMatrix2x3fv_ =       (PFNGLUNIFORMMATRIX2X3FVPROC)       getprocaddress("glUniformMatrix2x3fv");
    glUniformMatrix3x2fv_ =       (PFNGLUNIFORMMATRIX3X2FVPROC)       getprocaddress("glUniformMatrix3x2fv");
    glUniformMatrix2x4fv_ =       (PFNGLUNIFORMMATRIX2X4FVPROC)       getprocaddress("glUniformMatrix2x4fv");
    glUniformMatrix4x2fv_ =       (PFNGLUNIFORMMATRIX4X2FVPROC)       getprocaddress("glUniformMatrix4x2fv");
    glUniformMatrix3x4fv_ =       (PFNGLUNIFORMMATRIX3X4FVPROC)       getprocaddress("glUniformMatrix3x4fv");
    glUniformMatrix4x3fv_ =       (PFNGLUNIFORMMATRIX4X3FVPROC)       getprocaddress("glUniformMatrix4x3fv");

    glDrawBuffers_ =              (PFNGLDRAWBUFFERSPROC)              getprocaddress("glDrawBuffers");
#endif

    if(hasext(gfxexts, "GL_ARB_texture_float") || hasext(gfxexts, "GL_ATI_texture_float"))
    {
        hasTF = true;
        if(dbgexts) conoutf("\frUsing GL_ARB_texture_float extension");
        setvar("shadowmap", 1, false, true);
        setvar("smoothshadowmappeel", 1, false, true);
    }

    if(hasext(gfxexts, "GL_ARB_texture_rg"))
    {
        hasTRG = true;
        if(dbgexts) conoutf("\frUsing GL_ARB_texture_rg extension.");
    }

    if(hasext(gfxexts, "GL_NV_float_buffer"))
    {
        hasNVFB = true;
        if(dbgexts) conoutf("\frUsing GL_NV_float_buffer extension.");
    }

    if(hasext(gfxexts, "GL_ARB_framebuffer_object"))
    {
        glBindRenderbuffer_        = (PFNGLBINDRENDERBUFFERPROC)       getprocaddress("glBindRenderbufferEXT");
        glDeleteRenderbuffers_     = (PFNGLDELETERENDERBUFFERSPROC)    getprocaddress("glDeleteRenderbuffersEXT");
        glGenRenderbuffers_        = (PFNGLGENFRAMEBUFFERSPROC)        getprocaddress("glGenRenderbuffersEXT");
        glRenderbufferStorage_     = (PFNGLRENDERBUFFERSTORAGEPROC)    getprocaddress("glRenderbufferStorageEXT");
        glCheckFramebufferStatus_  = (PFNGLCHECKFRAMEBUFFERSTATUSPROC) getprocaddress("glCheckFramebufferStatusEXT");
        glBindFramebuffer_         = (PFNGLBINDFRAMEBUFFERPROC)        getprocaddress("glBindFramebufferEXT");
        glDeleteFramebuffers_      = (PFNGLDELETEFRAMEBUFFERSPROC)     getprocaddress("glDeleteFramebuffersEXT");
        glGenFramebuffers_         = (PFNGLGENFRAMEBUFFERSPROC)        getprocaddress("glGenFramebuffersEXT");
        glFramebufferTexture2D_    = (PFNGLFRAMEBUFFERTEXTURE2DPROC)   getprocaddress("glFramebufferTexture2DEXT");
        glFramebufferRenderbuffer_ = (PFNGLFRAMEBUFFERRENDERBUFFERPROC)getprocaddress("glFramebufferRenderbufferEXT");
        glGenerateMipmap_          = (PFNGLGENERATEMIPMAPPROC)         getprocaddress("glGenerateMipmapEXT");
        glBlitFramebuffer_         = (PFNGLBLITFRAMEBUFFERPROC)        getprocaddress("glBlitFramebufferEXT");
        hasAFBO = hasFBO = hasFBB = hasDS = true;
        if(dbgexts) conoutf("\frUsing GL_ARB_framebuffer_object extension.");
    }
    else if(hasext(gfxexts, "GL_EXT_framebuffer_object"))
    {
        glBindRenderbuffer_        = (PFNGLBINDRENDERBUFFERPROC)       getprocaddress("glBindRenderbufferEXT");
        glDeleteRenderbuffers_     = (PFNGLDELETERENDERBUFFERSPROC)    getprocaddress("glDeleteRenderbuffersEXT");
        glGenRenderbuffers_        = (PFNGLGENFRAMEBUFFERSPROC)        getprocaddress("glGenRenderbuffersEXT");
        glRenderbufferStorage_     = (PFNGLRENDERBUFFERSTORAGEPROC)    getprocaddress("glRenderbufferStorageEXT");
        glCheckFramebufferStatus_  = (PFNGLCHECKFRAMEBUFFERSTATUSPROC) getprocaddress("glCheckFramebufferStatusEXT");
        glBindFramebuffer_         = (PFNGLBINDFRAMEBUFFERPROC)        getprocaddress("glBindFramebufferEXT");
        glDeleteFramebuffers_      = (PFNGLDELETEFRAMEBUFFERSPROC)     getprocaddress("glDeleteFramebuffersEXT");
        glGenFramebuffers_         = (PFNGLGENFRAMEBUFFERSPROC)        getprocaddress("glGenFramebuffersEXT");
        glFramebufferTexture2D_    = (PFNGLFRAMEBUFFERTEXTURE2DPROC)   getprocaddress("glFramebufferTexture2DEXT");
        glFramebufferRenderbuffer_ = (PFNGLFRAMEBUFFERRENDERBUFFERPROC)getprocaddress("glFramebufferRenderbufferEXT");
        glGenerateMipmap_          = (PFNGLGENERATEMIPMAPPROC)         getprocaddress("glGenerateMipmapEXT");
        hasFBO = true;
        if(dbgexts) conoutf("\frUsing GL_EXT_framebuffer_object extension.");

        if(hasext(gfxexts, "GL_EXT_framebuffer_blit"))
        {
            glBlitFramebuffer_     = (PFNGLBLITFRAMEBUFFERPROC)        getprocaddress("glBlitFramebufferEXT");
            hasFBB = true;
            if(dbgexts) conoutf("\frUsing GL_EXT_framebuffer_blit extension.");
        }

        if(hasext(gfxexts, "GL_EXT_packed_depth_stencil") || hasext(gfxexts, "GL_NV_packed_depth_stencil"))
        {
            hasDS = true;
            if(dbgexts) conoutf("\frUsing GL_EXT_packed_depth_stencil extension.");
        }
    }
    else fatal("Framebuffer object support is required!");

    if(ati)
    {
        //conoutf("\frWARNING: ATI cards may show garbage in skybox. (use \"/ati_skybox_bug 1\" to fix)");

        minimizetcusage = 1;
        if(hasTF && (hasTRG || hasNVFB)) setvar("fpdepthfx", 1, false, true);
        // On Catalyst 10.2, issuing an occlusion query on the first draw using a given cubemap texture causes a nasty crash
        ati_cubemap_bug = 1;
    }
    else if(nvidia)
    {
        reservevpparams = 10;
        rtsharefb = 0; // work-around for strange driver stalls involving when using many FBOs
        if(glversion < 300 && !hasext(gfxexts, "GL_EXT_gpu_shader4")) setvar("filltjoints", 0, false, true); // DX9 or less NV cards seem to not cause many sparklies

        if(hasTF) setvar("fpdepthfx", 1, false, true);
    }
    else
    {
        if(intel)
        {
#ifdef __APPLE__
            intel_immediate_bug = 1;
#endif
#ifdef WIN32
            intel_immediate_bug = 1;
            intel_vertexarray_bug = 1;
#endif
        }

        reservevpparams = 20;
    }

    if(hasext(gfxexts, "GL_ARB_map_buffer_range"))
    {
        glMapBufferRange_         = (PFNGLMAPBUFFERRANGEPROC)        getprocaddress("glMapBufferRange");
        glFlushMappedBufferRange_ = (PFNGLFLUSHMAPPEDBUFFERRANGEPROC)getprocaddress("glFlushMappedBufferRange");
        hasMBR = true;
        if(dbgexts) conoutf("\frUsing GL_ARB_map_buffer_range.");
    }

    if(hasext(gfxexts, "GL_ARB_uniform_buffer_object"))
    {
        glGetUniformIndices_       = (PFNGLGETUNIFORMINDICESPROC)      getprocaddress("glGetUniformIndices");
        glGetActiveUniformsiv_     = (PFNGLGETACTIVEUNIFORMSIVPROC)    getprocaddress("glGetActiveUniformsiv");
        glGetUniformBlockIndex_    = (PFNGLGETUNIFORMBLOCKINDEXPROC)   getprocaddress("glGetUniformBlockIndex");
        glGetActiveUniformBlockiv_ = (PFNGLGETACTIVEUNIFORMBLOCKIVPROC)getprocaddress("glGetActiveUniformBlockiv");
        glUniformBlockBinding_     = (PFNGLUNIFORMBLOCKBINDINGPROC)    getprocaddress("glUniformBlockBinding");
        glBindBufferBase_          = (PFNGLBINDBUFFERBASEPROC)         getprocaddress("glBindBufferBase");
        glBindBufferRange_         = (PFNGLBINDBUFFERRANGEPROC)        getprocaddress("glBindBufferRange");

        useubo = 1;
        hasUBO = true;
        if(dbgexts) conoutf("\frUsing GL_ARB_uniform_buffer_object extension.");
    }

    if(hasext(gfxexts, "GL_ARB_texture_rectangle"))
    {
        hasTR = true;
        if(dbgexts) conoutf("\frUsing GL_ARB_texture_rectangle extension.");
    }
    else fatal("Texture rectangle support is required!");

    if(hasext(gfxexts, "GL_EXT_texture_compression_s3tc"))
    {
        hasS3TC = true;
#ifdef __APPLE__
        usetexcompress = 1;
#else
        if(!mesa) usetexcompress = 2;
#endif
        if(dbgexts) conoutf("\frUsing GL_EXT_texture_compression_s3tc extension.");
    }
    else if(hasext(gfxexts, "GL_EXT_texture_compression_dxt1") && hasext(gfxexts, "GL_ANGLE_texture_compression_dxt3") && hasext(gfxexts, "GL_ANGLE_texture_compression_dxt5"))
    {
        hasS3TC = true;
        if(dbgexts) conoutf("\frUsing GL_EXT_texture_compression_dxt1 extension.");
    }
    if(hasext(gfxexts, "GL_3DFX_texture_compression_FXT1"))
    {
        hasFXT1 = true;
        if(mesa) usetexcompress = max(usetexcompress, 1);
        if(dbgexts) conoutf("\frUsing GL_3DFX_texture_compression_FXT1.");
    }

    if(hasext(gfxexts, "GL_EXT_texture_filter_anisotropic"))
    {
        GLint val;
        glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &val);
        hwmaxanisotropy = val;
        hasAF = true;
        if(dbgexts) conoutf("\frUsing GL_EXT_texture_filter_anisotropic extension.");
    }

    if(glversion >= 300 || hasext(gfxexts, "GL_EXT_gpu_shader4"))
    {
        // on DX10 or above class cards (i.e. GF8 or RadeonHD) enable expensive features
        setvar("grass", 1, false, true);
        setvar("motionblur", 1, false, true);
        setvar("waterfallrefract", 1, false, true);
        setvar("glare", 1, false, true);
        setvar("maxdynlights", MAXDYNLIGHTS, false, true);
        setvar("depthfxsize", 10, false, true);
        setvar("blurdepthfx", 0, false, true);
    }
}

void glext(char *ext)
{
    if(!*gfxexts) setsvar("gfxexts", (const char *)glGetString(GL_EXTENSIONS));
    intret(hasext(gfxexts, ext) ? 1 : 0);
}
COMMAND(0, glext, "s");

void gl_init(int w, int h, int bpp, int depth, int fsaa)
{
    glViewport(0, 0, w, h);
    glClearColor(0, 0, 0, 0);
    glClearDepth(1);
    glDepthFunc(GL_LESS);
    glDisable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);

    glEnable(GL_LINE_SMOOTH);
    //glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    glFrontFace(GL_CW);
    glCullFace(GL_BACK);
    glDisable(GL_CULL_FACE);

#ifdef __APPLE__
    if(sdl_backingstore_bug)
    {
        if(fsaa)
        {
            sdl_backingstore_bug = 1;
            // since SDL doesn't add kCGLPFABackingStore to the pixelformat and so it isn't guaranteed to be preserved - only manifests when using fsaa?
            //conoutf(CON_WARN, "WARNING: Using SDL backingstore workaround. (use \"/sdl_backingstore_bug 0\" to disable if unnecessary)");
        }
        else sdl_backingstore_bug = -1;
    }
#endif

    extern void setupshaders();
    setupshaders();

    setuptexcompress();
}

void cleanupgl()
{
    extern void cleanupmotionblur();
    cleanupmotionblur();

    extern void clearminimap();
    clearminimap();

    extern void cleanupviews();
    cleanupviews();
}

#define VARRAY_INTERNAL
#include "varray.h"

VAR(0, wireframe, 0, 0, 1);

ICOMMAND(0, getcampos, "", (),
{
    defformatstring(pos, "%s %s %s", floatstr(camera1->o.x), floatstr(camera1->o.y), floatstr(camera1->o.z));
    result(pos);
});

physent camera, *camera1 = &camera;
vec worldpos, camdir, camright, camup;

void findorientation(vec &o, float yaw, float pitch, vec &pos)
{
    vec dir(yaw*RAD, pitch*RAD);
    if(raycubepos(o, dir, pos, 0, RAY_CLIPMAT|RAY_SKIPFIRST) == -1)
        pos = dir.mul(2*hdr.worldsize).add(o); //otherwise gui won't work when outside of map
}

void setcammatrix()
{
    // move from RH to Z-up LH quake style worldspace
    cammatrix = viewmatrix;
    cammatrix.rotate_around_y(camera1->roll*RAD);
    cammatrix.rotate_around_x(camera1->pitch*-RAD);
    cammatrix.rotate_around_z(camera1->yaw*-RAD);
    cammatrix.translate(vec(camera1->o).neg());

    cammatrix.transposedtransformnormal(vec(viewmatrix.b), camdir);
    cammatrix.transposedtransformnormal(vec(viewmatrix.a).neg(), camright);
    cammatrix.transposedtransformnormal(vec(viewmatrix.c), camup);
}

void setcamprojmatrix(bool init = true, bool flush = false)
{
    if(init)
    {   
        setcammatrix();
    }

    camprojmatrix.muld(projmatrix, cammatrix);

    if(init)
    {
        invcammatrix.invert(cammatrix);
        invcamprojmatrix.invert(camprojmatrix);
    }

    GLOBALPARAM(camprojmatrix, camprojmatrix);

    if(fogging)
    {
        vec fogplane(cammatrix.c);
        fogplane.x /= projmatrix.a.x;
        fogplane.y /= projmatrix.b.y;
        fogplane.z /= projmatrix.c.w;
        GLOBALPARAMF(fogplane, fogplane.x, fogplane.y, 0, fogplane.z); 
    }
    else
    {
        vec2 lineardepthscale = projmatrix.lineardepthscale();
        GLOBALPARAMF(fogplane, 0, 0, lineardepthscale.x, lineardepthscale.y);
    }

    if(flush && Shader::lastshader) Shader::lastshader->flushparams();
}

matrix4 hudmatrix, hudmatrixstack[64];
int hudmatrixpos = 0;

void resethudmatrix()
{
    hudmatrixpos = 0;
    GLOBALPARAM(hudmatrix, hudmatrix);
}

void pushhudmatrix()
{
    if(hudmatrixpos >= 0 && hudmatrixpos < int(sizeof(hudmatrixstack)/sizeof(hudmatrixstack[0]))) hudmatrixstack[hudmatrixpos] = hudmatrix;
    ++hudmatrixpos;
}

void flushhudmatrix(bool flushparams)
{
    GLOBALPARAM(hudmatrix, hudmatrix);
    if(flushparams && Shader::lastshader) Shader::lastshader->flushparams();
}

void pophudmatrix(bool flush, bool flushparams)
{
    --hudmatrixpos;
    if(hudmatrixpos >= 0 && hudmatrixpos < int(sizeof(hudmatrixstack)/sizeof(hudmatrixstack[0])))
    {
        hudmatrix = hudmatrixstack[hudmatrixpos];
        if(flush) flushhudmatrix(flushparams);
    }
}

void pushhudscale(float sx, float sy)
{
    if(!sy) sy = sx;
    pushhudmatrix();
    hudmatrix.scale(sx, sy, 1);
    flushhudmatrix();
}

void pushhudtranslate(float tx, float ty, float sx, float sy)
{
    if(!sy) sy = sx;
    pushhudmatrix();
    hudmatrix.translate(tx, ty, 0);
    if(sy) hudmatrix.scale(sx, sy, 1);
    flushhudmatrix();
}

float curfov = 100, fovy, aspect;
FVARN(IDF_PERSIST, aspect, forceaspect, 0, 0, 1e3f);

int farplane, xtraverts, xtravertsva;

VAR(IDF_WORLD, fog, 16, 4000, VAR_MAX);
bvec fogcolor(0x80, 0x99, 0xB3);
VARF(IDF_HEX|IDF_WORLD, fogcolour, 0, 0x8099B3, 0xFFFFFF,
{
    fogcolor = bvec((fogcolour>>16)&0xFF, (fogcolour>>8)&0xFF, fogcolour&0xFF);
});

void vecfromcursor(float x, float y, float z, vec &dir)
{
    vec dir1 = invcamprojmatrix.perspectivetransform(vec(x*2-1, 1-2*y, z*2-1)),
        dir2 = invcamprojmatrix.perspectivetransform(vec(x*2-1, 1-2*y, -1));
    (dir = dir1).sub(dir2).normalize();
}

bool vectocursor(const vec &v, float &x, float &y, float &z, float clampxy)
{
    vec4 clippos;
    camprojmatrix.transform(v, clippos);
    if(clippos.z <= -clippos.w)
    {
        x = y = z = 0;
        return false;
    }

    vec screenpos = vec(clippos).div(clippos.w);
    x = screenpos.x*0.5f + 0.5f;
    y = 0.5f - screenpos.y*0.5f;
    z = screenpos.z*0.5f + 0.5f;

    bool inside = true;
    if(clampxy >= 0)
    {
        if(x <= 0-clampxy) { y += (0-clampxy-x)*(0.5-y)/(0.5-x); x = 0-clampxy; inside = false; }
        else if(x >= 1+clampxy) { y += (1+clampxy-x)*(0.5-y)/(0.5-x); x = 1+clampxy; inside = false; }
        if(y <= 0-clampxy) { x += (0-clampxy-y)*(0.5-x)/(0.5-y); y = 0-clampxy; inside = false; }
        else if(y >= 1+clampxy) { x += (1+clampxy-y)*(0.5-x)/(0.5-y); y = 1+clampxy; inside = false; }
    }
    if(z <= 0) { z = 0; inside = false; }
    else if(z >= 1) { z = 1; inside = false; }
    return inside;
}

extern const matrix4 viewmatrix(vec(-1, 0, 0), vec(0, 0, 1), vec(0, -1, 0));
matrix4 cammatrix, projmatrix, camprojmatrix, invcammatrix, invcamprojmatrix;

FVAR(0, nearplane, 0.01f, 0.54f, 2.0f);

VAR(0, reflectclip, 0, 6, 64);

matrix4 clipmatrix, noclipmatrix;

FVAR(0, polygonoffsetfactor, -1e4f, -3.0f, 1e4f);
FVAR(0, polygonoffsetunits, -1e4f, -3.0f, 1e4f);
FVAR(0, depthoffset, -1e4f, 0.01f, 1e4f);

matrix4 nooffsetmatrix;

void enablepolygonoffset(GLenum type)
{
    if(!depthoffset)
    {
        glPolygonOffset(polygonoffsetfactor, polygonoffsetunits);
        glEnable(type);
        return;
    }

    bool clipped = reflectz < 1e15f && reflectclip;

    nooffsetmatrix = projmatrix;
    projmatrix.d.z += depthoffset * (clipped ? noclipmatrix.c.z : projmatrix.c.z);
    setcamprojmatrix(false, true);
}

void disablepolygonoffset(GLenum type)
{
    if(!depthoffset)
    {
        glDisable(type);
        return;
    }

    projmatrix = nooffsetmatrix;
    setcamprojmatrix(false, true);
}

void calcspherescissor(const vec &center, float size, float &sx1, float &sy1, float &sx2, float &sy2)
{
    vec worldpos(center), e;
    if(reflecting) worldpos.z = 2*reflectz - worldpos.z;
    cammatrix.transform(worldpos, e);
    if(e.z > 2*size) { sx1 = sy1 = 1; sx2 = sy2 = -1; return; }
    float zzrr = e.z*e.z - size*size,
          dx = e.x*e.x + zzrr, dy = e.y*e.y + zzrr,
          focaldist = 1.0f/tan(fovy*0.5f*RAD);
    sx1 = sy1 = -1;
    sx2 = sy2 = 1;
    #define CHECKPLANE(c, dir, focaldist, low, high) \
    do { \
        float nzc = (cz*cz + 1) / (cz dir drt) - cz, \
              pz = (d##c)/(nzc*e.c - e.z); \
        if(pz > 0) \
        { \
            float c = (focaldist)*nzc, \
                  pc = pz*nzc; \
            if(pc < e.c) low = c; \
            else if(pc > e.c) high = c; \
        } \
    } while(0)
    if(dx > 0)
    {
        float cz = e.x/e.z, drt = sqrtf(dx)/size;
        CHECKPLANE(x, -, focaldist/aspect, sx1, sx2);
        CHECKPLANE(x, +, focaldist/aspect, sx1, sx2);
    }
    if(dy > 0)
    {
        float cz = e.y/e.z, drt = sqrtf(dy)/size;
        CHECKPLANE(y, -, focaldist, sy1, sy2);
        CHECKPLANE(y, +, focaldist, sy1, sy2);
    }
}

static int scissoring = 0;
static GLint oldscissor[4];

int pushscissor(float sx1, float sy1, float sx2, float sy2)
{
    scissoring = 0;

    if(sx1 <= -1 && sy1 <= -1 && sx2 >= 1 && sy2 >= 1) return 0;

    sx1 = max(sx1, -1.0f);
    sy1 = max(sy1, -1.0f);
    sx2 = min(sx2, 1.0f);
    sy2 = min(sy2, 1.0f);

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    int sx = viewport[0] + int(floor((sx1+1)*0.5f*viewport[2])),
        sy = viewport[1] + int(floor((sy1+1)*0.5f*viewport[3])),
        sw = viewport[0] + int(ceil((sx2+1)*0.5f*viewport[2])) - sx,
        sh = viewport[1] + int(ceil((sy2+1)*0.5f*viewport[3])) - sy;
    if(sw <= 0 || sh <= 0) return 0;

    if(glIsEnabled(GL_SCISSOR_TEST))
    {
        glGetIntegerv(GL_SCISSOR_BOX, oldscissor);
        sw += sx;
        sh += sy;
        sx = max(sx, int(oldscissor[0]));
        sy = max(sy, int(oldscissor[1]));
        sw = min(sw, int(oldscissor[0] + oldscissor[2])) - sx;
        sh = min(sh, int(oldscissor[1] + oldscissor[3])) - sy;
        if(sw <= 0 || sh <= 0) return 0;
        scissoring = 2;
    }
    else scissoring = 1;

    glScissor(sx, sy, sw, sh);
    if(scissoring<=1) glEnable(GL_SCISSOR_TEST);

    return scissoring;
}

void popscissor()
{
    if(scissoring>1) glScissor(oldscissor[0], oldscissor[1], oldscissor[2], oldscissor[3]);
    else if(scissoring) glDisable(GL_SCISSOR_TEST);
    scissoring = 0;
}

static float findsurface(int fogmat, const vec &v, int &abovemat)
{
    fogmat &= MATF_VOLUME;
    ivec o(v), co;
    int csize;
    do
    {
        cube &c = lookupcube(o.x, o.y, o.z, 0, co, csize);
        int mat = c.material&MATF_VOLUME;
        if(mat != fogmat)
        {
            abovemat = isliquid(mat) ? c.material : MAT_AIR;
            return o.z;
        }
        o.z = co.z + csize;
    }
    while(o.z < hdr.worldsize);
    abovemat = MAT_AIR;
    return hdr.worldsize;
}

static void blendfog(int fogmat, float blend, float logblend, float &start, float &end, vec &fogc)
{
    switch(fogmat&MATF_VOLUME)
    {
        case MAT_WATER:
        {
            const bvec &wcol = getwatercol(fogmat);
            int wfog = getwaterfog(fogmat);
            fogc.madd(wcol.tocolor(), blend);
            end += logblend*min(fog, max(wfog*4, 32));
            break;
        }

        case MAT_LAVA:
        {
            const bvec &lcol = getlavacol(fogmat);
            int lfog = getlavafog(fogmat);
            fogc.madd(lcol.tocolor(), blend);
            end += logblend*min(fog, max(lfog*4, 32));
            break;
        }

        default:
            fogc.madd(fogcolor.tocolor(), blend); 
            start += logblend*(fog+64)/8;
            end += logblend*fog;
            break;
    }
}

vec oldfogcolor(0, 0, 0), curfogcolor(0, 0, 0);
float oldfogstart = 0, oldfogend = 1000000, curfogstart = 0, curfogend = 1000000;

void setfogcolor(const vec &v)
{
    GLOBALPARAM(fogcolor, v);
}

void zerofogcolor()
{
    setfogcolor(vec(0, 0, 0));
}

void resetfogcolor()
{
    setfogcolor(curfogcolor);
}

void pushfogcolor(const vec &v)
{
    oldfogcolor = curfogcolor;
    curfogcolor = v;
    resetfogcolor();
}

void popfogcolor()
{
    curfogcolor = oldfogcolor;
    resetfogcolor();
}

void setfogdist(float start, float end)
{
    GLOBALPARAMF(fogparams, 1/(end - start), end/(end - start));
}

void clearfogdist()
{
    setfogdist(0, 1000000);
}

void resetfogdist()
{
    setfogdist(curfogstart, curfogend);
}

void pushfogdist(float start, float end)
{
    oldfogstart = curfogstart;
    oldfogend = curfogend;
    curfogstart = start;
    curfogend = end;
    resetfogdist();
}

void popfogdist()
{
    curfogstart = oldfogstart;
    curfogend = oldfogend;
    resetfogdist();
}

static void resetfog()
{
    resetfogcolor();
    resetfogdist();

    glClearColor(curfogcolor.r, curfogcolor.g, curfogcolor.b, 1.0f);
}

static void setfog(int fogmat, float below = 1, int abovemat = MAT_AIR)
{
    float logscale = 256, logblend = log(1 + (logscale - 1)*below) / log(logscale);

    curfogstart = curfogend = 0;
    curfogcolor = vec(0, 0, 0);
    blendfog(fogmat, below, logblend, curfogstart, curfogend, curfogcolor);
    if(below < 1) blendfog(abovemat, 1-below, 1-logblend, curfogstart, curfogend, curfogcolor);

    resetfog();
}

static void setnofog(const vec &color = vec(0, 0, 0))
{
    curfogstart = 0;
    curfogend = 1000000;
    curfogcolor = color;
    
    resetfog();
}

bool deferdrawtextures = false;

void drawtextures()
{
    if(minimized) { deferdrawtextures = true; return; }
    deferdrawtextures = false;
    genenvmaps();
    drawminimap();
}

GLuint motiontex = 0;
int motionw = 0, motionh = 0, lastmotion = 0;

void cleanupmotionblur()
{
    if(motiontex) { glDeleteTextures(1, &motiontex); motiontex = 0; }
    motionw = motionh = 0;
    lastmotion = 0;
}

VARF(IDF_PERSIST, motionblur, 0, 0, 1, { if(!motionblur) cleanupmotionblur(); });
VAR(IDF_PERSIST, motionblurmillis, 1, 5, 1000);
FVAR(IDF_PERSIST, motionblurscale, 0, 1, 1);

void addmotionblur()
{
    extern int viewtype;
    if(!motionblur || viewtype || max(screen->w, screen->h) > hwtexsize) return;

    if(!motiontex || motionw != screen->w || motionh != screen->h)
    {
        if(!motiontex) glGenTextures(1, &motiontex);
        motionw = screen->w;
        motionh = screen->h;
        lastmotion = 0;
        createtexture(motiontex, motionw, motionh, NULL, 3, 0, GL_RGB, GL_TEXTURE_RECTANGLE);
    }

    float amount = min(hud::motionblur(motionblurscale), 1.0f);
    if(amount <= 0)
    {
        lastmotion = 0;
        return;
    }

    glBindTexture(GL_TEXTURE_RECTANGLE, motiontex);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    SETSHADER(screenrect);

    glColor4f(1, 1, 1, lastmotion ? pow(amount, max(float(totalmillis-lastmotion)/motionblurmillis, 1.0f)) : 0);
    glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(      0,       0); glVertex2f(-1, -1);
    glTexCoord2f(motionw,       0); glVertex2f( 1, -1);
    glTexCoord2f(      0, motionh); glVertex2f(-1,  1);
    glTexCoord2f(motionw, motionh); glVertex2f( 1,  1);
    glEnd();

    glDisable(GL_BLEND);

    if(totalmillis-lastmotion >= motionblurmillis)
    {
        lastmotion = totalmillis-totalmillis%motionblurmillis;

        glCopyTexSubImage2D(GL_TEXTURE_RECTANGLE, 0, 0, 0, 0, 0, screen->w, screen->h);
    }
}

bool dopostfx = false;

void invalidatepostfx()
{
    dopostfx = false;
}

static void blendfogoverlay(int fogmat, float blend, vec &overlay)
{
    float maxc;
    switch(fogmat&MATF_VOLUME)
    {
        case MAT_WATER:
        {
            const bvec &wcol = getwatercol(fogmat);
            maxc = max(wcol.r, max(wcol.g, wcol.b));
            overlay.madd(vec(wcol.r, wcol.g, wcol.b).div(min(32.0f + maxc*7.0f/8.0f, 255.0f)).max(0.4f), blend);
            break;
        }

        case MAT_LAVA:
        {
            const bvec &lcol = getlavacol(fogmat);
            maxc = max(lcol.r, max(lcol.g, lcol.b));
            overlay.madd(vec(lcol.r, lcol.g, lcol.b).div(min(32.0f + maxc*7.0f/8.0f, 255.0f)).max(0.4f), blend);
            break;
        }

        default:
            overlay.add(blend);
            break;
    }
}

void drawfogoverlay(int fogmat, float fogblend, int abovemat)
{
    SETSHADER(fogoverlay);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ZERO, GL_SRC_COLOR);
    vec overlay(0, 0, 0);
    blendfogoverlay(fogmat, fogblend, overlay);
    blendfogoverlay(abovemat, 1-fogblend, overlay);

    glColor3fv(overlay.v);
    glBegin(GL_TRIANGLE_STRIP);
    glVertex2f(-1, -1);
    glVertex2f(1, -1);
    glVertex2f(-1, 1);
    glVertex2f(1, 1);
    glEnd();
    glDisable(GL_BLEND);
}

bool renderedgame = false, renderedavatar = false;

void rendergame()
{
    game::render();
    if(!shadowmapping) renderedgame = true;
}

matrix4 oldprojmatrix;

void setavatarscale(float zscale)
{
    projmatrix = oldprojmatrix;
    projmatrix.scalez(zscale);
    setcamprojmatrix(false);
}

void renderavatar(bool early, bool project)
{
    if(project) oldprojmatrix = projmatrix;
    game::renderavatar(early, project);
    if(project)
    {
        projmatrix = oldprojmatrix;
        setcamprojmatrix(false);
    }
}

VAR(IDF_PERSIST, skyboxglare, 0, 1, 1);

void drawglare()
{
    glaring = true;
    refracting = -1;

    pushfogcolor(vec(0, 0, 0));

    glClearColor(0, 0, 0, 1);
    extern int skyglare, skybgglare;
    glClear((skyboxglare && (skyglare || skybgglare) ? 0 : GL_COLOR_BUFFER_BIT) | GL_DEPTH_BUFFER_BIT);

    if(skyboxglare && limitsky()) drawskybox(farplane, true);

    rendergeom();

    if(skyboxglare && !limitsky()) drawskybox(farplane, false);

    renderreflectedmapmodels();
    rendergame();

    renderwater();
    rendermaterials();
    renderalphageom();
    renderparticles();
    renderavatar(false, true);

    popfogcolor();

    refracting = 0;
    glaring = false;
}

VAR(IDF_PERSIST, reflectmms, 0, 1, 1);
VAR(IDF_WORLD, refractsky, 0, 0, 1);

matrix4 noreflectmatrix;

void drawreflection(float z, bool refract, int fogdepth, const bvec &col)
{
    reflectz = z < 0 ? 1e16f : z;
    reflecting = !refract;
    refracting = refract ? (z < 0 || camera1->o.z >= z ? -1 : 1) : 0;
    fading = waterrefract && waterfade && z>=0;
    fogging = refracting<0 && z>=0;
    refractfog = fogdepth;

    if(fogging)
    {
        pushfogdist(camera1->o.z - z, camera1->o.z - (z - max(refractfog, 1)));
        pushfogcolor(col.tocolor());
    }
    else
    {
        vec color(0, 0, 0);
        float start = 0, end = 0;
        blendfog(MAT_AIR, 1, 1, start, end, color);
        pushfogdist(start, end);
        pushfogcolor(color);
    }

    if(fading)
    {
        float scale = fogging ? -0.25f : 0.25f, offset = 2*fabs(scale) - scale*z;
        GLOBALPARAMF(waterfadeparams, scale, offset, -scale, offset + camera1->o.z*scale);
    }

    if(reflecting)
    {
        noreflectmatrix = cammatrix;
        cammatrix.reflectz(z);

        glFrontFace(GL_CCW);
    }

    if(reflectclip && z>=0)
    {
        float zoffset = reflectclip/4.0f, zclip;
        if(refracting<0)
        {
            zclip = z+zoffset;
            if(camera1->o.z<=zclip) zclip = z;
        }
        else
        {
            zclip = z-zoffset;
            if(camera1->o.z>=zclip && camera1->o.z<=z+4.0f) zclip = z;
            if(reflecting) zclip = 2*z - zclip;
        }
        plane clipplane;
        invcammatrix.transposedtransform(plane(0, 0, refracting>0 ? 1 : -1, refracting>0 ? -zclip : zclip), clipplane);
        clipmatrix.clip(clipplane, projmatrix);
        noclipmatrix = projmatrix;
        projmatrix = clipmatrix;
    }

    setcamprojmatrix(false, true);

    renderreflectedgeom(refracting<0 && z>=0 && caustics, fogging);

    if(reflecting || refracting>0 || (refracting<0 && refractsky) || z<0)
    {
        if(fading) glColorMask(COLORMASK, GL_TRUE);
        if(reflectclip && z>=0)
        {   
            projmatrix = noclipmatrix;
            setcamprojmatrix(false, true);
        }
        drawskybox(farplane, false);
        if(reflectclip && z>=0)
        {
            projmatrix = clipmatrix;
            setcamprojmatrix(false, true);
        }
        if(fading) glColorMask(COLORMASK, GL_FALSE);
    }
    else if(fading) glColorMask(COLORMASK, GL_FALSE);

    renderdecals();

    if(reflectmms) renderreflectedmapmodels();
    rendergame();

    if(refracting) rendergrass();
    rendermaterials();
    renderalphageom(fogging);
    renderparticles();

    if(game::thirdpersonview() || reflecting) renderavatar();

    if(fading) glColorMask(COLORMASK, GL_TRUE);

    if(reflectclip && z>=0) projmatrix = noclipmatrix;

    if(reflecting)
    {
        cammatrix = noreflectmatrix;

        glFrontFace(GL_CW);
    }

    popfogdist();
    popfogcolor();

    reflectz = 1e16f;
    refracting = 0;
    reflecting = fading = fogging = false;

    setcamprojmatrix(false, true);
}

int drawtex = 0;

void drawcubemap(int size, int level, const vec &o, float yaw, float pitch, bool flipx, bool flipy, bool swapxy)
{
    drawtex = DRAWTEX_ENVMAP;

    physent *oldcamera = camera1;
    static physent cmcamera;
    cmcamera = *camera1;
    cmcamera.reset();
    cmcamera.type = ENT_CAMERA;
    cmcamera.o = o;
    cmcamera.yaw = yaw;
    cmcamera.pitch = pitch;
    cmcamera.roll = 0;
    camera1 = &cmcamera;
    setviewcell(camera1->o);

    updatedynlights();

    int fogmat = lookupmaterial(camera1->o)&(MATF_VOLUME|MATF_INDEX), abovemat = MAT_AIR;
    float fogblend = 1.0f, causticspass = 0.0f;
    if(isliquid(fogmat&MATF_VOLUME))
    {
        float z = findsurface(fogmat, camera1->o, abovemat) - WATER_OFFSET;
        if(camera1->o.z < z + 1) fogblend = min(z + 1 - camera1->o.z, 1.0f);
        else fogmat = abovemat;
        if(level > 1 && caustics && (fogmat&MATF_VOLUME)==MAT_WATER && camera1->o.z < z)
            causticspass = min(z - camera1->o.z, 1.0f);
    }
    else
    {
        fogmat = MAT_AIR;
    }
    setfog(fogmat, fogblend, abovemat);

    int farplane = hdr.worldsize*2;

    projmatrix.perspective(90.0f, 1.0f, nearplane, farplane);
    if(flipx || flipy) projmatrix.scalexy(flipx ? -1 : 1, flipy ? -1 : 1);
    if(swapxy)
    {
        swap(projmatrix.a.x, projmatrix.a.y);
        swap(projmatrix.b.x, projmatrix.b.y);
        swap(projmatrix.c.x, projmatrix.c.y);
        swap(projmatrix.d.x, projmatrix.d.y);
    }
    setcamprojmatrix();

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    xtravertsva = xtraverts = glde = gbatches = 0;

    visiblecubes();

    glClear(GL_DEPTH_BUFFER_BIT);

    if(limitsky()) drawskybox(farplane, true);
    rendergeom(level > 1 ? causticspass : 0);
    if(level > 1) queryreflections();
    if(level) generategrass();
    if(!limitsky()) drawskybox(farplane, false);
    if(level > 1) renderdecals();

    rendermapmodels();
    renderalphageom();

    if(level) rendergame();
    if(level > 1)
    {
        drawglaretex();
        drawdepthfxtex();
        drawreflections();
        renderwater(); // water screws up for some reason
    }
    rendergrass();
    rendermaterials();
    renderparticles();

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    if(level > 1) addglare();

    camera1 = oldcamera;
    drawtex = 0;
}

namespace modelpreview
{
    physent *oldcamera;
    physent camera;

    void start(bool background)
    {
        drawtex = DRAWTEX_MODELPREVIEW;

        oldcamera = camera1;
        camera = *camera1;
        camera.reset();
        camera.type = ENT_CAMERA;
        camera.o = vec(0, 0, 0);
        camera.yaw = 0;
        camera.pitch = 0;
        camera.roll = 0;
        camera1 = &camera;

        clearfogdist();
        zerofogcolor();
        glClearColor(0, 0, 0, 1);

        glClear((background ? GL_COLOR_BUFFER_BIT : 0) | GL_DEPTH_BUFFER_BIT);

        projmatrix.perspective(90.0f, 1.0f, nearplane, 1024);
        setcamprojmatrix();

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
    }

    void end()
    {
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);

        resetfogdist();
        resetfogcolor();
        glClearColor(curfogcolor.r, curfogcolor.g, curfogcolor.b, 1);

        camera1 = oldcamera;
        drawtex = 0;
    }
}

GLuint minimaptex = 0;
vec minimapcenter(0, 0, 0), minimapradius(0, 0, 0), minimapscale(0, 0, 0);

void clearminimap()
{
    if(minimaptex) { glDeleteTextures(1, &minimaptex); minimaptex = 0; }
}

VAR(IDF_WORLD, minimapheight, 0, 0, 2<<16);
bvec minimapcolor(0, 0, 0);
VARF(IDF_HEX|IDF_WORLD, minimapcolour, 0, 0, 0xFFFFFF,
{
    minimapcolor = bvec((minimapcolour>>16)&0xFF, (minimapcolour>>8)&0xFF, minimapcolour&0xFF);
});
VAR(IDF_WORLD, minimapclip, 0, 0, 1);
VARF(IDF_PERSIST, minimapsize, 7, 8, 10, { if(minimaptex) drawminimap(); });

void bindminimap()
{
    glBindTexture(GL_TEXTURE_2D, minimaptex);
}

void clipminimap(ivec &bbmin, ivec &bbmax, cube *c = worldroot, int x = 0, int y = 0, int z = 0, int size = hdr.worldsize>>1)
{
    loopi(8)
    {
        ivec o(i, x, y, z, size);
        if(c[i].children) clipminimap(bbmin, bbmax, c[i].children, o.x, o.y, o.z, size>>1);
        else if(!isentirelysolid(c[i]) && (c[i].material&MATF_CLIP)!=MAT_CLIP)
        {
            loopk(3) bbmin[k] = min(bbmin[k], o[k]);
            loopk(3) bbmax[k] = max(bbmax[k], o[k] + size);
        }
    }
}

void drawminimap()
{
    if(!hud::needminimap()) { clearminimap(); return; }

    progress(0, "generating mini-map...");

    int size = 1<<minimapsize, sizelimit = min(hwtexsize, min(screen->w, screen->h));
    while(size > sizelimit) size /= 2;
    if(!minimaptex) glGenTextures(1, &minimaptex);

    extern vector<vtxarray *> valist;
    ivec bbmin(hdr.worldsize, hdr.worldsize, hdr.worldsize), bbmax(0, 0, 0);
    loopv(valist)
    {
        vtxarray *va = valist[i];
        loopk(3)
        {
            if(va->geommin[k]>va->geommax[k]) continue;
            bbmin[k] = min(bbmin[k], va->geommin[k]);
            bbmax[k] = max(bbmax[k], va->geommax[k]);
        }
    }
    if(minimapclip)
    {
        ivec clipmin(hdr.worldsize, hdr.worldsize, hdr.worldsize), clipmax(0, 0, 0);
        clipminimap(clipmin, clipmax);
        loopk(2) bbmin[k] = max(bbmin[k], clipmin[k]);
        loopk(2) bbmax[k] = min(bbmax[k], clipmax[k]);
    }

    minimapradius = vec(bbmax).sub(vec(bbmin)).mul(0.5f);
    minimapcenter = vec(bbmin).add(minimapradius);
    minimapradius.x = minimapradius.y = max(minimapradius.x, minimapradius.y);
    minimapscale = vec((0.5f - 1.0f/size)/minimapradius.x, (0.5f - 1.0f/size)/minimapradius.y, 1.0f);

    drawtex = DRAWTEX_MINIMAP;

    physent *oldcamera = camera1;
    static physent cmcamera;
    cmcamera = *camera1;
    cmcamera.reset();
    cmcamera.type = ENT_CAMERA;
    cmcamera.o = vec(minimapcenter.x, minimapcenter.y, max(minimapcenter.z + minimapradius.z + 1, float(minimapheight)));
    cmcamera.yaw = 0;
    cmcamera.pitch = -90;
    cmcamera.roll = 0;
    camera1 = &cmcamera;
    setviewcell(vec(-1, -1, -1));

    projmatrix.ortho(-minimapradius.x, minimapradius.x, -minimapradius.y, minimapradius.y, 0, camera1->o.z + 1);
    projmatrix.a.mul(-1); 
    setcamprojmatrix();

    setnofog(minimapcolor.tocolor());

    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    glViewport(0, 0, size, size);

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    glFrontFace(GL_CCW);

    xtravertsva = xtraverts = glde = gbatches = 0;

    visiblecubes(false);
    queryreflections();
    drawreflections();

    loopi(minimapheight > 0 && minimapheight < minimapcenter.z + minimapradius.z ? 2 : 1)
    {
        if(i)
        {
            glClear(GL_DEPTH_BUFFER_BIT);
            camera1->o.z = minimapheight;
            setcamprojmatrix();
        }
        rendergeom();
        rendermapmodels();
        renderwater();
        rendermaterials();
        renderalphageom();
    }

    glFrontFace(GL_CW);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    glViewport(0, 0, screen->w, screen->h);

    camera1 = oldcamera;
    drawtex = 0;

    glBindTexture(GL_TEXTURE_2D, minimaptex);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5, 0, 0, size, size, 0);
    setuptexparameters(minimaptex, NULL, 3, 1, GL_RGB5, GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    GLfloat border[4] = { minimapcolor.x/255.0f, minimapcolor.y/255.0f, minimapcolor.z/255.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    glBindTexture(GL_TEXTURE_2D, 0);
}

VAR(IDF_PERSIST, scr_virtw, 0, 1024, VAR_MAX);
VAR(IDF_PERSIST, scr_virth, 0, 768, VAR_MAX);
VAR(IDF_PERSIST, scr_minw, 0, 640, VAR_MAX);
VAR(IDF_PERSIST, scr_minh, 0, 480, VAR_MAX);

void getscreenres(int &w, int &h)
{
    float wk = 1, hk = 1;
    if(w < scr_virtw) wk = float(scr_virtw)/w;
    if(h < scr_virth) hk = float(scr_virth)/h;
    wk = hk = max(wk, hk);
    w = int(ceil(w*wk));
    h = int(ceil(h*hk));
}

void gettextres(int &w, int &h)
{
    if(w < scr_minw || h < scr_minh)
    {
        if(scr_minw > w*scr_minh/h)
        {
            h = h*scr_minw/w;
            w = scr_minw;
        }
        else
        {
            w = w*scr_minh/h;
            h = scr_minh;
        }
    }
}

void drawslice(float start, float length, float x, float y, float size)
{
    float end = start + length,
          sx = cosf((start + 0.25f)*2*M_PI), sy = -sinf((start + 0.25f)*2*M_PI),
          ex = cosf((end + 0.25f)*2*M_PI), ey = -sinf((end + 0.25f)*2*M_PI);
    glBegin(GL_TRIANGLE_FAN);
    glTexCoord2f(0.5f, 0.5f); glVertex2f(x, y);

    if(start < 0.125f || start >= 0.875f) { glTexCoord2f(0.5f + 0.5f*sx/sy, 0); glVertex2f(x + sx/sy*size, y - size);  }
    else if(start < 0.375f) { glTexCoord2f(1, 0.5f - 0.5f*sy/sx); glVertex2f(x + size, y - sy/sx*size); }
    else if(start < 0.625f) { glTexCoord2f(0.5f - 0.5f*sx/sy, 1); glVertex2f(x - sx/sy*size, y + size); }
    else { glTexCoord2f(0, 0.5f + 0.5f*sy/sx); glVertex2f(x - size, y + sy/sx*size); }

    if(start <= 0.125f && end >= 0.125f) { glTexCoord2f(1, 0); glVertex2f(x + size, y - size); }
    if(start <= 0.375f && end >= 0.375f) { glTexCoord2f(1, 1); glVertex2f(x + size, y + size); }
    if(start <= 0.625f && end >= 0.625f) { glTexCoord2f(0, 1); glVertex2f(x - size, y + size); }
    if(start <= 0.875f && end >= 0.875f) { glTexCoord2f(0, 0); glVertex2f(x - size, y - size); }

    if(end < 0.125f || end >= 0.875f) { glTexCoord2f(0.5f + 0.5f*ex/ey, 0); glVertex2f(x + ex/ey*size, y - size);  }
    else if(end < 0.375f) { glTexCoord2f(1, 0.5f - 0.5f*ey/ex); glVertex2f(x + size, y - ey/ex*size); }
    else if(end < 0.625f) { glTexCoord2f(0.5f - 0.5f*ex/ey, 1); glVertex2f(x - ex/ey*size, y + size); }
    else { glTexCoord2f(0, 0.5f + 0.5f*ey/ex); glVertex2f(x - size, y + ey/ex*size); }
    glEnd();
}

void drawfadedslice(float start, float length, float x, float y, float size, float alpha, float r, float g, float b, float minsize)
{
    float end = start + length,
          sx = cosf((start + 0.25f)*2*M_PI), sy = -sinf((start + 0.25f)*2*M_PI),
          ex = cosf((end + 0.25f)*2*M_PI), ey = -sinf((end + 0.25f)*2*M_PI);

    #define SLICEVERT(ox, oy) \
    { \
        glTexCoord2f(0.5f + (ox)*0.5f, 0.5f + (oy)*0.5f); \
        glVertex2f(x + (ox)*size, y + (oy)*size); \
    }
    #define SLICESPOKE(ox, oy) \
    { \
        SLICEVERT((ox)*minsize, (oy)*minsize); \
        SLICEVERT(ox, oy); \
    }

    glBegin(GL_TRIANGLE_STRIP);
    glColor4f(r, g, b, alpha);
    if(start < 0.125f || start >= 0.875f) SLICESPOKE(sx/sy, -1)
    else if(start < 0.375f) SLICESPOKE(1, -sy/sx)
    else if(start < 0.625f) SLICESPOKE(-sx/sy, 1)
    else SLICESPOKE(-1, sy/sx)

    if(start <= 0.125f && end >= 0.125f) { glColor4f(r, g, b, alpha - alpha*(0.125f - start)/(end - start)); SLICESPOKE(1, -1) }
    if(start <= 0.375f && end >= 0.375f) { glColor4f(r, g, b, alpha - alpha*(0.375f - start)/(end - start)); SLICESPOKE(1, 1) }
    if(start <= 0.625f && end >= 0.625f) { glColor4f(r, g, b, alpha - alpha*(0.625f - start)/(end - start)); SLICESPOKE(-1, 1) }
    if(start <= 0.875f && end >= 0.875f) { glColor4f(r, g, b, alpha - alpha*(0.875f - start)/(end - start)); SLICESPOKE(-1, -1) }

    glColor4f(r, g, b, 0);
    if(end < 0.125f || end >= 0.875f) SLICESPOKE(ex/ey, -1)
    else if(end < 0.375f) SLICESPOKE(1, -ey/ex)
    else if(end < 0.625f) SLICESPOKE(-ex/ey, 1)
    else SLICESPOKE(-1, ey/ex)
    glEnd();
}

float cursorx = 0.5f, cursory = 0.5f;
vec cursordir(0, 0, 0);

struct framebuffercopy
{
    GLuint tex;
    int w, h;

    framebuffercopy() : tex(0), w(0), h(0) {}

    void cleanup()
    {
        if(!tex) return;
        glDeleteTextures(1, &tex);
        tex = 0;
    }

    void setup()
    {
        if(tex) return;
        glGenTextures(1, &tex);
        w = screen->w;
        h = screen->h;
        createtexture(tex, w, h, NULL, 3, false, GL_RGB, GL_TEXTURE_RECTANGLE);
    }

    void copy()
    {
        if(w != screen->w || h != screen->h) cleanup();
        if(!tex) setup();

        glBindTexture(GL_TEXTURE_RECTANGLE, tex);
        glCopyTexSubImage2D(GL_TEXTURE_RECTANGLE, 0, 0, 0, 0, 0, screen->w, screen->h);
    }

    void draw(float sx, float sy, float sw, float sh)
    {
        sx = sx*2 - 1;
        sy = sy*2 - 1;
        sw *= 2;
        sh *= 2;
        float tx = 0, ty = 0, tw = screen->w, th = screen->h;
        SETSHADER(screenrect);
        glBindTexture(GL_TEXTURE_RECTANGLE, tex);
        glBegin(GL_TRIANGLE_STRIP);
        glTexCoord2f(tx,    ty);    glVertex2f(sx,    sy);
        glTexCoord2f(tx+tw, ty);    glVertex2f(sx+sw, sy);
        glTexCoord2f(tx,    ty+th); glVertex2f(sx,    sy+sh);
        glTexCoord2f(tx+tw, ty+th); glVertex2f(sx+sw, sy+sh);
        glEnd();
    }
};

enum { VW_NORMAL = 0, VW_LEFTRIGHT, VW_CROSSEYED, VW_STEREO, VW_STEREO_BLEND = VW_STEREO, VW_STEREO_BLEND_REDCYAN, VW_STEREO_AVG, VW_MAX, VW_STEREO_REDCYAN = VW_MAX };
enum { VP_LEFT, VP_RIGHT, VP_MAX, VP_CAMERA = VP_MAX };

framebuffercopy views[VP_MAX];

void cleanupviews()
{
    loopi(VP_MAX) views[i].cleanup();
}

VARF(IDF_PERSIST, viewtype, VW_NORMAL, VW_NORMAL, VW_MAX, cleanupviews());
FVAR(IDF_PERSIST, stereoblend, 0, 0.5f, 1);
FVAR(IDF_PERSIST, stereodist, 0, 0.5f, 10000);
FVAR(IDF_PERSIST, stereoplane, 1e-3f, 40.f, 1000);
FVAR(IDF_PERSIST, stereonear, 0, 2.f, 10000);

int fogmat = MAT_AIR, abovemat = MAT_AIR;
float fogblend = 1.0f, causticspass = 0.0f;

GLenum colormask[3] = { GL_TRUE, GL_TRUE, GL_TRUE };

void setcolormask(bool r, bool g, bool b)
{
    colormask[0] = r ? GL_TRUE : GL_FALSE;
    colormask[1] = g ? GL_TRUE : GL_FALSE;
    colormask[2] = b ? GL_TRUE : GL_FALSE;
}

bool needsview(int v, int targtype)
{
    switch(v)
    {
        case VW_NORMAL: return targtype == VP_CAMERA;
        case VW_LEFTRIGHT:
        case VW_CROSSEYED: return targtype == VP_LEFT || targtype == VP_RIGHT;
        case VW_STEREO_BLEND:
        case VW_STEREO_BLEND_REDCYAN: return targtype >= VP_LEFT && targtype <= VP_CAMERA;
        case VW_STEREO_AVG:
        case VW_STEREO_REDCYAN: return targtype == VP_LEFT || targtype == VP_RIGHT;
    }
    return false;
}

bool copyview(int v, int targtype)
{
    switch(v)
    {
        case VW_LEFTRIGHT:
        case VW_CROSSEYED: return targtype == VP_LEFT || targtype == VP_RIGHT;
        case VW_STEREO_BLEND:
        case VW_STEREO_BLEND_REDCYAN: return targtype == VP_RIGHT;
        case VW_STEREO_AVG: return targtype == VP_LEFT;
    }
    return false;
}

bool clearview(int v, int targtype)
{
    switch(v)
    {
        case VW_STEREO_BLEND:
        case VW_STEREO_BLEND_REDCYAN: return targtype == VP_LEFT || targtype == VP_CAMERA;
        case VW_STEREO_REDCYAN: return targtype == VP_LEFT;
    }
    return true;
}

void viewproject(int targtype = VP_CAMERA)
{
    if(targtype != VP_LEFT && targtype != VP_RIGHT) projmatrix.perspective(fovy, aspect, nearplane, farplane);
    else
    {
        float top = stereonear*tan(fovy/2*RAD), right = aspect*top, iod = stereodist/2;
        if(targtype == VP_RIGHT) iod = -iod;
        float fs = iod*stereonear/stereoplane;
        projmatrix.frustum(-right+fs, right+fs, -top, top, stereonear, farplane);
        projmatrix.translate(iod, 0, 0);
    }
}

void drawnoviewtype(int targtype)
{
    if(targtype == VP_LEFT || targtype == VP_RIGHT)
    {
        if(viewtype >= VW_STEREO)
        {
            switch(viewtype)
            {
                case VW_STEREO_BLEND: setcolormask(targtype == VP_LEFT, false, targtype == VP_RIGHT); break;
                case VW_STEREO_AVG: setcolormask(targtype == VP_LEFT, true, targtype == VP_RIGHT); break;
                case VW_STEREO_BLEND_REDCYAN:
                case VW_STEREO_REDCYAN: setcolormask(targtype == VP_LEFT, targtype == VP_RIGHT, targtype == VP_RIGHT); break;
            }
            glColorMask(COLORMASK, GL_TRUE);
        }
    }

    xtravertsva = xtraverts = glde = gbatches = 0;

    int w = screen->w, h = screen->h;
    if(forceaspect) w = int(ceil(h*forceaspect));
    gettextres(w, h);

    hudmatrix.ortho(0, w, h, 0, -1, 1);
    resethudmatrix();

    glClearColor(0.f, 0.f, 0.f, 1);
    if(clearview(viewtype, targtype)) glClear(GL_COLOR_BUFFER_BIT);

    hudshader->set();

    hud::update(screen->w, screen->h);
    hud::drawhud(true);
    hud::drawlast();

    if(targtype == VP_LEFT || targtype == VP_RIGHT)
    {
        if(viewtype >= VW_STEREO)
        {
            setcolormask();
            glColorMask(COLORMASK, GL_TRUE);
        }
    }
}

void drawnoview()
{
    int copies = 0, oldcurtime = curtime;
    loopi(VP_MAX) if(needsview(viewtype, i))
    {
        drawnoviewtype(i);
        if(copyview(viewtype, i))
        {
            views[i].copy();
            copies++;
        }
        curtime = 0;
    }
    if(needsview(viewtype, VP_CAMERA)) drawnoviewtype(VP_CAMERA);
    curtime = oldcurtime;

    if(!copies) return;

    glDisable(GL_BLEND);
    glColor3f(1.f, 1.f, 1.f);
    switch(viewtype)
    {
        case VW_LEFTRIGHT:
        {
            views[VP_LEFT].draw(0, 0, 0.5f, 1);
            views[VP_RIGHT].draw(0.5f, 0, 0.5f, 1);
            break;
        }
        case VW_CROSSEYED:
        {
            views[VP_LEFT].draw(0.5f, 0, 0.5f, 1);
            views[VP_RIGHT].draw(0, 0, 0.5f, 1);
            break;
        }
        case VW_STEREO_BLEND:
        case VW_STEREO_BLEND_REDCYAN:
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            if(viewtype == VW_STEREO_BLEND) glColorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_TRUE);
            glColor4f(1.f, 1.f, 1.f, stereoblend); views[VP_RIGHT].draw(0, 0, 1, 1);
            if(viewtype == VW_STEREO_BLEND) glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glDisable(GL_BLEND);
            break;
        }
        case VW_STEREO_AVG:
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_CONSTANT_COLOR);
            glBlendColor_(0.f, 0.5f, 1.f, 1.f);
            glColor3f(1.f, 0.5f, 0.f);
            views[VP_LEFT].draw(0, 0, 1, 1);
            glDisable(GL_BLEND);
            break;
        }
    }
}

void drawviewtype(int targtype)
{
    updatedynlights();

    setfog(fogmat, fogblend, abovemat);
    viewproject(targtype);
    setcamprojmatrix();
    if(targtype == VP_LEFT || targtype == VP_RIGHT)
    {
        if(viewtype >= VW_STEREO)
        {
            switch(viewtype)
            {
                case VW_STEREO_BLEND: setcolormask(targtype == VP_LEFT, false, targtype == VP_RIGHT); break;
                case VW_STEREO_AVG: setcolormask(targtype == VP_LEFT, true, targtype == VP_RIGHT); break;
                case VW_STEREO_BLEND_REDCYAN:
                case VW_STEREO_REDCYAN: setcolormask(targtype == VP_LEFT, targtype == VP_RIGHT, targtype == VP_RIGHT); break;
            }
            glColorMask(COLORMASK, GL_TRUE);
        }
    }

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    xtravertsva = xtraverts = glde = gbatches = 0;

    visiblecubes();

    glClearColor(0, 0, 0, 0);
    glClear(GL_DEPTH_BUFFER_BIT|(wireframe && editmode && clearview(viewtype, targtype) ? GL_COLOR_BUFFER_BIT : 0)|(hasstencil ? GL_STENCIL_BUFFER_BIT : 0));

    if(wireframe && editmode) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    if(limitsky()) drawskybox(farplane, true);

    rendergeom(causticspass);
    if(!wireframe && editmode && (outline || (fullbright && blankgeom))) renderoutline();

    queryreflections();
    generategrass();

    if(!limitsky()) drawskybox(farplane, false);

    renderdecals(true);
    renderavatar(true);

    rendermapmodels();
    rendergame();

    if(wireframe && editmode) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    drawglaretex();
    drawdepthfxtex();
    drawreflections();

    if(wireframe && editmode) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    renderwater();
    rendergrass();

    rendermaterials();
    renderalphageom();

    if(wireframe && editmode) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    renderparticles(true);

    if(wireframe && editmode) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    renderavatar(false, true);

    if(wireframe && editmode) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    addmotionblur();
    addglare();
    if(isliquid(fogmat&MATF_VOLUME)) drawfogoverlay(fogmat, fogblend, abovemat);
    renderpostfx();

    notextureshader->set();
    if(editmode && !pixeling)
    {
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        renderblendbrush();
        rendereditcursor();

        glDepthMask(GL_TRUE);
        glDisable(GL_DEPTH_TEST);
    }

    int w = screen->w, h = screen->h;
    if(forceaspect) w = int(ceil(h*forceaspect));
    gettextres(w, h);

    hudmatrix.ortho(0, w, h, 0, -1, 1);
    resethudmatrix();

    glColor3f(1, 1, 1);

    if(!pixeling || !editmode)
    {
        extern int debugsm;
        if(debugsm)
        {
            extern void viewshadowmap();
            viewshadowmap();
        }

        extern int debugglare;
        if(debugglare)
        {
            extern void viewglaretex();
            viewglaretex();
        }

        extern int debugdepthfx;
        if(debugdepthfx)
        {
            extern void viewdepthfxtex();
            viewdepthfxtex();
        }

        extern void debugparticles();
        debugparticles();

        hudshader->set();
        hud::drawhud();
        rendertexturepanel(w, h);
        hud::drawlast();
    }

    renderedgame = false;

    if(targtype == VP_LEFT || targtype == VP_RIGHT)
    {
        if(viewtype >= VW_STEREO)
        {
            setcolormask();
            glColorMask(COLORMASK, GL_TRUE);
        }
    }
}

bool hasnoview()
{
    return client::waiting()>0;
}

void gl_drawframe(int w, int h)
{
    if(hasnoview()) drawnoview();
    else
    {
        if(deferdrawtextures) drawtextures();

        fogmat = lookupmaterial(camera1->o)&(MATF_VOLUME|MATF_INDEX);
        causticspass = 0.f;
        if(isliquid(fogmat&MATF_VOLUME))
        {
            float z = findsurface(fogmat, camera1->o, abovemat) - WATER_OFFSET;
            if(camera1->o.z < z + 1) fogblend = min(z + 1 - camera1->o.z, 1.0f);
            else fogmat = abovemat;
            if(caustics && (fogmat&MATF_VOLUME) == MAT_WATER && camera1->o.z < z)
                causticspass = min(z - camera1->o.z, 1.0f);

            float blend = abovemat == MAT_AIR ? fogblend : 1.0f;
            fovy += blend*sinf(lastmillis/1000.0)*2.0f;
            aspect += blend*sinf(lastmillis/1000.0+PI)*0.1f;
        }
        else fogmat = MAT_AIR;

        farplane = hdr.worldsize*2;
        viewproject();
        setcamprojmatrix();
        game::project(w, h);

        int copies = 0, oldcurtime = curtime;
        loopi(VP_MAX) if(needsview(viewtype, i))
        {
            drawviewtype(i);
            if(copyview(viewtype, i))
            {
                views[i].copy();
                copies++;
            }
            curtime = 0;
        }
        if(needsview(viewtype, VP_CAMERA)) drawviewtype(VP_CAMERA);
        curtime = oldcurtime;

        if(!copies) return;

        glDisable(GL_BLEND);
        glColor3f(1.f, 1.f, 1.f);
        switch(viewtype)
        {
            case VW_LEFTRIGHT:
            {
                views[VP_LEFT].draw(0, 0, 0.5f, 1);
                views[VP_RIGHT].draw(0.5f, 0, 0.5f, 1);
                break;
            }
            case VW_CROSSEYED:
            {
                views[VP_LEFT].draw(0.5f, 0, 0.5f, 1);
                views[VP_RIGHT].draw(0, 0, 0.5f, 1);
                break;
            }
            case VW_STEREO_BLEND:
            case VW_STEREO_BLEND_REDCYAN:
            {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                if(viewtype == VW_STEREO_BLEND) glColorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_TRUE);
                glColor4f(1.f, 1.f, 1.f, stereoblend); views[VP_RIGHT].draw(0, 0, 1, 1);
                if(viewtype == VW_STEREO_BLEND) glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                glDisable(GL_BLEND);
                break;
            }
            case VW_STEREO_AVG:
            {
                glEnable(GL_BLEND);
                glBlendFunc(GL_ONE, GL_CONSTANT_COLOR);
                glBlendColor_(0.f, 0.5f, 1.f, 1.f);
                glColor3f(1.f, 0.5f, 0.f);
                views[VP_LEFT].draw(0, 0, 1, 1);
                glDisable(GL_BLEND);
                break;
            }
        }
    }
}

void usetexturing(bool on)
{
    if(on)
        hudshader->set();
    else
        hudnotextureshader->set();
}

FVAR(IDF_PERSIST, polycolour, 0, 1, 1);
FVAR(IDF_PERSIST, polylight, 0, 1, 1);
FVAR(IDF_PERSIST, polybright, 0, 0.65f, 1);

void polyhue(dynent *d, vec &colour, int flags)
{
    if(flags&MDL_LIGHT && d->light.millis != lastmillis)
    {
        vec lightpos = d->type == ENT_PLAYER || d->type == ENT_AI ? d->feetpos(0.75f*(d->height + d->aboveeye)) : d->o;
        lightreaching(lightpos, d->light.color, d->light.dir, (flags&MDL_LIGHT_FAST)!=0);
        dynlightreaching(lightpos, d->light.color, d->light.dir);
        if(flags&MDL_LIGHTFX) d->light.color.max(d->light.effect).lerp(d->light.effect, 0.6f);
        d->light.millis = lastmillis;
    }
    vec c = vec(colour).mul(polycolour).mul(vec(d->light.color).mul(polylight));
    loopi(3) colour[i] = max(c[i], colour[i]*polybright);
}

void polybox(vec o, float tofloor, float toceil, float xradius, float yradius)
{
    glBegin(GL_QUADS);
    loopi(6) loopj(4)
    {
        const ivec &cc = facecoords[i][j];
        glVertex3f(o.x + (cc.x ? xradius : -xradius), o.y + (cc.y ? yradius : -yradius), o.z + (cc.z ? toceil : -tofloor));
    }
    glEnd();
    xtraverts += 24;
}
