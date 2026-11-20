# Publicación inicial del repositorio — Ubuntu en WSL (Windows)

Esta guía describe acciones **a ejecutar manualmente** en el Ubuntu instalado mediante WSL. La herramienta que generó la estructura no publicó el repositorio ni instaló programas en tu equipo. Antes de usar el código, revisar los archivos y los permisos.

**Por qué WSL:** Git, GCC y CMake se ejecutan en Linux; evita mezclar compilación Linux con directorios de Windows. Conserva el proyecto en `~/verificador-epp-obra`, no bajo `/mnt/c`.

## A. Pasar el ZIP al Ubuntu

1. Descargar `verificador-epp-obra-ubuntu-wsl.zip` desde ChatGPT al disco de Windows.
2. En Ubuntu WSL, escribir `cd ~`.
3. En Ubuntu WSL, escribir `explorer.exe .` (abre el home de Ubuntu en el Explorador de Windows; requiere interoperabilidad WSL).
4. Con el Explorador, copiar allí el ZIP descargado en Windows.
5. En Ubuntu, escribir `ls -lh verificador-epp-obra-ubuntu-wsl.zip` para verificar su presencia.
6. Ejecutar `sudo apt update`.
7. Ejecutar `sudo apt install -y unzip git build-essential cmake libgtest-dev gh`.
8. Ejecutar `unzip verificador-epp-obra-ubuntu-wsl.zip`.
9. Ejecutar `cd ~/verificador-epp-obra`.

**Si no funciona `explorer.exe .`:** usa el explorador de archivos de Windows abriendo `\\wsl$` y la carpeta de la distribución y luego `home/<usuario>`; o copia el ZIP con `cp /mnt/c/Users/<usuario_windows>/Downloads/<nombre.zip> ~` adaptando la ruta real. Si Ubuntu es nativo (arranque dual), copia el ZIP a tu home con el administrador de archivos de Ubuntu.

## B. Pruebas x86 en Ubuntu

Ejecutar UN comando por paso, verificando el resultado antes de seguir.

1. `cmake --version` — identificar versión; se requiere >= 3.10.
2. `mkdir -p daemon/build` — sin salida.
3. `cd daemon/build` — sin salida.
4. `cmake .. -DBUILD_TESTING=ON` — al final: `Configuring done` / `Generating done`; si `GTest` falta, revisar instalación de `libgtest-dev`.
5. `cmake --build . -j2` — se compilan `epp-daemon` y `epp_tests`; si falla, guardar el primer error.
6. `ctest --output-on-failure` — debería pasar el ejecutable de pruebas; guardar salida completa.
7. `./epp-daemon` — estado inicial DENEGADO; no valida hardware.
8. `cd ../..` — volver a raíz.

## C. Git y GitHub

1. `git --version` — anotar versión.
2. `git init -b main` — inicializar rama; si falla por versión antigua, consultar antes de modificar manualmente.
3. `git status --short` — comprobar qué archivos se subirían, sin incluir claves, configuraciones privadas, fotos, dataset ni pesos.
4. `git add .` — preparar archivos.
5. `git diff --cached --stat` — inspeccionar nombres y tamaños.
6. `git diff --cached --check` — no debe generar salida.
7. `git commit -m "chore(repo): crea estructura inicial verificable"` — crear commit; si falta identidad, configurar nombre y correo propios (no inventar).
8. `gh auth status` — comprobar inicio de sesión; si no, ejecutar `gh auth login` y seguir las instrucciones del navegador sin compartir códigos ni tokens.
9. `gh repo create verificador-epp-obra --private --source=. --remote=origin --push` — crear y subir; revisar que no exista repo previo con ese nombre.
10. `bash sistema/scripts/github_inicializar.sh` — 10 milestones y etiquetas, requiere permisos sobre el repositorio.
11. `gh run list --workflow daemon-x86.yml --limit 5` — observar la primera ejecución de CI; si falla, inspeccionar logs.

## D. Verificaciones de GitHub

En la web, revisar la primera ejecución de `daemon-x86` (`build-and-test`) y configurar protección de `main` si el plan y visibilidad lo permiten, exigiendo PR, un revisor independiente y check exitoso de CI. GitHub Free permite protección de ramas en repositorios públicos, pero no en privados. **No hacer el repositorio público solo por activar protección** sin revisar las implicaciones del modelo/AGPL-3.0 y la política de datos.

En WSL, el proyecto **puede ejecutar la lógica x86**, pero no sustituye la validación del firmware ESP32-C6 sobre hardware ni de TensorRT/V4L2 en la Jetson. Para flasheo de ESP32 desde Windows, normalmente es más sencillo ESP-IDF nativo en Windows; acceso a USB serie desde WSL puede requerir configuración adicional.

## Registro de verificación

Anotar en `docs/registro_pruebas.md` la afirmación, comando, ambiente (Ubuntu/WSL, compilador, CMake), salida real, resultado y quién verificó. Para los artefactos generados por IA, conservar el origen y describir qué se comprobó realmente.
