// rendergl.cpp: core opengl rendering stuff

#include "engine.h"

bool hasVBO = false, hasDRE = false, hasMDA = false, hasOQ = false, hasTR = false, hasFBO = false, hasDS = false, hasTF = false, hasTRG = false, hasBE = false, hasBC = false, hasCM = false, hasNP2 = false, hasTC = false, hasS3TC = false, hasFXT1 = false, hasMT = false, hasAF = false, hasGLSL = false, hasNVFB = false, hasDT = false, hasPBO = false, hasFBB = false, hasUBO = false, hasMBR = false;
int hasstencil = 0;

VAR(IDF_READONLY, glversion, 1, 0, 0);
VAR(IDF_READONLY, glslversion, 1, 0, 0);

// GL_ARB_vertex_buffer_object, GL_ARB_pixel_buffer_object
PFNGLGENBUFFERSARBPROC       glGenBuffers_       = NULL;
PFNGLBINDBUFFERARBPROC       glBindBuffer_       = NULL;
PFNGLMAPBUFFERARBPROC        glMapBuffer_        = NULL;
PFNGLUNMAPBUFFERARBPROC      glUnmapBuffer_      = NULL;
PFNGLBUFFERDATAARBPROC       glBufferData_       = NULL;
PFNGLBUFFERSUBDATAARBPROC    glBufferSubData_    = NULL;
PFNGLDELETEBUFFERSARBPROC    glDeleteBuffers_    = NULL;
PFNGLGETBUFFERSUBDATAARBPROC glGetBufferSubData_ = NULL;

// GL_ARB_multitexture
PFNGLACTIVETEXTUREARBPROC       glActiveTexture_        = NULL;
PFNGLCLIENTACTIVETEXTUREARBPROC glClientActiveTexture_ = NULL;
PFNGLMULTITEXCOORD2FARBPROC  glMultiTexCoord2f_  = NULL;
PFNGLMULTITEXCOORD3FARBPROC  glMultiTexCoord3f_  = NULL;
PFNGLMULTITEXCOORD4FARBPROC  glMultiTexCoord4f_     = NULL;

// GL_ARB_occlusion_query
PFNGLGENQUERIESARBPROC      glGenQueries_       = NULL;
PFNGLDELETEQUERIESARBPROC    glDeleteQueries_    = NULL;
PFNGLBEGINQUERYARBPROC      glBeginQuery_       = NULL;
PFNGLENDQUERYARBPROC          glEndQuery_         = NULL;
PFNGLGETQUERYIVARBPROC      glGetQueryiv_       = NULL;
PFNGLGETQUERYOBJECTIVARBPROC  glGetQueryObjectiv_  = NULL;
PFNGLGETQUERYOBJECTUIVARBPROC glGetQueryObjectuiv_ = NULL;

// GL_EXT_framebuffer_object
PFNGLBINDRENDERBUFFEREXTPROC        glBindRenderbuffer_     = NULL;
PFNGLDELETERENDERBUFFERSEXTPROC  glDeleteRenderbuffers_  = NULL;
PFNGLGENFRAMEBUFFERSEXTPROC      glGenRenderbuffers_        = NULL;
PFNGLRENDERBUFFERSTORAGEEXTPROC  glRenderbufferStorage_  = NULL;
PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC  glCheckFramebufferStatus_  = NULL;
PFNGLBINDFRAMEBUFFEREXTPROC      glBindFramebuffer_      = NULL;
PFNGLDELETEFRAMEBUFFERSEXTPROC    glDeleteFramebuffers_   = NULL;
PFNGLGENFRAMEBUFFERSEXTPROC      glGenFramebuffers_      = NULL;
PFNGLFRAMEBUFFERTEXTURE2DEXTPROC    glFramebufferTexture2D_ = NULL;
PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC glFramebufferRenderbuffer_ = NULL;
PFNGLGENERATEMIPMAPEXTPROC        glGenerateMipmap_       = NULL;

// GL_EXT_framebuffer_blit
PFNGLBLITFRAMEBUFFEREXTPROC         glBlitFramebuffer_         = NULL;

// OpenGL 2.0: GL_ARB_shading_language_100, GL_ARB_shader_objects, GL_ARB_fragment_shader, GL_ARB_vertex_shader
#ifndef __APPLE__
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
PFNGLBINDATTRIBLOCATIONPROC       glBindAttribLocation_       = NULL;
PFNGLGETACTIVEUNIFORMPROC         glGetActiveUniform_         = NULL;
PFNGLENABLEVERTEXATTRIBARRAYPROC  glEnableVertexAttribArray_  = NULL;
PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray_ = NULL;
PFNGLVERTEXATTRIBPOINTERPROC      glVertexAttribPointer_      = NULL;

PFNGLUNIFORMMATRIX2X3FVPROC       glUniformMatrix2x3fv_       = NULL;
PFNGLUNIFORMMATRIX3X2FVPROC       glUniformMatrix3x2fv_       = NULL;
PFNGLUNIFORMMATRIX2X4FVPROC       glUniformMatrix2x4fv_       = NULL;
PFNGLUNIFORMMATRIX4X2FVPROC       glUniformMatrix4x2fv_       = NULL;
PFNGLUNIFORMMATRIX3X4FVPROC       glUniformMatrix3x4fv_       = NULL;
PFNGLUNIFORMMATRIX4X3FVPROC       glUniformMatrix4x3fv_       = NULL;
#endif

// GL_EXT_draw_range_elements
PFNGLDRAWRANGEELEMENTSEXTPROC glDrawRangeElements_ = NULL;

// GL_EXT_blend_minmax
PFNGLBLENDEQUATIONEXTPROC glBlendEquation_ = NULL;

// GL_EXT_blend_color
PFNGLBLENDCOLOREXTPROC glBlendColor_ = NULL;

// GL_EXT_multi_draw_arrays
PFNGLMULTIDRAWARRAYSEXTPROC   glMultiDrawArrays_ = NULL;
PFNGLMULTIDRAWELEMENTSEXTPROC glMultiDrawElements_ = NULL;

