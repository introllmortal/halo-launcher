<#
.SYNOPSIS
    Packages and publishes modpack updates to GitHub Releases.

.DESCRIPTION
    Creates a clean, distribution-ready modpack archive, generates/updates instance configuration,
    creates a GitHub Release, uploads the modpack archive, and updates updates.json for Halo Launcher.

.PARAMETER Version
    The version tag to release (e.g., 1.0.0).

.PARAMETER Changelog
    Markdown description of changes in this version.

.PARAMETER InstancePath
    Path to the source instance directory. Defaults to standard instances folder.

.PARAMETER Repo
    GitHub repository in 'owner/repo' format (default: introllmortal/mindice-modpack).

.PARAMETER Token
    GitHub Personal Access Token with repo scope. Can also be set via GITHUB_TOKEN environment variable.

.EXAMPLE
    .\publish_modpack.ps1 -Version "1.0.0" -Changelog "Initial release"
#>

[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [string]$Version,

    [Parameter(Position = 1)]
    [string]$Changelog,

    [Parameter()]
    [string]$InstancePath = "$env:APPDATA\PrismLauncher\instances\Create_Clockwork_Empire_Prism",

    [Parameter()]
    [string]$Repo = "introllmortal/mindice-modpack",

    [Parameter()]
    [string]$Token = $env:GITHUB_TOKEN,

    [Parameter()]
    [string]$PackId = "Create_Clockwork_Empire",

    [Parameter()]
    [string]$PackName = "Create Clockwork Empire",

    [Parameter()]
    [string]$OutputDir = "$PSScriptRoot\..\dist_modpack"
)

$ErrorActionPreference = "Stop"

# Fallback for Token
if (-not $Token) {
    $Token = $env:GH_TOKEN
}
if (-not $Token) {
    # Check default known location or prompt
    $Token = Read-Host -Prompt "Enter GitHub Personal Access Token (repo scope)"
}

if (-not $Token) {
    Write-Error "GitHub token is required to publish releases and update updates.json."
    exit 1
}

# Prompt for Version if not provided
if (-not $Version) {
    $Version = Read-Host -Prompt "Enter release version (e.g. 1.0.0)"
}
$Version = $Version.TrimStart('v')

if (-not $Version) {
    Write-Error "Release version cannot be empty."
    exit 1
}

# Prompt for Changelog if not provided
if (-not $Changelog) {
    $Changelog = Read-Host -Prompt "Enter changelog (or press Enter for default)"
    if (-not $Changelog) {
        $Changelog = "Release version $Version"
    }
}

if (-not (Test-Path $InstancePath)) {
    Write-Error "Source instance path not found: $InstancePath"
    exit 1
}

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host " Publishing Modpack: $PackName v$Version" -ForegroundColor Green
Write-Host " Target Repo: $Repo" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

# 1. Prepare Staging Directory
$stagingDir = Join-Path $env:TEMP ("halo_modpack_stage_" + [System.Guid]::NewGuid().ToString("N").Substring(0, 8))
if (Test-Path $stagingDir) { Remove-Item -Recurse -Force $stagingDir }
New-Item -ItemType Directory -Path $stagingDir | Out-Null

$stagedMinecraft = Join-Path $stagingDir ".minecraft"
New-Item -ItemType Directory -Path $stagedMinecraft | Out-Null

# 2. Copy mmc-pack.json
$srcMmcPack = Join-Path $InstancePath "mmc-pack.json"
if (Test-Path $srcMmcPack) {
    Copy-Item $srcMmcPack (Join-Path $stagingDir "mmc-pack.json") -Force
}

# 3. Copy Icon if present
$iconSrc = "$env:APPDATA\PrismLauncher\icons\clockwork_empire.jpg"
if (Test-Path $iconSrc) {
    Copy-Item $iconSrc (Join-Path $stagingDir "clockwork_empire.jpg") -Force
    Copy-Item $iconSrc (Join-Path $stagingDir "icon.png") -Force
}

