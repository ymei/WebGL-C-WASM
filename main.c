// GL enums used
#define GL_VERTEX_SHADER              0x8B31
#define GL_FRAGMENT_SHADER            0x8B30
#define GL_ARRAY_BUFFER               0x8892
#define GL_ELEMENT_ARRAY_BUFFER       0x8893
#define GL_STATIC_DRAW                0x88E4
#define GL_FLOAT                      0x1406
#define GL_TRIANGLES                  0x0004
#define GL_UNSIGNED_SHORT             0x1403
#define GL_COLOR_BUFFER_BIT           0x4000
#define GL_DEPTH_BUFFER_BIT           0x0100
#define GL_LINK_STATUS                0x8B82
#define GL_COMPILE_STATUS             0x8B81
#define GL_TEXTURE_2D                 0x0DE1
#define GL_TEXTURE0                   0x84C0
#define GL_TEXTURE_MIN_FILTER         0x2801
#define GL_TEXTURE_MAG_FILTER         0x2800
#define GL_TEXTURE_WRAP_S             0x2802
#define GL_TEXTURE_WRAP_T             0x2803
#define GL_NEAREST                    0x2600
#define GL_LINEAR                     0x2601
#define GL_CLAMP_TO_EDGE              0x812F
#define GL_RGBA                       0x1908
#define GL_UNSIGNED_BYTE              0x1401

// JS interop
extern void js_init(void);
extern void debug_log(const char* s);

extern int  gl_create_shader(int type);
extern void gl_shader_source(int sid, const char* src);
extern void gl_compile_shader(int sid);
extern int  gl_get_shader_iv(int sid, int pname);
extern void gl_get_shader_info_log(int sid, char* out, int maxLen);

extern int  gl_create_program(void);
extern void gl_attach_shader(int pid, int sid);
extern void gl_link_program(int pid);
extern int  gl_get_program_iv(int pid, int pname);
extern void gl_get_program_info_log(int pid, char* out, int maxLen);
extern void gl_use_program(int pid);

extern int  gl_gen_buffer(void);
extern void gl_bind_buffer(int target, int bid);
extern void gl_buffer_data(int target, const void* ptr, int byteLen, int usage);
extern void gl_enable_vertex_attrib_array(int loc);
extern void gl_vertex_attrib_pointer(int loc, int size, int type, int normalized, int stride, int offset);

extern int  gl_create_vertex_array(void);
extern void gl_bind_vertex_array(int vid);

extern int  gl_get_uniform_location(int pid, const char* name);
extern void gl_uniform_matrix4fv(int uid, int transpose, const float* m);
extern void gl_uniform_matrix3fv(int uid, int transpose, const float* m3);
extern void gl_uniform1i(int uid, int x);
extern void gl_uniform3f(int uid, float x, float y, float z);

extern void gl_viewport(int x, int y, int w, int h);
extern void gl_clear_color(float r, float g, float b, float a);
extern void gl_clear(int mask);
extern void gl_draw_elements(int mode, int count, int type, int offset);

extern int  gl_create_texture(void);
extern void gl_bind_texture(int target, int tid);
extern void gl_active_texture(int unit);
extern void gl_tex_parameteri(int target, int pname, int param);
extern void gl_tex_image_2d(int target, int level, int internalFormat, int width, int height, int border, int format, int type, const void* ptr);

// Globals
static int prog;
static int u_mvp, u_normal3x3, u_lightDir, u_sampler;
static int vbo_pos, vbo_col, vbo_nrm, vbo_uv, ebo, vao;
static int tex0;

static float angle = 0.0f;
static int vp_w = 1, vp_h = 1;

