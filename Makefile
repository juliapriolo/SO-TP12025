CC       := gcc
CSTD     := -std=c11
WARN     := -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wpointer-arith -Wcast-qual
OPT      := -O2
DEFS     := -D_POSIX_C_SOURCE=200809L
CFLAGS   := $(CSTD) $(WARN) $(OPT) $(DEFS)

LDFLAGS  :=
LDLIBS   := -pthread -lrt

SAN ?= 0
SAN_CFLAGS  := -g -O1 -fsanitize=address -fno-omit-frame-pointer
SAN_LDFLAGS := -fsanitize=address

ifeq ($(SAN),1)
  CFLAGS  += $(SAN_CFLAGS)
  LDFLAGS += $(SAN_LDFLAGS)
endif

ROOT     := .
INCDIR   := $(ROOT)/include
SRCDIR   := $(ROOT)/src
BINDIR   := $(ROOT)/bin
OBJDIR   := $(ROOT)/.obj
INCLUDES := -I$(INCDIR) -I$(INCDIR)/master -I$(INCDIR)/ipc -I$(INCDIR)/player -I$(INCDIR)/view -I$(INCDIR)/utils

VIEW_SRC   := $(SRCDIR)/view/view.c $(SRCDIR)/view/view_utils.c $(SRCDIR)/ipc/shm.c
VIEW_OBJ   := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(VIEW_SRC))
PLAYER_SRC := $(SRCDIR)/player/player.c $(SRCDIR)/player/player_utils.c $(SRCDIR)/ipc/shm.c $(SRCDIR)/utils/timing.c
PLAYER_OBJ := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(PLAYER_SRC))

MASTER_SRC := $(SRCDIR)/master/master.c \
				  $(SRCDIR)/master/args.c \
				  $(SRCDIR)/utils/timing.c \
              $(SRCDIR)/master/game.c \
			  $(SRCDIR)/master/game_init.c \
			  $(SRCDIR)/ipc/sync_init.c \
			  $(SRCDIR)/ipc/notify.c \
				  $(SRCDIR)/ipc/proc.c \
				  $(SRCDIR)/master/cleanup.c \
			  $(SRCDIR)/ipc/shm.c

MASTER_OBJ := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(MASTER_SRC))

# Binario propio
MASTER_BIN := $(abspath $(BINDIR)/master)

W ?= 10
H ?= 10
P ?= 1         # cantidad de jugadores por defecto
D ?= 200       # delay entre movimientos en milisegundos
T ?= 10        # timeout en segundos
S ?=           # semilla para random 

PLAYER_BIN := $(abspath $(BINDIR)/player)
VIEW_BIN   := $(abspath $(BINDIR)/view)

PLAYER_LIST := $(foreach i,$(shell seq 1 $(P)),$(PLAYER_BIN))

ARCH := $(shell uname -m)
ifeq ($(ARCH),x86_64)
  CHOMPCHAMPS_BIN := $(abspath ChompChamps_amd64)
else ifeq ($(ARCH),aarch64)
  CHOMPCHAMPS_BIN := $(abspath ChompChamps_arm64)
else
  CHOMPCHAMPS_BIN := $(abspath ChompChamps_amd64)  
endif

.PHONY: debug clean dirs view player master run-master run-master-noview master-run run-chompchamps run-valgrind clean-shm build format help build-noview debug-noview run-valgrind-noview
build-noview: deps
	$(MAKE) clean
	$(MAKE) SAN=1 debug-noview

debug-noview: deps dirs player master
# Ejecuta nuestro master sin la vista
run-master-noview: deps master player
	@echo "MASTER=$(MASTER_BIN)"
	@echo "P=$(P)"
	@echo "PLAYER_LIST=$(PLAYER_LIST)"
	@echo "D=$(D)"
	@echo "T=$(T)"
	@echo "S=$(S)"
	$(MASTER_BIN) -w $(W) -h $(H) -d $(D) -t $(T) $(if $(S),-s $(S),) -p $(PLAYER_LIST)

build: deps
	$(MAKE) clean
	$(MAKE) SAN=1 debug


debug: deps dirs view player master

view: $(BINDIR)/view
player: $(BINDIR)/player
master: $(BINDIR)/master

