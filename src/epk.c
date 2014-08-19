#include <epk.h>
#include <u-boot/image.h>
#include <arpa/inet.h>
#include <formats.h>

int is_kernel(const char *image_file) {
	FILE *file = fopen(image_file, "r");
	if (file == NULL) {
		printf("Can't open file %s", image_file);
		exit(1);
	}
	size_t header_size = sizeof(struct image_header);
	unsigned char* buffer = (unsigned char*) malloc(sizeof(char) * header_size);
	int read = fread(buffer, 1, header_size, file);
	if (read != header_size) return 0;
	fclose(file);
	struct image_header *image_header = (struct image_header *) buffer;
	int result = image_header->ih_magic == ntohl(IH_MAGIC);
	free(buffer);
	return result;
}

void extract_kernel(const char *image_file, const char *destination_file) {
	FILE *file = fopen(image_file, "r");
	if (file == NULL) {
		printf("Can't open file %s", image_file);
		exit(1);
	}
	fseek(file, 0, SEEK_END);
	int fileLength = ftell(file);
	rewind(file);
	unsigned char* buffer = (unsigned char*) malloc(sizeof(char) * fileLength);
	int read = fread(buffer, 1, fileLength, file);
	if (read != fileLength) {
		printf("Error reading file. read %d bytes from %d.\n", read, fileLength);
		free(buffer);
		exit(1);
	}
	fclose(file);

	struct image_header *image_header = (struct image_header *) buffer;
	FILE *out = fopen(destination_file, "w");
	int header_size = sizeof(struct image_header);
	fwrite(buffer + header_size, 1, read - header_size, out);
	fclose(out);
	free(buffer);
}

void processExtractedFile(char *filename, char *folderExtractTo, const char *PAKname) {
	char extractedFile[255] = "";
	int extracted = 0;
	if (is_lz4(filename)) {
		constructPath(extractedFile, folderExtractTo, PAKname, ".unlz4");
		printf("UnLZ4 %s to %s\n", filename, extractedFile);
		extracted = !decode_file(filename, extractedFile);
	} else if(check_lzo_header(filename)) {
		constructPath(extractedFile, folderExtractTo, PAKname, ".unpacked");
		printf("LZOunpack %s to %s\n", filename, extractedFile);
		extracted = !lzo_unpack((const char*) filename, (const char*) extractedFile);
	} else if(is_gzip(filename)) {
	    constructPath(extractedFile, folderExtractTo, "", "");
		printf("Ungzip %s to folder %s\n", filename, extractedFile);
		strcpy(extractedFile, file_uncompress_origname(filename, extractedFile));
		extracted=1;
	} else if (is_mtk_boot(filename)) {
		constructPath(extractedFile, folderExtractTo, "mtk_pbl.bin", "");		
		printf("Extracting primary bootloader to mtk_pbl.bin...\n");
		extract_mtk_boot(filename, extractedFile);
		printf("Extracting embedded LZHS files...\n");
		extract_lzhs(filename);
        return;
	} else if(is_cramfs_image(filename, "be")) {
	    constructPath(extractedFile, folderExtractTo, PAKname, ".cramswap");
	    printf("Swapping cramfs endian for file %s\n",filename);
	    cramswap(filename,extractedFile);
	    extracted=1;
	} else if(is_cramfs_image(filename, "le")) {
	    constructPath(extractedFile, folderExtractTo, PAKname, NULL);
	    printf("Uncramfs %s to folder %s\n", filename, extractedFile);
	    rmrf(extractedFile);
		uncramfs(extractedFile, filename);
	    return;
	} else if(is_kernel(filename)) {
		constructPath(extractedFile, folderExtractTo, PAKname, ".unpaked");
		printf("Extracting kernel %s to %s\n", filename, extractedFile);
		extract_kernel(filename, extractedFile);
		extracted = 1;
	} else if(is_nfsb(filename)) {
		constructPath(extractedFile, folderExtractTo, PAKname, ".unnfsb");
		printf("Unnfsb %s to %s\n", filename, extractedFile);
		unnfsb(filename, extractedFile);
		extracted = 1;
	} else if(is_squashfs(filename)) {
		constructPath(extractedFile, folderExtractTo, PAKname, NULL);
		printf("Unsquashfs %s to folder %s\n", filename, extractedFile);
		rmrf(extractedFile);
		unsquashfs(filename, extractedFile);
		return;
	} else if(isPartPakfile(filename)) {
		constructPath(extractedFile, folderExtractTo, PAKname, ".txt");
		printf("Saving Partition info to: %s\n", extractedFile);
		dump_partinfo(filename, extractedFile);
		return;
	} else if(is_jffs2(filename)) {
		constructPath(extractedFile, folderExtractTo, PAKname, NULL);
		printf("jffs2extract %s to folder %s\n", filename, extractedFile);
		rmrf(extractedFile);
		jffs2extract(filename, extractedFile, "1234");
	} else if (is_lzhs(filename)) {
		constructPath(extractedFile, folderExtractTo, PAKname, ".unlzhs");
		printf("Unlzhs %s to %s\n", filename, extractedFile);
		lzhs_decode(filename, extractedFile);
		extracted = 1;
    } else if (!strcmp(PAKname, "tzfw.pak") && is_elf(filename)) {
		printf("Splitting mtk tzfw...\n");
		split_mtk_tz(filename);
		extracted = 1;
	}
	if (extracted) processExtractedFile(extractedFile, folderExtractTo, PAKname);
}
