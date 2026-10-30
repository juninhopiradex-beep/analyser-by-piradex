; Instalador Windows do EQMotion by Piradex (Inno Setup 6)
; Compilado no GitHub Actions: ISCC /DVersao=2.0.1 installer\windows.iss

#ifndef Versao
  #define Versao "2.0.1"
#endif

[Setup]
AppId={{B0CC2B9A-AE1A-4809-B494-58A0305814CB}
AppName=EQMotion by Piradex
AppVersion={#Versao}
AppVerName=EQMotion by Piradex {#Versao}
AppPublisher=Piradex
AppPublisherURL=https://www.beatfreakstudio.com
AppSupportURL=https://www.beatfreakstudio.com
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=no
UninstallFilesDir={commonpf64}\Piradex\EQMotion
UninstallDisplayName=EQMotion by Piradex
OutputDir=..\out-win
OutputBaseFilename=EQMotion-by-Piradex-{#Versao}-Windows-Setup
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

[Messages]
WelcomeLabel2=Este assistente vai instalar o EQMotion by Piradex no teu sistema.%n%nProfessional Audio Plugin | Beatfreak Music%nCriador e Desenvolvedor: Ricardo Amorim / Juninho Piradex%nVersao 2.0.1

[Files]
Source: "..\build\EQMotion_artefacts\Release\VST3\EQMotion by Piradex.vst3\*"; \
  DestDir: "{commoncf64}\VST3\EQMotion by Piradex.vst3"; \
  Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\EQMotion by Piradex.vst3"
