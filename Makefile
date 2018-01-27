CC=gcc
INC=./include
OUT_DIR=./out
OUT=test

FLAGS=-Wimplicit-function-declaration -Woverflow

# Default all optimizations
#CC_OPTS=-c -I $(INC) -Ofast

# Debug
CC_OPTS=-I $(INC) -g $(FLAGS)

FILES=$(wildcard *.c)

.PHONY=clean

all: $(OUT_DIR) $(OUT)

$(OUT): $(FILES)
	$(CC) $(CC_OPTS) -o $(OUT_DIR)/$(OUT) $(FILES)

$(OUT_DIR):
	mkdir $(OUT_DIR)

clean:
	if [ -d $(OUT_DIR) ]; then rm -r $(OUT_DIR); fi

vpath %.c .
