
#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <libgen.h>
#elif defined(__linux__)
#include <libgen.h>
#include <unistd.h>
#elif defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

#include <string.h>

#include "console/c_console.h"
// clang-format off
#include "system/i_filesystem.h"
#include "backends/wad/fs_wad.h"
// clang-format on
#include "system/i_system.h"
#include "doom/doomtype.h"

#define MUD_MAX_PATH 2048

static fs* file_system = NULL;
static char executable_folder[MUD_MAX_PATH] = { 0 };

static void GetExecutableFolder(void)
{
#if defined(_WIN32)
    char temp[MUD_MAX_PATH];
    
    if (GetModuleFileName(NULL, temp, MUD_MAX_PATH))
    {
        char* pos = strrchr(temp, '\\');
        
        if (pos)
        {
            *pos = '\0';
            strncpy(executable_folder, temp, MUD_MAX_PATH);
            executable_folder[MUD_MAX_PATH - 1] = '\0';
        }
    }
#elif defined(__APPLE__)
    char temp[MUD_MAX_PATH];
    uint32_t len = MUD_MAX_PATH;
    
    if (!_NSGetExecutablePath(temp, &len))
    {
        char* folder = dirname(temp);
        
        if (folder)
        {
            strncpy(executable_folder, folder, MUD_MAX_PATH);
            executable_folder[MUD_MAX_PATH - 1] = '\0';
        }
    }
#elif defined(__linux__)
    char temp[MUD_MAX_PATH];
    ssize_t len = readlink("/proc/self/exe", temp, MUD_MAX_PATH - 1);
    
    if (len != -1)
    {
        temp[len] = '\0';
        char* folder = dirname(temp);
        
        if (folder)
        {
            strncpy(executable_folder, folder, MUD_MAX_PATH);
            executable_folder[MUD_MAX_PATH - 1] = '\0';
        }
    }
#endif
    
    // Fallback to current directory if all else fails
    if (executable_folder[0] == '\0')
        strcpy(executable_folder, ".");
}

void FS_Open()
{
    if (file_system)
    {
        I_Error("FS_Open: file_system is not NULL");
    }

    if (!executable_folder[0])
    {
        GetExecutableFolder();

        if (!executable_folder[0])
        {
            I_Error("FS_Open: Unable to determine executable folder");
        }
    }

    fs_result result = fs_init(NULL, &file_system);
    
    if (result != FS_SUCCESS)
    {
        I_Error("FS_Open: fs_init failed with error code %d", result);
    }

    const char* asset_path = M_StringJoin(executable_folder, DIR_SEPARATOR_S, "assets", NULL);

    result = fs_mount(file_system, asset_path, "assets", FS_READ);

    free((void*)asset_path);

    if (result != FS_SUCCESS)
    {
        fs_uninit(file_system);
        file_system = NULL;
        I_Error("FS_Open: fs_mount failed for assets path %s%cassets", executable_folder, DIR_SEPARATOR);
    }
}

void FS_Shutdown()
{
    if (file_system)
    {
        fs_uninit(file_system);
        file_system = NULL;
    }
}

fs_file *FS_OpenFile(const char *path, int mode, fs_bool32 external)
{
    if (!path) return NULL;
    if (external)
    {
        mode &= ~FS_ONLY_MOUNTS;
        mode |= FS_IGNORE_MOUNTS;
    }
    else
    {
        mode &= ~FS_IGNORE_MOUNTS;
        mode |= FS_ONLY_MOUNTS;
    }
    fs_file *handle = NULL;
    fs_result res = fs_file_open(external ? NULL : file_system, path, mode, &handle);
    if (handle && res == FS_SUCCESS)
    {
        return handle;
    }
    else
    {
        if (handle) fs_file_close(handle);
        return NULL;
    }
}

fs_memory_stream *FS_OpenMem(const void *data, size_t length)
{
    if (!data || !length) return NULL;
    fs_memory_stream *strm = malloc(sizeof(fs_memory_stream));
    if (!strm) return NULL;
    if (fs_memory_stream_init_readonly(data, length, strm) != FS_SUCCESS)
    {
        fs_memory_stream_uninit(strm);
        free(strm);
        return NULL;
    }
    return strm;
}