// GL_ARB_texture_compression
PFNGLCOMPRESSEDTEXIMAGE3DARBPROC    glCompressedTexImage3D_    = NULL;
PFNGLCOMPRESSEDTEXIMAGE2DARBPROC    glCompressedTexImage2D_    = NULL;
PFNGLCOMPRESSEDTEXIMAGE1DARBPROC    glCompressedTexImage1D_    = NULL;
PFNGLCOMPRESSEDTEXSUBIMAGE3DARBPROC glCompressedTexSubImage3D_ = NULL;
PFNGLCOMPRESSEDTEXSUBIMAGE2DARBPROC glCompressedTexSubImage2D_ = NULL;
PFNGLCOMPRESSEDTEXSUBIMAGE1DARBPROC glCompressedTexSubImage1D_ = NULL;
PFNGLGETCOMPRESSEDTEXIMAGEARBPROC   glGetCompressedTexImage_   = NULL;

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
VAR(0, usetexrect, 1, 0, 0);
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

    //extern int shaderprecision;
    // default to low precision shaders on certain cards, can be overridden with -f3
    // char *weakcards[] = { "GeForce FX", "Quadro FX", "6200", "9500", "9550", "9600", "9700", "9800", "X300", "X600", "FireGL", "Intel", "Chrome", NULL }
    // if(shaderprecision==2) for(char **wc = weakcards; *wc; wc++) if(strstr(gfxrenderer, *wc)) shaderprecision = 1;

    GLint val;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &val);
    hwtexsize = val;

    if(hasext(gfxexts, "GL_ARB_multitexture"))
    {
        glActiveTexture_       = (PFNGLACTIVETEXTUREARBPROC)      getprocaddress("glActiveTextureARB");
        glClientActiveTexture_ = (PFNGLCLIENTACTIVETEXTUREARBPROC)getprocaddress("glClientActiveTextureARB");
        glMultiTexCoord2f_     = (PFNGLMULTITEXCOORD2FARBPROC)    getprocaddress("glMultiTexCoord2fARB");
        glMultiTexCoord3f_     = (PFNGLMULTITEXCOORD3FARBPROC)    getprocaddress("glMultiTexCoord3fARB");
        glMultiTexCoord4f_     = (PFNGLMULTITEXCOORD4FARBPROC)    getprocaddress("glMultiTexCoord4fARB");
        hasMT = true;
        if(dbgexts) conoutf("\frUsing GL_ARB_multitexture extension.");
    }
    else conoutf("\frWARNING: No multitexture extension");

    if(hasext(gfxexts, "GL_ARB_vertex_buffer_object"))
    {
        hasVBO = true;
        if(dbgexts) conoutf("\frUsing GL_ARB_vertex_buffer_object extension.");
    }
    else conoutf("\frWARNING: No vertex_buffer_object extension! (geometry heavy maps will be SLOW)");
#ifdef __APPLE__
    /* VBOs over 256KB seem to destroy performance on 10.5, but not in 10.6 */
    extern int maxvbosize;
    if(osversion < 0x0A0600) maxvbosize = min(maxvbosize, 8192);
