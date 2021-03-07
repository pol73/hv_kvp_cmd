PREFIX?= /usr/local

.ifdef DEBUG
CFLAGS+= -g -O0 -UNDEBUG
.else
CFLAGS+= -O -DNDEBUG
STRIPPED= -s
.endif

CORE= hv_kvp_cmd

${CORE}: ${.TARGET}.c
	cc -Wall -Wextra ${CFLAGS} -o ${.TARGET} ${.ALLSRC}

all: build install

build: ${CORE}

install:
	install ${STRIPPED} -m 555 -o root -g wheel "${CORE}" ${PREFIX}/bin/

deinstall:
	rm -f "${PREFIX}/bin/${CORE}"

clean:
	rm -f ".depend" "${CORE}"
