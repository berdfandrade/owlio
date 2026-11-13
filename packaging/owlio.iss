; Instalador do Owlio para Windows (Inno Setup 6).
; Espera o programa já instalado em dist\ por:  cmake --install build --prefix "%CD%\dist"   (caminho absoluto)
; Gerar:  iscc /DAppVersion=1.0 packaging\owlio.iss   (o .exe sai em packaging\Output)

#ifndef AppVersion
  #define AppVersion "1.0"
#endif
#define AppName "Owlio"
#define AppExe "bin\owlio.exe"

[Setup]
; Nunca mude o AppId: é por ele que o Windows reconhece atualizações e desinstalação.
AppId={{345BE5AC-543D-4637-B1C2-5E0BE1601FB0}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=Owlio
AppPublisherURL=https://github.com/berdfandrade/owlio
AppCopyright=Owlio contributors, GPL-3.0
VersionInfoVersion={#AppVersion}
VersionInfoProductName={#AppName}
VersionInfoDescription={#AppName} Setup
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
OutputBaseFilename=Owlio-Setup-{#AppVersion}
SetupIconFile=..\resources\owlio.ico
UninstallDisplayIcon={app}\{#AppExe}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Instala só para o usuário atual, sem pedir administrador, a menos que ele escolha.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ChangesAssociations=yes

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "pt_BR"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"

[CustomMessages]
en.AssocFiles=Open .fountain and .fdx scripts with Owlio
pt_BR.AssocFiles=Abrir roteiros .fountain e .fdx com o Owlio
en.Screenplay=Screenplay
pt_BR.Screenplay=Roteiro

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "assoc"; Description: "{cm:AssocFiles}"

[Files]
Source: "..\dist\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Registry]
Root: HKA; Subkey: "Software\Classes\Owlio.Script"; ValueType: string; ValueName: ""; ValueData: "{#AppName} {cm:Screenplay}"; Flags: uninsdeletekey; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\Owlio.Script\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#AppExe},0"; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\Owlio.Script\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#AppExe}"" ""%1"""; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.fountain\OpenWithProgids"; ValueType: string; ValueName: "Owlio.Script"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.fdx\OpenWithProgids"; ValueType: string; ValueName: "Owlio.Script"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\Applications\owlio.exe\SupportedTypes"; ValueType: string; ValueName: ".fountain"; ValueData: ""; Flags: uninsdeletekey; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\Applications\owlio.exe\SupportedTypes"; ValueType: string; ValueName: ".fdx"; ValueData: ""; Tasks: assoc

[Run]
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent
