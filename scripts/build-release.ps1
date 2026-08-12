[CmdletBinding()]
param(
    [string]$CMakePath = "cmake",
    [string]$Generator = "Ninja",
    [string]$Architecture = "",
    [string]$CompilerPath = "",
    [string]$BuildToolPath = "",
    [string]$ObjdumpPath = "",
    [string]$InnoSetupPath = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$buildDirectory = Join-Path $repoRoot "build-release"
$distDirectory = Join-Path $repoRoot "dist"
$cmakeFile = Join-Path $repoRoot "CMakeLists.txt"
$cmakeText = Get-Content -Raw -LiteralPath $cmakeFile
$versionMatch = [regex]::Match(
    $cmakeText,
    'project\s*\(\s*DeutschTelex\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)',
    [Text.RegularExpressions.RegexOptions]::IgnoreCase)
if (-not $versionMatch.Success) {
    throw "Could not read the authoritative DeutschTelex version from CMakeLists.txt."
}
$releaseVersion = $versionMatch.Groups[1].Value
$releaseDirectoryName = "DeutschTelex-$releaseVersion-win64"
$portableArchiveName = "DeutschTelex-$releaseVersion-win64-portable.zip"
$installerName = "DeutschTelex-$releaseVersion-win64-setup.exe"
$stageDirectory = Join-Path $distDirectory $releaseDirectoryName
$portableArchive = Join-Path $distDirectory $portableArchiveName
$installerArtifact = Join-Path $distDirectory $installerName
$checksumsPath = Join-Path $distDirectory "SHA256SUMS.txt"

function Remove-ScopedPath {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$AllowedParent,
        [Parameter(Mandatory = $true)][string]$ExpectedLeaf,
        [switch]$Recursive
    )

    $fullPath = [IO.Path]::GetFullPath($Path)
    $fullParent = [IO.Path]::GetFullPath($AllowedParent).TrimEnd('\', '/')
    $parentPrefix = $fullParent + [IO.Path]::DirectorySeparatorChar
    if (-not $fullPath.StartsWith($parentPrefix, [StringComparison]::OrdinalIgnoreCase) -or
        [IO.Path]::GetFileName($fullPath) -ne $ExpectedLeaf) {
        throw "Refusing to remove unexpected release path: $fullPath"
    }
    if (Test-Path -LiteralPath $fullPath) {
        if ($Recursive) {
            Remove-Item -LiteralPath $fullPath -Recurse -Force
        } else {
            Remove-Item -LiteralPath $fullPath -Force
        }
    }
}

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)][string]$Program,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Program failed with exit code $LASTEXITCODE."
    }
}

function Resolve-AdjacentTool {
    param(
        [Parameter(Mandatory = $true)][string]$PrimaryTool,
        [Parameter(Mandatory = $true)][string]$SiblingName
    )
    $resolved = Get-Command $PrimaryTool -ErrorAction Stop
    $candidate = Join-Path (Split-Path -Parent $resolved.Source) $SiblingName
    if (Test-Path -LiteralPath $candidate) {
        return $candidate
    }
    return (Get-Command $SiblingName -ErrorAction Stop).Source
}

Remove-ScopedPath -Path $buildDirectory -AllowedParent $repoRoot `
    -ExpectedLeaf "build-release" -Recursive
New-Item -ItemType Directory -Path $buildDirectory | Out-Null
if (-not (Test-Path -LiteralPath $distDirectory)) {
    New-Item -ItemType Directory -Path $distDirectory | Out-Null
}
Remove-ScopedPath -Path $stageDirectory -AllowedParent $distDirectory `
    -ExpectedLeaf $releaseDirectoryName -Recursive
Remove-ScopedPath -Path $portableArchive -AllowedParent $distDirectory `
    -ExpectedLeaf $portableArchiveName
Remove-ScopedPath -Path $installerArtifact -AllowedParent $distDirectory `
    -ExpectedLeaf $installerName
