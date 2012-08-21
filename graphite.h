
struct s_graphite_config {
	char host[1024];
	char port[6];
	char namespace[2048];
};

struct s_graphite_entry {
	int host_id;
	float temperature;
	float humidity;
	float rainfall;
	float pressure;
	float wind_speed;
	long wind_direction;
	float light;
	char *timestamp;
};

int setup_graphite(struct s_graphite_config *config);
int graphite_write(struct s_graphite_entry *entry);
