#ifndef fs_wad_c
#define fs_wad_c

/*
 * fs_wad.c - WAD Archive Backend for the fs library
 * 
 * Implements id Software's WAD (Where's All the Data?) archive format as an fs backend.
 * This allows WAD files to be accessed through the standard fs API, treating lumps as files.
 * 
 * WAD Format Overview:
 * - Header: 4-byte magic ("IWAD" or "PWAD"), lump count, FAT offset
 * - Data: Raw lump data (uncompressed)
 * - FAT (File Allocation Table): Array of (offset, size, name[8]) entries
 * 
 * Virtual Directory Support:
 * WAD files are flat (no directories), but this backend synthesizes a directory structure
 * from marker lumps (e.g., F_START/F_END for flats, S_START/S_END for sprites).
 */

#include "../../fs.h"
#include "fs_wad.h"

#include <assert.h>
#include <string.h>

#ifndef FS_WAD_COPY_MEMORY
#define FS_WAD_COPY_MEMORY(dst, src, sz) memcpy((dst), (src), (sz))
#endif

#ifndef FS_WAD_ASSERT
#define FS_WAD_ASSERT(x) assert(x)
#endif

#define FS_WAD_ABS(x)   (((x) > 0) ? (x) : -(x))

/* WAD lump names are exactly 8 bytes and may NOT be null-terminated if all 8 chars are used. */
#define FS_WAD_LUMP_NAME_LENGTH 8

/* Size needed for a null-terminated buffer to hold a lump name. */
#define FS_WAD_LUMP_NAME_BUFFER_SIZE (FS_WAD_LUMP_NAME_LENGTH + 1)

/* Safe strnlen equivalent - returns length up to maxLen, does not require null termination. */
static size_t fs_wad_strnlen( const char* pStr, size_t maxLen)
{
    size_t len = 0;
    while (len < maxLen && pStr[len] != '\0') {
        len++;
    }
    return len;
}

/*
Safely copy a lump name to a null-terminated buffer.
dst must be at least FS_WAD_LUMP_NAME_BUFFER_SIZE (9) bytes.
Returns the length of the copied string (not including null terminator).
*/
static size_t fs_wad_copy_lump_name(char* dst, const char* src)
{
    size_t len = fs_wad_strnlen(src, FS_WAD_LUMP_NAME_LENGTH);
    FS_WAD_COPY_MEMORY(dst, src, len);
    dst[len] = '\0';
    return len;
}

/* Sentinel value used to mark directory entries in the iterator (directories
   don't exist in WAD format, so they have no valid FAT index). */
#define FS_WAD_DIRECTORY_SENTINEL ((fs_int32)0xFFFFFFFF)

/* BEG fs_wad.c */

typedef struct fs_wad_fat_entry
{
    fs_int32 offset;
    fs_int32 size;
    char     name[FS_WAD_LUMP_NAME_LENGTH];
} fs_wad_fat_entry;

typedef struct fs_wad
{
    fs_int32  fileCount;
    fs_wad_fat_entry* pFAT;
} fs_wad;

/*
Doom WADs have various zero-length "markers"
that denote the beginning and end of a logical grouping of lumps. The backend will
translate these into directories and list the associated lumps within said directories.
Maps also have zero-length markers at their beginning, but these names are not predictable
as Doom source ports have evolved to support arbitrary map names. If a map is detected, its lumps
will be listed in a subdirectory of "/maps" named after the map marker.

Note: Programs that use this backend are not required to support all of the markers listed here.
*/

