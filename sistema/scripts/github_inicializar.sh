#!/usr/bin/env bash
set -euo pipefail
command -v gh >/dev/null || { echo 'Instala GitHub CLI (gh).'; exit 2; }
repo=$(gh repo view --json nameWithOwner --jq '.nameWithOwner')
echo "Repositorio: $repo"
# Comprobar antes de crear para evitar duplicados.
existentes=$(gh api --method GET "repos/$repo/milestones?state=all&per_page=100" --jq '.[].title')
while IFS= read -r titulo; do
  [[ -n "$titulo" ]] || continue
  if grep -Fxq -- "$titulo" <<< "$existentes"; then
    echo "Ya existe milestone: $titulo"
  else
    gh api --method POST "repos/$repo/milestones" -f "title=$titulo" -f 'description=Proyecto EPP académico; asignar issues y evidencias.' >/dev/null
    echo "Creado milestone: $titulo"
  fi
done <<'FIN_MILESTONES'
Hardware base
Modelo de visión
Daemon C++
ESP32 y UART
Interfaces
Bring-up y factory reset
Instrumentación
Carcasa e integración
Verificación
Entregables
FIN_MILESTONES
while IFS='|' read -r etiqueta color descripcion; do
    [[ -n "$etiqueta" ]] || continue
    gh label create "$etiqueta" --repo "$repo" --color "$color" --description "$descripcion" --force >/dev/null
    echo "Label lista: $etiqueta"
done <<'FIN_LABELS'
jetson|176BC3|Jetson Nano y Linux
esp32|008672|Firmware ESP32-C6
vision|5319E7|Detección y modelo
dashboard|1D76DB|Interfaz web
docs|0075CA|Documentación y entregables
bug|D73A4A|Error o regresión
FIN_LABELS
