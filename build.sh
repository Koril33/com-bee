gcc $(pkg-config --cflags gtk4 libserialport) main.c $(pkg-config --libs gtk4 libserialport)
