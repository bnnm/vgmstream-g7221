# G722.1 annex C build

CC=gcc
CROSS_CC=gcc
CROSS_STRIP=strip
#CROSS_CC=i586-mingw32msvc-gcc
#CROSS_STRIP=i586-mingw32msvc-strip
CFLAGS = -fno-builtin -O2 -Wall
#todo -O2/3 change some bytes


BASE_I=T-REC-G.722.1-200505-I!!SOFT-ZST-E/Software/Fixed-200505-Rel.2.1
INCLUDES_I +=-I./${BASE_I}/common/stl-files
INCLUDES_I +=-I./${BASE_I}/common
INCLUDES_I +=-I./${BASE_I}/decode
INCLUDES_I +=-I./${BASE_I}/encode
SRC_COM_I += ${BASE_I}/common/stl-files/basop32.c
SRC_COM_I += ${BASE_I}/common/stl-files/count.c
SRC_COM_I += ${BASE_I}/common/common.c
SRC_COM_I += ${BASE_I}/common/huff_tab.c
SRC_COM_I += ${BASE_I}/common/tables.c
SRC_DEC_I += ${BASE_I}/decode/coef2sam.c
SRC_DEC_I += ${BASE_I}/decode/dct4_s.c
SRC_DEC_I += ${BASE_I}/decode/decoder.c
SRC_ENC_I += ${BASE_I}/encode/sam2coef.c
SRC_ENC_I += ${BASE_I}/encode/dct4_a.c
SRC_ENC_I += ${BASE_I}/encode/encoder.c

BASE_F=T-REC-G.722.1-200505-I!!SOFT-ZST-E/Software/Floating-200806-Rel.2.1
INCLUDES_F +=-I./${BASE_F}/common
INCLUDES_F +=-I./${BASE_F}/decode
INCLUDES_F +=-I./${BASE_F}/encode
SRC_COM_F += ${BASE_F}/common/common.c
SRC_DEC_F += ${BASE_F}/decode/rmlt_coefs_to_samples.c
SRC_DEC_F += ${BASE_F}/encode/dct4.c
SRC_DEC_F += ${BASE_F}/decode/decoder.c
SRC_ENC_F += ${BASE_F}/encode/samples_to_rmlt_coefs.c
SRC_ENC_F += ${BASE_F}/encode/dct4.c
SRC_ENC_F += ${BASE_F}/encode/encoder.c


all: decode_i encode_i decode_f encode_f

decode_i:
	${CC} ${CFLAGS} ${INCLUDES_I} \
	${SRC_COM_I} ${SRC_DEC_I} ${BASE_I}/decode/decode.c \
	-o decode_i

encode_i:
	${CC} ${CFLAGS} ${INCLUDES_I} \
	${SRC_COM_I} ${SRC_ENC_I} ${BASE_I}/encode/encode.c \
	-o encode_i

decode_f:
	${CC} ${CFLAGS} ${INCLUDES_F} \
	${SRC_COM_F} ${SRC_DEC_F} ${BASE_F}/decode/decode.c \
	-o decode_f

encode_f:
	${CC} ${CFLAGS} ${INCLUDES_F} \
	${SRC_COM_F} ${SRC_ENC_F} ${BASE_F}/encode/encode.c \
	-o encode_f

libg7221_decode_i.dll:
	${CROSS_CC} -shared -DG7221_EXPORT ${CFLAGS} ${INCLUDES_I} \
	${SRC_COM_I} ${SRC_DEC_I} g7221i.c \
	-Xlinker --output-def -Xlinker libg7221_decode_i.def \
	-o libg7221_decode_i.dll
	${CROSS_STRIP} libg7221_decode_i.dll

libg7221_decode.dll:
	${CROSS_CC} -shared -DG7221_EXPORT ${CFLAGS} ${INCLUDES_F} \
	${SRC_COM_F} ${SRC_DEC_F} g7221f.c  \
	-Xlinker --output-def -Xlinker libg7221_decode.def \
	-o libg7221_decode.dll
	${CROSS_STRIP} libg7221_decode.dll


clean:
	rm -f decode_i encode_i decode_f encode_f libg7221_decode_i.dll libg7221_decode.dll
