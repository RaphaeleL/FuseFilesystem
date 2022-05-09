//
//  myFs.cpp
//  myFs
//
//  Created by Oliver Waldhorst on 02.08.17.
//  Copyright © 2017 Oliver Waldhorst. All rights reserved.
//

// The functions fuseGettattr(), fuseRead(), and fuseReadDir() are taken from
// an example by Mohammed Q. Hussain. Here are original copyrights & licence:

/**
 * Simple & Stupid Filesystem.
 *
 * Mohammed Q. Hussain - http://www.maastaar.net
 *
 * This is an example of using FUSE to build a simple filesystem. It is a part of a tutorial in MQH Blog with the title "Writing a Simple Filesystem Using FUSE in C": http://www.maastaar.net/fuse/linux/filesystem/c/2016/05/21/writing-a-simple-filesystem-using-fuse/
 *
 * License: GNU GPL
 */

// For documentation of FUSE methods see https://libfuse.github.io/doxygen/structfuse__operations.html

#undef DEBUG

#define DEBUG
#define DEBUG_METHODS
#define DEBUG_RETURN_VALUES

#include <unistd.h>
#include <string.h>
#include <cerrno>
#include <iostream>

#include "macros.h"
#include "myfs.h"
#include "myfs-info.h"
#include "myfs-structs.h"

using namespace std;

SuperBlock::SuperBlock() {}

SuperBlock::~SuperBlock() {}

MyFile::MyFile() {}

MyFile::~MyFile() {}

MyFS *MyFS::_instance = NULL;

MyFS *MyFS::Instance() {
    if (_instance == NULL) {
        _instance = new MyFS();
    }
    return _instance;
}

MyFS::MyFS() {
    this->logFile = stderr;
    blockDevice = new BlockDevice(BD_BLOCK_SIZE);
    superBlock = new SuperBlock();
}

MyFS::~MyFS() {}

//Implemented methods
/**
 * This method is called for setting the attributes of a file or a directory.
 * @param path of directory or file
 * @param statBuf should contain all attributes of a file or a directory
 * @return 0 for success or a negative error value
 */
int MyFS::fuseGetattr(const char *path, struct stat *statBuf) {
    LogM();
    // TODO: fuseGetattr
    char *file = clearPath(path);
    //LogF("\tAttributes of %s requested\n", path);

    // GNU's definitions of the attributes (http://www.gnu.org/software/libc/manual/html_node/Attribute-Meanings.html):
    // 		st_uid: 	The user ID of the file’s owner.
    //		st_gid: 	The group ID of the file.
    //		st_atime: 	This is the last access time for the file.
    //		st_mtime: 	This is the time of the last modification to the contents of the file.
    //		st_mode: 	Specifies the mode of the file. This includes file type information (see Testing File Type) and the file permission bits (see Permission Bits).
    //		st_nlink: 	The number of hard links to the file. This count keeps track of how many directories have entries for this file. If the count is ever decremented to zero, then the file itself is discarded as soon
    //						as no process still holds it open. Symbolic links are not counted in the total.
    //		st_size:	This specifies the size of a regular file in bytes. For files that are really devices this field isn’t usually meaningful. For symbolic links this specifies the length of the file name the link refers to.
    statBuf->st_uid = getuid(); // The owner of the file/directory is the user who mounted the filesystem
    statBuf->st_gid = getgid(); // The group of the file/directory is the same as the group of the user who mounted the filesystem
    if (strcmp(path, "/") == 0) {
        statBuf->st_mode = S_IFDIR | 0555;
        statBuf->st_nlink = 2; // Why "two" hardlinks instead of "one"? The answer is here: http://unix.stackexchange.com/a/101536
    } else {
        unsigned int found = 0;
        for (unsigned int i = 0; i < NUM_DIR_ENTRIES; i++) {
            if (strcmp(file, root[i]->getFileName()) == 0 && hasRootIndexAFile[i] == 1) {
                found++;
                statBuf->st_mode = root[i]->getMode();
                statBuf->st_nlink = 1;
                statBuf->st_size = root[i]->getFileSize();
                statBuf->st_atime = root[i]->getATime();
                statBuf->st_mtime = root[i]->getMTime();
                statBuf->st_ctime = root[i]->getCTime();
            }
        }
        if (found < 1) {
            RETURN(-ENOENT)
        }
    }
    free(file);
    RETURN(0)
}

/**
 * This method is called if a new file should be created.
 * @param path of new file
 * @param mode of new file
 * @param dev optional
 * @return 0 for success or a negative error value
 */