#endif

    if(hasext(gfxexts, "GL_ARB_pixel_buffer_object"))
    {
        hasPBO = true;
        if(dbgexts) conoutf("\frUsing GL_ARB_pixel_buffer_object extension.");
    }

    if(hasVBO || hasPBO)
    {
        glGenBuffers_       = (PFNGLGENBUFFERSARBPROC)      getprocaddress("glGenBuffersARB");
        glBindBuffer_       = (PFNGLBINDBUFFERARBPROC)      getprocaddress("glBindBufferARB");
        glMapBuffer_        = (PFNGLMAPBUFFERARBPROC)       getprocaddress("glMapBufferARB");
        glUnmapBuffer_      = (PFNGLUNMAPBUFFERARBPROC)     getprocaddress("glUnmapBufferARB");
        glBufferData_       = (PFNGLBUFFERDATAARBPROC)      getprocaddress("glBufferDataARB");
        glBufferSubData_    = (PFNGLBUFFERSUBDATAARBPROC)   getprocaddress("glBufferSubDataARB");
        glDeleteBuffers_    = (PFNGLDELETEBUFFERSARBPROC)   getprocaddress("glDeleteBuffersARB");
        glGetBufferSubData_ = (PFNGLGETBUFFERSUBDATAARBPROC)getprocaddress("glGetBufferSubDataARB");
    }

    if(hasext(gfxexts, "GL_EXT_draw_range_elements"))
    {
        glDrawRangeElements_ = (PFNGLDRAWRANGEELEMENTSEXTPROC)getprocaddress("glDrawRangeElementsEXT");
        hasDRE = true;
        if(dbgexts) conoutf("\frUsing GL_EXT_draw_range_elements extension.");
    }

    if(hasext(gfxexts, "GL_EXT_multi_draw_arrays"))
    {
        glMultiDrawArrays_   = (PFNGLMULTIDRAWARRAYSEXTPROC)  getprocaddress("glMultiDrawArraysEXT");
        glMultiDrawElements_ = (PFNGLMULTIDRAWELEMENTSEXTPROC)getprocaddress("glMultiDrawElementsEXT");
        hasMDA = true;
        if(dbgexts) conoutf("\frUsing GL_EXT_multi_draw_arrays extension.");
    }

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

    if(hasext(gfxexts, "GL_EXT_framebuffer_object"))
    {
        glBindRenderbuffer_        = (PFNGLBINDRENDERBUFFEREXTPROC)       getprocaddress("glBindRenderbufferEXT");
        glDeleteRenderbuffers_     = (PFNGLDELETERENDERBUFFERSEXTPROC)    getprocaddress("glDeleteRenderbuffersEXT");
        glGenRenderbuffers_        = (PFNGLGENFRAMEBUFFERSEXTPROC)        getprocaddress("glGenRenderbuffersEXT");
        glRenderbufferStorage_     = (PFNGLRENDERBUFFERSTORAGEEXTPROC)    getprocaddress("glRenderbufferStorageEXT");
        glCheckFramebufferStatus_  = (PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC) getprocaddress("glCheckFramebufferStatusEXT");
        glBindFramebuffer_         = (PFNGLBINDFRAMEBUFFEREXTPROC)        getprocaddress("glBindFramebufferEXT");
        glDeleteFramebuffers_      = (PFNGLDELETEFRAMEBUFFERSEXTPROC)     getprocaddress("glDeleteFramebuffersEXT");
        glGenFramebuffers_         = (PFNGLGENFRAMEBUFFERSEXTPROC)        getprocaddress("glGenFramebuffersEXT");
        glFramebufferTexture2D_    = (PFNGLFRAMEBUFFERTEXTURE2DEXTPROC)   getprocaddress("glFramebufferTexture2DEXT");
        glFramebufferRenderbuffer_ = (PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC)getprocaddress("glFramebufferRenderbufferEXT");
        glGenerateMipmap_          = (PFNGLGENERATEMIPMAPEXTPROC)         getprocaddress("glGenerateMipmapEXT");
        hasFBO = true;
        if(dbgexts) conoutf("\frUsing GL_EXT_framebuffer_object extension.");

        if(hasext(gfxexts, "GL_EXT_framebuffer_blit"))
        {
            glBlitFramebuffer_     = (PFNGLBLITFRAMEBUFFEREXTPROC)        getprocaddress("glBlitFramebufferEXT");
            hasFBB = true;
            if(dbgexts) conoutf("\frUsing GL_EXT_framebuffer_blit extension.");
        }
    }
    else conoutf("\frWARNING: No framebuffer object support. (reflective water may be slow)");

    if(hasext(gfxexts, "GL_ARB_occlusion_query"))
    {
        glGetQueryiv_ = (PFNGLGETQUERYIVARBPROC)getprocaddress("glGetQueryivARB");
        glGenQueries_ =        (PFNGLGENQUERIESARBPROC)       getprocaddress("glGenQueriesARB");
        glDeleteQueries_ =     (PFNGLDELETEQUERIESARBPROC)    getprocaddress("glDeleteQueriesARB");
        glBeginQuery_ =        (PFNGLBEGINQUERYARBPROC)       getprocaddress("glBeginQueryARB");
        glEndQuery_ =          (PFNGLENDQUERYARBPROC)         getprocaddress("glEndQueryARB");
        glGetQueryObjectiv_ =  (PFNGLGETQUERYOBJECTIVARBPROC) getprocaddress("glGetQueryObjectivARB");
        glGetQueryObjectuiv_ = (PFNGLGETQUERYOBJECTUIVARBPROC)getprocaddress("glGetQueryObjectuivARB");
        hasOQ = true;
        if(dbgexts) conoutf("\frUsing GL_ARB_occlusion_query extension.");
    }

    if(glversion >= 200)
    {
#ifndef __APPLE__
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
        glBindAttribLocation_ =       (PFNGLBINDATTRIBLOCATIONPROC)       getprocaddress("glBindAttribLocation");
        glGetActiveUniform_ =         (PFNGLGETACTIVEUNIFORMPROC)         getprocaddress("glGetActiveUniform");
        glEnableVertexAttribArray_ =  (PFNGLENABLEVERTEXATTRIBARRAYPROC)  getprocaddress("glEnableVertexAttribArray");
        glDisableVertexAttribArray_ = (PFNGLDISABLEVERTEXATTRIBARRAYPROC) getprocaddress("glDisableVertexAttribArray");
        glVertexAttribPointer_ =      (PFNGLVERTEXATTRIBPOINTERPROC)      getprocaddress("glVertexAttribPointer");

        if(glversion >= 210)
        {
            glUniformMatrix2x3fv_ =   (PFNGLUNIFORMMATRIX2X3FVPROC)       getprocaddress("glUniformMatrix2x3fv");
            glUniformMatrix3x2fv_ =   (PFNGLUNIFORMMATRIX3X2FVPROC)       getprocaddress("glUniformMatrix3x2fv");
            glUniformMatrix2x4fv_ =   (PFNGLUNIFORMMATRIX2X4FVPROC)       getprocaddress("glUniformMatrix2x4fv");
            glUniformMatrix4x2fv_ =   (PFNGLUNIFORMMATRIX4X2FVPROC)       getprocaddress("glUniformMatrix4x2fv");
            glUniformMatrix3x4fv_ =   (PFNGLUNIFORMMATRIX3X4FVPROC)       getprocaddress("glUniformMatrix3x4fv");
            glUniformMatrix4x3fv_ =   (PFNGLUNIFORMMATRIX4X3FVPROC)       getprocaddress("glUniformMatrix4x3fv");
        }
#endif

        const char *str = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
        uint majorversion, minorversion;
        if(str && sscanf(str, " %u.%u", &majorversion, &minorversion) == 2)
            glslversion = majorversion*100 + minorversion;
    }
    if(glslversion < 120) fatal("GLSL 1.20 or greater is required!");
    
    if(ati)
    {
        //conoutf("\frWARNING: ATI cards may show garbage in skybox. (use \"/ati_skybox_bug 1\" to fix)");

        minimizetcusage = 1;
        if(hasTF && (hasTRG || hasNVFB)) setvar("fpdepthfx", 1, false, true);
    }
    else if(nvidia)
    {
        reservevpparams = 10;
        rtsharefb = 0; // work-around for strange driver stalls involving when using many FBOs
        if(!hasext(gfxexts, "GL_EXT_gpu_shader4")) setvar("filltjoints", 0, false, true); // DX9 or less NV cards seem to not cause many sparklies

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

    if(hasext(gfxexts, "GL_EXT_texture_rectangle") || hasext(gfxexts, "GL_ARB_texture_rectangle"))
    {
        usetexrect = 1;
        hasTR = true;
        if(dbgexts) conoutf("\frUsing GL_ARB_texture_rectangle extension.");
    }
    else conoutf("\frWARNING: No texture rectangle support. (no full screen shaders)");

    if(hasext(gfxexts, "GL_EXT_packed_depth_stencil") || hasext(gfxexts, "GL_NV_packed_depth_stencil"))
    {
        hasDS = true;
        if(dbgexts) conoutf("\frUsing GL_EXT_packed_depth_stencil extension.");
    }

    if(hasext(gfxexts, "GL_EXT_blend_minmax"))
    {
        glBlendEquation_ = (PFNGLBLENDEQUATIONEXTPROC) getprocaddress("glBlendEquationEXT");
        hasBE = true;
        if(ati) ati_minmax_bug = 1;
        if(dbgexts) conoutf("\frUsing GL_EXT_blend_minmax extension.");
    }

    if(hasext(gfxexts, "GL_EXT_blend_color"))
    {
        glBlendColor_ = (PFNGLBLENDCOLOREXTPROC) getprocaddress("glBlendColorEXT");
        hasBC = true;
        if(dbgexts) conoutf("\frUsing GL_EXT_blend_color extension.");
    }

    if(hasext(gfxexts, "GL_ARB_texture_cube_map"))
    {
        GLint val;
        glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB, &val);
        hwcubetexsize = val;
        hasCM = true;
        // On Catalyst 10.2, issuing an occlusion query on the first draw using a given cubemap texture causes a nasty crash
        if(ati) ati_cubemap_bug = 1;
        if(dbgexts) conoutf("\frUsing GL_ARB_texture_cube_map extension.");
    }
    else conoutf("\frWARNING: No cube map texture support. (no reflective glass)");

    extern int usenp2;
    if(hasext(gfxexts, "GL_ARB_texture_non_power_of_two"))
    {
        hasNP2 = true;
        if(dbgexts) conoutf("\frUsing GL_ARB_texture_non_power_of_two extension.");
    }
    else if(usenp2) conoutf("\frWARNING: Non-power-of-two textures not supported");

    if(hasext(gfxexts, "GL_ARB_texture_compression"))
    {
        glCompressedTexImage3D_ =    (PFNGLCOMPRESSEDTEXIMAGE3DARBPROC)   getprocaddress("glCompressedTexImage3DARB");
        glCompressedTexImage2D_ =    (PFNGLCOMPRESSEDTEXIMAGE2DARBPROC)   getprocaddress("glCompressedTexImage2DARB");
        glCompressedTexImage1D_ =    (PFNGLCOMPRESSEDTEXIMAGE1DARBPROC)   getprocaddress("glCompressedTexImage1DARB");
        glCompressedTexSubImage3D_ = (PFNGLCOMPRESSEDTEXSUBIMAGE3DARBPROC)getprocaddress("glCompressedTexSubImage3DARB");
        glCompressedTexSubImage2D_ = (PFNGLCOMPRESSEDTEXSUBIMAGE2DARBPROC)getprocaddress("glCompressedTexSubImage2DARB");
        glCompressedTexSubImage1D_ = (PFNGLCOMPRESSEDTEXSUBIMAGE1DARBPROC)getprocaddress("glCompressedTexSubImage1DARB");
        glGetCompressedTexImage_ =   (PFNGLGETCOMPRESSEDTEXIMAGEARBPROC)  getprocaddress("glGetCompressedTexImageARB");

        hasTC = true;
        if(dbgexts) conoutf("\frUsing GL_ARB_texture_compression.");

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
    }

    if(hasext(gfxexts, "GL_EXT_texture_filter_anisotropic"))
    {
       GLint val;
       glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &val);
       hwmaxanisotropy = val;
       hasAF = true;
       if(dbgexts) conoutf("\frUsing GL_EXT_texture_filter_anisotropic extension.");
    }

    if(hasext(gfxexts, "GL_ARB_depth_texture"))
    {
        hasDT = true;
        if(dbgexts) conoutf("\frUsing GL_ARB_depth_texture extension.");
    }

    if(hasext(gfxexts, "GL_EXT_gpu_shader4"))
    {
        // on DX10 or above class cards (i.e. GF8 or RadeonHD) enable expensive features
        setvar("grass", 1, false, true);
        setvar("motionblur", 1, false, true);
        setvar("waterfallrefract", 1, false, true);
        setvar("glare", 1, false, true);
        setvar("maxdynlights", MAXDYNLIGHTS, false, true);
        if(hasTR)
        {
            setvar("depthfxsize", 10, false, true);
            setvar("depthfxrect", 1, false, true);
            setvar("depthfxfilter", 0, false, true);
            setvar("blurdepthfx", 0, false, true);
        }
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


    glDisable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    //glHint(GL_FOG_HINT, GL_NICEST);
    GLfloat fogcolor[4] = { 0, 0, 0, 0 };
    glFogfv(GL_FOG_COLOR, fogcolor);


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
}

