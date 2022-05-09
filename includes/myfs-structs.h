//
//  myFs-structs.h
//  myFs
//
//  Created by Oliver Waldhorst on 07.09.17.
//  Copyright © 2017 Oliver Waldhorst. All rights reserved.
//

#ifndef myFs_structs_h
#define myFs_structs_h

#include "blockdevice.h"
#include "myfs-structs.h"
#include <time.h>

/**
 * File system constants
 */
#define BLOCK_SIZE 512

#define SUPER_BLOCK_BLOCK_INDEX_START 0
#define SUPER_BLOCK_BLOCKS 1
#define FILE_SYSTEM_MAX_DATA_SIZE_IN_MiB 33554432
#define FILE_SYSTEM_MAX_DATA_SIZE_IN_MB 30099999
#define D_Map_SIZE 65536
#define D_MAP_BLOCK_INDEX_START SUPER_BLOCK_BLOCK_INDEX_START+SUPER_BLOCK_BLOCKS
#define D_MAP_BLOCKS D_Map_SIZE/BLOCK_SIZE

#define FAT_SIZE D_Map_SIZE*4
#define FAT_BLOCK_INDEX_START D_MAP_BLOCK_INDEX_START+D_MAP_BLOCKS
#define FAT_BLOCKS FAT_SIZE/BLOCK_SIZE

#define NUM_DIR_ENTRIES 64
#define NUM_OPEN_FILES 64
#define FILE_NAME_MAX_LENGTH 255
#define ROOT_BLOCK_INDEX_START FAT_BLOCK_INDEX_START+FAT_BLOCKS
#define ROOT_BLOCKS NUM_DIR_ENTRIES

#define DATA_BLOCKS_INDEX_START ROOT_BLOCK_INDEX_START+ROOT_BLOCKS
#define DATA_BLOCKS FILE_SYSTEM_MAX_DATA_SIZE_IN_MiB/BLOCK_SIZE

/**
 * The SuperBlock contains:
 * - file system size
 * - number of first SuperBlock block
 * - number of first DMap block
 * - number of first Fat block
 * - number of first Root block
 * - number of files in the file system
 */
struct SuperBlock {
private:
    long unsigned int fileSystemSize = FILE_SYSTEM_MAX_DATA_SIZE_IN_MB;
    unsigned int superBlockBlockIndexStart = SUPER_BLOCK_BLOCK_INDEX_START;
    unsigned int dMapBlockIndexStart = D_MAP_BLOCK_INDEX_START;
    unsigned int fatBlockIndexStart = FAT_BLOCK_INDEX_START;
    unsigned int rootBlockIndexStart = ROOT_BLOCK_INDEX_START;
    unsigned int fileCount = 0;

public:
    SuperBlock();

    ~SuperBlock();

    /**
     * This methods increases the number of files by 1.
     */
    void addFile(void);

    /**
     * This methods decreases the number of files by 1.
     */
    void removeFile(void);

    /**
     * This methods returns the maximum file system size.
     * @return fileSystemSize
     */
    unsigned long getFileSystemSize(void);

    /**
     * This methods returns the first block of the superBlock.
     * @return firstSuperBlockBlock
     */
    unsigned int getSuperBlockIndexStart(void);

    /**
     * This methods returns the first block of the DMap array.
     * @return firstDMapBlock
     */
    unsigned int getDMapBlockIndexStart(void);

    /**
     * This methods returns the first block of the fat array.
     * @return firstFatBlock
     */
    unsigned int getFatBlockIndexStart(void);

    /**
     * This methods returns the first block of the root array.
     * @return firstRootBlock
     */
    unsigned int getRootBlockIndexStart(void);

    /**
     * This methods returns the number of files in the file system.
     * @return fileCount
     */
    unsigned int getFileCount(void);
};

/**
 * A MyFile contains:
 * - file name
 * - file size
 * - user id
 * - group id
 * - mode
 * - aTime
 * - mTime
 * - cTime
 * - first data block index
 * - open index
 * - number of first DMap block
 * - number of first Fat block
 * - number of first Root block
 * - number of files in the file system
 */
struct MyFile {
private:
    char fileName[FILE_NAME_MAX_LENGTH + 1];
    unsigned int fileSize;
    unsigned int userID;
    unsigned int groupID;
    unsigned int mode;
    time_t aTime;
    time_t mTime;
    time_t cTime;
    int firstDataBlock;
    short int openIndex;
public:
    /**
     * Constructor
     */
    MyFile();

    /**
     * Destructor
     */
    ~MyFile();

    /**
     * This methods sets the file name of a new file.
     * @param newFileName
     */
    void setFileName(char *newFileName);

    /**
    * This methods sets the size of a file.
    * @param newFileSize
    */
    void setFileSize(unsigned int newFileSize);

    /**
    * This methods sets the user id of a file.
    * @param newUserID
    */
    void setUserID(unsigned int newUserID);

    /**
     * This methods sets the group id of a file.
     * @param newGroupID
     */
    void setGroupID(unsigned int newGroupID);

    /**
     * This methods sets the mode of a file.
     * @param newMode
     */
    void setMode(unsigned int newMode);

    /**
     * This methods sets the access time of a file.
     * @param newATime
     */
    void setATime(time_t newATime);

    /**
     * This methods sets the modified time of a file.
     * @param newMTime
     */
    void setMTime(time_t newMTime);

    /**
     * This methods sets the time of a file when it was created.
     * @param newCTime
     */
    void setCTime(time_t newCTime);

    /**
     * This methods returns the maximum file system size.
     * @return fileSystemSize
     */
    void setFirstDataBlockIndex(int firstDataBlockVal);

    /**
     * This method sets the openIndex of a file.
     * @param newOpenIndex
     */
    void setOpenIndex(short int newOpenIndex);

    /**
     * This method clears (setting to -1) the openIndex of a file.
     */
    void clearOpenIndex();

    /**
     * This methods returns the name of a file.
     * @return fileName
     */
    char *getFileName(void);

    /**
     * This methods returns the size of a file.
     * @return fileSize
     */
    unsigned int getFileSize(void);

    /**
     * This methods returns the user ID of a file.
     * @return userID
     */
    unsigned getUserID(void);

    /**
     * This methods returns the groupID of a file.
     * @return groupID
     */
    unsigned getGroupID(void);

    /**
     * This methods returns the mode of a file.
     * @return mode
     */
    unsigned int getMode(void);

    /**
     * This methods returns the time of a file when it was accessed.
     * @return aTime
     */
    time_t getATime(void);

    /**
     * This methods returns the time of a file when it was modified.
     * @return mTime
     */
    time_t getMTime(void);

    /**
     * This methods returns the time of a file when it was created.
     * @return cTime
     */
    time_t getCTime(void);

    /**
     * This methods returns the first data block of a file.
     * @return firstDataBlock
     */
    int getFirstDataBlockIndex(void);

    /**
     * This methods returns the openIndex of a file.
     * @return openIndex
     */
    short int getOpenIndex(void);
};

#endif /* myFs_structs_h */