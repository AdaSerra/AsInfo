#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "const.h"

//source rir regional internet registry extended 
enum Region : uint8_t
{
    Reserved,
    AFRINIC,            // Africa
    APNIC,              // Asia - Pacific
    ARIN,               // North America
    LACNIC,             // Latin America
    RIPE_NCC,           // Europe and Middle-East
    JPNIC,              // Japan (nir)
    UNALLOCATED,
    PRIVATE,
    GLOBAL

};

//As relationship type
enum RelType : int8_t
{   
    NOT_REL = INT8_MIN,     // not relationship
    P2C = -1,               // provider to consumer
    P2P = 0,                // peer
    C2P = 1,                // consumer to provider
    ANY_REL = INT8_MAX      // any relationship
};

//As type extend
enum AsType : uint8_t
{
    TIER1 = 1,  // Global Transit Provider (No Upstreams)
    TIER2 = 2,  // Regional Transit Provider
    TIER3 = 3,  // Stub / End-user Network
    IXP = 4,    // Internet Exchange Point
    CONTENT = 5 // Content Provider
};

// As ranges numbers allocated
// https://www.iana.org/assignments/as-numbers/as-numbers.xhtml
// upper limit last update 2026-03-14
struct AsRange
{
    uint32_t upper_limit;
    Region rir;
};


// pack follow struct for lmdb 
#pragma pack(push, 1)

//Autonomous System Info
struct AutInfo
{
    uint32_t asn;           // Autonomous System identification number
    bool is_benign = true;  // True if the operator is known for secure routing practices, future use
    AsType type;            // Hierarchical classification (1: Tier-1, 2: Transit, 3: Stub)
    char org_id[24];        // Unique identifier of the organization (es. MSFT-ARIN)
    char aut_name[80];      // Friendly name registered in WHOIS/CAIDA
    uint32_t cone;          // Size of the Customer Cone (recursive customer AS)
    uint32_t degree;        // Total degree: sum of peers, providers, and customers (direct neighbors)
    uint32_t p2c_count;     // Number of direct customers (Downstream)
    uint32_t p2p_count;     // Number of peers (Settlement-free peering)
    uint32_t c2p_count;     // Number of providers (Upstream transit)
    uint32_t transit_deg;   // Sum of p2c and c2p (indicates the commercial transit role)

    void write_aut(FILE *fout) const;

    inline const char *typeToString() const
    {
        uint8_t index = static_cast<uint8_t>(type) - 1;
        if (index < 5)
        {
            return TIERS_STRING[index];
        }
        return "UNKNOWN";
    }
};

//Organitazion Info
struct OrgInfo
{
    char org_id[24];        // Organization ID (primary key for the org_info database)
    char org_name[96];      // Full name of the company/organization truncated to 96 at length (e.g., Microsoft Corporation)
    char cc[2];             // Country Code (es. "US", "IT"). 
    Region rir;             // Regional or National Registry  (e.g., RIPE, JPNIC) 
    uint32_t total_asn;     // Total number of ASNs managed    
    uint32_t total_p2p;     // Sum of all peers (p2p) of the ASNs belonging to the org
    uint32_t total_p2c;     // Sum of all direct clients (p2c) managed
    uint32_t total_c2p;     // Sum of all providers (c2p) to which the org is connected
    uint32_t total_cone;    // Size of the global customer cone at first level (direct custostomers) of org, see note on computeOrgCone function in ingest.cpp file
    uint32_t total_deg;     // Sum of all unique neighbors of all ASNs in the organization

    void write_org(FILE * fout) const;

};

#pragma pack(pop)

//As Relationship
struct AsRel
{
    uint32_t asn1;
    uint32_t asn2;
    RelType rel = NOT_REL;
    
    void write_rel(FILE *fout);

    inline bool operator==(const AsRel& other) const
    {
    return asn1 == other.asn1 && 
           asn2 == other.asn2 && 
           rel  == other.rel;
    }
};

//
inline constexpr AsRange ASN_RANGES[] = {
        {0, Reserved},         {1876, ARIN},          {1901, RIPE_NCC},   {2042, ARIN},
        {2043, RIPE_NCC},      {2046, ARIN},          {2047, RIPE_NCC},   {2106, ARIN},
        {2136, RIPE_NCC},      {2584, ARIN},          {2614, RIPE_NCC},   {2772, ARIN},
        {2822, RIPE_NCC},      {2829, ARIN},          {2879, RIPE_NCC},   {3153, ARIN},
        {3353, RIPE_NCC},      {4607, ARIN},          {4865, APNIC},      {5376, ARIN},
        {5631, RIPE_NCC},      {6655, ARIN},          {6911, RIPE_NCC},   {7466, ARIN},
        {7722, APNIC},         {8191, ARIN},          {9215, RIPE_NCC},   {10239, APNIC},
        {12287, ARIN},         {13311, RIPE_NCC},     {15359, ARIN},      {16383, RIPE_NCC},
        {17407, ARIN},         {18431, APNIC},        {20479, ARIN},      {21503, RIPE_NCC},
        {23455, ARIN},         {23456, Reserved},  // AS TRANS   1
        {23551, ARIN},         {24575, APNIC},        {25599, RIPE_NCC},  {27647, ARIN},
        {28671, LACNIC},       {29695, RIPE_NCC},     {30719, ARIN},      {31743, RIPE_NCC},
        {33791, ARIN},         {35839, RIPE_NCC},     {36863, ARIN},      {37887, AFRINIC},
        {38911, APNIC},        {39935, RIPE_NCC},     {40959, ARIN},      {45055, RIPE_NCC},
        {46079, APNIC},        {47103, ARIN},         {52223, RIPE_NCC},  {53247, LACNIC},
        {55295, ARIN},         {56319, APNIC},        {58367, RIPE_NCC},  {59391, APNIC},
        {61439, RIPE_NCC},     {61951, LACNIC},       {62463, RIPE_NCC},  {63487, ARIN},
        {64098, APNIC},        {64197, LACNIC},       {64296, ARIN},      {64395, APNIC},
        {64495, RIPE_NCC},     {64511, Reserved},     {65534, PRIVATE},   {131071, Reserved},
        {155961, APNIC},       {196607, UNALLOCATED}, {216475, RIPE_NCC}, {262143, UNALLOCATED},
        {275868, LACNIC},      {327679, UNALLOCATED}, {330751, AFRINIC},  {393215, UNALLOCATED},
        {402332, ARIN},        {403556, ARIN},        {404380, ARIN},     {4199999999, UNALLOCATED},
        {4294967294, PRIVATE}, {4294967295, Reserved}
    };

// AsRel hash
namespace std {
    template <>
    struct hash<AsRel> {
        size_t operator()(const AsRel& s) const noexcept {
            size_t h = 0;
            auto combine = [&](size_t v) {
                h ^= v + 0x9e3779b9 + (h << 6) + (h >> 2);
            };
            combine(std::hash<uint32_t>{}(s.asn1));
            combine(std::hash<uint32_t>{}(s.asn2));
            combine(std::hash<int8_t>{}(static_cast<int8_t>(s.rel)));
            return h;
        }
    };
}

/* 
struct AsStats
{
    uint32_t degree = 0;
    uint32_t customer_cone_size = 0;
}; */
