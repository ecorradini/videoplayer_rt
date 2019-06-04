exec1:
	mkdir -p bin
	gcc -g -Wall -o bin/videoplayer src/videoplayer.c -lavformat -lavcodec -lswscale -lz -lavutil -lm `sdl-config --cflags --libs`
	gcc -c src/libdl/dl_syscalls.c -o src/libdl/dl_syscalls.o
	ar rcs src/libdl/libdl_syscalls.a src/libdl/dl_syscalls.o
	gcc -g -Wall -o bin/player_spawner src/player_spawner.c -Lsrc/libdl -ldl_syscalls
	ln -s bin/player_spawner player
clean:
	rm -rf bin
	rm -rf src/libdl/dl_syscalls.o
	rm -rf src/libdl/libdl_syscalls.a
	rm -rf player