SRC:= src

main:
	$(MAKE) $@ --directory=$(SRC)

avio:
	${MAKE} $@ --directory=$(SRC)

avio_r:
	${MAKE} $@ --directory=$(SRC)

demux_decode_r:
	$(MAKE) $@ --directory=$(SRC)

demux_decode_p:
	$(MAKE) $@ --directory=$(SRC)

demux_decode_w_r:
	$(MAKE) $@ --directory=$(SRC)

transcode:
	$(MAKE) $@ --directory=$(SRC)

demux_decode:
	$(MAKE) $@ --directory=$(SRC)

multi_thread:
	$(MAKE) $@ --directory=$(SRC)

clean:
	${MAKE} $@ --directory=$(SRC)