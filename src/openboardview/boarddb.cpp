#include <cmath>
#include <iostream>
#include <limits.h>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <vector>
#include <cfloat>
using namespace std;

#include "boarddb.h"

int BoardDB::SetFilename(const std::string &f) {
	filename = f;
	return 0;
}

/*
static int sqlCallback(void *NotUsed, int argc, char **argv, char **azColName) {
    int i;
    for (i = 0; i < argc; i++) {
        printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    }
    printf("\n");
    return 0;
}
*/
	char sql_table_create_measurements[] =
		"CREATE TABLE measurements("
	    "ID INTEGER PRIMARY KEY AUTOINCREMENT,"
		 "selector TEXT," // NET/PIN/PART, though theoretically should only need NET:
	    "state INTEGER," // On, off, booting ?
		 "testtype TEXT," // diode, ohm, volts... something else? Temperature?
		 "value DOUBLE,"
		 "percentrange DOUBLE," // maybe 1% default
		 "highrange DOUBLE," // some things have non-symetrical range
		 "lowrange DOUBLE,"
		 "note TEXT,"
		 "ext TEXT );"; // extended data area, if we have an oversight and need to put in n:v pairs as text


	char sql_table_create_annotations[] =
	    "CREATE TABLE annotations("
	    "ID INTEGER PRIMARY KEY AUTOINCREMENT,"
	    "VISIBLE INTEGER,"
	    "PIN TEXT,"
	    "PART TEXT,"
	    "NET TEXT,"
	    "POSX INTEGER,"
	    "POSY INTEGER,"
	    "SIDE INTEGER,"
	    "NOTE TEXT );";

	char sql_table_create_nvpairs[] = 
		"CREATE TABLE nvpairs("
		"ID INTEGER PRIMARY KEY AUTOINCREMENT,"
		"NAME TEXT,"
		"VALUE TEXT );";

int BoardDB::Init(void) {

	char *zErrMsg = 0;
	int rc;
	//	char sql_table_test[] = "SELECT name FROM sqlite_master WHERE type='table' AND name='annotations'";

	if (!sqldb) {
	//	fprintf(stderr,"BoardDB::Init: returning early\n");
		return 1;
	}

	if (doesTableExist("nvpairs") == 0) {
			int lrc;

			lrc = sqlite3_exec(sqldb, sql_table_create_nvpairs, NULL, 0, &zErrMsg);
			if (lrc != SQLITE_OK) {
				if (debug) fprintf(stderr, "%s:%d:SQL error: %s\n", __FILE__, __LINE__, zErrMsg);
				sqlite3_free(zErrMsg);
			} else {
				if (debug) fprintf(stderr, "nvpairs table created successfully\n");
			}
	} else {
		checkForOldNVPairs();
	}

	/* Execute SQL statement */
	if (!doesTableExist("annotations")) {
		rc = sqlite3_exec(sqldb, sql_table_create_annotations, NULL, 0, &zErrMsg);
		if (rc != SQLITE_OK) {
			if (debug) fprintf(stderr, "SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		} else {
			if (debug) fprintf(stdout, "Annotations table created successfully\n");
		}
	}

	/*
	if (!doesTableExist("measurements")) {
		rc = sqlite3_exec(sqldb, sql_table_create_measurements, NULL, 0, &zErrMsg);
		if (rc != SQLITE_OK) {
			if (debug) fprintf(stderr, "SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		} else {
			if (debug) fprintf(stdout, "Measurements table created successfully\n");
		}
	}
	*/


	return 0;
}


int BoardDB::checkForOldNVPairs( void ) {
	sqlite3_stmt *stmt;
	char sql[1024];
	int rc;
	int result = 0;

	sqlite3_snprintf(sizeof(sql), sql, "SELECT COUNT(*) FROM sqlite_master WHERE name='nvpairs' AND SQL LIKE '%%UNIQUE%%';");
	rc = sqlite3_prepare_v2(sqldb, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		if (debug) fprintf(stderr,"%s:%d: Table test failed: %s\n", __FILE__,__LINE__, sqlite3_errmsg(sqldb));
		result = 0; // or throw
	} else {
		if ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
			result = sqlite3_column_int(stmt, 0);
		}
	}
	sqlite3_finalize(stmt);

	if (debug) fprintf(stderr,"%s:%d: %d nvpairs tables with unique constraint\r\n", __FILE__, __LINE__, result);
	if (result>0) {
		char *zErrMsg = 0;
		NVGenerateList();
		sqlite3_snprintf(sizeof(sql), sql, "DROP TABLE nvpairs;");
		rc = sqlite3_exec(sqldb, sql, NULL, 0, &zErrMsg);
		if (rc == SQLITE_OK) {
			int lrc;
			if (debug) fprintf(stderr,"%s:%d: Creating NVPAirs table\r\n", __FILE__, __LINE__);
			lrc = sqlite3_exec(sqldb, sql_table_create_nvpairs, NULL, 0, &zErrMsg);
			if (lrc != SQLITE_OK) {
				fprintf(stderr, "%s:%d: SQL error: %s\n", __FILE__, __LINE__, zErrMsg);
				sqlite3_free(zErrMsg);
			} else {
				if (debug) fprintf(stderr, "new nvpairs table created successfully\n");
				for (auto nvp: nvpairs) {
					NVAdd(nvp);
				}
			}

		} else {
				fprintf(stderr, "%s:%d: SQL error: %s\n", __FILE__, __LINE__, zErrMsg);
				sqlite3_free(zErrMsg);

		}

		// We have the old nvpairs table, and sadly we have to do a bit of stuffing around to convert it :(

	}

	return 0;
}

