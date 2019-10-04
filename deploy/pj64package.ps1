param (
    [switch]$debug = $false,
    [string]$platform = "x86"
)

$qtpath = $env:QTDIR

$rel = "Release"
$rels = "--release"

$plat = "Win32"

if ($platform -eq "x64")
{
    $plat = "x64"
}

if ($debug)
{
    $rel = "Debug"
    $rels = "--debug"
}

if ($null -eq $qtpath)
{
    if ($platform -eq "x64")
    {
        $qtpath = "C:\Qt\5.12.5\msvc2017_64"
    } else {
        $qtpath = "C:\Qt\5.12.5\msvc2017"
    }
}

$windeploy = "$($qtpath)\bin\windeployqt.exe"

# Copy dll
$pkpath = "gcadapter64_$($platform)"
$binpath = "$($pkpath)\gcadapter64.dll"

New-Item $pkpath -Type Directory -Force > $null
Copy-Item ("build\" + $plat + "\" + $rel + "\gcadapter64.dll") -Destination $binpath -Force

# Deploy Qt
& $windeploy $rels --no-quick-import --no-translations --no-system-d3d-compiler --no-compiler-runtime --no-angle --no-opengl-sw $binpath

# Package for PJ64

#libusb
$libusb = "libusb\MS32\libusb-1.0.dll"

if ($platform -eq "x64")
{
    $libusb = "libusb\MS64\libusb-1.0.dll"
}

Copy-Item $libusb -Destination ($pkpath + "\libusb-1.0.dll") -Force

New-Item ($pkpath + "\Plugin") -Type Directory -Force > $null
New-Item ($pkpath + "\Plugin\Input") -Type Directory -Force > $null

Move-Item -Path $binpath -Destination ($pkpath + "\Plugin\Input\gcadapter64.dll")

$pkgname = "$($pkpath).7z"
Write-Host "Packaging $($pkgname)..."

& 7z a -t7z $pkgname .\$pkpath\*

if ($env:APPVEYOR -eq $true) {
    Get-ChildItem $pkgname | % { Push-AppveyorArtifact $_.FullName -FileName $_.Name }
}
