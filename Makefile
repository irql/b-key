CC=gcc
INC=./include
OUT_DIR=./out
OUT=$(OUT_DIR)/test
DOC_DIR=./docs

FLAGS=-Wimplicit-function-declaration -Woverflow

# Default all optimizations
#CC_OPTS=-I $(INC) -Ofast

# Debug
CC_OPTS=-I $(INC) -g $(FLAGS)

FILES=$(wildcard *.c)

.PHONY=clean

all: $(OUT_DIR) $(OUT) $(DOC_DIR)

$(DOC_DIR): Doxyfile
	doxygen

$(OUT): $(FILES)
	$(CC) $(CC_OPTS) -o $(OUT) $(FILES)

$(OUT_DIR):
	mkdir $(OUT_DIR)

clean:
	if [ -d $(OUT_DIR) ]; then rm -r $(OUT_DIR); fi
	if [ -d $(DOC_DIR) ]; then rm -r $(DOC_DIR); fi

vpath %.c .