# 4. Generate Clean instance.cfg
$instanceCfgContent = @"
[General]
ConfigVersion=1.3
ManagedPack=true
ManagedPackType=github
ManagedPackID=$PackId
ManagedPackName="$PackName"
ManagedPackVersionID=$Version
ManagedPackVersionName="$Version"
ManagedPackURL=https://raw.githubusercontent.com/$Repo/main/updates.json
iconKey=clockwork_empire
InstanceType=OneSix
name=$PackName
OverrideMemory=true
MinMemAlloc=6144
MaxMemAlloc=8192
AutomaticJava=true
"@

Set-Content -Path (Join-Path $stagingDir "instance.cfg") -Value $instanceCfgContent -Encoding UTF8

# 5. Copy .minecraft files excluding saves, logs, and volatile local data
Write-Host "[1/5] Staging modpack files..." -ForegroundColor Yellow
$srcMinecraft = Join-Path $InstancePath ".minecraft"
$excludedFolders = @(
    "saves", "logs", "crash-reports", "screenshots",
    "_prism_backups", ".cache", "webcache", "server-cache"
)
$excludedFiles = @(
    "options.txt", "optionsshaders.txt", "servers.dat",
    "usercache.json", "usernamecache.json", "hotbar.nbt"
)

Get-ChildItem -Path $srcMinecraft | ForEach-Object {
    $itemName = $_.Name
    if ($_.PSIsContainer) {
        if ($excludedFolders -notcontains $itemName.ToLower()) {
            Copy-Item -Path $_.FullName -Destination (Join-Path $stagedMinecraft $itemName) -Recurse -Force
        }
    } else {
        if ($excludedFiles -notcontains $itemName.ToLower() -and -not ($itemName.EndsWith(".log") -or $itemName.EndsWith(".tmp"))) {
            Copy-Item -Path $_.FullName -Destination (Join-Path $stagedMinecraft $itemName) -Force
        }
    }
}

# 6. Compress into Zip
Write-Host "[2/5] Creating release archive..." -ForegroundColor Yellow
if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir | Out-Null
}

$zipFileName = "$PackId-$Version.zip"
$zipFilePath = Join-Path $OutputDir $zipFileName
if (Test-Path $zipFilePath) { Remove-Item -Force $zipFilePath }

Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory($stagingDir, $zipFilePath, [System.IO.Compression.CompressionLevel]::Optimal, $false)

$zipSizeMb = [math]::Round((Get-Item $zipFilePath).Length / 1MB, 2)
Write-Host "Archive created: $zipFilePath ($zipSizeMb MB)" -ForegroundColor Green

# 7. Create GitHub Release
Write-Host "[3/5] Creating GitHub Release v$Version on $Repo..." -ForegroundColor Yellow
$headers = @{
    "Authorization" = "token $Token"
    "Accept" = "application/vnd.github.v3+json"
    "User-Agent" = "HaloLauncher-ModpackPublisher"
}

$releaseBody = @{
    tag_name = "v$Version"
    target_commitish = "main"
    name = "$PackName v$Version"
    body = $Changelog
    draft = $false
    prerelease = $false
} | ConvertTo-Json -Compress

$releaseResponse = $null
try {
    $releaseResponse = Invoke-RestMethod -Uri "https://api.github.com/repos/$Repo/releases" -Method Post -Headers $headers -Body $releaseBody -ContentType "application/json; charset=utf-8"
} catch {
    # If release already exists, fetch it
    $existing = Invoke-RestMethod -Uri "https://api.github.com/repos/$Repo/releases/tags/v$Version" -Headers $headers -ErrorAction SilentlyContinue
    if ($existing) {
        $releaseResponse = $existing
        Write-Host "Release v$Version already exists, reusing." -ForegroundColor Yellow
    } else {
        Write-Error "Failed to create GitHub release: $_"
        exit 1
    }
}

# 8. Upload Archive to Release
Write-Host "[4/5] Uploading $zipFileName to GitHub Release..." -ForegroundColor Yellow
$uploadUrlTemplate = $releaseResponse.upload_url
$uploadUri = $uploadUrlTemplate -replace '\{.*\}', ("?name=" + [System.Web.HttpUtility]::UrlEncode($zipFileName))
Add-Type -AssemblyName System.Web

