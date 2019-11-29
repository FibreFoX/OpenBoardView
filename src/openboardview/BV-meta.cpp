#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <cfloat>

#include "BV-meta.h"

#define FL __FILE__,__LINE__
void BVMeta::v2s( double v, char *s, size_t bs, char *format ) {
	if (v == DBL_MAX) snprintf(s, bs, "NA");
	else if (v == DBL_MIN) snprintf(s, bs, "OL");
	else snprintf(s, bs, format, v);
}

double BVMeta::voltage2double( char *r, char **ep ) {
	double result = DBL_MAX;
	char *multiplier_pos = NULL;
	char *p = r;

	if (!r) return DBL_MAX;
	if (*r == '\0') return DBL_MAX;
	fprintf(stdout, "%s:%d: Decoding '%s'\n",FL, r);
	multiplier_pos = strpbrk(p,"vV");
	if (multiplier_pos) {
		*multiplier_pos = '.';
	}

	result = strtod(r, ep);
	return result;
}

double BVMeta::resistance2double( char *r, char **ep ) {
	double multiplier = 1.0;
	double result = DBL_MAX;
	char *multiplier_pos = NULL;
	char *p = r;

	if (!r) return DBL_MAX;
	if (*r == '\0') return DBL_MAX;
	fprintf(stdout, "%s:%d: Decoding '%s'\n",FL, r);
	multiplier_pos = strpbrk(p,"rRkKmM");
	if (multiplier_pos) {
		fprintf(stdout,"%s:%d: Multiplier char is %c\n", FL, *multiplier_pos);
		switch (*multiplier_pos) {
			case 'r':
			case 'R': multiplier = 1.0;
						 break;
			case 'k':
			case 'K': multiplier = 1000.0;
						 break;
			case 'm':
			case 'M': multiplier = 1000000.0;
						 break;

			default:
						 fprintf(stdout,"%s:%d: Unknown multiplier char in '%s'\n", FL, r);
						 break;
		}
		fprintf(stdout,"%s:%d: Multiplier set to %0.1f\n", FL, multiplier);

		if (strchr(r,'.')) {
			//
			// Simple suffix notion,  1.2K
			//
			//
			result = strtod(r, ep) *multiplier;
			return result;

		} else {
			//
			// Infix notion, 1K2
			// 
			//
			*multiplier_pos = '.';
			result = strtod(r, ep) *multiplier;
			return result;
		}

	} else {
		// 
		// If there was no multiplier character, just do a normal strtod()
		//
		result = strtod(r, ep);
		return result;
	}

	return DBL_MAX;
}

char *BVMeta::resistance2str( char *buf, size_t s, double r ) {
	char mc[]="r";

	if ( r == DBL_MAX ) {
		snprintf(buf, s, "NA");
		return buf;
	}

	if (r == DBL_MIN ) {
		snprintf(buf, s, "OL");
		return buf;
	}

	if (r >= 1000000) {
		r /= 1000000;
		mc[0] = 'M';
	} else if (r >= 1000) {
		r /= 1000;
		mc[0] = 'k';
	}
	snprintf(buf, s, "%0.2f%s", r, (infix?"":mc));
	if (infix) {
		char *p = strchr(buf,'.');
		if (p) *p = mc[0];
	}
	return buf;
}

char *BVMeta::voltage2str( char *buf, size_t s, double v) {

		if ( v == DBL_MAX ) {
		snprintf(buf, s, "NA");
		return buf;
	}

	if (v == DBL_MIN ) {
		snprintf(buf, s, "OL");
		return buf;
	}

	snprintf(buf,s,"%0.2f", v );
	if (infix) {
		char *p = strchr(buf,'.');
		if (p) *p = 'v';
	}
	return buf;
}

std::string BVMeta::resistance2str( double r ) {
	char mc[]="r";
	char buf[20];

	if (r >= 1000000) {
		r /= 1000000;
		mc[0] = 'M';
	} else if (r >= 1000) {
		r /= 1000;
		mc[0] = 'k';
	}
	snprintf(buf,sizeof(buf),"%0.2f%s", r, (infix?"":mc));
	if (infix) {
		char *p = strchr(buf,'.');
		if (p) *p = mc[0];
	}
	return (std::string)buf;
}

std::string BVMeta::voltage2str( double v ) {
	char buf[20];
	snprintf(buf,sizeof(buf),"%0.2f", v );
	if (infix) {
		char *p = strchr(buf,'.');
		if (p) *p = 'v';
	}
	return (std::string)buf;

}

