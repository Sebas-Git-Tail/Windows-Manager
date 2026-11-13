#!/bin/bash
# Arma la carpeta «herramientas» con los programas que usa la app:
#   yt-dlp.exe                 de sus releases oficiales (se verifica el SHA-256)
#   ffmpeg.exe, ffprobe.exe    de yt-dlp/FFmpeg-Builds, con sus DLL
#   qjs.exe                    QuickJS, para los desafíos de YouTube (bellard.org)
#   rsync.exe, ssh.exe         de los paquetes de MSYS2, con sus DLL
# Uso: Preparar-herramientas.sh  (askpass.exe lo agrega Compilar-en-Windows.bat)
set -euo pipefail
cd "$(dirname "$(realpath "$0")")"
SALIDA=$PWD/herramientas
QJS=quickjs-win-x86_64-2026-06-04.zip
MSYS=https://repo.msys2.org/msys/x86_64
TEMPORAL=$(mktemp -d)
trap 'rm -rf "$TEMPORAL"' EXIT
rm -rf "$SALIDA"; mkdir -p "$SALIDA"; cd "$TEMPORAL"

curl -fsSL -o "$SALIDA/yt-dlp.exe" https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe
curl -fsSL https://github.com/yt-dlp/yt-dlp/releases/latest/download/SHA2-256SUMS \
    | grep ' yt-dlp.exe$' | sed "s|yt-dlp.exe\$|$SALIDA/yt-dlp.exe|" | sha256sum -c --quiet

curl -fsSL -o ffmpeg.zip https://github.com/yt-dlp/FFmpeg-Builds/releases/latest/download/ffmpeg-master-latest-win64-gpl-shared.zip
bsdtar -xf ffmpeg.zip --strip-components=2 -C "$SALIDA" '*/bin/*'
rm -f "$SALIDA/ffplay.exe"
bsdtar -xOf ffmpeg.zip '*/LICENSE.txt' > "$SALIDA/ffmpeg-LICENSE.txt"

curl -fsSL -o qjs.zip "https://bellard.org/quickjs/binary_releases/$QJS"
bsdtar -xf qjs.zip -C "$SALIDA" qjs.exe libwinpthread-1.dll

# Los paquetes de MSYS2: cada DLL que pida un programa se saca del paquete que la tiene
curl -fsSL -o msys.db "$MSYS/msys.db"
python3 -I "$OLDPWD/armar-msys.py" msys.db "$MSYS" "$SALIDA" rsync.exe ssh.exe
ls -la "$SALIDA" | cut -c25-; du -sh "$SALIDA"
