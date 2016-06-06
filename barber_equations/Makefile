BARBER            = barber
EQUATIONS         = equations
BARBER_SOURCES    = barber.c
EQUATIONS_SOURCES = equations.c

CFLAGS         = -std=gnu99 -Wall

CC                = gcc
BARBER_OBJECTS    = $(BARBER_SOURCES:.c=.o)
EQUATIONS_OBJECTS = $(EQUATIONS_SOURCES:.c=.o)

.SUFFIXES: .c .o

.c.o:
		$(CC) $(CFLAGS) -c $<

all:		$(BARBER) $(EQUATIONS)

rebuild:	clean all

$(BARBER):	$(BARBER_OBJECTS)
		$(CC) $(BARBER_OBJECTS) -o $@

$(EQUATIONS):	$(EQUATIONS_OBJECTS)
		$(CC) $(EQUATIONS_OBJECTS) -o $@

clean:
	rm -fr *core* *~ $(BARBER_OBJECTS) $(EQUATIONS_OBJECTS) $(BARBER) $(EQUATIONS) .make.state .sb
