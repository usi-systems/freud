all:
	$(MAKE) -C elf
	$(MAKE) -C dwarf
	$(MAKE) freud-dwarf

install:
	$(MAKE) -C elf install
	$(MAKE) -C dwarf install

clean:
	$(MAKE) -C elf clean
	$(MAKE) -C dwarf clean
	rm *.o
	rm freud-dwarf 
check:
	cd test && ./test.sh

# Find libs
export PKG_CONFIG_PATH=elf:dwarf
#CPPFLAGS+=$$(pkg-config --cflags libelf++ libdwarf++)
CPPFLAGS+=-g -Ielf -Idwarf -std=c++11

# Statically link against our libs to keep the example binaries simple
# and dependencies correct.
LIBS=dwarf/libdwarf++.a elf/libelf++.a

# Dependencies
CPPFLAGS+=-MD -MP -MF .$@.d
-include .*.d

freud-dwarf: create-instrumentation.o dwarf-explorer.o code-gen.o table-generator.o utils.o instr-expr-context.o $(LIBS)
	$(LINK.cc) $^ $(LOADLIBES) $(LDLIBS) -o $@
