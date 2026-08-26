#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unordered_map>
#include <vector>

#include "../dep/lmdb.h"
#include "const.h"
#include "ingest.h"
#include "types.h"

#ifdef _WIN32
    #define _CRT_SECURE_NO_WARNINGS 
    #pragma comment(lib, "advapi32.lib")
    
    #ifdef ASINFO_AS_DLL
        #ifdef ASINFO_EXPORTS
            #define ASINFO_API __declspec(dllexport)
        #else
            #define ASINFO_API __declspec(dllimport)
        #endif
    #else
        #define ASINFO_API
    #endif
#else
    
    #ifdef ASINFO_AS_DLL
        #if __GNUC__ >= 4
            #define ASINFO_API __attribute__ ((visibility ("default")))
        #else
            #define ASINFO_API
        #endif
    #else
        #define ASINFO_API
    #endif
    
#endif
/*

 https://publicdata.caida.org/datasets/as-organizations/ org info / as name and org mapping
 https://publicdata.caida.org/datasets/as-relationships/ as rel for caida inference / use serial 2

using only serial 1 or serial 2 file is enough, but for complete analysis use both and rel_v6 file,
after parsing decouple double relationship
 */


struct ASINFO_API AsInfo
{
private:
    // --- LMDB Environment & Database Handles ---
    MDB_env* env = nullptr;             // Main LMDB environment handle
    MDB_dbi dbi = 0;                   // Default database handle
    MDB_txn* txn =nullptr;              // Current active transaction handle
    
    // Individual Database Handles for different tables
    MDB_dbi org_info = 0,          // Organization metadata
            aut_info = 0,          // Autonomous System metadata
            org_to_asn = 0,        // Mapping: Organization ID -> List of ASNs
            asname_to_asn = 0,     // Mapping: AS Name -> ASN
            asn_rel = 0,           // Direct AS relationships (P2P, P2C, P2P) with double key uint64_t (a1|a2 a2|a1)
            asn_p2c = 0,           // Direct customers list (pre-computed)
            asn_cone = 0;          // Full customer cone (pre-computed BFS)

    // Cached cursors for high-performance sequential access
    MDB_cursor* m_cur_org_to_asn = nullptr;
    MDB_cursor* m_cur_aut_info = nullptr;

    // --- Private Database Building Functions ---
   // bool create();                 // Initialize LMDB environment and databases
    bool load_files();             // Parse raw CAIDA/AS-Rank text files into memory
    void ingest();                 // Process in-memory data and build relationships
    bool bulk();                   // High-speed insertion of processed data into LMDB

public:
    // --- Static Containers (Used during DB build phase) ---
    static std::vector<AsRel> asrelv;      // Global list of AS relationships
    static std::vector<AutInfo> autinfov;  // Global list of AS info records
    static std::vector<OrgInfo> orginfov;  // Global list of Organization records
    
    // Adjacency maps for graph traversal during ingestion
    static std::unordered_map<uint32_t, std::vector<uint32_t>> adj_p2c;  // Map ASN to entire direct customers list ASN
    static std::unordered_map<uint32_t, std::vector<uint32_t>> adj_cone; // Map ASN to entire cone list ASN
    static std::unordered_map<uint32_t, size_t> asn_to_idx; // ASN to autinfov index mapping

    AsInfo();
    ~AsInfo();

    // --- Core Management API ---
    bool build_db();                                                // Full pipeline: Load files -> Ingest -> Save to LMDB
    bool open(bool create = false);                                  // Open/create a LMDB database for querying
    void benchmark(size_t num_queries = MAX_QUERIES_TEST);          // Performance stress test
    void stat();                                                    // Print database statistics (entries, pages, depth)
    void testing_tables();                                           // Internal consistency and validation tests

    // --- Data Retrieval API ---
    // Returns Organization metadata by ID
    OrgInfo* get_org(const char* org_key); 
    
    // Returns Org metadata and fills vector with all its associated ASNs
    OrgInfo* get_org_full(const char* org_key, std::vector<AutInfo>& asv); 
    
    // Returns basic AS info by number
    AutInfo* get_as(uint32_t asn_key);      
    
    // Returns AS info and populates its parent Organization metadata
    AutInfo* get_as_full(uint32_t asn_key, OrgInfo& oi); 
    
    // Returns all AutInfo objects matching a specific name (handling collisions)
    std::vector<AutInfo> get_as_name(const char* asname_key);

    // --- Validation & Relationship API ---
    // Checks if an ASN belongs to a specific Organization ID
    bool check_as_org(uint32_t asn_key, const char* expected_org_id); 
    
    // Validates if a specific relationship exists between two ASNs
    RelType check_rel(uint32_t asn1, uint32_t asn2, RelType cr = ANY_REL); 

    // Returns the list of immediate customers (P2C) for a given ASN
    std::vector<uint32_t> get_customers(uint32_t asn); 
    
    // Returns the full customer cone (recursive BFS) for a given ASN
    std::vector<uint32_t> get_cone(uint32_t asn_root); 
    
    // Efficiently checks if target_asn exists within root_asn's customer cone
    bool is_in_cone(uint32_t root_asn, uint32_t target_asn); 
    
    // Identifies the RIR (Regional Internet Registry) responsible for the ASN
    Region find_rir(uint32_t asn); 
    
    // Fast O(1) or O(log N) lookup for ASN existence check
    bool fast_asn(uint32_t asn_key);

     // Fast O(1) or O(log N) lookup for Organization existence check
    bool fast_org(const char* org_key); 

    // --- Debugging Tools --- NOT IMPLEMENTED
    void test_one();               // Quick test of a single hardcoded entry
    void debug_dump_as_info_one(); // Detailed hex/text dump of an AS record

    // --- Inline Helper Methods ---
    // Returns true if any relationship exists between two ASNs
    inline bool has_rel(uint32_t asn1, uint32_t asn2)
    {
        return check_rel(asn1, asn2, ANY_REL) != NOT_REL;
    };

    // Checks if 'asnc' is a customer of 'asnp'
    inline bool is_customer(uint32_t asnc, uint32_t asnp)
    {
        return check_rel(asnp, asnc, P2C) == P2C;
    }

    // Checks if 'asnp' is a provider for 'asnc'
    inline bool is_provider(uint32_t asnp, uint32_t asnc)
    {
        return check_rel(asnp, asnc, P2C) == P2C;
    }

    // Checks if two ASNs are in a Peer-to-Peer relationship
    inline bool is_peer(uint32_t asn1, uint32_t asn2)
    {
        return check_rel(asn1, asn2, P2P) == P2P;
    }
};

