#pragma once

#include "doom/doomdef.h"
#include "fs.h"
#include "wad/w_wad.h"

/*
 * Filesystem Abstraction Layer
 * 
 * Provides a unified interface for file I/O operations, supporting both
 * external filesystem access and mounted virtual filesystems (assets, WADs).
 * 
 * The 'external' parameter in many functions controls mount behavior:
 *   - FS_TRUE:  Access the real filesystem, ignoring mounts
 *   - FS_FALSE: Access only through mounted virtual filesystems
 */

/* Initialize/shutdown the filesystem layer. Must be called before any other FS functions. */
void FS_Open(void);
void FS_Shutdown(void);

/* File operations */
fs_file *FS_OpenFile(const char *path, int mode, fs_bool32 external);
int FS_CloseFile(fs_file *file);
int FS_EOF(fs_file *file);
int FS_GetChar(fs_file *file);
int FS_PutChar(char ch, fs_file *file);
char *FS_GetString(char *str, int count, fs_file *file);
int FS_PutString(const char *str, fs_file *file);
fs_int64 FS_Tell(fs_file *file);
size_t FS_Read(void *dest, size_t size, size_t count, fs_file *file);
size_t FS_Write(const void *src, size_t size, size_t count, fs_file *file);
int FS_Print(fs_file *file, const char *fmt, ...) FORMATATTR(2, 3);
int FS_ScanLine(fs_file *file, const char *fmt, ...);
int FS_Seek(fs_file *file, fs_int64 offset, fs_seek_origin seekpoint);

/* Memory stream operations (read-only wrapper around in-memory data) */
fs_memory_stream *FS_OpenMem(const void *data, size_t length);
int FS_CloseMem(fs_memory_stream *strm);
int FS_MemEOF(fs_memory_stream *strm);
int FS_MemGetChar(fs_memory_stream *strm);
char *FS_MemGetString(char *str, int count, fs_memory_stream *strm);
fs_int64 FS_MemTell(fs_memory_stream *strm);
size_t FS_MemRead(void *dest, size_t size, size_t count, fs_memory_stream *strm);
int FS_MemSeek(fs_memory_stream *strm, fs_int64 offset, fs_seek_origin seekpoint);

/* WAD file operations (loads entire WAD into memory for fast lump access) */
fs *FS_OpenWAD(const char *path, fs_bool32 external);
int FS_CloseWAD(fs *wad);
size_t FS_WADRead(void *dest, size_t size, size_t count, fs *wad);
int FS_WADSeek(fs *wad, fs_int64 offset, fs_seek_origin seekpoint);

/* Returns a direct pointer to lump data within a loaded WAD (no copy) */
void *FS_GetRawLump(lumpinfo_t *info);

/* Utility functions */
void FS_OpenURI(const char *url, const char *description);
int FS_MakeDir(const char *path, fs_bool32 external);
int FS_GetInfo(fs_file_info *info, const char *path, fs_bool32 external);
const char *FS_GetExeFolder(void);
fs_iterator *FS_GetDirIterator(const char *path, int mode, fs_bool32 external);