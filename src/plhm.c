/* 
 * "plhm" and "libplhm" are copyright 2009, Stephen Sinclair and
 * authors listed in file AUTHORS.
 *
 * written at:
 *   Input Devices and Music Interaction Laboratory
 *   McGill University, Montreal, Canada
 *
 * This code is licensed under the GNU General Public License v2.1 or
 * later.  See COPYING for more information.
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>

#include <lo/lo.h>

#include "config.h"
#include <plhm.h>

double starttime;
double curtime;
struct timeval temp;

int listen_port=0;
int started = 0;
int device_found = 0;
int device_open = 0;
int data_good = 0;

lo_address addr = 0;

int printswitch = 0;
struct timeval prev;

void liblo_error(int num, const char *msg, const char *path);
int start_handler(const char *path, const char *types, lo_arg **argv, int argc,
                  void *data, void *user_data);
int stop_handler(const char *path, const char *types, lo_arg **argv, int argc,
                 void *data, void *user_data);
int status_handler(const char *path, const char *types, lo_arg **argv, int argc,
                   void *data, void *user_data);

int read_stations_and_send(plhm_t *pol);

typedef union {
    const int *i;
    const unsigned int *ui;
    const short *s;
    const unsigned short *us;
    const float *f;
    const char *c;
    const unsigned char *uc;
} multiptr;

/* macros */
#define CHECKBRK(m,x) if (x) { printf("[plhm] error: " m "\n"); break; }
#define LOG(...) if (outfile) { fprintf(outfile, __VA_ARGS__); }

/* option flags */
static int daemon_flag = 0;
static int hex_flag = 0;
static int euler_flag = 0;
static int position_flag = 0;

const char *device_name = "/dev/ttyUSB0";
const char *osc_url = 0;

FILE *outfile = 0;

