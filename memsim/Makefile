RM        := /bin/rm -rf
CC        := g++
CURRENT_DIR := $(shell pwd)
CFLAGS    := -std=c++11 -O3 -lz -lm  -W -Wall -Wno-deprecated -Wno-unknown-warning-option -Wno-self-assign -Wno-unused-parameter -Wno-unused-but-set-variable -DCURRENT_DIR=\"$(CURRENT_DIR)\"
DFLAGS    :=  -fsanitize=address
PFLAGS    := -pg

SIM_DRAMSIM3 := ./sim_dramsim3
DRAMSIM3_DIR := $(shell pwd)/../DRAMsim3
DRAMSIM3_FLAGS    := -DDRAMSIM3 -I$(DRAMSIM3_DIR)/src -L$(DRAMSIM3_DIR) -Wl,-rpath,$(DRAMSIM3_DIR) -ldramsim3 $(OPTION)


all:  
	${CC} ${CFLAGS} ${DRAMSIM3_FLAGS} memsys_dramsim3.c mcore.c os.c  mcache.c sim.c  -o ${SIM_DRAMSIM3} -lz -ldramsim3

clean: 
	$(RM) ${SIM_DRAMSIM3} *.o
