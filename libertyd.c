/************************************************************************

		POLHEMUS PROPRIETARY

		Polhemus Inc.
		P.O. Box 560
		Colchester, Vermont 05446
		(802) 655-3159



        		
	    Copyright © 2004 by Polhemus
		All Rights Reserved.


*************************************************************************/

// LinuxTerm.c          Sample code to illustrate how to interface to a Liberty/Patriot over Linux via USB


#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <termios.h>
#include <stdlib.h>

#include "OSC-client.h"

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <lo/lo.h>


#define SIZE 10000
#define DEVICENAME "/dev/ttyUSB0"

long int starttime;
long int curtime;
timeval temp;

void GetPno();
int GetBinPno();
int GetVerInfo(char *);


int rdPort, wrPort;

float pos[2] = { 0, 0 };
float dist = 0.0;

const char *version = "1.1.0";
const char *hdr =
    "\nLinuxTerm Application version %s\nCopyright © 2004 by Polhemus\nAll Rights Reserved.\n\n";

OSCbuf myBuf;
OSCbuf *b = &myBuf;
char bytes[SIZE];

int sockfd;
struct sockaddr_in their_addr;	// connector's address information
struct hostent *he;
int numbytes;
int port=0;
char host[256];
int started = 0;
int device_found = 0;
int device_open = 0;
int data_good = 0;

int numChannels;
struct timeval prev;

void liblo_error(int num, const char *msg, const char *path);
int start_handler(const char *path, const char *types, lo_arg **argv, int argc,
                  void *data, void *user_data);
int stop_handler(const char *path, const char *types, lo_arg **argv, int argc,
                 void *data, void *user_data);
int status_handler(const char *path, const char *types, lo_arg **argv, int argc,
                   void *data, void *user_data);

int main(int argc, char *argv[])
{
    if (argc != 2 || atoi(argv[1]) <= 0) {
        printf("Usage: %s <number of channels>\n", argv[0]);
        exit(1);
    }

    port = 0;
    host[0] = 0;
    numChannels = atoi(argv[1]);

    char choice[10];
    char buf[1000];
    char trakType[30];
    int br;
    struct termios initialAtt, newAtt;
    int slp = 0;

    // setup OSC server
    lo_server_thread st = lo_server_thread_new("5000", liblo_error);
    lo_server_thread_add_method(st, "/liberty/start", "si", start_handler, NULL);
    lo_server_thread_add_method(st, "/liberty/start", "i", start_handler, NULL);
    lo_server_thread_add_method(st, "/liberty/stop", "", stop_handler, NULL);
    lo_server_thread_add_method(st, "/liberty/status", "si", status_handler, NULL);
    lo_server_thread_add_method(st, "/liberty/status", "i", status_handler, NULL);
    lo_server_thread_start(st);

    while (1) {
        sleep(slp);
        slp = 1;
        
        // Loop until device is available.
        struct stat st;
        if (stat(DEVICENAME, &st) == -1) {
            device_found = 0;
            continue;
        }
        device_found = 1;

        // Don't open device if nobody is listening
        if (!started || port==0)
            continue;

        // Open serial device for reading and writing
        rdPort = open(DEVICENAME, O_RDONLY | O_NDELAY);
        if (rdPort == -1) {
            printf("Could not open device %s for reading.\n", DEVICENAME);
            device_open = 0;
            exit(1);
        }

        wrPort = open(DEVICENAME, O_WRONLY);
        if (wrPort == -1) {
            printf("Could not open device %s for writing.\n", DEVICENAME);
            close(rdPort);
            device_open = 0;
            exit(1);
        }

        device_open = 1;

        // init sock & OSC stuff
        {
            OSC_initBuffer(b, SIZE, bytes);

            if ((he = gethostbyname(host)) == NULL) {	// get the host info
                perror("gethostbyname");
                exit(1);
            }

            if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
                perror("socket");
                exit(1);
            }
            
            their_addr.sin_family = AF_INET;	// host byte order
            their_addr.sin_port = htons(port);	// short, network byte order
            their_addr.sin_addr = *((struct in_addr *) he->h_addr);
            memset(&(their_addr.sin_zero), '\0', 8);	// zero the rest of the struct
        }

        // set up terminal for raw data
        tcgetattr(rdPort, &initialAtt);	// save this to restore later
        newAtt = initialAtt;
        cfmakeraw(&newAtt);
        if (tcsetattr(rdPort, TCSANOW, &newAtt)) {
            printf("Error setting terminal attributes\n");
            close(rdPort);
            close(wrPort);
            exit(2);
        }

        // determine tracker type        
        write(wrPort, "\r", 1);	// this read/write is used to clear out the buffers
        read(rdPort, (void *) "buf", 100);	// just throw this data away
        
        GetVerInfo(buf);
        if (strstr(buf, "Patriot"))
            strcpy(trakType, "Patriot");
        else if (strstr(buf, "Liberty"))
            strcpy(trakType, "Liberty");
        else
            strcpy(trakType, "Unknown tracker");
        
        memset(buf, 0, 100);
        
        write(wrPort, "H*,0,0,1\r", strlen("H*,0,0,1\r"));
        
        write(wrPort, "U1\r", strlen("U1\r"));
        write(wrPort, "R3\r", strlen("R3\r"));  // set the update rate to 240 Hz (R4), 120 Hz (R3)

        // position, euler angles, timestamp
        write(wrPort, "O*,2,4\r", strlen("O*,2,4\r"));  // set the update rate to 240 Hz (R4), 120 Hz (R3)
        
        gettimeofday(&temp, NULL);
        starttime = (temp.tv_sec * 1000.0) + (temp.tv_usec / 1000.0);
        
        write(wrPort, "F1\r", 3);	// put tracker into binary mode
        while (started && !GetBinPno()) {}
        write(wrPort, "F0\r", 3);	// return tracker to ASCII

        tcsetattr(rdPort, TCSANOW, &initialAtt);	// restore the original attributes
        close(rdPort);
        close(wrPort);
        
        close(sockfd);
    }

    return 0;
}



