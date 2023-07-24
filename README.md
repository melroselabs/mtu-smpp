# mtu-smpp
Simple SMPP server wrapper around Dialogic MTU SS7 application

## 

Copy mtu_main.c file to mtu_main_mod.c and update main() declaration in new file to:

    int main_inner(int argc, char** argv_in)

Copy mtu.mak file to mtu.mak and update OBJS line in new file to:

    OBJS = mtu.o mtu_fmt.o mtu_main_mod.o mtu_smpp_main.o

Add the following to makeall.sh

    make -f mtu_smpp.mak $make_opts

Run makeall.sh to build
