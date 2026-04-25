#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <chrono>
#include <iostream>
#include <ostream>
#include <random>
#include <unordered_map>
#include <vector>

#include "asinfo.h"
#include "const.h"
#include "ingest.h"
#include "parstxt.h"
#include "types.h"

std::vector<AutInfo> AsInfo::autinfov;
std::vector<AsRel> AsInfo::asrelv;
std::vector<OrgInfo> AsInfo::orginfov;
std::unordered_map<uint32_t, uint64_t> AsInfo::asn_to_idx;
std::unordered_map<uint32_t, std::vector<uint32_t>> AsInfo::adj_p2c;
std::unordered_map<uint32_t, std::vector<uint32_t>> AsInfo::adj_cone;

/// @brief AsInfo constructor, init MDB 
/// @return exit programm if init not success with code 1
AsInfo::AsInfo()
{
    int rc = mdb_env_create(&env);
    if (rc != MDB_SUCCESS)
    {
        printf("Error mdb_env_create rc=%d (%s)\n", rc, mdb_strerror(rc));
        exit(1);
    }
    rc = mdb_env_set_mapsize(env, MAX_SIZE_DB);
    if (rc != MDB_SUCCESS)
    {
        printf("Error mdb_set_mapsize rc=%d (%s)\n", rc, mdb_strerror(rc));
        exit(1);
    }
    rc = mdb_env_set_maxdbs(env, MAX_TABLE);
    if (rc != MDB_SUCCESS)
    {
        printf("Error mdb_env_set_maxdb rc=%d (%s)\n", rc, mdb_strerror(rc));
        exit(1);
    }
}
/// @brief Decostructor AsInfo, close MDB and null the pointers
AsInfo::~AsInfo()
{
    if (m_cur_org_to_asn)
    {
        mdb_cursor_close(m_cur_org_to_asn);
        m_cur_org_to_asn = nullptr;
    }
    if (m_cur_aut_info)
    {
        mdb_cursor_close(m_cur_aut_info);
        m_cur_aut_info = nullptr;
    }
    if (txn)
    {
        mdb_txn_abort(txn);
        txn = nullptr;
    };
    if (env) mdb_env_close(env);
}

/// @brief Create/Open MDB database, tables, and init txn pointer
/// @param create flag to open db in read only mode or not
/// @return false if not success
bool AsInfo::open(bool create)
{   
    int rc;

    if (create) 
        rc = mdb_env_open(env, DB_FILE, MDB_NOSUBDIR, 0664);
    else 
        rc = mdb_env_open(env, DB_FILE, MDB_NOSUBDIR | MDB_RDONLY, 0664);

    if (rc)
    {
        printf("Error mdb_env_open rc=%d (%s)\n", rc, mdb_strerror(rc));
        return false;
    }

    if(create) 
        rc = mdb_txn_begin(env, NULL, 0, &txn);
    else 
        rc = mdb_txn_begin(env, NULL, MDB_RDONLY, &txn);
        
    if (rc != MDB_SUCCESS)
    {
        printf("Error mdb_txn_begin rc=%d (%s)\n", rc, mdb_strerror(rc));
        return false;
    }
   
    if (mdb_dbi_open(txn, TABLE_ORG, MDB_CREATE, &org_info) ||
        mdb_dbi_open(txn, TABLE_AUT, MDB_CREATE | MDB_INTEGERKEY, &aut_info) ||
        mdb_dbi_open(txn, TABLE_ORG_ASN, MDB_CREATE | MDB_DUPSORT, &org_to_asn) ||
        mdb_dbi_open(txn, TABLE_ASNAME_ASN, MDB_CREATE | MDB_DUPSORT, &asname_to_asn) ||
        mdb_dbi_open(txn, TABLE_ASN_REL, MDB_CREATE | MDB_INTEGERKEY, &asn_rel) ||
        mdb_dbi_open(txn, TABLE_ASN_P2C, MDB_CREATE | MDB_INTEGERKEY, &asn_p2c) ||
        mdb_dbi_open(txn, TABLE_ASN_CONE, MDB_CREATE | MDB_INTEGERKEY, &asn_cone))
    {
        return false;
    }

    return true /* safe_txn.commit() */;
}

/// @brief Opes MDB database in reading mode, tables, and init txn pointer 
/// @return false if not success
/* bool AsInfo::open()
{
    int rc = mdb_env_open(env, DB_FILE, MDB_NOSUBDIR | MDB_RDONLY, 0664);
    if (rc)
    {
        printf("Error mdb_env_open rc=%d (%s)\n", rc, mdb_strerror(rc));
        return false;
    }

    rc = mdb_txn_begin(env, NULL, MDB_RDONLY, &txn);
    if (rc)
    {
        printf("Error mdb_txn_begin rc=%d (%s)\n", rc, mdb_strerror(rc));
        return false;
    }

    // same flag used in create function
    mdb_dbi_open(txn, TABLE_ORG, 0, &org_info);
    mdb_dbi_open(txn, TABLE_AUT, MDB_INTEGERKEY, &aut_info);
    mdb_dbi_open(txn, TABLE_ORG_ASN, MDB_DUPSORT, &org_to_asn);
    mdb_dbi_open(txn, TABLE_ASNAME_ASN, MDB_DUPSORT, &asname_to_asn);
    mdb_dbi_open(txn, TABLE_ASN_REL, MDB_INTEGERKEY, &asn_rel);
    mdb_dbi_open(txn, TABLE_ASN_P2C, MDB_INTEGERKEY, &asn_p2c);
    mdb_dbi_open(txn, TABLE_ASN_CONE, MDB_INTEGERKEY, &asn_cone);

    //  mdb_txn_abort(txn);

    return (aut_info != 0);
} */