/* Subtract the `struct timeval' values X and Y,
   storing the result in RESULT.
   Return 1 if the difference is negative, otherwise 0.  */

int timeval_subtract (struct timeval *result,
                      const struct timeval *x,
                      const struct timeval *y)
{
    /* Perform the carry for the later subtraction by updating y. */
    if (x->tv_usec < y->tv_usec) {
        int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
//        y->tv_usec -= 1000000 * nsec;
//        y->tv_sec += nsec;
    }
    if (x->tv_usec - y->tv_usec > 1000000) {
        int nsec = (x->tv_usec - y->tv_usec) / 1000000;
//        y->tv_usec += 1000000 * nsec;
//        y->tv_sec -= nsec;
    }

    /* Compute the time remaining to wait.
       tv_usec is certainly positive. */
    result->tv_sec = x->tv_sec - y->tv_sec;
    result->tv_usec = x->tv_usec - y->tv_usec;

    /* Return 1 if result is negative. */
    return x->tv_sec < y->tv_sec;
}


void GetPno()
{
    int br, count;
    char buf[2000];		// may need to be larger if getting alot of data
    int start = 0;

    memset(buf, 0, 2000);

    count = 0;
    write(wrPort, "P", 1);	// request data

    // keep reading till the well has been dry for at least 5 attempts
    count = 0;
    do {			
        br = read(rdPort, buf + start, 2000 - start);
        if (br > 0)
            start += br;
        usleep(1000);
    } while ((br > 0) || (count++ < 5));
    
    buf[start] = '\0';		//terminate
    printf("\n%d bytes Read\n", start);

    printf(buf);
}

