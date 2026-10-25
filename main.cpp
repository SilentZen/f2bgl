/*
 * Fade To Black engine rewrite
 * Copyright (C) 2006-2012 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "stub.h"

const char *g_caption = "Fade2Black/OpenGL";

static const char *kIconBmp = "icon.bmp";

static float gAspectRatio;

static const int kDefaultW = 640;
static const int kDefaultH = 480;

static int gWindowW = kDefaultW;
static int gWindowH = kDefaultH;

static float _aspectRatio[4];

static int gScale = 2;
static int gSaveSlot = 1;

static const int kTickDuration = 50;

static const char *kControlsCfg = "controls.cfg";

static const int kJoystickDefaultIndex = 0;
static const int kJoystickCommitValue = 16384;

static const int kJoystickMapSize = 8;
static int gJoystickMap[kJoystickMapSize];

static const int kGamepadMapSize = SDL_GAMEPAD_BUTTON_COUNT;
static int gGamepadMap[kGamepadMapSize];

static const int gKeyScancodeMapSize = 512;
static int gKeyScancodeMap[gKeyScancodeMapSize];

static int readKeyMap(FILE *fp) {
	static const char *kKeyboardScancode = "keyboard.scancode.";
	static const char *kGamepadButton = "gamepad.button.";
	static const char *kJoystickButton = "joystick.button.";
	static const struct {
		const char *keyword;
		int keyMapping;
	} mappings[] = {
		{ "gun_mode", kKeyCodeAlt },
		{ "shoot", kKeyCodeCtrl },
		{ "walk", kKeyCodeShift },
		{ "activate", kKeyCodeSpace },
		{ "reload_gun", kKeyCodeReturn },
		{ "change_inventory", kKeyCodeTab },
		{ "escape", kKeyCodeEscape },
		{ "inventory", kKeyCodeI },
		{ "jump", kKeyCodeJ },
		{ "use", kKeyCodeU },
		{ "item1", kKeyCode1 },
		{ "item2", kKeyCode2 },
		{ "item3", kKeyCode3 },
		{ "item4", kKeyCode4 },
		{ "item5", kKeyCode5 },
		{ "foot_step", kKeyCodePageUp },
		{ "back_step", kKeyCodePageDown },
		{ 0, -1 }
	};
	static const struct {
		const char *name;
		int code;
	} buttons[] = {
		{ "back", SDL_GAMEPAD_BUTTON_BACK },
		{ "guide", SDL_GAMEPAD_BUTTON_GUIDE },
		{ "start", SDL_GAMEPAD_BUTTON_START },
		{ "leftstick", SDL_GAMEPAD_BUTTON_LEFT_STICK },
		{ "leftshoulder", SDL_GAMEPAD_BUTTON_LEFT_SHOULDER },
		{ "rightstick", SDL_GAMEPAD_BUTTON_RIGHT_STICK },
		{ "rightshoulder", SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER },
		{ "a", SDL_GAMEPAD_BUTTON_SOUTH },
		{ "b", SDL_GAMEPAD_BUTTON_EAST },
		{ "x", SDL_GAMEPAD_BUTTON_WEST },
		{ "y", SDL_GAMEPAD_BUTTON_NORTH },
		{ 0, -1 }
	};
	int lineNumber = 0;
	int mappingCount = 0;
	char buf[256];
	while (fgets(buf, sizeof(buf), fp)) {
		++lineNumber;
		if (buf[0] == '#') {
			continue;
		}
		const char *p = strchr(buf, '=');
		if (p) {
			++p;
			while (*p && isspace(*p)) {
				++p;
			}
			int keyMapping = -1;
			if (*p) {
				for (int i = 0; mappings[i].keyword; ++i) {
					if (strncmp(p, mappings[i].keyword, strlen(mappings[i].keyword)) == 0) {
						keyMapping = mappings[i].keyMapping;
						break;
					}
				}
			}
			if (keyMapping == -1) {
				fprintf(stderr, "Unknown key mapping '%s' (line %d)\n", p, lineNumber);
				continue;
			}
			if (strncmp(buf, kKeyboardScancode, strlen(kKeyboardScancode)) == 0) {
				int scancode = atoi(buf + strlen(kKeyboardScancode));
				if (scancode >= 0 && scancode < gKeyScancodeMapSize) {
					gKeyScancodeMap[scancode] = keyMapping;
					++mappingCount;
				} else {
					fprintf(stderr, "Unhandled scancode %d (line %d)\n", scancode, lineNumber);
				}
			} else if (strncmp(buf, kGamepadButton, strlen(kGamepadButton)) == 0) {
				const char *name = buf + strlen(kGamepadButton);
				for (int i = 0; buttons[i].name; ++i) {
					if (strncmp(name, buttons[i].name, strlen(buttons[i].name)) == 0) {
						int button = buttons[i].code;
						if (button >= 0 && button < kGamepadMapSize) {
							gGamepadMap[button] = keyMapping;
							++mappingCount;
						} else {
							fprintf(stderr, "Unhandled gamepad button %d (line %d)\n", button, lineNumber);
						}
						break;
					}
				}
			} else if (strncmp(buf, kJoystickButton, strlen(kJoystickButton)) == 0) {
				int button = atoi(buf + strlen(kJoystickButton));
				if (button >= 0 && button < kJoystickMapSize) {
					gJoystickMap[button] = keyMapping;
					++mappingCount;
				} else {
					fprintf(stderr, "Unhandled joystick button %d (line %d)\n", button, lineNumber);
				}
			}
		}
	}
	return mappingCount;
}

static void setupKeyMap() {
	// keyboard
	gKeyScancodeMap[SDL_SCANCODE_LEFT]   = kKeyCodeLeft;
	gKeyScancodeMap[SDL_SCANCODE_RIGHT]  = kKeyCodeRight;
	gKeyScancodeMap[SDL_SCANCODE_UP]     = kKeyCodeUp;
	gKeyScancodeMap[SDL_SCANCODE_DOWN]   = kKeyCodeDown;
	// gamepad
	gGamepadMap[SDL_GAMEPAD_BUTTON_DPAD_UP]    = kKeyCodeUp;
	gGamepadMap[SDL_GAMEPAD_BUTTON_DPAD_DOWN]  = kKeyCodeDown;
	gGamepadMap[SDL_GAMEPAD_BUTTON_DPAD_LEFT]  = kKeyCodeLeft;
	gGamepadMap[SDL_GAMEPAD_BUTTON_DPAD_RIGHT] = kKeyCodeRight;

	// read controls mapping from file
	FILE *fp = fopen(kControlsCfg, "rb");
	if (fp) {
		const int mappingCount = readKeyMap(fp);
		fclose(fp);
		// use built-in mapping if no keys have been mapped
		if (mappingCount != 0) {
			return;
		}
	}
	// keyboard
	gKeyScancodeMap[SDL_SCANCODE_LALT]   = kKeyCodeAlt;
	gKeyScancodeMap[SDL_SCANCODE_RALT]   = kKeyCodeAlt;
	gKeyScancodeMap[SDL_SCANCODE_LSHIFT] = kKeyCodeShift;
	gKeyScancodeMap[SDL_SCANCODE_RSHIFT] = kKeyCodeShift;
	gKeyScancodeMap[SDL_SCANCODE_LCTRL]  = kKeyCodeCtrl;
	gKeyScancodeMap[SDL_SCANCODE_RCTRL]  = kKeyCodeCtrl;
	gKeyScancodeMap[SDL_SCANCODE_SPACE]  = kKeyCodeSpace;
	gKeyScancodeMap[SDL_SCANCODE_RETURN] = kKeyCodeReturn;
	gKeyScancodeMap[SDL_SCANCODE_TAB]    = kKeyCodeTab;
	gKeyScancodeMap[SDL_SCANCODE_ESCAPE] = kKeyCodeEscape;
	gKeyScancodeMap[SDL_SCANCODE_I]      = kKeyCodeI;
	gKeyScancodeMap[SDL_SCANCODE_J]      = kKeyCodeJ;
	gKeyScancodeMap[SDL_SCANCODE_U]      = kKeyCodeU;
	gKeyScancodeMap[SDL_SCANCODE_V]      = kKeyCodeAlt;
	gKeyScancodeMap[SDL_SCANCODE_B]      = kKeyCodeCtrl;
	gKeyScancodeMap[SDL_SCANCODE_H]      = kKeyCodeFarNear;
	gKeyScancodeMap[SDL_SCANCODE_K]      = kKeyCodeFarNear;
	gKeyScancodeMap[SDL_SCANCODE_PAGEUP]   = kKeyCodePageUp;
	gKeyScancodeMap[SDL_SCANCODE_PAGEDOWN] = kKeyCodePageDown;
	for (int i = 0; i < 5; ++i) {
		gKeyScancodeMap[SDL_SCANCODE_1 + i] = kKeyCode1 + i;
	}
	// joystick buttons
	gJoystickMap[0] = kKeyCodeAlt;
	gJoystickMap[1] = kKeyCodeShift;
	gJoystickMap[2] = kKeyCodeCtrl;
	gJoystickMap[3] = kKeyCodeSpace;
	gJoystickMap[4] = kKeyCodeI;
	gJoystickMap[5] = kKeyCodeJ;
	gJoystickMap[6] = kKeyCodeU;
	gJoystickMap[7] = kKeyCodeReturn;
	// gamecontroller buttons
	gGamepadMap[SDL_GAMEPAD_BUTTON_SOUTH] = kKeyCodeAlt;
	gGamepadMap[SDL_GAMEPAD_BUTTON_EAST] = kKeyCodeSpace;
	gGamepadMap[SDL_GAMEPAD_BUTTON_WEST] = kKeyCodeCtrl;
	gGamepadMap[SDL_GAMEPAD_BUTTON_NORTH] = kKeyCodeU;
	gGamepadMap[SDL_GAMEPAD_BUTTON_BACK]  = kKeyCodeEscape;
	gGamepadMap[SDL_GAMEPAD_BUTTON_GUIDE] = kKeyCodeI;
	gGamepadMap[SDL_GAMEPAD_BUTTON_START] = kKeyCodeI;
	gGamepadMap[SDL_GAMEPAD_BUTTON_LEFT_STICK]     = kKeyCodeJ;
	gGamepadMap[SDL_GAMEPAD_BUTTON_LEFT_SHOULDER]  = kKeyCodeJ;
	gGamepadMap[SDL_GAMEPAD_BUTTON_RIGHT_STICK]    = kKeyCodeShift;
	gGamepadMap[SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER] = kKeyCodeShift;
	gGamepadMap[SDL_GAMEPAD_BUTTON_DPAD_UP]    = kKeyCodeUp;
	gGamepadMap[SDL_GAMEPAD_BUTTON_DPAD_DOWN]  = kKeyCodeDown;
	gGamepadMap[SDL_GAMEPAD_BUTTON_DPAD_LEFT]  = kKeyCodeLeft;
	gGamepadMap[SDL_GAMEPAD_BUTTON_DPAD_RIGHT] = kKeyCodeRight;
}

static SDL_AudioStream *gAudioStream;
static StubMixProc gMix;

static void audioStreamCallback(void *userdata, SDL_AudioStream *stream, int additionalAmount, int totalAmount) {
	StubMixProc *mix = (StubMixProc *)userdata;
	const int amount = additionalAmount;
	if (!mix || !mix->proc || amount <= 0) {
		return;
	}
	uint8_t *buffer = (uint8_t *)SDL_malloc((size_t)amount);
	if (buffer) {
		mix->proc(mix->data, buffer, amount);
		SDL_PutAudioStreamData(stream, buffer, amount);
		SDL_free(buffer);
	}
}

static void lockAudio(int lock) {
	if (gAudioStream) {
		if (lock) SDL_LockAudioStream(gAudioStream);
		else SDL_UnlockAudioStream(gAudioStream);
	}
}

static void setupAudio(GameStub *stub) {
	SDL_AudioSpec desired;
	SDL_zero(desired);
	desired.freq = 22050;
	desired.format = SDL_AUDIO_S16;
	desired.channels = 2;
	gMix = stub->getMixProc(desired.freq, desired.format, lockAudio);
	if (gMix.proc) {
		gAudioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired, audioStreamCallback, &gMix);
		if (gAudioStream) SDL_ResumeAudioStreamDevice(gAudioStream);
	}
}
static void setAspectRatio(int w, int h) {
	const float currentAspectRatio = w / (float)h;
	// pillar box
	if (currentAspectRatio > gAspectRatio) {
		const float inset = 1. - gAspectRatio / currentAspectRatio;
		_aspectRatio[0] = inset / 2;
		_aspectRatio[1] = 0.;
		_aspectRatio[2] = 1. - inset;
		_aspectRatio[3] = 1.;
		return;
	}
	// letter box
	if (currentAspectRatio < gAspectRatio) {
		const float inset = 1. - currentAspectRatio / gAspectRatio;
		_aspectRatio[0] = 0.;
		_aspectRatio[1] = inset / 2;
		_aspectRatio[2] = 1.;
		_aspectRatio[3] = 1. - inset;
		return;
	}
}

static int transformPointerX(int x) {
	return int((x - _aspectRatio[0] * gWindowW) * 320 / (_aspectRatio[2] * gWindowW));
}

static int transformPointerY(int y) {
	return int((y - _aspectRatio[1] * gWindowH) * 200 / (_aspectRatio[3] * gWindowH));
}

int main(int argc, char *argv[]) {
	SDL_SetMainReady();
	GameStub *stub = GameStub_create();
	if (!stub) {
		return -1;
	}
	int ret = stub->setArgs(argc, argv);
	if (ret != 0) {
		return ret;
	}
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD | SDL_INIT_HAPTIC)) {
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return -1;
	}
	if (stub->hasCursor()) SDL_ShowCursor(); else SDL_HideCursor();
	bool widescreen = false;

	const int displayMode = stub->getDisplayMode();
	gAspectRatio = stub->getAspectRatio(widescreen);
	gWindowH = gWindowW / gAspectRatio;
	int flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
	if (displayMode != kDisplayModeWindow) {
		flags |= SDL_WINDOW_FULLSCREEN;
	}
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_Window *window = SDL_CreateWindow(g_caption, gWindowW, gWindowH, (SDL_WindowFlags)flags);
	if (!window) {
		return -1;
	}
	SDL_Surface *icon = SDL_LoadBMP(kIconBmp);
	if (icon) {
		SDL_SetWindowIcon(window, icon);
		SDL_DestroySurface(icon);
	}
	SDL_GetWindowSize(window, &gWindowW, &gWindowH);
	_aspectRatio[0] = 0.;
	_aspectRatio[1] = 0.;
	_aspectRatio[2] = 1.;
	_aspectRatio[3] = 1.;
	setAspectRatio(gWindowW, gWindowH);
	SDL_GLContext glcontext = SDL_GL_CreateContext(window);
	SDL_GL_SetSwapInterval(1);
	ret = stub->init();
	if (ret != 0) {
		return ret;
	}
	setupKeyMap();
	setupAudio(stub);
	SDL_Joystick *joystick = 0;
	SDL_JoystickID joystickID = 0;
	SDL_Gamepad *controller = 0;
	SDL_Haptic *haptic = 0;
	int joystickCount = 0;
	SDL_JoystickID *joystickIDs = SDL_GetJoysticks(&joystickCount);
	if (joystickCount > 0 && joystickIDs) {
		joystickID = joystickIDs[kJoystickDefaultIndex];
		SDL_AddGamepadMappingsFromFile("gamecontrollerdb.txt");
		if (SDL_IsGamepad(joystickID)) {
			controller = SDL_OpenGamepad(joystickID);
			if (controller) {
				fprintf(stdout, "Using controller '%s'\n", SDL_GetGamepadName(controller));

			}
		}
		if (!controller) {
			// no controller found, look for joystick
			joystick = SDL_OpenJoystick(joystickID);
			if (joystick) {
				fprintf(stdout, "Using joystick '%s'\n", SDL_GetJoystickName(joystick));
				haptic = SDL_OpenHapticFromJoystick(joystick);
			}
		}
		if (haptic) {
			if (!SDL_HapticRumbleSupported(haptic)) {
				// not supported, release the resource now
				SDL_CloseHaptic(haptic);
				haptic = 0;
			}
		}
	}
	stub->initGL(gWindowW, gWindowH, _aspectRatio);
	bool quitGame = false;
	bool paused = false;
	while (1) {
		int w = gWindowW;
		int h = gWindowH;
		SDL_Event ev;
		const uint32_t nextTick = SDL_GetTicks() + kTickDuration;
		while (SDL_PollEvent(&ev)) {
			switch (ev.type) {
			case SDL_EVENT_QUIT:
				quitGame = true;
				break;
			case SDL_EVENT_WINDOW_RESIZED:
				w = ev.window.data1;
				h = ev.window.data2;
				break;
			case SDL_EVENT_WINDOW_FOCUS_GAINED:
			case SDL_EVENT_WINDOW_FOCUS_LOST:
				paused = (ev.type == SDL_EVENT_WINDOW_FOCUS_LOST);
				if (gAudioStream) {
					if (paused) SDL_PauseAudioStreamDevice(gAudioStream);
					else SDL_ResumeAudioStreamDevice(gAudioStream);
				}
				break;
			case SDL_EVENT_KEY_UP:
				if (gKeyScancodeMap[ev.key.scancode] != 0) {
					stub->queueKeyInput(gKeyScancodeMap[ev.key.scancode], 0);
				} else {
					switch (ev.key.key) {
					case SDLK_PAGEUP:
						if (gScale < 8) {
							++gScale;
							w = (int)(kDefaultW * gScale * .5);
							h = (int)(kDefaultH * gScale * .5);
						}
						break;
					case SDLK_PAGEDOWN:
						if (gScale > 1) {
							--gScale;
							w = (int)(kDefaultW * gScale * .5);
							h = (int)(kDefaultH * gScale * .5);
						}
						break;
					case SDLK_EXCLAIM:
						stub->queueKeyInput(kKeyCodeCheatLifeCounter, 1);
						break;
					case SDLK_S:
						stub->saveState(gSaveSlot);
						break;
					case SDLK_L:
						stub->loadState(gSaveSlot);
						break;
					case SDLK_T:
						stub->takeScreenshot();
						break;
					case SDLK_P:
					case SDLK_PLUS:
					case SDLK_KP_PLUS:
						if (gSaveSlot < 99) {
							++gSaveSlot;
						}
						break;
					case SDLK_M:
					case SDLK_MINUS:
					case SDLK_KP_MINUS:
						if (gSaveSlot > 1) {
							--gSaveSlot;
						}
						break;
					case SDLK_Q:
						quitGame = true;
						break;
					case SDLK_F1:
						stub->queueKeyInput(kKeyCodeToggleFog, 1);
						break;
					case SDLK_F2:
						stub->queueKeyInput(kKeyCodeToggleGouraudShading, 1);
						break;
					}
				}
				break;
			case SDL_EVENT_KEY_DOWN:
				if (gKeyScancodeMap[ev.key.scancode] != 0) {
					stub->queueKeyInput(gKeyScancodeMap[ev.key.scancode], 1);
				}
				break;
			case SDL_EVENT_JOYSTICK_HAT_MOTION:
				if (joystick) {
					stub->queueKeyInput(kKeyCodeUp,    (ev.jhat.value & SDL_HAT_UP)    != 0);
					stub->queueKeyInput(kKeyCodeDown,  (ev.jhat.value & SDL_HAT_DOWN)  != 0);
					stub->queueKeyInput(kKeyCodeLeft,  (ev.jhat.value & SDL_HAT_LEFT)  != 0);
					stub->queueKeyInput(kKeyCodeRight, (ev.jhat.value & SDL_HAT_RIGHT) != 0);
				}
				break;
			case SDL_EVENT_JOYSTICK_AXIS_MOTION:
				if (joystick) {
					switch (ev.jaxis.axis) {
					case 0:
						stub->queueKeyInput(kKeyCodeLeft,  (ev.jaxis.value < -kJoystickCommitValue));
						stub->queueKeyInput(kKeyCodeRight, (ev.jaxis.value >  kJoystickCommitValue));
						break;
					case 1:
						stub->queueKeyInput(kKeyCodeUp,   (ev.jaxis.value < -kJoystickCommitValue));
						stub->queueKeyInput(kKeyCodeDown, (ev.jaxis.value >  kJoystickCommitValue));
						break;
					}
				}
				break;
			case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
			case SDL_EVENT_JOYSTICK_BUTTON_UP:
				if (joystick) {
					if (ev.jbutton.button < kJoystickMapSize) {
						if (gJoystickMap[ev.jbutton.button] != 0) {
							stub->queueKeyInput(gJoystickMap[ev.jbutton.button], ev.jbutton.down);
						}
					}
				}
				break;
			case SDL_EVENT_GAMEPAD_AXIS_MOTION:
				if (controller) {
					switch (ev.gaxis.axis) {
					case SDL_GAMEPAD_AXIS_LEFTX:
					case SDL_GAMEPAD_AXIS_RIGHTX:
						stub->queueKeyInput(kKeyCodeLeft,  (ev.gaxis.value < -kJoystickCommitValue));
						stub->queueKeyInput(kKeyCodeRight, (ev.gaxis.value >  kJoystickCommitValue));
						break;
					case SDL_GAMEPAD_AXIS_LEFTY:
					case SDL_GAMEPAD_AXIS_RIGHTY:
						stub->queueKeyInput(kKeyCodeUp,   (ev.gaxis.value < -kJoystickCommitValue));
						stub->queueKeyInput(kKeyCodeDown, (ev.gaxis.value >  kJoystickCommitValue));
						break;
					}
				}
				break;
			case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
			case SDL_EVENT_GAMEPAD_BUTTON_UP:
				if (controller && ev.gbutton.button < kGamepadMapSize) {
					if (gGamepadMap[ev.gbutton.button] != 0) {
						stub->queueKeyInput(gGamepadMap[ev.gbutton.button], ev.gbutton.down);
					}
				}
				break;
			case SDL_EVENT_MOUSE_MOTION:
				if (ev.motion.state & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) {
					stub->queueTouchInput(0, transformPointerX(ev.motion.x), transformPointerY(ev.motion.y), 1);
				}
				if (ev.motion.state & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT)) {
					stub->queueTouchInput(1, transformPointerX(ev.motion.x), transformPointerY(ev.motion.y), 1);
				}
				break;
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			case SDL_EVENT_MOUSE_BUTTON_UP:
				switch (ev.button.button) {
				case SDL_BUTTON_LEFT:
					stub->queueTouchInput(0, transformPointerX(ev.button.x), transformPointerY(ev.button.y), ev.button.down);
					break;
				case SDL_BUTTON_RIGHT:
					stub->queueTouchInput(1, transformPointerX(ev.button.x), transformPointerY(ev.button.y), ev.button.down);
					break;
				}
				break;
			default:
				break;
			}
		}
		if (quitGame) {
			break;
		}
		if (w != gWindowW || h != gWindowH) {
			gWindowW = w;
			gWindowH = h;
			SDL_SetWindowSize(window, gWindowW, gWindowH);
			stub->initGL(gWindowW, gWindowH, _aspectRatio);
		}
		if (!paused) {
			const unsigned int ticks = SDL_GetTicks();
			stub->doTick(ticks);
			stub->drawGL();
			SDL_GL_SwapWindow(window);
			if (stub->shouldVibrate()) {
				if (controller) {
					SDL_RumbleGamepad(controller, 0xFFFF, 0xFFFF, 500);
				} else if (haptic) {
					SDL_InitHapticRumble(haptic);
					SDL_PlayHapticRumble(haptic, 1., 500);
				}
			}
			const int delayTick = nextTick - SDL_GetTicks();
			SDL_Delay(delayTick < 16 ? 16 : delayTick);
		} else {
			SDL_Delay(kTickDuration);
		}
	}
	if (gAudioStream) SDL_PauseAudioStreamDevice(gAudioStream);
	stub->quit();
	SDL_GL_DestroyContext(glcontext);
	SDL_DestroyWindow(window);
	if (controller) {
		SDL_CloseGamepad(controller);
	}
	if (joystick) {
		SDL_CloseJoystick(joystick);
	}
	if (haptic) {
		SDL_CloseHaptic(haptic);
	}
	SDL_free(joystickIDs);
	if (gAudioStream) SDL_DestroyAudioStream(gAudioStream);
	SDL_Quit();
	return 0;
}