#define VARRAY_INTERNAL
#include "varray.h"

VAR(0, wireframe, 0, 0, 1);

ICOMMAND(0, getcampos, "", (),
{
    defformatstring(pos)("%s %s %s", floatstr(camera1->o.x), floatstr(camera1->o.y), floatstr(camera1->o.z));
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

void transplayer()
{
    // move from RH to Z-up LH quake style worldspace
    glLoadMatrixf(viewmatrix.v);

    glRotatef(camera1->roll, 0, 1, 0);
    glRotatef(camera1->pitch, -1, 0, 0);
    glRotatef(camera1->yaw, 0, 0, -1);

    glTranslatef(-camera1->o.x, -camera1->o.y, -camera1->o.z);
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
    vec dir1 = invmvpmatrix.perspectivetransform(vec(x*2-1, 1-2*y, z*2-1)),
        dir2 = invmvpmatrix.perspectivetransform(vec(x*2-1, 1-2*y, -1));
    (dir = dir1).sub(dir2).normalize();
}

bool vectocursor(const vec &v, float &x, float &y, float &z, float clampxy)
{
    vec4 clippos;
    mvpmatrix.transform(v, clippos);
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

extern const glmatrixf viewmatrix(vec4(-1, 0, 0, 0), vec4(0, 0, 1, 0), vec4(0, -1, 0, 0));
glmatrixf mvmatrix, projmatrix, mvpmatrix, invmvmatrix, invmvpmatrix;

void readmatrices()
{
    glGetFloatv(GL_MODELVIEW_MATRIX, mvmatrix.v);
    glGetFloatv(GL_PROJECTION_MATRIX, projmatrix.v);

    mvpmatrix.mul(projmatrix, mvmatrix);
    invmvmatrix.invert(mvmatrix);
    invmvpmatrix.invert(mvpmatrix);

    mvmatrix.transposedtransformnormal(vec(viewmatrix.getcolumn(1)), camdir);
    mvmatrix.transposedtransformnormal(vec(viewmatrix.getcolumn(0)).neg(), camright);
    mvmatrix.transposedtransformnormal(vec(viewmatrix.getcolumn(2)), camup);
}

FVAR(0, nearplane, 0.01f, 0.54f, 2.0f);

void project(float fovy, float aspect, int farplane, bool flipx, bool flipy, bool swapxy, float zscale)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    if(swapxy) glRotatef(90, 0, 0, 1);
    if(flipx || flipy!=swapxy || zscale!=1) glScalef(flipx ? -1 : 1, flipy!=swapxy ? -1 : 1, zscale);
    GLdouble ydist = nearplane * tan(fovy/2*RAD), xdist = ydist * aspect;
    glFrustum(-xdist, xdist, -ydist, ydist, nearplane, farplane);
    glMatrixMode(GL_MODELVIEW);
}

VAR(0, reflectclip, 0, 6, 64);

glmatrixf clipmatrix;

static const glmatrixf dummymatrix;
static int projectioncount = 0;
void pushprojection(const glmatrixf &m = dummymatrix)
{
    glMatrixMode(GL_PROJECTION);
    if(projectioncount <= 0) glPushMatrix();
    if(&m != &dummymatrix) glLoadMatrixf(m.v);
    if(fogging)
    {
        glMultMatrixf(mvmatrix.v);
        glMultMatrixf(invfogmatrix.v);
    }
    glMatrixMode(GL_MODELVIEW);
    projectioncount++;
}

void popprojection()
{
    --projectioncount;
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    if(projectioncount > 0)
    {
        glPushMatrix();
        if(fogging)
        {
            glMultMatrixf(mvmatrix.v);
            glMultMatrixf(invfogmatrix.v);
        }
    }
    glMatrixMode(GL_MODELVIEW);
}

FVAR(0, polygonoffsetfactor, -1e4f, -3.0f, 1e4f);
FVAR(0, polygonoffsetunits, -1e4f, -3.0f, 1e4f);
FVAR(0, depthoffset, -1e4f, 0.01f, 1e4f);

void enablepolygonoffset(GLenum type)
{
    if(!depthoffset)
    {
        glPolygonOffset(polygonoffsetfactor, polygonoffsetunits);
        glEnable(type);
        return;
    }

    bool clipped = reflectz < 1e15f && reflectclip;

    glmatrixf offsetmatrix = clipped ? clipmatrix : projmatrix;
    offsetmatrix[14] += depthoffset * projmatrix[10];

    glMatrixMode(GL_PROJECTION);
    if(!clipped) glPushMatrix();
    glLoadMatrixf(offsetmatrix.v);
    if(fogging)
    {
        glMultMatrixf(mvmatrix.v);
        glMultMatrixf(invfogmatrix.v);
    }
    glMatrixMode(GL_MODELVIEW);
}

void disablepolygonoffset(GLenum type)
{
    if(!depthoffset)
    {
        glDisable(type);
        return;
    }

    bool clipped = reflectz < 1e15f && reflectclip;

    glMatrixMode(GL_PROJECTION);
    if(clipped)
    {
        glLoadMatrixf(clipmatrix.v);
        if(fogging)
        {
            glMultMatrixf(mvmatrix.v);
            glMultMatrixf(invfogmatrix.v);
        }
    }
    else glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void calcspherescissor(const vec &center, float size, float &sx1, float &sy1, float &sx2, float &sy2)
{
    vec worldpos(center);
    if(reflecting) worldpos.z = 2*reflectz - worldpos.z;
    vec e(mvmatrix.transformx(worldpos),
          mvmatrix.transformy(worldpos),
          mvmatrix.transformz(worldpos));
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

glmatrixf envmatrix;

void setenvmatrix()
{
    envmatrix = fogging ? fogmatrix : mvmatrix;
    if(reflecting) envmatrix.reflectz(reflectz);
    envmatrix.transpose();
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

static void blendfog(int fogmat, float blend, float logblend, float &start, float &end, float *fogc)
{
    switch(fogmat&MATF_VOLUME)
    {
        case MAT_WATER:
        {
            const bvec &wcol = getwatercol(fogmat);
            int wfog = getwaterfog(fogmat);
            loopk(3) fogc[k] += blend*wcol[k]/255.0f;
            end += logblend*min(fog, max(wfog*4, 32));
            break;
        }

        case MAT_LAVA:
        {
            const bvec &lcol = getlavacol(fogmat);
            int lfog = getlavafog(fogmat);
            loopk(3) fogc[k] += blend*lcol[k]/255.0f;
            end += logblend*min(fog, max(lfog*4, 32));
            break;
        }

        default:
            loopk(3) fogc[k] += blend*fogcolor[k]/255.0f;
            start += logblend*(fog+64)/8;
            end += logblend*fog;
            break;
    }
}

static void setfog(int fogmat, float below = 1, int abovemat = MAT_AIR)
{
    float fogc[4] = { 0, 0, 0, 1 };
    float start = 0, end = 0;
    float logscale = 256, logblend = log(1 + (logscale - 1)*below) / log(logscale);

    blendfog(fogmat, below, logblend, start, end, fogc);
    if(below < 1) blendfog(abovemat, 1-below, 1-logblend, start, end, fogc);

    glFogf(GL_FOG_START, start);
    glFogf(GL_FOG_END, end);
    glFogfv(GL_FOG_COLOR, fogc);
    glClearColor(fogc[0], fogc[1], fogc[2], 1.0f);
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
    if(!motionblur || viewtype || !hasTR || max(screen->w, screen->h) > hwtexsize) return;

    if(!motiontex || motionw != screen->w || motionh != screen->h)
    {
        if(!motiontex) glGenTextures(1, &motiontex);
        motionw = screen->w;
        motionh = screen->h;
        lastmotion = 0;
        createtexture(motiontex, motionw, motionh, NULL, 3, 0, GL_RGB, GL_TEXTURE_RECTANGLE_ARB);
    }

    float amount = min(hud::motionblur(motionblurscale), 1.0f);
    if(amount <= 0)
    {
        lastmotion = 0;
        return;
    }

    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, motiontex);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_TEXTURE_RECTANGLE_ARB);

    rectshader->set();

    glColor4f(1, 1, 1, lastmotion ? pow(amount, max(float(totalmillis-lastmotion)/motionblurmillis, 1.0f)) : 0);
    glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(      0,       0); glVertex2f(-1, -1);
    glTexCoord2f(motionw,       0); glVertex2f( 1, -1);
    glTexCoord2f(      0, motionh); glVertex2f(-1,  1);
    glTexCoord2f(motionw, motionh); glVertex2f( 1,  1);
    glEnd();

    glDisable(GL_TEXTURE_RECTANGLE_ARB);
    glEnable(GL_TEXTURE_2D);

    glDisable(GL_BLEND);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    if(totalmillis-lastmotion >= motionblurmillis)
    {
        lastmotion = totalmillis-totalmillis%motionblurmillis;

        glCopyTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, 0, 0, screen->w, screen->h);
    }
}

bool dopostfx = false;

void invalidatepostfx()
{
    dopostfx = false;
}

static void blendfogoverlay(int fogmat, float blend, float *overlay)
{
    float maxc;
    switch(fogmat&MATF_VOLUME)
    {
        case MAT_WATER:
        {
            const bvec &wcol = getwatercol(fogmat);
            maxc = max(wcol[0], max(wcol[1], wcol[2]));
            loopk(3) overlay[k] += blend*max(0.4f, wcol[k]/min(32.0f + maxc*7.0f/8.0f, 255.0f));
            break;
        }

        case MAT_LAVA:
        {
            const bvec &lcol = getlavacol(fogmat);
            maxc = max(lcol[0], max(lcol[1], lcol[2]));
            loopk(3) overlay[k] += blend*max(0.4f, lcol[k]/min(32.0f + maxc*7.0f/8.0f, 255.0f));
            break;
        }

        default:
            loopk(3) overlay[k] += blend;
            break;
    }
}

void drawfogoverlay(int fogmat, float fogblend, int abovemat)
{
    notextureshader->set();
    glDisable(GL_TEXTURE_2D);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ZERO, GL_SRC_COLOR);
    float overlay[3] = { 0, 0, 0 };
    blendfogoverlay(fogmat, fogblend, overlay);
    blendfogoverlay(abovemat, 1-fogblend, overlay);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glColor3fv(overlay);
    glBegin(GL_TRIANGLE_STRIP);
    glVertex2f(-1, -1);
    glVertex2f(1, -1);
    glVertex2f(-1, 1);
    glVertex2f(1, 1);
    glEnd();
    glDisable(GL_BLEND);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glEnable(GL_TEXTURE_2D);
    defaultshader->set();
}

bool renderedgame = false, renderedavatar = false;

void rendergame()
{
    game::render();
    if(!shadowmapping) renderedgame = true;
}

void renderavatar(bool early, bool project)
{
    game::renderavatar(early, project);
}

VAR(IDF_PERSIST, skyboxglare, 0, 1, 1);

void drawglare()
{
    glaring = true;
    refracting = -1;

    float oldfogstart, oldfogend, oldfogcolor[4], zerofog[4] = { 0, 0, 0, 1 };
    glGetFloatv(GL_FOG_START, &oldfogstart);
    glGetFloatv(GL_FOG_END, &oldfogend);
    glGetFloatv(GL_FOG_COLOR, oldfogcolor);

    glFogf(GL_FOG_START, (fog+64)/8);
    glFogf(GL_FOG_END, fog);
    glFogfv(GL_FOG_COLOR, zerofog);

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

    glFogf(GL_FOG_START, oldfogstart);
    glFogf(GL_FOG_END, oldfogend);
    glFogfv(GL_FOG_COLOR, oldfogcolor);

    refracting = 0;
    glaring = false;
}

VAR(IDF_PERSIST, reflectmms, 0, 1, 1);
VAR(IDF_WORLD, refractsky, 0, 0, 1);

glmatrixf fogmatrix, invfogmatrix;

void drawreflection(float z, bool refract, int fogdepth, const bvec &col)
{
    reflectz = z < 0 ? 1e16f : z;
    reflecting = !refract;
    refracting = refract ? (z < 0 || camera1->o.z >= z ? -1 : 1) : 0;
    fading = waterrefract && waterfade && hasFBO && z>=0;
    fogging = refracting<0 && z>=0;
    refractfog = fogdepth;
    refractcolor = fogging ? col : fogcolor;

    float oldfogstart, oldfogend, oldfogcolor[4];
    glGetFloatv(GL_FOG_START, &oldfogstart);
    glGetFloatv(GL_FOG_END, &oldfogend);
    glGetFloatv(GL_FOG_COLOR, oldfogcolor);

    if(fogging)
    {
        glFogf(GL_FOG_START, camera1->o.z - z);
        glFogf(GL_FOG_END, camera1->o.z - (z-max(refractfog, 1)));
        GLfloat m[16] =
        {
             1,   0,  0, 0,
             0,   1,  0, 0,
             0,   0,  1, 0,
            -camera1->o.x, -camera1->o.y, -camera1->o.z, 1
        };
        memcpy(fogmatrix.v, m, sizeof(m));
        invfogmatrix.invert(fogmatrix);
        pushprojection();
        glPushMatrix();
        glLoadMatrixf(fogmatrix.v);
        float fogc[4] = { col.x/255.0f, col.y/255.0f, col.z/255.0f, 1.0f };
        glFogfv(GL_FOG_COLOR, fogc);
    }
    else
    {
        glFogf(GL_FOG_START, (fog+64)/8);
        glFogf(GL_FOG_END, fog);
        float fogc[4] = { fogcolor.x/255.0f, fogcolor.y/255.0f, fogcolor.z/255.0f, 1.0f };
        glFogfv(GL_FOG_COLOR, fogc);
    }

    if(fading)
    {
        float scale = fogging ? -0.25f : 0.25f, offset = 2*fabs(scale) - scale*z;
        setenvparamf("waterfadeparams", SHPARAM_VERTEX, 8, scale, offset, -scale, offset + camera1->o.z*scale);
        setenvparamf("waterfadeparams", SHPARAM_PIXEL, 8, scale, offset, -scale, offset + camera1->o.z*scale);
    }

    if(reflecting)
    {
        glPushMatrix();
        glTranslatef(0, 0, 2*z);
        glScalef(1, 1, -1);

        glFrontFace(GL_CCW);
    }

    setenvmatrix();

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
        invmvmatrix.transposedtransform(plane(0, 0, refracting>0 ? 1 : -1, refracting>0 ? -zclip : zclip), clipplane);
        clipmatrix.clip(clipplane, projmatrix);
        pushprojection(clipmatrix);
    }

    renderreflectedgeom(refracting<0 && z>=0 && caustics, fogging);

    if(reflecting || refracting>0 || (refracting<0 && refractsky) || z<0)
    {
        if(fading) glColorMask(COLORMASK, GL_TRUE);
        if(reflectclip && z>=0) popprojection();
        if(fogging)
        {
            popprojection();
            glPopMatrix();
        }
        drawskybox(farplane, false);
        if(fogging)
        {
            pushprojection();
            glPushMatrix();
            glLoadMatrixf(fogmatrix.v);
        }
        if(reflectclip && z>=0) pushprojection(clipmatrix);
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

    if(reflectclip && z>=0) popprojection();

    if(reflecting)
    {
        glPopMatrix();

        glFrontFace(GL_CW);
    }

    if(fogging)
    {
        popprojection();
        glPopMatrix();
    }
    glFogf(GL_FOG_START, oldfogstart);
    glFogf(GL_FOG_END, oldfogend);
    glFogfv(GL_FOG_COLOR, oldfogcolor);

    reflectz = 1e16f;
    refracting = 0;
    reflecting = fading = fogging = false;

    setenvmatrix();
}

bool envmapping = false;

void drawcubemap(int size, int level, const vec &o, float yaw, float pitch, bool flipx, bool flipy, bool swapxy)
{
    float fovy = 90.f, aspect = 1.f;
    envmapping = true;

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

    defaultshader->set();

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
    if(level > 1 && fogmat != MAT_AIR)
    {
        float blend = abovemat==MAT_AIR ? fogblend : 1.0f;
        fovy += blend*sinf(lastmillis/1000.0)*2.0f;
        aspect += blend*sinf(lastmillis/1000.0+PI)*0.1f;
    }

    int farplane = hdr.worldsize*2;

    project(fovy, aspect, farplane, flipx, flipy, swapxy);
    transplayer();
    readmatrices();
    setenvmatrix();

    glEnable(GL_FOG);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);

    xtravertsva = xtraverts = glde = gbatches = 0;

    if(level > 1 && !hasFBO)
    {
        if(dopostfx)
        {
            drawglaretex();
            drawdepthfxtex();
            drawreflections();
        }
        else dopostfx = true;
    }

    visiblecubes();

    if(level > 1 && shadowmap && !hasFBO) rendershadowmap();

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
        if(hasFBO)
        {
            drawglaretex();
            drawdepthfxtex();
            drawreflections();
        }
        renderwater(); // water screws up for some reason
    }
    rendergrass();
    rendermaterials();
    renderparticles();

    glDisable(GL_FOG);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    if(level > 1) addglare();

    glDisable(GL_TEXTURE_2D);

    camera1 = oldcamera;
    envmapping = false;
}

