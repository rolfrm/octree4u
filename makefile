OPT = -g3 -ggdb -O0
LIB_SOURCES1 = main.c octree_iterate.c octree.c
LIB_SOURCES = $(addprefix src/, $(LIB_SOURCES1))
CC = gcc
TARGET = run.exe
LIB_OBJECTS =$(LIB_SOURCES:.c=.o)
LDFLAGS= -L. $(OPT) -Wextra -fstack-protector-all -Wstack-protector --param ssp-buffer-size=4 -pie -ftrapv -D_FORTIFY_SOURCE=2
LIBS= -liron -lGL -lGLEW -lglfw -lm
ALL= $(TARGET)
CFLAGS = -Isrc/ -Iinclude/ -std=gnu11 -c $(OPT) -Wall -Wextra -Werror=implicit-function-declaration -Wformat=0 -D_GNU_SOURCE -fdiagnostics-color -Wextra  -Wwrite-strings -Werror -msse4.2 -Werror=maybe-uninitialized -fstack-protector-all -Wstack-protector --param ssp-buffer-size=4 -pie -ftrapv -D_FORTIFY_SOURCE=2 -Wl,dynamicbase



$(TARGET): $(LIB_OBJECTS)
	$(CC) $(LDFLAGS) $(LIB_OBJECTS) $(LIBS) -o $@

all: $(ALL)

.c.o: $(HEADERS)
	$(CC) $(CFLAGS) $< -o $@ -MMD -MF $@.depends 
depend: h-depend
clean:
	rm -f $(LIB_OBJECTS) $(ALL) src/*.o.depends
install: $(TARGET)
	make -f makefile.compiler
	cp -v include/* /usr/include/	
	cp -v $(TARGET) /usr/lib/
	cp -v icy_table_template.c icy_table_template.h /usr/bin/
	cp -v icy-table /usr/bin/
uninstall:
	rm -v /usr/include/icy_* || true
	rm -v /usr/lib/$(TARGET) || true
	rm -v /usr/bin/icy-table || true	
	rm -v /usr/bin/icy_* || true
	echo "done.."
.PHONY: test
test: $(TARGET)
	make -f makefile.compiler
	make -f makefile.test test

-include $(LIB_OBJECTS:.o=.o.depends)

