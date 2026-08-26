#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <iostream>
#include <ostream>
#include <unordered_map>
#include <vector>

#include "ingest.h"
#include "const.h"
#include "parstxt.h"
#include "types.h"

#ifdef _WIN32
    #define _CRT_SECURE_NO_WARNINGS 
#endif



/// @brief Computes the size of the Customer Cone for a given AS number
/// @details It runs a complete Breadth-First Search and is here that CAIDA data may be different
/// @param root_asn ASN to be calculated
/// @param adj Map ASN to entire direct customers list ASN
/// @param adjcone  Map ASN to entire Cone list ASN
/// @warning This function is NOT thread-safe due to static buffers.
/// @return the total count of unique Cone ASNs
uint32_t computeConeSize(uint32_t root_asn, std::unordered_map<uint32_t, std::vector<uint32_t>>& adj, std::unordered_map<uint32_t, std::vector<uint32_t>>& adjcone)
{
    static std::vector<uint8_t> global_visited_mask(MAX_ASN, 0);  // mask for check visited asn
    static std::vector<uint32_t> queue;
    static std::vector<uint32_t> visited_nodes;  // total unique asn visited

    auto it_root = adj.find(root_asn);  // check map asn / p2c
    if (it_root == adj.end()) return 0;

    auto it_cone = adjcone.find(root_asn);  // check map asn / cone (all asn)
    if (it_cone != adjcone.end())
    {
        return (uint32_t)it_cone->second.size() - 1;
    }

    queue.clear();
    visited_nodes.clear();
    queue.push_back(root_asn);
    global_visited_mask[root_asn] = 1;
    visited_nodes.push_back(root_asn);

    size_t head = 0;
    while (head < queue.size())
    {
        uint32_t current = queue[head++];
        auto it = adj.find(current);
        if (it != adj.end())
        {
            for (uint32_t child : it->second)
            {
                if (child < MAX_ASN && !global_visited_mask[child])
                {
                    global_visited_mask[child] = 1;
                    queue.push_back(child);
                    visited_nodes.push_back(child);
                }
            }
        }
    }

    // Cleanup 
    for (uint32_t asn : visited_nodes)
    {
        global_visited_mask[asn] = 0;
    }

    // remove root asn
    if (!visited_nodes.empty()) visited_nodes.erase(visited_nodes.begin());

    adjcone[root_asn] = visited_nodes;

    return (uint32_t)visited_nodes.size();
}


/// @brief Computes the size of the Customer Cone for a given organization
/// @details The calculation is limited to the first level (direct customers) to avoid inflated metrics caused by recursive or inferred relationships in raw data
/// @param as_list List of ASN in organization
/// @param adj Map ASN to all direct customers list ASN
/// @warning This function is NOT thread-safe due to static buffers.
/// @return the total count of unique direct customer ASNs
uint32_t computeOrgCone(std::vector<uint32_t>& as_list, std::unordered_map<uint32_t, std::vector<uint32_t>>& adj)
{
    // Static vectors to reuse memory across calls, improving performance in bulk processing
    static std::vector<uint32_t> queue;
    static std::vector<uint32_t> visited;

    queue.clear();
    visited.clear();

    // Mask used for deduplication to ensure each ASN is counted only once
    static std::vector<uint8_t> mask(MAX_ASN, 0);

    // Initial phase: Add all ASNs belonging to the Organization to the processing queue
    for (uint32_t asn : as_list)
    {
        if (asn < MAX_ASN && !mask[asn])
        {
            mask[asn] = 1;
            queue.push_back(asn);

            // Note: We do NOT add the organization's own ASNs to 'visited'
            // because we want to count only external customers.
        }
    }

    // Process the queue: Explore relationships
    size_t head = 0;
    while (head < queue.size())
    {
        uint32_t cur = queue[head++];

        auto it = adj.find(cur);
        if (it != adj.end())
        {
            for (uint32_t child : it->second)
            {
                // If child is within bounds and not already processed
                if (child < MAX_ASN && !mask[child])
                {
                    mask[child] = 1;

                    /**
                     * CRITICAL: We do NOT push 'child' back into the queue.
                     * This stops the BFS at the first level (Direct Customers only).
                     * This prevents "cone explosion" where a single inferred link to a
                     * large Tier-1 provider could incorrectly add thousands of indirect ASNs.
                     */
                    // queue.push_back(child);

                    visited.push_back(child);
                }
            }
        }
    }

    // Clean up phase: Reset the bitmask for the next function call.
    // We only reset bits that were actually set to maintain O(N) where N is nodes seen.
    for (uint32_t asn : as_list)
        if (asn < MAX_ASN) mask[asn] = 0;

    for (uint32_t asn : visited) mask[asn] = 0;

    // Return the total count of unique direct customer ASNs
    return (uint32_t)visited.size();
}


