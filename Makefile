########################################
##
## Makefile
## LINUX compilation
##
##############################################

# Flags
CFLAGS = -Wall -pedantic

# Math library

MATH_LIBS = -lm -lpthread

# Includes.
INCLUDES=  -I.

# Compiling all

all:
	gcc $(CFLAGS) socket_server.c -o ss

# Clean obj files
clean:
	rm -f *.o; rm -f ss