bool modelpreviewing = false;

namespace modelpreview
{
    physent *oldcamera;
    float oldfogstart, oldfogend, oldfogcolor[4];

    physent camera;

    void start(bool background)
    {
        float fovy = 90.f, aspect = 1.f;
        envmapping = modelpreviewing = true;

        oldcamera = camera1;
        camera = *camera1;
        camera.reset();
        camera.type = ENT_CAMERA;
        camera.o = vec(0, 0, 0);
        camera.yaw = 0;
        camera.pitch = 0;
        camera.roll = 0;
        camera1 = &camera;

        glGetFloatv(GL_FOG_START, &oldfogstart);
        glGetFloatv(GL_FOG_END, &oldfogend);
        glGetFloatv(GL_FOG_COLOR, oldfogcolor);

        GLfloat fogc[4] = { 0, 0, 0, 1 };
        glFogf(GL_FOG_START, 0);
        glFogf(GL_FOG_END, 1000000);
        glFogfv(GL_FOG_COLOR, fogc);
        glClearColor(fogc[0], fogc[1], fogc[2], fogc[3]);

        glClear((background ? GL_COLOR_BUFFER_BIT : 0) | GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        project(fovy, aspect, 1024);
        transplayer();
        readmatrices();
        setenvmatrix();

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
    }

    void end()
    {
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);

        defaultshader->set();

        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();

        glFogf(GL_FOG_START, oldfogstart);
        glFogf(GL_FOG_END, oldfogend);
        glFogfv(GL_FOG_COLOR, oldfogcolor);
        glClearColor(oldfogcolor[0], oldfogcolor[1], oldfogcolor[2], oldfogcolor[3]);

        camera1 = oldcamera;
        envmapping = modelpreviewing = false;
    }
}

