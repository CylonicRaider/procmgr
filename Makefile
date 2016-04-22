
SRCDIR = src
OBJDIR = obj

CCFLAGS = -Wall -Iinclude/

OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(wildcard $(SRCDIR)/*.c))

.PHONY: objects test debug clean deepclean

bin/progmgr: $(OBJECTS) | bin
	$(CC) $(LDFLAGS) -o $@ obj/*.o

objects: $(OBJECTS)

obj/%.o: src/%.c | obj
	$(CC) -c $(CFLAGS) $(CCFLAGS) -o $@ $<

obj bin:
	mkdir $@

test: bin/progmgr
	bin/progmgr test.cfg

debug: bin/progmgr
	gdb bin/progmgr

clean:
	rm -rf obj core
deepclean: clean
	rm -rf bin
