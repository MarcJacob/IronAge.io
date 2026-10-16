# Starts the main supervisor loop program, in charge of monitoring 
# game server health and restart it when needed (up to a certain amount of restarts)
# and monitor a specific git branch for updates 
# in order to deploy the latest release. 

param
(
    [string]$RepoPath = (Split-Path $PSScriptRoot -Parent), # Default repository path, take PSScriptRoot (script path) 
                                                            # and remove the last folder, giving the parent folder.
    [string]$Branch = "release-alpha",
    [int]$Port = 8080,
    [int]$SleepSeconds = 15,
    [int]$MaxRestarts = 5
)

# Remove previous Dev Setup.
Remove-Item Env:IRONAGE_DEV_SETUP -ErrorAction SilentlyContinue

$LogDir = Join-Path $PSScriptRoot ".logs" # Build path to logging directory

if (-not (Test-Path $LogDir)) 
{
    New-Item -ItemType Directory -Path $LogDir | Out-Null # Pipe object creation return to Out-Null
}

function Write-SupervisorLog 
{
    param([string]$Message)
    $line = "[{0}] {1}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"), $Message
    Write-Host $line
    Add-Content -Path (Join-Path $LogDir "supervisor.log") -Value $line
}

# Updates view of target branch from remote and returns whether an update is available (in which case the return value is the revision / commit SHA-1), or null if not.
function Get-AvailableUpdate
{
    $LocalRevision = git -C $RepoPath rev-parse HEAD
    if ($LASTEXITCODE -ne 0) { Write-SupervisorLog "Error parsing current repository revision."; return $null }

    # Fetch latest and get target revision from target branch.
    git -C $RepoPath fetch origin $Branch | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-SupervisorLog "Error fetching target branch state."; return $null }

    $TargetRevision = git -C $RepoPath rev-parse "origin/$Branch"
    if ($LASTEXITCODE -ne 0) { Write-SupervisorLog "Error parsing target revision."; return $null }

    # Compare current and target revisions.
    if ($LocalRevision -eq $TargetRevision)
    {
        return $null
    }

    return $TargetRevision
}

# Runs a full build on the local workspace.
# Returns $true if successful, $false otherwise.
function Invoke-FullBuild
{
    $BuildScript = Join-Path $RepoPath "scripts\win32_build_all.cmd"
    $BuildLog = Join-Path $LogDir "build.log"

    cmd /c "`"$BuildScript`" < NUL > `"$BuildLog`" 2>&1"    # Run full build command, disable stdin, 
                                                            # pipe its standard output to build log, and standard error to standard output.

    if ($LASTEXITCODE -ne 0) { Write-SupervisorLog "Error performing Win32 Game Server full build."; return $false }

    return $true
}

# SUPERVISOR STATE

$BuildFailed = $null # Indicates whether last attempt to build failed.
$RestartCount = 0 # Can go up to MaxRestarts before the supervisor gives up.

$ServerProcess = $null

Write-SupervisorLog "Starting IronAge Win32 Game Server Supervisor..."

while($true)
{
    if ($BuildFailed -ne $null)
    {
        # Sleep before next cycle.
        Start-Sleep -Seconds $SleepSeconds
    }
    
    # Supervisor main cycle
    $TargetRev = Get-AvailableUpdate
    if (($BuildFailed -eq $null) -or ($TargetRev -ne $null))
    {
        if ($TargetRev -ne $null)
        {
            # Update is available !
            Write-SupervisorLog ("Detected available update. Target revision = {0}." -f $TargetRev)

            # Perform hard reset to target revision.
            git -C $RepoPath reset --hard $TargetRev
            if ($LASTEXITCODE -ne 0) { Write-SupervisorLog "Error performing reset to target revision."; $BuildFailed = $true; continue; }
        }

        # Take server down. Gracefully.
        if ($ServerProcess -ne $null -and ($ServerProcess.HasExited -eq $false))
        {
            Write-SupervisorLog "Closing Game Server..."
            if (($ServerProcess.CloseMainWindow() -eq $false) -or ($ServerProcess.WaitForExit(5000) -eq $false))
            {
                Write-SupervisorLog "Game Server took too long to close. Killing process."
                $ServerProcess.Kill() | Out-Null
            }

            $ServerProcess = $null
        }

        # Perform full build. $BuildFailed will be set with the result.
        Write-SupervisorLog "Starting full build..."
        $BuildFailed = -not(Invoke-FullBuild)

        if ($BuildFailed -eq $false)
        {
            # Fresh build, reset launch attempt count.
            $RestartCount = 0
        }
    }
    
    if ($BuildFailed -eq $true)
    {
        # Build failed, either from last wakeup or after getting a new target revision. Go no further.
        Write-SupervisorLog ("Server build failed. Awaiting new target branch {0} revision before trying again..." -f $Branch)
        continue
    }


    # Start server if it's down.
    if ($RestartCount -lt $MaxRestarts -and ($ServerProcess -eq $null -or $ServerProcess.HasExited -eq $true))
    {
        if ($ServerProcess -ne $null -and $ServerProcess.HasExited)
        {
            Write-SupervisorLog ("Server ended with exit code {0}. Restarting..." -f $ServerProcess.ExitCode)
            $RestartCount = $RestartCount + 1
        }


        $Exe = Join-Path $RepoPath "build\Win32_Game_Server\win32_game_server.exe"
        if ((Test-Path $Exe) -eq $false)
        {
            Write-SupervisorLog ("Failed to start server. Could not find executable at '{0}'." -f $Exe)
            $BuildFailed = $true
            continue;
        }

        $ExeDir = Split-Path $Exe -Parent

        Write-SupervisorLog "Starting Server..."
        $ServerProcess = Start-Process -FilePath $Exe -ArgumentList "-port=$Port" -WorkingDirectory $ExeDir -PassThru
    }
}