bool minimapping = false;

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

    envmapping = minimapping = true;

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

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-minimapradius.x, minimapradius.x, -minimapradius.y, minimapradius.y, 0, camera1->o.z + 1);
    glScalef(-1, 1, 1);
    glMatrixMode(GL_MODELVIEW);

    transplayer();

    defaultshader->set();

    GLfloat fogc[4] = { minimapcolor.x/255.0f, minimapcolor.y/255.0f, minimapcolor.z/255.0f, 1.0f };
    glFogf(GL_FOG_START, 0);
    glFogf(GL_FOG_END, 1000000);
    glFogfv(GL_FOG_COLOR, fogc);

    glClearColor(fogc[0], fogc[1], fogc[2], fogc[3]);
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    glViewport(0, 0, size, size);

    glDisable(GL_FOG);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);

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
            transplayer();
        }
        rendergeom();
        rendermapmodels();
        renderwater();
        rendermaterials();
        renderalphageom();
    }

    glFrontFace(GL_CW);

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_FOG);

    glViewport(0, 0, screen->w, screen->h);

    camera1 = oldcamera;
    envmapping = minimapping = false;

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
    GLenum target;
    int w, h;

    framebuffercopy() : tex(0), target(GL_TEXTURE_2D), w(0), h(0) {}

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
        if(hasTR)
        {
            target = GL_TEXTURE_RECTANGLE_ARB;
            w = screen->w;
            h = screen->h;
        }
        else
        {
            target = GL_TEXTURE_2D;
            for(w = 1; w < screen->w; w *= 2);
            for(h = 1; h < screen->h; h *= 2);
        }
        createtexture(tex, w, h, NULL, 3, false, GL_RGB, target);
    }

    void copy()
    {
        if(target == GL_TEXTURE_RECTANGLE_ARB)
        {
            if(w != screen->w || h != screen->h) cleanup();
        }
        else if(w < screen->w || h < screen->h || w/2 >= screen->w || h/2 >= screen->h) cleanup();
        if(!tex) setup();

        glBindTexture(target, tex);
        glCopyTexSubImage2D(target, 0, 0, 0, 0, 0, screen->w, screen->h);
    }

    void draw(float sx, float sy, float sw, float sh)
    {
        float tx = 0, ty = 0, tw = float(screen->w)/w, th = float(screen->h)/h;
        if(target == GL_TEXTURE_RECTANGLE_ARB)
        {
            glDisable(GL_TEXTURE_2D);
            glEnable(GL_TEXTURE_RECTANGLE_ARB);
            rectshader->set();
            tx *= w;
            ty *= h;
            tw *= w;
            th *= h;
        }
        glBindTexture(target, tex);
        glBegin(GL_TRIANGLE_STRIP);
        glTexCoord2f(tx,    ty);    glVertex2f(sx,    sy);
        glTexCoord2f(tx+tw, ty);    glVertex2f(sx+sw, sy);
        glTexCoord2f(tx,    ty+th); glVertex2f(sx,    sy+sh);
        glTexCoord2f(tx+tw, ty+th); glVertex2f(sx+sw, sy+sh);
        glEnd();
        if(target == GL_TEXTURE_RECTANGLE_ARB)
        {
            glEnable(GL_TEXTURE_2D);
            glDisable(GL_TEXTURE_RECTANGLE_ARB);
            defaultshader->set();
        }
    }
};

