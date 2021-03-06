CC=gcc
INC=./include
OUT_DIR=./out
OUT=$(OUT_DIR)/test

FLAGS=-Wimplicit-function-declaration -Woverflow -fdiagnostics-color=always --std=c1x

# Default all optimizations
#CC_OPTS=-I $(INC) -Ofast

# Debug
CC_OPTS=-I $(INC) -g $(FLAGS)

FILES=$(wildcard *.c)

.PHONY=clean

all: $(OUT_DIR) $(OUT)

$(OUT): $(FILES)
	$(CC) $(CC_OPTS) -o $(OUT) $(FILES)

$(OUT_DIR):
	mkdir $(OUT_DIR)

clean:
	if [ -d $(OUT_DIR) ]; then rm -r $(OUT_DIR); fi

vpath %.c .
