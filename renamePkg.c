#include "pkg2zip_sys.h"
#include "pkg2zip_utils.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define PKG_HEADER_SIZE 192
#define PKG_HEADER_EXT_SIZE 64

// http://vitadevwiki.com/vita/System_File_Object_(SFO)_(PSF)#Internal_Structure
// https://github.com/TheOfficialFloW/VitaShell/blob/1.74/sfo.h#L29
static void parse_sfo(sys_file f, uint64_t sfo_offset, uint32_t sfo_size, char* title, char* content)
{
    uint8_t sfo[16 * 1024];
    if (sfo_size < 16)
    {
        fatal("ERROR: sfo information is too small\n");
    }
    if (sfo_size > sizeof(sfo))
    {
        fatal("ERROR: sfo information is too big, pkg file is probably corrupted\n");
    }
    sys_read(f, sfo_offset, sfo, sfo_size);

    if (get32le(sfo) != 0x46535000)
    {
        fatal("ERROR: incorrect sfo signature\n");
    }

    uint32_t keys = get32le(sfo + 8);
    uint32_t values = get32le(sfo + 12);
    uint32_t count = get32le(sfo + 16);

    int title_index = -1;
    int content_index = -1;
    for (uint32_t i=0; i<count; i++)
    {
        if (i*16 + 20 + 2 > sfo_size)
        {
            fatal("ERROR: sfo information is too small\n");
        }

        char* key = (char*)sfo + keys + get16le(sfo + i*16 + 20);
        if (strcmp(key, "TITLE") == 0)
        {
            if (title_index < 0)
            {
                title_index = (int)i;
            }
        }
        else if (strcmp(key, "STITLE") == 0)
        {
            title_index = (int)i;
        }
        else if (strcmp(key, "CONTENT_ID") == 0)
        {
            content_index = (int)i;
        }
    }

    if (title_index < 0 || content_index < 0)
    {
        fatal("ERROR: sfo information doesn't have game title or content id, pkg is probably corrupted\n");
    }

    const char* value = (char*)sfo + values + get32le(sfo + title_index*16 + 20 + 12);
    size_t i;
    size_t max = 255;
    for (i=0; i<max && *value; i++, value++)
    {
        if (*value >= 32 && *value < 127 && strchr("<>\"/\\|?*", *value) == NULL)
        {
            if (*value == ':')
            {
                *title++ = ' ';
                *title++ = '-';
                max--;
            }
            else
            {
                *title++ = *value;
            }
        }
        else if (*value == 10)
        {
            *title++ = ' ';
        }
    }
    *title = 0;

    value = (char*)sfo + values + get32le(sfo + content_index * 16 + 20 + 12);
    while (*value)
    {
        *content++ = *value++;
    }
    *content = 0;
}

static const char* get_region(const char* id)
{
    if (memcmp(id, "PCSE", 4) == 0 || memcmp(id, "PCSA", 4) == 0)
    {
        return "USA";
    }
    else if (memcmp(id, "PCSF", 4) == 0 || memcmp(id, "PCSB", 4) == 0)
    {
        return "EUR";
    }
    else if (memcmp(id, "PCSC", 4) == 0 || memcmp(id, "VCJS", 4) == 0 || 
             memcmp(id, "PCSG", 4) == 0 || memcmp(id, "VLJS", 4) == 0 ||
             memcmp(id, "VLJM", 4) == 0)
    {
        return "JPN";
    }
    else if (memcmp(id, "VCAS", 4) == 0 || memcmp(id, "PCSH", 4) == 0 ||
             memcmp(id, "VLAS", 4) == 0 || memcmp(id, "PCSD", 4) == 0)
    {
        return "ASA";
    }
    else
    {
        return "unknown region";
    }
}

static void rename_pkg(char* filename, const char* title, const char* id, const char* region, int dlc)
{
    char newname[1024];
    if (dlc)
    {
        snprintf(newname, sizeof(newname), "%s [%.9s] [%s] [DLC].pkg", title, id, region);
    }
    else
    {
        snprintf(newname, sizeof(newname), "%s [%.9s] [%s].pkg", title, id, region);
    }

    sys_rename(filename, newname);
}

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        fatal("Usage: %s file.pkg \n", argv[0]);
    }

// open the pkg file
    uint64_t pkg_size;
    sys_file pkg = sys_open(argv[1], &pkg_size);

    uint8_t pkg_header[PKG_HEADER_SIZE + PKG_HEADER_EXT_SIZE];
    sys_read(pkg, 0, pkg_header, sizeof(pkg_header));

    if (get32be(pkg_header) != 0x7f504b47 || get32be(pkg_header + PKG_HEADER_SIZE) != 0x7F657874)
    {
        fatal("ERROR: not a pkg file\n");
    }

    // http://www.psdevwiki.com/ps3/PKG_files
    uint64_t meta_offset = get32be(pkg_header + 8);
    uint32_t meta_count = get32be(pkg_header + 12);
    uint32_t item_count = get32be(pkg_header + 20);
    uint64_t total_size = get64be(pkg_header + 24);
    uint64_t enc_offset = get64be(pkg_header + 32);

    if (pkg_size < total_size)
    {
        fatal("ERROR: pkg file is too small\n");
    }
    if (pkg_size < enc_offset + item_count * 32)
    {
        fatal("ERROR: pkg file is too small\n");
    }

    // search for sfo offset in pkg file and content type (app, dlc, ...)
    uint32_t content_type = 0;
    uint32_t sfo_offset = 0;
    uint32_t sfo_size = 0;

    for (uint32_t i = 0; i < meta_count; i++)
    {
        uint8_t block[16];
        sys_read(pkg, meta_offset, block, sizeof(block));

        uint32_t type = get32be(block + 0);
        uint32_t size = get32be(block + 4);

        if (type == 2)
        {
            content_type = get32be(block + 8);
        }
        else if (type == 14)
        {
            sfo_offset = get32be(block + 8);
            sfo_size = get32be(block + 12);
        }

        meta_offset += 2 * sizeof(uint32_t) + size;
    }

    // test if the pkg is a DLC
    int dlc = content_type == 0x16; // 0x15 = APP

    // get the title, id and region from the sfo file
    char content[256];
    char title[256];
    parse_sfo(pkg, sfo_offset, sfo_size, title, content);
    const char* id = content + 7;

    rename_pkg(argv[1], title, id, get_region(id), dlc);
}
