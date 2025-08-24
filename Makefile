CC       := gcc
CSTD     := -std=c11
WARN     := -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wpointer-arith -Wcast-qual
OPT      := -O2
DEFS     := -D_POSIX_C_SOURCE=200809L
CFLAGS   := $(CSTD) $(WARN) $(OPT) $(DEFS)
LDLIBS   := -pthread -lrt

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

VIEW_SRC   := $(SRCDIR)/view.c   $(SRCDIR)/shm.c
VIEW_OBJ   := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(VIEW_SRC))
PLAYER_SRC := $(SRCDIR)/player.c $(SRCDIR)/shm.c
PLAYER_OBJ := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(PLAYER_SRC))

# Detección de arquitectura para elegir binario del master
ARCH := $(shell uname -m)

ifeq ($(ARCH),x86_64)
  MASTER ?= ./ChompChamps_amd64
else ifeq ($(ARCH),aarch64)
  MASTER ?= ./ChompChamps_arm64
else
  $(error Arquitectura desconocida: $(ARCH). Definí MASTER manualmente)
endif

W ?= 10
H ?= 10
PLAYERS ?= 1   # cantidad de jugadores por defecto

# Rutas absolutas a los binarios
PLAYER_BIN := $(abspath $(BINDIR)/player)
VIEW_BIN   := $(abspath $(BINDIR)/view)

# genera: /abs/bin/player /abs/bin/player ... (N veces)
PLAYER_LIST := $(foreach i,$(shell seq 1 $(PLAYERS)),$(PLAYER_BIN))

.PHONY: all clean dirs view player run-master clean-shm asan
asan:
	$(MAKE) clean
	$(MAKE) SAN=1 all

all: dirs view player

view: $(BINDIR)/view
player: $(BINDIR)/player

$(BINDIR)/view: $(VIEW_OBJ) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@ $(LDFLAGS) $(LDLIBS) -lncurses
$(BINDIR)/player: $(PLAYER_OBJ) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@ $(LDFLAGS) $(LDLIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | dirs
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

dirs:
	@mkdir -p $(BINDIR) $(OBJDIR)

clean:
	@$(RM) -r $(OBJDIR) $(BINDIR)/view $(BINDIR)/player $(BINDIR)/*.log
	@echo "Limpio."

# Ejecuta master con rutas ABS a view/player
run-master: view player
	@echo "MASTER=$(MASTER)"
	@echo "VIEW  =$(VIEW_BIN)"
	@echo "PLAYERS=$(PLAYERS)"
	@echo "PLAYER_LIST=$(PLAYER_LIST)"
	$(MASTER) -w $(W) -h $(H) -v $(VIEW_BIN) -p $(PLAYER_LIST)

# Si hubo residuos en /dev/shm
clean-shm:
	-@rm -f /dev/shm/game_state /dev/shm/game_sync 2>/dev/null || true

FORMAT = clang-format
FORMAT_FLAGS = -i

SRC = $(wildcard src/*.c include/*.h)

format:
	$(FORMAT) $(FORMAT_FLAGS) $(SRC)