static const char *knownWadMarkers[] =
{
    // Flats
    "F_START",
    "F_END",
    "FF_START",
    "FF_END",
    "F1_START",
    "F1_END",
    "F2_START",
    "F2_END",
    // Sprites
    "S_START",
    "S_END",
    "SS_START",
    "SS_END",
    // Textures
    "T_START",
    "T_END",
    "TX_START",
    "TX_END",
    // High Resolution Texture Replacements
    "HI_START",
    "HI_END",
    // ACS scripts
    "A_START",
    "A_END",
    // Boom colormaps
    "C_START",
    "C_END",
    "CC_START",
    "CC_END",
    // Patches
    "P_START",
    "P_END",
    "P1_START",
    "P1_END",
    "P2_START",
    "P2_END",
    "P3_START",
    "P3_END",
    "PP_START",
    "PP_END",
    // Strife voices
    "V_START",
    "V_END",
    // Voxels
    "VX_START",
    "VX_END",
    // Sentinel
    NULL
};

/*
These are the potential lump names that will appear after a map marker, but before
the marker for the subsequent map (or before a lump not in this list, in which case
we assume that all lumps for the map in question have been idenfitifed).

Note: This covers various map formats and features that programs implementing this backend
may or may not support. 
*/

static const char *knownMapLumps[] =
{
    // ACS
    "BEHAVIOR", // Compiled bytecode
    "SCRIPTS",  // ACS scripts
    // UDMF format
    "TEXTMAP",  // Map geometry and objects
    "ENDMAP",   // Marker denoting end of map lumps for this map
    // Binary format
    "THINGS",   // Map objects
    "LINEDEFS", // Map linedefs
    "SIDEDEFS", // Map sidedefs
    "VERTEXES", // Map vertices
    "SECTORS",  // Map sectors
    "SEGS",     // Line segments (optional, created via nodebuilder)
    "SSECTORS", // Subsectors (optional, created via nodebuilder)
    "REJECT",   // Fast lookup table for line-of-sight checks (optional, created via node or reject builder)
    "BLOCKMAP", // Structure for optimization of collision checks (optional, created via nodebuilder)
    // Alternative node formats
    "GL_VERT",  // Additional vertices for GL nodes (optional, created via nodebuilder)
    "GL_SEGS",  // Line segments for GL nodes (optional, created via nodebuilder)
    "GL_SSECT", // Subsectors for GL nodes (optional, created via nodebuilder)
    "GL_NODES", // GL nodes (optional, created via nodebuilder)
    "GL_PVS",   // Potentially visible set for GL nodes (optional, created via vis builder)
    "ZNODES",   // ZDoom-specific node format(s) (optional, created via nodebuilder)
    // Strife
    "DIALOGUE",  // Strife conversation data (optional, created via map editor or dialogue compiler)
    // Doom console ports
    "LEAFS",     // Node leaves (optional, created via nodebuilder)
    "LIGHTS",    // Colored lighting information (optional, created via map editor)
    // Doom 64
    "MACROS",    // Simple script system (optional, created via map editor or macro compiler)
    // Sentinel
    NULL
};

/* Note: knownWadMarkers and knownMapLumps are reserved for future virtual directory support. */

static size_t fs_alloc_size_wad(const void* pBackendConfig)
{
    (void)pBackendConfig;
    return sizeof(fs_wad);
}

