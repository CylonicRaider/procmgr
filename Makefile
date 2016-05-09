# procmgr -- init-like process manager
# https://github.com/CylonicRaider/procmgr

SRCDIR = src
OBJDIR = obj

CCFLAGS = -Wall -Iinclude/

OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(wildcard $(SRCDIR)/*.c))

.PHONY: debug install clean deepclean

bin/procmgr: $(OBJECTS) | bin
	$(CC) $(LDFLAGS) -o $@ obj/*.o

obj/%.o: src/%.c include/* | obj
	$(CC) -c $(CFLAGS) $(CCFLAGS) -o $@ $<

obj bin:
	mkdir -p $@

debug: bin/procmgr
	gdb bin/procmgr $$([ -r core ] && echo core)

install: bin/procmgr
	cp bin/procmgr /usr/local/bin/procmgr
	chmod a+x /usr/local/bin/procmgr

clean:
	rm -rf obj core
deepclean: clean
	rm -rf bin
