[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][int]$HostProcessId,
    [string]$OutputDirectory = 'docs/user_guides/assets/packaged_slicer'
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes, System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class RipGuideWindow {
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr handle);
}
'@

$processInfo = Get-CimInstance Win32_Process -Filter "ProcessId=$HostProcessId"
if ($processInfo.Name -ne 'slicer_ui_host_sim.exe' -or
    $processInfo.CommandLine -notmatch '--guide-self-test')
{
    throw 'Use a dedicated host launched with --guide-self-test (workspace persistence disabled).'
}
$process = Get-Process -Id $HostProcessId
$window = [System.Windows.Automation.AutomationElement]::FromHandle($process.MainWindowHandle)
$scope = [System.Windows.Automation.TreeScope]::Descendants
function Find-Control([string]$Name)
{
    $condition = [System.Windows.Automation.PropertyCondition]::new(
        [System.Windows.Automation.AutomationElement]::NameProperty, $Name)
    $element = $window.FindFirst($scope, $condition)
    if ($null -eq $element) { throw "Control not found: $Name" }
    return $element
}
function Invoke-Control($Element)
{
    $pattern = [System.Windows.Automation.InvokePattern]$Element.GetCurrentPattern(
        [System.Windows.Automation.InvokePattern]::Pattern)
    $pattern.Invoke()
    Start-Sleep -Milliseconds 400
}

[void][RipGuideWindow]::SetForegroundWindow($process.MainWindowHandle)
Invoke-Control (Find-Control 'RIP 设置')
$panel = (Find-Control '模型与导入预检').Current.BoundingRectangle
$configuration = (Find-Control 'RIP 配置').Current.BoundingRectangle
$actions = (Find-Control '打开输出').Current.BoundingRectangle
$root = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $root -Force | Out-Null
function Save-Capture([string]$Name, [int]$Height)
{
    $bitmap = [System.Drawing.Bitmap]::new([int]$panel.Width, $Height)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try
    {
        $graphics.CopyFromScreen([int]$panel.Left, [int]$panel.Top, 0, 0, $bitmap.Size)
        $bitmap.Save((Join-Path $root $Name), [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally
    {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
    Write-Output "RIP_GUIDE_CAPTURE $Name $([int]$panel.Width)x$Height"
}

Save-Capture '14_RIP设置与手动入口.png' ([int]($actions.Bottom - $panel.Top + 12))
foreach ($item in @(
    @{ Control = 'RIP 墨量'; File = '15_RIP双墨量选项.png' },
    @{ Control = '输出验证'; File = '16_RIP单色彩色验证.png' },
    @{ Control = 'RIP 颜色模式'; File = '17_RIP五档颜色模式.png' }
))
{
    $control = Find-Control $item.Control
    Invoke-Control $control
    Save-Capture $item.File ([int]($configuration.Bottom - $panel.Top + 8))
    Invoke-Control $control
}
