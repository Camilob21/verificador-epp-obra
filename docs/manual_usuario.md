# Manual de usuario — borrador

## Estado actual

Prototipo no operativo. El dashboard sólo indica `NO VERIFICADO`. **No conectar el relé a una barrera real ni usar esta versión para autorizar entrada**.

## Flujo futuro (pendiente de pruebas)

1. Administrador realiza bring-up y define requisitos de EPP.
2. Persona se coloca frente a la marca (~2 m, a verificar por campo visual).
3. El sistema verifica presencia, casco y chaleco; opcionalmente gafas/guantes tras nueva validación.
4. LED/relé expresan autorización únicamente si se cumplen reglas y latido.
5. Factory reset (~5 s) retorna al bring-up; aún no implementado.

## Instrucciones de desarrollo

Ver README para compilar y ejecutar tests sin conectar actuadores.
