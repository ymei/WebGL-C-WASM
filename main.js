const canvas = document.getElementById('c');
const gl = canvas.getContext('webgl2', {antialias: true});
if (!gl) throw new Error('WebGL2 required');

let wasm, memory;
const textDec = new TextDecoder();
const textEnc = new TextEncoder();

// Handle pools
const shaders = [];
const programs = [];
const buffers = [];
const uniforms = [];
const vaos = [];
const textures = [];

// Heap views
const u8  = () => new Uint8Array(memory.buffer);
const f32 = () => new Float32Array(memory.buffer);

// Helpers
function cstr(ptr) {
    const bytes = u8();
    let end = ptr;
    while (bytes[end] !== 0) ++end;
    return textDec.decode(bytes.subarray(ptr, end));
}
function view(ptr, byteLen) { return u8().subarray(ptr, ptr + byteLen); }

function resize() {
    const dpr = Math.min(2, window.devicePixelRatio || 1);
    const w = Math.floor(canvas.clientWidth * dpr);
    const h = Math.floor(canvas.clientHeight * dpr);
    if (canvas.width !== w || canvas.height !== h) {
        canvas.width = w; canvas.height = h;
        gl.viewport(0, 0, w, h);
        if (wasm) wasm.exports.set_viewport(w, h);
    }
}
window.addEventListener('resize', resize);

const imports = {
    env: {
        // Debug
        debug_log: (ptr) => console.log(cstr(ptr)),

        // Init
        js_init() {
            gl.enable(gl.DEPTH_TEST);
            gl.clearDepth(1.0);
            resize();
            gl.clearColor(0.1, 0.1, 0.12, 1.0);
            gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
        },

        // Math fallbacks if LLVM emits calls
        sinf: x => Math.sin(x),
        cosf: x => Math.cos(x),
        tanf: x => Math.tan(x),

        // Shaders
        gl_create_shader: (type) => { const s = gl.createShader(type); shaders.push(s); return shaders.length - 1; },
        gl_shader_source: (sid, srcPtr) => { gl.shaderSource(shaders[sid], cstr(srcPtr)); },
        gl_compile_shader: (sid) => { gl.compileShader(shaders[sid]); },
        gl_get_shader_iv: (sid, pname) => gl.getShaderParameter(shaders[sid], pname) ? 1 : 0,
        gl_get_shader_info_log: (sid, outPtr, maxLen) => {
            const log = gl.getShaderInfoLog(shaders[sid]) || '';
            const enc = textEnc.encode(log);
            const dst = u8();
            const n = Math.min(enc.length, maxLen - 1);
            dst.set(enc.subarray(0, n), outPtr);
            dst[outPtr + n] = 0;
        },

        // Program
        gl_create_program: () => { const p = gl.createProgram(); programs.push(p); return programs.length - 1; },
        gl_attach_shader: (pid, sid) => { gl.attachShader(programs[pid], shaders[sid]); },
        gl_link_program: (pid) => { gl.linkProgram(programs[pid]); },
        gl_get_program_iv: (pid, pname) => gl.getProgramParameter(programs[pid], pname) ? 1 : 0,
        gl_get_program_info_log: (pid, outPtr, maxLen) => {
            const log = gl.getProgramInfoLog(programs[pid]) || '';
            const enc = textEnc.encode(log);
            const dst = u8();
            const n = Math.min(enc.length, maxLen - 1);
            dst.set(enc.subarray(0, n), outPtr);
            dst[outPtr + n] = 0;
        },
        gl_use_program: (pid) => { gl.useProgram(programs[pid]); },

        // Buffers + attributes
        gl_gen_buffer: () => { const b = gl.createBuffer(); buffers.push(b); return buffers.length - 1; },
        gl_bind_buffer: (target, bid) => { gl.bindBuffer(target, buffers[bid]); },
        gl_buffer_data: (target, srcPtr, byteLen, usage) => { gl.bufferData(target, view(srcPtr, byteLen), usage); },
        gl_enable_vertex_attrib_array: (loc) => { gl.enableVertexAttribArray(loc); },
        gl_vertex_attrib_pointer: (loc, size, type, normalized, stride, offset) => {
            gl.vertexAttribPointer(loc, size, type, !!normalized, stride, offset);
        },

        // VAO
        gl_create_vertex_array: () => { const v = gl.createVertexArray(); vaos.push(v); return vaos.length - 1; },
        gl_bind_vertex_array: (vid) => { gl.bindVertexArray(vid >= 0 ? vaos[vid] : null); },

        // Uniforms
        gl_get_uniform_location: (pid, namePtr) => {
            const u = gl.getUniformLocation(programs[pid], cstr(namePtr));
            uniforms.push(u); return uniforms.length - 1;
        },
        gl_uniform_matrix4fv: (uid, transpose, ptr) => {
            gl.uniformMatrix4fv(uniforms[uid], !!transpose, f32().subarray(ptr>>2, (ptr>>2) + 16));
        },
        gl_uniform_matrix3fv: (uid, transpose, ptr) => {
            gl.uniformMatrix3fv(uniforms[uid], !!transpose, f32().subarray(ptr>>2, (ptr>>2) + 9));
        },
        gl_uniform1i: (uid, x) => { gl.uniform1i(uniforms[uid], x); },
        gl_uniform3f: (uid, x, y, z) => { gl.uniform3f(uniforms[uid], x, y, z); },

        // State + draw
        gl_viewport: (x, y, w, h) => { gl.viewport(x, y, w, h); },
        gl_clear_color: (r,g,b,a) => { gl.clearColor(r,g,b,a); },
        gl_clear: (mask) => { gl.clear(mask); },
        gl_draw_elements: (mode, count, type, offset) => { gl.drawElements(mode, count, type, offset); },

        // Textures
        gl_create_texture: () => { const t = gl.createTexture(); textures.push(t); return textures.length - 1; },
        gl_bind_texture: (target, tid) => { gl.bindTexture(target, textures[tid]); },
        gl_active_texture: (unit) => { gl.activeTexture(unit); },
        gl_tex_parameteri: (target, pname, param) => { gl.texParameteri(target, pname, param); },
        gl_tex_image_2d: (target, level, internalFormat, width, height, border, format, type, ptr) => {
            const pixels = new Uint8Array(memory.buffer, ptr, width*height*4);
            gl.texImage2D(target, level, internalFormat, width, height, border, format, type, pixels);
        },
    }
};

// Load and start
const resp = await fetch('main.wasm');
const {instance} = await WebAssembly.instantiateStreaming(resp, { env: imports.env });
wasm = instance;
memory = wasm.exports.memory;

imports.env.js_init();
wasm.exports.init();

let tPrev = performance.now();
function tick(tNow) {
    const dt = (tNow - tPrev) * 0.001;
    tPrev = tNow;
    wasm.exports.frame(dt);
    requestAnimationFrame(tick);
}
requestAnimationFrame(tick);
