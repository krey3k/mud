/*
Doom WAD file support.

This backend implements the id Software WAD (Where's All the Data?) archive format used by DOOM
and related games. It allows WAD files to be treated as virtual file systems through the fs API.

Usage:
------
    // Open a WAD file via a stream
    fs_file* pWadArchiveFile;
    fs_file_open(pFS, "archive.wad", FS_READ, &pWadArchiveFile);

    // Initialize WAD backend with the stream
    fs* pWad = NULL;
    fs_config wadConfig = fs_config_init(FS_WAD, NULL, fs_file_get_stream(pWadArchiveFile));
    fs_init(&wadConfig, &pWad);

    // Open lumps from within the WAD
    fs_file* pLump;
    fs_file_open(pWad, "PLAYPAL", FS_READ, &pLump);

    // Read lump data...
    fs_file_close(pLump);
    fs_uninit(pWad);
    fs_file_close(pWadArchiveFile);

Features:
---------
- Read-only access to WAD lumps (IWAD and PWAD formats supported)
- Virtual directory structure derived from marker lumps (F_START/F_END, S_START/S_END, etc.)
- Map lumps organized under a "/maps" virtual directory
- Standard fs_iterator support for enumerating lumps

Limitations:
------------
- Write operations are not supported
- Lump names are limited to 8 characters (WAD format limitation)
- No compression support (WAD format stores data uncompressed)

See also: FS_OpenWAD() in i_filesystem.h for a higher-level wrapper.
*/
#ifndef fs_wad_h
#define fs_wad_h

#if defined(__cplusplus)
extern "C" {
#endif

/* BEG fs_wad.h */
extern const fs_backend* FS_WAD;
/* END fs_wad.h */

#if defined(__cplusplus)
}
#endif
#endif  /* fs_wad_h */
