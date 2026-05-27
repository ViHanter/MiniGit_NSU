$ErrorActionPreference = "Stop"

$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$SrcDir = Join-Path $ProjectRoot "src"
$ExePath = Join-Path $SrcDir "coo.exe"

function Build-MiniGit {
    Push-Location $SrcDir
    try {
        & gcc -I. -Wall -Wextra -o coo.exe `
            main.c `
            core/commit.c `
            core/repo.c `
            core/tree.c `
            core/staging.c `
            core/repo_storage.c `
            utils.c
        if ($LASTEXITCODE -ne 0) {
            throw "Build failed with exit code $LASTEXITCODE"
        }
    }
    finally {
        Pop-Location
    }
}

function Invoke-Coo {
    param(
        [Parameter(Mandatory = $true)][string]$WorkingDirectory,
        [Parameter(Mandatory = $true)][string]$UserProfile,
        [Parameter(Mandatory = $true)][string[]]$Commands
    )

    New-Item -ItemType Directory -Force -Path $WorkingDirectory | Out-Null
    New-Item -ItemType Directory -Force -Path $UserProfile | Out-Null

    $psi = [System.Diagnostics.ProcessStartInfo]::new()
    $psi.FileName = $ExePath
    $psi.WorkingDirectory = $WorkingDirectory
    $psi.UseShellExecute = $false
    $psi.RedirectStandardInput = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.StandardOutputEncoding = [System.Text.Encoding]::UTF8
    $psi.StandardErrorEncoding = [System.Text.Encoding]::UTF8
    $psi.Environment["USERPROFILE"] = $UserProfile
    $psi.Environment["HOME"] = $UserProfile

    $process = [System.Diagnostics.Process]::Start($psi)
    $inputBytes = [System.Text.Encoding]::UTF8.GetBytes(($Commands -join "`n") + "`n")
    $process.StandardInput.BaseStream.Write($inputBytes, 0, $inputBytes.Length)
    $process.StandardInput.BaseStream.Close()

    $stdout = $process.StandardOutput.ReadToEnd()
    $stderr = $process.StandardError.ReadToEnd()
    $process.WaitForExit()

    if ($process.ExitCode -ne 0) {
        throw "coo.exe exited with $($process.ExitCode)`n$stdout`n$stderr"
    }

    return $stdout + $stderr
}

function Assert-Contains {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Needle
    )

    if (-not $Text.Contains($Needle)) {
        throw "Expected output to contain '$Needle'."
    }
}

function Assert-PathExists {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (-not (Test-Path $Path)) {
        throw "Expected path to exist: $Path"
    }
}

function Assert-PathMissing {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (Test-Path $Path) {
        throw "Expected path to be missing: $Path"
    }
}

Build-MiniGit

$RunRoot = Join-Path $env:TEMP ("minigit-tests-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $RunRoot | Out-Null

Write-Host "Running tests in $RunRoot"

$cwdRepo = Join-Path $RunRoot "cwd-repo"
$cwdHome = Join-Path $RunRoot "home-cwd"
New-Item -ItemType Directory -Force -Path $cwdRepo | Out-Null
Set-Content -Path (Join-Path $cwdRepo "readme.txt") -Value "hello from cwd" -NoNewline
$out = Invoke-Coo $cwdRepo $cwdHome @(
    "init",
    "add readme.txt",
    "commit cwd commit",
    "exists readme.txt",
    "rm readme.txt",
    "commit remove readme",
    "exists readme.txt",
    "exit"
)
Assert-Contains $out "[OK] Repository initialized in '.'"
Assert-Contains $out "[OK] Commit 2 created: 'cwd commit'"
Assert-Contains $out "[INFO] File 'readme.txt': [FOUND]"
Assert-Contains $out "[OK] File 'readme.txt' removed from working directory"
Assert-Contains $out "[OK] File 'readme.txt' staged for removal"
Assert-Contains $out "[OK] Commit 3 created: 'remove readme'"
Assert-Contains $out "[INFO] File 'readme.txt': [NOT FOUND]"
Assert-PathExists (Join-Path $cwdRepo ".minigit\HEAD")
Assert-PathMissing (Join-Path $cwdRepo "readme.txt")

$driverDir = Join-Path $RunRoot "driver"
$pathHome = Join-Path $RunRoot "home-path"
$repoDir = Join-Path $RunRoot "nested\repo"
New-Item -ItemType Directory -Force -Path $repoDir | Out-Null
Set-Content -Path (Join-Path $repoDir "a.txt") -Value "one" -NoNewline
$out = Invoke-Coo $driverDir $pathHome @(
    "init `"$repoDir`"",
    "add a.txt",
    "commit first",
    "exit"
)
Assert-Contains $out "[OK] Repository initialized in '$repoDir'"
Assert-Contains $out "[OK] Commit 2 created: 'first'"
Assert-PathExists (Join-Path $repoDir ".minigit\HEAD")

$objects = Get-ChildItem (Join-Path $repoDir ".minigit\objects") -Filter "*.blob"
if ($objects.Count -lt 1) {
    throw "Expected blob objects to be stored inside init path."
}

Set-Content -Path (Join-Path $repoDir "b.txt") -Value "two" -NoNewline
$out = Invoke-Coo $driverDir $pathHome @(
    "add b.txt",
    "commit second",
    "rollback 2",
    "exists b.txt",
    "log",
    "exit"
)
Assert-Contains $out "[OK] Repository loaded from '$repoDir'"
Assert-Contains $out "[OK] Commit 3 created: 'second'"
Assert-Contains $out "[OK] Rolled back to commit 2"
Assert-Contains $out "[INFO] File 'b.txt': [NOT FOUND]"
Assert-Contains $out "Commit 2"
Assert-Contains $out "Commit 1"
Assert-PathMissing (Join-Path $repoDir "b.txt")
Assert-PathExists (Join-Path $repoDir "a.txt")

$branchHome = Join-Path $RunRoot "home-branch"
$branchRepo = Join-Path $RunRoot "branch-repo"
New-Item -ItemType Directory -Force -Path $branchRepo | Out-Null
Set-Content -Path (Join-Path $branchRepo "feature.txt") -Value "feature work" -NoNewline
$out = Invoke-Coo $branchRepo $branchHome @(
    "init",
    "branch feature",
    "checkout feature",
    "add feature.txt",
    "commit feature commit",
    "branches",
    "exit"
)
Assert-Contains $out "[OK] Branch 'feature' created"
Assert-Contains $out "[OK] Switched to branch 'feature'"
Assert-Contains $out "[OK] Commit"
Assert-Contains $out "feature [CURRENT]"

$unicodeHome = Join-Path $RunRoot "home-unicode"
$unicodeDriver = Join-Path $RunRoot "unicode-driver"
$unicodeRepoName = -join ([char]0x0448, [char]0x0430, [char]0x0448, [char]0x043A, [char]0x0438)
$unicodeFileName = (-join ([char]0x0445, [char]0x043E, [char]0x0434)) + ".txt"
$unicodeRepo = Join-Path $RunRoot $unicodeRepoName
New-Item -ItemType Directory -Force -Path $unicodeRepo | Out-Null
Set-Content -Path (Join-Path $unicodeRepo $unicodeFileName) -Value "e2-e4" -NoNewline -Encoding UTF8
$out = Invoke-Coo $unicodeDriver $unicodeHome @(
    "init `"$unicodeRepo`"",
    "add $unicodeFileName",
    "commit unicode path",
    "exit"
)
Assert-Contains $out "[OK] Repository initialized in '$unicodeRepo'"
Assert-Contains $out "[OK] File '$unicodeFileName' added to staging"
Assert-Contains $out "[OK] Commit"
Assert-PathExists (Join-Path $unicodeRepo ".minigit\HEAD")
Assert-PathExists (Join-Path $unicodeRepo ".minigit\commits\2.commit")

Write-Host "All MiniGit tests passed."
