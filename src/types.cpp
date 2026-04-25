#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <vector>

#include "types.h"
#include "parstxt.h"

/// @brief Write AutInfo object in a stream
/// @param fout pointer to stream outputing
void AutInfo::write_aut(FILE* fout) const
{
    const char* typstr = typeToString();

    // correct order
    // %u -> asn
    // %s -> aut_name
    // %s -> org_id
    // %s -> typstr (era %u, causava crash/warning)

    fprintf(fout,
            "ASN: %u\n"
            "Name: %s | Organization ID: %s\n"
            "Type: %s | Customer Cone: %u | Global Degree: %u\n"
            "Customer (p2c): %u | Peer (p2p): %u | Provider(c2p): %u | Transit (c2p+p2c): %u\n",
            asn, aut_name, org_id,
            typstr, 
            cone, degree, p2c_count, p2p_count, c2p_count,
            (p2c_count + c2p_count));  
}

/// @brief Write OrgInfo object in a stream
/// @param fout pointer to stream outputing
void OrgInfo::write_org(/* std::vector<AsInfo> &asv */ FILE* fout) const
{
    const char* reg = regionToString(rir);

    fprintf(fout, "Organization Name: %s | ID: %s | Country: %.2s | RIR: %s\n", org_name, org_id, cc, reg);
    fprintf(fout, "Total As %u | Total Cone (1 level recursion) %u | Total Degree %u\n", total_asn, total_cone, total_deg);
    fprintf(fout, "Customers (p2c): %u | Peer (p2p): %u | Provider (c2p) %u\n", total_p2c, total_p2p, total_c2p);
    /*  for (int x = 0; x < asv.size(); x++)
     {
         asv[x].print_as();
     } */
}

/// @brief Write AsRel object in a stream
/// @param fout pointer to stream outputing
void AsRel::write_rel(FILE* fout)
{
    const char* relstr = "";
    if (rel == P2C)
        relstr = "P2C";
    else if (rel == C2P)
        relstr = "C2P";
    else if (rel == P2P)
        relstr = "PEER";
    else if (rel == NOT_REL)
        relstr = "NOT EXISTS";
    else
        relstr = "EXISTS";
    fprintf(fout, "AS%u AS%u REL %s\n", asn1, asn2, relstr);
}