/// @brief Analyze Relationship among couple of Asn
/// @details For every relationship increase relative counters in AsInfo struct and if rel == P2C put ASN customer in relative Map. Relationship are in double format: asn1|asn2 and asn2|asn1 with inverse relationship
/// @param vas All relaitionship from rels files
/// @param asi Global list of AS 
/// @param asn_to_idx ASN to autinfov index mapping
/// @param asn_p2c Map ASN to entire direct customers list ASN
void analyze_relationships(std::vector<AsRel>& vas, std::vector<AutInfo>& asi, std::unordered_map<uint32_t, size_t>& asn_to_idx, std::unordered_map<uint32_t, std::vector<uint32_t>>& asn_p2c)
{

    // Mapping ASN -> Index vector
    for (size_t i = 0; i < asi.size(); ++i)
    {
        asn_to_idx[asi[i].asn] = i;
    }

    for (/* const */ auto& rel : vas)
    {
        // P2C (Provider to Customer)
        if (rel.rel == P2C)
        {
            if (asn_to_idx.count(rel.asn1)) asi[asn_to_idx[rel.asn1]].p2c_count++;
            if (asn_to_idx.count(rel.asn2)) asi[asn_to_idx[rel.asn2]].c2p_count++;
            asn_p2c[rel.asn1].push_back(rel.asn2);
        }
        // P2P (Peer to Peer)
        else if (rel.rel == P2P)
        {
            if (asn_to_idx.count(rel.asn1)) asi[asn_to_idx[rel.asn1]].p2p_count++;
            if (asn_to_idx.count(rel.asn2)) asi[asn_to_idx[rel.asn2]].p2p_count++;
        }
        // C2P (Customer to Provider) never present in caida files, ignored
        /*   else if (rel.rel == C2P)
          {
              if (asn_to_idx.count(rel.asn1)) asi[asn_to_idx[rel.asn1]].c2p_count++;
              if (asn_to_idx.count(rel.asn2)) asi[asn_to_idx[rel.asn2]].p2c_count++;
              asn_p2c[rel.asn2].push_back(rel.asn1);
          } */
    }
}

/// @brief Compute metrics of a given AS
/// @details The classification of the 'Content' type is based on established network topology heuristics
/// @param asi 
/// @param asn_p2c 
/// @param asn_to_idx 
/// @param adjcone 
void compute_as_metrics(std::vector<AutInfo>& asi, std::unordered_map<uint32_t, std::vector<uint32_t>>& asn_p2c, std::unordered_map<uint32_t, size_t>& asn_to_idx,
                        std::unordered_map<uint32_t, std::vector<uint32_t>>& adjcone)
{
    for (auto& as_entry : asi)
    {
        uint32_t asn = as_entry.asn;

        // Cone
        if (asn_p2c.count(asn))
        {
            as_entry.cone = computeConeSize(asn, asn_p2c, adjcone);
        }

        // total degree
        as_entry.degree = as_entry.p2c_count + as_entry.p2p_count + as_entry.c2p_count;

        // Euristic type classification
        /**
        * NOTE:An AS is identified as a Content Provider when it exhibits a high peering density (p2p_count>100)
        * coupled with a minimal customer base (p2c_count≤5), 
        * a connectivity profile characteristic of major CDNs and edge networks
        */
        if (is_ixp(asn))
            as_entry.type = IXP;
        else if (is_clique(asn))
            as_entry.type = TIER1;
        else if (as_entry.p2p_count > 100 && as_entry.p2c_count <= 5)
            as_entry.type = CONTENT;
        else if (as_entry.p2c_count > 0)
            as_entry.type = TIER2;
        else
            as_entry.type = TIER3;
    }
}

/// @brief Compute metrics of a given organization
/// @param osi 
/// @param asi 
/// @param asn_p2c 
void aggregate_organizations(std::vector<OrgInfo>& osi, std::vector<AutInfo>& asi, std::unordered_map<uint32_t, std::vector<uint32_t>>& asn_p2c)
{
    std::unordered_map<std::string, std::vector<uint32_t>> org_to_asns_map;
    std::unordered_map<std::string, OrgInfo> agg_map;

    // group ASN per OrgID
    for (/* const */ auto& a : asi)
    {
        org_to_asns_map[a.org_id].push_back(a.asn);

        auto& stat = agg_map[a.org_id];
        stat.total_asn++;
        stat.total_p2c += a.p2c_count;
        stat.total_p2p += a.p2p_count;
        stat.total_c2p += a.c2p_count;
    }

    // aggregation on object
    for (auto& org : osi)
    {
        std::string oid = org.org_id;
        if (agg_map.count(oid))
        {
            org.total_asn = agg_map[oid].total_asn;
            org.total_p2c = agg_map[oid].total_p2c;
            org.total_p2p = agg_map[oid].total_p2p;
            org.total_c2p = agg_map[oid].total_c2p;
            org.total_deg = org.total_p2c + org.total_p2p + org.total_c2p;

            // Cone first level (Direct Customers)
            org.total_cone = computeOrgCone(org_to_asns_map[oid], asn_p2c);
        }
    }
}