/// @brief Build a new database calling functions for every fase of the process
/// @return false if not success
bool AsInfo::build_db()
{
    if (!this->open(true)) return false;

    if (!this->load_files()) return false;

    this->ingest();

    if (!this->bulk()) return false;

    this->stat();

    return true;
}

/// @brief Load Caida files and call parsing functions to extract data in static containers of the struct. 
/// @return false if not success
bool AsInfo::load_files()
{
    char riga[1024];

    FILE* fp = fopen(ORG_FILE, "r");
    if (!fp)
    {
        printf("Error: impossible opening file as_org2info.txt\n");
        return false;
    }

    autinfov.reserve(125000);
    orginfov.reserve(100000);

    if (fgets(riga, sizeof(riga), fp) == NULL)
    {
        char* ptr = riga;
        bom_check(&ptr);  // BOM
    }

    bool is_second_part = false;
    while (fgets(riga, sizeof(riga), fp))
    {
        riga[strcspn(riga, "\r\n")] = 0;

        // check change section
        if (strstr(riga, "aut") && strstr(riga, "changed"))
        {
            is_second_part = true;
        }

        if (riga[0] == '\0' || riga[0] == '#') continue;

        //
        if (!is_second_part)
        {
            extractOrgInfo(riga, orginfov);
        }
        else
        {
            extractAutInfo(riga, autinfov);
        }
    }

    fclose(fp);

    fp = fopen(REL_FILE_S1, "r");

    if (!fp)
    {
        printf("Error: Impossible open file as-rel.\n");
        return false;
    }

    asrelv.reserve(700000);

    if (fgets(riga, sizeof(riga), fp) == NULL)
    {
        char* ptr = riga;
        bom_check(&ptr);  // remove bom
    }

    while (fgets(riga, 1024, fp))
    {
        extractAsRel(riga, asrelv);
    }

    fclose(fp);

    fp = fopen(REL_FILE_S2, "r");

    if (fp)
    {
        if (fgets(riga, sizeof(riga), fp) == NULL)
        {
            char* ptr = riga;
            bom_check(&ptr);
        }

        while (fgets(riga, 1024, fp))
        {
            extractAsRel(riga, asrelv);
        }

        fclose(fp);
    }

    else
    {
        printf(
            "Warning: Impossible open file as-rel2 - serial 2\n Using only as-rel - serial 1 file\n");
    }

    fp = fopen(REL_FILE_V6, "r");
     
    if (fp)
    {
        if (fgets(riga, sizeof(riga), fp) == NULL)
        {
            char* ptr = riga;
            bom_check(&ptr);
        }

        while (fgets(riga, 1024, fp))
        {
            extractAsRel(riga, asrelv);
        }

        fclose(fp);
    }

    else
    {
        printf(
            "Warning: Impossible open file as-relv6 file\n Relationship v6 not will inlcude in database");
    }

    // sorting and decupling as relationship
    if (!asrelv.empty())
    {
        std::sort(asrelv.begin(), asrelv.end(),
                  [](const AsRel& a, const AsRel& b)
                  {
                      if (a.asn1 != b.asn1) return a.asn1 < b.asn1;
                      if (a.asn2 != b.asn2) return a.asn2 < b.asn2;
                      return a.rel < b.rel;
                  });

        auto last = std::unique(asrelv.begin(), asrelv.end(), [](const AsRel& a, const AsRel& b)
                                { return a.asn1 == b.asn1 && a.asn2 == b.asn2 && a.rel == b.rel; });

        asrelv.erase(last, asrelv.end());
    }

    if (!asrelv.empty() && !autinfov.empty() && !orginfov.empty())
        return true;
    else
        return false;
}

/// @brief Process data extract from Caida files calling functions to ingest, finally sort AutIinfo vector and single vectors in maps
/// @details Size_t counters are for debugging and optimizing structs, they can be deleted
void AsInfo::ingest()
{
    std::cout << "Starting prepocess: calculating As Cone and As Relationship...\n";
    std::cout << "Total Aut: " << autinfov.size() << "\nTotal Org: " << orginfov.size()
              << "\nTotal Rel: " << asrelv.size();
    analyze_relationships(asrelv, autinfov, asn_to_idx, adj_p2c);
    compute_as_metrics(autinfov, adj_p2c, asn_to_idx, adj_cone);
    aggregate_organizations(orginfov, autinfov, adj_p2c);

    std::sort(autinfov.begin(), autinfov.end(),
              [](const AutInfo& a, const AutInfo& b) { return a.asn < b.asn; });

    size_t total_p2c = 0;
    size_t total_cone = 0;
    for (auto& [a, v] : adj_p2c)
    {
        std::sort(v.begin(), v.end(),
                  [](const uint32_t& a1, const uint32_t a2) { return a1 < a2; });
        total_p2c += v.size();
    }

    for (auto& [a, v] : adj_cone)
    {
        std::sort(v.begin(), v.end(),
                  [](const uint32_t& a1, const uint32_t a2) { return a1 < a2; });
        total_cone += v.size();
    }

    size_t total_autname = 0;
    size_t total_orgname = 0;

    for (const auto& x : autinfov)
    {
        total_autname += strlen(x.aut_name);
    }

    for (const auto& x : orginfov)
    {
        total_orgname += strlen(x.org_name);
    }

    std::cout << "\nComplete, insert data in Database...\n";

    std::cout << "Average P2C size: " << (total_p2c / adj_p2c.size());
    std::cout << "\nAverage Cone size: " << (total_cone / adj_cone.size()) << "\n";

    std::cout << "\nAverage Aut Name len: " << (total_autname / autinfov.size()) << "\n";
    std::cout << "\nAverage Org Name len: " << (total_orgname / orginfov.size()) << "\n";
}

