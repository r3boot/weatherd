#include <stdbool.h>
#include <termios.h>

#define PAR_NONE 0
#define PAR_EVEN 1
#define PAR_ODD 2

char *line;
int speed;
int databits;
int parity;
int stopbits;

int serial_fd;

struct termios m_options;

void u_sleep(int64_t usecs, volatile bool* stop);
int64_t get_time_us();
int64_t get_time_sec();
int int_to_rate(int baudrate);
int serial_set_baudrate(int baudrate);
int setup_serial(char *port_name, int baudrate, int databits, int stopbits, int parity);
int serial_read(char *data, int len, int64_t usecs);
void serial_close();
void serial_setdtr(bool high);
void serial_setrts(bool high);
void *serial_thread(void *queue_ptr);
