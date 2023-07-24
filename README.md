# mtu-smpp
Simple SMPP server wrapper around Dialogic MTU SS7 application.

Perform the following MAP operations using SMPP:

+ Forward short message

## Install Dialogic

See https://developers.melroselabs.com/docs/sccp-hub-demo-mtu-mtr

## Build updated MTU

Copy mtu_main.c file to mtu_main_mod.c and update main() declaration in new file to:

    int main_inner(int argc, char** argv_in)

Copy mtu.mak file to mtu.mak and update OBJS line in new file to:

    OBJS = mtu.o mtu_fmt.o mtu_main_mod.o mtu_smpp_main.o

Add the following to makeall.sh

    make -f mtu_smpp.mak $make_opts

Run makeall.sh to build

## Run

    ./mtu_smpp

### Forward short message

*Forward short message* requires the MSC and IMSI of the destination mobile and follows a *send routing for SM* operation.

Connect using an SMPP client such as https://melroselabs.com/smppclient.  Host is the IP address of the host where mtu_smpp is running.  Any system ID and password can be used with the example code.

Source address field is the originating address for the MAP request (e.g. 12080011047228190600).  This is the gateway MSC (e.g. SMSC address).  See MTU -g parameter.

Destination address field is the destination address for the MAP request (e.g. 13010008001204448729600010).  This is the MSC serving the destination mobile number.  See MTU -a parameter.

Short message field is the short message text to be sent to the destination mobile.  See MTU -s parameter.

The IMSI is currently hard-coded to 987654321.  See MTU -i parameter.