int MyFS::fuseMkNod(const char *path, mode_t mode, dev_t dev) {
    // TODO: fuseMkNod
    LogM();
    int returnValue = 0;
    char *clearedPath = clearPath(path);
    LogF("Path %s", clearedPath);
    //Error detection
    if (superBlock->getFileCount() >= NUM_DIR_ENTRIES ||
        this->currentFileSystemSize >= FILE_SYSTEM_MAX_DATA_SIZE_IN_MB) {
        returnValue = -ENOSPC;
    } else if (checkFileIfNotUsed(clearedPath) == -1) {
        returnValue = -EEXIST;
    }
    //Creating and initializing a new file
    if (returnValue >= 0) {
        auto *file = new MyFile();
        file->setOpenIndex(-1);
        file->setFirstDataBlockIndex(-1);
        file->setFileName(clearedPath);
        file->setFileSize(0);
        file->setUserID(getuid());
        file->setGroupID(getgid());
        file->setMode(mode);
        file->setATime(time(nullptr));
        file->setMTime(time(nullptr));
        file->setCTime(time(nullptr));

        int fileIndex = findFreeRootIndex();
        root[fileIndex] = file;
        hasRootIndexAFile[fileIndex] = 1;
        superBlock->addFile();
    }
    free(clearedPath);
    RETURN(returnValue)
}

/**
 * This method is called if a file should be deleted.
 * @param path of file
 * @return 0 for success or a negative error value
 */
int MyFS::fuseUnlink(const char *path) {
    // TODO: fuseUnlink
    LogM();
    int returnValue = 0;
    char *clearedPath = clearPath(path);
    int fileIndex = -1;
    MyFile *file;
    int firstDataBlock;
    int saveLastBlock;
    for (int i = 0; i < NUM_DIR_ENTRIES; i++) {
        if (strcmp(root[i]->getFileName(), clearedPath) == 0) {
            fileIndex = i;
            break;
        }
    }
    if (fileIndex < 0) {
        returnValue = -ENOENT;
    } else {
        file = root[fileIndex];
        firstDataBlock = file->getFirstDataBlockIndex();
        while (firstDataBlock != -1) {
            saveLastBlock = firstDataBlock;
            firstDataBlock = fat[firstDataBlock];
            fat[saveLastBlock] = -1;
            dMap[saveLastBlock] = 'e';
        }
        hasRootIndexAFile[fileIndex] = 0;
        LogF("Path: %s", clearedPath);
        LogF("File index: %d", fileIndex);
        LogF("First data block index: %d", firstDataBlock);
        LogF("Opened file index  %d", file->getOpenIndex());
        LogF("Current file system size: %lu", currentFileSystemSize);
        LogF("Open files:  %hu", this->openFiles);
        if (file->getOpenIndex() >= 0) {
            openFilesArray[fileIndex] = -1;
            this->openFiles--;
        }
        currentFileSystemSize -= file->getFileSize();
        LogF("Open files now: %hu", openFiles);
        LogF("File size  %d", file->getFileSize());
        LogF("New current file system size: %lu", currentFileSystemSize);
        LogF("File count old: %d", superBlock->getFileCount());
        superBlock->removeFile();
        LogF("File count new: %d", superBlock->getFileCount());
    }
    logDMapAndFatInfos(0);
    RETURN(returnValue)
}

/**
 * This method is called if a file should be opened.
 * @param path of file
 * @param fileInfo contains a file handle, the file handle will get the root Index of the file
 * @return 0 for success or a negative error value
 */
int MyFS::fuseOpen(const char *path, struct fuse_file_info *fileInfo) {
    // TODO: fuseOpen
    LogM();
    int returnValue = 0;
    int fileFound = 0;
    char *clearedPath;
    //File handle -1 as standard for error case
    fileInfo->fh = -1;
    clearedPath = clearPath(path);
    if (this->openFiles > NUM_OPEN_FILES) {
        returnValue = -EMFILE;
    }
    //Setting file handle for an existing file which has been opened (Opened=0)
    for (uint16_t i = 0; i < NUM_DIR_ENTRIES && returnValue >= 0; i++) {
        if (strcmp(root[i]->getFileName(), clearedPath) == 0) {
            if ((getuid() == root[i]->getUserID() ||
                 getgid() == root[i]->getGroupID()) && root[i]->getOpenIndex() == -1) {
                fileInfo->fh = i;
                root[i]->setOpenIndex(openFiles);
                LogF("File handle:  %hu", i);
                LogF("File open index:  %d", openFiles);
                this->openFiles++;
                LogF("Open files now: %hu", openFiles);
                fileFound++;
            } else {
                returnValue = -EACCES;
            }
            break;
        }
    }
    if (fileFound == 0) {
        returnValue = -ENOENT;
    }
    LogF("File %s has been opened.", clearedPath);
    free(clearedPath);
    RETURN(returnValue)
}

/**
 * This method reads the content of a file and writes the requested content into the provided buffer.
 * @param path of file
 * @param buf buffer
 * @param size requested content size
 * @param offset requested offset of the content
 * @param fileInfo contains a file handle, the file handle will get the root Index of the file
 * @return read bytes for success or a negative error value
 */
