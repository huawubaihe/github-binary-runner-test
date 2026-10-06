CC := $(if $(filter cc gcc,$(CC)),x86_64-w64-mingw32-gcc,$(CC))
CC := $(if $(CC),$(CC),x86_64-w64-mingw32-gcc)
CFLAGS = -O2 -Wall -Wextra -municode -static
LDFLAGS = -municode -static
all: github-binary-runner.exe
github-binary-runner.exe: src/main.c
	$(CC) $(CFLAGS) -o $@ $< -lwinhttp $(LDFLAGS)
clean:
	rm -f github-binary-runner.exe