/// @brief Write data in Database
/// @details - Cycle on OrgInfo vector and for i elem put in org_info table: key org_id (char), data: OrgInfo raw
///          - Cycle on AutInfo vector and for i elem put in aut_info table: key as number (uint32_t), data AutInfo raw 
///                                                       in org_to_asn table: key org_id (char), data as number  
///                                                       in asname_to_asn table: aut_name(char), data as number 
///          - Cycle on AsRel vector and for i elem put in asn_rel: key (uint64_t) asn1|asn2, data: relationship [-1,0,1]
///                                                                 inverse key asn2|asn1, data: inverse relationship
///          - Cycle on adj_p2c and put in asn_p2c table: key asn(uint32_t), data, blob of asn(uint32_t) in associated vector
///          - Cycle on adj_cone and put in asn_cone table: key asn(uint32_t), data, blob of asn(uint32_t) in associated vector
/// @return commit transaction return value 
bool AsInfo::bulk()
{
    /*  MdbTxn safe_txn(env, false);
     if (!safe_txn.txn) return false;
  */
    // orginfo table
    MDB_val key, data;
    int rc;
    for (size_t i = 0; i < orginfov.size(); i++)
    {
        key.mv_size = strlen(orginfov[i].org_id);
        key.mv_data = orginfov[i].org_id;
        data.mv_size = sizeof(OrgInfo);
        data.mv_data = nullptr;

        rc = mdb_put(txn, org_info, &key, &data, MDB_RESERVE);
        if (rc == 0)
            memcpy(data.mv_data, &orginfov[i], sizeof(OrgInfo));

        else
        {
            std::cerr << "Error mdb_put org info table at record  " << i << ":" << mdb_strerror(rc)
                      << "\n";
            break;
        }
    }
    // autoinfo table
    for (size_t i = 0; i < autinfov.size(); i++)
    {
        key.mv_size = sizeof(uint32_t);
        key.mv_data = &autinfov[i].asn;
        data.mv_size = sizeof(AutInfo);
        data.mv_data = nullptr;

        rc = mdb_put(txn, aut_info, &key, &data, MDB_RESERVE);
        if (rc == 0)
            memcpy(data.mv_data, &autinfov[i], sizeof(AutInfo));

        else
        {
            std::cerr << "Error mdb_put aut_info table at record " << i << ":" << mdb_strerror(rc)
                      << "\n";
            break;
        }

        // org_to_asn table mapping org_id to as number
        key.mv_size = strlen(autinfov[i].org_id);
        key.mv_data = autinfov[i].org_id;
        data.mv_size = sizeof(uint32_t);
        data.mv_data = &autinfov[i].asn;
        rc = mdb_put(txn, org_to_asn, &key, &data, 0);
        if (rc != 0)

        {
            std::cerr << "Error mdb_put org_to_as table at record " << i << ":" << mdb_strerror(rc)
                      << "\n";
            break;
        }

        // asname_to asn table mapping as name to as number
        key.mv_size = strlen(autinfov[i].aut_name);
        if (key.mv_size == 0) continue;
        key.mv_data = autinfov[i].aut_name;
        data.mv_size = sizeof(uint32_t);
        data.mv_data = &autinfov[i].asn;

        rc = mdb_put(txn, asname_to_asn, &key, &data, 0);
        if (rc != 0)

        {
            std::cerr << "Error mdb_put asname_to_asn table at record " << i << ":"
                      << mdb_strerror(rc) << "\n";
            break;
        }
    }

    // asn relationship table
    for (size_t i = 0; i < asrelv.size(); i++)
    {
        key.mv_size = sizeof(uint64_t);
        data.mv_size = sizeof(RelType);
        uint64_t raw_key = (static_cast<uint64_t>(asrelv[i].asn1) << 32) | asrelv[i].asn2;

        key.mv_data = &raw_key;
        data.mv_data = &asrelv[i].rel;
        rc = mdb_put(txn, asn_rel, &key, &data, 0);
        if (rc != 0)
        {
            std::cerr << "Error mdb_put asn_rel table at record (direct key insert)" << i << ":"
                      << mdb_strerror(rc) << "\n";
            break;
        }

        // inversion relationship
        uint64_t inv_key = (static_cast<uint64_t>(asrelv[i].asn2) << 32) | asrelv[i].asn1;
        RelType inv_rel = asrelv[i].rel;

        if (asrelv[i].rel == P2C)
            inv_rel = C2P;
        else if (asrelv[i].rel == C2P)
            inv_rel = P2C;

        key.mv_data = &inv_key;
        data.mv_data = &inv_rel;

        rc = mdb_put(txn, asn_rel, &key, &data, 0);
        if (rc != 0)
        {
            std::cerr << "Error mdb_put asn_rel table at record (invers key insert)" << i << ":"
                      << mdb_strerror(rc) << "\n";
            break;
        }
    }

    // asn p2c table - direct customers
    for (const auto& [asn_key, as_vector] : adj_p2c)
    {
        if (as_vector.empty()) continue;

        uint32_t current_asn = asn_key;
        key.mv_size = sizeof(uint32_t);
        key.mv_data = &current_asn;

        data.mv_size = sizeof(uint32_t) * as_vector.size();
        data.mv_data = nullptr;
        rc = mdb_put(txn, asn_p2c, &key, &data, MDB_RESERVE);
        if (rc == 0)
        {
            memcpy(data.mv_data, as_vector.data(), data.mv_size);
        }
        else
        {
            std::cerr << "Error mdb_put asn_p2c: " << mdb_strerror(rc) << "\n";
            break;
        }
    }
    // asn cone table
    for (const auto& [asn_key, as_vector] : adj_cone)
    {
        if (as_vector.empty()) continue;

        uint32_t current_asn = asn_key;
        key.mv_size = sizeof(uint32_t);
        key.mv_data = &current_asn;

        data.mv_size = sizeof(uint32_t) * as_vector.size();
        data.mv_data = nullptr;
        rc = mdb_put(txn, asn_cone, &key, &data, MDB_RESERVE);
        if (rc == 0)
        {
            memcpy(data.mv_data, as_vector.data(), data.mv_size);
        }
        else
        {
            std::cerr << "Error mdb_put asn_cone: " << mdb_strerror(rc) << "\n";
            break;
        }
    }

    rc = mdb_txn_commit(txn);
    if (rc == MDB_SUCCESS)
        return true;
    else
    {
        //  std::cerr << "Error commit bulk: " << mdb_strerror(rc) << "\n";
        return false;
    }
}

