#ifndef CS_BLOB_H
#define CS_BLOB_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "FAT.h"
#include "MachO.h"

// Blob index
typedef struct __BlobIndex {
	uint32_t type;
	uint32_t offset;
} CS_BlobIndex;

// CMS superblob
typedef struct __SuperBlob {
	uint32_t magic;
	uint32_t length;
	uint32_t count;
	CS_BlobIndex *index;
} CS_SuperBlob;

// CMS blob magic types
enum {
    CSBLOB_REQUIREMENT = 0xfade0c00,
    CSBLOB_REQUIREMENTS = 0xfade0c01,
    CSBLOB_CODEDIRECTORY = 0xfade0c02,
    CSBLOB_EMBEDDED_SIGNATURE = 0xfade0cc0,
    CSBLOB_DETACHED_SIGNATURE = 0xfade0cc1,
    CSBLOB_ENTITLEMENTS = 0xfade7171,
    CSBLOB_DER_ENTITLEMENTS = 0xfade7172,
    CSBLOB_SIGNATURE_BLOB = 0xfade0b01
} CS_BlobType;

// Forward declaration
struct lc_code_signature;

// Convert blob magic to readable blob type string
char *cs_blob_magic_to_string(int magic);

// Retrieve CMS superblob from macho
// int macho_parse_superblob(FAT *fat, CS_SuperBlob *superblob, int machoIndex);

// Extract CMS signature to file
int macho_extract_cms_to_file(MachO *macho, CS_SuperBlob *superblob);

int macho_parse_superblob(MachO *macho, CS_SuperBlob *superblobOut, bool printAllSlots, bool verifySlots);

#endif // CS_BLOB_H