int BoardDB::doesTableExist( std::string tname ) {
	sqlite3_stmt *stmt;
	char sql[1024];
	int rc;
	int result = 0;

	sqlite3_snprintf(sizeof(sql), sql, "SELECT COUNT(name) FROM sqlite_master WHERE type='table' AND name='%s';", tname.c_str());
	rc = sqlite3_prepare_v2(sqldb, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		if (debug) fprintf(stderr,"%s:%d: Table test failed: %s\n", __FILE__,__LINE__, sqlite3_errmsg(sqldb));
		result = 0; // or throw
	} else {
		if ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
			result = sqlite3_column_int(stmt, 0);
		}
	}
	sqlite3_finalize(stmt);

	return result;
}


int BoardDB::Load(void) {
	std::string sqlfn                        = filename;
	auto pos                                 = sqlfn.rfind('.');
	if (pos != std::string::npos) sqlfn[pos] = '_';
	sqlfn += ".sqlite3";

	sqldb = nullptr;
	int r = sqlite3_open(sqlfn.c_str(), &sqldb);
	if (r) {
		if (debug) fprintf(stderr, "%s:%d: Can't open database: %s\r\n", FL, sqlite3_errmsg(sqldb));
	} else {
		if (debug) fprintf(stderr, "%s:%d: Opened database successfully\r\n", FL);
		Init();
		AnnotationGenerateList();
		NVGenerateList();
		PDFFavouritesGenerateList();
	}
	return 0;
}

int BoardDB::Close(void) {
	if (sqldb) {
		sqlite3_close(sqldb);
		sqldb = NULL;
	}

	return 0;
}

void BoardDB::AnnotationGenerateList(void) {
	sqlite3_stmt *stmt;
	//char sql[] = "SELECT id,side,posx,posy,net,part,pin,note from annotations where visible=1;";
	char sql[] = "SELECT id,side,posx,posy,net,part,pin,note,visible from annotations where visible>0";
	int rc;

	rc = sqlite3_prepare_v2(sqldb, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		if (debug) cerr << "SELECT failed: " << sqlite3_errmsg(sqldb) << endl;
		return; // or throw
	}

	annotations.clear();
	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		Annotation ann;
		ann.id      = sqlite3_column_int(stmt, 0);
		ann.side    = sqlite3_column_int(stmt, 1);
		ann.x       = sqlite3_column_int(stmt, 2);
		ann.y       = sqlite3_column_int(stmt, 3);
		ann.visible = sqlite3_column_int(stmt, 8);
		ann.hovered = false;

		const char *p = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
		if (!p) p     = "";
		ann.net       = p;

		p         = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 5));
		if (!p) p = "";
		ann.part  = p;

		p         = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
		if (!p) p = "";
		ann.pin   = p;

		p         = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7));
		if (!p) p = "";
		ann.note  = p;

		//debug=1;
		if (debug) fprintf(stderr,
			        "%d:%d(%d:%f,%f) Net:%s Part:%s Pin:%s: Note:%s\nAdded\n",
					  ann.visible,
			        ann.id,
			        ann.side,
			        ann.x,
			        ann.y,
			        ann.net.c_str(),
			        ann.part.c_str(),
			        ann.pin.c_str(),
			        ann.note.c_str());

		ann.vector_idx = annotations.size();
		annotations.push_back(ann);
	}
	if (rc != SQLITE_DONE) {
		if (debug) cerr << "SELECT failed: " << sqlite3_errmsg(sqldb) << endl;
		// if you return/throw here, don't forget the finalize
	}
	sqlite3_finalize(stmt);
}

