# mtu_smpp.mak

include ../makdefs.mak

TARGET = $(BINPATH)/mtu_smpp

all :   $(TARGET)

clean:
        rm -rf *.o
        rm -rf $(TARGET)

OBJS = mtu.o mtu_fmt.o mtu_main_mod.o mtu_smpp_main.o

$(TARGET): $(OBJS)
        $(LINKER) $(LFLAGS) -o $@ $(OBJS) $(LIBS) $(SYSLIBS)
