param(
    [Parameter(Mandatory)][string] $Source,
    [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{40}$')][string] $ExpectedSha,
    [Parameter(Mandatory)][string] $Out,
    [string] $QtKit = 'C:/Qt/6.8.3/msvc2022_64',
    [string] $Viewer = 'C:/Program Files/RealVNC/VNC Viewer/vncviewer.exe',
    [string] $CMake = 'C:/Qt/Tools/CMake_64/bin/cmake.exe',
    [string] $Ninja = 'C:/Qt/Tools/Ninja/ninja.exe',
    [ValidateRange(10, 3600)][int] $Seconds = 120
)

$ErrorActionPreference = 'Stop'
$Source = (Resolve-Path -LiteralPath $Source).Path
$actual = git -C $Source rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $actual -ne $ExpectedSha) {
    throw "Checkout is not $ExpectedSha"
}
$dirty = git -C $Source status --porcelain --untracked-files=no
if ($LASTEXITCODE -ne 0 -or $dirty) {
    throw 'The exact source checkout has tracked changes'
}
$outDir = (New-Item -Path $Out -ItemType Directory).FullName

function Assert-Exit([string] $Operation) {
    if ($LASTEXITCODE -ne 0) { throw "$Operation exited $LASTEXITCODE" }
}

$libs = 'Qt6Core', 'Qt6Gui', 'Qt6Network', 'Qt6Qml', 'Qt6Quick' |
    ForEach-Object { "$QtKit/lib/$_.lib" }
$includes = '', '/QtCore', '/QtGui', '/QtNetwork', '/QtQml', '/QtQuick' |
    ForEach-Object { "/I$QtKit/include$_" }
foreach ($flag in 'OFF', 'ON') {
    $dir = "$outDir/$flag"
    & $CMake -S $Source -B $dir -G Ninja "-DCMAKE_MAKE_PROGRAM=$Ninja" `
        "-DCMAKE_PREFIX_PATH=$QtKit" -DCMAKE_BUILD_TYPE=Release `
        -DHYREMOTE_BUILD_TESTS=OFF -DHYREMOTE_BUILD_CPP_API=ON `
        -DHYREMOTE_BUILD_QUICK_ADAPTER=ON -DHYREMOTE_BUILD_WIDGETS_ADAPTER=ON `
        -DHYREMOTE_BUILD_QML_API=OFF -DHYREMOTE_WITH_GENERIC_PLUGIN=OFF `
        -DHYREMOTE_WITH_QPA_PROXY=OFF -DHYREMOTE_WITH_TRANSPORT_SECURITY=OFF `
        -DHYREMOTE_WITH_VNC=ON -DHYREMOTE_ENABLE_CPU_PROBE=ON `
        -DHYREMOTE_EXPERIMENTAL_QUICK_ACTIVITY_WAKE=OFF `
        -DHYREMOTE_EXPERIMENTAL_QUICK_D3D11_STAGING=OFF `
        "-DHYREMOTE_EXPERIMENTAL_QUICK_GL_PBO=$flag"
    Assert-Exit "Configure $flag"
    & $CMake --build $dir --target hyremote-remoteaccess
    Assert-Exit "Build $flag"
    & cl.exe /nologo /std:c++17 /Zc:__cplusplus /permissive- /Zc:preprocessor `
        /EHsc /O2 /MD /utf-8 /DNDEBUG /DQT_NO_DEBUG @includes `
        "/I$Source/src/integrations/cpp/include" "/I$dir/runtime/generated" `
        "/Fo$dir/host.obj" "/Fe$dir/host.exe" "$PSScriptRoot/host.cpp" `
        @libs "$dir/runtime/HyRemoteRemoteAccess.lib"
    Assert-Exit "Build host $flag"
}

$port = 15940
foreach ($count in 1, 0, 2) {
    foreach ($arm in 'A1', 'B', 'A2') {
        $dir = if ($arm -eq 'B') { "$outDir/ON" } else { "$outDir/OFF" }
        & python "$PSScriptRoot/measure_matrix.py" --host "$dir/host.exe" `
            --runtime "$dir/runtime/HyRemoteRemoteAccess.dll" --qt-bin "$QtKit/bin" `
            --viewer $Viewer --out "$outDir/viewers-$count-$arm" --source-sha $ExpectedSha `
            --arm $arm --viewers $count --seconds $Seconds --port $port
        Assert-Exit "$count Viewer $arm"
        ++$port
    }
}
