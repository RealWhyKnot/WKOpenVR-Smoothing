;--------------------------------
; OpenVR-Smoothing installer.
;
; Installs the OpenVR-Smoothing.exe overlay (config UI) and bundles the
; shared OpenVR-PairDriver tree -- the driver is auto-installed if missing
; or refreshed in place. enable_smoothing.flag is dropped into the driver
; tree's resources/ folder so the driver wires up the skeletal hooks at
; SteamVR startup.
;
; Calibration flag handling: we never touch enable_calibration.flag. If SC
; is also installed it owns that flag; this installer's uninstaller leaves
; it alone so SC keeps working.

;--------------------------------
;Includes

	!include "MUI2.nsh"
	!include "FileFunc.nsh"

;--------------------------------
;General

	!ifndef ARTIFACTS_BASEDIR
		!define ARTIFACTS_BASEDIR "..\build\artifacts\Release"
	!endif
	!ifndef DRIVER_BASEDIR
		!define DRIVER_BASEDIR "..\lib\OpenVR-PairDriver\build\driver_openvrpair"
	!endif

	Name "OpenVR-Smoothing"
	OutFile "OpenVR-Smoothing-Setup.exe"
	InstallDir "$PROGRAMFILES64\OpenVR-Smoothing"
	InstallDirRegKey HKLM "Software\OpenVR-Smoothing\Main" ""
	RequestExecutionLevel admin
	ShowInstDetails show

	!ifndef VERSION
		!define VERSION "0.1.0.0"
	!endif
	VIProductVersion "${VERSION}"
	VIAddVersionKey /LANG=1033 "ProductName"     "OpenVR-Smoothing"
	VIAddVersionKey /LANG=1033 "FileDescription" "OpenVR-Smoothing Installer"
	VIAddVersionKey /LANG=1033 "LegalCopyright"  "MIT, https://github.com/RealWhyKnot/OpenVR-Smoothing"
	VIAddVersionKey /LANG=1033 "FileVersion"     "${VERSION}"
	VIAddVersionKey /LANG=1033 "ProductVersion"  "${VERSION}"

;--------------------------------
;Variables

VAR vrRuntimePath
VAR upgradeInstallation

;--------------------------------
;Interface Settings

	!define MUI_ABORTWARNING

;--------------------------------
;Pages

	!insertmacro MUI_PAGE_LICENSE "..\LICENSE"
	!define MUI_PAGE_CUSTOMFUNCTION_PRE dirPre
	!insertmacro MUI_PAGE_DIRECTORY
	!insertmacro MUI_PAGE_INSTFILES

	!insertmacro MUI_UNPAGE_CONFIRM
	!insertmacro MUI_UNPAGE_INSTFILES

;--------------------------------
;Languages

	!insertmacro MUI_LANGUAGE "English"

;--------------------------------
;Functions

Function dirPre
	StrCmp $upgradeInstallation "true" 0 +2
		Abort
FunctionEnd

Function .onInit
	StrCpy $upgradeInstallation "false"

	ReadRegStr $R0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenVR-Smoothing" "UninstallString"
	StrCmp $R0 "" done

	FindWindow $0 "Qt5QWindowIcon" "SteamVR Status"
	StrCmp $0 0 +3
		MessageBox MB_OK|MB_ICONEXCLAMATION \
			"SteamVR is still running. Cannot install while SteamVR holds the shared driver open.$\nPlease close SteamVR and try again."
		Abort

	MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION \
		"OpenVR-Smoothing is already installed.$\n$\nClick OK to upgrade or Cancel to abort." \
		IDOK upgrade
	Abort

	upgrade:
		StrCpy $upgradeInstallation "true"
	done:
FunctionEnd

;--------------------------------
;Helpers

!macro ResolveRuntimePath
	nsExec::ExecToStack 'powershell -NoProfile -Command "try { ((Get-Content -Raw \"$env:LOCALAPPDATA\openvr\openvrpaths.vrpath\" | ConvertFrom-Json).runtime)[0] } catch { exit 1 }"'
	Pop $0
	Pop $vrRuntimePath
	StrCmp $0 "0" +3
		MessageBox MB_OK|MB_ICONEXCLAMATION "Could not locate the SteamVR runtime path. Make sure SteamVR has been launched at least once.$\n$\nDetails: $vrRuntimePath"
		Abort
	Push $vrRuntimePath
	Call TrimNewlines
	Pop $vrRuntimePath
	DetailPrint "SteamVR runtime path: $vrRuntimePath"
!macroend

Function TrimNewlines
	Exch $R0
	Push $R1
	Push $R2
	StrCpy $R1 0
	loop:
		IntOp $R1 $R1 - 1
		StrCpy $R2 $R0 1 $R1
		StrCmp $R2 "$\r" loop
		StrCmp $R2 "$\n" loop
		IntOp $R1 $R1 + 1
		IntCmp $R1 0 noTrim
		StrCpy $R0 $R0 $R1
	noTrim:
	Pop $R2
	Pop $R1
	Exch $R0
FunctionEnd

;--------------------------------
;Installer

