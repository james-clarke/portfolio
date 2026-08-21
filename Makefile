CC     	:= gcc
CFLAGS 	+= -std=c11 -Wall -Wextra -g -O0 -MMD -MP -Isrc
ASAN   	:= -fsanitize=address

BN 	:= build/native

SRCS 	:= $(wildcard src/*.c) $(wildcard native/*.c)
OBJS 	:= $(patsubst %.c,$(BN)/%.o,$(notdir $(SRCS)))

WCC    	:= clang
WFLAGS 	+= --target=wasm32 -std=c11 -Wall -Wextra -Oz -nostdlib -fno-builtin -MMD -MP -Isrc

BW 	:= build/wasm

WOBJS 	:= $(BW)/field.o $(BW)/shim.o

VPATH 	:= src native wasm

native: $(BN)/waterfall

wasm: web/waterfall.wasm

web/waterfall.wasm: $(WOBJS)
	$(WCC) $(WFLAGS) -Wl,--no-entry -o $@ $^

$(BW)/%.o: %.c | $(BW)
	$(WCC) $(WFLAGS) -c -o $@ $<

$(BW):
	mkdir -p $@

$(BN)/waterfall: $(OBJS)
	$(CC) $(CFLAGS) $(ASAN) -o $@ $^

$(BN)/%.o: %.c | $(BN)
	$(CC) $(CFLAGS) $(ASAN) -c -o $@ $<

$(BN):
	mkdir -p $@

clean:
	rm -rf build

-include $(BN)/*.d $(BW)/*.d

.PHONY: native wasm clean