// Column-major 4x4.  index = col*4 + row.
static void mat4_identity(float* m) {
    for (int i=0;i<16;i++) m[i]=0.0f;
    m[0]=m[5]=m[10]=m[15]=1.0f;
}
static void mat4_mul(float* r, const float* a, const float* b) {
    float t[16];
    for (int col=0; col<4; ++col)
        for (int row=0; row<4; ++row)
            t[col*4 + row] =
                a[0*4 + row]*b[col*4 + 0] +
                a[1*4 + row]*b[col*4 + 1] +
                a[2*4 + row]*b[col*4 + 2] +
                a[3*4 + row]*b[col*4 + 3];
    for (int i=0;i<16;i++) r[i]=t[i];
}
static void mat4_perspective(float* m, float fovy, float aspect, float znear, float zfar) {
    float s = __builtin_sinf(fovy*0.5f);
    float c = __builtin_cosf(fovy*0.5f);
    float f = c / s; // 1/tan(fovy/2)
    for (int i=0;i<16;i++) m[i]=0.0f;
    m[0]  = f/aspect;
    m[5]  = f;
    m[10] = (zfar+znear)/(znear - zfar);
    m[11] = -1.0f;
    m[14] = (2.0f*zfar*znear)/(znear - zfar);
}
static void mat4_translate(float* m, float x, float y, float z) {
    mat4_identity(m);
    m[12]=x; m[13]=y; m[14]=z;
}
static void mat4_rotate_y(float* m, float a) {
    mat4_identity(m);
    float c = __builtin_cosf(a), s = __builtin_sinf(a);
    m[0]= c;  m[8]=  s;
    m[2]=-s;  m[10]= c;
}
static void mat3_from_rotation_y(float* m3, float a) {
    float c = __builtin_cosf(a), s = __builtin_sinf(a);
    // column-major 3x3
    m3[0]= c; m3[3]= 0; m3[6]= -s;
    m3[1]= 0; m3[4]= 1; m3[7]=  0;
    m3[2]= s; m3[5]= 0; m3[8]=  c;
}

// Cube data (8 vertices shared; simple normals and UVs)
static const float positions[] = {
    -1,-1,-1,  1,-1,-1,  1, 1,-1, -1, 1,-1,
    -1,-1, 1,  1,-1, 1,  1, 1, 1, -1, 1, 1
};
static const float colors[] = {
    1,0,0,  0,1,0,  0,0,1,  1,1,0,
    1,0,1,  0,1,1,  1,1,1,  0.2f,0.2f,0.2f
};
static const float normals[] = {
    0,0,-1, 0,0,-1, 0,0,-1, 0,0,-1,
    0,0, 1, 0,0, 1, 0,0, 1, 0,0, 1
};
static const float uvs[] = {
    0,0, 1,0, 1,1, 0,1,
    0,0, 1,0, 1,1, 0,1
};
static const unsigned short indices[] = {
    0,1,2, 2,3,0,
    4,5,6, 6,7,4,
    0,4,7, 7,3,0,
    1,5,6, 6,2,1,
    3,2,6, 6,7,3,
    0,1,5, 5,4,0
};

// GLSL ES 3.00 with normals, UVs, texture, diffuse light
static const char* vs_src =
    "#version 300 es\n"
    "precision mediump float;\n"
    "layout(location=0) in vec3 a_pos;\n"
    "layout(location=1) in vec3 a_col;\n"
    "layout(location=2) in vec3 a_nrm;\n"
    "layout(location=3) in vec2 a_uv;\n"
    "uniform mat4 u_mvp;\n"
    "uniform mat3 u_normal;\n"
    "out vec3 v_col;\n"
    "out vec3 v_nrm;\n"
    "out vec2 v_uv;\n"
    "void main(){\n"
    "  v_col = a_col;\n"
    "  v_nrm = normalize(u_normal * a_nrm);\n"
    "  v_uv  = a_uv;\n"
    "  gl_Position = u_mvp * vec4(a_pos,1.0);\n"
    "}\n";

static const char* fs_src =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in vec3 v_col;\n"
    "in vec3 v_nrm;\n"
    "in vec2 v_uv;\n"
    "uniform sampler2D u_tex;\n"
    "uniform vec3 u_lightDir;\n"
    "out vec4 frag;\n"
    "void main(){\n"
    "  vec3 N = normalize(v_nrm);\n"
    "  float diff = max(dot(N, normalize(u_lightDir)), 0.0);\n"
    "  vec3 texc = texture(u_tex, v_uv).rgb;\n"
    "  vec3 base = texc * v_col;\n"
    "  vec3 color = base * (0.15 + 0.85*diff);\n"
    "  frag = vec4(color, 1.0);\n"
    "}\n";

// Log buffer
static char logbuf[1024];

static int make_shader(int type, const char* src) {
    int s = gl_create_shader(type);
    gl_shader_source(s, src);
    gl_compile_shader(s);
    if (!gl_get_shader_iv(s, GL_COMPILE_STATUS)) {
        gl_get_shader_info_log(s, logbuf, sizeof logbuf);
        debug_log(logbuf);
    }
    return s;
}
static int make_program(const char* vs, const char* fs) {
    int v = make_shader(GL_VERTEX_SHADER, vs);
    int f = make_shader(GL_FRAGMENT_SHADER, fs);
    int p = gl_create_program();
    gl_attach_shader(p, v);
    gl_attach_shader(p, f);
    gl_link_program(p);
    if (!gl_get_program_iv(p, GL_LINK_STATUS)) {
        gl_get_program_info_log(p, logbuf, sizeof logbuf);
        debug_log(logbuf);
    }
    return p;
}

