OPT = -g3 -Og
LIB_SOURCES1 = main.c octree_iterate.c octree.c item_list.c stb_image.c gl_utils.c list_entity.c icydbs.c collision_detection.c tests.c ray.c list_index.c render.c game_context.c
LIB_SOURCES = $(addprefix src/, $(LIB_SOURCES1))
CC = gcc
TARGET = run.exe
LIB_OBJECTS =$(LIB_SOURCES:.c=.o)
LDFLAGS= -L. $(OPT) -Wextra 
LIBS= -liron -lGL -lGLEW -lglfw -lm -licydb
ALL= $(TARGET)
CFLAGS = -Isrc/ -Iinclude/ -std=gnu11 -c $(OPT) -Wall -Wextra -Werror=implicit-function-declaration -Wformat=0 -D_GNU_SOURCE -fdiagnostics-color -Wextra  -Wwrite-strings -Werror -msse4.2 -Werror=maybe-uninitialized -DUSE_VALGRIND -DDEBUG

$(TARGET): $(LIB_OBJECTS)
	$(CC) $(LDFLAGS) $(LIB_OBJECTS) $(LIBS) -o $@

all: $(ALL)

.c.o: $(HEADERS)
	$(CC) $(CFLAGS) $< -o $@ -MMD -MF $@.depends 
depend: h-depend
clean:
	rm -f $(LIB_OBJECTS) $(ALL) src/*.o.depends
.PHONY: test
test: $(TARGET)
	make -f makefile.compiler
	make -f makefile.test test

-include $(LIB_OBJECTS:.o=.o.depends)

