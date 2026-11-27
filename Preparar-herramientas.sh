#!/bin/bash
# Arma la carpeta «herramientas» con los programas que usa la app:
#   yt-dlp.exe                 de sus releases oficiales (se verifica el SHA-256)
#   ffmpeg.exe, ffprobe.exe    de yt-dlp/FFmpeg-Builds, con sus DLL (se verifica con su checksums.sha256)
#   qjs.exe                    QuickJS, para los desafíos de YouTube (bellard.org no publica sumas: la del
#                              .zip va fijada acá abajo; si se cambia de versión, poner la nueva)
#   rsync.exe, ssh.exe         de los paquetes de MSYS2, con sus DLL
# Uso: Preparar-herramientas.sh  (askpass.exe lo agrega Compilar-en-Windows.bat)
set -euo pipefail
cd "$(dirname "$(realpath "$0")")"
SALIDA=$PWD/herramientas
QJS=quickjs-win-x86_64-2026-06-04.zip
# SHA-256 del .zip de QuickJS (26/11/2026; su qjs.exe es el mismo que se usaba desde el 14/11)
QJS_SHA256=8d10e75796656f49a3797e2c14465bd67c1f085dba505cf0c8d8a14bf5b19cb4
FFMPEG=https://github.com/yt-dlp/FFmpeg-Builds/releases/latest/download
MSYS=https://repo.msys2.org/msys/x86_64
TEMPORAL=$(mktemp -d)
trap 'rm -rf "$TEMPORAL"' EXIT
rm -rf "$SALIDA"; mkdir -p "$SALIDA"; cd "$TEMPORAL"

curl -fsSL -o "$SALIDA/yt-dlp.exe" https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe
curl -fsSL https://github.com/yt-dlp/yt-dlp/releases/latest/download/SHA2-256SUMS \
    | grep ' yt-dlp.exe$' | sed "s|yt-dlp.exe\$|$SALIDA/yt-dlp.exe|" | sha256sum -c --quiet

curl -fsSL -o ffmpeg.zip "$FFMPEG/ffmpeg-master-latest-win64-gpl-shared.zip"
curl -fsSL "$FFMPEG/checksums.sha256" | grep ' ffmpeg-master-latest-win64-gpl-shared.zip$' \
    | sed 's|ffmpeg-master-latest-win64-gpl-shared.zip$|ffmpeg.zip|' | sha256sum -c --quiet
bsdtar -xf ffmpeg.zip --strip-components=2 -C "$SALIDA" '*/bin/*'
rm -f "$SALIDA/ffplay.exe"
bsdtar -xOf ffmpeg.zip '*/LICENSE.txt' > "$SALIDA/ffmpeg-LICENSE.txt"

curl -fsSL -o qjs.zip "https://bellard.org/quickjs/binary_releases/$QJS"
echo "$QJS_SHA256  qjs.zip" | sha256sum -c --quiet
bsdtar -xf qjs.zip -C "$SALIDA" qjs.exe libwinpthread-1.dll

# Los paquetes de MSYS2: cada DLL que pida un programa se saca del paquete que la tiene
curl -fsSL -o msys.db "$MSYS/msys.db"
python3 -I "$OLDPWD/armar-msys.py" msys.db "$MSYS" "$SALIDA" rsync.exe ssh.exe
ls -la "$SALIDA" | cut -c25-; du -sh "$SALIDA"