Remove-ScopedPath -Path $checksumsPath -AllowedParent $distDirectory `
    -ExpectedLeaf "SHA256SUMS.txt"

$configureArguments = @(
    "-S", $repoRoot,
    "-B", $buildDirectory,
    "-DBUILD_TESTING=ON",
    "-DCMAKE_BUILD_TYPE=Release"
)
if ($Generator) {
    $configureArguments += @("-G", $Generator)
}
if ($Architecture) {
    $configureArguments += @("-A", $Architecture)
}
if ($CompilerPath) {
    $configureArguments += "-DCMAKE_CXX_COMPILER=$CompilerPath"
}
if ($BuildToolPath) {
    $configureArguments += "-DCMAKE_MAKE_PROGRAM=$BuildToolPath"
}

Invoke-Checked -Program $CMakePath -Arguments $configureArguments
Invoke-Checked -Program $CMakePath -Arguments @(
    "--build", $buildDirectory, "--config", "Release", "--parallel")

$ctestPath = Resolve-AdjacentTool -PrimaryTool $CMakePath -SiblingName "ctest.exe"
Invoke-Checked -Program $ctestPath -Arguments @(
    "--test-dir", $buildDirectory, "-C", "Release", "--output-on-failure")

$executableCandidates = @(
    (Join-Path $buildDirectory "DeutschTelex.exe"),
    (Join-Path $buildDirectory "Release\DeutschTelex.exe")
)
$executable = $executableCandidates | Where-Object { Test-Path -LiteralPath $_ } |
    Select-Object -First 1
if (-not $executable) {
    throw "Release build did not produce DeutschTelex.exe."
}

$versionInfo = (Get-Item -LiteralPath $executable).VersionInfo
if ($versionInfo.FileVersion -ne "$releaseVersion.0") {
    throw "Unexpected file version '$($versionInfo.FileVersion)'."
}
if (-not $versionInfo.ProductVersion.StartsWith($releaseVersion,
                                                [StringComparison]::Ordinal)) {
    throw "Unexpected product version '$($versionInfo.ProductVersion)'."
}
if ($versionInfo.ProductName -ne "DeutschTelex" -or
    $versionInfo.OriginalFilename -ne "DeutschTelex.exe") {
    throw "Windows executable metadata is incomplete or inconsistent."
}

if (-not $ObjdumpPath) {
    $objdumpCommand = Get-Command "objdump.exe" -ErrorAction SilentlyContinue
    if ($objdumpCommand) {
        $ObjdumpPath = $objdumpCommand.Source
    }
}
if (-not $ObjdumpPath -or -not (Test-Path -LiteralPath $ObjdumpPath)) {
    throw "objdump.exe is required for x64/subsystem and runtime dependency inspection."
}

$fileHeader = (& $ObjdumpPath -f $executable | Out-String)
if ($LASTEXITCODE -ne 0 -or $fileHeader -notmatch 'pei-x86-64') {
    throw "DeutschTelex.exe is not a verified Windows x64 PE executable."
}
$peDetails = & $ObjdumpPath -p $executable
if ($LASTEXITCODE -ne 0) {
    throw "Could not inspect DeutschTelex.exe PE metadata."
}
$peText = $peDetails | Out-String
if ($peText -notmatch 'Subsystem\s+00000002\s+\(Windows GUI\)') {
    throw "DeutschTelex.exe is not using the Windows GUI subsystem."
}
$requiredMitigations = @("HIGH_ENTROPY_VA", "DYNAMIC_BASE", "NX_COMPAT")
foreach ($mitigation in $requiredMitigations) {
    if ($peText -notmatch [regex]::Escape($mitigation)) {
        throw "DeutschTelex.exe is missing required PE mitigation $mitigation."
    }
}
$controlFlowGuard = if ($peText -match 'GUARD_CF') { "enabled" } else { "not present" }

$importedDlls = @($peDetails | ForEach-Object {
    if ($_ -match 'DLL Name:\s*(\S+)') { $Matches[1] }
} | Sort-Object -Unique)
if ($importedDlls.Count -eq 0) {
    throw "No PE import dependencies were detected."
}

$systemDlls = @(
    "ADVAPI32.dll", "COMCTL32.dll", "COMDLG32.dll", "GDI32.dll",
    "KERNEL32.dll", "OLE32.dll", "OLEAUT32.dll", "SHELL32.dll",
    "USER32.dll", "USERENV.dll", "UXTHEME.dll", "VERSION.dll",
    "WINMM.dll", "WS2_32.dll"
)
$nonSystemDlls = @($importedDlls | Where-Object {
    $_ -notmatch '^(api-ms-win-|ext-ms-win-)' -and $_ -notin $systemDlls
})

New-Item -ItemType Directory -Path $stageDirectory | Out-Null
Copy-Item -LiteralPath $executable -Destination (Join-Path $stageDirectory "DeutschTelex.exe")
Copy-Item -LiteralPath (Join-Path $repoRoot "README.md") -Destination $stageDirectory
Copy-Item -LiteralPath (Join-Path $repoRoot "LICENSE") -Destination $stageDirectory
Copy-Item -LiteralPath (Join-Path $repoRoot "CHANGELOG.md") -Destination $stageDirectory
$releaseNotes = Join-Path $repoRoot "docs\release-$releaseVersion.md"
if (-not (Test-Path -LiteralPath $releaseNotes)) {
    throw "Release notes are missing for version $releaseVersion."
}
Copy-Item -LiteralPath $releaseNotes `
    -Destination (Join-Path $stageDirectory "RELEASE-NOTES.md")

