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

.PHONY: all clean dirs view player run-master clean-shm asan
asan:
	$(MAKE) clean
	$(MAKE) SAN=1 all

all: dirs view player

view: $(BINDIR)/view
player: $(BINDIR)/player

$(BINDIR)/view: $(VIEW_OBJ) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@ $(LDFLAGS) $(LDLIBS)
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
	@echo "VIEW  =$(abspath $(BINDIR)/view)"
	@echo "PLAYER=$(abspath $(BINDIR)/player)"
	$(MASTER) -w $(W) -h $(H) -p $(abspath $(BINDIR)/player) -v $(abspath $(BINDIR)/view)

# Si hubo residuos en /dev/shm
clean-shm:
	-@rm -f /dev/shm/game_state /dev/shm/game_sync 2>/dev/null || true

FORMAT = clang-format
FORMAT_FLAGS = -i

SRC = $(wildcard src/*.c include/*.h)

format:
	$(FORMAT) $(FORMAT_FLAGS) $(SRC)

# valgrind   --leak-check=full --show-leak-kinds=all --track-origins=yes --track-fds=yes   --trace-children=yes --trace-children-skip=/bin/*,/usr/bin/*   --log-file="$(pwd)/bin/valgrind-%p.log"   ./ChompChamps_arm64 -w 10 -h 10   -v "$(pwd)/bin/view"   -p "$(pwd)/bin/player"