fs *FS_OpenWAD(const char *path, fs_bool32 external)
{
    if (!path) return NULL;
    
    fs_file_info info;
    if (fs_info(external ? NULL : file_system, path, FS_READ, &info) != FS_SUCCESS)
        return NULL;
    if (!info.size) return NULL;
    
    fs_file *handle = NULL;
    fs_result res = fs_file_open(external ? NULL : file_system, path, FS_READ, &handle);
    if (!handle || res != FS_SUCCESS)
    {
        if (handle) fs_file_close(handle);
        return NULL;
    }
    
    // Load entire WAD into memory for fast random access to lumps.
    // Ownership of raw_wad transfers to the memory stream; it will be freed in FS_CloseWAD.
    uint8_t *raw_wad = malloc(info.size);
    if (!raw_wad)
    {
        fs_file_close(handle);
        return NULL;
    }
    
    if (FS_Read(raw_wad, info.size, 1, handle) != 1)
    {
        fs_file_close(handle);
        free(raw_wad);
        return NULL;
    }
    
    fs_memory_stream *wad_mem_stream = FS_OpenMem(raw_wad, info.size);
    if (!wad_mem_stream)
    {
        fs_file_close(handle);
        free(raw_wad);
        return NULL;
    }
    
    // File handle no longer needed; data is in memory
    fs_file_close(handle);
    
    fs *wad_stream = NULL;
    fs_config wad_config = fs_config_init(FS_WAD, NULL, (fs_stream *)wad_mem_stream);
    if (fs_init(&wad_config, &wad_stream) != FS_SUCCESS)
    {
        if (wad_stream) fs_uninit(wad_stream);
        fs_memory_stream_uninit(wad_mem_stream);
        free(wad_mem_stream);
        free(raw_wad);
        return NULL;
    }
    
    return wad_stream;
}

void *FS_GetRawLump(lumpinfo_t *info)
{
    if (!info || !info->wadfile || !info->wadfile->wad_stream) return NULL;
    fs_memory_stream *strm = (fs_memory_stream *)fs_get_stream(info->wadfile->wad_stream);
    if (!strm || !strm->ppData || !*strm->ppData || !strm->pDataSize) return NULL;
    
    // Bounds checking: ensure lump is fully contained within WAD data
    size_t wad_size = *strm->pDataSize;
    if ((size_t)info->position >= wad_size) return NULL;
    if ((size_t)info->position + (size_t)info->size > wad_size) return NULL;
    
    return (uint8_t *)*strm->ppData + info->position;
}

