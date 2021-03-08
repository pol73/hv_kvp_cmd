LOCAL?= /usr/local

.ifdef DEBUG
CFLAGS+= -g -O0 -UNDEBUG
.else
CFLAGS?= -O
CFLAGS+= -DNDEBUG
STRIPPED= -s
.endif

CORE= hv_kvp_cmd

${CORE}: ${.TARGET}.c
	cc -Wall -Wextra ${CFLAGS} -o ${.TARGET} ${.ALLSRC}

all: ${CORE}

install:
	install ${STRIPPED} -m 555 "${CORE}" ${LOCAL}/bin/

deinstall:
	rm -f "${LOCAL}/bin/${CORE}"

clean:
	rm -f ".depend" "${CORE}"

depend:
	rm -f ".depend"
	mkdep -a ${CFLAGS} ${CORE}

-include .depend
