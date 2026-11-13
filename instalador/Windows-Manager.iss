; Instalador de Windows-Manager (Inno Setup 6). Lo arma Compilar-en-Windows.bat:
;   ISCC /DVersion=1.1.0 /DCarpeta=<carpeta armada> /DSalida=<carpeta Salida> Windows-Manager.iss
; Se instala para el usuario (sin pedir permiso de administrador) en %LOCALAPPDATA%\Programs: así
; yt-dlp puede actualizarse solo (en «Archivos de programa» no podría escribir su propio .exe).

#ifndef Version
  #define Version "0.0.0"
#endif

[Setup]
AppId={{6B1E6F1C-55D5-4E0B-9C1A-3F1B7E2D9A41}
AppName=Windows-Manager
AppVersion={#Version}
AppVerName=Windows-Manager {#Version}
AppPublisher=Sebas-Git-Tail
AppPublisherURL=https://github.com/Sebas-Git-Tail/Windows-Manager
DefaultDirName={localappdata}\Programs\Windows-Manager
DefaultGroupName=Windows-Manager
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir={#Salida}
OutputBaseFilename=Windows-Manager-{#Version}-instalador
SetupIconFile=..\gui\Logo.ico
UninstallDisplayIcon={app}\Windows-Manager.exe
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
; Si la app está abierta, la cierra antes de reemplazar los archivos
CloseApplications=yes

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Tasks]
Name: "escritorio"; Description: "Crear un acceso directo en el escritorio"; GroupDescription: "Accesos directos:"

[Files]
Source: "{#Carpeta}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{userprograms}\Windows-Manager"; Filename: "{app}\Windows-Manager.exe"
Name: "{userdesktop}\Windows-Manager"; Filename: "{app}\Windows-Manager.exe"; Tasks: escritorio

[Run]
Filename: "{app}\Windows-Manager.exe"; Description: "Abrir Windows-Manager"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; yt-dlp guarda su caché al lado; los respaldos y ajustes del usuario (en AppData) no se tocan
Type: filesandordirs; Name: "{app}\herramientas"
