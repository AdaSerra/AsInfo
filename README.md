# AsInfo

![Language](https://img.shields.io/badge/language-C%2B%2B20-00599C.svg) ![License](https://img.shields.io/badge/license-MIT-blue.svg) ![LMDB](https://img.shields.io/badge/database-LMDB-blue)

AsInfo is a fast C++ search engine for managing and querying Autonomous System (AS), Organitazion and AS relationship data from [CAIDA](https://www.caida.org) (Center for Applied Internet Data Analysis) public datasets. 

It using [LMDB](https://github.com/LMDB/lmdb) Database to ensure data persistence, high access speed, and a small memory footprint thanks to memory mapping.

### 🚀 Key Features

- Pre-computed Data: AS and Organization metadata and metrics are calculated during bulk loading and stored for instant access.

- Extreme Performance: Thanks to LMDB's B-Tree structure, operations reach millions of queries per second (MLPS).

- Versatility: Can be used as a CLI, static library, or header-only module.

- Local Efficiency: Ideal for real-time analytics pipelines where network latency would be prohibitive.

### 🛠 Main API exposed (partial)

|  Function    |   Arguments      | CLI                   | Description                                            |
|--------------|----------------- |-----------------------|--------------------------------------------------------|
| get_as       | AS Number        | .\asinfo ASN          | Return (ptr) AS info object(metadata and counters)     |     
| get_org      | Org Id           | -o ORG_ID             | Return (ptr) Org info object(metadata and counters)    |
| check_rel    | ASN 1, ASN 2, rel| -r ASN1 ASN2 [rel]    | Validates if relationship [rel] exists between two ASN |
| get_as_name  | AS Name          | -n AS_Name            | Return all AS info objects matching thats AUT_NAME     |
| is_in_cone   | ASN 1, ASN 2     |                       | Check if ASN 2 is in customer cone of ASN 1            |

Mains functions have a light/fast version that check only existence of item and a complete/full version that return item and other items associated in others tables.
See source files for entire API list.
    

### 📊 Perfomance & Benchmark

The following tests were run with 1,000,000 queries. Thanks to LMDB's architecture, whose B-Tree have a maximum depth of 3 for these datasets, and memory mapping, the engine achieves performance in the millions of operations per second (MLPS).


| Test Type             | MLPS |Avg Latency (µs) |Match Rate (%)| Description                                                                     |
|-----------------------|------|-----------|----------|---------------------------------------------------------------------------------| 
|Simple Lookup          | 3.75 - 8 | ~0.25 - ~0.12     | ~30   | Looks up a random (gen from 0 to ~400k ) ASN in the database to see if it exists|
|Validation (Best Case) | 6.5 - 11.5   | ~0.15 - 0.09   | ~7   | Check if a given ASN (gen as above) belongs to a specific org_id using a small set of ASNs such as tier1 only, simulating an L1/L2 cache hit|
|Validation (Worst Case)| 4  - 4.7| ~0.25 - ~0.2    | ~0      | It performs the same verification as the previous one but using randomly generated pairs from a pool of 100,000 records, forcing a cache missing|
|Enriched Lookup Join 1:1 | 2 - 4.8 | ~0.5 - ~0.2   | ~30   | In addition to checking whether the ASN exists, it also extracts the associated organization data (OrgInfo) forcing more memory hop|
|Full Lookup Join 1:N | 1.4 - 0.2 | ~0.7 - ~5   | 100  | Given an existing organization (taken from Tier1 set), searches and returns the list of all ASNs belonging to it|
|Point Lookup           | 2.1 - 5 | ~0.45 - ~0.2  | ~13.5   | Check if a random (gen as usual) ASN is part of the Customer Cone of one of the top 200 global ASs by importance|   


(*Machines Test:  
left CPU Intel i7 3.5 GHz, L3 4Mb, 16GB RAM, HDD, OS: Windows 10 and Wsl2   
right CPU Amd 5 5 GHz, L3 32Mb, 32GB RAM, HDD, OS: Windows 11*)

A flat array and a more compact data structure (AS and Organization names are stored in fixed-length char arrays of length 80 and 96) would theoretically guarantee a 2/5x speedup, because the footprint would be reduced and the memory jumps would drop from the current 3-6 levels pointer indirection to a direct 1-2 memory offsets, but these results are still already remarkable and would lose the flexibility of LMDB.

### ⚙️ Installation and Use

##### Requirements
To compile and bulk correctly the database you need at least "as-org2info.txt" and "as-rel.txt" (relationship serial 1) files. 
  "as-rel2.txt" (serial 2) and "as-rel.v6-stable.txt" (v6 version) files are optional.
  Find these files links  https://www.caida.org/catalog/datasets/as-organizations and https://www.caida.org/catalog/datasets/as-relationships/

##### Compile
Change cmake option BUILD_AS_DLL ON / OFF if you want compile as Dll or Cli stand-alone.
Setting DELETE_OLD_DB if you want remove existing Database
  On Windows can launch .\build.ps1 with same options.

```
bash

mkdir build
cmake -DBUILD_AS_DLL=[ON/OFF] -S . -B build
cmake --build build --config Release
```

##### Initialization (C++)
If used as library, init the AsInfo object.
At first access create LMDB database and load datasets calling build_db. In following runs call open
Checking always return value. 

```
C++

AsInfo AsDb{}; 

//First access
if(!AsDb.build_db()) return -1;

//After
if(!AsDb.open()) return -1;

```

If use as CLI, first load datasets, querying
```
bash

.\asinfo -l
...
.\asinfo -n "GOOGLE" 
.\asinfo 3242 

```



### ⚖️ Licenses and Terms of Use

1. Software License

The AsInfo code is distributed under the MIT License.

LMDB: Included in the project, it is subject to the OpenLDAP Public License.

2. CAIDA Datasets (Important)

Using this tool with data provided by CAIDA automatically implies acceptance of the [CAIDA Acceptable Use Agreement](https://www.caida.org/about/legal/aua/public_aua/).

User Obligations:

Attribution: If you use this tool or the generated databases for publications, research, or reports, you must cite CAIDA as the data source.

Use: Attempts to de-anonymize the data or violate the redistribution terms specified by CAIDA are prohibited.

Recommended citation: "The CAIDA UCSD AS Relationships and AS-to-Organizations Datasets, [YYYYMM]".

3. Data Sources

CAIDA AS Relationships (Serial 1 & 2)

CAIDA AS to Organizations Mapping

### Note

This project is not officially affiliated with CAIDA, but is developed as a third-party tool to facilitate local analysis of their public datasets.
