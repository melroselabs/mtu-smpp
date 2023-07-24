//
//  mtu_smpp_main.cpp
//  mlSMPPMTU
//
//  Created by Mark Hay on 23/07/2023.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <time.h>
#include <sys/time.h>

#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <arpa/inet.h>

bool endserver = false;
uint64_t gMessageID = 1;

extern "C" int main_inner(int argc, char** argv);

void perror(char*str) { fprintf(stderr,"%s",str); }

int dolisten( int portno_in )
{
    struct sockaddr_in serv_addr;
    
    int listensockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listensockfd < 0) { perror("ERROR opening socket"); return -1; }
    
    int rc, on = 1;
    
    rc = setsockopt(listensockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
    if (rc < 0) { perror("setsockopt() failed"); close(listensockfd); exit(-1); }

    // Set socket to be nonblocking.
    rc = ioctl(listensockfd, FIONBIO, (char *)&on);
    if (rc < 0) { perror("ioctl() failed"); close(listensockfd); exit(-1); }
    
    bzero((char *) &serv_addr, sizeof(serv_addr));
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno_in);
    
    if (::bind(listensockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) { perror("ERROR on binding"); return -1; }
    
    int r = listen(listensockfd,64);
    printf("r=%d listensockfd=%d\n",r,listensockfd);
    
    return listensockfd;
}

typedef struct stSMPPState {
    bool empty, bound;
    void reset(void) { empty = true; bound = false; }
} SMPPState;

SMPPState smppStates[64000];

void setUint32(uint8_t* buf,uint32_t v) {
    buf[3] = v & 0xff;
    buf[2] = (v>>8) & 0xff;
    buf[1] = (v>>16) & 0xff;
    buf[0] = (v>>24) & 0xff;
}

void sendSMPP(int s,int cmd,int status,int seqno,uint8_t* bufext=NULL,int extlen=0) {
    uint8_t buf[16+extlen];
    setUint32(buf,16+extlen);
    setUint32(buf+4,cmd);
    setUint32(buf+8,status);
    setUint32(buf+12,seqno);
    if (bufext!=NULL) memcpy(buf+16,bufext,extlen);
    send(s,buf,16+extlen,0);
}

uint32_t getUint32(uint8_t* buf) {
    return (((((buf[0]<<8)+buf[1])<<8)+buf[2])<<8)+buf[3];
}

typedef enum { COctetString, Integer, OctetString, NoMoreParams } SMPPTypes;
typedef struct {
    SMPPTypes type;
    char v_coctetstring[256];
    int v_int;
    uint8_t v_octetstring[256];
} SMPPTypeVals;

int parseSMPPPDU( uint8_t* buf, int len, SMPPTypes types[], SMPPTypeVals typevals[] ) {
    int idx = 0;
    int param = 0;
    while (idx<len) {
        
        if ( types[param] == NoMoreParams ) break;
        
        typevals[param].type = types[param];
        
        switch (types[param]) {
            
            case COctetString: {
                typevals[param].v_coctetstring[0] = '\0';
                strcpy(typevals[param].v_coctetstring,(char*)(buf+idx));
                idx += 1 + strlen(typevals[param].v_coctetstring);
                break;
            }
            case Integer: {
                typevals[param].v_int = buf[idx];
                idx++;
                break;
            }
            case OctetString: {
                typevals[param].v_octetstring[0] = '\0';
                strcpy((char*)typevals[param].v_octetstring,(char*)(buf+idx));
                idx += 1 + strlen((char*)typevals[param].v_octetstring);
                break;
            }
        }
        
        param++;
        
    }
    
    /*for(int i=0;i<param;i++) {
        printf("%2d: type(%d) co(%s) i(%d) o(%s)\n",
               i,
               typevals[i].type,
               typevals[i].v_coctetstring,
               typevals[i].v_int,
               typevals[i].v_octetstring);
    }*/
    
    return param;
}

