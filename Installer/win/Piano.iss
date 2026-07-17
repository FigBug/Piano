; Piano installer (Inno Setup)

#define MyAppName "Piano"
#define MyAppCompany "SocaLabs"
#define MyAppPublisher "SocaLabs"
#define MyAppCopyright "2026 SocaLabs"
#define MyAppURL "https://socalabs.com/"
#define MyAppVersion GetStringFileInfo("bin\VST3\Piano.vst3\Contents\x86_64-win\Piano.vst3", "ProductVersion")
#define MyDefaultDirName "{commoncf64}\VST3"

[Setup]
AppID={{E3301DCA-A6DC-42FD-A3EF-AD0B339A0DCC}
AppName={#MyAppCompany} {#MyAppName} {#MyAppVersion}
AppVerName={#MyAppCompany} {#MyAppName} {#MyAppVersion}
AppVersion={#MyAppVersion}
AppCopyright={#MyAppCopyright}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={#MyDefaultDirName}
DisableProgramGroupPage=yes
OutputDir=.\bin
OutputBaseFilename=Piano
Compression=lzma/ultra
SolidCompression=true
ShowLanguageDialog=auto
LicenseFile=..\EULA.rtf
InternalCompressLevel=ultra
MinVersion=0,6.1.7600
FlatComponentsList=false
AppendDefaultDirName=false
AlwaysShowDirOnReadyPage=yes
DirExistsWarning=no
DisableDirPage=yes
DisableWelcomePage=no
DisableReadyPage=no
DisableReadyMemo=no
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
VersionInfoVersion={#MyAppVersion}
VersionInfoCompany={#MyAppPublisher}
VersionInfoCopyright={#MyAppCopyright}
VersionInfoProductName={#MyAppCompany} {#MyAppName} {#MyAppVersion} (64-bit)
VersionInfoProductVersion={#MyAppVersion}
VersionInfoProductTextVersion={#MyAppVersion}
UsePreviousGroup=False
Uninstallable=no
PrivilegesRequired=admin

[Languages]
Name: english; MessagesFile: compiler:Default.isl

[Components]
Name: "vst";       Description: "VST plug-in";   Types: full custom; Flags: checkablealone
Name: "vst3";      Description: "VST3 plug-in";  Types: full custom; Flags: checkablealone
Name: "clap";      Description: "CLAP plug-in";  Types: full custom; Flags: checkablealone
Name: "crashreporter"; Description: "Crash reporter (shared component, only updated if newer)"; Types: full custom; Flags: checkablealone


[InstallDelete]
Type: files;          Name: "{commoncf64}\VST2\Piano.dll";   Components: vst
Type: filesandordirs; Name: "{commoncf64}\VST3\Piano.vst3"; Components: vst3
Type: files;          Name: "{commoncf64}\CLAP\Piano.clap"; Components: clap


[Files]
Source: "bin\VST\Piano.dll";    DestDir: "{commoncf64}\VST2";                     Flags: ignoreversion overwritereadonly; Components: vst
Source: "bin\VST3\Piano.vst3\*"; DestDir: "{commoncf64}\VST3\Piano.vst3\"; Flags: ignoreversion overwritereadonly recursesubdirs; Components: vst3
Source: "bin\CLAP\Piano.clap";   DestDir: "{commoncf64}\CLAP";                    Flags: ignoreversion overwritereadonly; Components: clap
; CrashReporter app → C:\Program Files\Rabien Software\Crash Reporter, plus this
; plugin's registration JSON → C:\ProgramData\Rabien Software\Crash Reporter\Plugins.
; Shared across plugins: the app is only updated if newer and never removed on
; uninstall; the registration JSON is always installed and never removed.
Source: "bin\CrashReporter\CrashReporter.exe"; DestDir: "{commonpf}\Rabien Software\Crash Reporter"; Flags: skipifsourcedoesntexist uninsneveruninstall; Components: crashreporter
Source: "bin\CrashReporter\piano.json";        DestDir: "{commonappdata}\Rabien Software\Crash Reporter\Plugins"; Flags: ignoreversion uninsneveruninstall