$(BINDIR)/view: $(VIEW_OBJ) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@ $(LDFLAGS) $(LDLIBS) -lncurses
$(BINDIR)/player: $(PLAYER_OBJ) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@ $(LDFLAGS) $(LDLIBS)
$(BINDIR)/master: $(MASTER_OBJ) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@ $(LDFLAGS) $(LDLIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | dirs
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

dirs:
	@mkdir -p $(BINDIR) $(OBJDIR) $(OBJDIR)/master $(OBJDIR)/core $(OBJDIR)/player $(OBJDIR)/view $(OBJDIR)/ipc $(OBJDIR)/utils

clean:
	@$(RM) -r $(OBJDIR) $(BINDIR)/view $(BINDIR)/player $(BINDIR)/master $(BINDIR)/*.log
	@echo "Limpio."

clean-shm:
	@echo "Limpiando memoria compartida..."
	@-rm -f /dev/shm/game_state /dev/shm/game_sync
	@echo "Memoria compartida limpiada."

# Ejecuta nuestro master 
run-master: deps master view player
	@echo "MASTER=$(MASTER_BIN)"
	@echo "VIEW  =$(VIEW_BIN)"
	@echo "P=$(P)"
	@echo "PLAYER_LIST=$(PLAYER_LIST)"
	@echo "D=$(D)"
	@echo "T=$(T)"
	@echo "S=$(S)"
	$(MASTER_BIN) -w $(W) -h $(H) -d $(D) -t $(T) $(if $(S),-s $(S),) -v $(VIEW_BIN) -p $(PLAYER_LIST)

master-run: run-master

# Ejecuta con ChompChamps
run-chompchamps: deps view player
	@echo "CHOMPCHAMPS=$(CHOMPCHAMPS_BIN)"
	@echo "VIEW  =$(VIEW_BIN)"
	@echo "P=$(P)"
	@echo "PLAYER_LIST=$(PLAYER_LIST)"
	@echo "D=$(D)"
	@echo "T=$(T)"
	@echo "S=$(S)"
	@echo "Arquitectura detectada: $(ARCH)"
	$(CHOMPCHAMPS_BIN) -w $(W) -h $(H) -d $(D) -t $(T) $(if $(S),-s $(S),) -v $(VIEW_BIN) -p $(PLAYER_LIST)

# Ejecuta con valgrind usando nuestro master
run-valgrind: deps master view player
	@echo "Ejecutando con valgrind..."
	@echo "MASTER=$(MASTER_BIN)"
	@echo "VIEW  =$(VIEW_BIN)"
	@echo "P=$(P)"
	@echo "PLAYER_LIST=$(PLAYER_LIST)"
	@echo "D=$(D)"
	@echo "T=$(T)"
	@echo "S=$(S)"
	valgrind --leak-check=full \
	  --show-leak-kinds=all \
	  --track-origins=yes \
	  --track-fds=yes \
	  --error-exitcode=1 \
	  --trace-children=yes \
	  $(MASTER_BIN) -w $(W) -h $(H) -d $(D) -t $(T) $(if $(S),-s $(S),) -v $(VIEW_BIN) -p $(PLAYER_LIST)

# Ejecuta con valgrind sin view
run-valgrind-noview: deps master player
	@echo "Ejecutando con valgrind (sin view)..."
	@echo "MASTER=$(MASTER_BIN)"
	@echo "P=$(P)"
	@echo "PLAYER_LIST=$(PLAYER_LIST)"
	@echo "D=$(D)"
	@echo "T=$(T)"
	@echo "S=$(S)"
	valgrind --leak-check=full \
	  --show-leak-kinds=all \
	  --track-origins=yes \
	  --track-fds=yes \
	  --error-exitcode=1 \
	  --trace-children=yes \
	  $(MASTER_BIN) -w $(W) -h $(H) -d $(D) -t $(T) $(if $(S),-s $(S),) -p $(PLAYER_LIST)

FORMAT = clang-format
FORMAT_FLAGS = -i
SRC = $(shell find src -name '*.c' -print) $(wildcard include/*.h)

format:
	$(FORMAT) $(FORMAT_FLAGS) $(SRC)

.PHONY: deps

deps:
	apt-get update
	apt-get install -y libncurses5-dev libncursesw5-dev

help:
	@echo "PARA EJECUTAR CON EL BINARIO DE LA CATEDRA, ESTE SE DEBE UBICAR EN LA MISMA CARPETA QUE EL MAKEFILE"
	@echo "Opciones disponibles:"
	@echo ""
	@echo "BUILD"
	@echo "  build            - Compilar proyecto completo"
	@echo "  build-noview     - Compilar proyecto sin view"
	@echo "  run-master       - Ejecutar master con view"
	@echo "  run-master-noview - Ejecutar master sin view"
	@echo "  run-chompchamps  - Ejecutar con ChompChamps"
	@echo ""
	@echo "DEBUG (Para valgrind):"
	@echo "  debug            - Compilar proyecto completo"
	@echo "  debug-noview     - Compilar proyecto sin view"
	@echo "  run-valgrind     - Ejecutar con valgrind (con view)"
	@echo "  run-valgrind-noview - Ejecutar con valgrind (sin view)"
	@echo ""
	@echo "UTILIDADES:"
	@echo "  clean            - Limpiar archivos compilados"
	@echo "  clean-shm        - Limpiar solo memoria compartida"
	@echo "  format           - Formatear código con clang-format"
	@echo ""
	@echo "Variables configurables:"
	@echo "  W=10             - Ancho del tablero (default: 10)"
	@echo "  H=10             - Alto del tablero (default: 10)"
	@echo "  P=1              - Número de jugadores (default: 1)"
	@echo "  D=200            - Delay entre movimientos en milisegundos (default: 200)"
	@echo "  T=10             - Timeout del juego en segundos (default: 10)"
	@echo "  S=               - Semilla para random"
	@echo ""
	@echo "Ejemplos BUILD:"
	@echo "  make build && make run-master"
	@echo "  make build-noview && make run-master-noview"
	@echo "  make run-master W=15 H=15 P=5"
	@echo "  make run-master D=500 T=30"
	@echo ""
	@echo "Ejemplos DEBUG (para valgrind):"
	@echo "  make debug && make run-valgrind"
	@echo "  make debug-noview && make run-valgrind-noview"
	@echo "  make run-valgrind W=20 H=20 T=60"
	@echo "  make run-valgrind-noview D=300 T=45"
