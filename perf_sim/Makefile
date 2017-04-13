#
# Makefile
# Building the scalar MIPS CPU simulator.
# MIPT-MIPS Assignment 4.
# Ladin Oleg.
#

# Specifying relative path to the trunk
TRUNK= ..

# Including options and compiler flags
include $(TRUNK)/options.mk

# Clang
CXXFLAGS += -Wno-unused-private-field

#
# Enter for build "perf_sim" program
#
OBJS= elf_parser.o func_memory.o func_instr.o perf_sim.o func_sim.o config.o main.o bpu.o cache_tag_array.o
DEPS= $(OBJS:.o=.d)

mipt-mips: $(OBJS)
	@# don't forget to link ELF library using "-l elf"
	@$(CXX) $(LDFLAGS) -o $@ $^ -l elf -l boost_program_options
	@echo "---------------------------------"
	@echo "$@ is built SUCCESSFULLY"

-include $(DEPS)

config.o: config.cpp
	@echo "[$(CXX)] $@"
	@$(CXX) $(CXXFLAGS) -c $< $(INCL) $(BOOST_INCL)
	@$(CXX) $(CXXFLAGS) -MM -o $(patsubst %.o, %.d, $@) $< $(INCL) $(BOOST_INCL)

%.o: %.cpp
	@echo "[$(CXX)] $@"
	@$(CXX) $(CXXFLAGS) -c $< $(INCL)
	@$(CXX) $(CXXFLAGS) -MM -o $(patsubst %.o, %.d, $@) $< $(INCL)



OBJS= portstest.o
DEPS= $(OBJS:.o=.d)

portstest: $(OBJS)
	$(CXX) -o $@ $^
	@echo "---------------------------------"
	@echo "$@ is built SUCCESSFULLY"
#
# Enter to remove all created files
#
clean:
	@-rm -f *.o *.d
	@-rm -f mipt-mips
