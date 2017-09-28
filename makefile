ifeq ($(OS),Windows_NT)
  RM := rm
  EXE := .exe
else
  EXE :=
endif

BIN=pkg2zip${EXE}
BIN2=renamePkg${EXE}
SRC=${wildcard pkg2zip*.c} puff.c
SRC2=pkg2zip_sys.c pkg2zip_utils.c renamePkg.c
OBJ=${SRC:.c=.o}
OBJ2=${SRC2:.c=.o}
DEP=${SRC:.c=.d}
DEP2=${SRC2:.c=.d}

CFLAGS=-pipe -fvisibility=hidden -Wall -Wextra -DNDEBUG -O2
LDFLAGS=-s

.PHONY: all clean

all: ${BIN} ${BIN2}

clean:
	@${RM} ${BIN} ${OBJ} ${DEP} ${BIN2} ${OBJ2} ${DEP2}

${BIN}: ${OBJ}
	@echo [L] $@
	@${CC} ${LDFLAGS} -o $@ $^
	
${BIN2}: ${OBJ2}
	@echo [L] $@
	@${CC} ${LDFLAGS} -o $@ $^

%_x86.o: %_x86.c
	@echo [C] $<
	@${CC} ${CFLAGS} -maes -mssse3 -MMD -c -o $@ $<

%.o: %.c
	@echo [C] $<
	@${CC} ${CFLAGS} -MMD -c -o $@ $<

-include ${DEP} ${DEP2}
