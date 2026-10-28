
f2bgl README
Release version: 0.5.0
-------------------------------------------------------------------------------

0.5.0 highlights:
- Added a standalone Windows launcher with themed options and keyboard remapping.
- Added window resolution selection, Alt+Enter fullscreen toggling, and post-processing shaders.
- Added supported PlayStation cutscene data handling and improved data-file lookup.
- Refreshed the launcher artwork and multi-size Windows icon.
- Packaged the launcher, engine, and runtime libraries into a portable executable.


About:
------

f2bgl is a re-implementation of the engine used in the game Fade To Black
made by Delphine Software and released in 1995.


Running:
--------

You will need the original files of the PC release or PC demo.

By default, the program will try to load the game data files from the current
directory. The expected directory structure is :

	DELPHINE.INI
	TRIGO.DAT    - optional
	DATA/
	DATA/DRIVERS/
	DATA/SOUND/
	INSTDATA/    - optional
	TEXT/        - not present with demo version
	VOICE/       - not present with demo version

Defaults can be changed using command line switches :

    Usage: f2b [OPTIONS]...
  /? or --help                 Show this list of options
    --datapath=PATH             Path to data files (default '.')
    --language=EN|FR|GR|SP|IT   Language files to use (default 'EN')
    --playdemo                  Use inputs from .DEM files
    --level=NUM                 Start at level NUM
    --voice=EN|FR|GR            Voice files (default 'EN')
    --subtitles                 Display cutscene subtitles
    --savepath=PATH             Path to save files (default '.')
    --fullscreen                Fullscreen display (stretched)
    --resolution=WIDTHxHEIGHT   Window size (default 640x480); Alt+Enter toggles fullscreen
    --fov=DEG                   Field of vision in degrees (75-130)
    --soundfont=FILE            SoundFont (.sf2) file for music
    --texturefilter=FILTER      Texture filter (default 'linear')
    --texturescaler=NAME        Texture scaler (default 'scale2x')
    --shader=NAME               Post-processing shader: none, crt-easymode, crt-geom, lcd-grid
    --mouse                     Enable mouse controls
    --no-fog                    Disable fog rendering
    --no-gouraud                Disable gouraud shading
    --psxpath=PATH              Path to PSX data files

GUI launcher:
-------------

Run "python launcher.py" to open the optional GUI launcher. The main window keeps the
Launch game and keyboard controls buttons visible; the themed Options window contains
file paths and gameplay, display, and rendering settings. It lets you browse for the
f2bgl executable, choose the required PC game data and optional PlayStation data
for supported cutscenes and supplemental files, plus a save folder, and select a
SoundFont. It remembers the last-used settings in your user configuration folder.
The modern dark launcher uses 'assets/launcher-banner.png' and
'assets/launcher-icon.png' for its rounded header artwork and app icon. The Windows
icon is available in multiple sizes in 'assets/launcher.ico'.
Python 3 with Tkinter is required; the game itself remains a separate executable.
In the self-contained Windows package, double-clicking 'f2bgl.exe' opens the launcher;
passing command-line options to that executable starts the bundled game directly.
Use the "Configure keyboard controls…" button on the main launcher to remap keyboard
controls: select "Scan key…", press the desired key, then save. The launcher writes the
mappings to controls.cfg beside the selected game executable. Windows paths are normalized
to backslashes when settings are saved or used.

Post-processing shaders:
------------------------

The launcher and command line can select CRT EasyMode, CRT Geom, or LCD Grid effects.
CRT EasyMode adds scanlines and an RGB aperture mask; CRT Geom adds screen curvature and
vignetting; LCD Grid adds a subpixel and row pattern. The normal rendering path remains
the default when no shader is selected. The CRT designs are original lightweight
implementations inspired by the styles in the libretro GLSL shader collection.

Shader references:
CRT EasyMode by EasyMode: https://github.com/libretro/glsl-shaders/blob/master/crt/shaders/crt-easymode.glsl
CRT-Geom by cgwg, Themaister, and DOLLS: https://github.com/libretro/glsl-shaders/blob/master/crt/shaders/crt-geom.glsl
These are effect references only; the standalone shaders in f2bgl are independent implementations.


Controls:
---------

The game can be played using a joystick, a gamepad or the keyboard.
Key mappings can be changed in the launcher's Controls menu or by editing 'controls.cfg'.

In-game keys (default) :

    Arrow Keys     move Conrad
    Alt / V        toggle gun mode
    Shift          walk (or steps)
    Ctrl / B       shoot
    Enter          reload gun
    Space          activate (or shoot)
    Tab            change inventory category
    Escape         save/load menu
    I              inventory menu
    J              jump
    U              use current item
    1 .. 5         use item #
    T              take screenshot
    S              save game state
    L              load game state
    + and -        change game state save slot
    F1             toggle fog on/off
    F2             toggle flat/gouraud shading


Credits:
--------

Delphine Software for creating the game.


Contact:
--------

Gregory Montoir, cyx@users.sourceforge.net
