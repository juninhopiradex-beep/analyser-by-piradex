; Instalador Windows do ANALYSER by Piradex (Inno Setup 6)
; Compilado no GitHub Actions: ISCC /DVersao=1.0.0 installer\windows.iss

#ifndef Versao
  #define Versao "1.0.0"
#endif

[Setup]
AppId={{6E3B1C52-8F4A-4C1D-9B27-4A1F0D2E7C91}
AppName=ANALYSER by Piradex
AppVersion={#Versao}
AppVerName=ANALYSER by Piradex {#Versao}
AppPublisher=Piradex
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=no
UninstallFilesDir={commonpf64}\Piradex\ANALYSER
UninstallDisplayName=ANALYSER by Piradex
OutputDir=..\out-win
OutputBaseFilename=ANALYSER-by-Piradex-{#Versao}-Windows-Setup
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
Source: "..\build\Analyser_artefacts\Release\VST3\ANALYSER by Piradex.vst3\*"; \
  DestDir: "{commoncf64}\VST3\ANALYSER by Piradex.vst3"; \
  Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\ANALYSER by Piradex.vst3"
