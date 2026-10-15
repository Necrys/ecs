<#
.SYNOPSIS
    Formats or checks the ecs sources with clang-format.

.DESCRIPTION
    Thin wrapper around tools/format.py that adapts it to PowerShell
    conventions. The Python script does the actual discovery of clang-format and
    the file walking; this wrapper only forwards the switches and turns the
    result into real error records so that CI and $ErrorActionPreference behave.

    Prefer the Python script directly when a shell-agnostic command is needed.

.PARAMETER Check
    Do not modify anything. Fails when a file needs formatting, which makes the
    command usable as a gate. Exit code 1 signals "formatting required".

.PARAMETER List
    List the files that would be processed and stop.

.PARAMETER Staged
    Restrict to files staged in git. Handy before a commit.

.PARAMETER AnyVersion
    Accept a clang-format whose major version differs from the pinned one. The
    verdict differs between majors, so this is for machines where the pinned
    version is not installed; it prints a warning.

.PARAMETER Paths
    Explicit files or directories to process, relative to the repository root or
    absolute. Directories are searched recursively and filtered to source files.
    When omitted, every tracked source file is processed.

    Note: PowerShell only binds arrays when the script is invoked from inside
    PowerShell. Under `powershell -File` every argument arrives as a plain
    string, so pass a single path there and use tools/format.py when several are
    needed.

.EXAMPLE
    ./tools/format.ps1
    Reformats every source file in place.

.EXAMPLE
    ./tools/format.ps1 -Check
    Reports files whose formatting is wrong without touching them.

.EXAMPLE
    ./tools/format.ps1 -Paths lib/include/sparse_vector.hpp, lib/src
    Reformats a single file and a whole directory.

.OUTPUTS
    Nothing on the pipeline. Uses the console for progress and the exit code to
    report the result: 0 = ok, 1 = check failed, 2 = environment problem.
#>
[CmdletBinding()]
param(
    [switch] $Check,
    [switch] $List,
    [switch] $Staged,
    [switch] $AnyVersion,
    [Alias('Path')]
    [string[]] $Paths
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ExplicitPaths = $Paths

function Get-PythonCommand {
    <#
        Returns an array of the executable and its fixed arguments for the first
        usable Python launcher. Exit code 9009 is what cmd.exe reports for a
        missing command, which is how a failed probe surfaces here.
    #>
    $candidates = @(
        @{ Exe = 'py'; Args = @('-3') },
        @{ Exe = 'python'; Args = @() },
        @{ Exe = 'python3'; Args = @() }
    )

    foreach ( $candidate in $candidates ) {
        $command = Get-Command -Name $candidate.Exe -CommandType Application -ErrorAction SilentlyContinue
        if ( -not $command ) {
            continue
        }

        & $command.Source @($candidate.Args) '--version' *> $null
        if ( $LASTEXITCODE -eq 0 ) {
            return @($command.Source) + $candidate.Args
        }
    }

    return $null
}

function Stop-WithError {
    <#
        Report an environment or usage problem and terminate.

        Write-Error is deliberately not used for fatal paths: with
        $ErrorActionPreference = 'Stop' it raises a terminating error of its
        own, which replaces the intended exit code with 1. Writing to stderr and
        exiting explicitly keeps the documented contract (2 = environment).
    #>
    param(
        [Parameter(Mandatory = $true)][string] $Message,
        [int] $Code = 2
    )

    [Console]::Error.WriteLine($Message)
    exit $Code
}

if ( $Check -and $List ) {
    Stop-WithError -Message 'Check and List are mutually exclusive.'
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$script = Join-Path $repoRoot 'tools/format.py'

if ( -not (Test-Path -LiteralPath $script)) {
    Stop-WithError -Message "Cannot find $script. Run this script from a checkout of the repository."
}

# Resolve explicit paths against the repository root rather than the current
# directory, so that `-Path lib/include/foo.hpp` means the same thing wherever the
# script is invoked from.
$resolvedPaths = @()
if ( $ExplicitPaths ) {
    foreach ( $entry in $ExplicitPaths ) {
        $candidate = $entry
        if ( -not [System.IO.Path]::IsPathRooted($candidate)) {
            $candidate = Join-Path $repoRoot $candidate
        }

        if (Test-Path -LiteralPath $candidate) {
            $resolvedPaths += (Resolve-Path -LiteralPath $candidate).ProviderPath
        }
        else {
            # Report the resolved location, not the raw input: a user who passed
            # a repository-relative path needs to see where it was looked for.
            Stop-WithError -Message "No such file or directory: $candidate"
        }
    }
}

$python = Get-PythonCommand
if ( -not $python ) {
    Stop-WithError -Message 'Python was not found. Install it, or run the equivalent command by hand: clang-format -i <files>'
}

$arguments = @($script)
if ( $Check ) { $arguments += '--check' }
if ( $List ) { $arguments += '--list' }
if ( $Staged ) { $arguments += '--staged' }
if ( $AnyVersion ) { $arguments += '--any-version' }
if ( $resolvedPaths.Count -gt 0 ) {
    $arguments += '--files'
    $arguments += $resolvedPaths
}
if ( $VerbosePreference -eq 'Continue' ) { $arguments += '--verbose' }

& $python[0] @($python[1..($python.Count - 1)]) @arguments
$exitCode = $LASTEXITCODE

if ( $exitCode -ne 0 ) {
    if ( $Check -and $exitCode -eq 1 ) {
        # A formatting verdict rather than a failure: nothing is broken, the
        # code simply needs formatting. Surfaced as a catchable error record.
        Write-Error -Message 'Formatting problems found. Run ./tools/format.ps1 to fix them.' -Category InvalidData
    }
    else {
        Stop-WithError -Message "tools/format.py failed with exit code $exitCode." -Code $exitCode
    }
}

exit $exitCode
