#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_FILES 32
#define MAX_SIZE 200 * 1024 * 1024 // 200 MB

// Struct to hold file information
struct File {
    char name[256];
    mode_t permissions;
    off_t size;
};

// Function to check if a file is a text file
int isTextFile(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file) {
        int c;
        while ((c = fgetc(file)) != EOF) {
            if (!(c >= 0 && c <= 127)) {
                fclose(file);
                return 0; // Not a text file
            }
        }
        fclose(file);
        return 1; // Text file
    }
    return 0; // Unable to open file
}

// Function to create the archive
void createArchive(const char *outputFile, int fileCount, struct File *files) {
    FILE *archive = fopen(outputFile, "w");
    if (archive == NULL) {
        perror("Error creating archive");
        exit(EXIT_FAILURE);
    }

    // Write contents information section
    fseek(archive, 10, SEEK_SET); // Skip space for contents information size
    for (int i = 0; i < fileCount; ++i) {
        fprintf(archive, "%s,%o,%ld|", files[i].name, files[i].permissions, files[i].size);
    }

    // Write contents information size at the beginning of the file
    long contentsInfoSize = ftell(archive);
    fseek(archive, 0, SEEK_SET);
    fprintf(archive, "%010ld", contentsInfoSize);

    // Write archived files
    for (int i = 0; i < fileCount; ++i) {
        FILE *file = fopen(files[i].name, "r");
        if (file == NULL) {
            fprintf(stderr, "Error opening file %s for archiving\n", files[i].name);
            exit(EXIT_FAILURE);
        }

        int c;
        while ((c = fgetc(file)) != EOF) {
            fputc(c, archive);
        }

        fclose(file);
    }

    fclose(archive);
}