/// @brief Show Statistics info for every table
void AsInfo::stat()
{
    MDB_stat stat;
    // MdbTxn safe_txn(env, true);
    int rc = mdb_txn_begin(env, NULL, MDB_RDONLY, &txn);
    rc = mdb_stat(txn, org_info, &stat);
    double avg_size = 0.00f;

    if (rc == MDB_SUCCESS)
    {
        avg_size = (double)(stat.ms_leaf_pages * stat.ms_psize) / stat.ms_entries;
        printf("\nOrganization Info Table\n");
        printf("B-Tree depth: %u\n", stat.ms_depth);
        printf("Total entry: %llu\n", (unsigned long long)stat.ms_entries);
        printf("Average size per entry: %.2f bytes\n", avg_size);
        printf("Page Branch: %llu\n", (unsigned long long)stat.ms_branch_pages);
        printf("Page Leaf: %llu\n", (unsigned long long)stat.ms_leaf_pages);
        printf("Page Overflow: %llu\n", (unsigned long long)stat.ms_overflow_pages);
    }

    rc = mdb_stat(txn, aut_info, &stat);

    if (rc == MDB_SUCCESS)
    {
        avg_size = (double)(stat.ms_leaf_pages * stat.ms_psize) / stat.ms_entries;
        printf("\nAutonomous System Info Table\n");
        printf("B-Tree depth: %u\n", stat.ms_depth);
        printf("Total entry: %llu\n", (unsigned long long)stat.ms_entries);
        printf("Average size per entry: %.2f bytes\n", avg_size);
        printf("Page Branch: %llu\n", (unsigned long long)stat.ms_branch_pages);
        printf("Page Leaf: %llu\n", (unsigned long long)stat.ms_leaf_pages);
        printf("Page Overflow: %llu\n", (unsigned long long)stat.ms_overflow_pages);
    }

    rc = mdb_stat(txn, asn_rel, &stat);

    if (rc == MDB_SUCCESS)
    {
        avg_size = (double)(stat.ms_leaf_pages * stat.ms_psize) / stat.ms_entries;
        printf("\nAutonomous System Relationship Table\n");
        printf("B-Tree depth: %u\n", stat.ms_depth);
        printf("Total entry: %llu\n", (unsigned long long)stat.ms_entries);
        printf("Average size per entry: %.2f bytes\n", avg_size);
        printf("Page Branch: %llu\n", (unsigned long long)stat.ms_branch_pages);
        printf("Page Leaf: %llu\n", (unsigned long long)stat.ms_leaf_pages);
    }

    rc = mdb_stat(txn, asn_p2c, &stat);

    if (rc == MDB_SUCCESS)
    {
        avg_size = (double)(stat.ms_leaf_pages * stat.ms_psize) / stat.ms_entries;
        printf("\nAutonomous System Direct Customers (P2C) Table\n");
        printf("B-Tree depth: %u\n", stat.ms_depth);
        printf("Total entry: %llu\n", (unsigned long long)stat.ms_entries);
        printf("Average size per entry: %.2f bytes\n", avg_size);
        printf("Page Branch: %llu\n", (unsigned long long)stat.ms_branch_pages);
        printf("Page Leaf: %llu\n", (unsigned long long)stat.ms_leaf_pages);
        printf("Page Overflow: %llu\n", (unsigned long long)stat.ms_overflow_pages);
    }

    rc = mdb_stat(txn, asn_cone, &stat);

    if (rc == MDB_SUCCESS)
    {
        avg_size = (double)(stat.ms_leaf_pages * stat.ms_psize) / stat.ms_entries;
        printf("\nAutonomous System Cone (BFS recursive) Table\n");
        printf("B-Tree depth: %u\n", stat.ms_depth);
        printf("Total entry: %llu\n", (unsigned long long)stat.ms_entries);
        printf("Average size per entry: %.2f bytes\n", avg_size);
        printf("Page Branch: %llu\n", (unsigned long long)stat.ms_branch_pages);
        printf("Page Leaf: %llu\n", (unsigned long long)stat.ms_leaf_pages);
        printf("Page Overflow: %llu\n", (unsigned long long)stat.ms_overflow_pages);
    }

    rc = mdb_stat(txn, org_to_asn, &stat);

    if (rc == MDB_SUCCESS)
    {
        avg_size = (double)(stat.ms_leaf_pages * stat.ms_psize) / stat.ms_entries;
        printf("\nOrganization Id To Autonomous System Name Table\n");
        printf("B-Tree depth: %u\n", stat.ms_depth);
        printf("Total entry: %llu\n", (unsigned long long)stat.ms_entries);
        printf("Average size per entry: %.2f bytes\n", avg_size);
        printf("Page Branch: %llu\n", (unsigned long long)stat.ms_branch_pages);
        printf("Page Leaf: %llu\n", (unsigned long long)stat.ms_leaf_pages);
    }

    rc = mdb_stat(txn, asname_to_asn, &stat);
    if (rc == MDB_SUCCESS)
    {
        avg_size = (double)(stat.ms_leaf_pages * stat.ms_psize) / stat.ms_entries;
        printf("\nAutonomous System Name To Autonomous System Number Table\n");
        printf("B-Tree depth: %u\n", stat.ms_depth);
        printf("Total entry: %llu\n", (unsigned long long)stat.ms_entries);
        printf("Average size per entry: %.2f bytes\n", avg_size);
        printf("Page Branch: %llu\n", (unsigned long long)stat.ms_branch_pages);
        printf("Page Leaf: %llu\n", (unsigned long long)stat.ms_leaf_pages);
    }

    return;
}

