#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ORG_FILE_PATH
#define ORG_FILE_PATH "./dataset/20260401.as-org2info.txt"
#endif

#ifndef REL_FILE_S1_PATH
#define REL_FILE_S1_PATH "./dataset/20260401.as-rel.txt"
#endif

#ifndef REL_FILE_S2_PATH
#define REL_FILE_S2_PATH "./dataset/20260401.as-rel2.txt"
#endif

#ifndef REL_FILE_V6_PATH
#define REL_FILE_V6_PATH "./dataset/20260401.as-rel.v6-stable.txt"
#endif

static constexpr uint32_t MAX_ASN = 404380; //https://www.iana.org/assignments/as-numbers/as-numbers.xhtml
static constexpr size_t MAX_QUERIES_TEST = 1'000'000;
static constexpr size_t MAX_SIZE_DB = 1024ULL * 1024ULL * 512;
static constexpr uint8_t MAX_TABLE = 9;

static constexpr const char* ORG_FILE = ORG_FILE_PATH;        // as - org file
static constexpr const char* REL_FILE_S1 = REL_FILE_S1_PATH;  // serial 1 rel file
static constexpr const char* REL_FILE_S2 = REL_FILE_S2_PATH;  // serial 2 rel file
static constexpr const char* REL_FILE_V6 = REL_FILE_V6_PATH;  // v6 file - rel deduct from only ipv6 data

static constexpr const char* DB_FILE = "./AsInfo.db";
static constexpr const char* TABLE_ORG = "org_info";
static constexpr const char* TABLE_AUT = "aut_info";
static constexpr const char* TABLE_ORG_ASN = "org_to_asn";
static constexpr const char* TABLE_ASNAME_ASN ="asname_asn";
static constexpr const char* TABLE_ASN_REL = "asn_rel";
static constexpr const char* TABLE_ASN_P2C = "asn_p2c";
static constexpr const char* TABLE_ASN_CONE = "asn_cone";

// clique / tier1 AS , first line line of as-rel file
inline constexpr uint32_t TIER1_ASN[] =  
    {174, 209, 286, 701, 1239, 1299, 2828, 2914, 3257, 3320, 3356, 3491, 5511, 6453, 6461, 6762, 6830, 7018, 12956};

//IXp AS , second line of as-rel file
inline constexpr uint32_t IXP_ASN[] = {1200,  4635,  5507,  6695,  7606,  8714,  9355,  9439,  9560,  9722,  9989,  11670, 15645, 17819,
                                       18398, 21371, 24029, 24115, 24990, 35054, 40633, 42476, 43100, 47886, 48850, 50384, 55818, 57463};

inline constexpr const char* const REGIONS_STRING[] = {"Reserved","AFRINIC", "APNIC", "ARIN", "LACNIC", "RIPE", "JPNIC"};

inline constexpr const char* const TIERS_STRING[] = {"TIER-1", "TIER-2 TRANSIT", "TIER-3 STUB", "IXP", "CONTENT PROVIDER"};
