#!/usr/bin/env bash
# EXTREMADAMENTE DESTRUCTIVO: graba sobre DISCO ENTERO, no partición.
set -euo pipefail
if [[ $# -ne 2 ]]; then
  echo 'Uso: sudo bash sistema/scripts/grabar_verificar_sd.sh /ruta/imagen.img /dev/sdX' >&2
  exit 2
fi
imagen="$1"
disco="$2"
if [[ ! -f "$imagen" || ! -b "$disco" ]]; then
  echo 'Imagen inexistente o dispositivo no es de bloques.' >&2; exit 2
fi
if [[ "$(lsblk -ndo TYPE "$disco")" != "disk" ]]; then
  echo 'Se exige un disco completo, no una partición.' >&2; exit 2
fi
if lsblk -nr -o MOUNTPOINT "$disco" | grep -q '[^[:space:]]'; then
  echo 'Disco/partición montado: desmontar antes de intentar grabar.' >&2; exit 2
fi
bytes=$(stat -c %s "$imagen")
capacidad=$(blockdev --getsize64 "$disco")
if (( bytes > capacidad )); then
  echo 'La imagen es más grande que el dispositivo.' >&2; exit 2
fi
printf 'PELIGRO: se borrará TODO el disco %s; imagen %s (%s bytes)\n' "$disco" "$imagen" "$bytes"
lsblk -o NAME,SIZE,TYPE,MODEL,MOUNTPOINT "$disco"
printf 'Escribe EXACTAMENTE %s para continuar: ' "$disco"
read -r confirmacion
if [[ "$confirmacion" != "$disco" ]]; then echo 'Cancelado.'; exit 1; fi
# No se debe cambiar el disco entre confirmar y ejecutar dd.
dd if="$imagen" of="$disco" bs=4M status=progress conv=fsync
sync
cmp -n "$bytes" "$imagen" "$disco"
echo 'VERIFICACIÓN OK: primeros bytes del dispositivo idénticos a imagen.'
