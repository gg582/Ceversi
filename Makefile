CC = gcc
CFLAGS = -Wall -Wextra -O3
LDFLAGS = -lcwist -lttak -lcjson -lssl -lcrypto -luriparser -lpthread -ldl -lm

SRCS = \
	src/app/main.c \
	src/core/utils.c \
	src/core/memory.c \
	src/data/db.c \
	src/game/betting_logic.c \
	src/http/handlers_shared.c \
	src/http/handlers_game.c \
	src/http/handlers_auth.c \
	src/http/handlers_session.c \
	src/http/handlers_betting.c \
	src/http/handlers_page.c
OBJS = $(SRCS:.c=.o)
TARGET = server
WASM_SRC = src/game/betting_logic_wasm.c
WASM_OUT = public/betting_logic.wasm

all: $(TARGET) wasm

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) $(WASM_OUT)

wasm: $(WASM_OUT)

$(WASM_OUT): $(WASM_SRC) src/game/betting_logic.c src/game/betting_logic.h
	clang --target=wasm32 -O3 -nostdlib \
	-Wl,--no-entry -Wl,--export=wasm_betting_single_delta \
	-Wl,--export=wasm_betting_can_wager -Wl,--export=wasm_betting_project_points \
	-Wl,--export=wasm_betting_multiplayer_reward \
	$(WASM_SRC) src/game/betting_logic.c -o $(WASM_OUT)

.PHONY: all clean wasm
