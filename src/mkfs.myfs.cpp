//
//  mkFs.myFs.cpp
//  myFs
//
//  Created by Oliver Waldhorst on 07.09.17.
//  Copyright © 2017 Oliver Waldhorst. All rights reserved.

#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include "myfs.h"
#include "blockdevice.h"
#include "macros.h"
#include <libgen.h>
#include <ctime>

using namespace std;

BlockDevice *blockDevice;
SuperBlock *superBlock;
MyFile *root[NUM_DIR_ENTRIES];
char dMap[DATA_BLOCKS];
int fat[DATA_BLOCKS];
unsigned int countBlocksNeed = 0;
char frame[BLOCK_SIZE];
int fd;
unsigned int blockCount = 0;

void initializeObjects() {
    blockDevice = new BlockDevice(BD_BLOCK_SIZE);
    superBlock = new SuperBlock();
    for (int i = 0; i < DATA_BLOCKS; i++) {
        dMap[i] = 'e';
        fat[i] = -1;
    }
}

int inputChecks(int argc, char *argv[]) {
    //Check if more then 64 files has been provided.
    if (argc > 2 + NUM_DIR_ENTRIES) {
        cout << "Error(to much files): You provided more then 64 files. " << endl;
        return -1;
    }
    //Check if argv contains the container file.
    if (argc < 2) {
        cout << "Error(no container file): No container file has been provided. " <<
             "Please (create and) provide the file 'container.bin'." << endl;
        return -1;
    }
    //Check if argv contains at least one file.
    if (argc < 3) {
        cout << "Error(no file): No file has been provided. " <<
             "Please (create and) provide at least one file for the file system." << endl;
        return -1;
    }
    //Check if the container file has been provided again.
    for (int i = 2; i < argc; i++) {
        if (strncmp(basename(argv[i]), "container.bin", strlen(argv[i])) == 0) {
            cout << "Error(container file again): '" << argv[i] << "' has been provided more then once."
                 << "' Please provided the container file only once." << endl;
            return -1;
        } else if (strlen(argv[i]) > FILE_NAME_MAX_LENGTH) {
            cout << "Error(file name length to long): The file name length of '" << argv[i] << "' is" << strlen(argv[i])
                 << ". Please reduce the file name length to a maximum of " << FILE_NAME_MAX_LENGTH << " characters."
                 << endl;
            return -1;
        }

    }
    //Check if there are any file duplicates.
    for (int i = 2; i < argc; i++) {
        for (int n = i + 1; n < argc; n++) {
            if (strncmp(basename(argv[i]), basename(argv[n]), strlen(argv[i])) == 0) {
                cout << "Error(duplicate file name found): '" << argv[n] << "' and '" << argv[i]
                     << "' would represent the same file in this file system." << endl;
                return -1;
            }
        }
    }
    //Check if all inserted files are accessible.
    for (int i = 2; i < argc; i++) {
        if (open(argv[i], O_RDONLY) < 0) {
            cout << "Error(cannot open file): '" << argv[i]
                 << "' is not accessible. Please provide this file in an accessible mode." << endl;
            return -1;
        }
    }
    //Check if the correct container file has been provided, asks for the correct container file or create a
    //container file if argv contains no container file.
    if (strncmp(argv[1], "container.bin", strlen(argv[1])) == 0) {
        blockDevice->create(argv[1]);
    } else {
        fd = open("container.bin", O_CREAT);
        if (fd > 0) {
            close(fd);
            blockDevice->create("container.bin");
        } else {
            cout << "Error(wrong container file): You inserted '" << argv[1]
                 << "'. Please (create and) provide the file 'container.bin'." << endl;
            return -1;
        }
    }
    //Checks if all files combined are not greater then 30,1 MB
    ssize_t ret = 512;
    int fileSizes = 0;
    for (int j = 2; j < argc; j++) {
        fd = open(argv[j], O_RDONLY);
        if (fd < 0) {
            cout << "Error(cannot open file): '" << argv[j]
                 << "' is not accessible. Please provide this file in an accessible mode." << endl;
            return -errno;
        }
        while (ret == 512) {
            ret = read(fd, frame, sizeof(frame));
            fileSizes += ret;
        }
        if (ret < 0) {
            cout << "Error" << endl;
            return -errno;
        } else if (ret < BLOCK_SIZE) {
            close(fd);
            ret = 512;
        }
    }
    if (fileSizes > FILE_SYSTEM_MAX_DATA_SIZE_IN_MB) {
        cout << "Error(file system size overflow): Your files are combined "
             << fileSizes - FILE_SYSTEM_MAX_DATA_SIZE_IN_MB
             << " Byte(s) greater then the maximum file system size of 30,1 MB."
             << endl;
        return -1;
    }
    return 0;
}

void writeSuperBlockToContainer() {
    memcpy(frame, (char *) superBlock, sizeof(SuperBlock));
    blockDevice->write(0, frame);
}

void writeDMapAndFatToContainer() {
    char *writeFrame = (char *) dMap;
    for (int i = D_MAP_BLOCK_INDEX_START; i < FAT_BLOCK_INDEX_START; i++) {
        blockDevice->write(i, writeFrame);
        writeFrame += 512;
    }
    writeFrame = (char *) fat;
    for (unsigned int i = FAT_BLOCK_INDEX_START; i < ROOT_BLOCK_INDEX_START; i++) {
        blockDevice->write(i, writeFrame);
        writeFrame += 512;
    }
}

void writeRootToContainer(int argc) {
    for (int i = 0, j = ROOT_BLOCK_INDEX_START; i < argc - 2; i++, j++) {
        memcpy(frame, (char *) root[i], sizeof(MyFile));
        blockDevice->write(j, frame);
    }
}

