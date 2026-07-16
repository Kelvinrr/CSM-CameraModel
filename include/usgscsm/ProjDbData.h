#ifndef INCLUDE_USGSCSM_PROJDBDATA_H_
#define INCLUDE_USGSCSM_PROJDBDATA_H_

// The PROJ database (proj.db) embedded as a byte array so it is compiled
// directly into libusgscsm and served from memory (no loose file at runtime).
// The array and its size are generated at build time by cmake/embed_proj_db.cmake.

extern "C" const unsigned char usgscsm_proj_db_data[];
extern "C" const unsigned long long usgscsm_proj_db_size;

#endif  // INCLUDE_USGSCSM_PROJDBDATA_H_
