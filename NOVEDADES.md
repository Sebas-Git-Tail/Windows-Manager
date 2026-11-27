# Novedades de Windows-Manager

Lo que cambió en cada versión. La app lo muestra la primera vez que se abre una versión nueva y siempre desde
el botón del regalito. Los cambios marcados [Sebastián] solo aparecen en sus equipos.

## 1.7.0 (26/11/2026)
- Respaldo: lo que se borra o se cambia en el servidor ya no se pierde del todo. Queda guardado 30 días en
  «versiones anteriores», y al terminar te dice cuántos archivos fueron ahí.
- Respaldo: ahora también se puede **restaurar**: traer todo, una carpeta o un archivo, del último respaldo o
  de una versión anterior. Nunca borra nada de la computadora, y te pregunta antes de empezar.
- Respaldo: muestra el día y la hora del último respaldo, y en «Qué no se copia» podés sumar carpetas tuyas
  que no quieras mandar al servidor.
- Si el respaldo se corta de golpe, ahora dice que falló (antes decía «cancelado»).
- Sección nueva **Programas**: buscar, instalar y actualizar programas de Windows (con winget). Si a tu
  Windows le falta winget, te explica cómo conseguirlo.
- Sección nueva **Descargas en masa**: pegás una lista de enlaces de archivos y los baja todos. Si ya hay un
  archivo con el mismo nombre, no lo pisa: guarda «nombre (2)».
- Videos y Música: opción **Bajar con el servidor** (el servidor de casa baja con sus cookies y la
  computadora trae los archivos; sirve cuando YouTube pide iniciar sesión), y se puede elegir un archivo
  cookies.txt además de LibreWolf o Firefox.
- La lista de lo descargado queda guardada aunque cierres la app. «Descargar Vídeos» ahora se llama «Videos».
- [Sebastián] Pruebas en tests/ (respaldo, winget, descargas) que corren en Linux y en el CI; el CI sube la
  carpeta armada con windeployqt. Preparar-herramientas.sh verifica el SHA-256 de ffmpeg y QuickJS.

## 1.6.0 (21/11/2026)
- Novedades: la primera vez que abrís una versión nueva te cuenta qué cambió. Siempre están en el botón del
  regalito, arriba a la derecha.
- Si la app se cierra por una falla, manda un informe al servidor (sin tus datos) para que se pueda arreglar.
  También lo podés mandar a mano desde Novedades, con un comentario.

## 1.5.0 (18/11/2026)
- Modo claro y oscuro con un botón (se recuerda).

## 1.4.0 (18/11/2026)
- La ventana se vuelve a abrir del mismo tamaño, y maximizada o en pantalla completa si lo estaba.

## 1.3.0 (17/11/2026)
- Interfaz nueva, igual en todas las apps.

## 1.2.1 (16/11/2026)
- Arreglos: las descargas de otras apps ya no se rompen, «Apps» ya no cierra la app y los botones estándar
  están en español.

## 1.2.0 (16/11/2026)
- Se actualiza sola: avisa cuando hay una versión nueva y la instala.
- Sección «Apps» para instalar las otras apps de Sebastián.

## 1.1.0 (13/11/2026)
- Descargar Vídeos: elegir calidad, códecs, pistas de audio y subtítulos (todo en un MKV).
- Respaldo: solo se conecta al servidor de verdad (huella fija) y toma sus datos de servidor.json.
- Botón «Actualizar yt-dlp», la versión a la vista y aviso de versión nueva.
- Instalador para Windows (sin pedir permiso de administrador).

## 1.0.0 (12/11/2026)
- Primera versión: descargar videos y música con yt-dlp y respaldar la carpeta personal en el servidor.
