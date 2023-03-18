# This script is responsible for all common tasks after a release build is executed


# get all definitions for this project
$settings = scripts/OpenKNX-Build-Settings.ps1 $args[0]

# closeing tag for content.xml
$releaseTarget = "release/data/content.xml"
"    </Products>" >>$releaseTarget
"</Content>" >>$releaseTarget

# add necessary scripts
Copy-Item lib/OGM-Common/scripts/setup/reusable/Readme-Release.txt release/
Copy-Item lib/OGM-Common/scripts/setup/reusable/Build-knxprod.ps1 release/
# Copy-Item scripts/Upload-Firmware*.ps1 release/

# here we might need a better switch in future
# if ($($settings.releaseIndication) -eq "Big") 
# {
#     Remove-Item release/Upload-Firmware-*SAMD*.ps1
# }

# add optional files
if (Test-Path -Path scripts/Readme-Hardware.html -PathType Leaf) {
    Copy-Item scripts/Readme-Hardware.html release/
}

# cleanup
if ($IsMacOS -or $IsLinux) { 
Write-Host -ForegroundColor Yellow "
  Info: The knxprod (KNX Production) file is not created. 
  This OS does not support OpenKNX-Tools to create knxprod file.
  Please use Windows to Build your own KNX production file.

  For more Informations visit: 
  https://github.com/OpenKNX/OpenKNX/wiki/Installation-of-OpenKNX-tools"

Start-Sleep -Seconds 5
} else { Remove-Item "release/$($settings.targetName).knxprod" }

# calculate version string
$appVersion=Select-String -Path "$($settings.knxprod)" -Pattern MAIN_ApplicationVersion
$appVersion=$appVersion.ToString().Split()[-1]
$appMajor=[math]::Floor($appVersion/16)
$appMinor=$appVersion%16
$appRev=Select-String -Path src/main.cpp -Pattern "const uint8_t firmwareRevision"
$appRev=$appRev.ToString().Split()[-1].Replace(";","")
$appVersion="$appMajor.$appMinor"
if ($appRev -gt 0) {
    $appVersion="$appVersion.$appRev"
}

# create dependency file
if (Test-Path -Path dependencies.txt -PathType Leaf) {
    Remove-Item dependencies.txt
}
lib/OGM-Common/scripts/setup/reusable/Build-Dependencies.ps1
Get-Content dependencies.txt

# (re-)create restore directory
lib/OGM-Common/scripts/setup/reusable/Build-Project-Restore.ps1

# create package 
Compress-Archive -Path release/* -DestinationPath Release.zip
Remove-Item -Recurse release/*
Move-Item Release.zip "release/$($settings.targetName)-$($settings.appRelease)-$appVersion.zip"

Write-Host -ForegroundColor Green "Release $($settings.targetName)-$($settings.appRelease)-$appVersion successfully created!"
