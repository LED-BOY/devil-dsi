param(
    [string]$DevkitPro = 'C:/devkitPro',
    [string]$CMake = 'cmake',
    [string]$Ninja = 'ninja',
    [int]$Jobs = 6
)
$ErrorActionPreference = 'Stop'
$savedDevkitPro = $env:DEVKITPRO
try {
    $env:DEVKITPRO = (Resolve-Path -LiteralPath $DevkitPro).Path.Replace('\', '/')
    $ninjaPath = (Get-Command $Ninja -ErrorAction Stop).Source.Replace('\', '/')
    $buildDirectory = Join-Path $PSScriptRoot 'build-dsi'
    $configureArguments = @('-S', $PSScriptRoot, '-B', $buildDirectory, '-G', 'Ninja',
        "-DCMAKE_MAKE_PROGRAM=$ninjaPath",
        '-DCMAKE_TOOLCHAIN_FILE=CMake/platforms/dsi.toolchain.cmake',
        '-DCMAKE_BUILD_TYPE=MinSizeRel', '-DVERSION_SUFFIX=-DSi')
    $vendorDirectory = Join-Path $PSScriptRoot 'dsi-vendor'
    if (Test-Path -LiteralPath $vendorDirectory) {
        Get-ChildItem -LiteralPath $vendorDirectory -Directory | ForEach-Object {
            $dependencyName = $_.Name.ToUpperInvariant()
            $configureArguments += "-DFETCHCONTENT_SOURCE_DIR_${dependencyName}=$($_.FullName.Replace('\', '/'))"
        }
    }
    & $CMake @configureArguments
    if ($LASTEXITCODE -ne 0) { throw 'DSi CMake configuration failed' }
    & $CMake --build $buildDirectory --target devilutionx -j $Jobs
    if ($LASTEXITCODE -ne 0) { throw 'DSi engine build failed' }
    Write-Output "Built: $buildDirectory/devilutionx-dsi.nds"
} finally {
    $env:DEVKITPRO = $savedDevkitPro
}
