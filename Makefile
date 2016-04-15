
SRCDIR = src
OBJDIR = obj

INCLUDE = -Iinclude/

.PHONY: objects clean deepclean

bin/progmgr: objects
	$(CC) $(LDFLAGS) -o $@ $^

objects: $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(wildcard $(SRCDIR)/*.c)) | bin

obj/%.o: src/%.c | obj
	$(CC) -c $(CFLAGS) $(INCLUDE) -o $@ $<

obj bin:
	mkdir $@

clean:
	rm -rf obj
deepclean: clean
	rm -rf bin
