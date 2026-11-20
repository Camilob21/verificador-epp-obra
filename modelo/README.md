# Procedencia de los artefactos de visión

- **Dataset:** Construction-PPE de Ultralytics, CC BY 4.0 (según los metadatos reportados por el equipo). Ficha pública confirmada: https://docs.ultralytics.com/datasets/detect/construction-ppe ; creador **Ultralytics**, imágenes y anotaciones **CC BY 4.0**, noviembre de 2026. El identificador/hash de la **copia efectivamente usada en Colab** y el reetiquetado original continúan pendientes de cotejar.
- Reetiquetado por **nombre** a 9 clases; comprobar orden con el `epp.yaml` del entrenamiento ejecutado, no usar esta plantilla a ciegas.
- `epp_entrenamiento_colab.ipynb`, `scripts/reetiquetar.py`, `scripts/prueba_b.py`: **PLANTILLAS**, no sustituyen los originales del Colab/PC.
- `resultados/metricas_resumen_reportado.csv`: transcripción de cuatro valores aportados por el equipo, no resultados que hayamos recalculado ni los CSV completos.
- Sin dataset, imágenes de personas, pesos (`.pt`, `.onnx`) ni motores (`.engine`) en Git.

**Pendiente:** seleccionar YOLO26n o YOLOv8n tras comprobar conversión ONNX → TensorRT 8.2 y latencia en Jetson. Ver `docs/licencias.md` antes de distribuir modelos.
