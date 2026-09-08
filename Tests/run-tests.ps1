param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio Installer was not found.' }
$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (-not $msbuild) { throw 'MSBuild was not found.' }

& $msbuild (Join-Path $projectRoot 'ImageProcessor.sln') /m "/p:Configuration=$Configuration" /p:Platform=x64 /verbosity:minimal /nologo
if ($LASTEXITCODE -ne 0) { throw 'Application build failed.' }
& $msbuild (Join-Path $PSScriptRoot 'ImageProcessorTests.vcxproj') /m "/p:Configuration=$Configuration" /p:Platform=x64 /verbosity:minimal /nologo
if ($LASTEXITCODE -ne 0) { throw 'Test build failed.' }

$artifacts = Join-Path $projectRoot 'build\tests\artifacts'
& (Join-Path $projectRoot "build\tests\$Configuration\ImageProcessorTests.exe") $artifacts
if ($LASTEXITCODE -ne 0) { throw 'C++ tests failed.' }

$cliDirectory = Join-Path $projectRoot 'build\tests\cli'
New-Item -ItemType Directory -Force -Path $cliDirectory | Out-Null
$executable = Join-Path $projectRoot "x64\$Configuration\ImageProcessor.exe"

function Invoke-Case([string[]]$Arguments, [int]$Expected) {
    # Windows PowerShell 5.1은 native stderr를 ErrorRecord로 변환하므로 종료 코드로 판정한다.
    $ErrorActionPreference = 'Continue'
    & $executable @Arguments *> 'last-run.txt'
    $actual = $LASTEXITCODE
    if ($actual -ne $Expected) {
        throw "Expected exit $Expected, got $actual. $(Get-Content 'last-run.txt' -Raw)"
    }
}

Push-Location $cliDirectory
try {
    Invoke-Case -Arguments @('--help') -Expected 0
    foreach ($resource in Get-ChildItem (Join-Path $projectRoot 'Resource\*.bmp')) {
        foreach ($filter in @('grayscale', 'threshold')) {
            $common = @('--input', $resource.FullName, '--filter', $filter, '--log', 'test.log')
            Invoke-Case -Arguments ($common + @('--output', 'serial.bmp', '--threads', '1')) -Expected 0
            Invoke-Case -Arguments ($common + @('--output', 'parallel.bmp', '--threads', '4')) -Expected 0
            if ((Get-FileHash 'serial.bmp').Hash -ne (Get-FileHash 'parallel.bmp').Hash) {
                throw "Serial/parallel result mismatch: $($resource.Name), $filter"
            }
        }
    }

    $inputFile = Join-Path $projectRoot 'Resource\1_astronaut.bmp'
    $base = @('--input', $inputFile, '--output', 'result with spaces.bmp')
    Invoke-Case -Arguments ($base + @('--filter', 'threshold:128', '--log', 'test.log')) -Expected 0
    $aliasHash = (Get-FileHash 'result with spaces.bmp').Hash
    Invoke-Case -Arguments ($base + @('--filter', 'threshold', '--threshold', '128', '--log', 'test.log')) -Expected 0
    if ((Get-FileHash 'result with spaces.bmp').Hash -ne $aliasHash) { throw 'Threshold syntax mismatch.' }
    $success = Get-Content 'test.log' -Tail 1
    if ($success -notmatch 'status=success.*threshold=128.*actual_threads=[1-9].*filter_ms=.*elapsed_ms=') {
        throw 'Success log fields are missing.'
    }

    foreach ($value in @('-1', '256', '128abc', '999999999999999999999')) {
        Invoke-Case -Arguments ($base + @('--filter', 'threshold', '--threshold', $value)) -Expected 4
    }
    foreach ($value in @('0', '-1', '257', '2x')) {
        Invoke-Case -Arguments ($base + @('--filter', 'grayscale', '--threads', $value)) -Expected 4
    }
    Invoke-Case -Arguments ($base + @('--filter', 'grayscale', '--threshold', '128')) -Expected 4
    Invoke-Case -Arguments ($base + @('--filter', 'threshold:128', '--threshold', '64')) -Expected 4
    Invoke-Case -Arguments ($base + @('--filter', 'grayscale', '-f', 'threshold')) -Expected 4
    Invoke-Case -Arguments ($base + @('--filter', 'grayscale', '--threads')) -Expected 4
    Invoke-Case -Arguments @('--input', '--output', 'unused.bmp', '--filter', 'grayscale') -Expected 4
    Invoke-Case -Arguments ($base + @('--pipeline', 'grayscale,threshold:128')) -Expected 4
    Invoke-Case -Arguments ($base + @('--filter', 'blur', '--log', 'test.log')) -Expected 3
    Invoke-Case -Arguments @('-i', 'missing.bmp', '-o', 'unused.bmp', '-f', 'grayscale', '--log', 'test.log') -Expected 2
    if ((Get-Content 'test.log' -Tail 1) -notmatch 'status=failure.*Cannot open file') {
        throw 'Failure log is missing.'
    }
    Invoke-Case -Arguments @('-i', $inputFile, '-o', '.', '-f', 'grayscale', '--log', 'test.log') -Expected 2
    Invoke-Case -Arguments ($base + @('--filter', 'grayscale', '--log', '.')) -Expected 1

    $originalHash = (Get-FileHash -LiteralPath $inputFile).Hash
    Invoke-Case -Arguments ($base + @('--filter', 'grayscale', '--log', $inputFile)) -Expected 1
    if ((Get-FileHash -LiteralPath $inputFile).Hash -ne $originalHash) { throw 'Input changed on log conflict.' }
    $outputHash = (Get-FileHash 'result with spaces.bmp').Hash
    Invoke-Case -Arguments ($base + @('--filter', 'grayscale', '--log', 'RESULT WITH SPACES.BMP')) -Expected 1
    if ((Get-FileHash 'result with spaces.bmp').Hash -ne $outputHash) { throw 'Output changed on log conflict.' }
    # 인자 오류에서 기본 로그 경로를 열면 같은 이름의 입력 파일을 훼손할 수 있다.
    Copy-Item -LiteralPath $inputFile -Destination 'ImageProcessor.log' -Force
    Invoke-Case -Arguments @('-i', 'ImageProcessor.log', '-o', 'unused.bmp', '-f', 'grayscale', '--threads', '0') -Expected 4
    if ((Get-FileHash 'ImageProcessor.log').Hash -ne $originalHash) { throw 'Argument error changed an input file.' }
    Write-Output 'PASS: all 5 sample BMPs, serial/parallel equality, CLI errors, success/failure logs, path conflicts'
}
finally {
    Pop-Location
}
