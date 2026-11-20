# Licencias — decisión pendiente

**No se ha añadido LICENSE deliberadamente**. Esto no autoriza copiar o redistribuir código sin permiso; antes de hacer público el repositorio, el equipo debe decidir y documentar los derechos de las contribuciones.

Opciones a discutir:

- **MIT / BSD-3-Clause:** permisivas; permiten reutilización y código cerrado, pero pueden **no** ser compatibles con obligaciones AGPL de una obra derivada integrada.
- **GPL-3.0:** copyleft en distribución de software; **no sustituye automáticamente** obligaciones de AGPL por uso en red ni resuelve todo sobre modelos.
- **AGPL-3.0:** copyleft que cubre ciertos usos por red de aplicaciones modificadas; suele alinearse con la licencia indicada por Ultralytics, sujeto a la composición efectiva de la obra derivada y los derechos correspondientes.
- **Sin licencia:** reserva derechos por defecto; no significa "código libre".

Según la [licencia de Ultralytics](https://www.ultralytics.com/license), sus **modelos entrenados** quedan bajo AGPL-3.0 por defecto, y la organización describe obligaciones de liberar el código fuente correspondiente de obras derivadas completas o adquirir licencia Enterprise. Entrenar con Ultralytics y luego implementar inferencia propia en TensorRT **no asegura** por sí solo la desaparición de esas obligaciones. Confirmar versiones, archivos, modelos base, uso real y condiciones con el docente y, si corresponde, asesoría jurídica.

El dataset [Construction-PPE de Ultralytics](https://docs.ultralytics.com/datasets/detect/construction-ppe) aparece documentado por **Ultralytics** con imágenes y anotaciones bajo **CC BY 4.0**. Conservar atribución, enlace y referencia de la modificación de etiquetas. Verificar la versión/hash descargada y cualquier aviso adicional. CC BY 4.0 del dataset es **diferente** de licencia de código y de modelos; no confundirlas.

**Git LFS vs GitHub Releases:** LFS versiona punteros y ofrece almacenamiento/transferencia de grandes binarios con cuotas; Releases guarda adjuntos inmutables por versión/entrega sin arrastrarlos en cada clon. Por ahora **no publicar pesos ni engine**, revisar licencia y procedencia. Los motores TensorRT dependen de versión de TensorRT/Jetson, no asumir portabilidad.
