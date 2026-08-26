#define _CRT_SECURE_NO_WARNINGS
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <vector>

#include "asinfo.h"
#include "const.h"
#include "types.h"

#ifdef _WIN32
    #define _CRT_SECURE_NO_WARNINGS 
#endif

/// @brief Check if a char* is a valid number
inline long long check_number(const char* inp)
{
    char* endptr;
    errno = 0;

    long long res = strtoll(inp, &endptr, 10);

    // string empty or without number
    if (inp == endptr)
    {
        fprintf(stderr, "Error: No digits found in input.\n");
        exit(1);
    }

    // check overflow
    else if (errno == ERANGE)
    {
        fprintf(stderr, "Error: Number out of range for long long.\n");
        exit(1);
    }

    // check extra char, white char ' ' block execution
    else if (*endptr != '\0' && *endptr != '\n')
    {
        fprintf(stderr, "Error: Invalid characters after number: '%s'\n", endptr);
        exit(1);
    }

    return res;
}

/// @brief main function of cli
/// @details if not args are passed, default is

int main(int argc, char* argv[])
{
    uint32_t as_query = 0;
    AsRel ar_test{};
    enum class Action
    {
        NONE,
        BUILD,
        BENCH,
        STATS,
        TEST,
        QUERY_ASN,
        QUERY_AS_NAME,
        QUERY_ORG,
        QUERY_ORG_FULL,
        QUERY_CONE,
        CHECK_REL
    };
    Action current_action = Action::NONE;

    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <ASN> | -l | -t | -r <asn1> <asn2> [rel] | -o <ORG_ID>\n";
        return 1;
    }
    if (strcmp(argv[1], "-l") == 0 || strcmp(argv[1], "--load") == 0)
    {
        current_action = Action::BUILD;
    }
    else if (strcmp(argv[1], "-b") == 0 || strcmp(argv[1], "--benchmark") == 0)
    {
        current_action = Action::BENCH;
    }
    else if (strcmp(argv[1], "-s") == 0 || strcmp(argv[1], "--statistics") == 0)
    {
        current_action = Action::STATS;
    }
    else if (strcmp(argv[1], "-t") == 0 || strcmp(argv[1], "--test") == 0)
    {
        current_action = Action::TEST;
    }
    else if (strcmp(argv[1], "-r") == 0 || strcmp(argv[1], "--relationship") == 0)
    {
        if (argc >= 4)
        {
            ar_test.asn1 = (uint32_t)check_number(argv[2]);
            ar_test.asn2 = (uint32_t)check_number(argv[3]);

            if (ar_test.asn1 < MAX_ASN && ar_test.asn2 < MAX_ASN)
            {
                current_action = Action::CHECK_REL;

                // optional param
                if (argc > 4)
                {
                    ar_test.rel = (RelType)check_number(argv[4]);
                }
                else
                {
                    ar_test.rel = ANY_REL;
                }
            }
            else
            {
                std::cerr << "ASN out of range.\n";
                return 1;
            }
        }
        else
        {
            std::cerr << "-r requires two ASN: " << argv[0] << "examples: -r 123 456\n";
            return 1;
        }
    }
    else if (strcmp(argv[1], "-n") == 0 || strcmp(argv[1], "--as-name") == 0)
    {
        if (argc > 2)
        {
            current_action = Action::QUERY_AS_NAME;
        }
        else
        {
            std::cerr << "-n requires an AS name.\n";
            return 1;
        }
    }
    else if (strcmp(argv[1], "-o") == 0 || strcmp(argv[1], "--org") == 0)
    {
        if (argc > 2)
        {
            current_action = Action::QUERY_ORG;
        }
        else
        {
            std::cerr << "-o requires an Organization ID.\n";
            return 1;
        }
    }
    else if (strcmp(argv[1], "-of") == 0 || strcmp(argv[1], "--org-full") == 0)
    {
        if (argc > 2)
        {
            current_action = Action::QUERY_ORG_FULL;
        }
        else
        {
            std::cerr << "-of requires an Organization ID.\n";
            return 1;
        }
    }
    else if (strcmp(argv[1], "-c") == 0 || strcmp(argv[1], "--as-cone") == 0)
    {
        if (argc > 2)
        {
            as_query = (uint32_t)check_number(argv[2]);
            current_action = Action::QUERY_CONE;
        }
        else
        {
            std::cerr << "-c requires an as number\n";
            return 1;
        }
    }
    else
    {
        as_query = (uint32_t)check_number(argv[1]);
        current_action = Action::QUERY_ASN;
    }

    AsInfo ldb{};

    if (current_action == Action::BUILD)
    {
        if (ldb.build_db())
            return 0;
        else
            return 1;
    }
    else
    {
        if (!ldb.open()) return 1;
    }

    switch (current_action)
    {
        case Action::BENCH:
        {
            ldb.benchmark(MAX_QUERIES_TEST);
            break;
        }

        case Action::STATS:
        {
            ldb.stat();
            break;
        }
        case Action::TEST:
        {
            ldb.testing_tables();
            break;
        }
        case Action::QUERY_ASN:
        {
            AutInfo* r = ldb.get_as(as_query);
            if (r)
                r->write_aut(stdout);
            else
            {
                Region reg = ldb.find_rir(as_query);
                const char* regstr = regionToString(reg);
                printf("ASN %u not found | RIR %s\n", as_query, regstr);
            }
            break;
        }
        case Action::QUERY_AS_NAME:
        {
            char* asn = upper_char(argv[2]);
            std::vector<AutInfo> r = ldb.get_as_name(argv[2]);
            if (!r.empty())
            {
                printf("ASes found %zu\n\n", r.size());
                for (const auto& a : r) a.write_aut(stdout);
            }

            else
                printf("AS Name not found\n");
            break;
        }
        case Action::CHECK_REL:
        {
            ar_test.rel = ldb.check_rel(ar_test.asn1, ar_test.asn2, ar_test.rel);
            ar_test.write_rel(stdout);
            break;
        }
        case Action::QUERY_CONE:
        {
            std::vector<uint32_t> asnc{};
            asnc = ldb.get_cone(as_query);
            if (!asnc.empty())
            {
                printf("Cone for AS%u -  %zu\n\n", as_query, asnc.size());
                for (const auto& a : asnc) printf("%u\n", a);
            }
            else
                printf("Cone empty for AS%u\n", as_query);
            break;
        }
        case Action::QUERY_ORG:
        {
            char* up = upper_char(argv[2]);
            OrgInfo* r = ldb.get_org(up);
            if (r)
                r->write_org(stdout);
            else
                printf("Organitazion %s not found\n", argv[2]);
            break;
        }
        case Action::QUERY_ORG_FULL:
        {
            char* up = upper_char(argv[2]);
            std::vector<AutInfo> asv;
            OrgInfo* r = ldb.get_org_full(up, asv);
            if (r)
            {
                r->write_org(stdout);
                if (!asv.empty())
                {   
                    putchar('\n');
                    for (const auto& a : asv) 
                            a.write_aut(stdout);
                }
                
            }
            else
            {
                printf("Organitazion not found\n");
            }
            break;
        }
            default:
                break;
    }

    return 0;
}