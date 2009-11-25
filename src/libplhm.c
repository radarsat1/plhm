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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <ctype.h>

#include "plhm.h"

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

static int read_oneline(plhm_t *p)
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
                char tmp[plhm_rsp_max];
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

        rc = read(p->rd, &p->buffer[p->pos], plhm_rsp_max - p->pos);
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

int plhm_read_until_timeout(plhm_t *p, int ms)
{
    // TODO: check if anything is in p->buffer

    int rc, count=0, pos=0;
    while (count++ < (ms/5))
    {
        rc = read(p->rd, p->response+pos, plhm_rsp_max);
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

static int read_bytes(plhm_t *p, int bytes)
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
                char tmp[plhm_rsp_max];
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

        rc = read(p->rd, &p->buffer[p->pos], plhm_rsp_max - p->pos);
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
    printf("Timed out while reading.  Expected %d bytes, got %d.\n",
           bytes, p->pos);
    return 1;
}

int plhm_open_device(plhm_t *p, const char *device)
{
    struct termios newAtt;
    p->device_open = 0;

    if (p->device_open)
        return 0;

    // Open serial device for reading and writing
    p->rd = open(device, O_RDONLY | O_NDELAY);
    if (p->rd == -1) {
        printf("Could not open device %s for reading.\n", device);
        perror("open (rd)");
        return 1;
    }

    p->wr = open(device, O_WRONLY);
    if (p->wr == -1) {
        printf("Could not open device %s for writing.\n", device);
        perror("open (wr)");
        close(p->rd);
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

    p->device_open = 1;
    return 0;
}

int plhm_close_device(plhm_t *p)
{
    if (!p->device_open)
        return 0;

    // restore the original attributes
    tcsetattr(p->rd, TCSANOW, &p->initialAtt);

    close(p->rd);
    close(p->wr);

    p->device_open = 0;
    return 0;
}

int plhm_is_initialized(plhm_t *p)
{
    return p->device_open;
}

void plhm_reset(plhm_t *p)
{
    command(p, "\x19\r");
    sleep(10);
    plhm_read_until_timeout(p, 1000);
    return;
}

int plhm_find_device(const char *device)
{
    struct stat st;
    if (stat(device, &st) == -1) {
        return 1;
    }
    return 0;
}

void command(plhm_t *p, const char *cmd)
{
    tracecmd(cmd);
    write(p->wr, cmd, strlen(cmd));
}

int plhm_read_bits(plhm_t *p)
{
    command(p, "\x14\r");
    return plhm_read_until_timeout(p, 100);
}

int plhm_get_station_info(plhm_t *p, int station)
{
    char cmd[50];
    sprintf(cmd, "\x16%d\r", station+1);
    command(p, cmd);
    if (plhm_read_until_timeout(p, 100))
        return 1;
    if (strstr(p->response, "ID:0\r"))
        return -1;
    return 0;
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

int plhm_read_data_record(plhm_t *p, plhm_record_t *r)
{
    int rc, bytes;
    multiptr data;

    if (p->binary) {
        bytes = 0;
        if (p->fields & PLHM_DATA_POSITION)
            bytes += 20;
        if (p->fields & PLHM_DATA_EULER)
            bytes += 12;
        if (p->fields & PLHM_DATA_TIMESTAMP)
            bytes += 4;
        if (p->fields & PLHM_DATA_CRLF)
            bytes += 2;

        rc = read_bytes(p, bytes);
        if (rc) return rc;

        gettimeofday(&r->readtime, NULL);

        r->fields = p->fields;
        data.c = p->response;

        if (p->response_length != bytes) {
            trace("response not the expected length.  got %d bytes, "
                  "but expected %d\n", p->response_length, bytes);
            return 1;
        }

        if (strncmp(data.c, "LY", 2)) {
            printf("LY expected, got %c%c.\n",
                   ((*data.s >> 0) & 0xFF),
                   ((*data.s >> 8) & 0xFF));
            return 1;
        }
        data.c += 2;

        r->station = *data.c;
        trace("station %d\n", r->station);
        data.c += 1;

        // skip initiating command
        data.c += 1;

        r->error = *data.c;
        if (r->error != ' ')
            printf("error %d ('%c') detected for station %d.\n",
                   r->error, r->error, r->station);
        data.c += 1;

        // skip reserved byte
        data.c += 1;

        int size = *data.s;
        trace("size: %d\n", size);
        if (size != (bytes - 8))
            printf("error: size of record is %d, expected %d.\n",
                   size, bytes - 8);
        data.s += 1;

        if (p->fields & PLHM_DATA_POSITION) {
            r->position[0] = *data.f++;
            r->position[1] = *data.f++;
            r->position[2] = *data.f++;
        }

        if (p->fields & PLHM_DATA_EULER) {
            r->euler[0] = *data.f++;
            r->euler[1] = *data.f++;
            r->euler[2] = *data.f++;
        }

        if (p->fields & PLHM_DATA_TIMESTAMP) {
            r->timestamp = *data.ui++;
        }

        // skip cr/lf
        if (p->fields & PLHM_DATA_CRLF)
            data.c += 2;
    } else
        return plhm_read_until_timeout(p, 100);
    return 0;
}

int plhm_data_request(plhm_t *p)
{
    command(p, "P");
    return 0;
}

int plhm_data_request_continuous(plhm_t *p)
{
    plhm_read_until_timeout(p, 100);
    command(p, "C\r");
    return 0;
}

int plhm_get_stations(plhm_t *p)
{
    // check which stations are plugged in for now, we'll assume
    // stations are plugged in from left to right, and that there
    // are only 8 max stations.
    for (p->stations=0; p->stations<8; p->stations++) {
        int rc = plhm_get_station_info(p, p->stations);
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

int plhm_text_mode(plhm_t *p)
{
    command(p, "F0\r");
    // no response
    p->binary = 0;
    return 0;
}

int plhm_binary_mode(plhm_t *p)
{
    command(p, "F1\r");
    // no response
    p->binary = 1;
    return 0;
}

int plhm_get_version(plhm_t *p)
{
    char cmd[10];
    sprintf(cmd, "%c\r", 22);
    command(p, cmd);
    if (plhm_read_until_timeout(p, 100))
        return 1;

    if (strstr(p->response, "Patriot"))
        p->device_type = PLHM_PATRIOT;
    else if (strstr(p->response, "Liberty"))
        p->device_type = PLHM_LIBERTY;
    else
        p->device_type = PLHM_UNKNOWN;
    return 0;
}

int plhm_set_hemisphere(plhm_t *p)
{
    command(p, "H*,0,1,0\r");
    // no response
    return 0;
}

int plhm_set_units(plhm_t *p, plhm_unit units)
{
    switch (units)
    {
    case PLHM_UNITS_METRIC:
        command(p, "U1\r");
        break;
    default:
        printf("Unknown units specified.\n");
        break;
    }
    // no response
    return 0;
}

int plhm_set_rate(plhm_t *p, plhm_rate rate)
{
    switch (rate)
    {
    case PLHM_RATE_120:
        command(p, "R3\r");
        break;
    case PLHM_RATE_240:
        command(p, "R4\r");
        break;
    default:
        printf("Unknown rate specified.\n");
        break;
    }
    // no response
    return 0;
}

int plhm_set_data_fields(plhm_t *p, int fields)
{
    char cmd[1024];
    cmd[0] = 0;
    strcat(cmd, "O*");

    if (fields & PLHM_DATA_POSITION)
        strcat(cmd, ",2");

    if (fields & PLHM_DATA_EULER)
        strcat(cmd, ",4");

    if (fields & PLHM_DATA_TIMESTAMP)
        strcat(cmd, ",8");

    // ",1" indicates that lines should be terminated with cr/lf
    if (fields & PLHM_DATA_CRLF)
        strcat(cmd, ",1");

    strcat(cmd, "\r");
    command(p, cmd);
    // no response
    p->fields = fields;

    // for some reason we need to wait after setting the fields
    usleep(100000);

    return 0;
}
