/*
 * $Id$
 */

#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>

using std::string;

/* The AdjustValue functions used to be #define'd macros, but these are
 * evil for several reasons, *especially* when they contain multiple
 * statements, and have if statements in them. The macros did both.
 * See http://www.parashift.com/c++-faq-lite/inline-functions.html#faq-9.5
 * for more information.
 */
inline void AdjustValueMax(int &v, int val, int max) { v += val; if (v > max) v = max; }
inline void AdjustValueMin(int &v, int val, int min) { v += val; if (v < min) v = min; }
inline void AdjustValue(int &v, int val, int max, int min) { v += val; if (v > max) v = max; if (v < min) v = min; }

inline void AdjustValueMax(short &v, int val, int max) { v += val; if (v > max) v = max; }
inline void AdjustValueMin(short &v, int val, int min) { v += val; if (v < min) v = min; }
inline void AdjustValue(short &v, int val, int max, int min) { v += val; if (v > max) v = max; if (v < min) v = min; }

inline void AdjustValueMax(unsigned short &v, int val, int max) { v += val; if (v > max) v = max; }
inline void AdjustValueMin(unsigned short &v, int val, int min) { v += val; if (v < min) v = min; }
inline void AdjustValue(unsigned short &v, int val, int max, int min) { v += val; if (v > max) v = max; if (v < min) v = min; }

void xu4_srandom(uint32_t);
extern "C" int xu4_random(int upperval);
string& trim(string &val, const string &chars_to_trim = "\t\013\014 \n\r");
string& lowercase(string &val);
string& uppercase(string &val);
std::vector<string> split(const string &s, const string &separators);

#endif
