# Starts the main supervisor loop program, in charge of monitoring
# game server health and restart it when needed (up to a certain amount of restarts)
# and monitor the latest prebuilt release of a git branch (published by the GitHub workflow)
# in order to download and deploy it.

param
(
    [string]$Owner = "MarcJacob",   # GitHub repository owner.
    [string]$Repo = "IronAge.io",   # GitHub repository name.
    [string]$Branch = "release-alpha",
    [string]$DeployPath = (Join-Path $PSScriptRoot ".deploy"), # Holds the active run set, the backup of the previous one and the download staging area.
    [int]$Port = 8080,
    [int]$SleepSeconds = 15,
    [int]$UpdateCheckSeconds = 90, # Minimum time between two checks for a new release.
    [int]$MaxRestarts = 5
)

# Windows PowerShell 5.1 may not negotiate TLS 1.2 by default, which GitHub requires. Also, the progress bar slows down downloads a lot.
[Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
$ProgressPreference = "SilentlyContinue"

Add-Type -AssemblyName System.IO.Compression.FileSystem

$LogDir = Join-Path $PSScriptRoot ".logs" # Build path to logging directory

if (-not (Test-Path $LogDir))
{
    New-Item -ItemType Directory -Path $LogDir | Out-Null # Pipe object creation return to Out-Null
}

$ReleaseUrl = "https://github.com/$Owner/$Repo/releases/download/build-$Branch-latest/IronAge_Win32_Game_Server.zip"
$ActiveDir = Join-Path $DeployPath "active"
$BackupDir = Join-Path $DeployPath "backup"
$StagingDir = Join-Path $DeployPath "staging"
$ExeName = "win32_game_server.exe"

function Write-SupervisorLog
{
    param([string]$Message)
    $line = "[{0}] {1}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"), $Message
    Write-Host $line
    Add-Content -Path (Join-Path $LogDir "supervisor.log") -Value $line
}

# Asks the release asset download URL for its current version identifier (ETag, or Last-Modified if there is none) without downloading it.
# Returns the identifier, or $null on failure.
function Get-RemoteReleaseVersion
{
    try
    {
        $Response = Invoke-WebRequest -Uri $ReleaseUrl -Method Head -UseBasicParsing -ErrorAction Stop
        $Version = [string]$Response.Headers["ETag"]
        if ([string]::IsNullOrEmpty($Version)) { $Version = [string]$Response.Headers["Last-Modified"] }
        if ([string]::IsNullOrEmpty($Version)) { Write-SupervisorLog "Release response had neither ETag nor Last-Modified."; return $null }
        return $Version
    }
    catch
    {
        Write-SupervisorLog ("Error checking latest release: {0}" -f $_.Exception.Message)
        return $null
    }
}

# Downloads and unpacks the latest release into the staging folder, then checks the result looks like a run set.
# Does not touch the running server. Returns the folder holding the unpacked run set, or $null on failure.
function Get-StagedRelease
{
    $ZipPath = Join-Path $StagingDir "release.zip"
    $UnpackDir = Join-Path $StagingDir "unpacked"

    try
    {
        if (Test-Path $StagingDir) { Remove-Item $StagingDir -Recurse -Force -ErrorAction Stop }
        New-Item -ItemType Directory -Path $StagingDir | Out-Null

        Invoke-WebRequest -Uri $ReleaseUrl -OutFile $ZipPath -UseBasicParsing -ErrorAction Stop
        [System.IO.Compression.ZipFile]::ExtractToDirectory($ZipPath, $UnpackDir)
    }
    catch
    {
        Write-SupervisorLog ("Error downloading / unpacking latest release: {0}" -f $_.Exception.Message)
        return $null
    }

    # Zips made by Compress-Archive may store backslashes in entry names. Older .NET versions then unpack them as flat files with
    # backslashes in their names instead of folders. So check that the unpacked layout is the expected one.
    if ((Test-Path (Join-Path $UnpackDir $ExeName)) -eq $false)
    {
        Write-SupervisorLog ("Unpacked release does not contain '{0}' at its root." -f $ExeName)
        return $null
    }

    return $UnpackDir
}