/// @brief Lookup an as number in aut_info table
/// @param asn_key as number
/// @return if success return pointer to AutInfo struct, else nullptr
AutInfo* AsInfo::get_as(uint32_t asn_key)
{
    MDB_val key, data;
    key.mv_size = sizeof(uint32_t);
    key.mv_data = &asn_key;

    int rc = mdb_get(txn, aut_info, &key, &data);

    if (rc == MDB_NOTFOUND) return nullptr;

    if (rc == 0)
    {
        AutInfo* res = static_cast<AutInfo*>(data.mv_data);
        if (res) return res;
    }
    
    return nullptr;
}

/// @brief Lookup an as number in aut_info table and join org_id to lookup it org_table
/// @param asn_key as number
/// @param oi reference to OrgInfo object to store relative data
/// @return if success return pointer to AutInfo struct, else nullptr
AutInfo* AsInfo::get_as_full(uint32_t asn_key, OrgInfo& oi)
{
    MDB_val key, data;
    key.mv_size = sizeof(uint32_t);
    key.mv_data = &asn_key;

    if (mdb_get(txn, aut_info, &key, &data) != 0) return nullptr;

    AutInfo* res = static_cast<AutInfo*>(data.mv_data);
    if (!res) return nullptr;

    MDB_val org_key;
    org_key.mv_size = strlen(res->org_id);
    org_key.mv_data = res->org_id;

    if (mdb_get(txn, org_info, &org_key, &data) == 0)
    {
        OrgInfo* res2 = static_cast<OrgInfo*>(data.mv_data);
        if (res2)
        {
            memcpy(&oi, res2, sizeof(OrgInfo));
        }
    }

    return res;
}

/// @brief Performs a prefix search in aut_info table
/// @param asname_key as name to use as key
/// @return if success return a list of associated AutInfo objects with the keys found.
std::vector<AutInfo> AsInfo::get_as_name(const char* asname_key)
{
    std::vector<AutInfo> results{};
    MDB_val key, data;
    size_t key_len = strlen(asname_key);
    key.mv_size = key_len;
    key.mv_data = (void*)asname_key;

    MDB_cursor* cursor;

    if (mdb_cursor_open(txn, asname_to_asn, &cursor) != 0)
    {
        return results;
    }

    int rc = mdb_cursor_get(cursor, &key, &data, MDB_SET_RANGE);

    while (rc == 0)
    {
        // check prefix
        if (/* key.mv_size < key_len ||  */ memcmp(key.mv_data, asname_key, key_len) != 0)
        {
            break;
        }
        uint32_t found_asn = *(uint32_t*)data.mv_data;
        MDB_val key_asn, data_asn;
        key_asn.mv_size = sizeof(uint32_t);
        key_asn.mv_data = &found_asn;
        if (mdb_get(txn, aut_info, &key_asn, &data_asn) == 0)
        {
            AutInfo* as_details = static_cast<AutInfo*>(data_asn.mv_data);
            results.push_back(*as_details);
        }
        rc = mdb_cursor_get(cursor, &key, &data, MDB_NEXT);
    }
    mdb_cursor_close(cursor);
    return results;
}

/// @brief Lookup an org_id in org_info table
/// @param org_key string to use as org_id key
/// @return if success return a pointer to OrgInfo struct associated, else nullptr
OrgInfo* AsInfo::get_org(const char* org_key)
{
    MDB_val key, data;
    key.mv_size = strlen(org_key);
    key.mv_data = (void*)org_key;

    int rc = mdb_get(txn, org_info, &key, &data);
    if (rc == MDB_NOTFOUND) return nullptr;

    if (rc == 0)
    {
        OrgInfo* res = static_cast<OrgInfo*>(data.mv_data);
        if (res) return res;
    }

    return nullptr;
}