static fs_result fs_init_wad(fs* pFS, const void* pBackendConfig, fs_stream* pStream)
{
    fs_wad* pWad;
    fs_result result;
    char fourcc[4];
    fs_int32 totalFiles;
    fs_int32 fatOffset;

    /* No need for a backend config. */
    (void)pBackendConfig;

    if (pStream == NULL) {
        return FS_INVALID_OPERATION;    /* Most likely the FS is being opened without a stream. */
    }

    pWad = (fs_wad*)fs_get_backend_data(pFS);
    FS_WAD_ASSERT(pWad != NULL);

    result = fs_stream_read(pStream, fourcc, sizeof(fourcc), NULL);
    if (result != FS_SUCCESS) {
        return result;
    }

    if ((fourcc[0] != 'P' && fourcc[0] != 'I') || fourcc[1] != 'W' || fourcc[2] != 'A' || fourcc[3] != 'D') {
        return FS_INVALID_FILE;    /* Not a WAD file. */
    }

    result = fs_stream_read(pStream, &totalFiles, 4, NULL);
    if (result != FS_SUCCESS) {
        return result;
    }

    result = fs_stream_read(pStream, &fatOffset, 4, NULL);
    if (result != FS_SUCCESS) {
        return result;
    }

    /* Seek to the FAT so we can read it. */
    result = fs_stream_seek(pStream, fatOffset, FS_SEEK_SET);
    if (result != FS_SUCCESS) {
        return result;
    }

    pWad->pFAT = (fs_wad_fat_entry*)fs_malloc(totalFiles * sizeof(fs_wad_fat_entry), fs_get_allocation_callbacks(pFS));
    if (pWad->pFAT == NULL) {
        return FS_OUT_OF_MEMORY;
    }

    result = fs_stream_read(pStream, pWad->pFAT, totalFiles * sizeof(fs_wad_fat_entry), NULL);
    if (result != FS_SUCCESS) {
        fs_free(pWad->pFAT, fs_get_allocation_callbacks(pFS));
        pWad->pFAT = NULL;
        return result;
    }

    pWad->fileCount = totalFiles;

    return FS_SUCCESS;
}

static void fs_uninit_wad(fs* pFS)
{
    fs_wad* pWad = (fs_wad*)fs_get_backend_data(pFS);
    FS_WAD_ASSERT(pWad != NULL);

    fs_free(pWad->pFAT, fs_get_allocation_callbacks(pFS));
}

static fs_result fs_info_wad(fs* pFS, const char* pPath, int openMode, fs_file_info* pInfo)
{
    fs_wad* pWad;
    fs_uint32 fatIndex;

    (void)openMode;
    
    pWad = (fs_wad*)fs_get_backend_data(pFS);
    FS_WAD_ASSERT(pWad != NULL);

    for (fatIndex = 0; fatIndex < pWad->fileCount; fatIndex += 1) {
        if (fs_path_compare(pWad->pFAT[fatIndex].name, FS_WAD_LUMP_NAME_LENGTH, pPath, FS_NULL_TERMINATED) == 0) {
            pInfo->size      = pWad->pFAT[fatIndex].size;
            pInfo->directory = 0;

            return FS_SUCCESS;
        }
    }

    /* Getting here means we couldn't find a file with the given path, but it might be a folder. */
    for (fatIndex = 0; fatIndex < pWad->fileCount; fatIndex += 1) {
        if (fs_path_begins_with(pWad->pFAT[fatIndex].name, FS_WAD_LUMP_NAME_LENGTH, pPath, FS_NULL_TERMINATED)) {
            pInfo->size      = 0;
            pInfo->directory = 1;

            return FS_SUCCESS;
        }
    }

    /* Getting here means neither a file nor folder was found. */
    return FS_DOES_NOT_EXIST;
}


typedef struct fs_file_wad
{
    fs_stream* pStream;
    fs_int32 fatIndex;
    fs_uint32 cursor;
} fs_file_wad;

static size_t fs_file_alloc_size_wad(fs* pFS)
{
    (void)pFS;
    return sizeof(fs_file_wad);
}

static fs_result fs_file_open_wad(fs* pFS, fs_stream* pStream, const char* pPath, int openMode, fs_file* pFile)
{
    fs_wad* pWad;
    fs_file_wad* pWadFile;
    fs_int32 fatIndex;
    fs_result result;

    pWad = (fs_wad*)fs_get_backend_data(pFS);
    FS_WAD_ASSERT(pWad != NULL);

    pWadFile = (fs_file_wad*)fs_file_get_backend_data(pFile);
    FS_WAD_ASSERT(pWadFile != NULL);

    /* Write mode is currently unsupported. */
    if ((openMode & FS_WRITE) != 0) {
        return FS_INVALID_OPERATION;
    }

    for (fatIndex = 0; fatIndex < pWad->fileCount; fatIndex += 1) {
        if (fs_path_compare(pWad->pFAT[fatIndex].name, FS_WAD_LUMP_NAME_LENGTH, pPath, FS_NULL_TERMINATED) == 0) {
            pWadFile->fatIndex = fatIndex;
            pWadFile->cursor   = 0;
            pWadFile->pStream  = pStream;

            result = fs_stream_seek(pWadFile->pStream, pWad->pFAT[fatIndex].offset, FS_SEEK_SET);
            if (result != FS_SUCCESS) {
                return FS_INVALID_FILE;    /* Failed to seek. Archive is probably corrupt. */
            }

            return FS_SUCCESS;
        }
    }

    /* Getting here means the file was not found. */
    return FS_DOES_NOT_EXIST;
}