int MyFS::fuseRead(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo) {
    // TODO: fuseRead
    LogM();
    int returnValue = 1;
    int rootIndex = fileInfo->fh;
    int countDataBlocksInvolved = 0;
    unsigned int firstDataBlockRest = 0;
    unsigned int lastDataBlockRest = 0;
    unsigned int copySize = 512;
    unsigned int countBytes = 0;
    char *clearedPath = clearPath(path);
    char *writeFrame = new char[512];
    char *frameCopy = writeFrame;
    char *bufCopy = buf;
    MyFile *file;
    LogF("Root index: %d", rootIndex);
    file = root[rootIndex];
    int firstDataBlock = file->getFirstDataBlockIndex();
    LogF("First data block index: %d", firstDataBlock);
    LogF("File size: %d", file->getFileSize());
    LogF("Path: %s", clearedPath);
    LogF("Size: %zu", size);
    LogF("Offset: %ld", offset);

    //Error detection
    if (rootIndex > NUM_DIR_ENTRIES - 1 || rootIndex < 0) {
        returnValue = -EBADF;
    } else if (size == 0 || file->getFileSize() == 0) {
        returnValue = 0;
    } else if (file->getFileSize() == 0 || file->getFileSize() < offset || offset < 0) {
        returnValue = -ENXIO;
    }
    if (returnValue > 0) {
        //Finding the data block for the requested content
        for (unsigned int k = 0; k < (offset / BLOCK_SIZE) && fat[firstDataBlock] != -1; k++) {
            firstDataBlock = fat[firstDataBlock];
        }
        //Calculation the first rest, involved data blocks and the last rest, they will be used for copying the content
        firstDataBlockRest = (offset % BLOCK_SIZE);
        countDataBlocksInvolved = size / BLOCK_SIZE;
        lastDataBlockRest = (offset + size - firstDataBlockRest) % BLOCK_SIZE;
        if (file->getFileSize() < BLOCK_SIZE) {
            countDataBlocksInvolved = 1;
            lastDataBlockRest = BLOCK_SIZE - file->getFileSize();
        }
        LogF("First data block rest: %d", firstDataBlockRest);
        LogF("countDataBlocksInvolved: %d", countDataBlocksInvolved);
        LogF("Last data block rest: %d", lastDataBlockRest);

        //Copying the requested content into buf
        for (int i = firstDataBlock, j = 0; countBytes < file->getFileSize() && i != -1 && j < countDataBlocksInvolved;
             i = fat[i], j++) {
            //Using cached last read block or loading a new one
            if (i == lastBlockRead) {
                memcpy(frameCopy, lastBlockReadFrame + (BLOCK_SIZE * file->getOpenIndex()), BLOCK_SIZE);
            } else {
                blockDevice->read(DATA_BLOCKS_INDEX_START + i, frameCopy);
            }
            //Changing the copySize if necessary because of not proportional requested offSet and or size
            if (i == firstDataBlock) {
                frameCopy += (offset % BLOCK_SIZE);
                copySize = BLOCK_SIZE - (offset % BLOCK_SIZE);
            }
            if (file->getFileSize() - offset < copySize) {
                copySize = file->getFileSize() - offset;
            }
            if (size - countBytes < copySize) {
                copySize = size - countBytes;
            }
            memcpy(bufCopy, frameCopy, copySize);
            bufCopy += copySize;
            countBytes += copySize;
            frameCopy = writeFrame;
            lastBlockRead = i;
            copySize = 512;
        }
        //Caching last read data block
        memcpy(lastBlockReadFrame + (BLOCK_SIZE * file->getOpenIndex()), frameCopy, BLOCK_SIZE);
        if (offset + size > file->getFileSize()) {
            errno = ENXIO;
        }
        file->setATime(time(nullptr));
        free(writeFrame);
        free(clearedPath);
        returnValue = countBytes;
    }
    LogF("lastBlockRead %d", lastBlockRead);
    RETURN(returnValue)
}

/**
 * This method writes provided content in the buffer into a file.
 * @param path of file
 * @param buf buffer
 * @param size provided content size
 * @param offset provided offset of the content
 * @param fileInfo contains a file handle, the file handle will get the root Index of the file
 * @return written bytes for success or a negative error value
 */