$fileBytes = [System.IO.File]::ReadAllBytes($zipFilePath)
$uploadHeaders = @{
    "Authorization" = "token $Token"
    "Accept" = "application/vnd.github.v3+json"
    "Content-Type" = "application/zip"
    "User-Agent" = "HaloLauncher-ModpackPublisher"
}

try {
    $assetUpload = Invoke-RestMethod -Uri $uploadUri -Method Post -Headers $uploadHeaders -Body $fileBytes
    $downloadUrl = $assetUpload.browser_download_url
    Write-Host "Asset uploaded successfully: $downloadUrl" -ForegroundColor Green
} catch {
    Write-Warning "Direct asset upload failed or asset already exists. Checking release assets..."
    $assets = Invoke-RestMethod -Uri "https://api.github.com/repos/$Repo/releases/$($releaseResponse.id)/assets" -Headers $headers
    $matchedAsset = $assets | Where-Object { $_.name -eq $zipFileName }
    if ($matchedAsset) {
        $downloadUrl = $matchedAsset.browser_download_url
        Write-Host "Reusing existing asset: $downloadUrl" -ForegroundColor Green
    } else {
        Write-Error "Upload failed: $_"
        exit 1
    }
}

# 9. Update updates.json on GitHub
Write-Host "[5/5] Updating updates.json manifest..." -ForegroundColor Yellow
$manifestPath = "updates.json"
$existingManifestSha = $null
$manifestContent = @{
    name = $PackName
    id = $PackId
    versions = @()
}

try {
    $fileInfo = Invoke-RestMethod -Uri "https://api.github.com/repos/$Repo/contents/$manifestPath" -Headers $headers -ErrorAction Stop
    $existingManifestSha = $fileInfo.sha
    $rawBase64 = $fileInfo.content
    $bytes = [Convert]::FromBase64String($rawBase64)
    $jsonString = [System.Text.Encoding]::UTF8.GetString($bytes)
    $manifestContent = $jsonString | ConvertFrom-Json
} catch {
    Write-Host "No existing updates.json found in repository; creating new manifest." -ForegroundColor Gray
}

# Ensure versions list exists
$versionsList = [System.Collections.Generic.List[PSObject]]::new()
if ($manifestContent.versions) {
    foreach ($v in $manifestContent.versions) {
        if ($v.version -ne $Version) {
            $versionsList.Add($v)
        }
    }
}

$newVersionEntry = [PSCustomObject]@{
    version = $Version
    name = $Version
    released = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    changelog = $Changelog
    downloadUrl = $downloadUrl
}

# Prepend latest version so it's first
$versionsList.Insert(0, $newVersionEntry)

$updatedManifestObj = [PSCustomObject]@{
    name = $PackName
    id = $PackId
    versions = $versionsList
}

$updatedJsonString = $updatedManifestObj | ConvertTo-Json -Depth 10
$base64Content = [Convert]::ToBase64String([System.Text.Encoding]::UTF8.GetBytes($updatedJsonString))

$putBody = @{
    message = "Release $PackName v$Version"
    content = $base64Content
    branch = "main"
}
if ($existingManifestSha) {
    $putBody["sha"] = $existingManifestSha
}

$putBodyJson = $putBody | ConvertTo-Json -Compress
$putResult = Invoke-RestMethod -Uri "https://api.github.com/repos/$Repo/contents/$manifestPath" -Method Put -Headers $headers -Body $putBodyJson -ContentType "application/json; charset=utf-8"

# 10. Clean up staging
Remove-Item -Recurse -Force $stagingDir -ErrorAction SilentlyContinue

Write-Host "==========================================" -ForegroundColor Green
Write-Host " Modpack Successfully Published!" -ForegroundColor Green
Write-Host " Version: $Version" -ForegroundColor White
Write-Host " Download URL: $downloadUrl" -ForegroundColor Cyan
Write-Host " Manifest URL: https://raw.githubusercontent.com/$Repo/main/updates.json" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Green
