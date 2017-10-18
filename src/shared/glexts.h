#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif

// OpenGL deprecated functionality
#ifndef GL_QUADS
#define GL_QUADS                      0x0007
#endif

#ifndef GL_ALPHA
#define GL_ALPHA                      0x1906
#endif
#ifndef GL_ALPHA8
#define GL_ALPHA8                     0x803C
#endif
#ifndef GL_ALPHA16
#define GL_ALPHA16                    0x803E
#endif
#ifndef GL_COMPRESSED_ALPHA
#define GL_COMPRESSED_ALPHA           0x84E9
#endif

#ifndef GL_LUMINANCE
#define GL_LUMINANCE                  0x1909
#endif
#ifndef GL_LUMINANCE8
#define GL_LUMINANCE8                 0x8040
#endif
#ifndef GL_LUMINANCE16
#define GL_LUMINANCE16                0x8042
#endif
#ifndef GL_COMPRESSED_LUMINANCE
#define GL_COMPRESSED_LUMINANCE       0x84EA
#endif

#ifndef GL_LUMINANCE_ALPHA
#define GL_LUMINANCE_ALPHA            0x190A
#endif
#ifndef GL_LUMINANCE8_ALPHA8
#define GL_LUMINANCE8_ALPHA8          0x8045
#endif
#ifndef GL_LUMINANCE16_ALPHA16
#define GL_LUMINANCE16_ALPHA16        0x8048
#endif
#ifndef GL_COMPRESSED_LUMINANCE_ALPHA
#define GL_COMPRESSED_LUMINANCE_ALPHA 0x84EB
#endif

#ifndef GL_EXT_texture_filter_anisotropic
#define GL_EXT_texture_filter_anisotropic 1
#define GL_TEXTURE_MAX_ANISOTROPY_EXT     0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

#ifndef GL_EXT_texture_compression_s3tc
#define GL_EXT_texture_compression_s3tc 1
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT   0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT  0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT  0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT  0x83F3
#endif 

#ifndef GL_EXT_timer_query
#define GL_EXT_timer_query 1
#define GL_TIME_ELAPSED_EXT               0x88BF
typedef llong GLint64EXT;
typedef ullong GLuint64EXT;
typedef void (APIENTRYP PFNGLGETQUERYOBJECTI64VEXTPROC) (GLuint id, GLenum pname, GLint64EXT *params);
typedef void (APIENTRYP PFNGLGETQUERYOBJECTUI64VEXTPROC) (GLuint id, GLenum pname, GLuint64EXT *params);
#endif
extern PFNGLGETQUERYOBJECTI64VEXTPROC glGetQueryObjecti64v_;
extern PFNGLGETQUERYOBJECTUI64VEXTPROC glGetQueryObjectui64v_;

