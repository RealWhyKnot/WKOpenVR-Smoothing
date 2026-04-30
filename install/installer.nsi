;--------------------------------
;Include Modern UI

	!include "MUI2.nsh"

;--------------------------------
;General

	!define ARTIFACTS_BASEDIR "..\bin\artifacts\Release"
	!define DRIVER_RESDIR "..\bin\driver_01fingersmoothing"

	Name "FingerSmoothing"
	OutFile "FingerSmoothing.exe"
	InstallDir "$PROGRAMFILES64\FingerSmoothing"
	InstallDirRegKey HKLM "Software\FingerSmoothing\Main" ""
	RequestExecutionLevel admin
	ShowInstDetails show

	; Define the version number. Wrapped in !ifndef so the release CI workflow can
	; override it via `makensis /DVERSION=...` to match the git tag's version.
	!ifndef VERSION
		!define VERSION "0.1.0.0"
	!endif
	; Set version information
	VIProductVersion "${VERSION}"
	VIAddVersionKey /LANG=1033 "ProductName" "FingerSmoothing"
	VIAddVersionKey /LANG=1033 "FileDescription" "FingerSmoothing Installer"
	VIAddVersionKey /LANG=1033 "LegalCopyright" "Open source at https://github.com/RealWhyKnot/FingerSmoothing"
	VIAddVersionKey /LANG=1033 "FileVersion" "${VERSION}"
	VIAddVersionKey /LANG=1033 "ProductVersion" "${VERSION}"

;--------------------------------
;Variables

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

	ReadRegStr $R0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\FingerSmoothing" "UninstallString"
	StrCmp $R0 "" done

	; If SteamVR is already running, display a warning message and exit
	FindWindow $0 "Qt5QWindowIcon" "SteamVR Status"
	StrCmp $0 0 +3
		MessageBox MB_OK|MB_ICONEXCLAMATION \
			"SteamVR is still running. Cannot install this software.$\nPlease close SteamVR and try again."
		Abort

	MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION \
		"FingerSmoothing is already installed. $\n$\nClick `OK` to upgrade the \
		existing installation or `Cancel` to cancel this upgrade." \
		IDOK upgrade
	Abort

	upgrade:
		StrCpy $upgradeInstallation "true"
	done:
FunctionEnd

;--------------------------------
;Installer Sections

Section "Install" SecInstall

	StrCmp $upgradeInstallation "true" 0 noupgrade
		DetailPrint "Uninstall previous version..."
		ExecWait '"$INSTDIR\Uninstall.exe" /S _?=$INSTDIR'
		Delete $INSTDIR\Uninstall.exe
		Goto afterupgrade

	noupgrade:

	afterupgrade:

	SetOutPath "$INSTDIR"

	File "..\LICENSE"
	File /oname=README.md "..\README.md"
	File "${ARTIFACTS_BASEDIR}\FingerSmoothing.exe"
	File "..\lib\openvr\bin\win64\openvr_api.dll"
	File "..\src\overlay\manifest.vrmanifest"
	File "..\src\overlay\icon.png"
	File "..\src\overlay\taskbar_icon.png"

	; VC++ Redistributable. Only bundled when the release workflow has fetched
	; vcredist_x64.exe into install/ and passed /DBUNDLE_VCREDIST to makensis.
	!ifdef BUNDLE_VCREDIST
		File "vcredist_x64.exe"
		ExecWait '"$INSTDIR\vcredist_x64.exe" /install /quiet'
		Delete "$INSTDIR\vcredist_x64.exe"
	!endif

	Var /GLOBAL vrRuntimePath
	nsExec::ExecToStack '"$INSTDIR\FingerSmoothing.exe" -openvrpath'
	Pop $0
	Pop $vrRuntimePath
	DetailPrint "VR runtime path: $vrRuntimePath"

	SetOutPath "$vrRuntimePath\drivers\01fingersmoothing"
	File "${DRIVER_RESDIR}\driver.vrdrivermanifest"
	SetOutPath "$vrRuntimePath\drivers\01fingersmoothing\resources"
	File "${DRIVER_RESDIR}\resources\driver.vrresources"
	SetOutPath "$vrRuntimePath\drivers\01fingersmoothing\resources\settings"
	File "${DRIVER_RESDIR}\resources\settings\default.vrsettings"
	SetOutPath "$vrRuntimePath\drivers\01fingersmoothing\bin\win64"
	File "${DRIVER_RESDIR}\bin\win64\driver_01fingersmoothing.dll"

	WriteRegStr HKLM "Software\FingerSmoothing\Main" "" $INSTDIR
	WriteRegStr HKLM "Software\FingerSmoothing\Driver" "" $vrRuntimePath

	WriteUninstaller "$INSTDIR\Uninstall.exe"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\FingerSmoothing" "DisplayName" "FingerSmoothing"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\FingerSmoothing" "UninstallString" "$\"$INSTDIR\Uninstall.exe$\""

	CreateShortCut "$SMPROGRAMS\FingerSmoothing.lnk" "$INSTDIR\FingerSmoothing.exe"

SectionEnd

;--------------------------------
;Uninstaller Section

Section "Uninstall"
	; If SteamVR is already running, display a warning message and exit
	FindWindow $0 "Qt5QWindowIcon" "SteamVR Status"
	StrCmp $0 0 +3
		MessageBox MB_OK|MB_ICONEXCLAMATION \
			"SteamVR is still running. Cannot uninstall this software.$\nPlease close SteamVR and try again."
		Abort

	Var /GLOBAL vrRuntimePath2
	ReadRegStr $vrRuntimePath2 HKLM "Software\FingerSmoothing\Driver" ""
	DetailPrint "VR runtime path: $vrRuntimePath2"
	Delete "$vrRuntimePath2\drivers\01fingersmoothing\driver.vrdrivermanifest"
	Delete "$vrRuntimePath2\drivers\01fingersmoothing\resources\driver.vrresources"
	Delete "$vrRuntimePath2\drivers\01fingersmoothing\resources\settings\default.vrsettings"
	Delete "$vrRuntimePath2\drivers\01fingersmoothing\bin\win64\driver_01fingersmoothing.dll"
	Delete "$vrRuntimePath2\drivers\01fingersmoothing\bin\win64\fingersmoothing_driver.log"
	RMdir "$vrRuntimePath2\drivers\01fingersmoothing\resources\settings"
	RMdir "$vrRuntimePath2\drivers\01fingersmoothing\resources\"
	RMdir "$vrRuntimePath2\drivers\01fingersmoothing\bin\win64\"
	RMdir "$vrRuntimePath2\drivers\01fingersmoothing\bin\"
	RMdir "$vrRuntimePath2\drivers\01fingersmoothing\"

	Delete "$INSTDIR\LICENSE"
	Delete "$INSTDIR\README.md"
	Delete "$INSTDIR\FingerSmoothing.exe"
	Delete "$INSTDIR\openvr_api.dll"
	Delete "$INSTDIR\manifest.vrmanifest"
	Delete "$INSTDIR\icon.png"
	Delete "$INSTDIR\taskbar_icon.png"

	DeleteRegKey HKLM "Software\FingerSmoothing\Main"
	DeleteRegKey HKLM "Software\FingerSmoothing\Driver"
	DeleteRegKey HKLM "Software\FingerSmoothing"
	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\FingerSmoothing"

	Delete "$SMPROGRAMS\FingerSmoothing.lnk"
SectionEnd
