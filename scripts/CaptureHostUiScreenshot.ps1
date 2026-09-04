<#
.SYNOPSIS
    抓取 SliceSoft 宿主窗口截图，用于用户手册配图。

.DESCRIPTION
    只截【指定窗口】而不是整个桌面：手册配图会随仓库分发，全屏截图会把开发者桌面上的
    其它内容一并带进去。故此处按窗口句柄取其矩形后再裁剪。

    窗口若被最小化或被其它窗口遮挡，截出的内容不可靠，因此先将目标窗口前置并校验其可见。
#>
param(
    [string]$ProcessName = "slicer_ui_host_sim",
    [Parameter(Mandatory = $true)]
    [string]$OutputPath,
    [int]$SettleMs = 600
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing, System.Windows.Forms

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class HostWindow
{
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }

    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr h);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
}
"@

$process = Get-Process -Name $ProcessName -ErrorAction SilentlyContinue |
    Where-Object { $_.MainWindowHandle -ne 0 } |
    Select-Object -First 1
if ($null -eq $process)
{
    throw "未找到带主窗口的进程：$ProcessName。请先启动 SliceSoft。"
}

$handle = $process.MainWindowHandle
if ([HostWindow]::IsIconic($handle))
{
    # 9 = SW_RESTORE。最小化窗口无法取得有效像素。
    [void][HostWindow]::ShowWindow($handle, 9)
}
[void][HostWindow]::SetForegroundWindow($handle)
Start-Sleep -Milliseconds $SettleMs

$rect = New-Object HostWindow+RECT
if (-not [HostWindow]::GetWindowRect($handle, [ref]$rect))
{
    throw "无法取得窗口矩形。"
}
$width = $rect.Right - $rect.Left
$height = $rect.Bottom - $rect.Top
if ($width -le 0 -or $height -le 0)
{
    throw "窗口尺寸无效：$width x $height"
}

$directory = Split-Path -Parent $OutputPath
if ($directory -and -not (Test-Path -LiteralPath $directory -PathType Container))
{
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
}

$bitmap = New-Object System.Drawing.Bitmap $width, $height
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
try
{
    $graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bitmap.Size)
    $bitmap.Save($OutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
}
finally
{
    $graphics.Dispose()
    $bitmap.Dispose()
}

Write-Output ("CAPTURE_OK {0} {1}x{2}" -f $OutputPath, $width, $height)