/// @brief Lookup an org_id in org_info table and join org_id to lookup all as with the same org_id
/// @param org_key string to use as org_id key
/// @param asv vector to store all AutInfo objects found
/// @return return a pointer to OrgInfo struct associated, else nullptr 
OrgInfo* AsInfo::get_org_full(const char* org_key, std::vector<AutInfo>& asv)
{
    MDB_val key, data;
    key.mv_size = strlen(org_key);
    key.mv_data = (void*)org_key;

    if (mdb_get(txn, org_info, &key, &data) != 0) return nullptr;

    OrgInfo* res = static_cast<OrgInfo*>(data.mv_data);

    MDB_val v_mapping;
    if (mdb_get(txn, org_to_asn, &key, &v_mapping) == 0)
    {
        MDB_val key_asn, data_asn;
        key_asn.mv_size = sizeof(uint32_t);
        key_asn.mv_data = v_mapping.mv_data;

        if (!m_cur_aut_info) mdb_cursor_open(txn, aut_info, &m_cur_aut_info);

        if (mdb_cursor_get(m_cur_aut_info, &key_asn, &data_asn, MDB_SET_KEY) == 0)
        {
            asv.push_back(*static_cast<AutInfo*>(data_asn.mv_data));
        }
    }

    return res;
}

bool AsInfo::fast_asn(uint32_t asn_key)
{
    MDB_val key, data;

    key.mv_size = sizeof(uint32_t);
    key.mv_data = &asn_key;

    OrgInfo* res2 = nullptr;

    int rc = mdb_get(txn, aut_info, &key, &data);

    if (rc == 0)
    {
        return true;
    }
    return false;
}

bool AsInfo::fast_org(const char* org_key)
{
    MDB_val key, data;
    key.mv_size = strlen(org_key);
    key.mv_data = (void*)org_key;

    int rc = mdb_get(txn, org_info, &key, &data);

    if (rc == 0)
    {
        return true;
    }

    return false;
}

/// @brief Checks if an AS number belongs to a specific Organization Id
/// @param asn_key AS number to verify
/// @param expected_org_id org_id expected
/// @return result of comparing expected_org_id with org_id or false if as number not exist in db
bool AsInfo::check_as_org(uint32_t asn_key, const char* expected_org_id)
{
    MDB_val key, data;
    key.mv_size = sizeof(uint32_t);
    key.mv_data = &asn_key;

    // find asn
    int rc = mdb_get(txn, aut_info, &key, &data);
    if (rc != 0) return false;

    AutInfo* res = static_cast<AutInfo*>(data.mv_data);

    // compare org_id expected
    if (res && strcmp(res->org_id, expected_org_id) == 0)
    {
        return true;
    }

    return false;
}

/// @brief Validates if a specific relationship exists between two ASNs
/// @param asn1 first AS number
/// @param asn2 second AS number
/// @param cr Relationship type to verify, if not given, default value is ANY, so in this case check only if exist any kind of relationship
/// @return the result compare relationship given as cr and relationship in database
RelType AsInfo::check_rel(uint32_t asn1, uint32_t asn2, RelType cr)
{
    MDB_val key, data;

    key.mv_size = sizeof(uint64_t);
    uint64_t asn_key = (static_cast<uint64_t>(asn1) << 32) | asn2;
    key.mv_data = &asn_key;

    int rc = mdb_get(txn, asn_rel, &key, &data);
    if (rc != 0) return NOT_REL;

    RelType actual_rel = *static_cast<RelType*>(data.mv_data);

    if (cr == ANY_REL) return ANY_REL;

    return (cr == actual_rel) ? cr : NOT_REL;
}

/// @brief Lookup an AS number in asn_cone table and if exists return list of AS in full customer cone (recursive BFS)
/// @details BFS is precalculated 
/// @param asn AS number to lookup
/// @return vector with all AS number of customer cone or an empty vector
std::vector<uint32_t> AsInfo::get_cone(uint32_t asn)
{
    MDB_val key, data;
    key.mv_size = sizeof(uint32_t);
    key.mv_data = &asn;

    int rc = mdb_get(txn, asn_cone, &key, &data);

    if (rc == MDB_SUCCESS)
    {
        const uint32_t* begin = static_cast<const uint32_t*>(data.mv_data);
        const uint32_t* end = begin + (data.mv_size / sizeof(uint32_t));
        return std::vector<uint32_t>(begin, end);
    }

    return {};
}

/// @brief Lookup an AS number in asn_p2c table and if exists return list of AS direct customers
/// @param asn AS number to lookup
/// @return vector with all AS direct customers or an empty vector
std::vector<uint32_t> AsInfo::get_customers(uint32_t asn)
{
    MDB_val key, data;
    key.mv_size = sizeof(uint32_t);
    key.mv_data = &asn;

    int rc = mdb_get(txn, asn_p2c, &key, &data);

    if (rc == MDB_SUCCESS)
    {
        const uint32_t* begin = static_cast<const uint32_t*>(data.mv_data);
        const uint32_t* end = begin + (data.mv_size / sizeof(uint32_t));
        return std::vector<uint32_t>(begin, end);
    }

    return {};
}

/// @brief Check if an given AS is in customer cone of another AS
/// @details Takes entire customer cone of the other AS in table asn_cone and performs a binary search on it
/// @param root_asn as number owner of cone
/// @param target_asn as number to check
/// @return boolean value of that check 
bool AsInfo::is_in_cone(uint32_t root_asn, uint32_t target_asn)
{
    MDB_val key, data;
    key.mv_size = sizeof(uint32_t);
    key.mv_data = &root_asn;

    // (zero-copy)
    if (mdb_get(txn, asn_cone, &key, &data) == MDB_SUCCESS)
    {
        const uint32_t* ptr = static_cast<const uint32_t*>(data.mv_data);
        size_t count = data.mv_size / sizeof(uint32_t);

        
        return std::binary_search(ptr, ptr + count, target_asn);
    }
    return false;
}

