<#
.SYNOPSIS
    Installs the Sentry Unreal SDK plugin into Plugins/Sentry.

.DESCRIPTION
    By default installs the release pinned in sentry-unreal.properties (to try another
    release, change the pin). -Ref installs unreleased plugin sources on top of the pinned
    release binaries; -LocalPath links a local sentry-unreal checkout. Give a value to at
    most one of them. Empty values are treated as not given, so callers such as CI
    workflows can always pass -Ref.

    Requires the GitHub CLI (gh), authenticated with `gh auth login` locally or via the
    GH_TOKEN environment variable in CI. -LocalPath does not need it.

    Every run is a clean setup: the existing Plugins/Sentry (including its compiled
    Binaries/ and Intermediate/) is replaced. The new plugin is assembled in a temporary
    directory first; if anything fails, the existing plugin is left untouched.

.PARAMETER Ref
    Install plugin sources from this sentry-unreal branch or commit, combined with the
    binaries (Source/ThirdParty) of the pinned release. Warns when the ref pins different
    native/Java/Cocoa SDK versions than the pinned release.

.PARAMETER LocalPath
    Link Plugins/Sentry to a local sentry-unreal checkout (its plugin-dev directory) using a
    directory junction (Windows) or symlink (macOS/Linux). Binaries are whatever the checkout has.

.PARAMETER ProjectDir
    Unreal project directory to install into. Defaults to the repository root.

.EXAMPLE
    ./scripts/setup-sentry-sdk.ps1
.EXAMPLE
    ./scripts/setup-sentry-sdk.ps1 -Ref feat/my-branch
.EXAMPLE
    ./scripts/setup-sentry-sdk.ps1 -LocalPath D:\projects\sentry-unreal
#>
#Requires -Version 7.0

