#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"

/* #define COPY(dst, src)                                               \
    do                                                               \
    {                                                                \
        size_t len = 0;                                              \
        while (src[len] && src[len] != ',' && len < sizeof(dst) - 1) \
            len++;                                                   \
        memcpy(dst, src, len);                                       \
        dst[len] = 0;                                                \
    } while (0)
 */

void extractOrgInfo(char* line, std::vector<OrgInfo>& vos);
void extractAutInfo(char* line, std::vector<AutInfo>& vas);
void extractAsRel(char* line, std::vector<AsRel>& vas);

static inline void bom_check(char** lin)
{
    unsigned char* p = (unsigned char*)(*lin);
    if (p[0] == 0xEF && p[1] == 0xBB && p[2] == 0xBF)
    {
        *lin += 3;
    }
};

static inline void copy_field(char* dst, const char* src, size_t dst_size)
{
    size_t len = 0;
    while (src[len] != '\0' /* && src[len] != ',' */ && len < dst_size - 1)
    {
        dst[len] = src[len];
        len++;
    }
    dst[len] = '\0';
};

static inline char* upper_char(char* base)
{
    if (!base) return NULL;

    for (int i = 0; base[i] != '\0'; i++)
    {
        if (base[i] >= 'a' && base[i] <= 'z')
        {
            base[i] -= 32;
        }
    }
    return base;
}

inline Region getRegion(const char* st1)
{
    for (uint8_t x = 0; x < 7; x++)
    {
        if (strcmp(st1, REGIONS_STRING[x]) == 0) return static_cast<Region>(x);
    }

    return UNALLOCATED;
}

inline const char* regionToString(Region reg)
{
    uint8_t index = static_cast<uint8_t>(reg);
    if (index < 7)
    {
        return REGIONS_STRING[index];
    }
    return "UNALLOCATED";
}
