# encircle - a Xinerama mouse monitor wrapper
# See LICENSE file for copyright and license details.

include config.mk

SRC = encircle.c util.c
OBJ = ${SRC:.c=.o}

all: options encircle

options:
	@echo encircle build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	${CC} -c ${CFLAGS} $<

encircle: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	rm -f encircle ${OBJ} encircle-${VERSION}.tar.gz

dist: clean
	mkdir -p encircle-${VERSION}
	cp -R LICENSE Makefile config.mk\
		encircle.1 util.h ${SRC} encircle-${VERSION}
	tar -cf encircle-${VERSION}.tar encircle-${VERSION}
	gzip encircle-${VERSION}.tar
	rm -rf encircle-${VERSION}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f encircle ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/encircle
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	sed "s/VERSION/${VERSION}/g" < encircle.1 > ${DESTDIR}${MANPREFIX}/man1/encircle.1
	chmod 644 ${DESTDIR}${MANPREFIX}/man1/encircle.1

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/encircle\
		${DESTDIR}${MANPREFIX}/man1/encircle.1

.PHONY: all options clean dist install uninstall
