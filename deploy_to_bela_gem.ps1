<#WARNING:
    for some reason the first make run of a session has to be run from the browser IDE
#>
<# Usage:
   (Run from a PowerShell prompt, e.g. PS C:\Users\tobia>)
   powershell -ExecutionPolicy Bypass -File "\\wsl`$\Ubuntu\home\tobi\groove_box\deploy_to_bela.ps1" [-Debug]
#>

<#Warning:
All the u8x2 files have been compiled and are there as .o files in the build folder. if the build folder is deleted, the u8g2 files need to be recompiled.
For this the u8g2 folder has to be copied to the project folder, after that it can be deleted again. i dont know why it does not compile the 
files when outside the project folder even though the folder is specified in the make parameters.
#>

param(
    [switch]$Debug,
    [switch]$Rebuild,
    [switch]$CopyAll
)

Write-Host "Starting deployment to Bela..."

$BelaIp = "192.168.6.2"
$LocalRoot = "\\wsl`$\Ubuntu\home\tobi\groove_box\"
$Project = "instrumentFromPC"
$RemoteFolder = "/root/Bela/projects/" + $Project
$MakeDir = "/root/Bela"
$Target = "run"

$LocalRender = Join-Path $LocalRoot "render.cpp"
$LocalInclude = Join-Path $LocalRoot "include"
$LocalSrc = Join-Path $LocalRoot "src"
$LocalU8g2 = Join-Path $LocalRoot "u8g2"
$StateFile = Join-Path $PSScriptRoot ".deploy_state.txt"

function Ensure-Tool($Name) {
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "Required command '$Name' not found."
    }
}

function Invoke-Ssh {
    param(
        [Parameter(Mandatory = $true)][string]$Command,
        [switch]$ForceTty
    )

    $sshArgs = @()
    if ($ForceTty) {
        $sshArgs += "-tt"
    }

    & ssh @sshArgs $script:remoteUserHost $Command
    if ($LASTEXITCODE -ne 0) {
        throw "Remote command failed: $Command"
    }
}

