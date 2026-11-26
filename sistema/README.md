# Herramientas de sistema

| Archivo | Destino en la Jetson | Para qué |
|---|---|---|
| `scripts/instalar_jetson.sh` | — (se corre desde el repo) | **Bring-up técnico**: instala y configura todo de una vez |
| `epp.conf.example` | `/etc/epp/epp.conf` y `/usr/share/epp/epp.conf.fabrica` | Configuración del daemon; la copia de fábrica la usa el factory reset |
| `uvcvideo-ngs.conf` | `/etc/modprobe.d/` | Quirk de la webcam (sin él falla con `Input/output error`, `UVC probe control: -75`) |
| `systemd/epp-daemon.service` | `/etc/systemd/system/` | Arranca el daemon al encender y lo relanza si se cae |
| `scripts/hora-http.sh` | `/usr/local/bin/` | Reloj sin RTC ni NTP (ver abajo) |
| `systemd/hora-http.service` | `/etc/systemd/system/` | Al arrancar, adelanta el reloj a la última hora guardada; al apagar, la guarda |
| `systemd/hora-http-sync.service` | `/etc/systemd/system/` | Pide la hora por HTTP sin límite de intentos y la guarda cada 10 min |
| `scripts/epp-fabrica.sh` | `/usr/local/sbin/epp-fabrica` | **Factory reset** |
| `systemd/epp-fabrica.path` y `.service` | `/etc/systemd/system/` | Ejecutan el factory reset cuando el daemon lo pide (botón del ESP32) |
| `scripts/grabar_verificar_sd.sh` | — | **BORRA EL DISCO SELECCIONADO**: graba una imagen `.img` con `dd` y la verifica con `cmp` |

El manual para el usuario final (puesta en marcha, luces, factory reset) está en
[`docs/manual_usuario.md`](../docs/manual_usuario.md).

## Bring-up técnico (una Jetson nueva)

Requisitos: JetPack 4.6.1 grabado, usuario `epp`, nombre de equipo `epp-jetson` (así el dashboard
queda en `http://epp-jetson.local/`), Ethernet con internet y el modelo TensorRT en la ruta `motor`
de `epp.conf` (ver `modelo/README.md`).

1. Copiar el repositorio a la Jetson **desde el PC** (con `-rl` y no `-a`, para que los archivos
   lleguen con la hora de la Jetson y `make` sí recompile lo que cambió):
   ```bash
   rsync -rlv --exclude build --exclude .git ./ epp@epp-jetson.local:/home/epp/verificador-epp-obra/
   ```
2. En la Jetson:
   ```bash
   cd ~/verificador-epp-obra
   sudo sistema/scripts/instalar_jetson.sh
   ```
   El script es idempotente (se puede repetir): instala paquetes, libera `/dev/ttyTHS1` (nvgetty),
   da permisos de `dialout` y `video`, aplica el quirk de la cámara, compila e instala el daemon,
   la configuración, el reloj, el factory reset, el dashboard con nginx y habilita los servicios.
   Nunca pisa un `/etc/epp/epp.conf` existente (solo le agrega las claves nuevas que falten).
3. Si al final pide reiniciar: `sudo reboot`.
4. Abrir el dashboard y hacer la **puesta en marcha** (manual de usuario, sección 4).

Comprobaciones tras reiniciar:
```bash
systemctl is-active epp-daemon hora-http hora-http-sync epp-fabrica.path nginx   # todos "active"
cat /sys/module/uvcvideo/parameters/quirks                                        # 4
journalctl -u epp-daemon -n 20 --no-pager                                         # "PUESTA EN MARCHA" o el estado
```

## Puesta en marcha y factory reset

- **Puesta en marcha**: mientras no exista `/var/lib/epp/puesta_en_marcha.ok`, el daemon no
  controla el acceso (relé apagado, amarillo fijo) y el dashboard muestra la lista de
  comprobaciones. Se confirma con la mano frente al sensor 3 s; entonces el daemon crea el archivo.
- **Factory reset**: botón del ESP32 sostenido 5 s → el ESP32 envía `EVENTO_BOTON` (con ACK) → el
  daemon (usuario `epp`, sin privilegios) crea `/var/lib/epp/solicitud_fabrica` →
  `epp-fabrica.path` lo detecta y lanza `epp-fabrica.service` como root → `epp-fabrica` detiene el
  daemon, borra eventos, evidencias y la marca de puesta en marcha, restaura `epp.conf` de fábrica y
  vuelve a arrancar el daemon, que queda en puesta en marcha.
  Separar así los privilegios evita que el daemon (que atiende la red) pueda escribir en `/etc`.
- A mano: `sudo epp-fabrica` (pide escribir `SI`).
- Si cambian los umbrales definitivos, cambiarlos también en `epp.conf.example` y volver a correr
  el instalador: esa es la copia "de fábrica".

## Reloj (la Jetson Nano no tiene pila de RTC)

Al apagarse pierde la hora, y en la red de la universidad NTP (UDP 123) no responde
(`timedatectl` muestra `System clock synchronized: no`). Por eso:

1. `hora-http.service` (muy temprano en el arranque) adelanta el reloj a la última hora guardada:
   sin internet queda cerca de la real y nunca en 2021.
2. `hora-http-sync.service` lee la cabecera `Date` de `http://google.com` (pasa por cualquier red
   con navegación), reintenta cada 10 s sin rendirse, crea `/run/hora-http.ok` al lograrlo y guarda
   la hora cada 10 min (por si cortan la luz sin apagar).
3. El daemon no espera a la red: marca cada evento con `hora_confiable` y el dashboard muestra la
   hora del equipo con un aviso si no está sincronizada.

Antes (hasta el 26-nov) el script hacía 30 intentos en ~90 s y se rendía si el Ethernet tardaba
en tener IP: por eso la Jetson arrancaba con fecha de 2021.

## Correr el daemon a mano (pruebas)

Primero `sudo systemctl stop epp-daemon`: la cámara solo la puede usar un programa a la vez.
Tras cambiar `/etc/epp/epp.conf`: `sudo systemctl restart epp-daemon`.