static void fs_file_close_wad(fs_file* pFile)
{
    /* Nothing to do. */
    (void)pFile;
}

static fs_result fs_file_read_wad(fs_file* pFile, void* pDst, size_t bytesToRead, size_t* pBytesRead)
{
    fs_file_wad* pWadFile;
    fs_wad* pWad;
    fs_result result;
    fs_uint32 bytesRemainingInFile;
    size_t bytesReadLocal;

    pWadFile = (fs_file_wad*)fs_file_get_backend_data(pFile);
    FS_WAD_ASSERT(pWadFile != NULL);

    pWad = (fs_wad*)fs_get_backend_data(fs_file_get_fs(pFile));
    FS_WAD_ASSERT(pWad != NULL);

    bytesRemainingInFile = pWad->pFAT[pWadFile->fatIndex].size - pWadFile->cursor;
    if (bytesRemainingInFile == 0) {
        return FS_AT_END;   /* No more bytes remaining. Must return AT_END. */
    }

    if (bytesToRead > bytesRemainingInFile) {
        bytesToRead = bytesRemainingInFile;
    }

    result = fs_stream_read(pWadFile->pStream, pDst, bytesToRead, &bytesReadLocal);
    if (result != FS_SUCCESS) {
        return result;
    }

    pWadFile->cursor += (fs_uint32)bytesReadLocal;
    FS_WAD_ASSERT(pWadFile->cursor <= pWad->pFAT[pWadFile->fatIndex].size);

    if (pBytesRead != NULL) {
        *pBytesRead = bytesReadLocal;
    }

    return FS_SUCCESS;
}

static fs_result fs_file_seek_wad(fs_file* pFile, fs_int64 offset, fs_seek_origin origin)
{
    fs_file_wad* pWadFile;
    fs_wad* pWad;
    fs_result result;
    fs_int64 newCursor;

    pWadFile = (fs_file_wad*)fs_file_get_backend_data(pFile);
    FS_WAD_ASSERT(pWadFile != NULL);

    pWad = (fs_wad*)fs_get_backend_data(fs_file_get_fs(pFile));
    FS_WAD_ASSERT(pWad != NULL);

    if (FS_WAD_ABS(offset) > 0xFFFFFFFF) {
        return FS_BAD_SEEK;    /* Offset is too large. */
    }

    if (origin == FS_SEEK_SET) {
        newCursor = 0;
    } else if (origin == FS_SEEK_CUR) {
        newCursor = pWadFile->cursor;
    } else if (origin == FS_SEEK_END) {
        newCursor = pWad->pFAT[pWadFile->fatIndex].size;
    } else {
        FS_WAD_ASSERT(!"Invalid seek origin.");
        return FS_INVALID_ARGS;
    }

    newCursor += offset;
    if (newCursor < 0) {
        return FS_BAD_SEEK;    /* Negative offset. */
    }
    if (newCursor > pWad->pFAT[pWadFile->fatIndex].size) {
        return FS_BAD_SEEK;    /* Offset is larger than file size. */
    }

    /* Seek to the absolute position within the WAD file (lump offset + cursor). */
    result = fs_stream_seek(pWadFile->pStream, pWad->pFAT[pWadFile->fatIndex].offset + newCursor, FS_SEEK_SET);
    if (result != FS_SUCCESS) {
        return result;
    }

    pWadFile->cursor = (fs_uint32)newCursor;    /* Safe cast. */

    return FS_SUCCESS;
}