int MyFS::fuseWrite(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo) {
    // TODO: fuseWrite
    int returnValue = 1;
    int rootIndex = fileInfo->fh;
    char *clearedPath = clearPath(path);
    int firstDataBlock;
    int countMoreBytesNeeded = 0;
    int saveFirstDataBlock = -1;
    unsigned int lastWrittenDataBlock = 0;
    unsigned int copySize = BLOCK_SIZE;
    unsigned int countBytes = 0;
    char *writeFrame = new char[BLOCK_SIZE];
    char *frameCopy = writeFrame;
    char *bufCopy = new char[size];
    MyFile *file = root[rootIndex];

    //Error detection
    if (rootIndex > NUM_DIR_ENTRIES - 1 || rootIndex < 0) {
        returnValue = -EBADF;
    } else if (size == 0) {
        returnValue = 0;
    } else if (currentFileSystemSize >= superBlock->getFileSystemSize()) {
        returnValue = -ENOSPC;
    } else if (offset > file->getFileSize()) {
        returnValue = -ENXIO;
    }
    firstDataBlock = file->getFirstDataBlockIndex();
    //Beginning logs
    LogM();
    LogF("Path %s", clearedPath);
    LogF("Size: %zu", size);
    LogF("Offset: %ld", offset);
    LogF("RootIndex: %d", rootIndex);
    LogF("File size: %d", file->getFileSize());
    LogF("First data block: %d", firstDataBlock);
    LogF("Current file system size: %lu", currentFileSystemSize);
    LogF("File system size: %lu", superBlock->getFileSystemSize());

    //Enter only if returnValue greater then 0
    if (returnValue > 0) {
        //Copy the provided content into the copy array
        memcpy(bufCopy, buf, size);
        //Case if file is empty and has no dataBlock
        if (file->getFileSize() == 0 && firstDataBlock == -1) {
            //Offset must be 0
            if (offset != 0) {
                returnValue = -ENXIO;
            } else {
                firstDataBlock = assignFreeDataBlock();
                LogF("First data block: %d", firstDataBlock);
                file->setFirstDataBlockIndex(firstDataBlock);

                for (int j = 0; size > countBytes && currentFileSystemSize < superBlock->getFileSystemSize() &&
                                firstDataBlock != -1; j++) {
                    //Changing copySize if 512 Byte are to much
                    if (size - countBytes < BLOCK_SIZE) {
                        copySize = size - countBytes;
                    }
                    //Changing copySize if file system has less then 512 byte left
                    if (BLOCK_SIZE + currentFileSystemSize > superBlock->getFileSystemSize()) {
                        copySize = superBlock->getFileSystemSize() - currentFileSystemSize;
                    }
                    //Copy content into frameCopy for saving it onto the block device
                    memcpy(frameCopy, bufCopy, copySize);
                    currentFileSystemSize += copySize;
                    bufCopy += copySize;
                    countBytes += copySize;
                    frameCopy = writeFrame;
                    //Caching the last written block
                    lastBlockWritten = firstDataBlock;
                    lastBlockRead = lastBlockWritten;
                    memcpy(lastBlockReadFrame + (BLOCK_SIZE * file->getOpenIndex()), frameCopy, BLOCK_SIZE);
                    memcpy(lastBlockWritenFrame + (BLOCK_SIZE * file->getOpenIndex()), frameCopy, BLOCK_SIZE);
                    //write content onto block device
                    blockDevice->write(DATA_BLOCKS_INDEX_START + firstDataBlock, frameCopy);
                    //Resetting copySize to 512
                    copySize = BLOCK_SIZE;
                    //assign new data block and updating next data block of old last data block
                    lastWrittenDataBlock = firstDataBlock;
                    firstDataBlock = assignFreeDataBlock();
                    fat[lastWrittenDataBlock] = firstDataBlock;
                    //Set errno if no space left signaled with freeDataBlockIndex is -1
                    if (firstDataBlock == -1) {
                        errno = -ENOSPC;
                    }
                }
                //Updating meta information of the file
                file->setFileSize(countBytes);
                // reset last assigned data block if no new data block was needed if the
                // written bytes has been proportional to 2^n
                if (returnValue % BLOCK_SIZE == 0) {
                    dMap[fat[lastWrittenDataBlock]] = 'e';
                    fat[lastWrittenDataBlock] = -1;
                }
            }
            //Case if content of a file should be added to the end
        } else if (offset == file->getFileSize()) {
            //Getting the last data block
            for (unsigned int k = 0; k < (offset / BLOCK_SIZE) && firstDataBlock >= 0; k++) {
                saveFirstDataBlock = firstDataBlock;
                firstDataBlock = fat[firstDataBlock];
            }
            LogF("firstDataBlock: %d", firstDataBlock);

            // Case if the old file size was proportional to 2^n
            if (firstDataBlock == -1) {
                firstDataBlock = assignFreeDataBlock();
                //Case if no new data block is left
                if (firstDataBlock == -1) {
                    returnValue = -ENOSPC;
                } else {
                    //updating next data block of old last data block
                    fat[saveFirstDataBlock] = firstDataBlock;
                    for (int j = 0; size > countBytes && currentFileSystemSize < superBlock->getFileSystemSize() &&
                                    firstDataBlock != -1; j++) {
                        //not necessary because the offset is proportional to 2^n
                        if (j == 0) {
                            frameCopy += offset % BLOCK_SIZE;
                            copySize = BLOCK_SIZE - (offset % BLOCK_SIZE);
                        }
                        //Changing copySize if less then 512 are needed
                        if (size - countBytes < BLOCK_SIZE) {
                            copySize = size - countBytes;
                        }
                        //Changing copySize if file system has less then 512 byte left
                        if (copySize + currentFileSystemSize > superBlock->getFileSystemSize()) {
                            copySize = superBlock->getFileSystemSize() - currentFileSystemSize;
                            errno = -ENOSPC;
                        }
                        //Copy content into frameCopy for saving it onto the block device
                        memcpy(frameCopy, bufCopy, copySize);
                        currentFileSystemSize += copySize;
                        bufCopy += copySize;
                        countBytes += copySize;
                        frameCopy = writeFrame;
                        //Caching the last written block
                        lastBlockWritten = saveFirstDataBlock;
                        lastBlockRead = lastBlockWritten;
                        memcpy(lastBlockReadFrame + (BLOCK_SIZE * file->getOpenIndex()), frameCopy, BLOCK_SIZE);
                        memcpy(lastBlockWritenFrame + (BLOCK_SIZE * file->getOpenIndex()), frameCopy, BLOCK_SIZE);
                        //write content onto block device
                        blockDevice->write(DATA_BLOCKS_INDEX_START + firstDataBlock, frameCopy);
                        //Resetting copySize to 512
                        copySize = 512;
                        //assign new data block and updating next data block of old last data block
                        lastWrittenDataBlock = firstDataBlock;
                        firstDataBlock = assignFreeDataBlock();
                        fat[lastWrittenDataBlock] = firstDataBlock;
                        if (firstDataBlock == -1) {
                            errno = -ENOSPC;
                        }
                    }
                    //Updating meta information of the file
                    file->setFileSize(file->getFileSize() + countBytes);
                }
            } else {
                for (int j = 0; size > countBytes && currentFileSystemSize < superBlock->getFileSystemSize() &&
                                firstDataBlock != -1; j++) {
                    //Get cached content or load it from the block device
                    if (firstDataBlock == lastBlockWritten) {
                        memcpy(frameCopy, lastBlockWritenFrame + (BLOCK_SIZE * file->getOpenIndex()), BLOCK_SIZE);
                    } else {
                        blockDevice->read(DATA_BLOCKS_INDEX_START + firstDataBlock, frameCopy);
                    }
                    //Changing copySize if 512 Byte are to much
                    if (j == 0) {
                        frameCopy += (offset % BLOCK_SIZE);
                        copySize = BLOCK_SIZE - (offset % BLOCK_SIZE);
                    }
                    //Changing copySize if less then 512 are needed
                    if (copySize > size - countBytes) {
                        copySize = size - countBytes;
                    }
                    //Changing copySize if file system has less then 512 byte left
                    if (currentFileSystemSize + copySize > superBlock->getFileSystemSize()) {
                        copySize = superBlock->getFileSystemSize() - currentFileSystemSize;
                    }
                    //Copy content into frameCopy for saving it onto the block device
                    memcpy(frameCopy, bufCopy, copySize);
                    currentFileSystemSize += copySize;
                    bufCopy += copySize;
                    countBytes += copySize;
                    frameCopy = writeFrame;
                    //Caching the last written block
                    lastBlockWritten = firstDataBlock;
                    lastBlockRead = lastBlockWritten;
                    memcpy(lastBlockReadFrame + (BLOCK_SIZE * file->getOpenIndex()), frameCopy, BLOCK_SIZE);
                    memcpy(lastBlockWritenFrame + (BLOCK_SIZE * file->getOpenIndex()), frameCopy, BLOCK_SIZE);
                    //write content onto block device
                    blockDevice->write(DATA_BLOCKS_INDEX_START + firstDataBlock, frameCopy);
                    //Resetting copySize to 512
                    copySize = 512;
                    //assign new data block and updating next data block of old last data block
                    lastWrittenDataBlock = firstDataBlock;
                    firstDataBlock = assignFreeDataBlock();
                    fat[lastWrittenDataBlock] = firstDataBlock;
                    if (firstDataBlock == -1) {
                        errno = -ENOSPC;
                    }
                }
                //Updating meta information of the file
                file->setFileSize(file->getFileSize() + countBytes);
                // reset last assigned data block if no new data block was needed if the
                // written bytes has been proportional to 2^n
                if (returnValue % BLOCK_SIZE == 0) {
                    dMap[fat[lastWrittenDataBlock]] = 'e';
                    fat[lastWrittenDataBlock] = -1;
                }
            }
        } else if (offset < file->getFileSize()) {
            for (unsigned int k = 0; k < (offset / BLOCK_SIZE) && firstDataBlock >= 0; k++) {
                firstDataBlock = fat[firstDataBlock];
            }
            LogF("firstDataBlock: %d", firstDataBlock);

            for (int j = 0; size > countBytes && currentFileSystemSize < superBlock->getFileSystemSize() &&
                            firstDataBlock != -1; j++) {
                if (firstDataBlock == lastBlockWritten) {
                    memcpy(frameCopy, lastBlockWritenFrame + (BLOCK_SIZE * file->getOpenIndex()), BLOCK_SIZE);
                } else {
                    blockDevice->read(DATA_BLOCKS_INDEX_START + firstDataBlock, frameCopy);
                }
                //Changing copySize if 512 Byte are to much
                if (j == 0) {
                    frameCopy += (offset % BLOCK_SIZE);
                    copySize = BLOCK_SIZE - (offset % BLOCK_SIZE);
                }
                //Changing copySize if less then 512 are needed
                if (copySize > size - countBytes) {
                    copySize = size - countBytes;
                }
                //Changing copySize if file system has less then 512 byte left
                if (currentFileSystemSize + copySize > superBlock->getFileSystemSize()) {
                    copySize = superBlock->getFileSystemSize() - currentFileSystemSize;
                }
                //Copy content into frameCopy for saving it onto the block device
                memcpy(frameCopy, bufCopy, copySize);
                currentFileSystemSize += copySize;
                bufCopy += copySize;
                countBytes += copySize;
                frameCopy = writeFrame;
                //Caching the last written block
                lastBlockWritten = firstDataBlock;
                lastBlockRead = lastBlockWritten;
                memcpy(lastBlockReadFrame + (BLOCK_SIZE * file->getOpenIndex()), frameCopy, BLOCK_SIZE);
                memcpy(lastBlockWritenFrame + (BLOCK_SIZE * file->getOpenIndex()), frameCopy, BLOCK_SIZE);
                //write content onto block device
                blockDevice->write(DATA_BLOCKS_INDEX_START + firstDataBlock, frameCopy);
                //Resetting copySize to 512
                copySize = BLOCK_SIZE;
                //Adding a data block if there are additional bytes added at the end of the file
                lastWrittenDataBlock = firstDataBlock;
                if (fat[firstDataBlock] == -1) {
                    firstDataBlock = assignFreeDataBlock();
                    if (size - countBytes < BLOCK_SIZE) {
                        countMoreBytesNeeded += size - countBytes;
                    } else {
                        countMoreBytesNeeded += BLOCK_SIZE;
                    }
                }
                //updating next data block of old last data block
                fat[lastWrittenDataBlock] = firstDataBlock;
                if (firstDataBlock == -1) {
                    errno = -ENOSPC;
                }
            }
            //Updating meta information of the file
            file->setFileSize(file->getFileSize() + countMoreBytesNeeded);
            // reset last assigned data block if no new data block was needed if the
            // written bytes has been proportional to 2^n
            if (returnValue % BLOCK_SIZE == 0) {
                dMap[fat[lastWrittenDataBlock]] = 'e';
                fat[lastWrittenDataBlock] = -1;
            }
        }
        //Updating meta information of the file
        LogF("File size at the end of writing: %d", file->getFileSize());
        LogF("CountBytes: %d", countBytes);
        file->setATime(time(nullptr));
        file->setMTime(time(nullptr));
        if (returnValue > 0) {
            returnValue = countBytes;
        }
    }
    //Information logging after writing
    LogF("Current file system size after writing: %lu", currentFileSystemSize);
    LogF("last block read: %d", lastWrittenDataBlock);
    free(writeFrame);
    RETURN(returnValue)
}

