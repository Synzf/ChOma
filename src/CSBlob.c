#include "CSBlob.h"

#include "CodeDirectory.h"
#include "MachO.h"
#include "MachOByteOrder.h"
#include "MachOLoadCommand.h"
#include "BufferedStream.h"
#include "MemoryStream.h"
#include "FileStream.h"
#include <mach-o/loader.h>
#include <stddef.h>

char *cs_blob_magic_to_string(int magic)
{
	switch (magic)
	{
	case CSBLOB_REQUIREMENT:
		return "Requirement blob";
	case CSBLOB_REQUIREMENTS:
		return "Requirements blob";
	case CSBLOB_CODEDIRECTORY:
		return "Code directory blob";
	case CSBLOB_EMBEDDED_SIGNATURE:
		return "Embedded signature blob";
	case CSBLOB_DETACHED_SIGNATURE:
		return "Detached signature blob";
	case CSBLOB_ENTITLEMENTS:
		return "Entitlements blob";
	case CSBLOB_DER_ENTITLEMENTS:
		return "DER entitlements blob";
	case CSBLOB_SIGNATURE_BLOB:
		return "Signature blob";
	default:
		return "Unknown blob type";
	}
}

char *cs_slot_index_to_string(int magic)
{
	switch (magic)
	{
	case CSSLOT_CODEDIRECTORY:
		return "Code directory slot";
	case CSSLOT_ALTERNATE_CODEDIRECTORIES:
		return "Alternate code directory slot";
	case CSSLOT_INFOSLOT:
		return "Info slot";
	case CSSLOT_REQUIREMENTS:
		return "Requirements slot";
	case CSSLOT_RESOURCEDIR:
		return "Resource Dir slot";
	case CSSLOT_APPLICATION:
		return "Application slot";
	case CSSLOT_ENTITLEMENTS:
		return "Entitlements slot";
	case CSSLOT_DER_ENTITLEMENTS:
		return "DER entitlements slot";
	case CSSLOT_SIGNATURESLOT:
		return "Signature slot";
	default:
		return "Unknown blob type";
	}
}

int macho_parse_signature_blob_to_der_encoded_data(MachO *macho, uint32_t signatureBlobOffset, uint32_t signatureBlobLength, void *outputDER)
{
	return macho_read_at_offset(macho, signatureBlobOffset + 8, signatureBlobLength - 8, outputDER);
}

int decodedsuperblob_parse_blobs(MachO *macho, DecodedSuperBlob *decodedSuperblob, bool printAllSlots, bool verifySlots)
{
	DecodedBlob *currentBlob = decodedSuperblob->firstBlob;
	int count = 0;
    while (currentBlob->next) {
        currentBlob = currentBlob->next;
		uint32_t blobType = currentBlob->type;
		printf("Slot %d: %s (type: 0x%x).\n", count++, cs_slot_index_to_string(blobType), blobType);
		//printf("Slot %d: %s (offset 0x%x, type: 0x%x).\n", blobCount + 1, cs_slot_index_to_string(blobType), blobOffset + csLoadCommand->dataoff, blobType);

		if (blobType == CSSLOT_CODEDIRECTORY || blobType == CSSLOT_ALTERNATE_CODEDIRECTORIES)
		{
			CS_CodeDirectory *codeDirectory = malloc(sizeof(CS_CodeDirectory));
			memset(codeDirectory, 0, sizeof(CS_CodeDirectory));
			buffered_stream_read(currentBlob->stream, 0, sizeof(CS_CodeDirectory), codeDirectory);
			CODE_DIRECTORY_APPLY_BYTE_ORDER(codeDirectory, BIG_TO_HOST_APPLIER);
			printf("This is the %s, magic %#x.\n", cs_blob_magic_to_string(codeDirectory->magic), codeDirectory->magic);
			//macho_parse_code_directory_blob(macho, blobOffset + csLoadCommand->dataoff, codeDirectory, printAllSlots, verifySlots);
		}
		else if (blobType == CSSLOT_SIGNATURESLOT) {
			CS_GenericBlob *cms_blob = malloc(sizeof(CS_GenericBlob));
			memset(cms_blob, 0, sizeof(CS_GenericBlob));
			buffered_stream_read(currentBlob->stream, 0, sizeof(CS_GenericBlob), cms_blob);
			GENERIC_BLOB_APPLY_BYTE_ORDER(cms_blob, BIG_TO_HOST_APPLIER);
			printf("This is the %s, magic %#x.\n", cs_blob_magic_to_string(cms_blob->magic), cms_blob->magic);
		}
		else {
			CS_GenericBlob *generic_blob = malloc(sizeof(CS_GenericBlob));
			memset(generic_blob, 0, sizeof(CS_GenericBlob));
			buffered_stream_read(currentBlob->stream, 0, sizeof(CS_GenericBlob), generic_blob);
			GENERIC_BLOB_APPLY_BYTE_ORDER(generic_blob, BIG_TO_HOST_APPLIER);
			printf("This is the %s, magic %#x.\n", cs_blob_magic_to_string(generic_blob->magic), generic_blob->magic);
		}
	}
	return 0;
}

