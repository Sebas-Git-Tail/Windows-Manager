# Saca de los paquetes de MSYS2 los programas pedidos y las DLL que necesitan, siguiendo las
# dependencias de cada paquete. Uso: armar-msys.py msys.db URL_BASE SALIDA programa.exe...
import io
import os
import re
import subprocess
import sys
import tarfile
import tempfile
import urllib.request

base, url, salida, programas = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4:]

paquetes = {}
with tarfile.open(base) as tar:
    for miembro in tar.getmembers():
        if miembro.name.endswith("/desc"):
            texto = tar.extractfile(miembro).read().decode() + "\n\n"
            datos = {k: v.split("\n") for k, v in re.findall(r"%(\w+)%\n(.*?)\n\n", texto, re.S)}
            paquetes[datos["NAME"][0]] = datos
            for provisto in datos.get("PROVIDES", []):
                paquetes.setdefault(re.split("[<>=]", provisto)[0], datos)

necesarios = {}
def agregar(nombre):
    datos = paquetes.get(re.split("[<>=]", nombre)[0])
    if datos and datos["NAME"][0] not in necesarios:
        necesarios[datos["NAME"][0]] = datos
        for dependencia in datos.get("DEPENDS", []):
            agregar(dependencia)
for nombre in ("rsync", "openssh", "msys2-runtime"):
    agregar(nombre)

# Todos los .exe y .dll de esos paquetes, en memoria
archivos = {}
licencias = []
with tempfile.TemporaryDirectory() as temporal:
    for nombre, datos in necesarios.items():
        ruta = os.path.join(temporal, datos["FILENAME"][0])
        urllib.request.urlretrieve(f"{url}/{datos['FILENAME'][0]}", ruta)
        lista = subprocess.run(["bsdtar", "-tf", ruta], capture_output=True, text=True, check=True).stdout.split("\n")
        for entrada in lista:
            base_nombre = os.path.basename(entrada)
            if entrada.startswith("usr/bin/") and re.search(r"\.(exe|dll)$", base_nombre):
                contenido = subprocess.run(["bsdtar", "-xOf", ruta, entrada], capture_output=True, check=True).stdout
                archivos.setdefault(base_nombre.lower(), (base_nombre, contenido))

# Se copian solo los programas pedidos y las DLL que importan, una a una
pendientes = [p.lower() for p in programas]
copiados = set()
while pendientes:
    actual = pendientes.pop()
    if actual in copiados or actual not in archivos:
        continue
    nombre, contenido = archivos[actual]
    with open(os.path.join(salida, nombre), "wb") as destino:
        destino.write(contenido)
    copiados.add(actual)
    for importada in set(re.findall(rb"msys-[A-Za-z0-9_.+-]+\.dll", contenido)):
        pendientes.append(importada.decode().lower())
print("MSYS2:", ", ".join(sorted(copiados)))