int BoardDB::AnnotationAdd( Annotation a ) {
	int rowid = -1;
	sqlite3_stmt *stmt;
	const char *pzTest;
	char szSQL[] = "insert into annotations ( visible, side, posx, posy, net, part, pin, note ) values ( ?,?,?,?,?,?,?,? );";
	int rc = sqlite3_prepare( sqldb, szSQL, strlen(szSQL), &stmt, &pzTest );
	if (rc == SQLITE_OK) {
		int i = 1;
		// bind the values
		sqlite3_bind_int(stmt, i++, a.visible);
		sqlite3_bind_int(stmt, i++, a.side);
		sqlite3_bind_double(stmt, i++, a.x);
		sqlite3_bind_double(stmt, i++, a.y);
		sqlite3_bind_text(stmt, i++ , a.net.c_str(), a.net.size(), 0);
		sqlite3_bind_text(stmt, i++ , a.part.c_str(), a.part.size(), 0);
		sqlite3_bind_text(stmt, i++ , a.pin.c_str(), a.pin.size(), 0);
		sqlite3_bind_text(stmt, i++ , a.note.c_str(), a.note.size(), 0);

		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	if (rc != SQLITE_OK) {
		if (debug) {
			fprintf(stderr,"%s:%d: AnnotationAdd: Failed to add item to DB\r\n",FL);
			fprintf(stderr, "%s:%d: SQL error: %s\n", FL, sqlite3_errmsg(sqldb));
		}
		fprintf(stderr, "%s:%d: SQL error: %s\n", FL, sqlite3_errmsg(sqldb));
	} else {
		rowid = sqlite3_last_insert_rowid(sqldb);
		if (debug) fprintf(stderr, "%s:%d: AnnotationAdd created successfully ID=%d\r\n", FL, rowid );
	}

	return rowid;
}
int BoardDB::AnnotationUpdate( Annotation a ) {
	int rowid = -1;
	sqlite3_stmt *stmt;
	const char *pzTest;
	char szSQL[] = "update annotations set visible=?, side=?, posx=?, posy=?, net=?, part=?, pin=?, note=? where id=?;";
	int rc = sqlite3_prepare( sqldb, szSQL, strlen(szSQL), &stmt, &pzTest );
	if (rc == SQLITE_OK) {
		int i = 1;
		// bind the values
		sqlite3_bind_int(stmt, i++, a.visible);
		sqlite3_bind_int(stmt, i++, a.side);
		sqlite3_bind_double(stmt, i++, a.x);
		sqlite3_bind_double(stmt, i++, a.y);
		sqlite3_bind_text(stmt, i++ , a.net.c_str(), a.net.size(), 0);
		sqlite3_bind_text(stmt, i++ , a.part.c_str(), a.part.size(), 0);
		sqlite3_bind_text(stmt, i++ , a.pin.c_str(), a.pin.size(), 0);
		sqlite3_bind_text(stmt, i++ , a.note.c_str(), a.note.size(), 0);
		sqlite3_bind_int(stmt, i++, a.id);

		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	if (rc != SQLITE_OK) {
		if (debug) {
			fprintf(stderr,"%s:%d: AnnotationUpdate: Failed to add item to DB\r\n",FL);
			fprintf(stderr, "%s:%d: SQL error: %s\n", FL, sqlite3_errmsg(sqldb));
		}
		fprintf(stderr, "%s:%d: SQL error: %s\n", FL, sqlite3_errmsg(sqldb));
	} else {
		rowid = sqlite3_last_insert_rowid(sqldb);
		if (debug) fprintf(stderr, "%s:%d: AnnotationUpdate created successfully ID=%d\r\n", FL, rowid );
	}

	return rowid;
}


void BoardDB::AnnotationRemove(int id) {
	char sql[1024];
	char *zErrMsg = 0;
	int r;

	sqlite3_snprintf(sizeof(sql), sql, "UPDATE annotations set visible = 0 where id=%d;", id);
	r = sqlite3_exec(sqldb, sql, NULL, 0, &zErrMsg);
	if (r != SQLITE_OK) {
		if (debug) fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
	} else {
		if (debug) fprintf(stdout, "Records created successfully\n");
	}
}


void BoardDB::NVGenerateList(void) {
	sqlite3_stmt *stmt;
	char sql[] = "SELECT id,name,value from nvpairs;";
	int rc;

	rc = sqlite3_prepare_v2(sqldb, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		if (debug) cerr << "SELECT failed: " << sqlite3_errmsg(sqldb) << endl;
		return; // or throw
	}

	nvpairs.clear();
	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		NVPair nv;
		nv.id      = sqlite3_column_int(stmt, 0);
		nv.name	  = (char *)sqlite3_column_text(stmt, 1);
		nv.value	  = (char *)sqlite3_column_text(stmt, 2);

		if (debug) fprintf(stderr,
			        "%s:%d: %d(%s=%s)\n",
					  FL,
			        nv.id,
			        nv.name.c_str(),
			        nv.value.c_str()
					);

		nvpairs.push_back(nv);
	}

	if (rc != SQLITE_DONE) {
		if (debug) cerr << "SELECT failed: " << sqlite3_errmsg(sqldb) << endl;
		// if you return/throw here, don't forget the finalize
	}
	sqlite3_finalize(stmt);
}

void BoardDB::NVAdd( NVPair nv ) {
	NVAdd( nv.name, nv.value );
}


void BoardDB::NVAdd( std::string name, std::string value ) {
	char *zErrMsg = 0;

  sqlite3_stmt *stmt;
  const char *pzTest;
	char szSQL[] = "insert into nvpairs (name, value) values ( ?, ? );";
	int rc = sqlite3_prepare( sqldb, szSQL, strlen(szSQL), &stmt, &pzTest );
	if (rc == SQLITE_OK) {
		// bind the values
		sqlite3_bind_text(stmt, 1, name.c_str(), name.size(), 0 );
		sqlite3_bind_text(stmt, 2, value.c_str(), value.size(), 0);

		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

//	sqlite3_snprintf(sizeof(sql), sql, "INSERT into nvpairs ( name, value ) \ values ( '%s', '%s' );", name.c_str(), value.c_str());
//	r = sqlite3_exec(sqldb, sql, NULL, 0, &zErrMsg);
//
	if (rc != SQLITE_OK) {
		if (debug) {
			fprintf(stderr,"%s:%d: NVAdd: Failed to add item to DB (%s,%s)\n", __FILE__, __LINE__, name.c_str(), value.c_str()); 
			fprintf(stderr, "%s:%d: SQL error: %s\n", __FILE__, __LINE__, zErrMsg);
		}
		sqlite3_free(zErrMsg);
	} else {
		if (debug) fprintf(stderr, "%s:%d: NV Record created successfully (%s,%s)\r\n", __FILE__, __LINE__, name.c_str(), value.c_str());
	}
}

int BoardDB::NVGet(std::string name, std::vector<NVPair> &nvv ) {
	sqlite3_stmt *stmt;
	NVPair nv;
	char sql[1024]; 
	int rc;
	int rows = 0;

	sqlite3_snprintf(sizeof(sql), sql, "SELECT id,name,value from nvpairs where name='%s';", name.c_str());
	rc = sqlite3_prepare_v2(sqldb, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		if (debug) cerr << "SELECT failed: " << sqlite3_errmsg(sqldb) << endl;
		return 1; // or throw
	}

	nvv.clear();
	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		rows++;
		nv.id      = sqlite3_column_int(stmt, 0);
		nv.name	  = (char *)sqlite3_column_text(stmt, 1);
		nv.value	  = (char *)sqlite3_column_text(stmt, 2);
		nvv.push_back(nv);


		if (debug) fprintf(stderr,
			        "%s:%d: id=%d(%s=%s)\n",
					  FL,
			        nv.id,
			        nv.name.c_str(),
			        nv.value.c_str()
					);
	}

	if (rc != SQLITE_DONE) {
		if (debug) cerr << "SELECT failed: " << sqlite3_errmsg(sqldb) << endl;
		// if you return/throw here, don't forget the finalize
	}
	sqlite3_finalize(stmt);

	if (rows == 0) return -1;

	return 0;
}


int BoardDB::NVGet(std::string name, NVPair *nv ) {
	sqlite3_stmt *stmt;
	char sql[1024]; 
	int rc;
	int rows = 0;

	sqlite3_snprintf(sizeof(sql), sql, "SELECT id,name,value from nvpairs where name='%s';", name.c_str());
	rc = sqlite3_prepare_v2(sqldb, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		if (debug) cerr << "SELECT failed: " << sqlite3_errmsg(sqldb) << endl;
		return 1; // or throw
	}

	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		rows++;
		nv->id      = sqlite3_column_int(stmt, 0);
		nv->name	  = (char *)sqlite3_column_text(stmt, 1);
		nv->value	  = (char *)sqlite3_column_text(stmt, 2);

		if (debug) fprintf(stderr,
			        "%s:%d: id=%d(%s=%s)\n",
					  FL,
			        nv->id,
			        nv->name.c_str(),
			        nv->value.c_str()
					);
	}

	if (rc != SQLITE_DONE) {
		if (debug) cerr << "SELECT failed: " << sqlite3_errmsg(sqldb) << endl;
		// if you return/throw here, don't forget the finalize
	}
	sqlite3_finalize(stmt);

	if (rows == 0) return -1;

	return 0;
}


