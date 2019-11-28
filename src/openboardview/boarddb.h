#include "sqlite3.h"

#ifndef FL
#define FL __FILE__,__LINE__
#endif

#ifndef __BOARDDB
#define __BOARDDB
#define BOARDDB_FNAME_LEN_MAX 2048

struct NVPair {
	int id;
	string name;
	string value;
};

struct Annotation {
	int id;
	int visible;
	int side;
	string note, net, part, pin;
	double x, y;
	bool hovered;
	int vector_idx;
};

struct BoardDB {
	std::string filename;
	sqlite3 *sqldb;
	bool debug = false;
	vector<Annotation> annotations;
	vector<NVPair> nvpairs;
	vector<NVPair> pdfFavourites;

	int Init(void);

	int SetFilename(const std::string &f);
	int Load(void);
	int Close(void);
	void Remove(int id);

	int AnnotationAdd( Annotation a );
	int AnnotationUpdate( Annotation a );
	void AnnotationRemove(int id);
	void AnnotationGenerateList(void);

	void PDFFavouritesGenerateList(void);

	void NVAdd( NVPair nv );
	void NVAdd(std::string name, std::string value);
	void NVRemove(int id );
	int checkForOldNVPairs( void );

	int NVGet(std::string name, std::vector<NVPair> &nvv );
	int NVGet(std::string name, NVPair *nv );
	int NVGetStr( std::string name, std::string *value );
	int NVGetBool( std::string name, bool *value );
	int NVGetInt( std::string name, int *value );
	int NVGetFloat( std::string name, float *value );
	int NVGetDouble( std::string name, double *value );

	string NVGetStr( std::string name );
	int NVGetInt( std::string name );
	bool NVGetBool( std::string name );
	float NVGetFloat( std::string name );
	double NVGetDouble( std::string name );

	string NVGetStr( std::string name, std::string defaultValue );
	int NVGetInt( std::string name, int defaultValue );
	bool NVGetBool( std::string name, bool defaultValue );
	float NVGetFloat( std::string name, float defaultValue );
	double NVGetDouble( std::string name, double defaultValue );

	void NVSet(int id, std::string value);
	void NVSet(std::string name, std::string value);
	void NVSetInt(std::string name, int value);
	void NVSetFloat(std::string name, float value);
	void NVSetDouble(std::string name, double value);
	void NVSetBool(std::string name, bool value );


	void NVGenerateList(void);

	int doesTableExist( std::string tname );
};

#endif