int GetBinPno()
{
    char buf[2000];
    char hex[4000];
    char tmp[10];
    int i = 0;
    int br, count, start;
    struct timeval now, diff;

    gettimeofday(&now, NULL);
    timeval_subtract(&diff, &now, &prev);
    prev.tv_sec = now.tv_sec;
    prev.tv_usec = now.tv_usec;
    printf("Update frequency: %0.2f Hz           \r", 1.0 / (diff.tv_usec/1000000.0 + diff.tv_sec));
    
    memset(buf, 0, 2000);
    
    write(wrPort, "P", 1);	// request data
    
    // keep reading till the well has been dry for at least 5 attempts
    count = start = 0;
    usleep(100);
    do {
        br = read(rdPort, buf + start, 2000 - start);
        if (br > 0)
            start += br;
        usleep(100);
    } while (((br > 0) || (count++ < 5))
             && (start < (numChannels*(6*4+8))));

    // (numChannels*(6*4+8))
    // -> 6 floats (position, orientation) * 4 bytes/float + 8 bytes for header

    br = start;
        
    
    // check for proper format LY for Liberty
    if (strncmp(buf, "LY", 2) && strncmp(buf, "PA", 2)) {
        // PA for Patriot
        data_good = 0;
        printf("Corrupted data received\n");
        printf("%s\n", buf);
        return 1;
    }
    data_good = 1;
    
    gettimeofday(&temp, NULL);
    curtime = ((temp.tv_sec * 1000) + (temp.tv_usec / 1000)) - starttime;
    
    //system("clear");
    //printf("Time: %ld \n", curtime);


    for (int s = 0; s < numChannels; s++) {
        float *pData = (float *) (buf + (8 + (6*4)) * (s));	// header is first 8 bytes

        int station = buf[((8 + 6*4) * (s)) + 2];
        int size = (int)*(unsigned short*)(buf+(8+6*4)*s+6);
        
        // this line can be used to capture raw text file of marker
        // information that can be easily imported into matlab
        // to use: 1) uncomment 2) on command line, pipe output to text file
        /*
          printf("%d, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %ld\n", station,
          pData[0], pData[1], pData[2], pData[3], pData[4], pData[5],
          curtime);
        */
        
        //x message
        //int addrLength = OSC_effectiveStringLength("/liberty/marker/%d/x");           
        char *addr = new char[30];
        sprintf(addr, "/liberty/marker/%d/x", station);
        
        if (OSC_writeAddressAndTypes(b, addr, ",f")) {
            printf("** ERROR 1: %s\n", OSC_errorMessage);
        }
        
        
        if (OSC_writeFloatArg(b, pData[0])) {
            printf("** ERROR 2: %s\n", OSC_errorMessage);
        }

        //printf("Sending %d bytes\n", OSC_packetSize(b));
        if ((numbytes = sendto(sockfd, b->buffer, OSC_packetSize(b), 0,
                               (struct sockaddr *) &their_addr,
                               sizeof(struct sockaddr))) == -1) {
            perror("sendto");
            exit(1);
        }
        OSC_resetBuffer(b);
        
        //y message
        sprintf(addr, "/liberty/marker/%d/y", station);
        
        if (OSC_writeAddressAndTypes(b, addr, ",f")) {
            printf("** ERROR 1: %s\n", OSC_errorMessage);
        }
        
        
        if (OSC_writeFloatArg(b, pData[1])) {
            printf("** ERROR 2: %s\n", OSC_errorMessage);
        }

        //printf("Sending %d bytes\n", OSC_packetSize(b));
        if ((numbytes = sendto(sockfd, b->buffer, OSC_packetSize(b), 0,
                               (struct sockaddr *) &their_addr,
                               sizeof(struct sockaddr))) == -1) {
            perror("sendto");
            exit(1);
        }
        OSC_resetBuffer(b);
        
        //z message
        sprintf(addr, "/liberty/marker/%d/z", station);
        
        if (OSC_writeAddressAndTypes(b, addr, ",f")) {
            printf("** ERROR 1: %s\n", OSC_errorMessage);
        }
        
        
        if (OSC_writeFloatArg(b, pData[2])) {
            printf("** ERROR 2: %s\n", OSC_errorMessage);
        }
        //printf("Sending %d bytes\n", OSC_packetSize(b));
        if ((numbytes = sendto(sockfd, b->buffer, OSC_packetSize(b), 0,
                               (struct sockaddr *) &their_addr,
                               sizeof(struct sockaddr))) == -1) {
            perror("sendto");
            exit(1);
        }
        OSC_resetBuffer(b);
        
        
        //Azimuth message
        sprintf(addr, "/liberty/marker/%d/azimuth", station);
        
        if (OSC_writeAddressAndTypes(b, addr, ",f")) {
            printf("** ERROR 1: %s\n", OSC_errorMessage);
        }
        
        
        if (OSC_writeFloatArg(b, pData[3])) {
            printf("** ERROR 2: %s\n", OSC_errorMessage);
        }
        //printf("Sending %d bytes\n", OSC_packetSize(b));
        if ((numbytes = sendto(sockfd, b->buffer, OSC_packetSize(b), 0,
                               (struct sockaddr *) &their_addr,
			       sizeof(struct sockaddr))) == -1) {
            perror("sendto");
            exit(1);
        }
        OSC_resetBuffer(b);
        
        
        //Elevation message
        sprintf(addr, "/liberty/marker/%d/elevation", station);
        
        if (OSC_writeAddressAndTypes(b, addr, ",f")) {
            printf("** ERROR 1: %s\n", OSC_errorMessage);
        }
        

        if (OSC_writeFloatArg(b, pData[4])) {
            printf("** ERROR 2: %s\n", OSC_errorMessage);
        }
        //printf("Sending %d bytes\n", OSC_packetSize(b));
        if ((numbytes = sendto(sockfd, b->buffer, OSC_packetSize(b), 0,
                               (struct sockaddr *) &their_addr,
                               sizeof(struct sockaddr))) == -1) {
            perror("sendto");
            exit(1);
        }
        OSC_resetBuffer(b);
        
        
        //Roll message
        sprintf(addr, "/liberty/marker/%d/roll", station);
        
        if (OSC_writeAddressAndTypes(b, addr, ",f")) {
            printf("** ERROR 1: %s\n", OSC_errorMessage);
        }
        
        
        if (OSC_writeFloatArg(b, pData[5])) {
            printf("** ERROR 2: %s\n", OSC_errorMessage);
        }
        //printf("Sending %d bytes\n", OSC_packetSize(b));
        if ((numbytes = sendto(sockfd, b->buffer, OSC_packetSize(b), 0,
                               (struct sockaddr *) &their_addr,
                               sizeof(struct sockaddr))) == -1) {
            perror("sendto");
            exit(1);
        }
        OSC_resetBuffer(b);
        
        
        //Timestamp message
        sprintf(addr, "/liberty/marker/%d/timestamp", station);
        
        if (OSC_writeAddressAndTypes(b, addr, ",f")) {
            printf("** ERROR 1: %s\n", OSC_errorMessage);
        }
        
        
        if (OSC_writeFloatArg(b, curtime)) {
            printf("** ERROR 2: %s\n", OSC_errorMessage);
        }
        //printf("Sending %d bytes\n", OSC_packetSize(b));
        if ((numbytes = sendto(sockfd, b->buffer, OSC_packetSize(b), 0,
                               (struct sockaddr *) &their_addr,
                               sizeof(struct sockaddr))) == -1) {
            perror("sendto");
            exit(1);
        }
        OSC_resetBuffer(b);
    }

    return 0;
}

