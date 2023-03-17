# check on which os we are
if ($PSVersionTable.PSVersion.Major -lt 6.0) {
  switch ($([System.Environment]::OSVersion.Platform)) {
      'Win32NT' { 
          New-Variable -Option Constant -Name IsWindows -Value $True -ErrorAction SilentlyContinue
          New-Variable -Option Constant -Name IsLinux -Value $false -ErrorAction SilentlyContinue
          New-Variable -Option Constant -Name IsMacOs -Value $false -ErrorAction SilentlyContinue
      }
  }
}
$script:IsLinuxEnv = (Get-Variable -Name "IsLinux" -ErrorAction Ignore) -and $IsLinux
$script:IsMacOSEnv = (Get-Variable -Name "IsMacOS" -ErrorAction Ignore) -and $IsMacOS
$script:IsWinEnv = !$IsLinuxEnv -and !$IsMacOSEnv

if ($IsLinuxEnv) { Write-Host -ForegroundColor Yellow "We are on Linux Build Enviroment" }
if ($IsMacOSEnv ) { Write-Host -ForegroundColor Yellow "We are on MacOS Build Enviroment" }
if ($IsWinEnv ) { Write-Host -ForegroundColor Yellow "We are on Windows Build Enviroment" }

# we assume, we start this script in projects "restore" directory
$currentDir = Get-Location
Set-Location ..
$subprojects = Get-ChildItem -File lib
Get-ChildItem -File lib
$projectDir = Get-Location


# we first check, if we are in Admin- or Developer mode and allowed to create symlinks:
if ($IsMacOS -or $IsLinux) {
  New-Item -ItemType SymbolicLink -Path "$projectDir\lib\linktest" -Value "..\restore"
  # cleanup after test
  Remove-Item -Path "$projectDir\lib\linktest" -Force
} else { 
  cmd /c "mklink /D `"$projectDir\lib\linktest`" ..\restore"
  if (!$?) { 
      Write-Host "You need Developer-Mode or Administrator privileges to execute this script!"
      timeout /T 20
      exit 1 
  }
  #cleanup after test
  cmd /c "rmdir `"$projectDir\lib\linktest`""
}
if (!$?) { exit 1 }

Set-Location ..
foreach ($subproject in $subprojects) {
    $cloned = 0
    if ($subproject.Name -eq "knx")
    {
        # the following command may fail, because the project might already have been cloned
        git clone https://github.com/thelsing/knx.git
        if (!$?) { 
            Write-Host -ForegroundColor Yellow "If the above message was 'fatal: ... already exists' then you can ignore this message, the project is already cloned"
        }
        $cloned = 1
    }
    elseif ($subproject.Name -ne "README")
    {
        # the following command may fail, because the project might already have been cloned
        $GitClone= "https://github.com/OpenKNX/"+$subproject.Name+".git" 
        Write-Host -ForegroundColor Yellow $GitClone
        git clone $GitClone
        if (!$?) { 
            Write-Host -ForegroundColor Yellow "If the above message was 'fatal: ... already exists' then you can ignore this message, the project is already cloned"
        }
        $cloned = 1
    }
    if ($cloned)
    {
        # here we have a potential problem case: 
        #  - to create a link with mklink, we have to remove the existing file first
        #  - if link creation fails, the file is missed, if we restart the script
        # for robustness, we rename first and delete only if link creation was successful
        Rename-Item $projectDir/lib/$subproject.Name $projectDir/lib/tmp-openknx-restore
        if (!$?) { exit 1 }
        # this has to be a cmd-shell to work in developer mode
        if ($IsMacOS -or $IsLinux) { New-Item -ItemType SymbolicLink -Path "$projectDir\lib\$subproject.Name" -Value "..\..\$subproject.Name" }
        else { cmd /c "mklink /D `"$projectDir\lib\$subproject`" ..\..\$subproject" }
        

        if ($?) { 
            # link creation successful, now we can safely delete old renamed file
            Remove-Item $projectDir/lib/tmp-openknx-restore
            if (!$?) { exit 1 }
        } else {
            # link creation failed, we restore previous state and leave script
            Rename-Item $projectDir/lib/tmp-openknx-restore $projectDir/lib/$subproject.Name 
            exit 1 
        }
    }
}
if ($subprojects.Count -le 1) {
    Write-Host "Everything is fine, project is already in expected state"
}
Set-Location $currentDir
if ($IsMacOS -or $IsLinux) { Start-Sleep -Seconds 5 } else { timeout /T 20 }