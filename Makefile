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
INCLUDES := -I$(INCDIR)

VIEW_SRC   := $(SRCDIR)/view/view.c $(SRCDIR)/view/view_utils.c $(SRCDIR)/ipc/shm.c
VIEW_OBJ   := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(VIEW_SRC))
PLAYER_SRC := $(SRCDIR)/player/player.c $(SRCDIR)/player/player_utils.c $(SRCDIR)/ipc/shm.c
PLAYER_OBJ := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(PLAYER_SRC))
MASTER_SRC := $(SRCDIR)/master/master.c \
				  $(SRCDIR)/master/args.c \
				  $(SRCDIR)/master/timing.c \
			  $(SRCDIR)/core/game.c \
			  $(SRCDIR)/ipc/sync_init.c \
			  $(SRCDIR)/ipc/notify.c \
				  $(SRCDIR)/ipc/proc.c \
				  $(SRCDIR)/master/cleanup.c \
			  $(SRCDIR)/ipc/shm.c

MASTER_OBJ := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(MASTER_SRC))

# Binario propio (compilado desde src/master.c)
MASTER_BIN := $(abspath $(BINDIR)/master)

W ?= 10
H ?= 10
PLAYERS ?= 9   # cantidad de jugadores por defecto

# Rutas absolutas a los binarios
PLAYER_BIN := $(abspath $(BINDIR)/player)
VIEW_BIN   := $(abspath $(BINDIR)/view)

# genera: /abs/bin/player /abs/bin/player ... (N veces)
PLAYER_LIST := $(foreach i,$(shell seq 1 $(PLAYERS)),$(PLAYER_BIN))

# Detectar arquitectura para ChompChamps
ARCH := $(shell uname -m)
ifeq ($(ARCH),x86_64)
  CHOMPCHAMPS_BIN := $(abspath ChompChamps_amd64)
else ifeq ($(ARCH),aarch64)
  CHOMPCHAMPS_BIN := $(abspath ChompChamps_arm64)
else
  CHOMPCHAMPS_BIN := $(abspath ChompChamps_amd64)  # fallback
endif

.PHONY: all clean dirs view player master run-master master-run run-chompchamps run-valgrind clean-shm asan format help
asan: deps
	$(MAKE) clean
	$(MAKE) SAN=1 all

all: dirs view player master

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
	@mkdir -p $(BINDIR) $(OBJDIR) $(OBJDIR)/master $(OBJDIR)/core $(OBJDIR)/player $(OBJDIR)/view $(OBJDIR)/ipc

clean: clean-shm
	@$(RM) -r $(OBJDIR) $(BINDIR)/view $(BINDIR)/player $(BINDIR)/master $(BINDIR)/*.log
	@echo "Limpio."

clean-shm:
	@echo "Limpiando memoria compartida..."
	@-rm -f /dev/shm/game_state /dev/shm/game_sync
	@echo "Memoria compartida limpiada."

# Ejecuta *nuestro* master con rutas ABS a view/player
run-master: deps master view player
	@echo "MASTER=$(MASTER_BIN)"
	@echo "VIEW  =$(VIEW_BIN)"
	@echo "PLAYERS=$(PLAYERS)"
	@echo "PLAYER_LIST=$(PLAYER_LIST)"
	$(MASTER_BIN) -w $(W) -h $(H) -v $(VIEW_BIN) -p $(PLAYER_LIST)

# Alias conveniente
master-run: run-master

# Ejecuta con ChompChamps según la arquitectura
run-chompchamps: deps view player
	@echo "CHOMPCHAMPS=$(CHOMPCHAMPS_BIN)"
	@echo "VIEW  =$(VIEW_BIN)"
	@echo "PLAYERS=$(PLAYERS)"
	@echo "PLAYER_LIST=$(PLAYER_LIST)"
	@echo "Arquitectura detectada: $(ARCH)"
	$(CHOMPCHAMPS_BIN) -w $(W) -h $(H) -v $(VIEW_BIN) -p $(PLAYER_LIST)

# Ejecuta con valgrind usando nuestro master
run-valgrind: deps master view player
	@echo "Ejecutando con valgrind..."
	@echo "MASTER=$(MASTER_BIN)"
	@echo "VIEW  =$(VIEW_BIN)"
	@echo "PLAYERS=$(PLAYERS)"
	@echo "PLAYER_LIST=$(PLAYER_LIST)"
	valgrind --leak-check=full \
	  --show-leak-kinds=all \
	  --track-origins=yes \
	  --track-fds=yes \
	  --error-exitcode=1 \
	  $(MASTER_BIN) -w $(W) -h $(H) -v $(VIEW_BIN) -p $(PLAYER_LIST)

FORMAT = clang-format
FORMAT_FLAGS = -i
SRC = $(shell find src -name '*.c' -print) $(wildcard include/*.h)

format:
	$(FORMAT) $(FORMAT_FLAGS) $(SRC)

.PHONY: deps

deps:
	apt-get update
	apt-get install -y libncurses5-dev libncursesw5-dev

# Ayuda - muestra todas las opciones disponibles
help:
	@echo "Opciones disponibles:"
	@echo "  all              - Compilar proyecto sin asan"
	@echo "  asan             - Compilar proyecto con AddressSanitizer"
	@echo "  run-master       - Ejecutar con binario master propio"
	@echo "  run-chompchamps  - Ejecutar con ChompChamps (detecta arquitectura automáticamente)"
	@echo "  run-valgrind     - Ejecutar con valgrind usando master propio"
	@echo "  clean            - Limpiar archivos compilados y memoria compartida"
	@echo "  clean-shm        - Limpiar solo memoria compartida"
	@echo "  format           - Formatear código con clang-format"
	@echo ""
	@echo "Variables configurables:"
	@echo "  W=10             - Ancho del tablero (default: 10)"
	@echo "  H=10             - Alto del tablero (default: 10)"
	@echo "  PLAYERS=9        - Número de jugadores (default: 9)"
	@echo ""
	@echo "Ejemplos:"
	@echo "  make run-master W=15 H=15 PLAYERS=5"
	@echo "  make run-chompchamps"
	@echo "  make run-valgrind"