int BoardDB::NVGetStr( std::string name, std::string *value ) {
	int r;
	NVPair nv;
	r = NVGet( name, &nv );
	if (r == 0) *value = nv.value;
	return r;
}

int BoardDB::NVGetBool( std::string name, bool *value ) {
	int r;
	NVPair nv;
	r = NVGet( name, &nv );
	if (r == 0) *value = atoi(nv.value.c_str());
	return r;
}

int BoardDB::NVGetInt( std::string name, int *value ) {
	int r;
	NVPair nv;
	r = NVGet( name, &nv );
	if (r == 0) *value = atoi(nv.value.c_str());
	return r;
}


int BoardDB::NVGetDouble( std::string name, double *value ) {
	int r;
	NVPair nv;
	r = NVGet( name, &nv );
	if (r == 0) *value = atof(nv.value.c_str());
	return r;
}

int BoardDB::NVGetFloat( std::string name, float *value ) {
	int r;
	NVPair nv;
	r = NVGet( name, &nv );
	if (r == 0) *value = atof(nv.value.c_str());
	return r;
}

string BoardDB::NVGetStr( std::string name ) {
	int r;
	NVPair nv;
	r = NVGet( name, &nv );
	if (r == 0) return nv.value;
	return std::string("");
}


int BoardDB::NVGetInt( std::string name ) {
	NVPair nv;
	if (NVGet( name, &nv )) {
		return atoi(nv.value.c_str());
	} else {
		return INT_MIN;
	}
}

