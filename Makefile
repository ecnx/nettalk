# Net Talk Makefile
OPENCORE_AMR=../opencore-amr
GSMAMR=$(OPENCORE_AMR)/opencore/codecs_v2/audio/gsm_amr
AMRNB_INC=-I $(OPENCORE_AMR)/oscl -I $(GSMAMR)/amr_nb/common/include -I $(GSMAMR)/common/dec/include -I $(GSMAMR)/amr_nb/enc/src -I $(GSMAMR)/amr_nb/dec/include -I $(GSMAMR)/amr_nb/dec/src
INCLUDES=-I include -I lib `pkg-config --cflags gtk+-3.0` $(AMRNB_INC) -I ../soxr/src
INDENT_FLAGS=-br -ce -i4 -bl -bli0 -bls -c4 -cdw -ci4 -cs -nbfda -l100 -lp -prs -nlp -nut -nbfde -npsl -nss
LIBS=$(OPENCORE_AMR)/amrnb/.libs/libopencore-amrnb.a -pthread -lmbedcrypto -lm `pkg-config --libs gtk+-3.0` -lasound ../soxr/src/libsoxr.so -lnotify

OBJS = \
	bin/sound.o \
	bin/playback.o \
	bin/uncompress.o \
	bin/capture.o \
	bin/compress.o \
	bin/startup.o \
	bin/config.o \
	bin/logger.o \
	bin/fxcrypt.o \
	bin/random.o \
	bin/util.o \
	bin/connect.o \
	bin/handshake.o \
	bin/forward.o \
	bin/nettask.o \
	bin/window.o \
	bin/socks5.o \
	bin/dns.o \
	bin/program_icon.o

all: host

icons:
	@ld -r -b binary -o bin/program_icon.o res/program_icon.jpg

internal: prepare icons
	@echo "  CC    src/sound.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/sound.c -o bin/sound.o
	@echo "  CC    src/playback.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/playback.c -o bin/playback.o
	@echo "  CC    src/uncompress.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/uncompress.c -o bin/uncompress.o
	@echo "  CC    src/capture.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/capture.c -o bin/capture.o
	@echo "  CC    src/compress.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/compress.c -o bin/compress.o
	@echo "  CC    src/window.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/window.c -o bin/window.o
	@echo "  CC    src/startup.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/startup.c -o bin/startup.o
	@echo "  CC    src/config.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/config.c -o bin/config.o
	@echo "  CC    src/logger.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/logger.c -o bin/logger.o
	@echo "  CC    src/random.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/random.c -o bin/random.o
	@echo "  CC    src/util.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/util.c -o bin/util.o
	@echo "  CC    src/connect.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/connect.c -o bin/connect.o
	@echo "  CC    src/handshake.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/handshake.c -o bin/handshake.o
	@echo "  CC    src/forward.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/forward.c -o bin/forward.o
	@echo "  CC    src/nettask.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/nettask.c -o bin/nettask.o
	@echo "  CC    src/socks5.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/socks5.c -o bin/socks5.o
	@echo "  CC    lib/fxcrypt.c"
	@$(CC) $(CFLAGS) $(INCLUDES) lib/fxcrypt.c -o bin/fxcrypt.o
	@echo "  CC    lib/dns.c"
	@$(CC) $(CFLAGS) $(INCLUDES) lib/dns.c -o bin/dns.o
	@echo "  LD    bin/nettalk"
	@$(LD) -o bin/nettalk $(OBJS) $(LDFLAGS) $(LIBS)

prepare:
	@mkdir -p bin

host:
	@make internal \
		CC=gcc \
		LD=gcc \
		CFLAGS='-c -Wall -Wextra -O2 -ffunction-sections -fdata-sections -Wstrict-prototypes' \
		LDFLAGS='-s -Wl,--gc-sections -Wl,--relax'

install:
	@cp -v bin/nettalk /usr/bin/nettalk

uninstall:
	@rm -fv /usr/bin/nettalk

indent:
	@indent $(INDENT_FLAGS) ./*/*.h
	@indent $(INDENT_FLAGS) ./*/*.c
	@rm -rf ./*/*~

clean:
	@echo "  CLEAN ."
	@rm -rf bin

analysis:
	@scan-build make
	@cppcheck --force */*.h
	@cppcheck --force */*.c
