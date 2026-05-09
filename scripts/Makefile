CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -O2
LDLIBS ?=
PREFIX ?= /usr/local
USER_PREFIX ?= $(HOME)/.local
SHELL_RC ?= $(HOME)/.bashrc

BIN := tsql
SRC := src/tsql.c

.PHONY: all clean install install-user path-user uninstall-user test test-crud

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

install: $(BIN)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(BIN) $(DESTDIR)$(PREFIX)/bin/$(BIN)

install-user: $(BIN)
	install -d $(USER_PREFIX)/bin
	install -m 755 $(BIN) $(USER_PREFIX)/bin/$(BIN)
	@echo "installed: $(USER_PREFIX)/bin/$(BIN)"
	@echo "If 'tsql' is not found, run: make path-user && source $(SHELL_RC)"

path-user:
	@mkdir -p $(USER_PREFIX)/bin
	@if ! grep -F '$(USER_PREFIX)/bin' $(SHELL_RC) >/dev/null 2>&1; then \
		echo '' >> $(SHELL_RC); \
		echo '# tsql user bin path' >> $(SHELL_RC); \
		echo 'export PATH="$$HOME/.local/bin:$$PATH"' >> $(SHELL_RC); \
		echo "added $(USER_PREFIX)/bin to $(SHELL_RC)"; \
	else \
		echo "$(USER_PREFIX)/bin already exists in $(SHELL_RC)"; \
	fi
	@echo "Run: source $(SHELL_RC)"

uninstall-user:
	rm -f $(USER_PREFIX)/bin/$(BIN)

test: $(BIN)
	chmod +x test_tsql.sh
	./test_tsql.sh

test-crud: $(BIN)
	chmod +x tests/test.sh
	./tests/test.sh

clean:
	rm -f $(BIN)