/// @brief Runs a series of lookups and functions and measure time of them
/// @param num_queries Number of queries it runs for each function, default = 1,000,000
void AsInfo::benchmark(size_t num_queries)
{
    std::cout << "\nStarting Benchmark with Iterations " << num_queries << std::endl;

    /// init benchmark setup
    std::mt19937 gen(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<uint32_t> dist(0, MAX_ASN);
    std::uniform_int_distribution<uint32_t> dist2(0, 18);
    std::uniform_int_distribution<uint32_t> dist3(0, 200);

    std::vector<uint32_t> queries;
    queries.reserve(num_queries);


    for (size_t i = 0; i < num_queries; ++i) queries.push_back(dist(gen));

    std::vector<OrgInfo> tier1_org_vector;

    for (int x = 0; x < 19; x++)
    {
        OrgInfo oc;
        AutInfo* ac = get_as_full(TIER1_ASN[x], oc);
        if (strlen(oc.org_id) > 1) tier1_org_vector.push_back(oc);
    }

    /// 1 
    auto start = std::chrono::high_resolution_clock::now();
    size_t found = 0;

    for (size_t i = 0; i < num_queries; ++i)
    {
        AutInfo* as = get_as(queries[i]);
        if (as) found++;
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;

    double mlps = (num_queries / diff.count()) / 1000000.0;
    double fpc = (double)found / num_queries * 100;

      printf(
        "\nSIMPLE LOOKUP\n"
        "Speed: %.4f MLPS\n"
        "Average lookup: %.2f us\n"
        "Match found %zu %.2f %%\n",
        mlps, ((double)diff.count() * 1000000.0 / num_queries), found, fpc);

    /// 2

    start = std::chrono::high_resolution_clock::now();
    found = 0;

    for (size_t i = 0; i < num_queries; ++i)
    {
        OrgInfo otest = tier1_org_vector[dist2(gen)];

        if (check_as_org(TIER1_ASN[dist2(gen)], otest.org_id)) found++;
    }

    end = std::chrono::high_resolution_clock::now();
    diff = end - start;
    mlps = (num_queries / diff.count()) / 1000000.0;
    fpc = (double)found / num_queries * 100;

     printf(
        "\nVALIDATION LOOKUP (Boolean) Best case (asn and org_id in cache)\n"
        "Speed: %.4f MLPS\n"
        "Average lookup: %.2f us\n"
        "Match found %zu %.2f %%\n",
        mlps, ((double)diff.count() * 1000000.0 / num_queries), found, fpc);


    ///3 pre
    std::vector<std::string> all_org_ids;
    for (size_t i = 0; i < 100'000; ++i)
    {
        OrgInfo ot{};
        AutInfo* as = get_as_full(queries[i], ot);
        if (strlen(ot.org_id) > 1) all_org_ids.push_back(std::string(ot.org_id));
    }
    struct QueryPair
    {
        uint32_t asn;
        std::string org_id;
    };
    std::vector<QueryPair> stress_queries;
    std::uniform_int_distribution<size_t> org_dist(0, all_org_ids.size() - 1);

    for (size_t i = 0; i < num_queries; ++i)
    {
        stress_queries.push_back({dist(gen), all_org_ids[org_dist(gen)]});
    }

    /// 3
    start = std::chrono::high_resolution_clock::now();
    found = 0;

    for (size_t i = 0; i < num_queries; ++i)
    {
        // tempting forcing chache missing
        if (check_as_org(stress_queries[i].asn, stress_queries[i].org_id.c_str())) found++;
    }

    end = std::chrono::high_resolution_clock::now();

    diff = end - start;
    mlps = (num_queries / diff.count()) / 1000000.0;
    fpc = (double)found / num_queries * 100;

        printf(
        "\nVALIDATION LOOKUP (Boolean) worst case (asn and org_id random)\n"
        "Speed: %.4f MLPS\n"
        "Average lookup: %.2f us\n"
        "Match found %zu %.2f %%\n",
        mlps, ((double)diff.count() * 1000000.0 / num_queries), found, fpc);


    /// 4
    start = std::chrono::high_resolution_clock::now();
    found = 0;
    for (size_t i = 0; i < num_queries; ++i)
    {
        OrgInfo otest;
        AutInfo* as = get_as_full(queries[i], otest);
        if (as) found++;
    }

    end = std::chrono::high_resolution_clock::now();
    diff = end - start;
    mlps = (num_queries / diff.count()) / 1000000.0;
    fpc = (double)found / num_queries * 100;

    printf(
        "\nENRICHED LOOKUP (Join 1:1)\n"
        "Speed: %.4f MLPS\n"
        "Average lookup: %.2f us\n"
        "Match found %zu %.2f %%\n",
        mlps, ((double)diff.count() * 1000000.0 / num_queries), found, fpc);

    /// 5
    start = std::chrono::high_resolution_clock::now();
    found = 0;
    std::vector<AutInfo> asvr{};
    asvr.reserve(100);

    for (size_t i = 0; i < num_queries; ++i)
    {
        OrgInfo otest = tier1_org_vector[dist2(gen)];
        asvr.clear();

        OrgInfo* os = get_org_full(otest.org_id, asvr);
        if (os && !asvr.empty()) found++;
    }

    end = std::chrono::high_resolution_clock::now();
    diff = end - start;
    mlps = (num_queries / diff.count()) / 1000000.0;
    fpc = (double)found / num_queries * 100;

    printf(
        "\nFULL LOOKUP (Join 1:N)\n"
        "Speed: %.4f MLPS\n"
        "Average lookup: %.2f us\n"
        "Match found %zu %.2f %%\n",
        mlps, ((double)diff.count() * 1000000.0 / num_queries), found, fpc);

    /// 6 pre
    std::vector<AutInfo> topas_cone{};
    for (uint32_t x = 0; x < MAX_ASN; x++)
    {
        AutInfo* ar = get_as(x);
        if (ar) topas_cone.push_back(*ar);
    }
    std::sort(topas_cone.begin(), topas_cone.end(),
              [](const AutInfo& a, const AutInfo& b) { return a.cone > b.cone; });
    topas_cone.resize(200);

    /// 6
    start = std::chrono::high_resolution_clock::now();
    found = 0;
    for (size_t i = 0; i < num_queries; ++i)
    {
        uint32_t asn = topas_cone[dist3(gen)].asn;
        if (is_in_cone(asn, dist(gen))) found++;
    }
    end = std::chrono::high_resolution_clock::now();
    diff = end - start;
    mlps = (num_queries / diff.count()) / 1000000.0;
    fpc = (double)found / num_queries * 100;
    printf(
        "\nPOINT LOOKUP (Membership query)\n"
        "Speed: %.4f MLPS\n"
        "Average lookup: %.2f us\n"
        "Match found %zu %.2f %%\n",
        mlps, ((double)diff.count() * 1000000.0 / num_queries), found, fpc);
    
}

/// @brief Test key data, key size and data size of every table in database
void AsInfo::testing_tables()
{
    MDB_cursor* cur = nullptr;
    MDB_dbi array_table1[] = {
        asn_rel, aut_info, asn_p2c, asn_cone, org_info, org_to_asn, asname_to_asn
    };

    int rc;
    uint64_t kf64, kl64;
    uint32_t kf32, kl32;
    char kfchar[80];
    char klchar[80];

    for (uint8_t i = 0; i < 7; i++)
    {
        MDB_val key{}, data{};
        rc = mdb_cursor_open(txn, array_table1[i], &cur);
        if (rc != MDB_SUCCESS)
        {
            printf("cursor_open rc=%d (%s)\n", rc, mdb_strerror(rc));
            return;
        }

        rc = mdb_cursor_get(cur, &key, &data, MDB_FIRST);
        if (rc != MDB_SUCCESS)
        {
            printf("Table %u: cursor_get FIRST rc=%d (%s)\n", i + 1, rc, mdb_strerror(rc));
            mdb_cursor_close(cur);
            continue; 
        }

        if (i == 0) kf64 = *static_cast<uint64_t*>(key.mv_data);
        else if (i < 4) kf32 = *static_cast<uint32_t*>(key.mv_data);
        else 
        {
            size_t copy_size = (key.mv_size < 79) ? key.mv_size : 79;
            memcpy(kfchar, key.mv_data, copy_size);
            kfchar[copy_size] = '\0';
        }
        size_t d1 = data.mv_size;

        rc = mdb_cursor_get(cur, &key, &data, MDB_LAST);
        if (rc != MDB_SUCCESS)
        {
            printf("Table %u: cursor_get LAST rc=%d\n", i + 1, rc);
            mdb_cursor_close(cur);
            continue;
        }

        if (i == 0) kl64 = *static_cast<uint64_t*>(key.mv_data);
        else if (i < 4) kl32 = *static_cast<uint32_t*>(key.mv_data);
        else 
        {
            size_t copy_size = (key.mv_size < 79) ? key.mv_size : 79;
            memcpy(klchar, key.mv_data, copy_size);
            klchar[copy_size] = '\0';
        }
        size_t d2 = data.mv_size;

     
        if (d1 != 0 && d2 != 0)
        {
            if (i == 0) // As rel table uint64_t
            {
                printf("Table %u\n"
                       "First key value: %" PRIu64 "\n"
                       "First data size: %zu\n"
                       "Last key value: %llu\n"
                       "Last data size: %zu\n",
                       i + 1, kf64, d1, kl64, d2);
            }
            else if (i < 4) // table asn uint32_t
            {
                printf("Table %u\n"
                       "First key value: %u\n"
                       "First data size: %zu\n"
                       "Last key value: %u\n"
                       "Last data size: %zu\n",
                       i + 1, kf32, d1, kl32, d2);
            }
            else // table strings (char*)
            {
                printf("Table %u\n"
                       "First key value: %s\n"
                       "First data size: %zu\n"
                       "Last key value: %s\n"
                       "Last data size: %zu\n",
                       i + 1, kfchar, d1, klchar, d2);
            }
        }
        else 
        {
            printf("No data Table: %u\n", i + 1);
        }

        mdb_cursor_close(cur); 
    }
}
/// @brief For a given AS number find corrispective range from IANA AS ranges number allocated for RIRs.
/// @details It is a fallback if ASN not exists in database
Region AsInfo::find_rir(uint32_t asn)
{
    auto it = std::lower_bound(std::begin(ASN_RANGES), std::end(ASN_RANGES), asn,
                               [](const AsRange& a, uint32_t val) { return a.upper_limit < val; });

    if (it != std::end(ASN_RANGES))
        return it->rir;
    else
        return UNALLOCATED;
} 

