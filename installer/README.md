# Windows installer

`F2BGLSetup.cs` is a small per-user installer for the Windows SDL3 build. The setup program extracts an archive appended to its executable, installs the game under the selected folder, creates shortcuts, stores saves under `%LOCALAPPDATA%\f2bgl\saves`, and registers a per-user uninstall entry.

To build it on Windows, first build `f2bgl.exe` and place the user's game files in `build/F2B_DATA`. Then run:

```powershell
.\installer\build-installer.ps1
```

The script writes `build/f2bgl-Windows-Setup.exe`. Pass `-OutputPath` to choose another destination. It packages DLLs from `build`, `controls.cfg`, and `F2B_DATA`; it does not package build objects or save files.

Game data is user-supplied. The source-only repository does not contain the original game files, and the generated installer should only be shared where the distributor has rights to the included game data.
