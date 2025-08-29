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

VIEW_SRC   := $(SRCDIR)/view.c $(SRCDIR)/view_utils.c $(SRCDIR)/shm.c
VIEW_OBJ   := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(VIEW_SRC))
PLAYER_SRC := $(SRCDIR)/player.c $(SRCDIR)/player_utils.c $(SRCDIR)/shm.c
PLAYER_OBJ := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(PLAYER_SRC))
MASTER_SRC := $(SRCDIR)/master.c $(SRCDIR)/shm.c
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

.PHONY: all clean dirs view player master run-master master-run clean-shm asan format
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
	@mkdir -p $(BINDIR) $(OBJDIR)

clean:
	@$(RM) -r $(OBJDIR) $(BINDIR)/view $(BINDIR)/player $(BINDIR)/master $(BINDIR)/*.log
	@echo "Limpio."

# Ejecuta *nuestro* master con rutas ABS a view/player
run-master: deps master view player
	@echo "MASTER=$(MASTER_BIN)"
	@echo "VIEW  =$(VIEW_BIN)"
	@echo "PLAYERS=$(PLAYERS)"
	@echo "PLAYER_LIST=$(PLAYER_LIST)"
	$(MASTER_BIN) -w $(W) -h $(H) -v $(VIEW_BIN) -p $(PLAYER_LIST)

# Alias conveniente
master-run: run-master

FORMAT = clang-format
FORMAT_FLAGS = -i
SRC = $(wildcard src/*.c include/*.h)

format:
	$(FORMAT) $(FORMAT_FLAGS) $(SRC)

.PHONY: deps

deps:
	apt-get update
	apt-get install -y libncurses5-dev libncursesw5-dev
