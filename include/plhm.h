#ifndef _PLHM_H_
#define _PLHM_H_

#include <termios.h>
#include <sys/time.h>

#define plhm_rsp_max 1024

typedef enum _polhemus_tracker_type
{
    POLHEMUS_UNKNOWN,
    POLHEMUS_LIBERTY,
    POLHEMUS_PATRIOT,
} polhemus_tracker_type;

typedef enum _polhemus_unit_type
{
    POLHEMUS_UNITS_METRIC,
} polhemus_unit_type;

typedef enum _polhemus_rate
{
    POLHEMUS_RATE_120,
    POLHEMUS_RATE_240,
} polhemus_rate;

/* This is a bit field. */
enum polhemus_data_fields
{
    POLHEMUS_DATA_POSITION = 1,
    POLHEMUS_DATA_EULER = 2,
    POLHEMUS_DATA_CRLF = 4,
    POLHEMUS_DATA_TIMESTAMP = 8,
    // more..
};

typedef struct _polhemus
{
    // input / output serial ports
    int rd;
    int wr;
    char response[plhm_rsp_max];
    char buffer[plhm_rsp_max];
    int pos;
    int response_length;
    int device_open;
    struct termios initialAtt;
    polhemus_tracker_type tracker_type;
    int fields;
    int binary;
    int stations;
} polhemus_t;

typedef struct _polhemus_record
{
    int fields;
    int station;
    int error;
    float position[3];
    float euler[3];
    unsigned int timestamp;
    struct timeval readtime;
} polhemus_record_t;

int plhm_find_device(const char *device);
int plhm_open_device(polhemus_t *p, const char *device);
int plhm_close_device(polhemus_t *p);
int plhm_is_initialized(polhemus_t *p);
int plhm_read_bits(polhemus_t *p);
int plhm_read_until_timeout(polhemus_t *p, int ms);
int plhm_get_station_info(polhemus_t *p, int station);
int plhm_data_request(polhemus_t *p);
int plhm_data_request_continuous(polhemus_t *p);
int plhm_read_data_record(polhemus_t *p, polhemus_record_t *r);
int plhm_get_stations(polhemus_t *p);
int plhm_text_mode(polhemus_t *p);
int plhm_binary_mode(polhemus_t *p);
int plhm_get_version(polhemus_t *p);
int plhm_set_hemisphere(polhemus_t *p);
int plhm_set_units(polhemus_t *p, polhemus_unit_type units);
int plhm_set_rate(polhemus_t *p, polhemus_rate rate);
int plhm_set_data_fields(polhemus_t *p, int fields);

#endif // _PLHM_H_
