param([string]$OutputPath)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$build = Join-Path $repo 'build'
foreach ($item in @('f2bgl.exe', 'F2B_DATA', 'controls.cfg')) {
    $path = Join-Path $build $item
    if (-not (Test-Path -LiteralPath $path)) { throw "Required build input is missing: $path" }
}
if (-not $OutputPath) { $OutputPath = Join-Path $build 'f2bgl-Windows-Setup.exe' }
$OutputPath = [IO.Path]::GetFullPath($OutputPath)
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutputPath) | Out-Null
$csc = @(
    (Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'),
    (Join-Path $env:WINDIR 'Microsoft.NET\Framework\v4.0.30319\csc.exe')
) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $csc) { throw 'The .NET Framework C# compiler was not found.' }
$temp = Join-Path ([IO.Path]::GetTempPath()) ('f2bgl-setup-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $temp | Out-Null
try {
    $payload = Join-Path $temp 'payload'
    New-Item -ItemType Directory -Force -Path $payload | Out-Null
    Copy-Item (Join-Path $build 'f2bgl.exe') $payload
    Copy-Item (Join-Path $repo 'controls.cfg') $payload
    Copy-Item (Join-Path $build 'F2B_DATA') (Join-Path $payload 'F2B_DATA') -Recurse
    Get-ChildItem $build -Filter '*.dll' -File | Copy-Item -Destination $payload
    $readme = @"
f2bgl for Windows 11

This installer contains the Windows SDL3 engine, runtime libraries, and game data supplied by the person who built it. It installs for the current Windows user and creates a Start menu shortcut plus an optional desktop shortcut. Saves and screenshots are stored in %LOCALAPPDATA%\f2bgl\saves.

The original game data is user-provided. Do not redistribute this installer or game data unless you have the rights to do so.
"@
    Set-Content (Join-Path $payload 'README-WINDOWS.txt') $readme -Encoding UTF8
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = Join-Path $temp 'payload.zip'
    [IO.Compression.ZipFile]::CreateFromDirectory($payload, $zip, [IO.Compression.CompressionLevel]::Optimal, $false)
    $stub = Join-Path $temp 'F2BGLSetup.exe'
    $compilerArgs = @('/nologo','/target:winexe','/platform:anycpu',"/out:$stub",'/reference:System.Windows.Forms.dll','/reference:System.Drawing.dll','/reference:System.IO.Compression.dll','/reference:System.IO.Compression.FileSystem.dll',(Join-Path $PSScriptRoot 'F2BGLSetup.cs'))
    $proc = Start-Process -FilePath $csc -ArgumentList $compilerArgs -Wait -PassThru -NoNewWindow
    if ($proc.ExitCode -ne 0) { throw "Setup compiler failed with exit code $($proc.ExitCode)." }
    $out = [IO.File]::Create($OutputPath)
    try {
        foreach ($file in @($stub, $zip)) {
            $input = [IO.File]::OpenRead($file)
            try { $input.CopyTo($out) } finally { $input.Dispose() }
        }
        $writer = New-Object IO.BinaryWriter($out, [Text.Encoding]::ASCII, $true)
        $writer.Write([int64](Get-Item $zip).Length)
        $writer.Write([Text.Encoding]::ASCII.GetBytes('F2BGLPKG'))
        $writer.Flush()
        $writer.Dispose()
    } finally { $out.Dispose() }
    Write-Output "Created $OutputPath"
} finally {
    Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue
}
