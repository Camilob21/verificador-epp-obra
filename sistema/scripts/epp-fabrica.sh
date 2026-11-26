#!/bin/sh
# epp-fabrica — factory reset del verificador de EPP: deja el equipo como recien instalado.
#
# Borra:     eventos registrados (SQLite), fotos de evidencia y la marca de puesta en marcha.
# Restaura:  /etc/epp/epp.conf desde la copia de fabrica (/usr/share/epp/epp.conf.fabrica).
# Conserva:  sistema operativo, programa, modelo, dashboard y la configuracion de red.
# Despues:   el daemon arranca en modo PUESTA EN MARCHA (bring-up) y no controla el acceso hasta
#            que el instalador lo confirme.
#
# Dos formas de llamarlo:
#   - Boton del ESP32 sostenido 5 s: el daemon crea /var/lib/epp/solicitud_fabrica y
#     epp-fabrica.path (systemd, como root) ejecuta este script sin preguntar.
#   - A mano:  sudo epp-fabrica   (pide escribir SI)
set -u
DATOS=${EPP_DATOS:-/var/lib/epp}          # variables EPP_* solo para probar el script
CONF=${EPP_CONF:-/etc/epp/epp.conf}
FABRICA=${EPP_FABRICA:-/usr/share/epp/epp.conf.fabrica}

if [ "$(id -u)" -ne 0 ]; then
  echo "Ejecutar con sudo: sudo epp-fabrica" >&2
  exit 1
fi
if [ ! -f "$FABRICA" ]; then
  echo "ERROR: falta $FABRICA (correr sistema/scripts/instalar_jetson.sh)" >&2
  rm -f "$DATOS/solicitud_fabrica"   # evita que epp-fabrica.path lo intente sin fin
  exit 1
fi
if [ -t 0 ]; then
  echo "Esto BORRA los eventos y las fotos de evidencia y restaura la configuracion de fabrica."
  printf "Escriba SI para continuar: "
  read -r r
  [ "$r" = "SI" ] || { echo "Cancelado."; exit 1; }
fi

echo "Factory reset: deteniendo epp-daemon"
systemctl stop epp-daemon
rm -f "$DATOS/solicitud_fabrica" "$DATOS/solicitud_fabrica.tmp"
n_ev=$(ls "$DATOS"/evento_*.jpg 2>/dev/null | wc -l)
rm -f "$DATOS"/eventos.db "$DATOS"/eventos.db-wal "$DATOS"/eventos.db-shm
rm -f "$DATOS"/evento_*.jpg
rm -f "$DATOS/puesta_en_marcha.ok" "$DATOS/puesta_en_marcha.ok.tmp"
install -m 644 "$FABRICA" "$CONF"
echo "Factory reset: borrados la base de eventos y $n_ev fotos; $CONF restaurado"
systemctl start epp-daemon
echo "Factory reset: listo. El equipo arranca en modo PUESTA EN MARCHA."
