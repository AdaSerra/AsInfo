
param (
    [Parameter(Mandatory = $false)]
    [ValidateSet("Release", "Debug")]
    [string]$Config = "Release",
    [switch]$Delete,
    [switch]$Dll
   # [switch]$Check
    
)

$CaidaPath = @("","","","")
$resCheck = $true
#source dataset caida
<# $file_org = ".as-org2info.txt" # gz
$file_rel_v6 = ".as-rel.v6-stable.txt" #bz2
$file_rel_serial1 = ".as-rel.txt" #bz2
$file_rel_serial2 = ".as-rel2.txt" #bz2 #>

$link1 = "https://publicdata.caida.org/datasets/as-organizations/" #date.filename.gz
$link2 = "https://publicdata.caida.org/datasets/as-relationships/serial-1/" #date.filename.bz2
$link3 = "https://publicdata.caida.org/datasets/as-relationships/serial-2/" #date.filename.bz2

$dataDir = "dataset"

$CaidaFiles = @(
    "as-org2info.txt", "as-rel.txt","as-rel.v6-stable.txt", 
    , "as-rel2.txt"
    )

$dtrstr = Get-Date -Format "yyyyMM"
$dtrstr += "01."

if(-not(Test-Path $dataDir))
{
    mkdir $dataDir
}
Set-Location $dataDir

for ($i = 0; $i -lt 4; $i++)
{
    $filename = $($dtrstr)+($CaidaFiles[$i])
  #  $filepath = Join-Path $Caidadir -ChildPath $filename -Path $PWD 
  if(-not(Test-Path $filename))
  { 
    $link = $null
    if ($i -eq 0)
   { 
    $link ="$link1$filename.gz"
    }
    elseif ($i -eq 3)
    {
        $link ="$link3$filename.bz2"
    }
    else {
        $link ="$link2$filename.bz2"
    }
    Write-Warning "File $filename not present, download at link $link and decomprime it in dataset folder "
    if ($i -eq 0 || $i -eq 1) {
        $resCheck = $false
    }
  }
  $CaidaPath[$i] ="./$dataDir/$filename"
 
}
Set-Location ..

$DefinePath = @(
    '/DORG_FILE_PATH="./dataset/20260401.as-org2info.txt"',
    '/DREL_FILE_S1_PATH="./dataset/20260401.as-rel.txt"',
    '/DREL_FILE_S2_PATH="./dataset/20260401.as-rel2.txt"',
    '/DREL_FILE_V6_PATH="./dataset/20260401.as-rel.v6-stable.txt"'
)

if ($false -eq $resCheck)
{
    Write-Error "Compiling not possible for missing minimum dataset files $($CaidaFiles[0].ToUpper()) and $($CaidaFiles[1].ToUpper())"
    return
}


# 1. sources file compiling
$Sources = @(
    "src\asinfo.cpp",
    "src\types.cpp", "src\parstxt.cpp", "src\ingest.cpp", 
    "dep\mdb.c", "dep\midl.c"
)
if (-not $Dll) {
    $Sources += "src\cli.cpp"
}

if ($Config -eq "Release") {
    $CFlags = @(
        "/O2", "/Ob2", "/Oi", "/Ot", "/Oy", "/GT", "/GL", "/GA", "/Gw", "/Gy",
        "/arch:AVX2", "/favor:INTEL64", "/std:c++20", "/EHsc", "/MT", "/GS-", 
        "/GR-", "/Qspectre-", "/DNDEBUG", "-I./dep"
    )
    $LFlags = @(
        "/LTCG", "/OPT:REF", "/OPT:ICF", "/DEBUG:FASTLINK"
        #  "/LIBPATH:./vcpkg_installed/x64-windows/lib"
    )
}
else {
    $CFlags = @(
        "/Od", "/Zi", "/EHsc", "/MTd", "/std:c++20", "/D_DEBUG", "/RTC1", "/fsanitize=address",
        "-I./dep"
    )
    $LFlags = @(
        "/DEBUG"<# , "/LIBPATH:./vcpkg_installed/x64-windows/lib" #>
    )
}

if ($Dll) {
    $CFlags += "/LD"              
    $CFlags += "/DASINFO_AS_DLL"
    $CFlags += "/DASINFO_EXPORTS"  
    $OutFile = "asinfo.dll"
} else {
    #$CFlags += "/link AsInfo.lib"
    $OutFile = "asinfo.exe"
}

Write-Host "Compiling $Config ($($Dll ? 'DLL' : 'EXE')) ongoing..." -ForegroundColor Cyan

& cl.exe $DefinePath $CFlags $Sources /link $LFlags /OUT:$OutFile



if ($Delete) {
    Write-Host "Deleting old database..."
    if (Test-Path "AsInfo.db") { 
    
        Remove-Item AsInfo.db
    }
    if (Test-Path "AsInfo.db-lock") { 
    
        Remove-Item AsInfo.db-lock
    }
}

if ($LASTEXITCODE -eq 0) {
    Write-Host "Success! Created $OutFile" -ForegroundColor Green
    Get-ChildItem -Include *.obj, *.ilk, *.exp -Path . -Recurse | Remove-Item
}