/**
 * This method is called if a file should be released.
 * @param path of file
 * @param fileInfo with the root file index registered on the file handle
 * @return 0 for success or a negative error value
 */
int MyFS::fuseRelease(const char *path, struct fuse_file_info *fileInfo) {
    // TODO: fuseRelease
    LogM();
    if (fileInfo->fh >= 0) {
        root[fileInfo->fh]->clearOpenIndex();
        this->openFiles--;
        RETURN(0)
    } else {
        RETURN(-ENOENT)
    }
}

/**
 * This method is called for reading from the root directory all available files and print them to the console.
 * @param path which contains all files and directories
 * @param buf with file and directory attributes
 * @param filler fills the attributes for a file or directory
 * @param offset
 * @param fileInfo with the root file index registered on the file handle
 * @return 0 for success or a negative error value
 */
int
MyFS::fuseReaddir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fileInfo) {
    // TODO: fuseReaddir
    LogM();
    LogF("--> Getting the List of files of %s\n", path);
    // Current Directory
    filler(buf, ".", NULL, 0);
    // Parent Directory
    filler(buf, "..", NULL, 0);
    // If the user is trying to show the files/directories of the root directory show the following
    if (strcmp(path, "/") == 0) {
        for (unsigned int i = 0; i < NUM_DIR_ENTRIES; i++) {
            if (hasRootIndexAFile[i] == 1 && filler(buf, root[i]->getFileName(), NULL, 0) == 1) {
                LogF("buffer is full, size of buffer: %ld", sizeof(buf));
            }
        }
    } else {
        RETURN(-ENOTDIR)
    }
    RETURN(0)
}