uint8_t *macho_find_code_signature(MachO *macho)
{
	__block uint8_t *dataOut = NULL;
	macho_enumerate_load_commands(macho, ^(struct load_command loadCommand, uint64_t offset, void *cmd, bool *stop) {
		if (loadCommand.cmd == LC_CODE_SIGNATURE) {
			struct linkedit_data_command *csLoadCommand = ((struct linkedit_data_command *)cmd);
			LINKEDIT_DATA_COMMAND_APPLY_BYTE_ORDER(csLoadCommand, LITTLE_TO_HOST_APPLIER);
			dataOut = malloc(csLoadCommand->datasize);
			macho_read_at_offset(macho, csLoadCommand->dataoff, csLoadCommand->datasize, dataOut);
			*stop = true;
		}
	});
	return dataOut;
}

uint64_t macho_find_code_signature_offset(MachO *macho)
{
	__block uint64_t offsetOut = 0;
	macho_enumerate_load_commands(macho, ^(struct load_command loadCommand, uint64_t offset, void *cmd, bool *stop) {
		if (loadCommand.cmd == LC_CODE_SIGNATURE) {
			struct linkedit_data_command *csLoadCommand = ((struct linkedit_data_command *)cmd);
			LINKEDIT_DATA_COMMAND_APPLY_BYTE_ORDER(csLoadCommand, LITTLE_TO_HOST_APPLIER);
			offsetOut = csLoadCommand->dataoff;
			*stop = true;
		}
	});
	return offsetOut;
}

int macho_extract_cs_to_file(MachO *macho, CS_SuperBlob *superblob)
{
	FILE *csDataFile = fopen("Code_Signature-Data", "wb+");
	fwrite(superblob, BIG_TO_HOST(superblob->length), 1, csDataFile);
	fclose(csDataFile);
	return 0;
}

DecodedSuperBlob *superblob_decode(CS_SuperBlob *superblob)
{
	DecodedSuperBlob *decodedSuperblob = malloc(sizeof(DecodedSuperBlob));
	if (!decodedSuperblob) return NULL;
	memset(decodedSuperblob, 0, sizeof(DecodedSuperBlob));

	DecodedBlob **nextBlob = &decodedSuperblob->firstBlob;
	decodedSuperblob->magic = BIG_TO_HOST(superblob->magic);

	for (uint32_t i = 0; i < BIG_TO_HOST(superblob->count); i++) {
		CS_BlobIndex curIndex = superblob->index[i];
		BLOB_INDEX_APPLY_BYTE_ORDER(&curIndex, BIG_TO_HOST_APPLIER);
		printf("decoding %u (type: %x, offset: 0x%x)\n", i, curIndex.type, curIndex.offset);

		CS_GenericBlob *start = (CS_GenericBlob *)(((uint8_t*)superblob) + curIndex.offset);

		MemoryStream *stream = buffered_stream_init_from_buffer(start, BIG_TO_HOST(start->length));
		if (!stream) {
			decoded_superblob_free(decodedSuperblob);
			return NULL;
		}

		*nextBlob = malloc(sizeof(DecodedBlob));
		(*nextBlob)->stream = stream;
		(*nextBlob)->next = NULL;
		(*nextBlob)->type = curIndex.type;
		nextBlob = &(*nextBlob)->next;
	}
	return decodedSuperblob;
}

