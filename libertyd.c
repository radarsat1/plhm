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

double starttime;
double curtime;
timeval temp;

void GetPno();
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
int printswitch;
int whichdata;
int hexfloats=1;
struct timeval prev;

void liblo_error(int num, const char *msg, const char *path);
int start_handler(const char *path, const char *types, lo_arg **argv, int argc,
                  void *data, void *user_data);
int stop_handler(const char *path, const char *types, lo_arg **argv, int argc,
                 void *data, void *user_data);
int status_handler(const char *path, const char *types, lo_arg **argv, int argc,
                   void *data, void *user_data);

enum polhemus_tracker_type
{
    POLHEMUS_UNKNOWN,
    POLHEMUS_LIBERTY,
    POLHEMUS_PATRIOT,
};

enum polhemus_unit_type
{
    POLHEMUS_UNITS_METRIC,
};

enum polhemus_rate
{
    POLHEMUS_RATE_120,
    POLHEMUS_RATE_240,
};

enum polhemus_data_fields
{
    POLHEMUS_DATA_POSITION = 1,
    POLHEMUS_DATA_EULER = 2,
    POLHEMUS_DATA_CRLF = 4,
    POLHEMUS_DATA_TIMESTAMP = 8,
    // more..
};

static const int RSPLEN = 1024;
typedef struct _polhemus
{
    // input / output serial ports
    int rd;
    int wr;
    char response[RSPLEN];
    char buffer[RSPLEN];
    int pos;
    int response_length;
    int device_open;
    struct termios initialAtt;
    polhemus_tracker_type tracker_type;
    int fields;
    int binary;
    int stations;
} polhemus_t;

#ifdef DEBUG
static void traceit(const char* str, const char* prefix)
{
    char tmp[1024], *t=tmp;
    const char *s = str;
    while (*s) {
        if (*s == '\r') {
            *t++ = '\\';
            *t++ = 'r';
            s++;
        }
        else if (*s == '\n') {
            *t++ = '\\';
            *t++ = 'n';
            *t++ = '\n';
            *t++ = '>';
            *t++ = ' ';
            s++;
        }
        else if (!isprint(*s)) {
            sprintf(t, "\\x%02x", *s++);
            t += 4;
        }
        else
            *t++ = *s++;
    }
    *t++ = 0;
    printf("%s: %s\n", prefix, tmp);
}
static void tracecmd(const char* cmd)
{
    traceit(cmd, "cmd");
}
static void tracersp(const char* rsp)
{
    traceit(rsp, "rsp");
}
#define trace(...) printf(__VA_ARGS__)
#else
#define tracecmd(a)
#define tracersp(a)
#define trace(...)
#endif

static int read_oneline(polhemus_t *p)
{
    int rc;
    int count=0;

    while (count++ < 5)
    {
        if (p->pos > 0) {
            const char *c = strchr(p->buffer, '\r');
            if (c) {
                /* copy from buffer into response, then move
                   everything after it to the beginning of
                   buffer */
                char tmp[RSPLEN];
                p->response_length = c - p->buffer;
                memcpy(p->response, p->buffer, p->response_length);
                p->response[p->response_length] = 0;
                strcpy(tmp, c+2);
                strcpy(p->buffer, tmp);
                p->pos = strlen(p->buffer);
                tracersp(p->response);
                return 0;
            }
        }

        rc = read(p->rd, &p->buffer[p->pos], RSPLEN - p->pos);
        if (rc >= 0) {
            p->pos += rc;
            continue;
        }
        if (errno == EAGAIN) {
            usleep(100000);
            continue;
        }
        else {
            printf("[error %d] ", errno);
            fflush(stdout);
            perror("read");
            return 2;
        }
    }
    printf("Timed out while reading a line.\n");
    return 1;
}

static int read_until_timeout(polhemus_t *p, int ms)
{
    // TODO: check if anything is in p->buffer

    int rc, count=0, pos=0;
    while (count++ < (ms/5))
    {
        rc = read(p->rd, p->response+pos, RSPLEN);
        if (rc >= 0) {
            pos += rc;
            continue;
        }
        if (errno == EAGAIN) {
            usleep(5000);
            continue;
        }
        else {
            printf("[error %d] ", errno);
            fflush(stdout);
            perror("read");
            return 2;
        }
    }

    // should have read something, otherwise error
    if (pos) {
        p->response_length = pos;
        p->response[pos] = 0;
        tracersp(p->response);
        return 0;
    }
    return 1;
}

