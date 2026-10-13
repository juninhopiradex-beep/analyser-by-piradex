; Instalador Windows do MIXSPACE by Piradex (Inno Setup 6)
; Compilado no GitHub Actions: ISCC /DVersao=0.1.0 installer\windows.iss

#ifndef Versao
  #define Versao "0.1.0"
#endif

[Setup]
AppId={{6DECE589-E05E-4029-ACD5-F86C695C7594}
AppName=MIXSPACE by Piradex
AppVersion={#Versao}
AppVerName=MIXSPACE by Piradex {#Versao}
AppPublisher=Piradex
AppPublisherURL=https://www.beatfreakstudio.com
AppSupportURL=https://www.beatfreakstudio.com
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=no
UninstallFilesDir={commonpf64}\Piradex\MIXSPACE
UninstallDisplayName=MIXSPACE by Piradex
OutputDir=..\out-win
OutputBaseFilename=MIXSPACE-by-Piradex-{#Versao}-Windows-Setup
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
MinVersion=10.0
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
InfoBeforeFile=LEIA-ME-Windows.txt

[Languages]
Name: "pt"; MessagesFile: "compiler:Languages\Portuguese.isl"
Name: "ptbr"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "..\build\Mixspace_artefacts\Release\VST3\MIXSPACE by Piradex.vst3\*"; \
  DestDir: "{commoncf64}\VST3\MIXSPACE by Piradex.vst3"; \
  Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\MIXSPACE by Piradex.vst3"