// Function to extract the archive
void extractArchive(const char *archiveFile, const char *directory) {
    FILE *archive = fopen(archiveFile, "r");
    if (archive == NULL) {
        perror("Error opening archive");
        exit(EXIT_FAILURE);
    }

    // Read contents information size
    char contentsInfoSizeStr[11];
    fread(contentsInfoSizeStr, 1, 10, archive);
    contentsInfoSizeStr[10] = '\0';
    long contentsInfoSize = strtol(contentsInfoSizeStr, NULL, 10);

    // Read contents information
    char *contentsInfo = (char *)malloc(contentsInfoSize + 1);
    fread(contentsInfo, 1, contentsInfoSize, archive);
    contentsInfo[contentsInfoSize] = '\0';

    // Parse contents information
    char *token;
    token = strtok(contentsInfo, "|");
    while (token != NULL) {
        char fileName[256];
        mode_t permissions;
        off_t size;
        sscanf(token, "%[^,],%o,%ld", fileName, &permissions, &size);

        // Create directory if it doesn't exist
        char filePath[512];
        snprintf(filePath, sizeof(filePath), "%s/%s", directory, fileName);
        char *lastSlash = strrchr(filePath, '/');
        if (lastSlash != NULL) {
            *lastSlash = '\0';
            mkdir(filePath, 0777); // Create directory
            *lastSlash = '/';
        }

        // Write extracted file
        FILE *file = fopen(filePath, "w");
        if (file == NULL) {
            fprintf(stderr, "Error creating file %s\n", filePath);
            exit(EXIT_FAILURE);
        }

        for (off_t i = 0; i < size; ++i) {
            int c = fgetc(archive);
            if (c != EOF) {
                fputc(c, file);
            } else {
                fprintf(stderr, "Unexpected end of archive file\n");
                exit(EXIT_FAILURE);
            }
        }

        fclose(file);

        // Set file permissions
        chmod(filePath, permissions);

        token = strtok(NULL, "|");
    }

    free(contentsInfo);
    fclose(archive);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s -b file1 file2 ... -o output.sau OR %s -a archive.sau directory\n", argv[0], argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "-b") == 0) {
        // Create archive

        // Check if output file is provided
        char *outputFile = "a.sau";
        if (argc > 4 && strcmp(argv[argc - 2], "-o") == 0) {
            outputFile = argv[argc - 1];
        }

        struct File files[MAX_FILES];
        int fileCount = 0;
        off_t totalSize = 0;

        // Gather file information
        for (int i = 2; i < argc - 2; ++i) {
            if (fileCount >= MAX_FILES) {
                fprintf(stderr, "Maximum number of files exceeded\n");
                return EXIT_FAILURE;
            }

            // Check if the file exists and is a text file
            if (access(argv[i], F_OK) != -1 && isTextFile(argv[i])) {
                struct stat fileStat;
                if (stat(argv[i], &fileStat) == 0) {
                    files[fileCount].permissions = fileStat.st_mode & 0777;
                    files[fileCount].size = fileStat.st_size;
                    strncpy(files[fileCount].name, argv[i], sizeof(files[fileCount].name));
                    totalSize += files[fileCount].size;
                    ++fileCount;
                } else {
                    fprintf(stderr, "Error getting file information for %s\n", argv[i]);
                    return EXIT_FAILURE;
                }
            } else {
                fprintf(stderr, "File %s is not a valid text file\n", argv[i]);
                return EXIT_FAILURE;
            }
        }

        if (totalSize > MAX_SIZE) {
            fprintf(stderr, "Total size of input files exceeds the limit\n");
            return EXIT_FAILURE;
        }

        createArchive(outputFile, fileCount, files);
        printf("The files have been merged.\n");
    } else if (strcmp(argv[1], "-a") == 0) {
        // Extract archive

        if (argc != 4) {
            fprintf(stderr, "Invalid number of arguments for extraction\n");
            return EXIT_FAILURE;
        }

        const char *archiveFile = argv[2];
        const char *directory = argv[3];
    // Check if the archive file exists
        if (access(archiveFile, F_OK) == -1) {
            fprintf(stderr, "Archive file does not exist\n");
            return EXIT_FAILURE;
        }

        // Open archive file
        FILE *archive = fopen(archiveFile, "r");
        if (archive == NULL) {
            perror("Error opening archive");
            return EXIT_FAILURE;
        }

        // Read contents information size
        char contentsInfoSizeStr[11];
        fread(contentsInfoSizeStr, 1, 10, archive);
        contentsInfoSizeStr[10] = '\0';
        long contentsInfoSize = strtol(contentsInfoSizeStr, NULL, 10);

        // Read contents information
        char *contentsInfo = (char *)malloc(contentsInfoSize + 1);
        fread(contentsInfo, 1, contentsInfoSize, archive);
        contentsInfo[contentsInfoSize] = '\0';

        // Parse contents information
        char *token;
        token = strtok(contentsInfo, "|");
        while (token != NULL) {
            char fileName[256];
            mode_t permissions;
            off_t size;
            sscanf(token, "%[^,],%o,%ld", fileName, &permissions, &size);

            // Create file path
            char filePath[512];
            snprintf(filePath, sizeof(filePath), "%s/%s", directory, fileName);

            // Check if the directory exists, if not, create it
            struct stat st = {0};
            if (stat(directory, &st) == -1) {
                if (mkdir(directory, 0777) == -1) {
                    perror("Error creating directory");
                    return EXIT_FAILURE;
                }
            }

            // Write extracted file
            FILE *file = fopen(filePath, "w");
            if (file == NULL) {
                fprintf(stderr, "Error creating file %s\n", filePath);
                return EXIT_FAILURE;
            }
            int c;
            for (off_t i = 0; i < size; ++i) {
                c = fgetc(archive);
                if (c != EOF) {
                    fputc(c, file);
                } else {
                    fprintf(stderr, "Unexpected end of archive file\n");
                    fclose(file);
                    return EXIT_FAILURE;
                }
            }

            fclose(file);

            // Set file permissions
            chmod(filePath, permissions);
            token = strtok(NULL, "|");
        }

        free(contentsInfo);
        fclose(archive);

        printf("Files opened in the %s directory.\n", directory);
    } else {
        fprintf(stderr, "Invalid command option\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
