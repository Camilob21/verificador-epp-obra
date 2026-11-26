#!/usr/bin/env bash
# instalar_jetson.sh — bring-up tecnico: deja una Jetson Nano lista como verificador de EPP.
#
# Uso (en la Jetson, desde la raiz del repositorio):
#     sudo sistema/scripts/instalar_jetson.sh
#
# Requisitos: JetPack 4.6.1 con el usuario 'epp', red por Ethernet con internet (para apt y la
# hora) y el modelo TensorRT en la ruta 'motor' de epp.conf (ver modelo/README.md).
# Se puede correr varias veces: no borra datos ni pisa un /etc/epp/epp.conf existente.
# Al terminar, el daemon arranca en modo PUESTA EN MARCHA si el equipo nunca se confirmo.
set -euo pipefail

RAIZ=$(cd "$(dirname "$0")/../.." && pwd)
USUARIO=epp
REINICIAR=0
AVISOS=0
paso()  { printf '\n== %s\n' "$*"; }
ok()    { printf '   OK     %s\n' "$*"; }
aviso() { printf '   AVISO  %s\n' "$*"; AVISOS=$((AVISOS + 1)); }
error() { printf '   ERROR  %s\n' "$*" >&2; exit 1; }

[ "$(id -u)" -eq 0 ] || error "ejecutar con sudo: sudo $0"
id "$USUARIO" >/dev/null 2>&1 || error "no existe el usuario '$USUARIO' (se crea en el primer arranque de JetPack)"
[ -f "$RAIZ/daemon/CMakeLists.txt" ] || error "no encuentro el repositorio en $RAIZ"
cd "$RAIZ"

paso "1/7 Paquetes"
[ -e /usr/include/aarch64-linux-gnu/NvInfer.h ] || error "no hay TensorRT: se requiere JetPack 4.6.1"
faltan=""
for p in nginx libsqlite3-dev curl cmake g++; do
  dpkg -s "$p" >/dev/null 2>&1 || faltan="$faltan $p"
done
if [ -n "$faltan" ]; then
  apt-get update && apt-get install -y $faltan || error "no se pudieron instalar:$faltan (hay internet?)"
fi
ok "nginx, sqlite, curl, cmake y g++ instalados"

paso "2/7 Puerto serie, permisos y camara"
if systemctl is-enabled nvgetty >/dev/null 2>&1; then
  systemctl disable --now nvgetty
  REINICIAR=1
fi
ok "nvgetty desactivado (/dev/ttyTHS1 libre para el ESP32)"
usermod -aG dialout,video "$USUARIO"
ok "$USUARIO en los grupos dialout y video"
install -m 644 sistema/uvcvideo-ngs.conf /etc/modprobe.d/uvcvideo-ngs.conf
if [ "$(cat /sys/module/uvcvideo/parameters/quirks 2>/dev/null)" != "4" ]; then
  REINICIAR=1
  aviso "el ajuste de la camara (uvcvideo quirks=4) se aplica al reiniciar"
else
  ok "ajuste de la camara activo (uvcvideo quirks=4)"
fi

paso "3/7 Compilar epp-daemon (unos minutos)"
DUENO=$(stat -c %U "$RAIZ")
sudo -u "$DUENO" sh -c "mkdir -p '$RAIZ/daemon/build' && cd '$RAIZ/daemon/build' && cmake -DBUILD_TESTING=OFF .. >/dev/null && make -j4" \
  || error "fallo la compilacion"
install -m 755 daemon/build/epp-daemon /usr/local/bin/epp-daemon
ok "/usr/local/bin/epp-daemon"

paso "4/7 Configuracion y datos"
install -d -m 755 /etc/epp /usr/share/epp
install -m 644 sistema/epp.conf.example /usr/share/epp/epp.conf.fabrica
ok "copia de fabrica en /usr/share/epp/epp.conf.fabrica"
if [ ! -f /etc/epp/epp.conf ]; then
  install -m 644 /usr/share/epp/epp.conf.fabrica /etc/epp/epp.conf
  ok "/etc/epp/epp.conf creado con los valores de fabrica"
else
  ok "/etc/epp/epp.conf existente se conserva (valores de fabrica: sudo epp-fabrica)"
  for clave in marca_puesta_en_marcha solicitud_fabrica; do
    if ! grep -Eq "^[[:space:]]*$clave[[:space:]]*=" /etc/epp/epp.conf; then
      grep -E "^[[:space:]]*$clave[[:space:]]*=" /usr/share/epp/epp.conf.fabrica >> /etc/epp/epp.conf
      ok "agregada a epp.conf la clave $clave"
    fi
  done
fi
install -d -o "$USUARIO" -g "$USUARIO" -m 750 /var/lib/epp
ok "/var/lib/epp (eventos y evidencias)"
MOTOR=$(sed -n 's/^[[:space:]]*motor[[:space:]]*=[[:space:]]*\([^#[:space:]]*\).*/\1/p' /etc/epp/epp.conf | tail -1)
MOTOR=${MOTOR:-/home/epp/yolov8n_480x640_fp16.engine}
if [ -f "$MOTOR" ]; then ok "modelo $MOTOR"; else aviso "falta el modelo $MOTOR (ver modelo/README.md)"; fi

paso "5/7 Reloj, factory reset y servicio"
install -m 755 sistema/scripts/hora-http.sh /usr/local/bin/hora-http.sh
install -m 755 sistema/scripts/epp-fabrica.sh /usr/local/sbin/epp-fabrica
for u in epp-daemon.service hora-http.service hora-http-sync.service epp-fabrica.service epp-fabrica.path; do
  install -m 644 "sistema/systemd/$u" /etc/systemd/system/
done
systemctl daemon-reload
systemctl reenable hora-http.service hora-http-sync.service epp-fabrica.path epp-daemon.service >/dev/null 2>&1
systemctl start hora-http.service epp-fabrica.path
systemctl restart hora-http-sync.service
ok "hora-http (restaurar/guardar), hora-http-sync (HTTP), epp-fabrica.path y epp-daemon habilitados"

paso "6/7 Dashboard (nginx, puerto 80)"
install -d /opt/epp/dashboard
install -m 644 dashboard/index.html dashboard/app.js dashboard/styles.css /opt/epp/dashboard/
install -m 644 dashboard/nginx-epp.conf /etc/nginx/sites-available/epp
ln -sf /etc/nginx/sites-available/epp /etc/nginx/sites-enabled/epp
rm -f /etc/nginx/sites-enabled/default
nginx -t >/dev/null 2>&1 || error "configuracion de nginx invalida (sudo nginx -t)"
systemctl enable nginx >/dev/null 2>&1
systemctl restart nginx
ok "dashboard en /opt/epp/dashboard"

paso "7/7 Arranque"
if [ "$REINICIAR" -eq 1 ]; then
  aviso "hay que reiniciar para terminar: sudo reboot (el servicio arranca solo)"
else
  systemctl restart epp-daemon
  sleep 3
  if systemctl is-active --quiet epp-daemon; then ok "epp-daemon activo"; else aviso "epp-daemon no arranco: journalctl -u epp-daemon -n 30"; fi
fi

IP=$(hostname -I 2>/dev/null | awk '{print $1}')
printf '\nListo (%d avisos). Dashboard: http://%s.local/  o  http://%s/\n' "$AVISOS" "$(hostname)" "${IP:-<ip>}"
if [ ! -f /var/lib/epp/puesta_en_marcha.ok ]; then
  echo "El equipo esta en PUESTA EN MARCHA: siga el asistente del dashboard (docs/manual_usuario.md)."
fi