$runtimeSearchDirectories = @((Split-Path -Parent $executable))
if ($CompilerPath) {
    $runtimeSearchDirectories += (Split-Path -Parent $CompilerPath)
}
foreach ($dll in $nonSystemDlls) {
    $located = $null
    foreach ($directory in $runtimeSearchDirectories) {
        $candidate = Join-Path $directory $dll
        if (Test-Path -LiteralPath $candidate) {
            $located = $candidate
            break
        }
    }
    if (-not $located) {
        throw "Required non-system runtime DLL '$dll' was not found for packaging."
    }
    Copy-Item -LiteralPath $located -Destination $stageDirectory
}

$dependencyReport = @(
    "DeutschTelex $releaseVersion runtime dependency report",
    "Architecture: Windows x64",
    "Subsystem: Windows GUI",
    "Runtime strategy: static libgcc/libstdc++ when built with MinGW",
    "PE mitigations: High Entropy VA, Dynamic Base/ASLR, NX/DEP",
    "Control Flow Guard: $controlFlowGuard",
    "",
    "Imported Windows DLLs:"
) + ($importedDlls | ForEach-Object { "- $_" }) + @(
    "",
    "Packaged non-system runtime DLLs:"
) + $(if ($nonSystemDlls.Count -eq 0) { "- none" } else {
        $nonSystemDlls | ForEach-Object { "- $_" }
    })
Set-Content -LiteralPath (Join-Path $stageDirectory "RUNTIME-DEPENDENCIES.txt") `
    -Value $dependencyReport -Encoding UTF8

Compress-Archive -LiteralPath $stageDirectory -DestinationPath $portableArchive `
    -CompressionLevel Optimal

Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::OpenRead($portableArchive)
try {
    $prefix = $releaseDirectoryName + "/"
    $unexpectedEntry = $archive.Entries | Where-Object {
        $normalizedName = $_.FullName.Replace('\', '/')
        $normalizedName -and -not $normalizedName.StartsWith(
            $prefix, [StringComparison]::Ordinal)
    } | Select-Object -First 1
    if ($unexpectedEntry) {
        throw "Portable ZIP does not contain exactly one release root directory."
    }
    $actualEntries = @($archive.Entries | ForEach-Object {
        $_.FullName.Replace('\', '/')
    } | Where-Object { $_ } | Sort-Object)
    $expectedEntries = @(
        "$releaseDirectoryName/DeutschTelex.exe",
        "$releaseDirectoryName/README.md",
        "$releaseDirectoryName/LICENSE",
        "$releaseDirectoryName/CHANGELOG.md",
        "$releaseDirectoryName/RELEASE-NOTES.md",
        "$releaseDirectoryName/RUNTIME-DEPENDENCIES.txt"
    ) + @($nonSystemDlls | ForEach-Object { "$releaseDirectoryName/$_" })
    $expectedEntries = @($expectedEntries | Sort-Object)
    if (Compare-Object -ReferenceObject $expectedEntries `
                       -DifferenceObject $actualEntries) {
        throw "Portable ZIP contents differ from the explicit release allowlist."
    }
} finally {
    $archive.Dispose()
}

if (-not $InnoSetupPath) {
    $innoCommand = Get-Command "ISCC.exe" -ErrorAction SilentlyContinue
    if ($innoCommand) {
        $InnoSetupPath = $innoCommand.Source
    } else {
        $commonInnoPaths = @(
            "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
            "$env:ProgramFiles\Inno Setup 6\ISCC.exe"
        )
        $InnoSetupPath = $commonInnoPaths | Where-Object {
            $_ -and (Test-Path -LiteralPath $_)
        } | Select-Object -First 1
    }
}

$installerStatus = "not built (Inno Setup 6 compiler not found)"
if ($InnoSetupPath -and (Test-Path -LiteralPath $InnoSetupPath)) {
    $installerScript = Join-Path $buildDirectory "generated\DeutschTelex.iss"
    Invoke-Checked -Program $InnoSetupPath -Arguments @($installerScript)
    if (-not (Test-Path -LiteralPath $installerArtifact)) {
        throw "Inno Setup completed without producing the expected installer."
    }
    $installerStatus = "built"
}

$checksumTargets = @($portableArchive)
if (Test-Path -LiteralPath $installerArtifact) {
    $checksumTargets += $installerArtifact
}
$checksumLines = $checksumTargets | ForEach-Object {
    $hash = Get-FileHash -LiteralPath $_ -Algorithm SHA256
    "$($hash.Hash.ToLowerInvariant())  $([IO.Path]::GetFileName($_))"
}
Set-Content -LiteralPath $checksumsPath -Value $checksumLines -Encoding ASCII

Write-Host "Release version: $releaseVersion"
Write-Host "Portable directory: $stageDirectory"
Write-Host "Portable ZIP: $portableArchive"
Write-Host "Installer: $installerStatus"
Write-Host "Checksums: $checksumsPath"
