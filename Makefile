LOCAL?= /usr/local
MANDIR?= share/man

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
	install ${STRIPPED} -m 555 "${CORE}" "${LOCAL}/bin/"
	install -m 444 ${CORE}.1 "${LOCAL}/${MANDIR}/man1/"

deinstall:
	rm -f \
		"${LOCAL}/bin/${CORE}" \
		"${LOCAL}/${MANDIR}/man1/${CORE}.1"

clean:
	rm -f ".depend" "${CORE}"

depend:
	rm -f ".depend"
	mkdep -a ${CFLAGS} ${CORE}

-include .depend
