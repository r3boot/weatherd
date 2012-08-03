int verbose;
int pid;

void log_info(char *msg);
void log_debug(char *msg);
void log_error(char *msg);
void setup_logging(int opt_verbose, char *package);
