CFLAGS= -O2 -DNDEBUG

.ifdef DEBUG
CFLAGS= -g -O0
.endif

hv_kvp_cmd: ${.TARGET}.c
	cc -Wall -Wextra ${CFLAGS} -o ${.TARGET} ${.ALLSRC}
.ifndef DEBUG
	strip ${.TARGET}
.endif
