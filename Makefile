# ==== toolchain ====
CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -Werror -O2 -D_POSIX_C_SOURCE=200809L
# En muchas imágenes modernas NO hace falta -lrt; para semáforos a veces ayuda -pthread.
LIBS    := -pthread
INCLUDE := -Iinclude

# ==== layout ====
SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin

# ==== fuentes/objetos ====
COMMON_OBJS := $(OBJ_DIR)/shm.o

WRITER_OBJS := $(OBJ_DIR)/writer_shm.o $(COMMON_OBJS)
READER_OBJS := $(OBJ_DIR)/reader_shm.o $(COMMON_OBJS)

WRITER_BIN  := $(BIN_DIR)/writer_shm
READER_BIN  := $(BIN_DIR)/reader_shm

# ==== reglas principales ====
.PHONY: all clean veryclean dirs test run-writer run-reader clean-shm

all: dirs $(WRITER_BIN) $(READER_BIN)

$(WRITER_BIN): $(WRITER_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(READER_BIN): $(READER_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# Compilación genérica de .c -> .o
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c include/ipc.h include/shm.h
	$(CC) $(CFLAGS) $(INCLUDE) -c -o $@ $<

dirs:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR)

clean:
	@rm -rf $(OBJ_DIR) $(BIN_DIR)

# Intenta borrar los objetos de shm del sistema (para reiniciar corridas)
SHM_STATE := /game_state
SHM_SYNC  := /game_sync
clean-shm:
	-@if [ -e "/dev/shm$(SHM_STATE)" ]; then echo "Borrando /dev/shm$(SHM_STATE)"; rm -f "/dev/shm$(SHM_STATE)"; fi
	-@if [ -e "/dev/shm$(SHM_SYNC)"  ]; then echo "Borrando /dev/shm$(SHM_SYNC)";  rm -f "/dev/shm$(SHM_SYNC)";  fi

veryclean: clean clean-shm

# ==== atajos de ejecución (M0) ====
# Podés overridear: make run-writer W=12 H=10 S=123
W ?= 12
H ?= 10
S ?= 123
run-writer: $(WRITER_BIN)
	$(WRITER_BIN) -w $(W) -h $(H) -s $(S)

# Podés overridear: make run-reader R=3
R ?= 3
run-reader: $(READER_BIN)
	$(READER_BIN) -r $(R)

# Test rápido M0: crea shm y luego lee 3 filas
test: all
	@echo "== test M0 =="
	@$(WRITER_BIN) -w 12 -h 10 -s 777
	@$(READER_BIN) -r 3
	@echo "== OK =="
