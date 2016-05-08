# procmgr -- init-like process manager
# https://github.com/CylonicRaider/procmgr

SRCDIR = src
OBJDIR = obj

CCFLAGS = -Wall -Iinclude/

OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(wildcard $(SRCDIR)/*.c))

.PHONY: objects install debug clean deepclean

bin/procmgr: $(OBJECTS) | bin
	$(CC) $(LDFLAGS) -o $@ obj/*.o

objects: $(OBJECTS)

obj/%.o: src/%.c include/* | obj
	$(CC) -c $(CFLAGS) $(CCFLAGS) -o $@ $<

obj bin:
	mkdir -p $@

install: bin/procmgr
	cp bin/procmgr /usr/local/bin/procmgr
	chmod a+x /usr/local/bin/procmgr

debug: bin/procmgr
	gdb bin/procmgr $$([ -r core ] && echo core)

clean:
	rm -rf obj core
deepclean: clean
	rm -rf bin