void FS_OpenURI(const char *url, const char *description)
{
#if defined(_WIN32)
    ShellExecute(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
#elif defined(__linux__) || defined(__APPLE__)
    #ifdef __linux__
        char *open = M_StringJoin("xdg-open ", url, NULL);
    #else
        char *open = M_StringJoin("open ", url, NULL);
    #endif
    if (open)
    {
        int status = system(open);
        if(!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS)
            C_Warning(0, "The \x1C%s\x1D wouldn't open!",description);
        free(open);
    }
    else
        C_Warning(0, "The \x1C%s\x1D wouldn't open!",description);
#elif defined(__EMSCRIPTEN__)
    EM_ASM({ window.open(UTF8ToString($0)); }, url);
#endif
}

int FS_CloseFile(fs_file *file)
{
    if (!file) return EOF;
    fs_file_close(file);
    return 0;
}

int FS_CloseMem(fs_memory_stream *strm)
{
    if (!strm) return EOF;
    fs_memory_stream_uninit(strm);
    free(strm);
    return 0;
}

int FS_CloseWAD(fs *wad)
{
    if (!wad) return EOF;
    
    fs_memory_stream *strm = (fs_memory_stream *)fs_get_stream(wad);
    if (!strm)
    {
        fs_uninit(wad);
        return EOF;
    }
    
    // The memory stream holds a pointer to the raw WAD data we allocated in FS_OpenWAD.
    // We need to free it before uninitializing the stream.
    if (strm->ppData && *strm->ppData)
    {
        free((void *)*strm->ppData);
    }
    
    fs_memory_stream_uninit(strm);
    free(strm);
    fs_uninit(wad);
    return 0;
}

int FS_EOF(fs_file *file)
{
    // likely the safest return for a null pointer so it doesn't try to keep reading
    if (!file) return EOF;
    fs_int64 pos;
    if (fs_file_tell(file, &pos) != FS_SUCCESS) return EOF;
    fs_file_info info;
    if (fs_file_get_info(file, &info) != FS_SUCCESS) return EOF;
    return (pos >= (fs_int64)info.size) ? EOF : 0;
}

int FS_MemEOF(fs_memory_stream *strm)
{
    // likely the safest return for a null pointer so it doesn't try to keep reading
    if (!strm) return EOF;
    size_t cursor;
    if (fs_memory_stream_tell(strm, &cursor) != FS_SUCCESS)
        return EOF;
    // unlike fs_file, there is no function to "get info", so we must
    // access pDataSize directly
    return (cursor >= *strm->pDataSize) ? EOF : 0;
}

int FS_GetChar(fs_file* file)
{
    if (!file) return EOF;
    byte ch;
    if(fs_file_read(file, &ch, 1, NULL) == FS_SUCCESS)
        return (int)ch;
    return EOF;
}

int FS_MemGetChar(fs_memory_stream *strm)
{
    if (!strm) return EOF;
    byte ch;
    if(fs_memory_stream_read(strm, &ch, 1, NULL) == FS_SUCCESS)
        return (int)ch;
    return EOF;
}

int FS_PutChar(char ch, fs_file *file)
{
    if (!file) return EOF;
    if (fs_file_write(file, &ch, 1, NULL) == FS_SUCCESS)
        return (int)ch;
    else
        return EOF;
}

char *FS_GetString(char* str, int count, fs_file* file)
{
    int i;
    if(!file || !str || count <= 0)
        return NULL;
    for(i = 0; i < count - 1; i++)
    {
        byte ch;
        fs_result res = fs_file_read(file, &ch, 1, NULL);
        if(res != FS_SUCCESS)
        {
            if(i == 0) return NULL;
            break;
        }
        str[i] = ch;
        if(ch == '\0')
            return str;
        if(ch == '\n')
        {
            i++;
            break;
        }
    }
    str[i] = '\0';
    return str;
}

char *FS_MemGetString(char *str, int count, fs_memory_stream *strm)
{
    int i;
    if(!strm || !str || count <= 0)
        return NULL;
    for(i = 0; i < count - 1; i++)
    {
        byte ch;
        fs_result res = fs_memory_stream_read(strm, &ch, 1, NULL);
        if(res != FS_SUCCESS)
        {
            if(i == 0) return NULL;
            break;
        }
        str[i] = ch;
        if(ch == '\0')
            return str;
        if(ch == '\n')
        {
            i++;
            break;
        }
    }
    str[i] = '\0';
    return str;
}

int FS_PutString(const char *str, fs_file *file)
{
    if (!str || !file) return EOF;
    if (fs_file_write(file, str, strlen(str), NULL) == FS_SUCCESS)
        return 0;
    else
        return EOF;
}

fs_int64 FS_Tell(fs_file *file)
{
    if (!file) return -1;
    fs_int64 cursor = 0;
    if (fs_file_tell(file, &cursor) == FS_SUCCESS)
        return cursor;
    else
        return -1;
}

fs_int64 FS_MemTell(fs_memory_stream *strm)
{
    if (!strm) return -1;
    size_t cursor = 0;
    if (fs_memory_stream_tell(strm, &cursor) == FS_SUCCESS)
        return cursor;
    else
        return -1;
}

size_t FS_Read(void *dest, size_t size, size_t count, fs_file *file)
{
    if (!dest || !file || !size || !count) return 0;
    
    size_t res = 0;
    fs_result result = fs_file_read(file, dest, size * count, &res);
    
    if (result != FS_SUCCESS && result != FS_AT_END)
        return 0;
    
    return res / size;
}

size_t FS_MemRead(void *dest, size_t size, size_t count, fs_memory_stream *strm)
{
    if (!dest || !strm || !size || !count) return 0;

    size_t res = 0;
    fs_result result = fs_memory_stream_read(strm, dest, size * count, &res);
    
    if (result != FS_SUCCESS && result != FS_AT_END)
        return 0;
    
    return res / size;
}

size_t FS_WADRead(void *dest, size_t size, size_t count, fs *wad)
{
    if (!dest || !wad || !size || !count) return 0;
    fs_memory_stream *strm = (fs_memory_stream *)fs_get_stream(wad);
    if (!strm) return 0;
    return FS_MemRead(dest, size, count, strm);
}

size_t FS_Write(const void *src, size_t size, size_t count, fs_file *file)
{
    if (!src || !size || !count || !file) return 0;
    size_t res = 0;
    fs_file_write(file, src, size * count, &res);
    return res / size;
}

int FS_Print(fs_file *file, const char *fmt, ...)
{
    if (!file || !fmt) return -1;
    va_list param_list;
    va_start(param_list, fmt);
    fs_result res = fs_file_writefv(file, fmt, param_list);
    va_end(param_list);
    return res == FS_SUCCESS ? 0 : -1;
}

// This is used for line-by-line parsing of the config
// file. Although fscanf was used previously, the complete
// behavior of fscanf is not needed and this should suffice
// as a replacement.
int FS_ScanLine(fs_file *file, const char *fmt, ...)
{
    char buff[4096];
    memset(buff, 0, 4096);
    char *buffer = FS_GetString(buff, 4096, file);

    va_list param_list;
    va_start(param_list, fmt);

    int32_t num_found = 0;
    if (buffer)
        num_found = vsscanf(buffer, fmt, param_list);

    va_end(param_list);
    return num_found;
}

int FS_Seek(fs_file *file, fs_int64 offset, fs_seek_origin seekpoint)
{
    if (!file) return -1;
    if (fs_file_seek(file, offset, seekpoint) == FS_SUCCESS)
        return 0;
    else
        return -1;
}

int FS_MemSeek(fs_memory_stream *strm, fs_int64 offset, fs_seek_origin seekpoint)
{
    if (!strm) return -1;
    if (fs_memory_stream_seek(strm, offset, seekpoint) == FS_SUCCESS)
        return 0;
    else
        return -1;
}

int FS_WADSeek(fs *wad, fs_int64 offset, fs_seek_origin seekpoint)
{
    if (!wad) return -1;
    fs_memory_stream *strm = (fs_memory_stream *)fs_get_stream(wad);
    if (!strm) return -1;
    return FS_MemSeek(strm, offset, seekpoint);
}

fs_result FS_MakeDir(const char *path, fs_bool32 external)
{
    if (!path) return FS_ERROR;
    // Not sure if we want to implicitly create whole
    // directory trees
    int options = FS_NO_CREATE_DIRS;
    if (external)
    {
        options |= FS_IGNORE_MOUNTS;
    }
    else
    {
        options |= FS_ONLY_MOUNTS;
    }
    return fs_mkdir(external ? NULL : file_system, path, options);
}

int FS_GetInfo(fs_file_info *info, const char *path, fs_bool32 external)
{
    if (!info || !path) return FS_ERROR;
    return fs_info(external ? NULL : file_system, path, FS_READ, info);
}

const char *FS_GetExeFolder(void)
{
    return executable_folder;
}

fs_iterator *FS_GetDirIterator(const char *path, int mode, fs_bool32 external)
{
    if (!path) return NULL;
    if (external)
    {
        mode &= ~FS_ONLY_MOUNTS;
        mode |= FS_IGNORE_MOUNTS;
    }
    else
    {
        mode &= ~FS_IGNORE_MOUNTS;
        mode |= FS_ONLY_MOUNTS;
    }
    return fs_first(external ? NULL : file_system, path, mode);
}