int main(int argc, char *argv[])
{
    static struct option long_options[] =
    {
        {"daemon",   no_argument,       &daemon_flag,   1},
        {"device",   required_argument, 0,              'd'},
        {"hex",      no_argument,       &hex_flag,      1},
        {"euler",    no_argument,       &euler_flag,    1},
        {"position", no_argument,       &position_flag, 1},
        {"output",   optional_argument, 0,              'o'},
        {"oscurl",   required_argument, 0,              'u'},
        {"oscport",  required_argument, 0,              'p'},
        {"help",     no_argument,       0,              0},
        {"version",  no_argument,       0,              'V'},
        {0, 0, 0, 0}
    };

    while (1)
    {
        int option_index = 0;
        int c = getopt_long(argc, argv, "Dd:HEPo::u:p:hV",
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

        case 'P':
            position_flag = 1;
            break;

        case 'E':
            euler_flag = 1;
            break;

        case 'd':
            // serial device name
            device_name = optarg;
            break;

        case 'u':
            // handle OSC url (liblo)
            osc_url = optarg;
            break;

        case 'p':
            listen_port = atoi(optarg);
            break;

        case 'o':
            // output file name, if specified
            // otherwise, stdout
            if (optarg) {
                outfile = fopen(optarg, "w");
            }
            else
                outfile = stdout;
            break;

        case 'V':
            printf(PACKAGE_STRING "  (" __DATE__ ")");
            exit(0);
            break;

        case '?':
            break;

        default:
        case 'h':
            printf("Usage: %s [options]\n"
"  where options are:\n"
"  -D --daemon           wait indefinitely for device\n"
"  -d --device=<device>  specify the serial device to use\n"
"  -P --position         request position data\n"
"  -E --euler            request euler angle data\n"
"  -o --output=[path]    write data to stdout, or to a file\n"
"                        if path is specified\n"
"  -H --hex              write float values as hexidecimal\n"
"  -u --oscurl=<url>     provide a URL for OSC destination\n"
"                        this URL must be liblo-compatible,\n"
"                        e.g., osc.udp://localhost:9999\n"
"                        this option is required to enable\n"
"                        the Open Sound Control interface\n"
"  -p --oscport=<port>   port on which to listen for OSC messages\n"
"  -V --version          print the version string and exit\n"
"  -h --help             show this help\n"
                   , argv[0]);
            exit(c!='h');
            break;
        }
    }

    int slp = 0;

    plhm_t pol;
    memset((void*)&pol, 0, sizeof(plhm_t));

    // setup OSC server
    lo_server_thread st = 0;
    if (listen_port > 0)
    {
        char str[256];
        sprintf(str, "%d", listen_port);
        st = lo_server_thread_new(str, liblo_error);
        lo_server_thread_add_method(st, "/liberty/start", "si",
                                    start_handler, NULL);
        lo_server_thread_add_method(st, "/liberty/start", "i",
                                    start_handler, NULL);
        lo_server_thread_add_method(st, "/liberty/stop", "",
                                    stop_handler, NULL);
        lo_server_thread_add_method(st, "/liberty/status", "si",
                                    status_handler, NULL);
        lo_server_thread_add_method(st, "/liberty/status", "i",
                                    status_handler, NULL);
        lo_server_thread_start(st);
    }

    if (osc_url != 0) {
        addr = lo_address_new_from_url(osc_url);
        if (!addr) {
            printf("[plhm] Couldn't open OSC address %s\n", osc_url);
            exit(1);
        }
    }

    started = 1;

    while (1) {
        sleep(slp);
        slp = 1;
        
        // Loop until device is available.
        if (plhm_find_device(device_name)) {
            if (daemon_flag)
                continue;
            else {
                printf("[plhm] Could not find device at %s\n", device_name);
                break;
            }
        }

        // Don't open device if nobody is listening
        if (!(started && (addr || outfile)) && daemon_flag)
            continue;

        if (plhm_open_device(&pol, device_name))
        {
            if (daemon_flag)
                continue;
            else {
                printf("[plhm] Could not open device %s\n", device_name);
                break;
            }
        }

        // stop any incoming continuous data just in case
        // ignore the response
        CHECKBRK("data_request",plhm_data_request(&pol));

        plhm_read_until_timeout(&pol, 500);

        CHECKBRK("text_mode",plhm_text_mode(&pol));

        // determine tracker type        
        CHECKBRK("get_version",plhm_get_version(&pol));

        // check for initialization errors
        CHECKBRK("read_bits",plhm_read_bits(&pol));

        // check what stations are available
        CHECKBRK("get_stations",plhm_get_stations(&pol));

        CHECKBRK("set_hemisphere",plhm_set_hemisphere(&pol));

        CHECKBRK("set_units",plhm_set_units(&pol, PLHM_UNITS_METRIC));

        CHECKBRK("set_rate",plhm_set_rate(&pol, PLHM_RATE_240));

        CHECKBRK("set_data_fields",
                 plhm_set_data_fields(&pol,
                                      (position_flag ? PLHM_DATA_POSITION : 0)
                                      | (euler_flag ? PLHM_DATA_EULER : 0)
                                      | PLHM_DATA_TIMESTAMP));

        gettimeofday(&temp, NULL);
        starttime = (temp.tv_sec * 1000.0) + (temp.tv_usec / 1000.0);

        CHECKBRK("binary_mode",plhm_binary_mode(&pol));

        CHECKBRK("data_request_continuous",plhm_data_request_continuous(&pol));

        /* loop getting data until stop is requested or error occurs */
        while (started && !read_stations_and_send(&pol)) {}

        // stop any incoming continuous data
        CHECKBRK("data_request",plhm_data_request(&pol));

        plhm_read_until_timeout(&pol, 500);
        plhm_read_until_timeout(&pol, 500);
        plhm_read_until_timeout(&pol, 500);

        CHECKBRK("text_mode",plhm_text_mode(&pol));

        plhm_close_device(&pol);
    }

    plhm_close_device(&pol);

    if (st)
        lo_server_thread_free(st);
    if (addr)
        lo_address_free(addr);
    if (outfile && (outfile != stdout))
        fclose(outfile);

    return 0;
}



