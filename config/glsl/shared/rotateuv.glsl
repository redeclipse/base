// Texture coordinate rotation, the GLSL counterpart of rotateuv in
// config/glsl/init.cfg (config/comp/misc.cfg still uses the alias; keep the
// two in step). ROTATEUV_FUNC defines vec2 rotateuv(uv, rotation, mid):
// uv rotated by rotation radians about mid. Write it where the shader
// defines its functions.
#define ROTATEUV_FUNC vec2 rotateuv(vec2 uv, float rotation, vec2 mid) { float cosangle = cos(rotation); float sinangle = sin(rotation); vec2 p = uv - mid; return vec2( cosangle * p.x + sinangle * p.y + mid.x, cosangle * p.y - sinangle * p.x + mid.y ); }
