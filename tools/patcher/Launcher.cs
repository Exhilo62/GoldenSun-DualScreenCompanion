// GoldenSun3DS-Patcher.exe: a tiny launcher that starts GoldenSun3DS-Patcher.ps1 (next to it) in a hidden PowerShell, so the patcher opens
// with a double-click like a normal program. All the real work is in the script; keep this exe in the same folder as the script and bin\.
using System;
using System.Diagnostics;
using System.IO;
using System.Windows.Forms;

static class Launcher
{
    [STAThread]
    static void Main(string[] args)
    {
        string dir = AppDomain.CurrentDomain.BaseDirectory;
        string script = Path.Combine(dir, "GoldenSun3DS-Patcher.ps1");
        if (!File.Exists(script))
        {
            MessageBox.Show("GoldenSun3DS-Patcher.ps1 was not found next to this program.\nUnzip the whole download and keep the files together.",
                "Golden Sun - Dual Screen Companion - Patcher", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return;
        }
        string extra = args.Length > 0 ? " " + string.Join(" ", args) : "";
        var psi = new ProcessStartInfo("powershell.exe", "-NoProfile -ExecutionPolicy Bypass -STA -WindowStyle Hidden -File \"" + script + "\"" + extra);
        psi.UseShellExecute = false;
        psi.CreateNoWindow = true;
        psi.WorkingDirectory = dir;
        try { Process.Start(psi); }
        catch (Exception e)
        {
            MessageBox.Show("Could not start Windows PowerShell:\n" + e.Message, "Golden Sun - Dual Screen Companion - Patcher", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }
}
