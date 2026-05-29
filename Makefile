CC      = gcc
CFLAGS  = -Wall -Wextra -Werror -pedantic -g -Iinclude -Wno-newline-eof
LDFLAGS = -Llib -ldata_structures

BUILD   = build
SRC     = src

# ─── Data Structures (shared library) ─────────────────
DS_SRC  = $(SRC)/ds/HashMap.c $(SRC)/ds/vector.c \
          $(SRC)/ds/genqueue.c $(SRC)/ds/gen_dlist.c

lib/libdata_structures.so: $(DS_SRC)
	mkdir -p lib
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $^

# ─── Server ────────────────────────────────────────────
SERVER_SRC = $(SRC)/server_main.c $(SRC)/server_mng.c \
             $(SRC)/server_net.c  $(SRC)/protocol.c

server_app: lib/libdata_structures.so $(SERVER_SRC)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $(BUILD)/$@ $(SERVER_SRC) $(LDFLAGS)

# ─── Client ────────────────────────────────────────────
CLIENT_SRC = $(SRC)/client_main.c $(SRC)/client_mng.c \
             $(SRC)/client_net.c  $(SRC)/client_ui.c  \
             $(SRC)/protocol.c

client_app: lib/libdata_structures.so $(CLIENT_SRC)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $(BUILD)/$@ $(CLIENT_SRC) $(LDFLAGS)

# ─── Multicast ─────────────────────────────────────────
sender: $(SRC)/sender.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $(BUILD)/$@ $^

receiver: $(SRC)/receiver.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $(BUILD)/$@ $^

# ─── Tests ─────────────────────────────────────────────
TEST_SRC = tests/test_runner.c  tests/test_protocol.c \
           tests/test_ds.c      tests/test_server.c

test: lib/libdata_structures.so
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $(BUILD)/test_runner \
	    $(TEST_SRC) \
	    $(SRC)/protocol.c \
	    $(SRC)/server_mng.c \
	    $(SRC)/server_net.c \
	    $(LDFLAGS)
	LD_LIBRARY_PATH=lib ./$(BUILD)/test_runner

# ─── Shortcuts ─────────────────────────────────────────
all: server_app client_app sender receiver

clean:
	rm -rf $(BUILD) lib/libdata_structures.so

.PHONY: all clean test server_app client_app sender receiver
