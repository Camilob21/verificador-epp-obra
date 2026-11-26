#!/bin/sh
# hora-http.sh — reloj de la Jetson Nano sin pila de RTC y sin NTP.
#
# La Jetson Nano pierde la hora al apagarse (arranca en 2021 o en 2000) y en la red de la
# universidad NTP (UDP 123) no responde. Este script:
#   restaurar    al arrancar: si el reloj esta ATRAS de la ultima hora guardada, lo adelanta a
#                ella (sin internet al menos queda cerca de la hora real, nunca en el pasado).
#   guardar      guarda la hora actual (solo si es plausible: posterior al 1-ene-2026).
#   sincronizar  pide la hora a un servidor web (cabecera "Date" de HTTP, pasa por cualquier red
#                con navegacion), reintentando sin limite; luego la guarda cada 10 min por si
#                cortan la luz sin apagar, y vuelve a sincronizar cada 6 h.
#
# Instalado en /usr/local/bin/hora-http.sh por sistema/scripts/instalar_jetson.sh.
# Servicios: hora-http.service (restaurar/guardar) y hora-http-sync.service (sincronizar).
set -u
ULTIMA=${HORA_ULTIMA:-/var/lib/hora-http/ultima}   # solo importa su fecha de modificacion
OK=/run/hora-http.ok               # existe = la hora vino de la red en este arranque (lo lee el daemon)
MINIMA=1767225600                  # 2026-01-01 00:00:00 UTC
URL=http://google.com

guardar() {
  if [ "$(date +%s)" -gt "$MINIMA" ]; then
    mkdir -p "$(dirname "$ULTIMA")" && touch "$ULTIMA"
  fi
}

pedir_hora() {
  curl -sI --max-time 5 "$URL" | grep -i '^date:' | cut -d' ' -f2- | tr -d '\r'
}

case "${1:-sincronizar}" in
  restaurar)
    if [ -f "$ULTIMA" ]; then
      g=$(stat -c %Y "$ULTIMA")
      if [ "$(date +%s)" -lt "$g" ]; then
        date -s "@$g" >/dev/null && echo "Reloj llevado a la ultima hora guardada: $(date)"
      fi
    fi
    exit 0
    ;;
  guardar)
    guardar
    exit 0
    ;;
  sincronizar)
    while :; do
      intentos=0
      until D=$(pedir_hora) && [ -n "$D" ] && date -s "$D" >/dev/null; do
        intentos=$((intentos + 1))
        [ $((intentos % 6)) -eq 1 ] && echo "Sin respuesta de $URL; reintento cada 10 s"
        sleep 10
      done
      touch "$OK"
      guardar
      echo "Hora sincronizada por HTTP: $(date)"
      i=0
      while [ $i -lt 36 ]; do  # 36 x 10 min = 6 h
        sleep 600
        guardar
        i=$((i + 1))
      done
    done
    ;;
  *)
    echo "Uso: $0 restaurar|guardar|sincronizar" >&2
    exit 2
    ;;
esac