#define DTR 0.0174532925
enum { VW_NORMAL = 0, VW_LEFTRIGHT, VW_CROSSEYED, VW_STEREO, VW_STEREO_BLEND = VW_STEREO, VW_STEREO_BLEND_REDCYAN, VW_STEREO_AVG, VW_MAX, VW_STEREO_REDCYAN = VW_MAX };
enum { VP_LEFT, VP_RIGHT, VP_MAX, VP_CAMERA = VP_MAX };

framebuffercopy views[VP_MAX];

VARF(IDF_PERSIST, viewtype, VW_NORMAL, VW_NORMAL, VW_MAX, loopi(VP_MAX) views[i].cleanup());
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

static int curview = VP_CAMERA;

void viewproject(float zscale)
{
    if(curview != VP_LEFT && curview != VP_RIGHT) project(fovy, aspect, farplane, false, false, false, zscale);
    else
    {
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        if(zscale != 1) glScalef(1, 1, zscale);
        float top = stereonear*tan(DTR*fovy/2), right = aspect*top, iod = stereodist/2, fs = iod*stereonear/stereoplane;
        glFrustum(curview == VP_LEFT ? -right+fs : -right-fs, curview == VP_LEFT ? right+fs : right-fs, -top, top, stereonear, farplane);
        glTranslatef(curview == VP_LEFT ? iod : -iod, 0.f, 0.f);
        glMatrixMode(GL_MODELVIEW);
    }
}