/**
 * This method is called at the beginning for initializing the file system.
 * @param conn
 */
void *MyFS::fuseInit(struct fuse_conn_info *conn) {
    // TODO: fuseInit
    int ret;
    char *copy;
    char frame[512];
    // Open logfile
    this->logFile = fopen(((MyFsInfo *) fuse_get_context()->private_data)->logFile, "w+");
    if (this->logFile == NULL) {
        fprintf(stderr, "ERROR: Cannot open logfile %s\n",
                ((MyFsInfo *) fuse_get_context()->private_data)->logFile);
    } else {
        //this->logFile= reinterpret_cast<FILE *>(((MyFsInfo *) fuse_get_context()->private_data)->logFile);
        // turn of logfile buffering
        setvbuf(this->logFile, NULL, _IOLBF, 0);
        LogM();
        LOG("Starting logging...\n");
        // you can get the contain file name here:
        LogF("Container file name: %s", ((MyFsInfo *) fuse_get_context()->private_data)->contFile);

        ret = blockDevice->open(((MyFsInfo *) fuse_get_context()->private_data)->contFile);
        LogF("Return wert of opening container file: %d", ret);
        if (ret >= 0) {
            //Initializing superBlock
            copy = (char *) superBlock;
            blockDevice->read(0, frame);
            memcpy(copy, frame, sizeof(SuperBlock));
            //Initializing DMap
            copy = dMap;
            for (unsigned int i = D_MAP_BLOCK_INDEX_START; i < FAT_BLOCK_INDEX_START; i++, copy += BLOCK_SIZE) {
                blockDevice->read(i, copy);
            }
            //Initializing Fat
            copy = (char *) fat;
            for (int i = FAT_BLOCK_INDEX_START; i < ROOT_BLOCK_INDEX_START; i++, copy += BLOCK_SIZE) {
                blockDevice->read(i, copy);
            }
            //Initializing Root
            for (unsigned int i = 0; i < NUM_DIR_ENTRIES; i++) {
                blockDevice->read(ROOT_BLOCK_INDEX_START + i, frame);
                root[i] = new MyFile();
                memcpy(root[i], (MyFile *) frame, sizeof(MyFile));
            }
            //Initializing OpenFile array, currentFileSystemSize and hasRootIndexAFile array
            for (unsigned int j = 0; j < NUM_DIR_ENTRIES; j++) {
                openFilesArray[j] = -1;
                currentFileSystemSize += root[j]->getFileSize();
                if (j < superBlock->getFileCount()) {
                    hasRootIndexAFile[j] = 1;
                } else {
                    hasRootIndexAFile[j] = 0;
                }
            }
            LogF("currentFileSystemSize: %lu", currentFileSystemSize);
            logSuperBlockInfos(0);
            logDMapAndFatInfos(0);
            logRootInfos(0);
        }
    }
    RETURN(0)
}


