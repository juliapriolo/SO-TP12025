# TP1 - Sistemas Operativos (72.11) - ChompChamps

En este trabajo práctico se expone el desarrollo de un sistema denominado ChompChamps, un juego multijugador inspirado en el clásico Snake. El proyecto tiene como eje central la implementación de mecanismos de comunicación entre procesos (IPC) en un entorno POSIX, orientados a la coordinación de la lógica del juego, la interacción entre múltiples participantes y la visualización del estado en tiempo real.


## Compilación

Para compilar el proyecto, existen diferentes comandos según si se desea incluir la vista (view) o no, y si se requiere compilación normal o para debug (valgrind).

### Compilación completa (con vista)

```
make build
```

### Compilación sin la vista

```
make build-noview
```

### Compilación para debug (con vista)

```
make debug
```

### Compilación para debug sin la vista

```
make debug-noview
```

Para más información sobre los comandos disponibles o los parámetros configurables, podés ejecutar:

```
make help
```

# SO-TP12025