// Procedural checkerboard RGBA8
static unsigned char checker[64*64*4];
static void make_checker(void) {
    for (int y=0; y<64; ++y) for (int x=0; x<64; ++x) {
            int c = ((x>>3) ^ (y>>3)) & 1;
            int i = (y*64 + x)*4;
            checker[i+0] = c ? 220 : 40;
            checker[i+1] = c ? 220 : 40;
            checker[i+2] = c ? 220 : 200;
            checker[i+3] = 255;
        }
}

__attribute__((export_name("init")))
void init(void) {
    js_init();

    prog = make_program(vs_src, fs_src);
    gl_use_program(prog);
    u_mvp       = gl_get_uniform_location(prog, "u_mvp");
    u_normal3x3 = gl_get_uniform_location(prog, "u_normal");
    u_lightDir  = gl_get_uniform_location(prog, "u_lightDir");
    u_sampler   = gl_get_uniform_location(prog, "u_tex");

    vao = gl_create_vertex_array();
    gl_bind_vertex_array(vao);

    // Positions
    vbo_pos = gl_gen_buffer();
    gl_bind_buffer(GL_ARRAY_BUFFER, vbo_pos);
    gl_buffer_data(GL_ARRAY_BUFFER, positions, sizeof(positions), GL_STATIC_DRAW);
    gl_enable_vertex_attrib_array(0);
    gl_vertex_attrib_pointer(0, 3, GL_FLOAT, 0, 3*sizeof(float), 0);

    // Colors
    vbo_col = gl_gen_buffer();
    gl_bind_buffer(GL_ARRAY_BUFFER, vbo_col);
    gl_buffer_data(GL_ARRAY_BUFFER, colors, sizeof(colors), GL_STATIC_DRAW);
    gl_enable_vertex_attrib_array(1);
    gl_vertex_attrib_pointer(1, 3, GL_FLOAT, 0, 3*sizeof(float), 0);

    // Normals
    vbo_nrm = gl_gen_buffer();
    gl_bind_buffer(GL_ARRAY_BUFFER, vbo_nrm);
    gl_buffer_data(GL_ARRAY_BUFFER, normals, sizeof(normals), GL_STATIC_DRAW);
    gl_enable_vertex_attrib_array(2);
    gl_vertex_attrib_pointer(2, 3, GL_FLOAT, 0, 3*sizeof(float), 0);

    // UVs
    vbo_uv = gl_gen_buffer();
    gl_bind_buffer(GL_ARRAY_BUFFER, vbo_uv);
    gl_buffer_data(GL_ARRAY_BUFFER, uvs, sizeof(uvs), GL_STATIC_DRAW);
    gl_enable_vertex_attrib_array(3);
    gl_vertex_attrib_pointer(3, 2, GL_FLOAT, 0, 2*sizeof(float), 0);

    // Indices
    ebo = gl_gen_buffer();
    gl_bind_buffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    gl_buffer_data(GL_ELEMENT_ARRAY_BUFFER, indices, sizeof(indices), GL_STATIC_DRAW);

    // Texture
    make_checker();
    tex0 = gl_create_texture();
    gl_active_texture(GL_TEXTURE0);
    gl_bind_texture(GL_TEXTURE_2D, tex0);
    gl_tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl_tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl_tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl_tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl_tex_image_2d(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, checker);

    // Bind sampler to texture unit 0
    gl_uniform1i(u_sampler, 0);
}

__attribute__((export_name("set_viewport")))
void set_viewport(int w, int h) { vp_w=w; vp_h=h; }

__attribute__((export_name("frame")))
void frame(float dt) {
    angle += dt;

    float P[16], T[16], R[16], M[16], MVP[16];
    float N3[9];
    float aspect = (vp_h>0) ? (float)vp_w/(float)vp_h : 1.0f;

    mat4_perspective(P, 60.0f*(3.14159265f/180.0f), aspect, 0.1f, 100.0f);
    mat4_translate(T, 0.0f, 0.0f, -6.0f);
    mat4_rotate_y(R, angle);
    mat4_mul(M, T, R);
    mat4_mul(MVP, P, M);
    mat3_from_rotation_y(N3, angle);

    gl_clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    gl_use_program(prog);
    gl_bind_vertex_array(vao);
    gl_uniform_matrix4fv(u_mvp, 0, MVP);
    gl_uniform_matrix3fv(u_normal3x3, 0, N3);
    gl_uniform3f(u_lightDir, 0.4f, 0.6f, 0.7f);
    gl_draw_elements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, 0);
}