function Get-RelativePath([string]$BasePath, [string]$FullPath) {
    $baseFull = (Resolve-Path -LiteralPath $BasePath).ProviderPath
    if (-not $baseFull.EndsWith('\')) { $baseFull = "$baseFull\" }
    $full = (Resolve-Path -LiteralPath $FullPath).ProviderPath
    return $full.Substring($baseFull.Length)
}

function Get-RemoteDir([string]$RemotePath) {
    $idx = $RemotePath.LastIndexOf('/')
    if ($idx -lt 0) { return $RemoteFolder }
    return $RemotePath.Substring(0, $idx)
}

function Sync-BelaClock {
    $nowUtc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-dd HH:mm:ss")
    Write-Host "[bela] Syncing clock to host UTC: $nowUtc"
    Invoke-Ssh "date -u -s '$nowUtc'"
}

Ensure-Tool "ssh"
Ensure-Tool "scp"

if (-not (Test-Path $LocalRender -PathType Leaf)) { throw "Missing file: $LocalRender" }
if (-not (Test-Path $LocalInclude -PathType Container)) { throw "Missing directory: $LocalInclude" }
if (-not (Test-Path $LocalSrc -PathType Container)) { throw "Missing directory: $LocalSrc" }
if (-not (Test-Path $LocalU8g2 -PathType Container)) { throw "Missing directory: $LocalU8g2" }

${remoteUserHost} = "root@${BelaIp}"
$createdRemoteDirs = [System.Collections.Generic.HashSet[string]]::new()

function Ensure-RemoteDir([string]$Dir) {
    if ([string]::IsNullOrEmpty($Dir)) { return }
    if (-not $createdRemoteDirs.Contains($Dir)) {
        Invoke-Ssh "mkdir -p '$Dir'"
        $createdRemoteDirs.Add($Dir) | Out-Null
    }
}

function Remove-RemoteArtifacts {
    param(
        [Parameter(Mandatory = $true)][string]$Key
    )

    if ([string]::IsNullOrEmpty($Key)) { return }

    $normalizedKey = $Key -replace '\\','/'
    $windowsKey = $normalizedKey -replace '/','\'
    $withoutExtension = [System.IO.Path]::ChangeExtension($windowsKey, $null)
    if ($withoutExtension) {
        $withoutExtension = $withoutExtension -replace '\\','/'
    }

    $patterns = @()
    switch -Wildcard ($normalizedKey) {
        "render.cpp" {
            $patterns += "build/render.cpp.*"
            if ($withoutExtension) { $patterns += "build/$withoutExtension.*" }
        }
        "src/*" {
            $patterns += "build/$normalizedKey.*"
            if ($withoutExtension) { $patterns += "build/$withoutExtension.*" }
        }
    }

    if ($patterns.Count -eq 0) { return }

    $patterns = $patterns | Sort-Object -Unique
    foreach ($pattern in $patterns) {
        Invoke-Ssh "cd $RemoteFolder && rm -f $pattern"
    }
}

Write-Host "Reload systemctl"
Invoke-Ssh "systemctl daemon-reload"

Sync-BelaClock

Invoke-Ssh "mkdir -p '$RemoteFolder' '$RemoteFolder/include' '$RemoteFolder/src' '$RemoteFolder/u8g2'"

if ($Rebuild) {
    $confirm1 = (Read-Host "are you sure you want to rebuild").Trim().ToLowerInvariant()
    if ($confirm1 -ne "y") {
        Write-Host "Rebuild cancelled."
        return
    }

    $confirm2 = (Read-Host "are you really sure").Trim().ToLowerInvariant()
    if ($confirm2 -ne "y") {
        Write-Host "Rebuild cancelled."
        return
    }

    Write-Host "[bela] Removing remote build directory..."
    #Invoke-Ssh "rm -r '$RemoteFolder/build'"
    Invoke-Ssh "rm -rf Bela/projects/instrumentFromPC/build/src"
}

if ($CopyAll) {
    Write-Host "[bela] Copying all files (forced)."
}

$oldState = @{}
if (Test-Path $StateFile -PathType Leaf) {
    foreach ($line in Get-Content $StateFile) {
        if ($line -match '^(.*)\|(.*)$') {
            $oldState[$Matches[1]] = $Matches[2]
        }
    }
}

$newState = @{}
$entries = @()

$entries += [pscustomobject]@{
    Key = "render.cpp"
    LocalPath = (Resolve-Path -LiteralPath $LocalRender).ProviderPath
    RemotePath = "$RemoteFolder/render.cpp"
}

Get-ChildItem -LiteralPath $LocalInclude -File -Recurse | ForEach-Object {
    $relative = Get-RelativePath $LocalInclude $_.FullName
    $normalized = ($relative -replace '\\','/')
    $entries += [pscustomobject]@{
        Key = "include/$normalized"
        LocalPath = $_.FullName
        RemotePath = "$RemoteFolder/include/$normalized"
    }
}

Get-ChildItem -LiteralPath $LocalSrc -File -Recurse | ForEach-Object {
    $relative = Get-RelativePath $LocalSrc $_.FullName
    $normalized = ($relative -replace '\\','/')
    $entries += [pscustomobject]@{
        Key = "src/$normalized"
        LocalPath = $_.FullName
        RemotePath = "$RemoteFolder/src/$normalized"
    }
}

Get-ChildItem -LiteralPath $LocalU8g2 -File -Recurse | ForEach-Object {
    $relative = Get-RelativePath $LocalU8g2 $_.FullName
    $normalized = ($relative -replace '\\','/')
    $entries += [pscustomobject]@{
        Key = "u8g2/$normalized"
        LocalPath = $_.FullName
        RemotePath = "$RemoteFolder/u8g2/$normalized"
    }
}


$copied = @()
foreach ($entry in $entries) {
    $hash = (Get-FileHash -LiteralPath $entry.LocalPath -Algorithm SHA256).Hash
    $newState[$entry.Key] = $hash

    if ($CopyAll -or -not $oldState.ContainsKey($entry.Key) -or $oldState[$entry.Key] -ne $hash) {
        $remotePath = $entry.RemotePath -replace '\\','/'
        $remoteDir = Get-RemoteDir $remotePath
        Ensure-RemoteDir $remoteDir

        & scp -q $entry.LocalPath "${remoteUserHost}:$remotePath"
        if ($LASTEXITCODE -ne 0) { throw "Failed to copy $($entry.Key)" }
        $copied += $entry.Key
        Remove-RemoteArtifacts -Key $entry.Key
        $copiedName = Split-Path -Path $entry.Key -Leaf
        Write-Host "copied $copiedName"
    }
}

$removed = @()
foreach ($key in $oldState.Keys) {
    if (-not $newState.ContainsKey($key)) {
        $remotePath = "$RemoteFolder/$key" -replace '\\','/'
        Invoke-Ssh "rm -f '$remotePath'"
        $removed += $key
    }
}

($newState.GetEnumerator() | ForEach-Object { "$($_.Key)|$($_.Value)" }) |
    Set-Content -Path $StateFile -Encoding UTF8

Write-Host "[bela] Copied $($copied.Count) file(s); removed $($removed.Count)."

$remoteU8g2 = "$RemoteFolder/u8g2"
$cflags = "-I$remoteU8g2/csrc"
$cppflags = "-I$remoteU8g2/csrc"
if ($Debug) {
    $cflags += " -DDEBUG_BUILD -UNDEBUG"
    $cppflags += " -DDEBUG_BUILD -UNDEBUG"
}

$makeCmd = "cd '$MakeDir' && env CFLAGS='$cflags' CPPFLAGS='$cppflags' make PROJECT='$Project' $Target"
if ($Debug) {
    $makeCmd += " debug"
}
Invoke-Ssh -Command $makeCmd -ForceTty

Write-Host "[bela] Done."
