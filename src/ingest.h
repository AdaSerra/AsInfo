#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unordered_map>
#include <vector>

#include "const.h"
#include "types.h"
#include "parstxt.h"


uint32_t computeConeSize(uint32_t root_asn, 
                        std::unordered_map<uint32_t, std::vector<uint32_t>> &adj, 
                        std::unordered_map<uint32_t, std::vector<uint32_t>> &adjcone);
void analyze_relationships(std::vector<AsRel>& vas, 
                                 std::vector<AutInfo>& asi, 
                                 std::unordered_map<uint32_t, size_t>& asn_to_idx,
                                 std::unordered_map<uint32_t, std::vector<uint32_t>>& asn_p2c);

void compute_as_metrics(std::vector<AutInfo>& asi, 
                        std::unordered_map<uint32_t, std::vector<uint32_t>>& asn_p2c,
                        std::unordered_map<uint32_t, size_t>& asn_to_idx,
                        std::unordered_map<uint32_t, std::vector<uint32_t>> &adjcone);

void aggregate_organizations(std::vector<OrgInfo>& osi, std::vector<AutInfo>& asi,
                            std::unordered_map<uint32_t, std::vector<uint32_t>>& asn_p2c);

inline bool is_clique(uint32_t asn)
{
    for (int x = 0; x < 19; x++)
    {
        if (asn == TIER1_ASN[x])
            return true;
    }
    return false;
}

inline bool is_ixp(uint32_t asn)
{
    for (int x = 0; x < 28; x++)
    {
        if (asn == IXP_ASN[x])
            return true;
    }
    return false;
}