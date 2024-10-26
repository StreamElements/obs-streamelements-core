param(
    [string] $projectDir,
    [string] $outDir
)

if (!$projectDir)
{
    Write-Host "Please provide a value for command line argument -projectDir"
    Exit 1
}

if (!$outDir)
{
    Write-Host "Please provide a value for command line argument -outDir"
    Exit 1
}

try
{
    . $projectDir\Scripts\env.ps1
}
catch
{
    $BUGSPLAT_CLIENT_ID = $Env:BUGSPLAT_CLIENT_ID
    $BUGSPLAT_CLIENT_SECRET = $Env:BUGSPLAT_CLIENT_SECRET
}

if (!$BUGSPLAT_CLIENT_ID)
{
    Write-Host 'Please add "$BUGSPLAT_CLIENT_ID={id}" to .\Scripts\env.ps1 or add BUGSPLAT_CLIENT_ID as an env variable'

    Exit 1
}

if (!$BUGSPLAT_CLIENT_SECRET)
{
    Write-Host 'Please add "$BUGSPLAT_CLIENT_SECRET={secret}" to .\Scripts\env.ps1  or add BUGSPLAT_CLIENT_SECRET as an env variable'

    Exit 1
}

$headerPath = $projectDir + "myConsoleCrasher.h"
$nonCommentedDatabaseDefine = Select-String -Path $headerPath -Pattern "^#define BUGSPLAT_DATABASE"
$nonCommentedDatabaseDefine -match 'BUGSPLAT_DATABASE L"(.*)"'
$database = $matches[1]
if (!$database)
{
    Write-Host "Please add '#define BUGSPLAT_DATABASE L`"{your BugSplat database}`"' to ..\Main.h"
    Exit 1
}
Clear-Variable -Name "matches"

$nonCommentedApplicationDefine = Select-String -Path $headerPath -Pattern "^#define APPLICATION_NAME"
$nonCommentedApplicationDefine -match 'APPLICATION_NAME L"(.*)"'
$appName = $matches[1]
if (!$appName)
{
    Write-Host "Please add '#define APPLICATION_NAME L`"{your application name}`"' to ..\Main.h"
    Exit 1
}
Clear-Variable -Name "matches"

$nonCommentedVersionDefine = Select-String -Path $headerPath -Pattern "^#define APPLICATION_VERSION"
$nonCommentedVersionDefine -match 'APPLICATION_VERSION L"(.*)"'
$appVersion = $matches[1]
if (!$appVersion)
{
    Write-Host "Please add '#define APPLICATION_VERSION L`"{your application name}`"' to ..\Main.h"
    Exit 1
}
Clear-Variable -Name "matches"

$symbolUploadPath = $projectDir + "..\..\Tools\symbol-upload-windows.exe"
Write-Host "Running symbol-upload-windows.exe -b $database -a `"$appName`" -v `"$appVersion`" -i $BUGSPLAT_CLIENT_ID -s ****** -d `"$outDir`" -f `"*.{pdb,exe,dll}`""
Start-Process -NoNewWindow -FilePath $symbolUploadPath -ArgumentList "-b $database -a `"$appName`" -v `"$appVersion`" -i $BUGSPLAT_CLIENT_ID -s $BUGSPLAT_CLIENT_SECRET -d $outDir -f `"*.{pdb,exe,dll}`""