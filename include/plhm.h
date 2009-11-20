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

#ifndef _PLHM_H_
#define _PLHM_H_

#include <termios.h>
#include <sys/time.h>

#define plhm_rsp_max 1024

typedef enum _plhm_device_type
{
    PLHM_UNKNOWN,
    PLHM_LIBERTY,
    PLHM_PATRIOT,
} plhm_device_type;

typedef enum _plhm_unit
{
    PLHM_UNITS_METRIC,
} plhm_unit;

typedef enum _plhm_rate
{
    PLHM_RATE_120,
    PLHM_RATE_240,
} plhm_rate;

/* This is a bit field. */
enum plhm_data_fields
{
    PLHM_DATA_POSITION = 1,
    PLHM_DATA_EULER = 2,
    PLHM_DATA_CRLF = 4,
    PLHM_DATA_TIMESTAMP = 8,
    // more..
};

typedef struct _plhm
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
    plhm_device_type device_type;
    int fields;
    int binary;
    int stations;
} plhm_t;

typedef struct _plhm_record
{
    int fields;
    int station;
    int error;
    float position[3];
    float euler[3];
    unsigned int timestamp;
    struct timeval readtime;
} plhm_record_t;

int plhm_find_device(const char *device);
int plhm_open_device(plhm_t *p, const char *device);
int plhm_close_device(plhm_t *p);
int plhm_is_initialized(plhm_t *p);
int plhm_read_bits(plhm_t *p);
int plhm_read_until_timeout(plhm_t *p, int ms);
int plhm_get_station_info(plhm_t *p, int station);
int plhm_data_request(plhm_t *p);
int plhm_data_request_continuous(plhm_t *p);
int plhm_read_data_record(plhm_t *p, plhm_record_t *r);
int plhm_get_stations(plhm_t *p);
int plhm_text_mode(plhm_t *p);
int plhm_binary_mode(plhm_t *p);
int plhm_get_version(plhm_t *p);
int plhm_set_hemisphere(plhm_t *p);
int plhm_set_units(plhm_t *p, plhm_unit units);
int plhm_set_rate(plhm_t *p, plhm_rate rate);
int plhm_set_data_fields(plhm_t *p, int fields);

#endif // _PLHM_H_
