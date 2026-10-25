using System;
using System.IO;
using System.IO.Compression;
using System.Windows.Forms;
using System.Drawing;
using Microsoft.Win32;
using System.Runtime.InteropServices;

internal sealed class SetupForm : Form
{
    TextBox folder; CheckBox desktop; Button install; Label status; ProgressBar progress;
    const string AppName = "f2bgl";
    public SetupForm() {
        Text = "Install f2bgl"; Width = 520; Height = 235; FormBorderStyle = FormBorderStyle.FixedDialog;
        MaximizeBox = false; MinimizeBox = false; StartPosition = FormStartPosition.CenterScreen;
        var title = new Label { Left=18, Top=15, Width=470, Height=25, Text="Install f2bgl for Windows", Font=new Font("Segoe UI", 12, FontStyle.Bold) };
        Controls.Add(title);
        Controls.Add(new Label { Left=20, Top=55, Width=470, Text="Install location:" });
        folder = new TextBox { Left=20, Top=78, Width=365, Text=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Programs", "f2bgl") };
        var browse = new Button { Left=395, Top=76, Width=90, Text="Browse..." };
        browse.Click += (s,e) => { using (var d = new FolderBrowserDialog()) { d.SelectedPath=folder.Text; if (d.ShowDialog(this)==DialogResult.OK) folder.Text=d.SelectedPath; } };
        Controls.Add(folder); Controls.Add(browse);
        desktop = new CheckBox { Left=20, Top=111, Width=250, Text="Create a desktop shortcut", Checked=true }; Controls.Add(desktop);
        progress = new ProgressBar { Left=20, Top=143, Width=465, Height=17, Visible=false }; Controls.Add(progress);
        status = new Label { Left=20, Top=165, Width=330, Text="Game data is included in this installer." }; Controls.Add(status);
        install = new Button { Left=385, Top=158, Width=100, Height=30, Text="Install" }; install.Click += InstallClick; Controls.Add(install);
        AcceptButton=install;
    }
    void InstallClick(object sender, EventArgs e) {
        string target=Path.GetFullPath(folder.Text.Trim());
        if (String.IsNullOrWhiteSpace(folder.Text)) { MessageBox.Show(this,"Choose an install location.",Text,MessageBoxButtons.OK,MessageBoxIcon.Warning); return; }
        if (Directory.Exists(target) && Directory.GetFiles(target).Length>0 && MessageBox.Show(this,"Files already exist in this folder. Continue and replace matching application files?","Confirm install",MessageBoxButtons.YesNo,MessageBoxIcon.Question)!=DialogResult.Yes) return;
        install.Enabled=false; folder.Enabled=false; desktop.Enabled=false; progress.Visible=true;
        try {
            Directory.CreateDirectory(target);
            using (var exe=File.OpenRead(Application.ExecutablePath)) {
                long size=exe.Length; exe.Position=size-16; byte[] footer=new byte[16]; exe.Read(footer,0,16);
                string magic=System.Text.Encoding.ASCII.GetString(footer,8,8);
                long zipLength=BitConverter.ToInt64(footer,0);
                if (magic!="F2BGLPKG" || zipLength<=0 || zipLength>size-16) throw new InvalidDataException("Installer payload is missing or damaged.");
                using (var view=new SliceStream(exe,size-16-zipLength,zipLength))
                using (var zip=new ZipArchive(view,ZipArchiveMode.Read,false)) {
                    progress.Maximum=Math.Max(1,zip.Entries.Count); int n=0;
                    foreach (var entry in zip.Entries) {
                        string dest=Path.GetFullPath(Path.Combine(target,entry.FullName));
                        if (!dest.StartsWith(target.TrimEnd(Path.DirectorySeparatorChar)+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase) && dest!=target) throw new InvalidDataException("Invalid installer file path.");
                        if (entry.FullName.EndsWith("/",StringComparison.Ordinal) || entry.FullName.EndsWith("\\",StringComparison.Ordinal)) Directory.CreateDirectory(dest);
                        else { Directory.CreateDirectory(Path.GetDirectoryName(dest)); using(var input=entry.Open()) using(var output=File.Create(dest)) input.CopyTo(output); }
                        progress.Value=Math.Min(++n,progress.Maximum); if ((n%20)==0) Application.DoEvents();
                    }
                }
            }
            string saves=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"f2bgl","saves"); Directory.CreateDirectory(saves);
            string startMenu=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),"Microsoft","Windows","Start Menu","Programs","f2bgl"); Directory.CreateDirectory(startMenu);
            MakeShortcut(Path.Combine(startMenu,"f2bgl.lnk"),Path.Combine(target,"f2bgl.exe"),"--datapath=\""+Path.Combine(target,"F2B_DATA")+"\" --savepath=\""+saves+"\"");
            if (desktop.Checked) MakeShortcut(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory),"f2bgl.lnk"),Path.Combine(target,"f2bgl.exe"),"--datapath=\""+Path.Combine(target,"F2B_DATA")+"\" --savepath=\""+saves+"\"");
            WriteUninstaller(target);
            using (var key=Registry.CurrentUser.CreateSubKey(@"Software\Microsoft\Windows\CurrentVersion\Uninstall\f2bgl")) {
                key.SetValue("DisplayName","f2bgl"); key.SetValue("DisplayVersion","Windows SDL3 build"); key.SetValue("Publisher","f2bgl contributors"); key.SetValue("InstallLocation",target);
                key.SetValue("UninstallString","powershell.exe -NoProfile -ExecutionPolicy Bypass -File \""+Path.Combine(target,"uninstall.ps1")+"\""); key.SetValue("NoModify",1,RegistryValueKind.DWord); key.SetValue("NoRepair",1,RegistryValueKind.DWord);
            }
            status.Text="Installation complete."; install.Text="Finish"; install.Enabled=true; install.Click -= InstallClick; install.Click += (s2,e2)=>Close();
            MessageBox.Show(this,"f2bgl is installed. Saves will be stored in your local AppData folder.",Text,MessageBoxButtons.OK,MessageBoxIcon.Information);
        } catch(Exception ex) { MessageBox.Show(this,"Installation failed:\r\n"+ex.Message,Text,MessageBoxButtons.OK,MessageBoxIcon.Error); install.Enabled=true; folder.Enabled=true; desktop.Enabled=true; status.Text="Installation failed."; }
    }
    static void MakeShortcut(string path,string target,string args) {
        Type t=Type.GetTypeFromProgID("WScript.Shell"); dynamic shell=Activator.CreateInstance(t); dynamic lnk=shell.CreateShortcut(path); lnk.TargetPath=target; lnk.Arguments=args; lnk.WorkingDirectory=Path.GetDirectoryName(target); lnk.Description="f2bgl SDL3 port"; lnk.Save(); Marshal.FinalReleaseComObject(lnk); Marshal.FinalReleaseComObject(shell);
    }
    static void WriteUninstaller(string target) {
        string script="$ErrorActionPreference = 'SilentlyContinue'\r\n$root = $PSScriptRoot\r\n$start = Join-Path $env:APPDATA 'Microsoft\\Windows\\Start Menu\\Programs\\f2bgl'\r\nRemove-Item -LiteralPath $start -Recurse -Force\r\nRemove-Item -LiteralPath (Join-Path ([Environment]::GetFolderPath('DesktopDirectory')) 'f2bgl.lnk') -Force\r\nRemove-Item -Path 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\f2bgl' -Recurse -Force\r\nStart-Sleep -Milliseconds 500\r\nRemove-Item -LiteralPath $root -Recurse -Force\r\n";
        File.WriteAllText(Path.Combine(target,"uninstall.ps1"),script,new System.Text.UTF8Encoding(true));
    }
    [STAThread] static void Main() { Application.EnableVisualStyles(); Application.SetCompatibleTextRenderingDefault(false); Application.Run(new SetupForm()); }
}

internal sealed class SliceStream : Stream {
    Stream s; long start,len,pos;
    public SliceStream(Stream source,long offset,long length){s=source;start=offset;len=length;pos=0;}
    public override bool CanRead{get{return true;}} public override bool CanSeek{get{return true;}} public override bool CanWrite{get{return false;}}
    public override long Length{get{return len;}} public override long Position{get{return pos;}set{Seek(value,SeekOrigin.Begin);}}
    public override int Read(byte[] b,int o,int c){int want=(int)Math.Min(c,len-pos);if(want<=0)return 0;s.Position=start+pos;int n=s.Read(b,o,want);pos+=n;return n;}
    public override long Seek(long off,SeekOrigin origin){long p=origin==SeekOrigin.Begin?off:origin==SeekOrigin.Current?pos+off:len+off;if(p<0||p>len)throw new IOException();pos=p;return pos;}
    public override void Flush(){} public override void SetLength(long v){throw new NotSupportedException();} public override void Write(byte[] b,int o,int c){throw new NotSupportedException();}
}