// Our file systems own additional methods:
char *MyFS::clearPath(const char *path) {
    char *subString = new char[strlen(path)];
    for (unsigned int i = 0; i < strlen(subString) + 1; i++) {
        subString[i] = path[i + 1];
    }
    subString[strlen(subString)] = '\0';
    return subString;
}

void MyFS::logSuperBlockInfos(int log) {
    if (log == 1) {
        LOG("SuperBlock:");
        LogF("SuperBlockIndexStart: %d", superBlock->getSuperBlockIndexStart());
        LogF("FileSystemSize: %ld", superBlock->getFileSystemSize());
        LogF("DMApBlockStart: %d", superBlock->getDMapBlockIndexStart());
        LogF("FATBlockStart: %d", superBlock->getFatBlockIndexStart());
        LogF("RootBlockStart: %d", superBlock->getRootBlockIndexStart());
        LogF("FileCount: : %d", superBlock->getFileCount());
        LOG();
    }
}

void MyFS::logDMapAndFatInfos(int log) {
    if (log == 1) {
        for (unsigned int i = 0; i < 65536; i++) {
            LogF("Index: %d:, DMap-Value: %c Fat-Value: %d", i, dMap[i], fat[i]);
        }
    }
}

void MyFS::logRootInfos(int log) {
    if (log == 1) {
        for (unsigned int i = 0; i < superBlock->getFileCount(); i++) {
            LogF("Root: %d", i);
            LogF("Filename: %s", root[i]->getFileName());
            LogF("Filesize: %d", root[i]->getFileSize());
            LogF("UserID: %d", root[i]->getUserID());
            LogF("GroupID: %d", root[i]->getGroupID());
            LogF("Mode: %d", root[i]->getMode());
            LogF("ATime: %ld", root[i]->getATime());
            LogF("MTime: %ld", root[i]->getMTime());
            LogF("CTime: %ld", root[i]->getCTime());
            LogF("FirstDataBlockIndex: %d", root[i]->getFirstDataBlockIndex());
            LOG();
        }
    }
}

int MyFS::checkFileIfNotUsed(char *newFileName) {
    for (int i = 0; i < NUM_DIR_ENTRIES; i++) {
        if (hasRootIndexAFile[i] && strcmp(root[i]->getFileName(), newFileName) == 0) {
            return -i;
        }
    }
    return 0;
}

int MyFS::findFreeRootIndex() {
    for (int i = 0; i < NUM_DIR_ENTRIES; i++) {
        if (hasRootIndexAFile[i] == 0) {
            return i;
        }
    }
    return -1;
}

int MyFS::assignFreeDataBlock() {
    for (int i = 0; i < D_Map_SIZE; i++) {
        if (dMap[i] == 'e') {
            dMap[i] = 'f';
            fat[i] = -1;
            return i;
        }
    }
    return -1;
}


void SuperBlock::addFile() {
    this->fileCount++;
}

void SuperBlock::removeFile() {
    if (this->fileCount > 0) {
        this->fileCount--;
    }
}

unsigned long SuperBlock::getFileSystemSize() {
    return this->fileSystemSize;
}

unsigned int SuperBlock::getSuperBlockIndexStart() {
    return this->superBlockBlockIndexStart;
}

unsigned int SuperBlock::getDMapBlockIndexStart() {
    return this->dMapBlockIndexStart;
}

unsigned int SuperBlock::getFatBlockIndexStart() {
    return this->fatBlockIndexStart;
}

unsigned int SuperBlock::getRootBlockIndexStart() {
    return this->rootBlockIndexStart;
}

unsigned int SuperBlock::getFileCount() {
    return this->fileCount;
}


void MyFile::setFileName(char *newFileName) {
    strcpy(this->fileName, newFileName);
}

