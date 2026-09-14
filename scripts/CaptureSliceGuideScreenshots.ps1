[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][int]$HostProcessId,
    [ValidateSet('Prepare', 'Inspect', 'Invoke', 'Click', 'Toggle', 'Focus', 'Capture')]
    [string]$Action = 'Inspect',
    [string]$ControlName,
    [string]$OutputPath,
    [string]$BottomControlName
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes, System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class SliceGuideWindow {
    public delegate bool EnumProc(IntPtr h, IntPtr param);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc callback, IntPtr param);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, System.Text.StringBuilder text, int count);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int w, int height, uint flags);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint flags, uint x, uint y, uint data, UIntPtr extra);
}
'@

$info = Get-CimInstance Win32_Process -Filter "ProcessId=$HostProcessId"
if ($info.Name -ne 'slicer_ui_host_sim.exe' -or
    $info.CommandLine -notmatch '--manual-guide-self-test')
{
    throw 'Only use a dedicated --manual-guide-self-test host (workspace persistence disabled).'
}
$process = Get-Process -Id $HostProcessId
$script:handle = [IntPtr]::Zero
[void][SliceGuideWindow]::EnumWindows({ param($h, $param)
    [uint32]$owner = 0
    [void][SliceGuideWindow]::GetWindowThreadProcessId($h, [ref]$owner)
    $title = [Text.StringBuilder]::new(512)
    [void][SliceGuideWindow]::GetWindowText($h, $title, $title.Capacity)
    if ($owner -eq $HostProcessId -and $title.ToString().Contains('打印宿主参考实现')) { $script:handle = $h }
    return $true
}, [IntPtr]::Zero)
if ($script:handle -eq 0) { throw 'The dedicated host main window was not found.' }
$window = [System.Windows.Automation.AutomationElement]::FromHandle($script:handle)
$scope = [System.Windows.Automation.TreeScope]::Descendants
function Find-Control([string]$Name)
{
    $condition = [System.Windows.Automation.PropertyCondition]::new(
        [System.Windows.Automation.AutomationElement]::NameProperty, $Name)
    $control = $window.FindFirst($scope, $condition)
    if ($null -eq $control) { throw "Control not found: $Name" }
    return $control
}

if ($Action -eq 'Prepare')
{
    [void][SliceGuideWindow]::SetWindowPos($script:handle, [IntPtr]::Zero, 80, 40, 1800, 1300, 0x0040)
    [void][SliceGuideWindow]::SetForegroundWindow($script:handle)
    Start-Sleep -Milliseconds 600
    $panel = (Find-Control '模型与导入预检').Current.BoundingRectangle
    [void][SliceGuideWindow]::SetCursorPos([int]$panel.Left - 3, [int]$panel.Top + 200)
    [SliceGuideWindow]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 100
    [void][SliceGuideWindow]::SetCursorPos([int]$panel.Left - 400, [int]$panel.Top + 200)
    Start-Sleep -Milliseconds 100
    [SliceGuideWindow]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
}
elseif ($Action -eq 'Inspect')
{
    $window.FindAll($scope, [System.Windows.Automation.Condition]::TrueCondition) |
        Where-Object { $_.Current.Name } |
        ForEach-Object { '{0} | {1} | {2}' -f $_.Current.ControlType.ProgrammaticName, $_.Current.Name, $_.Current.BoundingRectangle }
}
elseif ($Action -eq 'Invoke')
{
    $control = Find-Control $ControlName
    $pattern = [System.Windows.Automation.InvokePattern]$control.GetCurrentPattern(
        [System.Windows.Automation.InvokePattern]::Pattern)
    $pattern.Invoke()
}
elseif ($Action -eq 'Toggle')
{
    $control = Find-Control $ControlName
    $pattern = [System.Windows.Automation.TogglePattern]$control.GetCurrentPattern(
        [System.Windows.Automation.TogglePattern]::Pattern)
    $pattern.Toggle()
}
elseif ($Action -eq 'Click')
{
    $rect = (Find-Control $ControlName).Current.BoundingRectangle
    if ($rect.Width -le 0 -or $rect.Height -le 0) { throw 'Control has no visible rectangle.' }
    [void][SliceGuideWindow]::SetCursorPos([int]($rect.Left + $rect.Width / 2), [int]($rect.Top + $rect.Height / 2))
    [SliceGuideWindow]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)
    [SliceGuideWindow]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
}
elseif ($Action -eq 'Focus') { (Find-Control $ControlName).SetFocus() }
elseif ($Action -eq 'Capture')
{
    if (-not $OutputPath) { throw 'OutputPath is required for Capture.' }
    [void][SliceGuideWindow]::SetForegroundWindow($script:handle)
    Start-Sleep -Milliseconds 600
    $rect = (Find-Control $ControlName).Current.BoundingRectangle
    $bottom = if ($BottomControlName) { (Find-Control $BottomControlName).Current.BoundingRectangle.Bottom } else { $rect.Bottom }
    $rootRect = $window.Current.BoundingRectangle
    if ($rect.Width -le 0 -or $bottom -le $rect.Top -or
        $rect.Top -lt $rootRect.Top -or $bottom -gt $rootRect.Bottom)
    {
        throw 'Capture rectangle is not fully inside the dedicated host window.'
    }
    $target = [IO.Path]::GetFullPath($OutputPath)
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
    $bitmap = [System.Drawing.Bitmap]::new([int]$rect.Width, [int]($bottom - $rect.Top))
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try
    {
        $graphics.CopyFromScreen([int]$rect.Left, [int]$rect.Top, 0, 0, $bitmap.Size)
        $bitmap.Save($target, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally { $graphics.Dispose(); $bitmap.Dispose() }
    Write-Output "SLICE_GUIDE_CAPTURE $target"
}
Start-Sleep -Milliseconds 400
