# check on which os we are running
# After check, the Os-Informations are availibe in the PS-Env.
if ($PSVersionTable.PSVersion.Major -lt 6.0) {
  switch ($([System.Environment]::OSVersion.Platform)) 
  { 'Win32NT' 
    { 
      New-Variable -Option Constant -Name IsWindows -Value $True -ErrorAction SilentlyContinue
      New-Variable -Option Constant -Name IsLinux -Value $false -ErrorAction SilentlyContinue
      New-Variable -Option Constant -Name IsMacOs -Value $false -ErrorAction SilentlyContinue
    }
  }
}
$script:IsLinuxEnv = (Get-Variable -Name "IsLinux" -ErrorAction Ignore) -and $IsLinux
$script:IsMacOSEnv = (Get-Variable -Name "IsMacOS" -ErrorAction Ignore) -and $IsMacOS
$script:IsWinEnv = !$IsLinuxEnv -and !$IsMacOSEnv

if ($IsLinuxEnv) { Write-Host -ForegroundColor Green "We are on Linux Build Enviroment" }
if ($IsMacOSEnv ) { Write-Host -ForegroundColor Green "We are on MacOS Build Enviroment" }
if ($IsWinEnv ) { Write-Host -ForegroundColor Green "We are on Windows Build Enviroment" }

# we assume, we start this script in projects "restore" directory
$currentDir = Get-Location

# Go one directory back, to get the project dir.
Set-Location ..
$projectDir = Get-Location

# Get the files in sub dir /lib
$subprojects = Get-ChildItem lib
# Just for listing the Informations
#Write-Host $subprojects

# We are now Outside of the project directory
Set-Location ..

#Now go to all files listened in der lib directory (where the dependended source repositories will be linked)
#ToDo - Create a dependency file that includes information on dependent repositories, such as the symbolic link name and target Git repository.
foreach ($subproject in $subprojects) {
  #Obtain the basename of the files/folders. Reason here is that in some cases, for example if you wokring on different OS and FS, 
  #we need to extract only the basenames on the linked files/folders
  if ($IsMacOS -or $IsLinux) { $ProjectFile = Get-Item $subproject | Select-Object BaseName }
  else { $ProjectFile = Get-Item $projectDir"\lib\"$subproject | Select-Object BaseName }
  
  # Check for emtpy entries
  if( ![string]::IsNullOrWhitespace($ProjectFile.BaseName) ) {
    if ($ProjectFile.BaseName -ne "README") {
      # Check and clone the depended OpenKNX git repos.
      $GitPath = $ProjectFile.BaseName+"\.git"
      if (Test-Path $GitPath) { Write-Host -ForegroundColor Yellow "-"$ProjectFile.BaseName"- Target repo already exists. Skip cloning." ([Char]0x221A) }
      else 
      {
        if ($ProjectFile.BaseName -eq "knx") {
          $GitClone= "https://github.com/thelsing/knx.git"
        } else { 
          $GitClone= "https://github.com/OpenKNX/"+$ProjectFile.BaseName+".git" 
        }
        Write-Host -ForegroundColor Yellow "-"$ProjectFile.BaseName"- Target does not exists - cloning."$GitClone
        git clone $GitClone
      }
      if (!$?) { exit 1 }

      $CreateSymLink = 1
      
      # Check if there is a valid symlink
      if( (Get-Item $subproject -ErrorAction SilentlyContinue) -and 
          (Test-Path -Path (Get-Item $subproject ).ToString() -ErrorAction SilentlyContinue))
      {
        $symlink = Get-Item $subproject
        if ($IsMacOS -or $IsLinux) {
          # Info - the symlink here are differend handled
          $symlink = (Get-Item $subproject).Target 
          $linkTarget = "../../"+$ProjectFile.BaseName.ToString()
        }
        else { 
          $linkTarget = Get-Item ((Get-Location).ToString()+"/"+$ProjectFile.BaseName.ToString()).ToString() 
        }
        
        if ($symlink.ToString() -eq $linkTarget.ToString()) { 
          Write-Host -ForegroundColor Yellow "-"$ProjectFile.BaseName"- A existing and valid symbolic link detected. Skip linking."([Char]0x221A)
          $CreateSymLink = 0
        }
      }
      
      # Create a symlink
      if ($CreateSymLink)
      {
        Write-Host -ForegroundColor Yellow "-"$ProjectFile.BaseName"- No valid symbolic link detected. Creating new symbolic link."
        # Here we have a potential problem case: 
        #  - to create a link, we have to remove the existing file first
        #  - if link creation fails, the file is missed, if we restart the script
        # for robustness, we rename first and delete only if link creation was successful
        if ($IsMacOS -or $IsLinux) { 
          Rename-Item -Path $subproject -NewName tmp-openknx-restore 
        } else { 
          Rename-Item $projectDir"\lib\"$subproject $projectDir"\lib\tmp-openknx-restore" 
        }

        if (!$?) { exit 1 }

        $linkTarget = $ProjectFile.BaseName
        $lnkPath = $projectDir.ToString()+"\lib\"
        $lnkName = $linkTarget
        if ($IsMacOS -or $IsLinux) { New-Item -ItemType SymbolicLink -Path $subproject -Value ..\..\$linkTarget } 
        else { 
          New-Item -Path $lnkPath -Name $lnkName -ItemType Junction -Value $linkTarget -OutVariable output | Out-Null
        }
        if ($?) { 
          # link creation successful, now we can safely delete old renamed file
          Remove-Item $projectDir/lib/tmp-openknx-restore -Recurse -Force
          if (!$?) { exit 1 }
        } else {
          # link creation failed, we restore previous state and leave script
          if ($IsMacOS -or $IsLinux) { Rename-Item -Path $projectDir/lib/tmp-openknx-restore -NewName $ProjectFile.BaseName }
          else { Rename-Item $projectDir/lib/tmp-openknx-restore $projectDir/lib/$subproject }
          exit 1
        }
      }
    }
  }
}

if ($?) 
{
  Write-Host -ForegroundColor Green "
  Everything seems to be fine. The project is in expected state."([Char]0x221A)
  Write-Host -ForegroundColor Blue "
  "([Char]0x2139) "To start a release build, you can now call: .\scripts\Build-Release.ps1
  "
  
}

Set-Location $currentDir
Set-Location ..
if ($IsMacOS -or $IsLinux) { Start-Sleep -Seconds 5 } else { timeout /T 5 }