/* Subtract the `struct timeval' values X and Y,
   storing the result in RESULT.
   Return 1 if the difference is negative, otherwise 0.  */

int timeval_subtract (struct timeval *result,
                      const struct timeval *x,
                      const struct timeval *y)
{
    /* Compute the time remaining to wait.
       tv_usec is certainly positive. */
    result->tv_sec = x->tv_sec - y->tv_sec;
    result->tv_usec = x->tv_usec - y->tv_usec;

    /* Return 1 if result is negative. */
    return x->tv_sec < y->tv_sec;
}

int read_stations_and_send(plhm_t *pol)
{
    int i = 0;
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
    
    const float *pData;
    plhm_record_t rec;
    multiptr p;
    int s;

    for (s = 0; s < pol->stations; s++)
    {
        if (plhm_read_data_record(pol, &rec))
            return 1;

        curtime = ((rec.readtime.tv_sec * 1000.0)
                   + (rec.readtime.tv_usec / 1000.0));

        LOG("%d", rec.station);

        if (rec.fields & PLHM_DATA_POSITION)
        {
            if (hex_flag)
                for (i=0; i<3; i++) {
                    p.f = &rec.position[i];
                    LOG(", 0x%02x%02x%02x%02x",
                        p.uc[0] & 0xFF,
                        p.uc[1] & 0xFF,
                        p.uc[2] & 0xFF,
                        p.uc[3] & 0xFF);
                }
            else
                LOG(", %.4f, %.4f, %.4f",
                    rec.position[0],
                    rec.position[1],
                    rec.position[2]);
        }

        if (rec.fields & PLHM_DATA_EULER)
        {
            if (hex_flag)
                for (i=0; i<3; i++) {
                    p.f = &rec.euler[i];
                    LOG(", 0x%02x%02x%02x%02x",
                        p.uc[0] & 0xFF,
                        p.uc[1] & 0xFF,
                        p.uc[2] & 0xFF,
                        p.uc[3] & 0xFF);
                }
            else
                LOG(", %.4f, %.4f, %.4f",
                    rec.euler[0],
                    rec.euler[1],
                    rec.euler[2]);
        }

        if (rec.fields & PLHM_DATA_TIMESTAMP)
            LOG(", %u", rec.timestamp);

        LOG(", %f\n", curtime);

        if (!addr)
            continue;

        int station = rec.station;
        pData = &rec.position[0];

        char path[30];
        if (rec.fields & PLHM_DATA_POSITION)
        {
            sprintf(path, "/liberty/marker/%d/x", station);
            lo_send(addr, path, "f", rec.position[0]);

            sprintf(path, "/liberty/marker/%d/y", station);
            lo_send(addr, path, "f", rec.position[1]);

            sprintf(path, "/liberty/marker/%d/z", station);
            lo_send(addr, path, "f", rec.position[2]);
        }

        if (rec.fields & PLHM_DATA_EULER)
        {
            sprintf(path, "/liberty/marker/%d/azimuth", station);
            lo_send(addr, path, "f", rec.euler[0]);

            sprintf(path, "/liberty/marker/%d/elevation", station);
            lo_send(addr, path, "f", rec.euler[1]);

            sprintf(path, "/liberty/marker/%d/roll", station);
            lo_send(addr, path, "f", rec.euler[2]);
        }

        if (rec.fields & PLHM_DATA_TIMESTAMP)
        {
            sprintf(path, "/liberty/marker/%d/timestamp", station);
            lo_send(addr, path, "i", rec.timestamp);
        }

        sprintf(path, "/liberty/marker/%d/readtime", station);
        lo_send(addr, path, "f", curtime);
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
    int port;
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

    char url[256];
    sprintf(url, "osc.udp://%s:%d", hostname, port);
    lo_address a = lo_address_new_from_url(url);
    if (a && addr) lo_address_free(addr);
    addr = a;
    printf("starting... %s\n", url);

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