int writeFilesToContainer(int argc, char *argv[]) {
    ssize_t ret = BLOCK_SIZE;
    int emptyFileDetected = 1;
    for (int i = 0, j = 2; j < argc; i++, j++) {
        root[i] = new MyFile();
        root[i]->setFirstDataBlockIndex(blockCount);
        root[i]->setOpenIndex(-1);
        fd = open(argv[j], O_RDONLY);
        if (fd < 0) {
            cout << "Error opening file " << argv[j] << endl;
            return -errno;
        }
        while (ret == BLOCK_SIZE) {
            ret = read(fd, frame, BLOCK_SIZE);
            if (ret < 0) {
                cout << "Error reading from file " << argv[j] << endl;
                return -errno;
            } else if (ret != 0) {
                blockDevice->write(DATA_BLOCKS_INDEX_START + blockCount, frame);
                dMap[blockCount] = 'f';
                fat[blockCount] = blockCount + 1;
                blockCount++;
                countBlocksNeed++;
                emptyFileDetected = 0;
            }

            if (ret < BLOCK_SIZE || ret == 0) {
                cout << "File " << j - 1 << "(" << argv[j]
                     << "): File end reached. File saved on container.bin. CountBlockNeeded: " << countBlocksNeed
                     << endl;
                close(fd);
            }
        }
        if (blockCount > 0) {
            fat[blockCount - 1] = -1;
        }
        //Fill root information.
        root[i]->setFileName(basename(argv[j]));
        if (emptyFileDetected == 1) {
            root[i]->setFirstDataBlockIndex(-1);
            root[i]->setFileSize(0);
            emptyFileDetected = 0;
        } else if (ret == 0) {
            root[i]->setFileSize((countBlocksNeed) * BLOCK_SIZE);
        } else {
            root[i]->setFileSize(((countBlocksNeed - 1) * BLOCK_SIZE) + ret);
        }
        root[i]->setUserID(getuid());
        root[i]->setGroupID(getgid());
        root[i]->setMode(S_IFREG | 0444);
        struct stat stat1{};
        stat(argv[j], &stat1);
        root[i]->setATime(stat1.st_atim.tv_sec);
        root[i]->setMTime(stat1.st_mtim.tv_sec);
        root[i]->setCTime(stat1.st_ctim.tv_sec);
        ret = 512;
        countBlocksNeed = 0;
        superBlock->addFile();
    }
    writeSuperBlockToContainer();
    writeDMapAndFatToContainer();
    writeRootToContainer(argc);
    blockDevice->read(SUPER_BLOCK_BLOCK_INDEX_START, frame);
    memcpy((char *) superBlock, frame, sizeof(SuperBlock));
    blockDevice->close();
    return 0;
}

void printSuperBlockInfo(int print, SuperBlock *sBlock) {
    if (print == 1) {
        cout << endl << "SuperBlock: " << endl <<
             "SuperBlockIndexStart: " << sBlock->getSuperBlockIndexStart() << endl <<
             "FileSystemSize: " << sBlock->getFileSystemSize() << endl <<
             "DMApBlockStart: " << sBlock->getDMapBlockIndexStart() << endl <<
             "FATBlockStart: " << sBlock->getFatBlockIndexStart() << endl <<
             "RootBlockStart: " << sBlock->getRootBlockIndexStart() << endl <<
             "FileCount: " << sBlock->getFileCount() << endl << endl;
    }
}

void printDMapAndFat(int print) {
    if (print == 1) {
        for (unsigned int i = 0; i < 65536; i++) {
            cout << "Index: " << i << ", DMap-Value: " << dMap[i] << " Fat-Value: " << fat[i] << endl;
        }
    }
}

void printRootFileInfos(int print, int argc) {
    int dataBlocks = 0;
    if (print == 1) {
        for (int i = 0; i < argc - 2; i++) {
            cout << "Root " << i << ": " << endl;
            cout <<
                 "Filename: " << root[i]->getFileName() << endl <<
                 "FileSize: " << root[i]->getFileSize() << endl <<
                 "UserID: " << root[i]->getUserID() << endl <<
                 "GroupID: " << root[i]->getGroupID() << endl <<
                 "Mode: " << root[i]->getMode() << endl <<
                 "ATime: " << root[i]->getATime() << endl <<
                 "MTime: " << root[i]->getMTime() << endl <<
                 "CTime: " << root[i]->getCTime() << endl <<
                 "FirstDataBlockIndex: " << root[i]->getFirstDataBlockIndex() << endl;
            if (root[i]->getFileSize() % 512 != 0) {
                dataBlocks = ((int) (root[i]->getFileSize() / 512) + 1);
            } else {
                dataBlocks = ((int) (root[i]->getFileSize() / 512));
            }
            cout << "LastDataBlockIndex: " << (dataBlocks + root[i]->getFirstDataBlockIndex()) << endl <<
                 "UsedDataBlocks: " << dataBlocks << endl <<
                 "FreeDataBlocks: "
                 << ((FILE_SYSTEM_MAX_DATA_SIZE_IN_MiB / 512) - (dataBlocks + root[i]->getFirstDataBlockIndex()))
                 << endl << endl;

        }
    }
}

int main(int argc, char *argv[]) {
    initializeObjects();
    if (inputChecks(argc, argv) < 0) {
        return -1;
    }
    int writeFilesRet = writeFilesToContainer(argc, argv);
    printSuperBlockInfo(1, superBlock);
    printDMapAndFat(0);
    printRootFileInfos(1, argc);
    return writeFilesRet;
}