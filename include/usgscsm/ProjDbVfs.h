#ifndef INCLUDE_USGSCSM_PROJDBVFS_H_
#define INCLUDE_USGSCSM_PROJDBVFS_H_

#include <string>

// A custom read-only SQLite3 VFS that serves the embedded proj.db byte array
// (see ProjDbData.h) directly from memory. PROJ opens proj.db read-only and
// lets a caller select a VFS by name via proj_context_set_sqlite3_vfs_name();
// this lets the plugin carry its own PROJ database with no loose file on disk.
//
// The stock sqlite ext/misc/memvfs.c passes the buffer address as URI
// parameters in the open filename, but PROJ opens with a plain path and no
// SQLITE_OPEN_URI flag, so a purpose-built VFS is used instead.

namespace usgscsm {

// Register the in-memory proj.db VFS with SQLite (idempotent; thread-safe).
// Returns the VFS name to pass to proj_context_set_sqlite3_vfs_name().
const std::string& ensureProjDbVfsRegistered();

}  // namespace usgscsm

#endif  // INCLUDE_USGSCSM_PROJDBVFS_H_
