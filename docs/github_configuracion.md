# Configuración GitHub (a realizar por el dueño de la cuenta)

## Visibilidad / protección

**Inicio recomendado: privado** para revisar licencias y archivos. GitHub Free no ofrece protección exigible de rama en repositorios privados (ver documentación GitHub actual); con `public` en Free o plan de pago elegible sí puede habilitarse. Mientras no haya protección efectiva, la revisión de PR es una **convención**, no una barrera técnica. No hacer público sin acuerdo de licencias.

En `Settings → Branches → Add branch protection rule` (si está disponible): patrón `main`, exigir PR, mínimo **1 aprobación**, no permitir aprobar PR propio, exigir estado `build-and-test` del workflow `daemon-x86` **después de ejecutarlo al menos una vez**, impedir force-push y borrado, aplicar a administradores si la modalidad ofrece esa opción. Revisar explícitamente las opciones efectivas.

## Milestones / labels

Con GitHub CLI autenticada y dentro de este repositorio, ejecutar una vez `bash sistema/scripts/github_inicializar.sh` para crear las **10 milestones** y **6 labels** básicas (idempotente de manera aproximada). Verificar en la web `Issues → Milestones` y `Issues → Labels`.

## Invitaciones

`Settings → Collaborators / Access` y enviar invitaciones a integrantes (correos/usuarios **no** se deben escribir en código del repositorio). Conceder permisos mínimos; comprobar acceso mediante clon en otra máquina.
