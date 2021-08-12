#
# On MS Windows using Msys/MinGW gfortran invoke like this:
#
# FC=gfortran make
#
# On macOS using MacPorts gfortran invoke like this:
#
# FC=gfortran make
#
# or if the gfortran compiler is named gfortran-mp-8 or similar
#
# FC=gfortran-mp-8 make
#
# otherwise invoke like this:
#
# make
#

ifeq ($(OS),Windows_NT)
	EXE = .exe
endif

EXES = hashcodes$(EXE) std_call_to_c28$(EXE) nonstd_to_c58$(EXE) \
	free_text_to_f71$(EXE) grid4_to_g15$(EXE) grid6_to_g25$(EXE) \
	gen_crc14$(EXE)

%.o:	%.f90
	$(FC) -c $(FFLAGS) -o $@ $<

all:	$(EXES)

hashcodes$(EXE): hashcodes.o
	${FC} -o $@ $^

std_call_to_c28$(EXE): std_call_to_c28.o
	${FC} -o $@ $^

nonstd_to_c58$(EXE): nonstd_to_c58.o
	${FC} -o $@ $^

free_text_to_f71$(EXE): free_text_to_f71.o
	${FC} -o $@ $^

grid4_to_g15$(EXE): grid4_to_g15.o
	${FC} -o $@ $^

grid6_to_g25$(EXE): grid6_to_g25.o
	${FC} -o $@ $^

gen_crc14$(EXE): gen_crc14.o
	${FC} -o $@ $^

clean:
	-rm $(EXES) *.o