static fs_result fs_file_tell_wad(fs_file* pFile, fs_int64* pCursor)
{
    fs_file_wad* pWadFile;

    pWadFile = (fs_file_wad*)fs_file_get_backend_data(pFile);
    FS_WAD_ASSERT(pWadFile != NULL);

    *pCursor = pWadFile->cursor;

    return FS_SUCCESS;
}

static fs_result fs_file_flush_wad(fs_file* pFile)
{
    /* Nothing to do. */
    (void)pFile;
    return FS_SUCCESS;
}

static fs_result fs_file_info_wad(fs_file* pFile, fs_file_info* pInfo)
{
    fs_file_wad* pWadFile;
    fs_wad* pWad;

    pWadFile = (fs_file_wad*)fs_file_get_backend_data(pFile);
    FS_WAD_ASSERT(pWadFile != NULL);

    pWad = (fs_wad*)fs_get_backend_data(fs_file_get_fs(pFile));
    FS_WAD_ASSERT(pWad != NULL);

    FS_WAD_ASSERT(pInfo != NULL);
    pInfo->size      = pWad->pFAT[pWadFile->fatIndex].size;
    pInfo->directory = FS_FALSE; /* An opened file should never be a directory. */
    
    return FS_SUCCESS;
}

static fs_result fs_file_duplicate_wad(fs_file* pFile, fs_file* pDuplicatedFile)
{
    fs_file_wad* pWadFile;
    fs_file_wad* pDuplicatedWadFile;

    pWadFile = (fs_file_wad*)fs_file_get_backend_data(pFile);
    FS_WAD_ASSERT(pWadFile != NULL);

    pDuplicatedWadFile = (fs_file_wad*)fs_file_get_backend_data(pDuplicatedFile);
    FS_WAD_ASSERT(pDuplicatedWadFile != NULL);

    /* We should be able to do this with a simple memcpy. */
    FS_WAD_COPY_MEMORY(pDuplicatedWadFile, pWadFile, fs_file_alloc_size_wad(fs_file_get_fs(pFile)));

    return FS_SUCCESS;
}


typedef struct fs_iterator_wad
{
    fs_iterator base;
    fs_int32  index;    /* The index of the current item. */
    fs_uint32 count;    /* The number of entries in items. */
    struct
    {
        char name[FS_WAD_LUMP_NAME_BUFFER_SIZE];  /* Always null-terminated. */
        fs_int32 fatIndex;  /* Will be set to FS_WAD_DIRECTORY_SENTINEL for directories. */
    } items[1];
} fs_iterator_wad;

static fs_bool32 fs_iterator_item_exists_wad(fs_iterator_wad* pIteratorWad, const char* pName, size_t nameLen)
{
    fs_uint32 i;

    /* Clamp nameLen to max lump name size to prevent buffer overread. */
    if (nameLen > FS_WAD_LUMP_NAME_LENGTH) {
        nameLen = FS_WAD_LUMP_NAME_LENGTH;
    }

    for (i = 0; i < pIteratorWad->count; i += 1) {
        /* items[].name is guaranteed null-terminated, check exact match. */
        size_t storedLen = fs_wad_strnlen(pIteratorWad->items[i].name, FS_WAD_LUMP_NAME_LENGTH);
        if (storedLen == nameLen && fs_strncmp(pIteratorWad->items[i].name, pName, nameLen) == 0) {
            return FS_TRUE;
        }
    }

    return FS_FALSE;
}

