//
//  myFs.h
//  myFs
//
//  Created by Oliver Waldhorst on 02.08.17.
//  Copyright © 2017 Oliver Waldhorst. All rights reserved.
//

#ifndef myFs_h
#define myFs_h

#include <fuse.h>
#include <cmath>

#include "blockdevice.h"
#include "myfs-structs.h"

class MyFS {
private:
    static MyFS *_instance;
    FILE *logFile;
    BlockDevice *blockDevice;
    SuperBlock *superBlock;
    char dMap[DATA_BLOCKS];
    int fat[DATA_BLOCKS];
    MyFile *root[NUM_DIR_ENTRIES];
    int lastBlockRead = -1;
    int lastBlockWritten = -1;
    char lastBlockReadFrame[BLOCK_SIZE * NUM_OPEN_FILES];
    char lastBlockWritenFrame[BLOCK_SIZE * NUM_OPEN_FILES];
    unsigned short int openFiles = 0;
    short int openFilesArray[BLOCK_SIZE * NUM_OPEN_FILES];

    long unsigned int currentFileSystemSize = 0;
    int hasRootIndexAFile[NUM_DIR_ENTRIES];



public:
    static MyFS *Instance();

    // TODO: Add attributes of your file system here

    MyFS();

    ~MyFS();

//Methods which are not implemented
    int fuseReadlink(const char *path, char *link, size_t size);

    int fuseMkdir(const char *path, mode_t mode);

    int fuseRmdir(const char *path);

    int fuseSymlink(const char *path, const char *link);

    int fuseRename(const char *path, const char *newpath);

    int fuseLink(const char *path, const char *newpath);

    int fuseChmod(const char *path, mode_t mode);

    int fuseChown(const char *path, uid_t uid, gid_t gid);

    int fuseTruncate(const char *path, off_t newSize);

    int fuseUtime(const char *path, struct utimbuf *ubuf);

    int fuseStatfs(const char *path, struct statvfs *statInfo);

    int fuseFlush(const char *path, struct fuse_file_info *fileInfo);

    int fuseFsync(const char *path, int datasync, struct fuse_file_info *fi);

    int fuseListxattr(const char *path, char *list, size_t size);

    int fuseRemovexattr(const char *path, const char *name);

    int fuseOpendir(const char *path, struct fuse_file_info *fileInfo);

    int fuseReaddir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fileInfo);

    int fuseReleasedir(const char *path, struct fuse_file_info *fileInfo);

    int fuseFsyncdir(const char *path, int datasync, struct fuse_file_info *fileInfo);

    int fuseTruncate(const char *path, off_t offset, struct fuse_file_info *fileInfo);

    int fuseCreate(const char *, mode_t, struct fuse_file_info *);

    void fuseDestroy();

#ifdef __APPLE__
    int fuseSetxattr(const char *path, const char *name, const char *value, size_t size, int flags, uint32_t x);
    int fuseGetxattr(const char *path, const char *name, char *value, size_t size, uint x);
#else

    int fuseSetxattr(const char *path, const char *name, const char *value, size_t size, int flags);

    int fuseGetxattr(const char *path, const char *name, char *value, size_t size);

#endif
    // --- Methods called by FUSE ---
    // For Documentation see https://libfuse.github.io/doxygen/structfuse__operations.html

    //Implemented methods
    int fuseGetattr(const char *path, struct stat *statBuf);

    int fuseMkNod(const char *path, mode_t mode, dev_t dev);

    int fuseUnlink(const char *path);

    int fuseOpen(const char *path, struct fuse_file_info *fileInfo);

    int fuseRead(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo);

    int fuseWrite(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo);

    int fuseRelease(const char *path, struct fuse_file_info *fileInfo);

    void *fuseInit(struct fuse_conn_info *conn);

    // TODO: Add methods of your file system here
    /**
     * This method removes '/' at the beginning of a file.
     * @param log=1 for logging
     */
    static char *clearPath(const char *path);

    /**
     * This method logs informations about the superblock.
     * @param log=1 for logging
     */
    void logSuperBlockInfos(int log);

    /**
     * This method logs informations about DMap and fat entries.
     * @param log=1 for logging
     */
    void logDMapAndFatInfos(int log);

    /**
     * This method logs informations about the all files in the root array.
     * @param log=1 for logging
     */
    void logRootInfos(int log);

    /**
     * This method checks if a file is already in use.
     * @param newFileName file name which should be checked
     * @return 0 if this file is not used or -1 if it is used
     */
    int checkFileIfNotUsed(char *newFileName);

    /**
     * This method find a free space in the root array for an new file.
     * @return index in the root array for the new file or -1 as error
     */
    int findFreeRootIndex();

    /**
     * This methods assigns a freeDataBlock to a data block of a file.
     * @return assigned data block number or -1 as error
     */
    int assignFreeDataBlock();
};

#endif /* myFs_h */