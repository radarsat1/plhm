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
#include <getopt.h>

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

#include "plhm.h"

#define SIZE 10000
#define DEVICENAME "/dev/ttyUSB0"

double starttime;
double curtime;
struct timeval temp;

void GetPno();


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
int printswitch = 0;
int whichdata = 0;
int hexfloats=1;
struct timeval prev;

void liblo_error(int num, const char *msg, const char *path);
int start_handler(const char *path, const char *types, lo_arg **argv, int argc,
                  void *data, void *user_data);
int stop_handler(const char *path, const char *types, lo_arg **argv, int argc,
                 void *data, void *user_data);
int status_handler(const char *path, const char *types, lo_arg **argv, int argc,
                   void *data, void *user_data);

int GetBinPno(polhemus_t *pol);

typedef union {
    const int *i;
    const unsigned int *ui;
    const short *s;
    const unsigned short *us;
    const float *f;
    const char *c;
    const unsigned char *uc;
} multiptr;

/* option flags */
static int daemon_flag = 0;
static int hex_flag = 0;
static int euler_flag = 0;
static int position_flag = 1;

const char *device_name = "/dev/ttyUSB0";

int main(int argc, char *argv[])
{
    static struct option long_options[] =
    {
        {"daemon",   no_argument,       &daemon_flag,   1},
        {"device",   required_argument, 0,              'd'},
        {"hex",      no_argument,       &hex_flag,      1},
        {"euler",    no_argument,       &euler_flag,    1},
        {"position", no_argument,       &position_flag, 1},
        {"output",   optional_argument, 0,              1},
        {"oscurl",   required_argument, 0,              'u'},
        {"help",     no_argument,       0,              0},
        {0, 0, 0, 0}
    };

    while (1)
    {
        int option_index = 0;
        int c = getopt_long(argc, argv, "Dd:Hepou:h",
                            long_options, &option_index);
        if (c==-1)
            break;

        switch (c)
        {
        case 0:
            if (long_options[option_index].flag != 0)
                break;
            break;

        case 'D':
            daemon_flag = 1;
            break;

        case 'H':
            hex_flag = 1;
            break;

        case 'p':
            position_flag = 1;
            break;

        case 'e':
            euler_flag = 1;
            break;

        case 'd':
            // serial device name
            device_name = optarg;
            break;

        case 'u':
            // handle OSC url (liblo)
            break;

        case 'o':
            // output file name, if specified
            // otherwise, stdout
            printswitch = 1;
            break;

        case '?':
            break;

        default:
        case 'h':
            printf("Usage: %s [options]\n"
"  where options are:\n"
"  -D --daemon           wait indefinitely for device\n"
"  -d --device=<device>  specify the serial device to use\n"
"  -p --position         request position data\n"
"  -e --euler            request euler angle data\n"
"  -o --output=[path]    write data to stdout, or to a file\n"
"                        if path is specified\n"
"  -H --hex              write hex values as hexidecimal\n"
"  -u --sendurl=<url>    provide a URL for OSC destination\n"
"                        this URL must be liblo-compatible,\n"
"                        e.g., osc.udp://localhost:9000\n"
"                        this option is required to enable\n"
"                        the Open Sound Control interface\n"
"  -h --help             show this help\n"
                   , argv[0]);
            exit(c!='h');
            break;
        }
    }

    whichdata = euler_flag;
    hexfloats = hex_flag;

    port = 0;
    host[0] = 0;

    char choice[10];
    char buf[1000];
    char trakType[30];
    int br;
    int slp = 0;

    polhemus_t pol;
    memset((void*)&pol, 0, sizeof(polhemus_t));

    // setup OSC server
    lo_server_thread st = lo_server_thread_new("5000", liblo_error);
    lo_server_thread_add_method(st, "/liberty/start", "si", start_handler, NULL);
    lo_server_thread_add_method(st, "/liberty/start", "i", start_handler, NULL);
    lo_server_thread_add_method(st, "/liberty/stop", "", stop_handler, NULL);
    lo_server_thread_add_method(st, "/liberty/status", "si", status_handler, NULL);
    lo_server_thread_add_method(st, "/liberty/status", "i", status_handler, NULL);
    lo_server_thread_start(st);

    started = 1;
    port = 9999;
    strcpy(host, "localhost");

    while (1) {
        sleep(slp);
        slp = 1;
        
        // Loop until device is available.
        if (plhm_find_device(device_name)) {
            if (daemon_flag)
                continue;
            else {
                printf("Could not find device at %s\n", device_name);
                break;
            }
        }

        // Don't open device if nobody is listening
        if ((!started || port==0) && daemon_flag)
            continue;

        if (plhm_open_device(&pol, device_name))
        {
            if (daemon_flag)
                continue;
            else {
                printf("Could not open device %s\n", device_name);
                break;
            }
        }

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

        // stop any incoming continuous data just in case
        // ignore the response
        if (plhm_data_request(&pol))
            break;

        plhm_read_until_timeout(&pol, 500);

        if (plhm_text_mode(&pol))
            break;

        // determine tracker type        
        if (plhm_get_version(&pol))
            break;

        // check for initialization errors
        if (plhm_read_bits(&pol))
            break;

        // check what stations are available
        if (plhm_get_stations(&pol))
            break;

        if (plhm_set_hemisphere(&pol))
            break;

        if (plhm_set_units(&pol, POLHEMUS_UNITS_METRIC))
            break;

        if (plhm_set_rate(&pol, POLHEMUS_RATE_240))
            break;

        if (whichdata) {
            if (plhm_set_data_fields(&pol,
                                    POLHEMUS_DATA_POSITION
                                    | POLHEMUS_DATA_EULER
                                    | POLHEMUS_DATA_TIMESTAMP))
                break;
        } else {
            if (plhm_set_data_fields(&pol,
                                    POLHEMUS_DATA_POSITION
                                    | POLHEMUS_DATA_TIMESTAMP))
                break;
        }

        gettimeofday(&temp, NULL);
        starttime = (temp.tv_sec * 1000.0) + (temp.tv_usec / 1000.0);

        if (plhm_binary_mode(&pol))
            break;

        if (plhm_data_request_continuous(&pol))
            break;

        /* loop getting data until stop is requested or error occurs */
        while (started && !GetBinPno(&pol)) {}

        // stop any incoming continuous data
        if (plhm_data_request(&pol))
            break;

        plhm_read_until_timeout(&pol, 500);
        plhm_read_until_timeout(&pol, 500);
        plhm_read_until_timeout(&pol, 500);

        if (plhm_text_mode(&pol))
            break;

        plhm_close_device(&pol);
        close(sockfd);
    }

    plhm_close_device(&pol);
    close(sockfd);

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
    char buf[2000];		// may need to be larger if getting a lot of data
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

int GetBinPno(polhemus_t *pol)
{
    const char *buf;
    char hex[4000];
    char tmp[10];
    int i = 0;
    int br, count, start;
    struct timeval now, diff;

    static int c=0;
    if (c++ > 30) {
        gettimeofday(&now, NULL);
        timeval_subtract(&diff, &now, &prev);
        prev.tv_sec = now.tv_sec;
        prev.tv_usec = now.tv_usec;

        fprintf(stderr, "Update frequency: %0.2f Hz           \r",
                30.0 / (diff.tv_usec/1000000.0 + diff.tv_sec));
        c=0;
    }

/*     plhm_data_request(pol); */
    
    //system("clear");
    //printf("Time: %ld \n", curtime);

    const float *pData;
    polhemus_record_t rec;
    multiptr p;
    int s;

    for (s = 0; s < pol->stations; s++)
    {
        if (plhm_read_data_record(pol, &rec))
            return 1;

        curtime = ((rec.readtime.tv_sec * 1000.0)
                   + (rec.readtime.tv_usec / 1000.0));

        printf("%d", rec.station);

        if (rec.fields & POLHEMUS_DATA_POSITION)
        {
            if (hexfloats)
                for (i=0; i<3; i++) {
                    p.f = &rec.position[i];
                    printf(", 0x%02x%02x%02x%02x",
                           p.uc[0] & 0xFF,
                           p.uc[1] & 0xFF,
                           p.uc[2] & 0xFF,
                           p.uc[3] & 0xFF);
                }
            else
                printf(", %.4f, %.4f, %.4f",
                       rec.position[0],
                       rec.position[1],
                       rec.position[2]);
        }

        if (rec.fields & POLHEMUS_DATA_EULER)
        {
            if (hexfloats)
                for (i=0; i<3; i++) {
                    p.f = &rec.euler[i];
                    printf(", 0x%02x%02x%02x%02x",
                           p.uc[0] & 0xFF,
                           p.uc[1] & 0xFF,
                           p.uc[2] & 0xFF,
                           p.uc[3] & 0xFF);
                }
            else
                printf(", %.4f, %.4f, %.4f",
                       rec.euler[0],
                       rec.euler[1],
                       rec.euler[2]);
        }

        if (rec.fields & POLHEMUS_DATA_TIMESTAMP)
            printf(", %u", rec.timestamp);

        printf(", %f\n", curtime);

        if (printswitch)
            continue;

        int station = rec.station;
        pData = &rec.position[0];

        //x message
        //int addrLength = OSC_effectiveStringLength("/liberty/marker/%d/x");           
        char addr[30];
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
        
	if(whichdata){
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
        }
        
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