static void fs_iterator_resolve_wad(fs_iterator_wad* pIteratorWad)
{
    fs_wad* pWad;

    pWad = (fs_wad*)fs_get_backend_data(pIteratorWad->base.pFS);
    FS_WAD_ASSERT(pWad != NULL);

    /* items[].name is guaranteed null-terminated (FS_WAD_LUMP_NAME_BUFFER_SIZE bytes). */
    pIteratorWad->base.pName = pIteratorWad->items[pIteratorWad->index].name;
    pIteratorWad->base.nameLen = fs_wad_strnlen(pIteratorWad->base.pName, FS_WAD_LUMP_NAME_LENGTH);

    memset(&pIteratorWad->base.info, 0, sizeof(fs_file_info));
    if (pIteratorWad->items[pIteratorWad->index].fatIndex == FS_WAD_DIRECTORY_SENTINEL) {
        pIteratorWad->base.info.directory = FS_TRUE;
        pIteratorWad->base.info.size = 0;
    } else {
        pIteratorWad->base.info.directory = FS_FALSE;
        pIteratorWad->base.info.size = pWad->pFAT[pIteratorWad->items[pIteratorWad->index].fatIndex].size;
    }
}

FS_API fs_iterator* fs_first_wad(fs* pFS, const char* pDirectoryPath, size_t directoryPathLen)
{
    /*
    WAD files only list files. They do not include any explicit directory entries. We'll therefore need
    to derive folders from the list of file paths. This means we'll need to accumulate the list of
    entries in this functions.
    */
    fs_wad* pWad;
    fs_iterator_wad* pIteratorWad;
    fs_uint32 fatIndex;
    fs_uint32 itemCap = 16;

    pWad = (fs_wad*)fs_get_backend_data(pFS);
    FS_WAD_ASSERT(pWad != NULL);

    pIteratorWad = (fs_iterator_wad*)fs_calloc(sizeof(fs_iterator_wad) + (itemCap - 1) * sizeof(pIteratorWad->items[0]), fs_get_allocation_callbacks(pFS));
    if (pIteratorWad == NULL) {
        return NULL;
    }

    pIteratorWad->base.pFS = pFS;
    pIteratorWad->index = 0;
    pIteratorWad->count = 0;

    /* Skip past "/" if it was specified. */
    if (directoryPathLen > 0) {
        if (pDirectoryPath[0] == '/') {
            pDirectoryPath += 1;
            if (directoryPathLen != FS_NULL_TERMINATED) {
                directoryPathLen -= 1;
            }
        }
    }

    for (fatIndex = 0; fatIndex < pWad->fileCount; fatIndex += 1) {
        const char* pPathTail = fs_path_trim_base(pWad->pFAT[fatIndex].name, FS_WAD_LUMP_NAME_LENGTH, pDirectoryPath, directoryPathLen);
        if (pPathTail != NULL) {
            /*
            The file is contained within the directory, but it might not necessarily be appropriate to
            add this entry. We need to look at the next segments. If there is only one segment, it means
            this is a file and we can add it straight to the list. Otherwise, if there is an additional
            segment it means it's a folder, in which case we'll need to ensure there are no duplicates.
            */
            fs_path_iterator iPathSegment;
            size_t copyLen;
            
            /* Calculate remaining length in the lump name buffer from pPathTail position.
               pPathTail points somewhere within the 8-byte name field which may not be null-terminated. */
            size_t pathTailLen = FS_WAD_LUMP_NAME_LENGTH - (size_t)(pPathTail - pWad->pFAT[fatIndex].name);
            
            if (fs_path_first(pPathTail, pathTailLen, &iPathSegment) == FS_SUCCESS) {
                /*
                It's a candidate. If this item is valid for this iteration, the name will be that of the
                first segment.
                */
                if (!fs_iterator_item_exists_wad(pIteratorWad, iPathSegment.pFullPath + iPathSegment.segmentOffset, iPathSegment.segmentLength)) {
                    if (pIteratorWad->count >= itemCap) {
                        fs_iterator_wad* pNewIterator;

                        itemCap *= 2;
                        pNewIterator = (fs_iterator_wad*)fs_realloc(pIteratorWad, sizeof(fs_iterator_wad) + (itemCap - 1) * sizeof(pIteratorWad->items[0]), fs_get_allocation_callbacks(pFS));
                        if (pNewIterator == NULL) {
                            fs_free(pIteratorWad, fs_get_allocation_callbacks(pFS));
                            return NULL;    /* Out of memory. */
                        }

                        pIteratorWad = pNewIterator;
                    }

                    /* Safely copy segment name with null termination. */
                    copyLen = (iPathSegment.segmentLength <= FS_WAD_LUMP_NAME_LENGTH) 
                            ? iPathSegment.segmentLength : FS_WAD_LUMP_NAME_LENGTH;
                    FS_WAD_COPY_MEMORY(pIteratorWad->items[pIteratorWad->count].name, 
                                       iPathSegment.pFullPath + iPathSegment.segmentOffset, copyLen);
                    pIteratorWad->items[pIteratorWad->count].name[copyLen] = '\0';  /* Ensure null termination. */

                    if (fs_path_is_last(&iPathSegment)) {
                        /* It's a file. */
                        pIteratorWad->items[pIteratorWad->count].fatIndex = fatIndex;
                    } else {
                        /* It's a directory. */
                        pIteratorWad->items[pIteratorWad->count].fatIndex = FS_WAD_DIRECTORY_SENTINEL;
                    }

                    pIteratorWad->count += 1;
                }
            } else {
                /*
                pDirectoryPath is exactly equal to this item. Since WAD archives only explicitly list files
                and not directories, it means pDirectoryPath is actually a file. It is invalid to try iterating
                a file, so we need to abort.
                */
                fs_free(pIteratorWad, fs_get_allocation_callbacks(pFS));
                return NULL;
            }
        }
    }

    if (pIteratorWad->count == 0) {
        fs_free(pIteratorWad, fs_get_allocation_callbacks(pFS));
        return NULL;
    }

    pIteratorWad->index = 0;
    fs_iterator_resolve_wad(pIteratorWad);

    return (fs_iterator*)pIteratorWad;
}