Section "Install" SecInstall

	StrCmp $upgradeInstallation "true" 0 noupgrade
		DetailPrint "Removing previous installation..."
		ExecWait '"$INSTDIR\Uninstall.exe" /S _?=$INSTDIR'
		Delete $INSTDIR\Uninstall.exe
	noupgrade:

	SetOutPath "$INSTDIR"
	File "..\LICENSE"
	File /oname=README.md "..\README.md"
	File "${ARTIFACTS_BASEDIR}\OpenVR-Smoothing.exe"

	!insertmacro ResolveRuntimePath

	; Legacy migration: if the predecessor OpenVR-Smoothing release shipped
	; under <SteamVR>\drivers\01fingersmoothing\, rename its manifest to
	; .disabled-by-pair-migration so SteamVR ignores it. Folder kept in place
	; (rollback = rename-the-manifest-back).
	IfFileExists "$vrRuntimePath\drivers\01fingersmoothing\driver.vrdrivermanifest" 0 nolegacy
		Rename "$vrRuntimePath\drivers\01fingersmoothing\driver.vrdrivermanifest" \
		       "$vrRuntimePath\drivers\01fingersmoothing\driver.vrdrivermanifest.disabled-by-pair-migration"
		DetailPrint "Disabled legacy 01fingersmoothing driver."
	nolegacy:

	; Lay the shared driver tree down. Auto-install / refresh -- if SC's
	; installer ran first, this overwrites the same files with the bundled
	; copy from this installer. Driver-DLL version-skew is intentionally
	; last-write-wins for v1 of the installer; a future build-time
	; comparison gate can refuse-to-downgrade.
	SetOutPath "$vrRuntimePath\drivers\01openvrpair"
	File "${DRIVER_BASEDIR}\driver.vrdrivermanifest"
	SetOutPath "$vrRuntimePath\drivers\01openvrpair\resources"
	File "${DRIVER_BASEDIR}\resources\driver.vrresources"
	SetOutPath "$vrRuntimePath\drivers\01openvrpair\resources\settings"
	File "${DRIVER_BASEDIR}\resources\settings\default.vrsettings"
	SetOutPath "$vrRuntimePath\drivers\01openvrpair\bin\win64"
	File "${DRIVER_BASEDIR}\bin\win64\driver_openvrpair.dll"

	; Drop our feature flag. SC's enable_calibration.flag, if present, is
	; left untouched.
	FileOpen $0 "$vrRuntimePath\drivers\01openvrpair\resources\enable_smoothing.flag" w
	FileWrite $0 "enabled"
	FileClose $0
	DetailPrint "Dropped enable_smoothing.flag in $vrRuntimePath\drivers\01openvrpair\resources\"

	WriteRegStr HKLM "Software\OpenVR-Smoothing\Main"   "" $INSTDIR
	WriteRegStr HKLM "Software\OpenVR-Smoothing\Driver" "" $vrRuntimePath
	WriteRegStr HKLM "Software\OpenVR-Smoothing\Main"   "Version" "${VERSION}"

	WriteUninstaller "$INSTDIR\Uninstall.exe"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenVR-Smoothing" "DisplayName"     "OpenVR-Smoothing"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenVR-Smoothing" "DisplayVersion"  "${VERSION}"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenVR-Smoothing" "Publisher"       "RealWhyKnot"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenVR-Smoothing" "UninstallString" "$\"$INSTDIR\Uninstall.exe$\""

	CreateShortCut "$SMPROGRAMS\OpenVR-Smoothing.lnk" "$INSTDIR\OpenVR-Smoothing.exe"

SectionEnd

;--------------------------------
;Uninstaller

Section "Uninstall"
	FindWindow $0 "Qt5QWindowIcon" "SteamVR Status"
	StrCmp $0 0 +3
		MessageBox MB_OK|MB_ICONEXCLAMATION "SteamVR is still running. Please close SteamVR and try again."
		Abort

	; Remove our flag file. The shared driver tree itself stays in place
	; -- if SC's enable_calibration.flag is present, the tree is still
	; needed; if not, the user can run the OpenVR-PairDriver uninstaller
	; (or just delete the folder) to remove the orphan tree.
	ReadRegStr $vrRuntimePath HKLM "Software\OpenVR-Smoothing\Driver" ""
	StrCmp $vrRuntimePath "" skipFlag
	Delete "$vrRuntimePath\drivers\01openvrpair\resources\enable_smoothing.flag"
	DetailPrint "Removed enable_smoothing.flag."
	IfFileExists "$vrRuntimePath\drivers\01openvrpair\resources\enable_calibration.flag" 0 noOrphanWarn
		Goto skipFlag
	noOrphanWarn:
		DetailPrint "No remaining feature flags in 01openvrpair\resources\. The shared driver is now inert."
		DetailPrint "Run the OpenVR-PairDriver uninstaller or delete <SteamVR>\drivers\01openvrpair\ to remove the now-dormant driver tree."
	skipFlag:

	Delete "$INSTDIR\LICENSE"
	Delete "$INSTDIR\README.md"
	Delete "$INSTDIR\OpenVR-Smoothing.exe"
	Delete "$INSTDIR\Uninstall.exe"
	RMDir "$INSTDIR"

	Delete "$SMPROGRAMS\OpenVR-Smoothing.lnk"

	DeleteRegKey HKLM "Software\OpenVR-Smoothing\Driver"
	DeleteRegKey HKLM "Software\OpenVR-Smoothing\Main"
	DeleteRegKey /ifempty HKLM "Software\OpenVR-Smoothing"
	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenVR-Smoothing"

SectionEnd