void MyFile::setFileSize(unsigned int newFileSize) {
    this->fileSize = newFileSize;
}

void MyFile::setUserID(unsigned int newUserID) {
    this->userID = newUserID;
}

void MyFile::setGroupID(unsigned int newGroupID) {
    this->groupID = newGroupID;
}

void MyFile::setMode(unsigned int newMode) {
    this->mode = newMode;
}

void MyFile::setATime(time_t newATime) {
    this->aTime = newATime;
}

void MyFile::setMTime(time_t newMTime) {
    this->mTime = newMTime;
}

void MyFile::setCTime(time_t newCTime) {
    this->cTime = newCTime;
}

void MyFile::setFirstDataBlockIndex(int firstDataBlockVal) {
    this->firstDataBlock = firstDataBlockVal;
}

void MyFile::setOpenIndex(short int newOpenIndex) {
    this->openIndex = newOpenIndex;
}

void MyFile::clearOpenIndex() {
    this->openIndex = -1;
}

char *MyFile::getFileName() {
    char *file = new char[strlen(this->fileName) + 1];
    strcpy(file, this->fileName);
    return file;
}

unsigned int MyFile::getFileSize() {
    return this->fileSize;
}

unsigned MyFile::getUserID() {
    return userID;
}

unsigned MyFile::getGroupID() {
    return groupID;
}

unsigned int MyFile::getMode() {
    return this->mode;
}

time_t MyFile::getATime() {
    return this->aTime;
}

time_t MyFile::getMTime() {
    return this->mTime;
}

time_t MyFile::getCTime() {
    return this->cTime;
}

int MyFile::getFirstDataBlockIndex() {
    return this->firstDataBlock;
}

short int MyFile::getOpenIndex() {
    return this->openIndex;
}

//Methods which are not implemented
int MyFS::fuseReadlink(const char *path, char *link, size_t size) {
    //LogM();
    return 0;
}

int MyFS::fuseMkdir(const char *path, mode_t mode) {
    //LogM();
    return 0;
}

int MyFS::fuseRmdir(const char *path) {
    //LogM();
    return 0;
}

int MyFS::fuseSymlink(const char *path, const char *link) {
    //LogM();
    return 0;
}

int MyFS::fuseRename(const char *path, const char *newpath) {
    //LogM();
    return 0;
}

int MyFS::fuseLink(const char *path, const char *newpath) {
    //LogM();
    return 0;
}

int MyFS::fuseChmod(const char *path, mode_t mode) {
    //LogM();
    return 0;
}

int MyFS::fuseChown(const char *path, uid_t uid, gid_t gid) {
    //LogM();
    return 0;
}

int MyFS::fuseTruncate(const char *path, off_t newSize) {
    //LogM();
    return 0;
}

int MyFS::fuseUtime(const char *path, struct utimbuf *ubuf) {
    //LogM();
    return 0;
}

int MyFS::fuseStatfs(const char *path, struct statvfs *statInfo) {
    //LogM();
    return 0;
}

int MyFS::fuseFlush(const char *path, struct fuse_file_info *fileInfo) {
    //LogM();
    return 0;
}

int MyFS::fuseFsync(const char *path, int datasync, struct fuse_file_info *fileInfo) {
    //LogM();
    return 0;
}

int MyFS::fuseListxattr(const char *path, char *list, size_t size) {
    //LogM();
    //RETURN(0)
    return 0;
}

int MyFS::fuseRemovexattr(const char *path, const char *name) {
    //LogM();
    //RETURN(0)
    return 0;
}

int MyFS::fuseOpendir(const char *path, struct fuse_file_info *fileInfo) {
    //LogM();
    // (TODO: Optional implement this!)
    //  RETURN(0)
    //  RETURN(0)
    return 0;
}

int MyFS::fuseReleasedir(const char *path, struct fuse_file_info *fileInfo) {
//    LogM();
    // (TODO: Optional implement this!)
//    RETURN(0)
    return 0;
}

int MyFS::fuseFsyncdir(const char *path, int datasync, struct fuse_file_info *fileInfo) {
    //LogM();
    //RETURN(0)
    return 0;
}

int MyFS::fuseTruncate(const char *path, off_t offset, struct fuse_file_info *fileInfo) {
    //LogM();
    //RETURN(0)
    return 0;
}

int MyFS::fuseCreate(const char *path, mode_t mode, struct fuse_file_info *fileInfo) {
    //LogM();
    // TODO: Optional implement this!
    //RETURN(0)
    return 0;
}

void MyFS::fuseDestroy() {
//    LogM();
}

#ifdef __APPLE__
int MyFS::fuseSetxattr(const char *path, const char *name, const char *value, size_t size, int flags, uint32_t x) {
#else

int MyFS::fuseSetxattr(const char *path, const char *name, const char *value, size_t size, int flags) {
#endif
    //LogM();
    //RETURN(0)
    return 0;
}

#ifdef __APPLE__
int MyFS::fuseGetxattr(const char *path, const char *name, char *value, size_t size, uint x) {
#else

int MyFS::fuseGetxattr(const char *path, const char *name, char *value, size_t size) {
#endif
    //LogM();
    //RETURN(0)
    return 0;
}