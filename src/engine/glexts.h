// OpenGL 1.3
#ifndef WIN32
#define glActiveTexture_ glActiveTexture
#define glClientActiveTexture_ glClientActiveTexture

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
extern PFNGLACTIVETEXTUREPROC       glActiveTexture_;
extern PFNGNCLIENTACTIVETEXTUREPROC glClientActiveTexture_;

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

#ifndef GL_VERSION_2_1
#define GL_VERSION_2_1 1
#define GL_FLOAT_MAT2x3                   0x8B65
#define GL_FLOAT_MAT2x4                   0x8B66
#define GL_FLOAT_MAT3x2                   0x8B67
#define GL_FLOAT_MAT3x4                   0x8B68
#define GL_FLOAT_MAT4x2                   0x8B69
#define GL_FLOAT_MAT4x3                   0x8B6A
typedef void (APIENTRYP PFNGLUNIFORMMATRIX2X3FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP PFNGLUNIFORMMATRIX3X2FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP PFNGLUNIFORMMATRIX2X4FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP PFNGLUNIFORMMATRIX4X2FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP PFNGLUNIFORMMATRIX3X4FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP PFNGLUNIFORMMATRIX4X3FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
#endif

// OpenGL 2.1
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

#define glUniformMatrix2x3fv_ glUniformMatrix2x3fv
#define glUniformMatrix3x2fv_ glUniformMatrix3x2fv
#define glUniformMatrix2x4fv_ glUniformMatrix2x4fv
#define glUniformMatrix4x2fv_ glUniformMatrix4x2fv
#define glUniformMatrix3x4fv_ glUniformMatrix3x4fv
#define glUniformMatrix4x3fv_ glUniformMatrix4x3fv

#define glDrawBuffers_ glDrawBuffers
#else
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

extern PFNGLUNIFORMMATRIX2X3FVPROC       glUniformMatrix2x3fv_;
extern PFNGLUNIFORMMATRIX3X2FVPROC       glUniformMatrix3x2fv_;
extern PFNGLUNIFORMMATRIX2X4FVPROC       glUniformMatrix2x4fv_;
extern PFNGLUNIFORMMATRIX4X2FVPROC       glUniformMatrix4x2fv_;
extern PFNGLUNIFORMMATRIX3X4FVPROC       glUniformMatrix3x4fv_;
extern PFNGLUNIFORMMATRIX4X3FVPROC       glUniformMatrix4x3fv_;

extern PFNGLDRAWBUFFERSPROC glDrawBuffers_;
#endif

// GL_EXT_framebuffer_object
extern PFNGLBINDRENDERBUFFEREXTPROC        glBindRenderbuffer_;
extern PFNGLDELETERENDERBUFFERSEXTPROC     glDeleteRenderbuffers_;
extern PFNGLGENFRAMEBUFFERSEXTPROC         glGenRenderbuffers_;
extern PFNGLRENDERBUFFERSTORAGEEXTPROC     glRenderbufferStorage_;
extern PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC  glCheckFramebufferStatus_;
extern PFNGLBINDFRAMEBUFFEREXTPROC         glBindFramebuffer_;
extern PFNGLDELETEFRAMEBUFFERSEXTPROC      glDeleteFramebuffers_;
extern PFNGLGENFRAMEBUFFERSEXTPROC         glGenFramebuffers_;
extern PFNGLFRAMEBUFFERTEXTURE2DEXTPROC    glFramebufferTexture2D_;
extern PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC glFramebufferRenderbuffer_;
extern PFNGLGENERATEMIPMAPEXTPROC          glGenerateMipmap_;

// GL_EXT_framebuffer_blit
#ifndef GL_EXT_framebuffer_blit
#define GL_READ_FRAMEBUFFER_EXT           0x8CA8
#define GL_DRAW_FRAMEBUFFER_EXT           0x8CA9
#define GL_DRAW_FRAMEBUFFER_BINDING_EXT   0x8CA6
#define GL_READ_FRAMEBUFFER_BINDING_EXT   0x8CAA
typedef void (APIENTRYP PFNGLBLITFRAMEBUFFEREXTPROC) (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
#endif
extern PFNGLBLITFRAMEBUFFEREXTPROC         glBlitFramebuffer_;

// GL_EXT_packed_depth_stencil
#ifndef GL_DEPTH_STENCIL_EXT
#define GL_DEPTH_STENCIL_EXT 0x84F9
#endif
#ifndef GL_DEPTH24_STENCIL8_EXT
#define GL_DEPTH24_STENCIL8_EXT 0x88F0
#endif

// GL_ARB_texture_rg
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

// GL_ARB_map_buffer_range
#ifndef GL_ARB_map_buffer_range
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

#ifndef GL_VERSION_3_0
#define GL_VERSION_3_0 1
typedef void (APIENTRYP PFNGLBINDBUFFERRANGEPROC) (GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
typedef void (APIENTRYP PFNGLBINDBUFFERBASEPROC) (GLenum target, GLuint index, GLuint buffer);
#elif GL_GLEXT_VERSION < 43
typedef void (APIENTRYP PFNGLBINDBUFFERRANGEPROC) (GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
typedef void (APIENTRYP PFNGLBINDBUFFERBASEPROC) (GLenum target, GLuint index, GLuint buffer);
#endif

// GL_ARB_uniform_buffer_object
extern PFNGLGETUNIFORMINDICESPROC       glGetUniformIndices_;
extern PFNGLGETACTIVEUNIFORMSIVPROC     glGetActiveUniformsiv_;
extern PFNGLGETUNIFORMBLOCKINDEXPROC    glGetUniformBlockIndex_;
extern PFNGLGETACTIVEUNIFORMBLOCKIVPROC glGetActiveUniformBlockiv_;
extern PFNGLUNIFORMBLOCKBINDINGPROC     glUniformBlockBinding_;
extern PFNGLBINDBUFFERBASEPROC          glBindBufferBase_;
extern PFNGLBINDBUFFERRANGEPROC         glBindBufferRange_;

