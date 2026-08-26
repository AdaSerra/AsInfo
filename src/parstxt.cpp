#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <vector>

#include "parstxt.h"
#include "const.h"
#include "types.h"

/// @brief Parsing a single line of FIRST HALF caida org2info caida file
/// @details Format: org_id|changed|org_name|country|source
/// @param line char pointer to line to parse
/// @param vos reference to vector for storing object parsed
void extractOrgInfo(char* line, std::vector<OrgInfo>& vos)
{
    /**
    * NOTE:Kinds of prefix org_id
    * @aut	Automated AS	An AS number that doesn't have an organization declared in the WHOIS. Use it as a temporary
    *                       org_id. It indicates a "lone" AS whose owner is unknown to CAIDA.
    * @family	AS Family   A group of ASNs that CAIDA has determined belong to the same company. 
    *                       This he most important. Use it to group together "ASN Members" that don't have an official ID
    * @del	Delegated AS    An ASN extracted from the Registry Delegation Files (RIR). Indicates that
    *                       the ASN has been assigned to that country, but the company details are unknown. It often only has the country code
    */

       /*  if (line[0] == '#' || line[0] == '\0' || line[0] == '\n')
         return; */

    const char* fields[5];
    int f = 0;
    fields[f++] = line;

    for (char* p = line; *p && f < 5; p++)
    {
        if (*p == '|')
        {
            *p = '\0';
            fields[f++] = p + 1;
        }
    }

    if (f < 5) return;
    OrgInfo os{};
    //memset(&os, 0, sizeof(OrgInfo));

    copy_field(os.org_id, fields[0], sizeof(os.org_id));
    copy_field(os.org_name, fields[2], sizeof(os.org_name));
    os.cc[0] = fields[3][0];
    os.cc[1] = fields[3][1];

    os.rir = getRegion(fields[4]);
    // debug if (os.rir == UNALLOCATED) printf("%s", line);
    vos.emplace_back(os);
}

/// @brief Parsing a single line of SECOND HALF caida org2info file
/// @details Format: aut|changed|aut_name|org_id|opaque_id|source
/// @param line char pointer to line to parse
/// @param vas reference to vector for storing object parsed
void extractAutInfo(char* line, std::vector<AutInfo>& vas)
{

    /*  line[strcspn(line, "\r\n")] = 0;
     if (line[0] == '#' || line[0] == '\0' || line[0] == '\n')
         return; */

    const char* fields[6];
    int f = 0;
    fields[f++] = line;

    for (char* p = line; *p && f < 6; p++)
    {
        if (*p == '|')
        {
            *p = '\0';
            fields[f++] = p + 1;
        }
    }

    if (f < 6) return;
    AutInfo as{};
    //memset(&as, 0, sizeof(AutInfo));

    as.asn = (uint32_t)strtoul(fields[0], nullptr, 10);

    copy_field(as.aut_name, fields[2], sizeof(as.aut_name));
    copy_field(as.org_id, fields[3], sizeof(as.org_id));

    vas.emplace_back(as);
}
/*
old source file
void extractAsInfo(char* line, std::vector<AsInfo> &vas)
{
    const char *fields[5];
    int f = 0;
    fields[f++] = line;

    // Parsing
    for (char *p = line; *p && f < 5; p++) {
        if (*p == ',') {
            *p = '\0'; // trick , with 0
            fields[f++] = p + 1;
        }
    }

    if (f < 4) return; // field need

    AsInfo as;
    memset(&as, 0, sizeof(AsInfo)); // Init asinfo

    //
    as.asn = (uint32_t)strtoul(fields[0], nullptr, 10);


    copy_field(as.nshort, fields[1], sizeof(as.nshort));
    copy_field(as.nfull, fields[2], sizeof(as.nfull));

    as.cc[0] = fields[3][0];
    as.cc[1] = fields[3][1];

    // 4. Benign

    vas.emplace_back(as);
} */

/// @brief Parsing a single line of relationship caida file (serial1, serial2, v6)
/// @details Format: asn|asn|[-1,0]|[bgp,mpl] last field is only serial2 and is ignored
/// @param line char pointer to line to parse
/// @param vas reference to vector for storing object parse
void extractAsRel(char* line, std::vector<AsRel>& vas)
{
    //
    if (line[0] == '#' || line[0] == '\0' || line[0] == '\n') return;

    const char* fields[3];
    int f = 0;
    fields[f++] = line;

    for (char* p = line; *p && f < 3; p++)
    {
        if (*p == '|')
        {
            *p = '\0';
            fields[f++] = p + 1;
        }
    }

    if (f < 3) return;
    // last field can be bgp/mpl in serial 2 or only \n in serial 1, isolate it
    char* p_extra = (char*)fields[2];
    while (*p_extra && *p_extra != '|' && *p_extra != '\n' && *p_extra != '\r')
    {
        p_extra++;
    }
    *p_extra = '\0';  // truncate

    AsRel as;
    as.asn1 = (uint32_t)strtoul(fields[0], nullptr, 10);
    as.asn2 = (uint32_t)strtoul(fields[1], nullptr, 10);

    // int for negative value
    int temp_rel = (int)strtol(fields[2], nullptr, 10);
    as.rel = (RelType)temp_rel;

    vas.emplace_back(as);
}