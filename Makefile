CFLAGS += -Wall -Werror -Wpedantic -larchive
.PHONY: clean

server: res.zip server.bin
	cat server.bin res.zip > server
	chmod +x server

res.zip: res
	(cd res && zip ../res.zip *)

server.bin: src/markup.h src/markup.o src/main.o src/mongoose.o
	$(CC) $(CFLAGS) src/markup.o src/main.o src/mongoose.o -o server.bin

clean:
	rm -f src/*.o server server.bin res.zip
