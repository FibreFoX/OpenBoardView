
struct obdata_s {
	std::string netname;
	double diode;
	double volts;
	double resistance;
	std::string note;
};

struct OBData {
	std::vector<obdata_s> data;
	bool infix = false;

	char ds[20], vs[20], rs[20]; // suspect this won't be overly threadsafe

	void v2s( double v, char *s, size_t bs, char *format );
	void meta2string( struct obdata_s m );

	double resistance2double( char *r, char **ep );
	double voltage2double( char *r, char **ep );

	char *voltage2str( char *buf, size_t s, double v );
	char *resistance2str( char *buf, size_t s, double r );

	std::string resistance2str( double r);
	std::string voltage2str( double v);

	void load( std::string filename, std::string boardcode );
};
