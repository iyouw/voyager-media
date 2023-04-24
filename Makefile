src := src

.PHONY: main demux_decode

main:
	$(MAKE) main --directory=$(src)


demux_decode:
	${MAKE} demux_decode --directory=$(src)

clean:
	${MAKE} clean --directory=$(src)