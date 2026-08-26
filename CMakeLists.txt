cmake_minimum_required(VERSION 3.15)
project(AsInfo VERSION 1.0 LANGUAGES C CXX)

# --- Build options ---
option(BUILD_AS_DLL "Compile as DLL (shared library)" OFF)
option(DELETE_OLD_DB "Delete old database during setup" OFF)
option(CHECK_DATE "Check the dates of the latest available caida files" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- DATASET Folder---
set(DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dataset")
if(NOT EXISTS "${DATA_DIR}")
    file(MAKE_DIRECTORY "${DATA_DIR}")
endif()

# Check datasets dates
if(NOT CHECK_DATE)
    
    # check file exist
    file(GLOB FILES_FOUND RELATIVE "${DATA_DIR}" "${DATA_DIR}/*.as-org2info.txt")

    # find first date prefix and use it as prefix for all files
    if(FILES_FOUND)
        list(GET FILES_FOUND 0 FIRST_FILE)
        string(REGEX REPLACE "\\..*" "" CURRENT_DATE "${FIRST_FILE}") 

    else()
    message(FATAL_ERROR "File as-org2info.txt not found. Download last version of it at https://publicdata.caida.org/datasets/as-organizations/ and decomprime it in ${DATA_DIR}")
    endif()
else()
    # Current date string (Format yyyyMM01)
    string(TIMESTAMP CURRENT_DATE "%Y%m01")
endif()


# path to file
set(ORG_FILE "${DATA_DIR}/${CURRENT_DATE}.as-org2info.txt")
set(REL_S1_FILE "${DATA_DIR}/${CURRENT_DATE}.as-rel.txt")
set(REL_S2_FILE "${DATA_DIR}/${CURRENT_DATE}.as-rel2.txt")
set(REL_V6_FILE "${DATA_DIR}/${CURRENT_DATE}.as-rel.v6-stable.txt")

# Check exist mininum dataset files
if(NOT EXISTS "${ORG_FILE}")
    message(FATAL_ERROR "File ${ORG_FILE} not found. Download last version of it at https://publicdata.caida.org/datasets/as-organizations/ and decomprime it in ${DATA_DIR}")
endif()
if(NOT EXISTS "${REL_S1_FILE}")
    message(FATAL_ERROR "File ${REL_S1_FILE} not found. Download last version of it at https://publicdata.caida.org/datasets/as-relationships/serial-1/ and decomprime it in ${DATA_DIR}")
endif()
if(NOT EXISTS "${REL_S2_FILE}")
    message(WARNING "File ${REL_S2_FILE} not found. Download last version of it at https://publicdata.caida.org/datasets/as-relationships/serial-2/ and decomprime it in ${DATA_DIR}")
endif()
if(NOT EXISTS "${REL_V6_FILE}")
    message(WARNING "File ${REL_V6_FILE} not found. Download last version of it at https://publicdata.caida.org/datasets/as-relationships/serial-1/ and decomprime it in ${DATA_DIR}")
endif()

# --- SOURCES ---
set(SOURCES
    "src/asinfo.cpp"
    "src/types.cpp"
    "src/parstxt.cpp"
    "src/ingest.cpp"
    "dep/mdb.c"
    "dep/midl.c"
)

# --- TARGET (DLL o EXE) ---
if(BUILD_AS_DLL)
    add_library(asinfo SHARED ${SOURCES})
    target_compile_definitions(asinfo PRIVATE ASINFO_AS_DLL ASINFO_EXPORTS)
else()
    list(APPEND SOURCES "src/cli.cpp")
    add_executable(asinfo ${SOURCES})
endif()

# --- INCLUDES E FILES DEFINITIONS ---
target_include_directories(asinfo PRIVATE "./dep" "./src")
target_compile_definitions(asinfo PRIVATE
    ORG_FILE_PATH="${ORG_FILE}"
    REL_FILE_S1_PATH="${REL_S1_FILE}"
    REL_FILE_S2_PATH="${REL_S2_FILE}"
    REL_FILE_V6_PATH="${REL_V6_FILE}"
)

# --- COMPILER FLAGS ---
if(MSVC)
    set_source_files_properties(dep/mdb.c dep/midl.c PROPERTIES COMPILE_FLAGS 
    "/wd4333 /wd4172 /wd4267 /wd4244")

    target_compile_options(asinfo PRIVATE 
        $<$<CONFIG:Release>:/O2 /Ob2 /Oi /Ot /GT /GL /GA /Gw /Gy /arch:AVX2 /MT /GS- /GR->
        $<$<CONFIG:Debug>:/Od /Zi /MTd /RTC1 /fsanitize=address>
    )
    # Linker flags
    target_link_options(asinfo PRIVATE
        $<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF>
    )
    # Library Windows
    target_link_libraries(asinfo PRIVATE advapi32.lib)
else()
    # Flags GCC/Clang (POSIX)
    set_source_files_properties(dep/mdb.c PROPERTIES COMPILE_FLAGS "-Wno-unused-parameter -Wno-unused-but-set-variable")
    target_compile_options(asinfo PRIVATE 
        -Wall -Wextra
        $<$<CONFIG:Release>:-O3 -march=native>
        $<$<CONFIG:Debug>:-g -O0 -fsanitize=address>
    )
endif()

# --- DELETE DATABASE ---
if(DELETE_OLD_DB)
    if(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/AsInfo.db")
        file(REMOVE "${CMAKE_CURRENT_BINARY_DIR}/AsInfo.db")
        file(REMOVE "${CMAKE_CURRENT_BINARY_DIR}/AsInfo.db-lock")
    endif()
endif()