bool processSMPP(int s) {
    
    uint8_t buf[1024];
    long l = recv(s,buf,(int)sizeof(buf),0);
    if (l<16) return true;
    
    if (smppStates[s].empty) { smppStates[s].reset(); smppStates[s].empty = false; }
    SMPPState& state = smppStates[s];
    
    uint32_t len = getUint32(buf);
    uint32_t cmd = getUint32(buf+4);
    uint32_t status = getUint32(buf+8);
    uint32_t seqno = getUint32(buf+12);
    
    printf("SMPP [0x%x 0x%x 0x%x 0x%x] (%d)\n",len,cmd,status,seqno,s);
    
    if ((cmd==0x00000001)||(cmd==0x00000002)||(cmd==0x00000009/*bind_transceiver*/)) {
        
        if (state.bound) {
            sendSMPP(s,cmd+0x80000000,0x00000005/*ESME_RALYBND*/,seqno);
            return false;
        }
        state.bound = true;
        char systemid[] = "mtu_smpp";
        sendSMPP(s,cmd+0x80000000,0x00000000/*ESME_ROK*/,seqno,(uint8_t*)systemid,(int)(strlen(systemid)+1));
    }
    
    if (cmd==0x00000004/*submit_sm*/) {
        
        /*
        ./mtu -d0 -m0x2d -u0x15 -p1 -g12080011047228190600 -a13010008001204448729600010 -i987654321 -s"@@@@@"
         */
        
        SMPPTypes submitPDU[] = {COctetString,
            Integer,Integer,COctetString,
            Integer,Integer,COctetString,
            Integer,
            Integer,
            Integer,
            COctetString,
            COctetString,
            Integer,
            Integer,
            Integer,
            Integer,
            Integer,
            OctetString,
            NoMoreParams};
        SMPPTypeVals submitParams[sizeof(submitPDU)];
        int countParams = parseSMPPPDU(buf+16, (int) l-16,  submitPDU, submitParams );
        
        //
        
        for(int i=0;i<countParams;i++) {
            printf("%2d: type(%d) co(%s) i(%d) o(%s)\n",
                   i,
                   submitParams[i].type,
                   submitParams[i].v_coctetstring,
                   submitParams[i].v_int,
                   submitParams[i].v_octetstring);
        }
        
        //
        
        // call "int main_inner(argc, argv)" in mtu_main_mod.c
        
        char* n_argv[9];
        n_argv[0] = "";
        n_argv[1] = "-d0";
        n_argv[2] = "-m0x2d";
        n_argv[3] = "-u0x15";
        n_argv[4] = "-p1";
        n_argv[5] = "-g12080011047228190600";
        n_argv[6] = "-a13010008001204448729600010";
        n_argv[7] = "-i987654321";
        n_argv[8] = "-s\"Hello world\"";
        
        char n_argv_5[64]{0};
        snprintf(n_argv_5,64,"-g%s",submitParams[3].v_coctetstring);
        n_argv[5] = n_argv_5;

        char n_argv_6[64]{0};
        snprintf(n_argv_6,64,"-a%s",submitParams[6].v_coctetstring);
        n_argv[6] = n_argv_6;

        char n_argv_8[256]{0};
        snprintf(n_argv_8,64,"-s\"%s\"",submitParams[17].v_octetstring);
        n_argv[8] = n_argv_8;
        
        main_inner(9,n_argv);
        
        //
        
        char messageid[32];
        snprintf(messageid,32,"%08llx",gMessageID++);
        sendSMPP(s,cmd+0x80000000,0x00000000/*ESME_ROK*/,seqno,(uint8_t*)messageid,(int)(strlen(messageid)+1));
        
    }
    
    return false;
}

int main(int argc, const char * argv[]) {
    
    int listensockfdSMPP = dolisten(2775);
    if (listensockfdSMPP == -1) { perror("Failed to listen on SMPP port"); return -1; }
    
    int rc=0,i=0,desc_ready=0,new_sd=0,close_conn=0,max_sd=-1;
    struct timeval timeout;
    timeout.tv_sec  = 0;
    timeout.tv_usec = 1000*5; // 5ms

    fd_set master_set, working_set;
    FD_ZERO(&master_set);
    FD_SET(listensockfdSMPP, &master_set);
    if (listensockfdSMPP > max_sd) max_sd = listensockfdSMPP;
    
    do {
        
        memcpy(&working_set, &master_set, sizeof(master_set));
        rc = select(max_sd + 1, &working_set, NULL, NULL, &timeout);
        if (rc < 0) { perror("  select() failed"); if (errno == EINTR) continue; break; }
        
        if ((rc == 0 /* select() timed-out */)||(rc != 0)) {
            
            if (rc==0) continue; /* select() timed-out */
        }
        
        desc_ready = rc;
        for (i=0; i <= max_sd  &&  desc_ready > 0; ++i) {
            
            if (FD_ISSET(i, &working_set))
            {
                desc_ready -= 1;
                
                if (i == listensockfdSMPP)
                {
                    do
                    {
                        struct sockaddr_in client_addr;
                        int clen = sizeof(sockaddr_in);
                        new_sd = accept(listensockfdSMPP, (struct sockaddr *)&client_addr, (socklen_t*) &clen);
                        if (new_sd < 0)
                        {
                            if (errno != EWOULDBLOCK) { perror("  accept() failed (SMPP)"); if (errno != EMFILE) endserver = true; }
                            break;
                        }
                        
                        char* ip = inet_ntoa(client_addr.sin_addr);
                        printf("New connection - %s (%d)\n",ip,new_sd);
                        
                        FD_SET(new_sd, &master_set);
                        if (new_sd > max_sd) max_sd = new_sd;
                        
                    } while (new_sd != -1);
                }
                else {
                    
                    bool closed = processSMPP(i);
                    
                    if (closed) { printf("%ld  Closing connection - %d\n", time(NULL), i); close_conn = true; }
                    else close_conn = false;
                    
                    if (close_conn)
                    {
                        smppStates[i].reset();
                        close(i);
                        FD_CLR(i, &master_set);
                        if (i == max_sd) { while (FD_ISSET(max_sd, &master_set) == false) max_sd -= 1; }
                    }
                }
            }
        }
        
    } while (!endserver);
    
    close(listensockfdSMPP);
    
    return 0;
}