void BVMeta::meta2string( struct bvmeta_s m ) {
	/*
	 * This will convert the values in bvmeta_s to
	 * string versions that are then held in the objs
	 * fields.
	 *
	 * Clearly this isn't thread-safe but it works
	 * fine for the tasks it's being used for currently
	 *
	 * Other option is to have bvmeta_s have its own
	 * string storage but that has the memory impact.
	 *
	 */
	voltage2str( ds, sizeof(ds), m.diode );
	voltage2str( vs, sizeof(vs), m.volts );
	resistance2str( rs, sizeof(rs), m.resistance );
}

void BVMeta::load( std::string filename, std::string boardcode ) {
	FILE *f;
	char line[10240];

	f = fopen(filename.c_str(),"r");
	if (f) {

		data.clear();

		while (fgets(line, sizeof(line), f)) {

			//fprintf(stdout,"%s:%d: Parsing: %s\n", FL, line);

			/*
			 * white space and # denote comment/discard line
			 *
			 */
			if (*line == '#') continue;
			if (*line == ' ') continue;

			/*
			 * If the line contains our board code at the start, then we're good to go
			 *
			 */
			if (strstr(line, boardcode.c_str()) == line) {
				char *p = line +boardcode.size();

				if (*p != ' ') continue;

				bvmeta_s b;
				char *r = strrchr(line,'\r');
				char *n = strrchr(line,'\n');

				if (r) *r = '\0';
				if (n) *n = '\0';

				b.diode = b.volts = b.resistance = DBL_MAX;

				/*
				 * First char after the board code SHOULD be a space, if not, move on.
				 *
				 */
				if (*p != ' ') {
					continue;

				} else if (*p == ' ') {
					p++;
					char *ep = strchr(p,' ');
					if (ep) {
						*ep = '\0';

						b.netname = p;
						p = ep +1;


						if (strncmp(p, "===", 3)==0) {
							/*
							 * If we have an ALIAS request
							 *
							 */
							b.note = p;
							data.push_back(b); // we don't fill in data until the end.
							fprintf(stdout, "%s:%d: %s is aliased to %s\n", FL, b.netname.c_str(), b.note.c_str());
							continue;
						}


						/* end alias */

						if (strncmp(p,"na",2)==0) { b.diode = DBL_MAX; p+=3; }
						else if (strncmp(p,"ol",2)==0) { b.diode = DBL_MIN; p+=3; }
						else {
							b.diode = voltage2double( p, &ep );
							if (!ep) continue;
							p = ep +1;
						}

						if (strncmp(p,"na",2)==0) { b.volts = DBL_MAX; p+=3; }
						else if (strncmp(p,"ol",2)==0) { b.volts = DBL_MIN; p+=3; }
						else {
							b.volts = voltage2double( p, &ep );
							if (!ep) continue;
							p = ep +1;
						}

						if (strncmp(p,"na",2)==0){ b.resistance = DBL_MAX; p+=3; }
						else if (strncmp(p,"ol",2)==0){ b.resistance = DBL_MIN; p+=3; }
						else {
							b.resistance = resistance2double(p, &ep );
							if (!ep) continue;
							p = ep +1;
						}

						{ 
							char *q = strstr(p,"\\n");
							while(q) {
								*q = '\r'; q++;
								*q = '\n'; q++;
								q = strstr(q,"\\n");
							}
						}

						b.note = p;

						//						fprintf(stdout,"%s:%d: values- %0.2f/%0.2f/%0.2f/%s\n", FL, b.diode, b.volts, b.resistance, b.note.c_str());

						data.push_back(b);

					} // if EP
				}// if p
			} // if line
		} // while
		fclose(f);

		/*
		 * Post processing
		 *
		 * Expand aliases
		 *
		 */
		for (auto &a: data) {
			char ap[1024];
			snprintf(ap, sizeof(ap), "%s", a.note.c_str());

			if (strncmp(ap,"===",3)==0) {
//				fprintf(stdout,"%s:%d: %s is aliased to %s\n", FL, ap, a.note.c_str());
				for (auto b: data) {
					if (strcmp(ap+3, b.netname.c_str())==0) {
						//fprintf(stdout,"%s:%d: Alias hit %s === %s\n", FL, ap, b.netname.c_str());
						a.diode = b.diode;
						a.volts = b.volts;
						a.resistance = b.resistance;
						a.note = b.note;
					}
				}
			}
		}
	} else {
		fprintf(stderr,"%s:%d: Could not open '%s' for reading\n", FL, filename.c_str());
	}// if (f)
}

