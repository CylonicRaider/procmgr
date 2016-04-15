
SRCDIR = src
OBJDIR = obj

INCLUDE = -Iinclude/

.PHONY: clean deepclean

bin/progmgr: $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(wildcard $(SRCDIR)/*.c)) | bin
	$(CC) $(LDFLAGS) -o $@ $^

obj/%.o: src/%.c | obj
	$(CC) -c $(CFLAGS) $(INCLUDE) -o $@ $<

obj bin:
	mkdir $@

clean:
	rm -rf obj
deepclean: clean
	rm -rf bin