static int read_bytes(polhemus_t *p, int bytes)
{
    int rc;
    int count=0;

    while (count++ < 5)
    {
        if (p->pos > 0) {
            if (p->pos >= bytes) {
                const char *c = p->buffer + bytes;

                /* copy from buffer into response, then move
                   everything after it to the beginning of
                   buffer */
                char tmp[RSPLEN];
                p->response_length = bytes;
                int left = p->pos - bytes;
                memcpy(p->response, p->buffer, bytes);
                p->response[bytes] = 0;
                memcpy(tmp, c, left);
                memcpy(p->buffer, tmp, left);
                p->pos = left;
                tracersp(p->response);
                return 0;
            }
        }

        rc = read(p->rd, &p->buffer[p->pos], RSPLEN - p->pos);
        if (rc >= 0) {
            p->pos += rc;
            continue;
        }
        if (errno == EAGAIN) {
            usleep(100000);
            continue;
        }
        else {
            printf("[error %d] ", errno);
            fflush(stdout);
            perror("read");
            return 2;
        }
    }
    printf("Timed out while reading.\n");
    return 1;
}

static int open_device(polhemus_t *p, const char *device)
{
    struct termios newAtt;
    device_open = 0;
    p->device_open = 0;

    if (p->device_open)
        return 0;

    // Open serial device for reading and writing
    p->rd = open(device, O_RDONLY | O_NDELAY);
    if (p->rd == -1) {
        printf("Could not open device %s for reading.\n", device);
        perror("open (rd)");
        device_open = 0;
        return 1;
    }

    p->wr = open(device, O_WRONLY);
    if (p->wr == -1) {
        printf("Could not open device %s for writing.\n", device);
        perror("open (wr)");
        close(p->rd);
        device_open = 0;
        return 1;
    }

    // set up terminal for raw data
    tcgetattr(p->rd, &p->initialAtt);	// save this to restore later
    newAtt = p->initialAtt;
    cfmakeraw(&newAtt);
    if (tcsetattr(p->rd, TCSANOW, &newAtt)) {
        printf("Error setting terminal attributes\n");
        fflush(stdout);
        perror("tcsetattr");
        close(p->rd);
        close(p->wr);
        return 2;
    }

    device_open = 1;
    p->device_open = 1;
    return 0;
}

static int close_device(polhemus_t *p)
{
    if (!device_open)
        return 0;

    // restore the original attributes
    tcsetattr(p->rd, TCSANOW, &p->initialAtt);

    close(p->rd);
    close(p->wr);

    p->device_open = 0;
    device_open = 0;
    return 0;
}

static int is_initialized(polhemus_t *p)
{
    return p->device_open;
}

static int find_device(const char *device)
{
    struct stat st;
    if (stat(device, &st) == -1) {
        device_found = 0;
        return 1;
    }
    device_found = 1;
    return 0;
}

static void command(polhemus_t *p, const char *cmd)
{
    tracecmd(cmd);
    write(p->wr, cmd, strlen(cmd));
}

static int cmd_read_bits(polhemus_t *p)
{
    command(p, "\x14\r");
    return read_until_timeout(p, 100);
}

static int cmd_get_station_info(polhemus_t *p, int station)
{
    char cmd[50];
    sprintf(cmd, "\x16%d\r", station+1);
    command(p, cmd);
    if (read_until_timeout(p, 100))
        return 1;
    if (strstr(p->response, "ID:0\r"))
        return -1;
    return 0;
}

static int read_data_record(polhemus_t *p)
{
    if (p->binary) {
        int bytes = 0;
        if (p->fields & POLHEMUS_DATA_POSITION)
            bytes += 20;
        if (p->fields & POLHEMUS_DATA_EULER)
            bytes += 12;
        if (p->fields & POLHEMUS_DATA_TIMESTAMP)
            bytes += 4;
        if (p->fields & POLHEMUS_DATA_CRLF)
            bytes += 2;

        return read_bytes(p, bytes);
    } else
        return read_until_timeout(p, 100);
}

static int cmd_data_request(polhemus_t *p)
{
    command(p, "P");
    return 0;
}

static int cmd_data_request_continuous(polhemus_t *p)
{
    read_until_timeout(p, 100);
    command(p, "C\r");
    return 0;
}

static int cmd_get_stations(polhemus_t *p)
{
    // check which stations are plugged in for now, we'll assume
    // stations are plugged in from left to right, and that there
    // are only 8 max stations.
    for (p->stations=0; p->stations<8; p->stations++) {
        int rc = cmd_get_station_info(p, p->stations);
        if (rc < 0)
            break;
        else if (rc > 0)
            return rc;
    }
    if (p->stations == 0) {
        printf("No stations detected.\n");
        return 1;
    }
    else
        trace("%d station%s detected.\n", p->stations, p->stations>1?"s":"");
    return 0;
}

