
struct s_graphite_config {
	char host[1024];
	char port[6];
	char namespace[2048];
};

struct s_graphite_entry {
	char data[4096];
};

int setup_graphite(struct s_graphite_config *config);
int graphite_write(struct s_graphite_entry *entry);