# Closes the Game Server gracefully if it is running, killing it if it doesn't close in time.
function Stop-GameServer
{
    if ($script:ServerProcess -ne $null -and ($script:ServerProcess.HasExited -eq $false))
    {
        Write-SupervisorLog "Closing Game Server..."
        if (($script:ServerProcess.CloseMainWindow() -eq $false) -or ($script:ServerProcess.WaitForExit(5000) -eq $false))
        {
            Write-SupervisorLog "Game Server took too long to close. Killing process."
            $script:ServerProcess.Kill() | Out-Null
            $script:ServerProcess.WaitForExit(5000) | Out-Null
        }
    }

    $script:ServerProcess = $null
}

# Downloads the latest release and replaces the active run set with it (hard swap, previous active run set becomes the backup).
# Returns $true if the new release is now the active one, $false otherwise.
function Install-LatestRelease
{
    $UnpackDir = Get-StagedRelease
    if ($UnpackDir -eq $null) { return $false }

    # Release is staged and looks valid. Take server down, then swap.
    Stop-GameServer

    try
    {
        if (Test-Path $BackupDir) { Remove-Item $BackupDir -Recurse -Force -ErrorAction Stop }
        if (Test-Path $ActiveDir) { Move-Item $ActiveDir $BackupDir -ErrorAction Stop }
        Move-Item $UnpackDir $ActiveDir -ErrorAction Stop
    }
    catch
    {
        # Locked files or folders (antivirus, an explorer window, a leftover process...) end up here. The server stays down until the next successful swap
        # if the active dir was already moved away.
        Write-SupervisorLog ("Error swapping in new release: {0}" -f $_.Exception.Message)
        return $false
    }

    return $true
}

# SUPERVISOR STATE

$LastReleaseVersion = $null # Version identifier of the currently deployed release. Empty at start so first check always deploys.
$LastUpdateCheck = [datetime]::MinValue
$RestartCount = 0 # Can go up to MaxRestarts before the supervisor gives up.

$ServerProcess = $null

Write-SupervisorLog ("Starting IronAge Win32 Game Server Supervisor... Release URL = {0}" -f $ReleaseUrl)

while($true)
{
    # Supervisor main cycle

    # Check for a new release, no more often than UpdateCheckSeconds.
    if (((Get-Date) - $LastUpdateCheck).TotalSeconds -ge $UpdateCheckSeconds)
    {
        $LastUpdateCheck = Get-Date
        $RemoteVersion = Get-RemoteReleaseVersion

        if ($RemoteVersion -ne $null -and $RemoteVersion -ne $LastReleaseVersion)
        {
            Write-SupervisorLog ("Detected new release (version '{0}'). Installing..." -f $RemoteVersion)

            if (Install-LatestRelease)
            {
                # Only remember the version after a successful swap, so failures are retried on next check.
                $LastReleaseVersion = $RemoteVersion
                $RestartCount = 0
            }
        }
    }

    # Start server if it's down.
    if ($RestartCount -lt $MaxRestarts -and ($ServerProcess -eq $null -or $ServerProcess.HasExited -eq $true))
    {
        if ($ServerProcess -ne $null -and $ServerProcess.HasExited)
        {
            Write-SupervisorLog ("Server ended with exit code {0}. Restarting..." -f $ServerProcess.ExitCode)
            $RestartCount = $RestartCount + 1
        }

        $Exe = Join-Path $ActiveDir $ExeName
        if ((Test-Path $Exe) -eq $false)
        {
            Write-SupervisorLog ("Failed to start server. Could not find executable at '{0}'. Awaiting a successful release install..." -f $Exe)
            $ServerProcess = $null
        }
        else
        {
            Write-SupervisorLog "Starting Server..."
            $ServerProcess = Start-Process -FilePath $Exe -ArgumentList "-port=$Port" -WorkingDirectory $ActiveDir -PassThru
        }
    }

    Start-Sleep -Seconds $SleepSeconds
}