[CmdletBinding()]
param(
    [string]$Ref,
    [string]$LocalPath,
    [string]$ProjectDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue' # Invoke-WebRequest is many times slower with the progress bar

Add-Type -AssemblyName System.IO.Compression.FileSystem

$PinFileName = 'sentry-unreal.properties'
$IsGitHubActions = $env:GITHUB_ACTIONS -eq 'true'
$Warnings = [System.Collections.Generic.List[string]]::new()
$TotalTimer = [System.Diagnostics.Stopwatch]::StartNew()
$GhHint = 'Install the GitHub CLI (https://cli.github.com) and run `gh auth login`. In GitHub Actions, set GH_TOKEN: ${{ github.token }}.'

# Dependencies whose versions the plugin sources are compiled against. Compared for -Ref.
$Dependencies = @(
    @{ Name = 'sentry-native'; Kind = 'submodule'; Path = 'modules/sentry-native'; Repo = 'getsentry/sentry-native' }
    @{ Name = 'sentry-java'; Kind = 'submodule'; Path = 'modules/sentry-java'; Repo = 'getsentry/sentry-java' }
    @{ Name = 'sentry-cocoa'; Kind = 'properties'; Path = 'modules/sentry-cocoa.properties' }
)

class SetupError : System.Exception
{
    [string]$Hint
    SetupError([string]$message, [string]$hint) : base($message) { $this.Hint = $hint }
}

function Fail([string]$Message, [string]$Hint = '')
{
    throw [SetupError]::new($Message, $Hint)
}

function Write-Warn([string]$Message)
{
    $Warnings.Add($Message)
    # In GitHub Actions the annotation is rendered inline in the log as well as on the run page
    if ($IsGitHubActions) { Write-Host "::warning title=Sentry SDK setup::$Message" }
    else { Write-Host "WARNING: $Message" -ForegroundColor Yellow }
}

function Invoke-Step([string]$Name, [scriptblock]$Action)
{
    # Returns whatever $Action outputs
    $timer = [System.Diagnostics.Stopwatch]::StartNew()
    if ($IsGitHubActions) { Write-Host "::group::$Name" } else { Write-Host "==> $Name" -ForegroundColor Cyan }
    try
    {
        $result = & $Action
    }
    finally
    {
        if ($IsGitHubActions) { Write-Host '::endgroup::' }
    }
    $seconds = [math]::Round($timer.Elapsed.TotalSeconds, 1)
    if ($IsGitHubActions) { Write-Host "$Name - done in ${seconds}s" } else { Write-Host "    done in ${seconds}s" }
    return $result
}

function Invoke-GitHubApi([string]$Path, [switch]$AllowNotFound)
{
    # gh takes care of authentication (gh auth login locally, GH_TOKEN in CI)
    $output = & gh api $Path 2>&1
    $exitCode = $LASTEXITCODE
    $stdout = ($output | Where-Object { $_ -isnot [System.Management.Automation.ErrorRecord] }) -join "`n"
    if ($exitCode -eq 0) { return $stdout | ConvertFrom-Json }

    $stderr = ($output | Where-Object { $_ -is [System.Management.Automation.ErrorRecord] }) -join "`n"
    $match = [regex]::Match("$stderr`n$stdout", 'HTTP (\d{3})')
    $status = $match.Success ? [int]$match.Groups[1].Value : 0

    if ($AllowNotFound -and ($status -eq 404 -or $status -eq 422)) { return $null }
    switch ($status)
    {
        401 { Fail 'GitHub CLI is not authenticated.' $GhHint }
        { $_ -eq 403 -or $_ -eq 429 } { Fail "GitHub API request was refused (HTTP $status, likely rate limited): $Path" }
        0 { Fail "GitHub API request failed: $Path ($($stderr.Trim()))" }
        default { Fail "GitHub API request failed with HTTP ${status}: $Path" }
    }
}

function Invoke-Download([string]$Url, [string]$OutFile)
{
    Write-Host "    from $Url"
    Invoke-WebRequest -Uri $Url -OutFile $OutFile -MaximumRetryCount 3 -RetryIntervalSec 5
    Write-Host "    $([math]::Round((Get-Item $OutFile).Length / 1MB, 1)) MB"
}

function Read-Properties([string]$Path)
{
    $result = @{}
    foreach ($line in Get-Content -LiteralPath $Path)
    {
        if ($line -match '^\s*([^#=\s][^=]*?)\s*=\s*(.*?)\s*$') { $result[$Matches[1]] = $Matches[2] }
    }
    return $result
}

function Get-DependencyPins([string]$Repo, [string]$GitRef)
{
    $pins = @{}
    foreach ($dep in $Dependencies)
    {
        $content = Invoke-GitHubApi "repos/$Repo/contents/$($dep.Path)?ref=$([uri]::EscapeDataString($GitRef))" -AllowNotFound
        if ($null -eq $content) { $pins[$dep.Name] = $null; continue }
        if ($dep.Kind -eq 'submodule')
        {
            $pins[$dep.Name] = ($content.type -eq 'submodule') ? $content.sha : $null
        }
        else
        {
            $text = [System.Text.Encoding]::UTF8.GetString([System.Convert]::FromBase64String($content.content))
            $match = [regex]::Match($text, '(?m)^\s*version\s*=\s*(\S+)')
            $pins[$dep.Name] = $match.Success ? $match.Groups[1].Value : $null
        }
    }
    return $pins
}

$TagCache = @{}
function Format-DependencyPin($Dep, [string]$Pin)
{
    if ([string]::IsNullOrEmpty($Pin)) { return 'unknown' }
    if ($Dep.Kind -ne 'submodule') { return $Pin }
    # Submodule pins are commit SHAs; show the release tag when the commit is one
    if (-not $TagCache.ContainsKey($Dep.Repo))
    {
        $map = @{}
        foreach ($tag in @(Invoke-GitHubApi "repos/$($Dep.Repo)/tags?per_page=100" -AllowNotFound)) { if ($tag) { $map[$tag.commit.sha] = $tag.name } }
        $TagCache[$Dep.Repo] = $map
    }
    $name = $TagCache[$Dep.Repo][$Pin]
    return $name ? $name : $Pin.Substring(0, 8)
}

function Move-ExistingPlugin([string]$TargetDir, [string]$BackupDir)
{
    # Returns $true when a plugin directory was moved to $BackupDir (and can be restored from there)
    if (-not (Test-Path -LiteralPath $TargetDir)) { return $false }
    $existing = Get-Item -LiteralPath $TargetDir -Force
    if ($existing.LinkType)
    {
        # Delete only the junction/symlink itself, never the directory it points to
        Write-Host "    removing existing link to $($existing.Target)"
        $existing.Delete()
        return $false
    }
    try
    {
        # A single rename on the same volume: moves the whole directory or nothing. (Move-Item and
        # Remove-Item -Recurse process items one by one, leaving a half-moved plugin if a file is locked.)
        [System.IO.Directory]::Move($TargetDir, $BackupDir)
        return $true
    }
    catch
    {
        Fail "Could not move the existing Plugins/Sentry out of the way: $($_.Exception.InnerException.Message ?? $_.Exception.Message)" `
            'Close the Unreal Editor and any running builds, then retry. The existing plugin was not changed.'
    }
}

function Write-GitHubOutputs([hashtable]$Outputs)
{
    if (-not $IsGitHubActions -or -not $env:GITHUB_OUTPUT) { return }
    foreach ($key in $Outputs.Keys) { "$key=$($Outputs[$key])" | Out-File -FilePath $env:GITHUB_OUTPUT -Append -Encoding utf8 }
}

function Write-GitHubSummary([string[]]$Lines)
{
    if (-not $IsGitHubActions -or -not $env:GITHUB_STEP_SUMMARY) { return }
    $Lines | Out-File -FilePath $env:GITHUB_STEP_SUMMARY -Append -Encoding utf8
}

function Invoke-Setup
{
    # 1. Validate arguments (offline)
    $Ref = $Ref.Trim()
    $LocalPath = $LocalPath.Trim()

    $projectInput = [string]::IsNullOrWhiteSpace($ProjectDir) ? (Join-Path $PSScriptRoot '..') : $ProjectDir
    if (-not (Test-Path -LiteralPath $projectInput -PathType Container))
    {
        Fail "Project directory not found: $projectInput"
    }
    $projectRoot = (Resolve-Path -LiteralPath $projectInput).Path
    $uproject = Get-ChildItem -LiteralPath $projectRoot -Filter '*.uproject' -File | Select-Object -First 1
    if (-not $uproject)
    {
        Fail "No .uproject found in $projectRoot" 'Pass -ProjectDir pointing at the Unreal project root.'
    }

    if ($Ref -and $LocalPath)
    {
        Fail 'Only one of -Ref and -LocalPath can have a value.' `
            '-Ref combines unreleased sources with the pinned release binaries; -LocalPath links a local checkout instead.'
    }
    $mode = $Ref ? 'ref' : $LocalPath ? 'local' : 'pinned'

    if ($mode -eq 'ref' -and $Ref -notmatch '^[\w./-]+$')
    {
        Fail "Invalid -Ref '$Ref'." 'Expected a branch name or commit SHA.'
    }

    $localPluginDir = $null
    if ($mode -eq 'local')
    {
        if (-not (Test-Path -LiteralPath $LocalPath -PathType Container)) { Fail "-LocalPath not found: $LocalPath" }
        $localRoot = (Resolve-Path -LiteralPath $LocalPath).Path
        $localPluginDir = (Join-Path $localRoot 'plugin-dev'), $localRoot |
            Where-Object { Test-Path -LiteralPath (Join-Path $_ 'Sentry.uplugin') } | Select-Object -First 1
        if (-not $localPluginDir)
        {
            Fail "-LocalPath '$localRoot' is not a sentry-unreal checkout (no plugin-dev/Sentry.uplugin)."
        }
    }

    $pin = $null
    $repo = $null
    if ($mode -ne 'local')
    {
        if (-not (Get-Command gh -ErrorAction SilentlyContinue)) { Fail 'GitHub CLI (gh) is required but was not found.' $GhHint }
        $pinPath = Join-Path $projectRoot $PinFileName
        if (-not (Test-Path -LiteralPath $pinPath)) { Fail "Pin file not found: $pinPath" }
        $pin = Read-Properties $pinPath
        if (-not $pin.ContainsKey('version') -or $pin['version'] -notmatch '^\d+\.\d+\.\d+(-[0-9A-Za-z.-]+)?$')
        {
            Fail "$PinFileName has no valid 'version' entry."
        }
        if (-not $pin.ContainsKey('repo') -or $pin['repo'] -notmatch '^https://github\.com/([^/]+/[^/]+?)(\.git)?/?$')
        {
            Fail "$PinFileName has no valid 'repo' entry (expected https://github.com/<owner>/<repo>)."
        }
        $repo = $Matches[1]
    }

    # 2. Resolve (online, nothing changed yet)
    $releaseVersion = $null
    $asset = $null
    $refSha = $null
    $refLabel = $null
    $mismatches = @()

    if ($mode -ne 'local')
    {
        $releaseVersion = $pin['version']
        $release = Invoke-GitHubApi "repos/$repo/releases/tags/$releaseVersion" -AllowNotFound
        if ($null -eq $release)
        {
            $recent = @(Invoke-GitHubApi "repos/$repo/releases?per_page=5") | ForEach-Object { $_.tag_name }
            Fail "Release $releaseVersion not found in $repo." "Recent releases: $($recent -join ', ')"
        }
        $assetName = "sentry-unreal-$releaseVersion.zip"
        $asset = @($release.assets) | Where-Object { $_.name -eq $assetName } | Select-Object -First 1
        if (-not $asset)
        {
            Fail "Release $releaseVersion has no '$assetName' asset." "Available assets: $((@($release.assets) | ForEach-Object { $_.name }) -join ', ')"
        }
    }

    if ($mode -eq 'ref')
    {
        $escapedRef = ($Ref -split '/' | ForEach-Object { [uri]::EscapeDataString($_) }) -join '/'
        $commit = Invoke-GitHubApi "repos/$repo/commits/$escapedRef" -AllowNotFound
        if ($null -eq $commit)
        {
            Fail "Ref '$Ref' not found in $repo." 'Expected an existing branch name or commit SHA.'
        }
        $refSha = $commit.sha
        $shortSha = $refSha.Substring(0, 8)
        $isCommit = $Ref -match '^[0-9a-fA-F]{7,40}$' -and $refSha.StartsWith($Ref.ToLower())
        $refLabel = $isCommit ? "commit $shortSha" : "$Ref @ $shortSha"

        try
        {
            $releasePins = Get-DependencyPins $repo $releaseVersion
            $refPins = Get-DependencyPins $repo $refSha
            foreach ($dep in $Dependencies)
            {
                $a = $releasePins[$dep.Name]; $b = $refPins[$dep.Name]
                if ($a -ne $b) { $mismatches += "$($dep.Name) $(Format-DependencyPin $dep $a) -> $(Format-DependencyPin $dep $b)" }
            }
        }
        catch [SetupError]
        {
            Write-Warn "Could not compare SDK versions between $refLabel and release ${releaseVersion}: $($_.Exception.Message)"
        }
    }

    # Plan
    $pinnedText = "release $releaseVersion (pinned in $PinFileName)"
    $sourcesText = switch ($mode)
    {
        'ref' { "$refLabel ($repo)" }
        'local' { "local $localPluginDir" }
        default { $pinnedText }
    }
    $binariesText = ($mode -eq 'local') ? 'from the local checkout' : $pinnedText
    $targetDir = Join-Path $projectRoot 'Plugins/Sentry'

    Write-Host ''
    Write-Host 'Sentry SDK setup' -ForegroundColor Green
    Write-Host "  Project   $($uproject.FullName)"
    Write-Host "  Sources   $sourcesText"
    Write-Host "  Binaries  $binariesText"
    Write-Host "  Target    $targetDir"
    Write-Host ''

    if ($mismatches.Count -gt 0)
    {
        Write-Warn ("$refLabel pins different SDK versions than release ${releaseVersion}: $($mismatches -join '; '). " +
            'Plugin sources may not compile against the release binaries.')
    }

    # 3. Assemble in a staging directory, then swap it in
    # Staging lives inside the project (same volume, so moves are renames) and outside Plugins/
    # (so a leftover copy never gets picked up as a second Sentry plugin).
    $staging = Join-Path $projectRoot 'Intermediate/SentrySdkSetup'
    if (Test-Path -LiteralPath $staging) { Remove-Item -LiteralPath $staging -Recurse -Force }
    New-Item -ItemType Directory -Path $staging -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $projectRoot 'Plugins') -Force | Out-Null

    try
    {
        if ($mode -eq 'local')
        {
            $linkType = $IsWindows ? 'Junction' : 'SymbolicLink'
            $install = {
                New-Item -ItemType $linkType -Path $targetDir -Target $localPluginDir | Out-Null
                Write-Host "    $linkType -> $localPluginDir"
            }
        }
        else
        {
            $releaseZip = Join-Path $staging $asset.name
            $newPluginDir = Join-Path $staging 'release'
            Invoke-Step "Downloading $($asset.name)" { Invoke-Download $asset.browser_download_url $releaseZip }
            Invoke-Step "Extracting $($asset.name)" { [System.IO.Compression.ZipFile]::ExtractToDirectory($releaseZip, $newPluginDir) }

            if ($mode -eq 'ref')
            {
                $sourceZip = Join-Path $staging 'source.zip'
                $sourceDir = Join-Path $staging 'source'
                Invoke-Step "Downloading sources at $shortSha" { Invoke-Download "https://github.com/$repo/archive/$refSha.zip" $sourceZip }
                $newPluginDir = Invoke-Step "Combining plugin-dev @ $shortSha with release $releaseVersion binaries" {
                    # GitHub source archives have a single top-level directory: <repo>-<sha>/
                    [System.IO.Compression.ZipFile]::ExtractToDirectory($sourceZip, $sourceDir)
                    $refPluginDir = Join-Path (Get-ChildItem -LiteralPath $sourceDir -Directory | Select-Object -First 1).FullName 'plugin-dev'
                    if (-not (Test-Path -LiteralPath (Join-Path $refPluginDir 'Sentry.uplugin'))) { Fail "No plugin-dev/Sentry.uplugin at $refLabel." }
                    $refThirdParty = Join-Path $refPluginDir 'Source/ThirdParty'
                    if (Test-Path -LiteralPath $refThirdParty) { Remove-Item -LiteralPath $refThirdParty -Recurse -Force }
                    [System.IO.Directory]::Move((Join-Path $newPluginDir 'Source/ThirdParty'), $refThirdParty)
                    Write-Host "    Source/ThirdParty taken from release $releaseVersion"
                    $refPluginDir
                }
            }

            if (-not (Test-Path -LiteralPath (Join-Path $newPluginDir 'Sentry.uplugin'))) { Fail 'Assembled plugin has no Sentry.uplugin.' }
            $install = { [System.IO.Directory]::Move($newPluginDir, $targetDir) }
        }

        Invoke-Step 'Replacing Plugins/Sentry' {
            $backupDir = Join-Path $staging 'previous'
            $backedUp = Move-ExistingPlugin $targetDir $backupDir
            try
            {
                & $install
            }
            catch
            {
                if ($backedUp) { [System.IO.Directory]::Move($backupDir, $targetDir) }
                Fail "Could not install the new plugin into Plugins/Sentry: $($_.Exception.InnerException.Message ?? $_.Exception.Message)" `
                    ($backedUp ? 'The previous plugin was restored.' : '')
            }
        }
    }
    finally
    {
        Remove-Item -LiteralPath $staging -Recurse -Force -ErrorAction SilentlyContinue
    }

    # Report
    $pluginVersion = (Get-Content -LiteralPath (Join-Path $targetDir 'Sentry.uplugin') -Raw | ConvertFrom-Json).VersionName
    $isPinned = $mode -eq 'pinned'

    Write-Host ''
    Write-Host "Done: Sentry $pluginVersion installed to $targetDir in $([math]::Round($TotalTimer.Elapsed.TotalSeconds, 1))s" -ForegroundColor Green
    if ($Warnings.Count -gt 0) { Write-Host "  with $($Warnings.Count) warning(s), see above" -ForegroundColor Yellow }

    Write-GitHubOutputs @{
        'is-pinned'            = $isPinned.ToString().ToLower()
        'sdk-mismatch'         = ($mismatches.Count -gt 0).ToString().ToLower()
        'sdk-mismatch-details' = $mismatches -join '; '
    }

    $summary = @(
        '### Sentry SDK'
        ''
        '| | |'
        '|---|---|'
        "| Sources | $sourcesText |"
        "| Binaries | $binariesText |"
        "| Plugin version | $pluginVersion |"
        "| Pinned | $($isPinned ? 'yes' : 'no') |"
    )
    foreach ($warning in $Warnings) { $summary += @('', '> [!WARNING]', "> $warning") }
    Write-GitHubSummary $summary
}

try
{
    Invoke-Setup
    exit 0
}
catch
{
    $message = $_.Exception.Message
    $hint = ($_.Exception -is [SetupError]) ? $_.Exception.Hint : ''
    if ($IsGitHubActions)
    {
        # Rendered inline in the log as well as on the run page
        Write-Host ("::error title=Sentry SDK setup::$message $hint".TrimEnd())
    }
    else
    {
        Write-Host ''
        Write-Host "ERROR: $message" -ForegroundColor Red
        if ($hint) { Write-Host "       $hint" -ForegroundColor Red }
    }
    if ($_.Exception -isnot [SetupError]) { Write-Host $_.ScriptStackTrace -ForegroundColor DarkGray }
    exit 1
}