bool BoardDB::NVGetBool( std::string name ) {
	NVPair nv;
	if (NVGet( name, &nv ) == 0) {
		if (nv.value.front() == '1') return true;
	}
	return false;
}


float BoardDB::NVGetFloat( std::string name ) {
	NVPair nv;
	if (NVGet( name, &nv )) {
		return atof(nv.value.c_str());
	} else {
		return FLT_MIN;
	}
}

double BoardDB::NVGetDouble( std::string name ) {
	NVPair nv;
	if (NVGet( name, &nv )) {
		return atof(nv.value.c_str());
	} else {
		return DBL_MIN;
	}
}

std::string BoardDB::NVGetStr( std::string name, std::string defaultValue ) {

	NVPair nv;
	std::string value;

	int r = NVGet( name, &nv );
	if (r == 0) {
		NVSet( name, defaultValue );
		return defaultValue;
	}
	return nv.value;
}

int BoardDB::NVGetInt( std::string name, int defaultValue ) {
	int r;
	int value;
	r = NVGetInt( name, &value );
	if (r != 0) {
		NVSetInt( name, defaultValue );
		value = defaultValue;
	}
	return value;
}

bool BoardDB::NVGetBool( std::string name, bool defaultValue ) {
	int r;
	bool value;
	r = NVGetBool( name, &value );
	if (r != 0) {
		NVSetBool( name, defaultValue );
		value = defaultValue;
	}
	return value;
}

float BoardDB::NVGetFloat( std::string name, float defaultValue ) {
	int r;
	float value;
	r = NVGetFloat( name, &value );
	if (r != 0) {
		NVSetFloat( name, defaultValue );
		value = defaultValue;
	}
	return value;
}

