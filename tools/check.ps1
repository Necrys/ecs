<#
.SYNOPSIS
    Runs the checks a commit is expected to pass.

.DESCRIPTION
    Thin wrapper around tools/check.py that adapts it to PowerShell
    conventions, in the same shape as tools/format.ps1. The Python script does
    the work; this wrapper only finds a Python launcher, forwards the switches
    and turns the result into real error records so that CI and
    $ErrorActionPreference behave.

.PARAMETER Preset
    Configure/build preset to use. Default: tests-only, which builds the library,
    the header check and the tests.

.PARAMETER Full
    Also build and run the sanitizer configuration and clang-tidy.

.PARAMETER Sanitizers
    Also build and run the sanitizer configuration.

.PARAMETER Tidy
    Also run clang-tidy over the library, the header check and the tests.

.PARAMETER StrictTidy
    Treat clang-tidy findings as a failure. By default they are a report, see
    .clang-tidy.

.PARAMETER FormatAll
    Check the formatting of every tracked file instead of the staged ones.

.PARAMETER NoFormat
    Skip the formatting check.

.PARAMETER NoBuild
    Skip the build (and, with it, the tests).

.PARAMETER NoTests
    Skip ctest.

.PARAMETER Jobs
    Parallel build jobs.

.EXAMPLE
    ./tools/check.ps1
    Formatting of what is staged, build with warnings as errors, and the test suite.

.EXAMPLE
    ./tools/check.ps1 -Full
    The same plus sanitizers and clang-tidy.

.OUTPUTS
    Nothing on the pipeline. Exit code 0 = passed, 1 = a check failed,
    2 = environment problem.
#>
[CmdletBinding()]
param(
    [string] $Preset,
    [switch] $Full,
    [switch] $Sanitizers,
    [switch] $Tidy,
    [switch] $StrictTidy,
    [switch] $FormatAll,
    [switch] $NoFormat,
    [switch] $NoBuild,
    [switch] $NoTests,
    [int] $Jobs = 0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-PythonCommand {
    <#
        Returns an array of the executable and its fixed arguments for the first
        usable Python launcher, the same probe tools/format.ps1 uses.
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
    param(
        [Parameter(Mandatory = $true)][string] $Message,
        [int] $Code = 2
    )

    [Console]::Error.WriteLine($Message)
    exit $Code
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$script = Join-Path $repoRoot 'tools/check.py'

if ( -not (Test-Path -LiteralPath $script)) {
    Stop-WithError -Message "Cannot find $script. Run this script from a checkout of the repository."
}

$python = Get-PythonCommand
if ( -not $python ) {
    Stop-WithError -Message 'Python was not found. Install it, or run the equivalent cmake and ctest commands by hand.'
}

$arguments = @($script)
if ( $Preset ) { $arguments += @('--preset', $Preset) }
if ( $Full ) { $arguments += '--full' }
if ( $Sanitizers ) { $arguments += '--sanitizers' }
if ( $Tidy ) { $arguments += '--tidy' }
if ( $StrictTidy ) { $arguments += '--strict-tidy' }
if ( $FormatAll ) { $arguments += '--format-all' }
if ( $NoFormat ) { $arguments += '--no-format' }
if ( $NoBuild ) { $arguments += '--no-build' }
if ( $NoTests ) { $arguments += '--no-tests' }
if ( $Jobs -gt 0 ) { $arguments += @('--jobs', $Jobs) }

& $python[0] @($python[1..($python.Count - 1)]) @arguments
$exitCode = $LASTEXITCODE

if ( $exitCode -eq 1 ) {
    # A verdict, not an environment problem: something the commit needs is not
    # in place. Surfaced as a catchable error record.
    Write-Error -Message 'Checks failed. See the output above.' -Category InvalidData
}

exit $exitCode
