typedef struct sTPV {
	char time[32];
	double	lat;
	double lon;
	double speed;
} sTPV;

int gps_init(void);
int gps_disable(void);
int parse_json_tpv(struct sTPV *);