void drawnoviewtype(int targtype)
{
    curview = targtype;
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

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    int w = screen->w, h = screen->h;
    if(forceaspect) w = int(ceil(h*forceaspect));
    gettextres(w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, h, 0, -1, 1);

    glClearColor(0.f, 0.f, 0.f, 1);
    if(clearview(viewtype, targtype)) glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_TEXTURE_2D);
    defaultshader->set();

    hud::update(screen->w, screen->h);
    hud::drawhud(true);
    hud::drawlast();

    glDisable(GL_TEXTURE_2D);

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

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 1, 0, 1, -1, 1);
    glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    defaultshader->set();
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
            if(hasBC)
            {
                glBlendFunc(GL_ONE, GL_CONSTANT_COLOR_EXT);
                glBlendColor_(0.f, 0.5f, 1.f, 1.f);
            }
            else
            {
                glDisable(GL_TEXTURE_2D);
                glBlendFunc(GL_ZERO, GL_SRC_COLOR);
                glColor3f(0.f, 0.5f, 1.f);
                glBegin(GL_TRIANGLE_STRIP);
                glVertex2f(0, 0);
                glVertex2f(1, 0);
                glVertex2f(0, 1);
                glVertex2f(1, 1);
                glEnd();
                glEnable(GL_TEXTURE_2D);
                glBlendFunc(GL_ONE, GL_ONE);
            }
            glColor3f(1.f, 0.5f, 0.f);
            views[VP_LEFT].draw(0, 0, 1, 1);
            glDisable(GL_BLEND);
            break;
        }
    }
    glDisable(GL_TEXTURE_2D);
}

void drawviewtype(int targtype)
{
    curview = targtype;

    defaultshader->set();
    updatedynlights();

    setfog(fogmat, fogblend, abovemat);
    viewproject();
    transplayer();
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

    readmatrices();

    glEnable(GL_FOG);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);

    xtravertsva = xtraverts = glde = gbatches = 0;

    if(!hasFBO)
    {
        if(dopostfx)
        {
            drawglaretex();
            drawdepthfxtex();
            drawreflections();
        }
        else dopostfx = true;
    }

    visiblecubes();
    if(shadowmap && !hasFBO) rendershadowmap();

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

    if(hasFBO)
    {
        drawglaretex();
        drawdepthfxtex();
        drawreflections();
    }

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

    glDisable(GL_FOG);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    addmotionblur();
    addglare();
    if(isliquid(fogmat&MATF_VOLUME)) drawfogoverlay(fogmat, fogblend, abovemat);
    renderpostfx();

    glDisable(GL_TEXTURE_2D);
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

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    int w = screen->w, h = screen->h;
    if(forceaspect) w = int(ceil(h*forceaspect));
    gettextres(w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, h, 0, -1, 1);
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

        glEnable(GL_TEXTURE_2D);
        defaultshader->set();
        hud::drawhud();
        rendertexturepanel(w, h);
        hud::drawlast();
        glDisable(GL_TEXTURE_2D);
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
        project(fovy, aspect, farplane);
        transplayer();
        readmatrices();
        game::project(w, h);
        setenvmatrix();

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

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, 1, 0, 1, -1, 1);
        glDisable(GL_BLEND);
        glEnable(GL_TEXTURE_2D);
        defaultshader->set();
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
                if(hasBC)
                {
                    glBlendFunc(GL_ONE, GL_CONSTANT_COLOR_EXT);
                    glBlendColor_(0.f, 0.5f, 1.f, 1.f);
                }
                else
                {
                    glDisable(GL_TEXTURE_2D);
                    glBlendFunc(GL_ZERO, GL_SRC_COLOR);
                    glColor3f(0.f, 0.5f, 1.f);
                    glBegin(GL_TRIANGLE_STRIP);
                    glVertex2f(0, 0);
                    glVertex2f(1, 0);
                    glVertex2f(0, 1);
                    glVertex2f(1, 1);
                    glEnd();
                    glEnable(GL_TEXTURE_2D);
                    glBlendFunc(GL_ONE, GL_ONE);
                }
                glColor3f(1.f, 0.5f, 0.f);
                views[VP_LEFT].draw(0, 0, 1, 1);
                glDisable(GL_BLEND);
                break;
            }
        }
        glDisable(GL_TEXTURE_2D);
    }
}

void usetexturing(bool on)
{
    if(on)
    {
        defaultshader->set();
        glEnable(GL_TEXTURE_2D);
    }
    else
    {
        notextureshader->set();
        glDisable(GL_TEXTURE_2D);
    }
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