double BoardDB::NVGetDouble( std::string name, double defaultValue ) {
	int r;
	double value;
	r = NVGetDouble( name, &value );
	if (r != 0) {
		NVSetDouble( name, defaultValue );
		value = defaultValue;
	}
	return value;
}


void BoardDB::NVRemove(int id) {
	char sql[1024];
	char *zErrMsg = 0;
	int r;

	sqlite3_snprintf(sizeof(sql), sql, "DELETE from nvpairs where id=%d;", id);
	if (debug) fprintf(stderr,"%s:%d: SQL='%s'\r\n", FL, sql);
	r = sqlite3_exec(sqldb, sql, NULL, 0, &zErrMsg);
	if (r != SQLITE_OK) {
		if (debug) fprintf(stderr, "%s:%d: SQL error: %s\n", FL, zErrMsg);
		sqlite3_free(zErrMsg);
	} else {
		if (debug) fprintf(stdout, "%s:%d: Records created successfully\n", FL);
	}
}

void BoardDB::NVSet(int id, std::string value) {
	char *zErrMsg = 0;
  sqlite3_stmt *stmt;
  const char *pzTest;
	char szSQL[] = "update nvpairs set value=? where id=?;";
	int rc = sqlite3_prepare( sqldb, szSQL, strlen(szSQL), &stmt, &pzTest );
	if (rc == SQLITE_OK) {
		// bind the values
		sqlite3_bind_text(stmt, 1, value.c_str(), value.size(), 0);
		sqlite3_bind_int(stmt, 2, id );

		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	if (rc != SQLITE_OK) {
		if (debug) fprintf(stderr, "%s:%d: SQL error: %s\n", FL, zErrMsg);
		sqlite3_free(zErrMsg);
	} else {
		if (debug) fprintf(stdout, "%s:%d: Records created successfully\n", FL);
	}
}

void BoardDB::NVSet(std::string name, std::string value) {
	char *zErrMsg = 0;
  sqlite3_stmt *stmt;
  const char *pzTest;
	char szSQL[] = "update nvpairs set value=? where name=?;";
	int rc = sqlite3_prepare( sqldb, szSQL, strlen(szSQL), &stmt, &pzTest );
	if (rc == SQLITE_OK) {
		// bind the values
		sqlite3_bind_text(stmt, 1, value.c_str(), value.size(), 0);
		sqlite3_bind_text(stmt, 2, name.c_str(), name.size(), 0);

		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

//	fprintf(stderr,"Setting %s to %s\n", name.c_str(), value.c_str());
//	sqlite3_snprintf(sizeof(sql), sql, "UPDATE nvpairs set value = '%s' where name='%s';", value.c_str(), name.c_str());
//	r = sqlite3_exec(sqldb, sql, NULL, 0, &zErrMsg);
	if (rc != SQLITE_OK) {
		//if (debug) fprintf(stderr, "SQL error: %s\n", zErrMsg);
		fprintf(stderr, "%s:%d: SQL error: %s\n", FL, zErrMsg);
		sqlite3_free(zErrMsg);
	} else {
		int rows = sqlite3_changes(sqldb);
		if (rows == 0) {
			if (debug) fprintf(stdout,"%s:%d: Row doesn't exist, creating (%s,%s)\r\n", FL ,name.c_str(), value.c_str());
			NVAdd(name,value);
		} else if (debug) fprintf(stdout, "%s:%d: Records created successfully\n", FL);
	//	sqlite3_db_cacheflush(sqldb);
	}
}


void BoardDB::NVSetInt(std::string name, int value) {
	NVSet(name, std::to_string(value));
}

void BoardDB::NVSetDouble(std::string name, double value) {
	NVSet(name, std::to_string(value));
}

void BoardDB::NVSetFloat(std::string name, float value) {
	NVSet(name, std::to_string(value));
}

void BoardDB::NVSetBool( std::string name, bool value ) {
//	fprintf(stderr,"Setting %s ...%c ", name.c_str(), value?'t':'f');
	NVSetInt( name, value?1:0 );
}

bool sortFavourites ( NVPair i, NVPair j ) {
	int a, b;
	a = strtol(i.value.c_str(), NULL, 10);
	b = strtol(j.value.c_str(), NULL, 10);
	return (a<b); 
}

void BoardDB::PDFFavouritesGenerateList(void) {
	NVGet("pdffavourite", pdfFavourites);
	std::sort(pdfFavourites.begin(), pdfFavourites.end(), sortFavourites);
}