CS_SuperBlob *superblob_encode(DecodedSuperBlob *decodedSuperblob)
{
	uint32_t blobCount = 0, blobSize = 0;

	// Determine amount and size of contained blobs
	DecodedBlob *nextBlob = decodedSuperblob->firstBlob;
	while (nextBlob) {
		blobCount++;
		blobSize += memory_stream_get_size(nextBlob->stream);
		nextBlob = nextBlob->next;
	}

	uint32_t superblobLength = sizeof(CS_SuperBlob) + (sizeof(CS_BlobIndex) * blobCount) + blobSize;
	CS_SuperBlob *superblob = malloc(superblobLength);

	// Populate superblob fields
	superblob->count = blobCount;
	superblob->length = superblobLength;
	superblob->magic = decodedSuperblob->magic;
	SUPERBLOB_APPLY_BYTE_ORDER(superblob, HOST_TO_BIG_APPLIER)

	// Populate indexes and write actual backing data
	uint32_t idx = 0;
	uint32_t dataStartOffset = sizeof(CS_SuperBlob) + (sizeof(CS_BlobIndex) * blobCount);
	uint8_t *superblobData = ((uint8_t*)superblob) + dataStartOffset;
	uint8_t *superblobDataCur = superblobData;
	nextBlob = decodedSuperblob->firstBlob;
	while (nextBlob) {
		CS_BlobIndex *curIndex = &superblob->index[idx];
		MemoryStream *curStream = nextBlob->stream;
		uint32_t curSize = memory_stream_get_size(curStream);

		memory_stream_read(curStream, 0, curSize, superblobDataCur);

		// Automatically update the length of the blob based on the length of the stream backing it
		((CS_GenericBlob *)superblobDataCur)->length = HOST_TO_BIG(curSize);

		curIndex->offset = dataStartOffset + (superblobDataCur - superblobData);
		curIndex->type = nextBlob->type;
		BLOB_INDEX_APPLY_BYTE_ORDER(curIndex, HOST_TO_BIG_APPLIER);

		superblobDataCur += curSize;
		idx++;

		nextBlob = nextBlob->next;
	}
	return superblob;
}

void decoded_superblob_free(DecodedSuperBlob *decodedSuperblob)
{
	DecodedBlob *nextBlob = decodedSuperblob->firstBlob;
	while (nextBlob) {
		DecodedBlob *prevBlob = nextBlob;
		nextBlob = nextBlob->next;
		if (prevBlob->stream) {
			memory_stream_free(prevBlob->stream);
		}
		free(prevBlob);
	}
	free(decodedSuperblob);
}

uint64_t alignToSize(int size, int alignment)
{
	return (size + alignment - 1) & ~(alignment - 1);
}

int update_load_commands(MachO *macho, CS_SuperBlob *superblob, uint64_t originalSize) {
	macho_enumerate_load_commands(macho, ^(struct load_command loadCommand, uint64_t offset, void *cmd, bool *stop) {
		bool foundOne = false;
		if (loadCommand.cmd == LC_SEGMENT_64) {
			struct segment_command_64 *segmentCommand = ((struct segment_command_64 *)cmd);
			SEGMENT_COMMAND_64_APPLY_BYTE_ORDER(segmentCommand, LITTLE_TO_HOST_APPLIER); // TODO: Move this to macho_enumerate_load_commands impl
			if (strcmp(segmentCommand->segname, "__LINKEDIT") != 0) return;
			uint64_t difference = segmentCommand->filesize - originalSize;
			uint64_t newFileSize = (uint64_t)(superblob->length >> 0x10) + difference;
			uint64_t newVMSize = alignToSize(newFileSize, 0x4000);
			printf("Updating %s segment - offset: 0x%llx, filesize: 0x%llx, vmsize: 0x%llx.\n", segmentCommand->segname, segmentCommand->fileoff, newFileSize, newVMSize);
			*stop = foundOne;
			foundOne = true;
		}
		if (loadCommand.cmd == LC_CODE_SIGNATURE) {
			struct linkedit_data_command *csLoadCommand = ((struct linkedit_data_command *)cmd);
			LINKEDIT_DATA_COMMAND_APPLY_BYTE_ORDER(csLoadCommand, LITTLE_TO_HOST_APPLIER);
			csLoadCommand->datasize = superblob->length;
			// LINKEDIT_DATA_COMMAND_APPLY_BYTE_ORDER(csLoadCommand, HOST_TO_LITTLE_APPLIER);
			printf("Updating code signature load command - offset: 0x%x, size: 0x%x.\n", csLoadCommand->dataoff, HOST_TO_BIG(csLoadCommand->datasize));
			*stop = foundOne;
			foundOne = true;
		}
	});
	return 0;
}