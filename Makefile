# ====== Compilación y flags ======
CC       := gcc
CSTD     := -std=c11
WARN     := -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wpointer-arith -Wcast-qual
OPT      := -O2
DEFS     := -D_POSIX_C_SOURCE=200809L
CFLAGS   := $(CSTD) $(WARN) $(OPT) $(DEFS)
LDFLAGS  :=
LDLIBS   := -pthread -lrt

# Sanitizer opcional: make SAN=1
SAN ?= 0
SAN_CFLAGS  := -g -O1 -fsanitize=address -fno-omit-frame-pointer
SAN_LDFLAGS := -fsanitize=address
ifeq ($(SAN),1)
  CFLAGS  += $(SAN_CFLAGS)
  LDFLAGS += $(SAN_LDFLAGS)
endif

# ====== Directorios ======
ROOT     := .
INCDIR   := $(ROOT)/include
SRCDIR   := $(ROOT)/src
BINDIR   := $(ROOT)/bin
OBJDIR   := $(ROOT)/.obj
INCLUDES := -I$(INCDIR)

# ====== Fuentes / Objetos ======
VIEW_SRC    := $(SRCDIR)/view.c   $(SRCDIR)/shm.c
VIEW_OBJ    := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(VIEW_SRC))
PLAYER_SRC  := $(SRCDIR)/player.c $(SRCDIR)/shm.c
PLAYER_OBJ  := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(PLAYER_SRC))
MASTER_SRC  := $(SRCDIR)/master.c $(SRCDIR)/shm.c
MASTER_OBJ  := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(MASTER_SRC))

# ====== Master de la cátedra (detección de arquitectura) ======
ARCH := $(shell uname -m)
ifeq ($(ARCH),x86_64)
  MASTER_CATEDRA ?= ./ChompChamps_amd64
else ifeq ($(ARCH),aarch64)
  MASTER_CATEDRA ?= ./ChompChamps_arm64
else ifeq ($(ARCH),arm64)
  MASTER_CATEDRA ?= ./ChompChamps_arm64
else
  $(error Arquitectura desconocida: $(ARCH). Definí MASTER_CATEDRA manualmente)
endif

# ====== Parámetros de ejecución ======
W ?= 10
H ?= 10
PLAYERS ?= 1   # cantidad de jugadores por defecto

# Rutas ABS a binarios
PLAYER_BIN := $(abspath $(BINDIR)/player)
VIEW_BIN   := $(abspath $(BINDIR)/view)
MASTER_BIN := $(abspath $(BINDIR)/master)

# Lista de N players ABS (/abs/bin/player ... N veces)
PLAYER_LIST := $(foreach i,$(shell seq 1 $(PLAYERS)),$(PLAYER_BIN))

# ====== Targets ======
.PHONY: all clean dirs view player master-local master-catedra run-master run-catedra clean-shm asan format

asan:
	$(MAKE) clean
	$(MAKE) SAN=1 all

# Compila todo (incluye tu master local)
all: dirs view player master-local

view:   $(BINDIR)/view
player: $(BINDIR)/player
master-local: $(BINDIR)/master
master-catedra:
	@echo "Usando master de la cátedra: $(MASTER_CATEDRA)"

# Binarios
$(BINDIR)/view: $(VIEW_OBJ) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@ $(LDFLAGS) $(LDLIBS) -lncurses

$(BINDIR)/player: $(PLAYER_OBJ) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@ $(LDFLAGS) $(LDLIBS)

$(BINDIR)/master: $(MASTER_OBJ) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@ $(LDFLAGS) $(LDLIBS)

# Objetos
$(OBJDIR)/%.o: $(SRCDIR)/%.c | dirs
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

dirs:
	@mkdir -p $(BINDIR) $(OBJDIR)

clean:
	@$(RM) -r $(OBJDIR) $(BINDIR)/view $(BINDIR)/player $(BINDIR)/master $(BINDIR)/*.log
	@echo "Limpio."

# ====== Ejecución ======
# Por defecto ahora corre tu master local
run-master: view player master-local
	@echo "MASTER     = $(MASTER_BIN)"
	@echo "VIEW       = $(VIEW_BIN)"
	@echo "PLAYERS    = $(PLAYERS)"
	@echo "PLAYER_LIST= $(PLAYER_LIST)"
	$(MASTER_BIN) -w $(W) -h $(H) -v $(VIEW_BIN) -p $(PLAYER_LIST)

# Conveniencia: correr siempre con el master de la cátedra
run-catedra: view player
	@echo "MASTER     = $(MASTER_CATEDRA)"
	@echo "VIEW       = $(VIEW_BIN)"
	@echo "PLAYERS    = $(PLAYERS)"
	@echo "PLAYER_LIST= $(PLAYER_LIST)"
	$(MASTER_CATEDRA) -w $(W) -h $(H) -v $(VIEW_BIN) -p $(PLAYER_LIST)

# ====== Utils ======
# Si hubo residuos en /dev/shm
clean-shm:
	-@rm -f /dev/shm/game_state /dev/shm/game_sync 2>/dev/null || true

# Formateo
FORMAT = clang-format
FORMAT_FLAGS = -i
SRC = $(wildcard src/*.c include/*.h)

format:
	$(FORMAT) $(FORMAT_FLAGS) $(SRC)