#ifndef GL_ARB_framebuffer_object
#define GL_ARB_framebuffer_object 1
#define GL_INVALID_FRAMEBUFFER_OPERATION  0x0506
#define GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING 0x8210
#define GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE 0x8211
#define GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE 0x8212
#define GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE 0x8213
#define GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE 0x8214
#define GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE 0x8215
#define GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE 0x8216
#define GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE 0x8217
#define GL_FRAMEBUFFER_DEFAULT            0x8218
#define GL_FRAMEBUFFER_UNDEFINED          0x8219
#define GL_DEPTH_STENCIL_ATTACHMENT       0x821A
#define GL_MAX_RENDERBUFFER_SIZE          0x84E8
#define GL_DEPTH_STENCIL                  0x84F9
#define GL_UNSIGNED_INT_24_8              0x84FA
#define GL_DEPTH24_STENCIL8               0x88F0
#define GL_TEXTURE_STENCIL_SIZE           0x88F1
#define GL_TEXTURE_RED_TYPE               0x8C10
#define GL_TEXTURE_GREEN_TYPE             0x8C11
#define GL_TEXTURE_BLUE_TYPE              0x8C12
#define GL_TEXTURE_ALPHA_TYPE             0x8C13
#define GL_TEXTURE_DEPTH_TYPE             0x8C16
#define GL_UNSIGNED_NORMALIZED            0x8C17
#define GL_FRAMEBUFFER_BINDING            0x8CA6
#define GL_DRAW_FRAMEBUFFER_BINDING       GL_FRAMEBUFFER_BINDING
#define GL_RENDERBUFFER_BINDING           0x8CA7
#define GL_READ_FRAMEBUFFER               0x8CA8
#define GL_DRAW_FRAMEBUFFER               0x8CA9
#define GL_READ_FRAMEBUFFER_BINDING       0x8CAA
#define GL_RENDERBUFFER_SAMPLES           0x8CAB
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE 0x8CD0
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME 0x8CD1
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL 0x8CD2
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE 0x8CD3
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER 0x8CD4
#define GL_FRAMEBUFFER_COMPLETE           0x8CD5
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT 0x8CD6
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT 0x8CD7
#define GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER 0x8CDB
#define GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER 0x8CDC
#define GL_FRAMEBUFFER_UNSUPPORTED        0x8CDD
#define GL_MAX_COLOR_ATTACHMENTS          0x8CDF
#define GL_COLOR_ATTACHMENT0              0x8CE0
#define GL_COLOR_ATTACHMENT1              0x8CE1
#define GL_COLOR_ATTACHMENT2              0x8CE2
#define GL_COLOR_ATTACHMENT3              0x8CE3
#define GL_COLOR_ATTACHMENT4              0x8CE4
#define GL_COLOR_ATTACHMENT5              0x8CE5
#define GL_COLOR_ATTACHMENT6              0x8CE6
#define GL_COLOR_ATTACHMENT7              0x8CE7
#define GL_COLOR_ATTACHMENT8              0x8CE8
#define GL_COLOR_ATTACHMENT9              0x8CE9
#define GL_COLOR_ATTACHMENT10             0x8CEA
#define GL_COLOR_ATTACHMENT11             0x8CEB
#define GL_COLOR_ATTACHMENT12             0x8CEC
#define GL_COLOR_ATTACHMENT13             0x8CED
#define GL_COLOR_ATTACHMENT14             0x8CEE
#define GL_COLOR_ATTACHMENT15             0x8CEF
#define GL_DEPTH_ATTACHMENT               0x8D00
#define GL_STENCIL_ATTACHMENT             0x8D20
#define GL_FRAMEBUFFER                    0x8D40
#define GL_RENDERBUFFER                   0x8D41
#define GL_RENDERBUFFER_WIDTH             0x8D42
#define GL_RENDERBUFFER_HEIGHT            0x8D43
#define GL_RENDERBUFFER_INTERNAL_FORMAT   0x8D44
#define GL_STENCIL_INDEX1                 0x8D46
#define GL_STENCIL_INDEX4                 0x8D47
#define GL_STENCIL_INDEX8                 0x8D48
#define GL_STENCIL_INDEX16                0x8D49
#define GL_RENDERBUFFER_RED_SIZE          0x8D50
#define GL_RENDERBUFFER_GREEN_SIZE        0x8D51
#define GL_RENDERBUFFER_BLUE_SIZE         0x8D52
#define GL_RENDERBUFFER_ALPHA_SIZE        0x8D53
#define GL_RENDERBUFFER_DEPTH_SIZE        0x8D54
#define GL_RENDERBUFFER_STENCIL_SIZE      0x8D55
#define GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE 0x8D56
#define GL_MAX_SAMPLES                    0x8D57
typedef GLboolean (APIENTRYP PFNGLISRENDERBUFFERPROC) (GLuint renderbuffer);
typedef void (APIENTRYP PFNGLBINDRENDERBUFFERPROC) (GLenum target, GLuint renderbuffer);
typedef void (APIENTRYP PFNGLDELETERENDERBUFFERSPROC) (GLsizei n, const GLuint *renderbuffers);
typedef void (APIENTRYP PFNGLGENRENDERBUFFERSPROC) (GLsizei n, GLuint *renderbuffers);
typedef void (APIENTRYP PFNGLRENDERBUFFERSTORAGEPROC) (GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (APIENTRYP PFNGLGETRENDERBUFFERPARAMETERIVPROC) (GLenum target, GLenum pname, GLint *params);
typedef GLboolean (APIENTRYP PFNGLISFRAMEBUFFERPROC) (GLuint framebuffer);
typedef void (APIENTRYP PFNGLBINDFRAMEBUFFERPROC) (GLenum target, GLuint framebuffer);
typedef void (APIENTRYP PFNGLDELETEFRAMEBUFFERSPROC) (GLsizei n, const GLuint *framebuffers);
typedef void (APIENTRYP PFNGLGENFRAMEBUFFERSPROC) (GLsizei n, GLuint *framebuffers);
typedef GLenum (APIENTRYP PFNGLCHECKFRAMEBUFFERSTATUSPROC) (GLenum target);
typedef void (APIENTRYP PFNGLFRAMEBUFFERTEXTURE1DPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (APIENTRYP PFNGLFRAMEBUFFERTEXTURE2DPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (APIENTRYP PFNGLFRAMEBUFFERTEXTURE3DPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset);
typedef void (APIENTRYP PFNGLFRAMEBUFFERRENDERBUFFERPROC) (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
typedef void (APIENTRYP PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC) (GLenum target, GLenum attachment, GLenum pname, GLint *params);
typedef void (APIENTRYP PFNGLGENERATEMIPMAPPROC) (GLenum target);
typedef void (APIENTRYP PFNGLBLITFRAMEBUFFERPROC) (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
typedef void (APIENTRYP PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC) (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (APIENTRYP PFNGLFRAMEBUFFERTEXTURELAYERPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer);
#endif

// GL_EXT_framebuffer_object
extern PFNGLBINDRENDERBUFFERPROC           glBindRenderbuffer_;
extern PFNGLDELETERENDERBUFFERSPROC        glDeleteRenderbuffers_;
extern PFNGLGENFRAMEBUFFERSPROC            glGenRenderbuffers_;
extern PFNGLRENDERBUFFERSTORAGEPROC        glRenderbufferStorage_;
extern PFNGLGETRENDERBUFFERPARAMETERIVPROC glGetRenderbufferParameteriv_;
extern PFNGLCHECKFRAMEBUFFERSTATUSPROC     glCheckFramebufferStatus_;
extern PFNGLBINDFRAMEBUFFERPROC            glBindFramebuffer_;
extern PFNGLDELETEFRAMEBUFFERSPROC         glDeleteFramebuffers_;
extern PFNGLGENFRAMEBUFFERSPROC            glGenFramebuffers_;
extern PFNGLFRAMEBUFFERTEXTURE1DPROC       glFramebufferTexture1D_;
extern PFNGLFRAMEBUFFERTEXTURE2DPROC       glFramebufferTexture2D_;
extern PFNGLFRAMEBUFFERTEXTURE3DPROC       glFramebufferTexture3D_;
extern PFNGLFRAMEBUFFERRENDERBUFFERPROC    glFramebufferRenderbuffer_;
extern PFNGLGENERATEMIPMAPPROC             glGenerateMipmap_;

// GL_EXT_framebuffer_blit
extern PFNGLBLITFRAMEBUFFERPROC         glBlitFramebuffer_;

// GL_EXT_framebuffer_multisample
extern PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC glRenderbufferStorageMultisample_;

#ifndef GL_ARB_texture_multisample
#define GL_ARB_texture_multisample 1
#define GL_SAMPLE_POSITION                0x8E50
#define GL_SAMPLE_MASK                    0x8E51
#define GL_SAMPLE_MASK_VALUE              0x8E52
#define GL_MAX_SAMPLE_MASK_WORDS          0x8E59
#define GL_TEXTURE_2D_MULTISAMPLE         0x9100
#define GL_PROXY_TEXTURE_2D_MULTISAMPLE   0x9101
#define GL_TEXTURE_2D_MULTISAMPLE_ARRAY   0x9102
#define GL_PROXY_TEXTURE_2D_MULTISAMPLE_ARRAY 0x9103
#define GL_TEXTURE_BINDING_2D_MULTISAMPLE 0x9104
#define GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY 0x9105
#define GL_TEXTURE_SAMPLES                0x9106
#define GL_TEXTURE_FIXED_SAMPLE_LOCATIONS 0x9107
#define GL_SAMPLER_2D_MULTISAMPLE         0x9108
#define GL_INT_SAMPLER_2D_MULTISAMPLE     0x9109
#define GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE 0x910A
#define GL_SAMPLER_2D_MULTISAMPLE_ARRAY   0x910B
#define GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY 0x910C
#define GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY 0x910D
#define GL_MAX_COLOR_TEXTURE_SAMPLES      0x910E
#define GL_MAX_DEPTH_TEXTURE_SAMPLES      0x910F
#define GL_MAX_INTEGER_SAMPLES            0x9110
typedef void (APIENTRYP PFNGLTEXIMAGE2DMULTISAMPLEPROC) (GLenum target, GLsizei samples, GLint internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations);
typedef void (APIENTRYP PFNGLTEXIMAGE3DMULTISAMPLEPROC) (GLenum target, GLsizei samples, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations);
typedef void (APIENTRYP PFNGLGETMULTISAMPLEFVPROC) (GLenum pname, GLuint index, GLfloat *val);
typedef void (APIENTRYP PFNGLSAMPLEMASKIPROC) (GLuint index, GLbitfield mask);
#endif
extern PFNGLTEXIMAGE2DMULTISAMPLEPROC glTexImage2DMultisample_;
extern PFNGLTEXIMAGE3DMULTISAMPLEPROC glTexImage3DMultisample_;
extern PFNGLGETMULTISAMPLEFVPROC      glGetMultisamplefv_;
extern PFNGLSAMPLEMASKIPROC           glSampleMaski_;

#ifndef GL_EXT_framebuffer_multisample_blit_scaled
#define GL_EXT_framebuffer_multisample_blit_scaled 1
#define GL_SCALED_RESOLVE_FASTEST_EXT     0x90BA
#define GL_SCALED_RESOLVE_NICEST_EXT      0x90BB
#endif

#ifndef GL_ARB_texture_rg
#define GL_ARB_texture_rg 1
#define GL_RG                             0x8227
#define GL_RG_INTEGER                     0x8228
#define GL_R8                             0x8229
#define GL_R16                            0x822A
#define GL_RG8                            0x822B
#define GL_RG16                           0x822C
#define GL_R16F                           0x822D
#define GL_R32F                           0x822E
#define GL_RG16F                          0x822F
#define GL_RG32F                          0x8230
#define GL_R8I                            0x8231
#define GL_R8UI                           0x8232
#define GL_R16I                           0x8233
#define GL_R16UI                          0x8234
#define GL_R32I                           0x8235
#define GL_R32UI                          0x8236
#define GL_RG8I                           0x8237
#define GL_RG8UI                          0x8238
#define GL_RG16I                          0x8239
#define GL_RG16UI                         0x823A
#define GL_RG32I                          0x823B
#define GL_RG32UI                         0x823C
#endif

#ifndef GL_EXT_texture_compression_latc
#define GL_EXT_texture_compression_latc 1
#define GL_COMPRESSED_LUMINANCE_LATC1_EXT              0x8C70
#define GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT        0x8C72
#endif

#ifndef GL_ARB_texture_compression_rgtc
#define GL_ARB_texture_compression_rgtc 1
#define GL_COMPRESSED_RED_RGTC1           0x8DBB
#define GL_COMPRESSED_RG_RGTC2            0x8DBD
#endif

#ifndef GL_EXT_depth_bounds_test
#define GL_EXT_depth_bounds_test 1
#define GL_DEPTH_BOUNDS_TEST_EXT          0x8890
#define GL_DEPTH_BOUNDS_EXT               0x8891
typedef void (APIENTRYP PFNGLDEPTHBOUNDSEXTPROC) (GLclampd zmin, GLclampd zmax);
#endif
extern PFNGLDEPTHBOUNDSEXTPROC glDepthBounds_;

#ifndef GL_ARB_map_buffer_range
#define GL_ARB_map_buffer_range 1
#define GL_MAP_READ_BIT                   0x0001
#define GL_MAP_WRITE_BIT                  0x0002
#define GL_MAP_INVALIDATE_RANGE_BIT       0x0004
#define GL_MAP_INVALIDATE_BUFFER_BIT      0x0008
#define GL_MAP_FLUSH_EXPLICIT_BIT         0x0010
#define GL_MAP_UNSYNCHRONIZED_BIT         0x0020
typedef GLvoid* (APIENTRYP PFNGLMAPBUFFERRANGEPROC) (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
typedef void (APIENTRYP PFNGLFLUSHMAPPEDBUFFERRANGEPROC) (GLenum target, GLintptr offset, GLsizeiptr length);
#endif
extern PFNGLMAPBUFFERRANGEPROC         glMapBufferRange_;
extern PFNGLFLUSHMAPPEDBUFFERRANGEPROC glFlushMappedBufferRange_;

// OpenGL 1.3
#ifndef WIN32
#define glActiveTexture_ glActiveTexture

#define glBlendEquation_ glBlendEquation
#define glBlendColor_ glBlendColor

#define glTexImage3D_ glTexImage3D
#define glTexSubImage3D_ glTexSubImage3D
#define glCopyTexSubImage3D_ glCopyTexSubImage3D

#define glCompressedTexImage3D_ glCompressedTexImage3D
#define glCompressedTexImage2D_ glCompressedTexImage2D
#define glCompressedTexImage1D_ glCompressedTexImage1D
#define glCompressedTexSubImage3D_ glCompressedTexSubImage3D
#define glCompressedTexSubImage2D_ glCompressedTexSubImage2D
#define glCompressedTexSubImage1D_ glCompressedTexSubImage1D
#define glGetCompressedTexImage_ glGetCompressedTexImage

#define glDrawRangeElements_ glDrawRangeElements
#else
extern PFNGLACTIVETEXTUREPROC    glActiveTexture_;

extern PFNGLBLENDEQUATIONPROC glBlendEquation_;
extern PFNGLBLENDCOLORPROC    glBlendColor_;

extern PFNGLTEXIMAGE3DPROC        glTexImage3D_;
extern PFNGLTEXSUBIMAGE3DPROC     glTexSubImage3D_;
extern PFNGLCOPYTEXSUBIMAGE3DPROC glCopyTexSubImage3D_;

extern PFNGLCOMPRESSEDTEXIMAGE3DPROC    glCompressedTexImage3D_;
extern PFNGLCOMPRESSEDTEXIMAGE2DPROC    glCompressedTexImage2D_;
extern PFNGLCOMPRESSEDTEXIMAGE1DPROC    glCompressedTexImage1D_;
extern PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC glCompressedTexSubImage3D_;
extern PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC glCompressedTexSubImage2D_;
extern PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC glCompressedTexSubImage1D_;
extern PFNGLGETCOMPRESSEDTEXIMAGEPROC   glGetCompressedTexImage_;

extern PFNGLDRAWRANGEELEMENTSPROC glDrawRangeElements_;
#endif

// OpenGL 2.0
#ifdef __APPLE__
#define glMultiDrawArrays_ glMultiDrawArrays
#define glMultiDrawElements_ glMultiDrawElements

#define glBlendFuncSeparate_ glBlendFuncSeparate
#define glBlendEquationSeparate_ glBlendEquationSeparate
#define glStencilOpSeparate_ glStencilOpSeparate
#define glStencilFuncSeparate_ glStencilFuncSeparate
#define glStencilMaskSeparate_ glStencilMaskSeparate

#define glGenBuffers_ glGenBuffers
#define glBindBuffer_ glBindBuffer
#define glMapBuffer_ glMapBuffer
#define glUnmapBuffer_ glUnmapBuffer
#define glBufferData_ glBufferData
#define glBufferSubData_ glBufferSubData
#define glDeleteBuffers_ glDeleteBuffers
#define glGetBufferSubData_ glGetBufferSubData

#define glGenQueries_ glGenQueries
#define glDeleteQueries_ glDeleteQueries
#define glBeginQuery_ glBeginQuery
#define glEndQuery_ glEndQuery
#define glGetQueryiv_ glGetQueryiv
#define glGetQueryObjectiv_ glGetQueryObjectiv
#define glGetQueryObjectuiv_ glGetQueryObjectuiv

#define glCreateProgram_ glCreateProgram
#define glDeleteProgram_ glDeleteProgram
#define glUseProgram_ glUseProgram
#define glCreateShader_ glCreateShader
#define glDeleteShader_ glDeleteShader
#define glShaderSource_ glShaderSource
#define glCompileShader_ glCompileShader
#define glGetShaderiv_ glGetShaderiv
#define glGetProgramiv_ glGetProgramiv
#define glAttachShader_ glAttachShader
#define glGetProgramInfoLog_ glGetProgramInfoLog
#define glGetShaderInfoLog_ glGetShaderInfoLog
#define glLinkProgram_ glLinkProgram
#define glGetUniformLocation_ glGetUniformLocation
#define glUniform1f_ glUniform1f
#define glUniform2f_ glUniform2f
#define glUniform3f_ glUniform3f
#define glUniform4f_ glUniform4f
#define glUniform1fv_ glUniform1fv
#define glUniform2fv_ glUniform2fv
#define glUniform3fv_ glUniform3fv
#define glUniform4fv_ glUniform4fv
#define glUniform1i_ glUniform1i
#define glUniform2i_ glUniform2i
#define glUniform3i_ glUniform3i
#define glUniform4i_ glUniform4i
#define glUniform1iv_ glUniform1iv
#define glUniform2iv_ glUniform2iv
#define glUniform3iv_ glUniform3iv
#define glUniform4iv_ glUniform4iv
#define glUniformMatrix2fv_ glUniformMatrix2fv
#define glUniformMatrix3fv_ glUniformMatrix3fv
#define glUniformMatrix4fv_ glUniformMatrix4fv
#define glBindAttribLocation_ glBindAttribLocation
#define glGetActiveUniform_ glGetActiveUniform
#define glEnableVertexAttribArray_ glEnableVertexAttribArray
#define glDisableVertexAttribArray_ glDisableVertexAttribArray

#define glVertexAttrib1f_ glVertexAttrib1f
#define glVertexAttrib1fv_ glVertexAttrib1fv
#define glVertexAttrib1s_ glVertexAttrib1s
#define glVertexAttrib1sv_ glVertexAttrib1sv
#define glVertexAttrib2f_ glVertexAttrib2f
#define glVertexAttrib2fv_ glVertexAttrib2fv
#define glVertexAttrib2s_ glVertexAttrib2s
#define glVertexAttrib2sv_ glVertexAttrib2sv
#define glVertexAttrib3f_ glVertexAttrib3f
#define glVertexAttrib3fv_ glVertexAttrib3fv
#define glVertexAttrib3s_ glVertexAttrib3s
#define glVertexAttrib3sv_ glVertexAttrib3sv
#define glVertexAttrib4f_ glVertexAttrib4f
#define glVertexAttrib4fv_ glVertexAttrib4fv
#define glVertexAttrib4s_ glVertexAttrib4s
#define glVertexAttrib4sv_ glVertexAttrib4sv
#define glVertexAttrib4bv_ glVertexAttrib4bv
#define glVertexAttrib4iv_ glVertexAttrib4iv
#define glVertexAttrib4ubv_ glVertexAttrib4ubv
#define glVertexAttrib4uiv_ glVertexAttrib4uiv
#define glVertexAttrib4usv_ glVertexAttrib4usv
#define glVertexAttrib4Nbv_ glVertexAttrib4Nbv
#define glVertexAttrib4Niv_ glVertexAttrib4Niv
#define glVertexAttrib4Nub_ glVertexAttrib4Nub
#define glVertexAttrib4Nubv_ glVertexAttrib4Nubv
#define glVertexAttrib4Nuiv_ glVertexAttrib4Nuiv
#define glVertexAttrib4Nusv_ glVertexAttrib4Nusv
#define glVertexAttribPointer_ glVertexAttribPointer

#define glDrawBuffers_ glDrawBuffers
#else
extern PFNGLMULTIDRAWARRAYSPROC   glMultiDrawArrays_;
extern PFNGLMULTIDRAWELEMENTSPROC glMultiDrawElements_;

extern PFNGLBLENDFUNCSEPARATEPROC glBlendFuncSeparate_;
extern PFNGLBLENDEQUATIONSEPARATEPROC glBlendEquationSeparate_;
extern PFNGLSTENCILOPSEPARATEPROC glStencilOpSeparate_;
extern PFNGLSTENCILFUNCSEPARATEPROC glStencilFuncSeparate_;
extern PFNGLSTENCILMASKSEPARATEPROC glStencilMaskSeparate_;

extern PFNGLGENBUFFERSPROC       glGenBuffers_;
extern PFNGLBINDBUFFERPROC       glBindBuffer_;
extern PFNGLMAPBUFFERPROC        glMapBuffer_;
extern PFNGLUNMAPBUFFERPROC      glUnmapBuffer_;
extern PFNGLBUFFERDATAPROC       glBufferData_;
extern PFNGLBUFFERSUBDATAPROC    glBufferSubData_;
extern PFNGLDELETEBUFFERSPROC    glDeleteBuffers_;
extern PFNGLGETBUFFERSUBDATAPROC glGetBufferSubData_;

extern PFNGLGENQUERIESPROC        glGenQueries_;
extern PFNGLDELETEQUERIESPROC     glDeleteQueries_;
extern PFNGLBEGINQUERYPROC        glBeginQuery_;
extern PFNGLENDQUERYPROC          glEndQuery_;
extern PFNGLGETQUERYIVPROC        glGetQueryiv_;
extern PFNGLGETQUERYOBJECTIVPROC  glGetQueryObjectiv_;
extern PFNGLGETQUERYOBJECTUIVPROC glGetQueryObjectuiv_;

extern PFNGLCREATEPROGRAMPROC            glCreateProgram_;
extern PFNGLDELETEPROGRAMPROC            glDeleteProgram_;
extern PFNGLUSEPROGRAMPROC               glUseProgram_;
extern PFNGLCREATESHADERPROC             glCreateShader_;
extern PFNGLDELETESHADERPROC             glDeleteShader_;
extern PFNGLSHADERSOURCEPROC             glShaderSource_;
extern PFNGLCOMPILESHADERPROC            glCompileShader_;
extern PFNGLGETSHADERIVPROC              glGetShaderiv_;
extern PFNGLGETPROGRAMIVPROC             glGetProgramiv_;
extern PFNGLATTACHSHADERPROC             glAttachShader_;
extern PFNGLGETPROGRAMINFOLOGPROC        glGetProgramInfoLog_;
extern PFNGLGETSHADERINFOLOGPROC         glGetShaderInfoLog_;
extern PFNGLLINKPROGRAMPROC              glLinkProgram_;
extern PFNGLGETUNIFORMLOCATIONPROC       glGetUniformLocation_;
extern PFNGLUNIFORM1FPROC                glUniform1f_;
extern PFNGLUNIFORM2FPROC                glUniform2f_;
extern PFNGLUNIFORM3FPROC                glUniform3f_;
extern PFNGLUNIFORM4FPROC                glUniform4f_;
extern PFNGLUNIFORM1FVPROC               glUniform1fv_;
extern PFNGLUNIFORM2FVPROC               glUniform2fv_;
extern PFNGLUNIFORM3FVPROC               glUniform3fv_;
extern PFNGLUNIFORM4FVPROC               glUniform4fv_;
extern PFNGLUNIFORM1IPROC                glUniform1i_;
extern PFNGLUNIFORM2IPROC                glUniform2i_;
extern PFNGLUNIFORM3IPROC                glUniform3i_;
extern PFNGLUNIFORM4IPROC                glUniform4i_;
extern PFNGLUNIFORM1IVPROC               glUniform1iv_;
extern PFNGLUNIFORM2IVPROC               glUniform2iv_;
extern PFNGLUNIFORM3IVPROC               glUniform3iv_;
extern PFNGLUNIFORM4IVPROC               glUniform4iv_;
extern PFNGLUNIFORMMATRIX2FVPROC         glUniformMatrix2fv_;
extern PFNGLUNIFORMMATRIX3FVPROC         glUniformMatrix3fv_;
extern PFNGLUNIFORMMATRIX4FVPROC         glUniformMatrix4fv_;
extern PFNGLBINDATTRIBLOCATIONPROC       glBindAttribLocation_;
extern PFNGLGETACTIVEUNIFORMPROC         glGetActiveUniform_;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC  glEnableVertexAttribArray_;
extern PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray_;

extern PFNGLVERTEXATTRIB1FPROC           glVertexAttrib1f_;
extern PFNGLVERTEXATTRIB1FVPROC          glVertexAttrib1fv_;
extern PFNGLVERTEXATTRIB1SPROC           glVertexAttrib1s_;
extern PFNGLVERTEXATTRIB1SVPROC          glVertexAttrib1sv_;
extern PFNGLVERTEXATTRIB2FPROC           glVertexAttrib2f_;
extern PFNGLVERTEXATTRIB2FVPROC          glVertexAttrib2fv_;
extern PFNGLVERTEXATTRIB2SPROC           glVertexAttrib2s_;
extern PFNGLVERTEXATTRIB2SVPROC          glVertexAttrib2sv_;
extern PFNGLVERTEXATTRIB3FPROC           glVertexAttrib3f_;
extern PFNGLVERTEXATTRIB3FVPROC          glVertexAttrib3fv_;
extern PFNGLVERTEXATTRIB3SPROC           glVertexAttrib3s_;
extern PFNGLVERTEXATTRIB3SVPROC          glVertexAttrib3sv_;
extern PFNGLVERTEXATTRIB4FPROC           glVertexAttrib4f_;
extern PFNGLVERTEXATTRIB4FVPROC          glVertexAttrib4fv_;
extern PFNGLVERTEXATTRIB4SPROC           glVertexAttrib4s_;
extern PFNGLVERTEXATTRIB4SVPROC          glVertexAttrib4sv_;
extern PFNGLVERTEXATTRIB4BVPROC          glVertexAttrib4bv_;
extern PFNGLVERTEXATTRIB4IVPROC          glVertexAttrib4iv_;
extern PFNGLVERTEXATTRIB4UBVPROC         glVertexAttrib4ubv_;
extern PFNGLVERTEXATTRIB4UIVPROC         glVertexAttrib4uiv_;
extern PFNGLVERTEXATTRIB4USVPROC         glVertexAttrib4usv_;
extern PFNGLVERTEXATTRIB4NBVPROC         glVertexAttrib4Nbv_;
extern PFNGLVERTEXATTRIB4NIVPROC         glVertexAttrib4Niv_;
extern PFNGLVERTEXATTRIB4NUBPROC         glVertexAttrib4Nub_;
extern PFNGLVERTEXATTRIB4NUBVPROC        glVertexAttrib4Nubv_;
extern PFNGLVERTEXATTRIB4NUIVPROC        glVertexAttrib4Nuiv_;
extern PFNGLVERTEXATTRIB4NUSVPROC        glVertexAttrib4Nusv_;
extern PFNGLVERTEXATTRIBPOINTERPROC      glVertexAttribPointer_;

extern PFNGLDRAWBUFFERSPROC glDrawBuffers_;
#endif

#ifndef GL_VERSION_2_1
#define GL_VERSION_2_1 1
#define GL_PIXEL_PACK_BUFFER              0x88EB
#define GL_PIXEL_UNPACK_BUFFER            0x88EC
#endif

#ifndef GL_ARB_uniform_buffer_object
#define GL_ARB_uniform_buffer_object 1
#define GL_UNIFORM_BUFFER                 0x8A11
#define GL_UNIFORM_BUFFER_BINDING         0x8A28
#define GL_UNIFORM_BUFFER_START           0x8A29
#define GL_UNIFORM_BUFFER_SIZE            0x8A2A
#define GL_MAX_VERTEX_UNIFORM_BLOCKS      0x8A2B
#define GL_MAX_GEOMETRY_UNIFORM_BLOCKS    0x8A2C
#define GL_MAX_FRAGMENT_UNIFORM_BLOCKS    0x8A2D
#define GL_MAX_COMBINED_UNIFORM_BLOCKS    0x8A2E
#define GL_MAX_UNIFORM_BUFFER_BINDINGS    0x8A2F
#define GL_MAX_UNIFORM_BLOCK_SIZE         0x8A30
#define GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS 0x8A31
#define GL_MAX_COMBINED_GEOMETRY_UNIFORM_COMPONENTS 0x8A32
#define GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS 0x8A33
#define GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT 0x8A34
#define GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH 0x8A35
#define GL_ACTIVE_UNIFORM_BLOCKS          0x8A36
#define GL_UNIFORM_TYPE                   0x8A37
#define GL_UNIFORM_SIZE                   0x8A38
#define GL_UNIFORM_NAME_LENGTH            0x8A39
#define GL_UNIFORM_BLOCK_INDEX            0x8A3A
#define GL_UNIFORM_OFFSET                 0x8A3B
#define GL_UNIFORM_ARRAY_STRIDE           0x8A3C
#define GL_UNIFORM_MATRIX_STRIDE          0x8A3D
#define GL_UNIFORM_IS_ROW_MAJOR           0x8A3E
#define GL_UNIFORM_BLOCK_BINDING          0x8A3F
#define GL_UNIFORM_BLOCK_DATA_SIZE        0x8A40
#define GL_UNIFORM_BLOCK_NAME_LENGTH      0x8A41
#define GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS  0x8A42
#define GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES 0x8A43
#define GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER 0x8A44
#define GL_UNIFORM_BLOCK_REFERENCED_BY_GEOMETRY_SHADER 0x8A45
#define GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER 0x8A46
#define GL_INVALID_INDEX                  0xFFFFFFFFu
typedef void (APIENTRYP PFNGLGETUNIFORMINDICESPROC) (GLuint program, GLsizei uniformCount, const GLchar* *uniformNames, GLuint *uniformIndices);
typedef void (APIENTRYP PFNGLGETACTIVEUNIFORMSIVPROC) (GLuint program, GLsizei uniformCount, const GLuint *uniformIndices, GLenum pname, GLint *params);
typedef GLuint (APIENTRYP PFNGLGETUNIFORMBLOCKINDEXPROC) (GLuint program, const GLchar *uniformBlockName);
typedef void (APIENTRYP PFNGLGETACTIVEUNIFORMBLOCKIVPROC) (GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint *params);
typedef void (APIENTRYP PFNGLUNIFORMBLOCKBINDINGPROC) (GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding);
#endif
#ifndef GL_INVALID_INDEX
#define GL_INVALID_INDEX                  0xFFFFFFFFu
#endif
extern PFNGLGETUNIFORMINDICESPROC       glGetUniformIndices_;
extern PFNGLGETACTIVEUNIFORMSIVPROC     glGetActiveUniformsiv_;
extern PFNGLGETUNIFORMBLOCKINDEXPROC    glGetUniformBlockIndex_;
extern PFNGLGETACTIVEUNIFORMBLOCKIVPROC glGetActiveUniformBlockiv_;
extern PFNGLUNIFORMBLOCKBINDINGPROC     glUniformBlockBinding_;

#ifndef GL_VERSION_3_0
#define GL_VERSION_3_0 1
#define GL_NUM_EXTENSIONS                 0x821D
#define GL_COMPARE_REF_TO_TEXTURE         0x884E
#define GL_RGBA32F                        0x8814
#define GL_RGB32F                         0x8815
#define GL_RGBA16F                        0x881A
#define GL_RGB16F                         0x881B
#define GL_MIN_PROGRAM_TEXEL_OFFSET       0x8904
#define GL_MAX_PROGRAM_TEXEL_OFFSET       0x8905
#define GL_CLAMP_READ_COLOR               0x891C
#define GL_FIXED_ONLY                     0x891D
#define GL_MAX_VARYING_COMPONENTS         0x8B4B
#define GL_R11F_G11F_B10F                 0x8C3A
#define GL_RGBA32UI                       0x8D70
#define GL_RGB32UI                        0x8D71
#define GL_RGBA16UI                       0x8D76
#define GL_RGB16UI                        0x8D77
#define GL_RGBA8UI                        0x8D7C
#define GL_RGB8UI                         0x8D7D
#define GL_RGBA32I                        0x8D82
#define GL_RGB32I                         0x8D83
#define GL_RGBA16I                        0x8D88
#define GL_RGB16I                         0x8D89
#define GL_RGBA8I                         0x8D8E
#define GL_RGB8I                          0x8D8F
#define GL_RED_INTEGER                    0x8D94
#define GL_GREEN_INTEGER                  0x8D95
#define GL_BLUE_INTEGER                   0x8D96
#define GL_RGB_INTEGER                    0x8D98
#define GL_RGBA_INTEGER                   0x8D99
#define GL_BGR_INTEGER                    0x8D9A
#define GL_UNSIGNED_INT_VEC2              0x8DC6
#define GL_UNSIGNED_INT_VEC3              0x8DC7
#define GL_UNSIGNED_INT_VEC4              0x8DC8
#define GL_BGRA_INTEGER                   0x8D9B
#define GL_COMPRESSED_RED                 0x8225
#define GL_COMPRESSED_RG                  0x8226
#define GL_QUERY_WAIT                     0x8E13
#define GL_QUERY_NO_WAIT                  0x8E14
#define GL_QUERY_BY_REGION_WAIT           0x8E15
#define GL_QUERY_BY_REGION_NO_WAIT        0x8E16
typedef void (APIENTRYP PFNGLBINDBUFFERRANGEPROC) (GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
typedef void (APIENTRYP PFNGLBINDBUFFERBASEPROC) (GLenum target, GLuint index, GLuint buffer);
typedef void (APIENTRYP PFNGLBINDFRAGDATALOCATIONPROC) (GLuint program, GLuint color, const GLchar *name);
typedef void (APIENTRYP PFNGLCLAMPCOLORPROC) (GLenum target, GLenum clamp);
typedef const GLubyte * (APIENTRYP PFNGLGETSTRINGIPROC) (GLenum name, GLuint index);
typedef void (APIENTRYP PFNGLCOLORMASKIPROC) (GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a);
typedef void (APIENTRYP PFNGLENABLEIPROC) (GLenum target, GLuint index);
typedef void (APIENTRYP PFNGLDISABLEIPROC) (GLenum target, GLuint index);
typedef void (APIENTRYP PFNGLBEGINCONDITIONALRENDERPROC) (GLuint id, GLenum mode);
typedef void (APIENTRYP PFNGLENDCONDITIONALRENDERPROC) (void);
typedef void (APIENTRYP PFNGLUNIFORM1UIPROC) (GLint location, GLuint v0);
typedef void (APIENTRYP PFNGLUNIFORM2UIPROC) (GLint location, GLuint v0, GLuint v1);
typedef void (APIENTRYP PFNGLUNIFORM3UIPROC) (GLint location, GLuint v0, GLuint v1, GLuint v2);
typedef void (APIENTRYP PFNGLUNIFORM4UIPROC) (GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
typedef void (APIENTRYP PFNGLUNIFORM1UIVPROC) (GLint location, GLsizei count, const GLuint *value);
typedef void (APIENTRYP PFNGLUNIFORM2UIVPROC) (GLint location, GLsizei count, const GLuint *value);
typedef void (APIENTRYP PFNGLUNIFORM3UIVPROC) (GLint location, GLsizei count, const GLuint *value);
typedef void (APIENTRYP PFNGLUNIFORM4UIVPROC) (GLint location, GLsizei count, const GLuint *value);
typedef void (APIENTRYP PFNGLTEXPARAMETERIIVPROC) (GLenum target, GLenum pname, const GLint *params);
typedef void (APIENTRYP PFNGLTEXPARAMETERIUIVPROC) (GLenum target, GLenum pname, const GLuint *params);
typedef void (APIENTRYP PFNGLGETTEXPARAMETERIIVPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (APIENTRYP PFNGLGETTEXPARAMETERIUIVPROC) (GLenum target, GLenum pname, GLuint *params);
typedef void (APIENTRYP PFNGLCLEARBUFFERIVPROC) (GLenum buffer, GLint drawbuffer, const GLint *value);
typedef void (APIENTRYP PFNGLCLEARBUFFERUIVPROC) (GLenum buffer, GLint drawbuffer, const GLuint *value);
typedef void (APIENTRYP PFNGLCLEARBUFFERFVPROC) (GLenum buffer, GLint drawbuffer, const GLfloat *value);
typedef void (APIENTRYP PFNGLCLEARBUFFERFIPROC) (GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil);
#elif GL_GLEXT_VERSION < 43
typedef void (APIENTRYP PFNGLBINDBUFFERRANGEPROC) (GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
typedef void (APIENTRYP PFNGLBINDBUFFERBASEPROC) (GLenum target, GLuint index, GLuint buffer);
#endif

extern PFNGLGETSTRINGIPROC glGetStringi_;
extern PFNGLBINDBUFFERBASEPROC glBindBufferBase_;
extern PFNGLBINDBUFFERRANGEPROC glBindBufferRange_;
extern PFNGLCLEARBUFFERIVPROC glClearBufferiv_;
extern PFNGLCLEARBUFFERUIVPROC glClearBufferuiv_;
extern PFNGLCLEARBUFFERFVPROC glClearBufferfv_;
extern PFNGLCLEARBUFFERFIPROC glClearBufferfi_;

// GL_EXT_gpu_shader4
extern PFNGLBINDFRAGDATALOCATIONPROC glBindFragDataLocation_;
extern PFNGLUNIFORM1UIPROC glUniform1ui_;
extern PFNGLUNIFORM2UIPROC glUniform2ui_;
extern PFNGLUNIFORM3UIPROC glUniform3ui_;
extern PFNGLUNIFORM4UIPROC glUniform4ui_;
extern PFNGLUNIFORM1UIVPROC glUniform1uiv_;
extern PFNGLUNIFORM2UIVPROC glUniform2uiv_;
extern PFNGLUNIFORM3UIVPROC glUniform3uiv_;
extern PFNGLUNIFORM4UIVPROC glUniform4uiv_;

// GL_EXT_draw_buffers2
extern PFNGLCOLORMASKIPROC glColorMaski_;
extern PFNGLENABLEIPROC glEnablei_;
extern PFNGLDISABLEIPROC glDisablei_;

// GL_NV_conditional_render
extern PFNGLBEGINCONDITIONALRENDERPROC glBeginConditionalRender_;
extern PFNGLENDCONDITIONALRENDERPROC glEndConditionalRender_;

// GL_ARB_color_buffer_float
extern PFNGLCLAMPCOLORPROC glClampColor_;

#ifndef GL_EXT_texture_integer
#define GL_EXT_texture_integer 1
typedef void (APIENTRYP PFNGLCLEARCOLORIIEXTPROC) (GLint red, GLint green, GLint blue, GLint alpha);
typedef void (APIENTRYP PFNGLCLEARCOLORIUIEXTPROC) (GLuint red, GLuint green, GLuint blue, GLuint alpha);
#endif
extern PFNGLTEXPARAMETERIIVPROC glTexParameterIiv_;
extern PFNGLTEXPARAMETERIUIVPROC glTexParameterIuiv_;
extern PFNGLGETTEXPARAMETERIIVPROC glGetTexParameterIiv_;
extern PFNGLGETTEXPARAMETERIUIVPROC glGetTexParameterIuiv_;
extern PFNGLCLEARCOLORIIEXTPROC glClearColorIi_;
extern PFNGLCLEARCOLORIUIEXTPROC glClearColorIui_;

#ifndef GL_ARB_half_float_vertex
#define GL_ARB_half_float_vertex 1
#define GL_HALF_FLOAT                     0x140B
#endif

#ifndef GL_VERSION_3_1
#define GL_VERSION_3_1 1
#define GL_SAMPLER_2D_RECT                0x8B63
#define GL_SAMPLER_2D_RECT_SHADOW         0x8B64
#define GL_TEXTURE_RECTANGLE              0x84F5
#define GL_TEXTURE_BINDING_RECTANGLE      0x84F6
#define GL_MAX_RECTANGLE_TEXTURE_SIZE     0x84F8
#endif

#ifndef GL_ARB_copy_buffer
#define GL_ARB_copy_buffer 1
#define GL_COPY_READ_BUFFER_BINDING       0x8F36
#define GL_COPY_READ_BUFFER               GL_COPY_READ_BUFFER_BINDING
#define GL_COPY_WRITE_BUFFER_BINDING      0x8F37
#define GL_COPY_WRITE_BUFFER              GL_COPY_WRITE_BUFFER_BINDING
typedef void (APIENTRYP PFNGLCOPYBUFFERSUBDATAPROC) (GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size);
#endif
extern PFNGLCOPYBUFFERSUBDATAPROC glCopyBufferSubData_;

#ifndef GL_ARB_vertex_array_object
#define GL_ARB_vertex_array_object 1
#define GL_VERTEX_ARRAY_BINDING           0x85B5
typedef void (APIENTRYP PFNGLBINDVERTEXARRAYPROC) (GLuint array);
typedef void (APIENTRYP PFNGLDELETEVERTEXARRAYSPROC) (GLsizei n, const GLuint *arrays);
typedef void (APIENTRYP PFNGLGENVERTEXARRAYSPROC) (GLsizei n, GLuint *arrays);
typedef GLboolean (APIENTRYP PFNGLISVERTEXARRAYPROC) (GLuint array);
#endif
extern PFNGLBINDVERTEXARRAYPROC    glBindVertexArray_;
extern PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays_;
extern PFNGLGENVERTEXARRAYSPROC    glGenVertexArrays_;
extern PFNGLISVERTEXARRAYPROC      glIsVertexArray_;

#ifndef GL_ARB_depth_clamp
#define GL_ARB_depth_clamp 1
#define GL_DEPTH_CLAMP                    0x864F
#endif

#ifndef GL_ARB_texture_swizzle
#define GL_ARB_texture_swizzle 1
#define GL_TEXTURE_SWIZZLE_R              0x8E42
#define GL_TEXTURE_SWIZZLE_G              0x8E43
#define GL_TEXTURE_SWIZZLE_B              0x8E44
#define GL_TEXTURE_SWIZZLE_A              0x8E45
#define GL_TEXTURE_SWIZZLE_RGBA           0x8E46
#endif

#ifndef GL_ARB_occlusion_query2
#define GL_ARB_occlusion_query2 1
#define GL_ANY_SAMPLES_PASSED             0x8C2F
#endif

#ifndef GL_ARB_blend_func_extended
#define GL_ARB_blend_func_extended 1
#define GL_SRC1_COLOR                     0x88F9
#define GL_ONE_MINUS_SRC1_COLOR           0x88FA
#define GL_ONE_MINUS_SRC1_ALPHA           0x88FB
#define GL_MAX_DUAL_SOURCE_DRAW_BUFFERS   0x88FC
typedef void (APIENTRYP PFNGLBINDFRAGDATALOCATIONINDEXEDPROC) (GLuint program, GLuint colorNumber, GLuint index, const GLchar *name);
#endif
extern PFNGLBINDFRAGDATALOCATIONINDEXEDPROC glBindFragDataLocationIndexed_;

#ifndef GL_VERSION_4_0
#define GL_VERSION_4_0 1
#define GL_SAMPLE_SHADING                 0x8C36
#define GL_MIN_SAMPLE_SHADING_VALUE       0x8C37
typedef void (APIENTRYP PFNGLMINSAMPLESHADINGPROC) (GLfloat value);
typedef void (APIENTRYP PFNGLBLENDEQUATIONIPROC) (GLuint buf, GLenum mode);
typedef void (APIENTRYP PFNGLBLENDEQUATIONSEPARATEIPROC) (GLuint buf, GLenum modeRGB, GLenum modeAlpha);
typedef void (APIENTRYP PFNGLBLENDFUNCIPROC) (GLuint buf, GLenum src, GLenum dst);
typedef void (APIENTRYP PFNGLBLENDFUNCSEPARATEIPROC) (GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);
#endif

// GL_ARB_sample_shading
extern PFNGLMINSAMPLESHADINGPROC glMinSampleShading_;

// GL_ARB_draw_buffers_blend
extern PFNGLBLENDEQUATIONIPROC glBlendEquationi_;
extern PFNGLBLENDEQUATIONSEPARATEIPROC glBlendEquationSeparatei_;
extern PFNGLBLENDFUNCIPROC glBlendFunci_;
extern PFNGLBLENDFUNCSEPARATEIPROC glBlendFuncSeparatei_;

#ifndef GL_VERSION_4_3
#define GL_VERSION_4_3 1
#define GL_ANY_SAMPLES_PASSED_CONSERVATIVE 0x8D6A
#define GL_DEBUG_OUTPUT_SYNCHRONOUS       0x8242
#define GL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH 0x8243
#define GL_DEBUG_CALLBACK_FUNCTION        0x8244
#define GL_DEBUG_CALLBACK_USER_PARAM      0x8245
#define GL_DEBUG_SOURCE_API               0x8246
#define GL_DEBUG_SOURCE_WINDOW_SYSTEM     0x8247
#define GL_DEBUG_SOURCE_SHADER_COMPILER   0x8248
#define GL_DEBUG_SOURCE_THIRD_PARTY       0x8249
#define GL_DEBUG_SOURCE_APPLICATION       0x824A
#define GL_DEBUG_SOURCE_OTHER             0x824B
#define GL_DEBUG_TYPE_ERROR               0x824C
#define GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR 0x824D
#define GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR  0x824E
#define GL_DEBUG_TYPE_PORTABILITY         0x824F
#define GL_DEBUG_TYPE_PERFORMANCE         0x8250
#define GL_DEBUG_TYPE_OTHER               0x8251
#define GL_MAX_DEBUG_MESSAGE_LENGTH       0x9143
#define GL_MAX_DEBUG_LOGGED_MESSAGES      0x9144
#define GL_DEBUG_LOGGED_MESSAGES          0x9145
#define GL_DEBUG_SEVERITY_HIGH            0x9146
#define GL_DEBUG_SEVERITY_MEDIUM          0x9147
#define GL_DEBUG_SEVERITY_LOW             0x9148
typedef void (APIENTRY *GLDEBUGPROC)(GLenum source,GLenum type,GLuint id,GLenum severity,GLsizei length,const GLchar *message,const void *userParam);
typedef void (APIENTRYP PFNGLDEBUGMESSAGECONTROLPROC) (GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint *ids, GLboolean enabled);
typedef void (APIENTRYP PFNGLDEBUGMESSAGEINSERTPROC) (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *buf);
typedef void (APIENTRYP PFNGLDEBUGMESSAGECALLBACKPROC) (GLDEBUGPROC callback, const void *userParam);
typedef GLuint (APIENTRYP PFNGLGETDEBUGMESSAGELOGPROC) (GLuint count, GLsizei bufSize, GLenum *sources, GLenum *types, GLuint *ids, GLenum *severities, GLsizei *lengths, GLchar *messageLog);
typedef void (APIENTRYP PFNGLCOPYIMAGESUBDATAPROC) (GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth);
#endif

// GL_ARB_debug_output
extern PFNGLDEBUGMESSAGECONTROLPROC glDebugMessageControl_;
extern PFNGLDEBUGMESSAGEINSERTPROC glDebugMessageInsert_;
extern PFNGLDEBUGMESSAGECALLBACKPROC glDebugMessageCallback_;
extern PFNGLGETDEBUGMESSAGELOGPROC glGetDebugMessageLog_;

// GL_ARB_copy_image
extern PFNGLCOPYIMAGESUBDATAPROC glCopyImageSubData_;