// return version info in info
// if info is NULL, just write the info to the screen
// return 0 for success, -1 for failure
int GetVerInfo(char *info)
{
    char cmd[10];
    char buf[2000];
    int br;
    int rv = 0;
    memset(buf, 0, 2000);
    sprintf(cmd, "%c\r", 22);
    write(wrPort, cmd, strlen(cmd));
    usleep(100000);		// wait 100 ms
    br = read(rdPort, buf, 2000);
    if (br >= 0) {
        if (info)
            strcpy(info, buf);
        else
            printf(buf);
    }
    else {
        printf("Error obtaining version information\n");
        rv = -1;
    }

    return rv;
}

void liblo_error(int num, const char *msg, const char *path)
{
    printf("liblo server error %d in path %s: %s\n", num, path, msg);
    fflush(stdout);
}

int start_handler(const char *path, const char *types, lo_arg **argv, int argc,
                  void *data, void *user_data)
{
    started = 0;

    const char *hostname;
    if (argc == 1) {
        hostname = lo_address_get_hostname(lo_message_get_source(data));

        // SS: this is obviously not the right way to handle IPV6 hosts, but
        //     for a reason I don't understand simply passing hostname to
        //     gethostbyname() fails with error code Success.  This is a (bad)
        //     work-around for now.
        if (hostname[0]==':' && hostname[1]==':') {
            hostname += 7;
        }
        port = argv[0]->i;
    }
    else {
        hostname = &argv[0]->s;
        port = argv[1]->i;
    }

    strncpy(host, hostname, 256);
    printf("starting... %s:%d\n", host, port);

    started = 1;
    return 0;
}

int stop_handler(const char *path, const char *types, lo_arg **argv, int argc,
                 void *data, void *user_data)
{
    printf("stopping..\n");
    started = 0;
    return 0;
}

void send_status(const char* hostname, int port)
{
    char port_s[30];
    char *status;

    if (started) {
        status = "sending";
        if (!device_found)
            status = "device_not_found";
        else if (!device_open)
            status = "device_found_but_not_open";
        else if (!data_good)
            status = "data_stream_error";
    }
    else {
        status = "waiting";
    }

    sprintf(port_s, "%d", port);
    lo_address t = lo_address_new(hostname, port_s);
    if (t) {
        lo_send(t, "/liberty/status","s", status);
        lo_address_free(t);
    }
}

int status_handler(const char *path, const char *types, lo_arg **argv, int argc,
                   void *data, void *user_data)
{
    int port;
    const char *hostname;
    if (argc == 1) {
        hostname = lo_address_get_hostname(lo_message_get_source(data));
        port = argv[0]->i;
    }
    else {
        hostname = &argv[0]->s;
        port = argv[1]->i;
    }

    send_status(hostname, port);

    return 0;
}
