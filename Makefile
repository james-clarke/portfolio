CC     	:= gcc
CFLAGS 	+= -std=c11 -Wall -Wextra -g -O0 -MMD -MP -Isrc
ASAN   	:= -fsanitize=address

BN 	:= build/native

SRCS 	:= $(wildcard src/*.c) $(wildcard native/*.c)
OBJS 	:= $(patsubst %.c,$(BN)/%.o,$(notdir $(SRCS)))

VPATH 	:= src native

native: $(BN)/waterfall

$(BN)/waterfall: $(OBJS)
	$(CC) $(CFLAGS) $(ASAN) -o $@ $^

$(BN)/%.o: %.c | $(BN)
	$(CC) $(CFLAGS) $(ASAN) -c -o $@ $<

$(BN):
	mkdir -p $@

clean:
	rm -rf build

-include $(BN)/*.d

.PHONY: native clean