static int cmd_text_mode(polhemus_t *p)
{
    command(p, "F0\r");
    // no response
    p->binary = 0;
    return 0;
}

static int cmd_binary_mode(polhemus_t *p)
{
    command(p, "F1\r");
    // no response
    p->binary = 1;
    return 0;
}

static int cmd_get_version(polhemus_t *p)
{
    char cmd[10];
    sprintf(cmd, "%c\r", 22);
    command(p, cmd);
    if (read_until_timeout(p, 100))
        return 1;

    if (strstr(p->response, "Patriot"))
        p->tracker_type = POLHEMUS_PATRIOT;
    else if (strstr(p->response, "Liberty"))
        p->tracker_type = POLHEMUS_LIBERTY;
    else
        p->tracker_type = POLHEMUS_UNKNOWN;
    return 0;
}

static int cmd_set_hemisphere(polhemus_t *p)
{
    command(p, "H*,0,1,0\r");
    // no response
    return 0;
}

static int cmd_set_units(polhemus_t *p, polhemus_unit_type units)
{
    switch (units)
    {
    case POLHEMUS_UNITS_METRIC:
        command(p, "U1\r");
        break;
    default:
        printf("Unknown units specified.\n");
        break;
    }
    // no response
    return 0;
}

static int cmd_set_rate(polhemus_t *p, polhemus_rate rate)
{
    switch (rate)
    {
    case POLHEMUS_RATE_120:
        command(p, "R3\r");
        break;
    case POLHEMUS_RATE_240:
        command(p, "R4\r");
        break;
    default:
        printf("Unknown rate specified.\n");
        break;
    }
    // no response
    return 0;
}

static int cmd_set_data_fields(polhemus_t *p, int fields)
{
    char cmd[1024];
    cmd[0] = 0;
    strcat(cmd, "O*");

    if (fields & POLHEMUS_DATA_POSITION)
        strcat(cmd, ",2");

    if (fields & POLHEMUS_DATA_EULER)
        strcat(cmd, ",4");

    if (fields & POLHEMUS_DATA_TIMESTAMP)
        strcat(cmd, ",8");

    // ",1" indicates that lines should be terminated with cr/lf
    if (fields & POLHEMUS_DATA_CRLF)
        strcat(cmd, ",1");

    strcat(cmd, "\r");
    command(p, cmd);
    // no response
    p->fields = fields;

    // for some reason we need to wait after setting the fields
    usleep(100000);

    return 0;
}

int GetBinPno(polhemus_t *pol);

int main(int argc, char *argv[])
{
  if (argc < 4 || atoi(argv[1]) <= 0 || atoi(argv[2]) < 0 || atoi(argv[2]) > 1 || atoi(argv[3]) < 0 || atoi(argv[3]) > 1) {
        printf("Usage: %s <number of channels> <print location> <which data> "
               "<hexfloat>\n (print location: 0 -> send over OSC "
               "port 9999, 1-> print to terminal)\n (which data: 0 -> "
               "X,Y,Z + timestamp only, 1 -> X,Y,Z, & Euler Angles + "
               "timestamp)\n"
               " (hexfloat: 0 -> print floats in decimal, 1 -> in hex)\n\n",
               argv[0]);
        exit(1);
    }

    port = 0;
    host[0] = 0;
    numChannels = atoi(argv[1]);
    printswitch = atoi(argv[2]); //routes data to the terminal or over OSC
    whichdata = atoi(argv[3]);   //for outputting either 3DoF or 6DoF per marker + timestamp

    if (argc > 4)
        hexfloats = atoi(argv[4]);

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
        if (find_device(DEVICENAME))
            continue;

        // Don't open device if nobody is listening
        if (!started || port==0)
            continue;

        if (open_device(&pol, DEVICENAME))
            continue;

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
        if (cmd_data_request(&pol))
            break;

        read_until_timeout(&pol, 500);

        if (cmd_text_mode(&pol))
            break;

        // determine tracker type        
        if (cmd_get_version(&pol))
            break;

        // check for initialization errors
        if (cmd_read_bits(&pol))
            break;

        // check what stations are available
        if (cmd_get_stations(&pol))
            break;

        if (cmd_set_hemisphere(&pol))
            break;

        if (cmd_set_units(&pol, POLHEMUS_UNITS_METRIC))
            break;

        if (cmd_set_rate(&pol, POLHEMUS_RATE_240))
            break;

        if (whichdata) {
            if (cmd_set_data_fields(&pol,
                                    POLHEMUS_DATA_POSITION
                                    | POLHEMUS_DATA_EULER
                                    | POLHEMUS_DATA_TIMESTAMP))
                break;
        } else {
            if (cmd_set_data_fields(&pol,
                                    POLHEMUS_DATA_POSITION
                                    | POLHEMUS_DATA_TIMESTAMP))
                break;
        }

        gettimeofday(&temp, NULL);
        starttime = (temp.tv_sec * 1000.0) + (temp.tv_usec / 1000.0);

        if (cmd_binary_mode(&pol))
            break;

        if (cmd_data_request_continuous(&pol))
            break;

        /* loop getting data until stop is requested or error occurs */
        while (started && !GetBinPno(&pol)) {}

        // stop any incoming continuous data
        if (cmd_data_request(&pol))
            break;

        read_until_timeout(&pol, 500);
        read_until_timeout(&pol, 500);
        read_until_timeout(&pol, 500);

        if (cmd_text_mode(&pol))
            break;

        close_device(&pol);
        close(sockfd);
    }

    close_device(&pol);
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

