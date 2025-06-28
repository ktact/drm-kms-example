CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I/usr/include/drm
LIBS = -ldrm -lm

TARGET = triangle
SOURCES = main.c drm.c triangle.c
OBJECTS = $(SOURCES:.c=.o)
HEADERS = drm.h triangle.h

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS) $(LIBS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJECTS)

.PHONY: clean