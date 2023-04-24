SRC:= src

main:
	$(MAKE) $@ --directory=$(SRC)

avio:
	${MAKE} $@ --directory=$(SRC)

demux_decode:
	$(MAKE) $@ --directory=$(SRC)

clean:
	${MAKE} $@ --directory=$(SRC)