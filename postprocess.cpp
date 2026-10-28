/*
 * Fade To Black engine rewrite
 * Copyright (C) 2006-2012 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <stdio.h>
#include <string.h>
#include "postprocess.h"

#define GL_FRAMEBUFFER_EXT 0x8D40
#define GL_RENDERBUFFER_EXT 0x8D41
#define GL_COLOR_ATTACHMENT0_EXT 0x8CE0
#define GL_DEPTH_ATTACHMENT_EXT 0x8D00
#define GL_FRAMEBUFFER_COMPLETE_EXT 0x8CD5
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_CLAMP_TO_EDGE 0x812F

static const char *kVertexShader =
	"#version 120\n"
	"varying vec2 vTexCoord;\n"
	"void main() {\n"
	"  gl_Position = gl_Vertex;\n"
	"  vTexCoord = gl_MultiTexCoord0.xy;\n"
	"}\n";

static const char *kCrtEasyModeShader =
	"#version 120\n"
	"uniform sampler2D uTexture;\n"
	"uniform vec2 uResolution;\n"
	"varying vec2 vTexCoord;\n"
	"void main() {\n"
	"  vec3 color = texture2D(uTexture, vTexCoord).rgb;\n"
	"  float scanline = 0.90 + 0.10 * cos(vTexCoord.y * uResolution.y * 3.14159265);\n"
	"  float triad = mod(floor(gl_FragCoord.x), 3.0);\n"
	"  vec3 mask = triad < 1.0 ? vec3(1.0, 0.82, 0.82) :\n"
	"              (triad < 2.0 ? vec3(0.82, 1.0, 0.82) : vec3(0.82, 0.82, 1.0));\n"
	"  gl_FragColor = vec4(color * scanline * mask, 1.0);\n"
	"}\n";

static const char *kCrtGeomShader =
	"#version 120\n"
	"uniform sampler2D uTexture;\n"
	"uniform vec2 uResolution;\n"
	"varying vec2 vTexCoord;\n"
	"void main() {\n"
	"  vec2 p = vTexCoord * 2.0 - 1.0;\n"
	"  p *= 1.0 + dot(p, p) * 0.055;\n"
	"  vec2 uv = p * 0.5 + 0.5;\n"
	"  if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {\n"
	"    gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0); return;\n"
	"  }\n"
	"  vec3 color = texture2D(uTexture, uv).rgb;\n"
	"  float scanline = 0.78 + 0.22 * cos(uv.y * uResolution.y * 3.14159265);\n"
	"  float triad = mod(floor(gl_FragCoord.x), 3.0);\n"
	"  vec3 mask = triad < 1.0 ? vec3(1.0, 0.86, 0.86) :\n"
	"              (triad < 2.0 ? vec3(0.86, 1.0, 0.86) : vec3(0.86, 0.86, 1.0));\n"
	"  float vignette = 1.0 - 0.18 * dot(p, p);\n"
	"  gl_FragColor = vec4(color * scanline * mask * vignette, 1.0);\n"
	"}\n";

static const char *kLcdGridShader =
	"#version 120\n"
	"uniform sampler2D uTexture;\n"
	"uniform vec2 uResolution;\n"
	"varying vec2 vTexCoord;\n"
	"void main() {\n"
	"  vec3 color = texture2D(uTexture, vTexCoord).rgb;\n"
	"  float subpixel = mod(floor(gl_FragCoord.x), 3.0);\n"
	"  vec3 mask = subpixel < 1.0 ? vec3(1.0, 0.72, 0.72) :\n"
	"              (subpixel < 2.0 ? vec3(0.72, 1.0, 0.72) : vec3(0.72, 0.72, 1.0));\n"
	"  float row = mod(floor(gl_FragCoord.y), 2.0);\n"
	"  float grid = row < 1.0 ? 0.92 : 1.0;\n"
	"  gl_FragColor = vec4(color * mask * grid, 1.0);\n"
	"}\n";

struct PostProcessor::Impl {
	PFNGLCREATESHADERPROC createShader;
	PFNGLSHADERSOURCEPROC shaderSource;
	PFNGLCOMPILESHADERPROC compileShader;
	PFNGLGETSHADERIVPROC getShaderiv;
	PFNGLGETSHADERINFOLOGPROC getShaderInfoLog;
	PFNGLDELETESHADERPROC deleteShader;
	PFNGLCREATEPROGRAMPROC createProgram;
	PFNGLATTACHSHADERPROC attachShader;
	PFNGLLINKPROGRAMPROC linkProgram;
	PFNGLGETPROGRAMIVPROC getProgramiv;
	PFNGLGETPROGRAMINFOLOGPROC getProgramInfoLog;
	PFNGLDELETEPROGRAMPROC deleteProgram;
	PFNGLUSEPROGRAMPROC useProgram;
	PFNGLGETUNIFORMLOCATIONPROC getUniformLocation;
	PFNGLUNIFORM1IPROC uniform1i;
	PFNGLUNIFORM2FPROC uniform2f;
	PFNGLGENFRAMEBUFFERSEXTPROC genFramebuffers;
	PFNGLBINDFRAMEBUFFEREXTPROC bindFramebuffer;
	PFNGLDELETEFRAMEBUFFERSEXTPROC deleteFramebuffers;
	PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC checkFramebufferStatus;
	PFNGLFRAMEBUFFERTEXTURE2DEXTPROC framebufferTexture2D;
	PFNGLGENRENDERBUFFERSEXTPROC genRenderbuffers;
	PFNGLBINDRENDERBUFFEREXTPROC bindRenderbuffer;
	PFNGLDELETERENDERBUFFERSEXTPROC deleteRenderbuffers;
	PFNGLRENDERBUFFERSTORAGEEXTPROC renderbufferStorage;
	PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC framebufferRenderbuffer;
	GLuint framebuffer;
	GLuint colorTexture;
	GLuint depthBuffer;
	GLuint program;
	GLint textureUniform;
	GLint resolutionUniform;
	int width;
	int height;
	bool requested;
	bool enabled;
	const char *shaderName;
	const char *fragmentShader;

	Impl(const char *shader)
		: createShader(0), shaderSource(0), compileShader(0), getShaderiv(0),
		  getShaderInfoLog(0), deleteShader(0), createProgram(0), attachShader(0),
		  linkProgram(0), getProgramiv(0), getProgramInfoLog(0), deleteProgram(0),
		  useProgram(0), getUniformLocation(0), uniform1i(0), uniform2f(0),
		  genFramebuffers(0), bindFramebuffer(0), deleteFramebuffers(0),
		  checkFramebufferStatus(0), framebufferTexture2D(0), genRenderbuffers(0),
		  bindRenderbuffer(0), deleteRenderbuffers(0), renderbufferStorage(0),
		  framebufferRenderbuffer(0), framebuffer(0), colorTexture(0), depthBuffer(0),
		  program(0), textureUniform(-1), resolutionUniform(-1), width(0), height(0),
		  requested(shader && strcmp(shader, "none") != 0), enabled(false),
		  shaderName(shader ? shader : "none"), fragmentShader(0) {
		if (strcmp(shaderName, "crt-easymode") == 0) {
			fragmentShader = kCrtEasyModeShader;
		} else if (strcmp(shaderName, "crt-geom") == 0) {
			fragmentShader = kCrtGeomShader;
		} else if (strcmp(shaderName, "lcd-grid") == 0) {
			fragmentShader = kLcdGridShader;
		} else if (requested) {
			fprintf(stderr, "Unknown post-processing shader '%s'; available: none, crt-easymode, crt-geom, lcd-grid\n", shaderName);
			requested = false;
		}
	}

	bool loadProc() {
#define LOAD_PROC(field, name) field = reinterpret_cast<decltype(field)>(SDL_GL_GetProcAddress(name)); if (!field) return false
		LOAD_PROC(createShader, "glCreateShader");
		LOAD_PROC(shaderSource, "glShaderSource");
		LOAD_PROC(compileShader, "glCompileShader");
		LOAD_PROC(getShaderiv, "glGetShaderiv");
		LOAD_PROC(getShaderInfoLog, "glGetShaderInfoLog");
		LOAD_PROC(deleteShader, "glDeleteShader");
		LOAD_PROC(createProgram, "glCreateProgram");
		LOAD_PROC(attachShader, "glAttachShader");
		LOAD_PROC(linkProgram, "glLinkProgram");
		LOAD_PROC(getProgramiv, "glGetProgramiv");
		LOAD_PROC(getProgramInfoLog, "glGetProgramInfoLog");
		LOAD_PROC(deleteProgram, "glDeleteProgram");
		LOAD_PROC(useProgram, "glUseProgram");
		LOAD_PROC(getUniformLocation, "glGetUniformLocation");
		LOAD_PROC(uniform1i, "glUniform1i");
		LOAD_PROC(uniform2f, "glUniform2f");
		LOAD_PROC(genFramebuffers, "glGenFramebuffersEXT");
		LOAD_PROC(bindFramebuffer, "glBindFramebufferEXT");
		LOAD_PROC(deleteFramebuffers, "glDeleteFramebuffersEXT");
		LOAD_PROC(checkFramebufferStatus, "glCheckFramebufferStatusEXT");
		LOAD_PROC(framebufferTexture2D, "glFramebufferTexture2DEXT");
		LOAD_PROC(genRenderbuffers, "glGenRenderbuffersEXT");
		LOAD_PROC(bindRenderbuffer, "glBindRenderbufferEXT");
		LOAD_PROC(deleteRenderbuffers, "glDeleteRenderbuffersEXT");
		LOAD_PROC(renderbufferStorage, "glRenderbufferStorageEXT");
		LOAD_PROC(framebufferRenderbuffer, "glFramebufferRenderbufferEXT");
#undef LOAD_PROC
		return true;
	}

	GLuint compile(GLenum type, const char *source) {
		const GLuint shader = createShader(type);
		shaderSource(shader, 1, &source, 0);
		compileShader(shader);
		GLint status = 0;
		getShaderiv(shader, GL_COMPILE_STATUS, &status);
		if (!status) {
			GLint length = 0;
			getShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
			char *log = length > 0 ? new char[length] : 0;
			if (log) {
				getShaderInfoLog(shader, length, 0, log);
				fprintf(stderr, "Shader '%s' failed to compile: %s\n", shaderName, log);
				delete[] log;
			}
			deleteShader(shader);
			return 0;
		}
		return shader;
	}

	bool createProgramObject() {
		const GLuint vertex = compile(GL_VERTEX_SHADER, kVertexShader);
		const GLuint fragment = compile(GL_FRAGMENT_SHADER, fragmentShader);
		if (!vertex || !fragment) {
			if (vertex) deleteShader(vertex);
			if (fragment) deleteShader(fragment);
			return false;
		}
		program = createProgram();
		attachShader(program, vertex);
		attachShader(program, fragment);
		linkProgram(program);
		deleteShader(vertex);
		deleteShader(fragment);
		GLint status = 0;
		getProgramiv(program, GL_LINK_STATUS, &status);
		if (!status) {
			GLint length = 0;
			getProgramiv(program, GL_INFO_LOG_LENGTH, &length);
			char *log = length > 0 ? new char[length] : 0;
			if (log) {
				getProgramInfoLog(program, length, 0, log);
				fprintf(stderr, "Shader '%s' failed to link: %s\n", shaderName, log);
				delete[] log;
			}
			deleteProgram(program);
			program = 0;
			return false;
		}
		textureUniform = getUniformLocation(program, "uTexture");
		resolutionUniform = getUniformLocation(program, "uResolution");
		return textureUniform >= 0 && resolutionUniform >= 0;
	}

	void destroyTarget() {
		if (framebuffer && deleteFramebuffers) deleteFramebuffers(1, &framebuffer);
		if (depthBuffer && deleteRenderbuffers) deleteRenderbuffers(1, &depthBuffer);
		if (colorTexture) glDeleteTextures(1, &colorTexture);
		framebuffer = depthBuffer = colorTexture = 0;
		width = height = 0;
	}

	bool createTarget(int w, int h) {
		if (w <= 0 || h <= 0) return false;
		destroyTarget();
		width = w;
		height = h;
		glGenTextures(1, &colorTexture);
		glBindTexture(GL_TEXTURE_2D, colorTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
		genFramebuffers(1, &framebuffer);
		bindFramebuffer(GL_FRAMEBUFFER_EXT, framebuffer);
		framebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, colorTexture, 0);
		genRenderbuffers(1, &depthBuffer);
		bindRenderbuffer(GL_RENDERBUFFER_EXT, depthBuffer);
		renderbufferStorage(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24, width, height);
		framebufferRenderbuffer(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, depthBuffer);
		const GLenum status = checkFramebufferStatus(GL_FRAMEBUFFER_EXT);
		bindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
		bindRenderbuffer(GL_RENDERBUFFER_EXT, 0);
		if (status != GL_FRAMEBUFFER_COMPLETE_EXT) {
			fprintf(stderr, "Unable to create post-processing framebuffer (0x%x)\n", status);
			destroyTarget();
			return false;
		}
		return true;
	}
};

PostProcessor::PostProcessor(const char *shader) : _impl(new Impl(shader)) {
}

PostProcessor::~PostProcessor() {
	delete _impl;
}

bool PostProcessor::requested() const {
	return _impl->requested;
}

bool PostProcessor::active() const {
	return _impl->enabled;
}

bool PostProcessor::init(int width, int height) {
	if (!_impl->requested) return false;
	if (!_impl->loadProc()) {
		fprintf(stderr, "OpenGL shader/framebuffer support unavailable; post-processing disabled\n");
		return false;
	}
	if (!_impl->createProgramObject() || !_impl->createTarget(width, height)) {
		fprintf(stderr, "Post-processing shader '%s' could not be enabled; see the OpenGL diagnostics above\n", _impl->shaderName);
		shutdown();
		return false;
	}
	_impl->enabled = true;
	fprintf(stdout, "Post-processing shader enabled: %s\n", _impl->shaderName);
	return true;
}

bool PostProcessor::resize(int width, int height) {
	if (!_impl->enabled || (width == _impl->width && height == _impl->height)) return _impl->enabled;
	_impl->bindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
	if (!_impl->createTarget(width, height)) {
		fprintf(stderr, "Post-processing disabled after window resize\n");
		shutdown();
		return false;
	}
	return true;
}

void PostProcessor::beginFrame() {
	if (_impl->enabled) {
		_impl->bindFramebuffer(GL_FRAMEBUFFER_EXT, _impl->framebuffer);
		glViewport(0, 0, _impl->width, _impl->height);
	}
}

void PostProcessor::endFrame() {
	if (!_impl->enabled) return;
	_impl->bindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glViewport(0, 0, _impl->width, _impl->height);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	glDisable(GL_FOG);
	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_COLOR_MATERIAL);
	glDisable(GL_TEXTURE_2D);
	glClearColor(0.f, 0.f, 0.f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT);
	_impl->useProgram(_impl->program);
	_impl->uniform1i(_impl->textureUniform, 0);
	_impl->uniform2f(_impl->resolutionUniform, (GLfloat)_impl->width, (GLfloat)_impl->height);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, _impl->colorTexture);
	glColor4f(1.f, 1.f, 1.f, 1.f);
	glBegin(GL_QUADS);
		glTexCoord2f(0.f, 0.f); glVertex2f(-1.f, -1.f);
		glTexCoord2f(1.f, 0.f); glVertex2f( 1.f, -1.f);
		glTexCoord2f(1.f, 1.f); glVertex2f( 1.f,  1.f);
		glTexCoord2f(0.f, 1.f); glVertex2f(-1.f,  1.f);
	glEnd();
	_impl->useProgram(0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopAttrib();
}

void PostProcessor::shutdown() {
	if (!_impl) return;
	_impl->enabled = false;
	if (_impl->bindFramebuffer) _impl->bindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
	_impl->destroyTarget();
	if (_impl->program && _impl->deleteProgram) _impl->deleteProgram(_impl->program);
	_impl->program = 0;
}