FS_API fs_iterator* fs_next_wad(fs_iterator* pIterator)
{
    fs_iterator_wad* pIteratorWad = (fs_iterator_wad*)pIterator;
    
    FS_WAD_ASSERT(pIteratorWad != NULL);

    if ((fs_uint32)(pIteratorWad->index + 1) >= pIteratorWad->count) {
        fs_free(pIterator, fs_get_allocation_callbacks(pIteratorWad->base.pFS));
        return NULL;    /* No more items. */
    }

    pIteratorWad->index += 1;
    fs_iterator_resolve_wad(pIteratorWad);

    return (fs_iterator*)pIteratorWad;
}

FS_API void fs_free_iterator_wad(fs_iterator* pIterator)
{
    fs_iterator_wad* pIteratorWad = (fs_iterator_wad*)pIterator;
    FS_WAD_ASSERT(pIteratorWad != NULL);

    fs_free(pIteratorWad, fs_get_allocation_callbacks(pIteratorWad->base.pFS));
}


fs_backend fs_wad_backend =
{
    fs_alloc_size_wad,
    fs_init_wad,
    fs_uninit_wad,
    NULL,   /* remove */
    NULL,   /* rename */
    NULL,   /* mkdir */
    fs_info_wad,
    fs_file_alloc_size_wad,
    fs_file_open_wad,
    fs_file_close_wad,
    fs_file_read_wad,
    NULL,   /* write */
    fs_file_seek_wad,
    fs_file_tell_wad,
    fs_file_flush_wad,
    NULL,   /* truncate */
    fs_file_info_wad,
    fs_file_duplicate_wad,
    fs_first_wad,
    fs_next_wad,
    fs_free_iterator_wad
};
const fs_backend* FS_WAD = &fs_wad_backend;
/* END fs_wad.c */

#endif  /* fs_wad_c */