typedef union {
    const int *i;
    const unsigned int *ui;
    const short *s;
    const unsigned short *us;
    const float *f;
    const char *c;
    const unsigned char *uc;
} multiptr;

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

/*     cmd_data_request(pol); */

    gettimeofday(&temp, NULL);
    curtime = ((temp.tv_sec * 1000.0) + (temp.tv_usec / 1000.0)) - starttime;
    
    //system("clear");
    //printf("Time: %ld \n", curtime);

    const float *pData;

    multiptr p;
    for (int s = 0; s < pol->stations; s++)
    {
        if (read_data_record(pol))
            return 1;

        p.c = pol->response;
        trace("response: %d bytes\n", pol->response_length);

        if (strncmp(p.c, "LY", 2)) {
            printf("LY expected, got %c%c.\n",
                   ((*p.s >> 0) & 0xFF),
                   ((*p.s >> 8) & 0xFF));
            return 1;
        }
        p.c += 2;

        int station = *p.c;
        trace("station %d\n", station);
        p.c += 1;

        // skip initiating command
        p.c += 1;

        int error = *p.c;
        if (error != ' ')
            printf("error %d ('%c') detected for station.\n",
                   error, error, station);
        p.c += 1;

        // skip reserved byte
        p.c += 1;

        int size = *p.s;
        trace("size: %d\n", size);
        p.s += 1;

        pData = p.f;

        if (whichdata) {
            p.f += 6;
        } else {
            p.f += 3;
        }

        // timestamp:
        unsigned int timestamp = 0;
        if (pol->fields & POLHEMUS_DATA_TIMESTAMP) {
            timestamp = *p.ui;
        }

        if (whichdata) {
            //X,Y,Z,azimuth,elevation,roll 
            if (printswitch) {
                if (hexfloats) {
                    int i;
                    printf("%d", station);
                    for (i=0; i<6; i++)
                        printf(", 0x%02x%02x%02x%02x",
                               ((*(int*)&pData[i])>>24) & 0xFF,
                               ((*(int*)&pData[i])>>16) & 0xFF,
                               ((*(int*)&pData[i])>> 8) & 0xFF,
                               ((*(int*)&pData[i])>> 0) & 0xFF);
                    printf(", %d, %f\n", timestamp, curtime);
                } else {
                    printf("%d, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %d, %f\n",
                           station, pData[0], pData[1], pData[2], pData[3],
                           pData[4], pData[5], timestamp, curtime);
                }
            }
        } else {
            // X,Y,Z only
            if (printswitch) {
                if (hexfloats) {
                    int i;
                    printf("%d", station);
                    for (i=0; i<3; i++)
                        printf(", 0x%02x%02x%02x%02x",
                               ((*(int*)&pData[i])>>24) & 0xFF,
                               ((*(int*)&pData[i])>>16) & 0xFF,
                               ((*(int*)&pData[i])>> 8) & 0xFF,
                               ((*(int*)&pData[i])>> 0) & 0xFF);
                    printf(", %d, %f\n", timestamp, curtime);
                } else {
                    printf("%d, %.4f, %.4f, %.4f, %d, %f\n", station,
                           pData[0], pData[1], pData[2], timestamp, curtime);
                }
            }
        }

        // skip cr/lf
        if (pol->fields & POLHEMUS_DATA_CRLF)
            p.c += 2;

        if (printswitch)
            continue;

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
