GDB=../../processors/ARM/gdb-8.3.1
PREREQUISITES:= ../gdbarm32/bfd/libbfd.a \
				../gdbarm32/libiberty/libiberty.a \
				../gdbarm32/opcodes/libopcodes.a \
				../gdbarm32/sim/arm/libsim.a

INCFIRST:=$(GDB)/include $(GDB)/sim/arm $(GDB)/bfd ../gdbarm32/bfd $(GDB)

# assert uses warning, which is in the VM, so -undefined dynamic_lookup needed
ifeq ($(CONFIGURATION),assert)
    EXTRA_FOR_ASSERT:=-undefined dynamic_lookup
else ifeq ($(CONFIGURATION),debug)
    EXTRA_FOR_ASSERT:=-undefined dynamic_lookup
    BUILD:=builddbg 
else # default CONFIGURATION=product
    EXTRA_FOR_ASSERT:=
endif 
EXTRALIBS:= -L../gdbarm32/bfd -L../gdbarm32/libiberty \
			-L../gdbarm32/opcodes -L../gdbarm32/sim/arm \
			-lbfd -liberty -lopcodes -lopcodes -lsim -lz \
			$(EXTRA_FOR_ASSERT)

include ../common/Makefile.plugin

INCDIRS:= $(INCFIRST) $(MAKERVMDIR) $(CROSSVMDIR) $(MAKERDIR) $(OSXPLGDIR) $(OSXVMDIR) $(CROSSDIR)
INCLUDES:= $(addprefix -I,. $(INCDIRS))
