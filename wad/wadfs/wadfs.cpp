#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <iostream>
#include <string>
#include <unordered_set>

#include "/home/reptilian/P3/libWad/Wad.h"

Wad *wadObject = nullptr;

// https://maastaar.net/fuse/linux/filesystem/c/2019/09/28/writing-less-simple-yet-stupid-filesystem-using-FUSE-in-C/

static int do_getattr(const char *path, struct stat *st) {
    st->st_uid = getuid();
    st->st_gid = getgid();
	st->st_atime = time( NULL );
	st->st_mtime = time( NULL );

    std::string strPath(path);

   if ( strcmp( path, "/" ) == 0 || wadObject->isDirectory(strPath)) {
		st->st_mode = S_IFDIR | 0755;
		st->st_nlink = 2;
	}
	else if (wadObject->isContent(strPath)) {
		st->st_mode = S_IFREG | 0644;
		st->st_nlink = 1;
		st->st_size = wadObject->getSize(strPath);
	}
	else {
		return -ENOENT;
	}
	
	return 0;
}

static int do_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    std::vector<std::string> directory;
    std::string strPath(path);

    if (wadObject->getDirectory(strPath, &directory) < 0) {
        return -EIO;
    }

    filler(buffer, ".", nullptr, 0);
    filler(buffer, "..", nullptr, 0);
    

    std::unordered_set<std::string> entries;

    for (const auto &dir : directory) {
        if (entries.find(dir) == entries.end()) {
            filler(buffer, dir.c_str(), nullptr, 0);
            entries.insert(dir);
        }
        
    }
    return 0;
}

static int do_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
    std::string strPath(path);
    if (!wadObject->isContent(strPath)) {
        return -ENOENT;
    }

    int bytesRead = wadObject->getContents(strPath, buffer, size, offset);
    if (bytesRead > 0) {
        return bytesRead;
    }
    return -ENOENT;
}

int do_mkdir(const char *path, mode_t mode) {
    std::string strPath(path);
    wadObject->createDirectory(strPath);
    return 0;
}

int do_mknod(const char *path, mode_t mode, dev_t dev) {
    std::string strPath(path);
    wadObject->createFile(strPath);
    return 0;
}   

static int do_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
    std::string strPath(path);
    if (!wadObject->isContent(strPath)) {
        wadObject->createFile(strPath);
    }

    int bytesWritten = wadObject->writeToFile(strPath, buffer, size, offset);
    if (bytesWritten < 0) {
        return -EIO;
    }
    return bytesWritten;
}

static struct fuse_operations operations {
    .getattr = do_getattr,
    .mknod = do_mknod,
    .mkdir = do_mkdir,
    .read = do_read,
    .write = do_write,
    .readdir = do_readdir,  
};

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Not enough arguments." << std::endl;
        exit(EXIT_SUCCESS);
    }

    std::string wadPath = argv[argc - 2];

    if (wadPath.at(0) != '/') {
        wadPath = std::string(get_current_dir_name()) + "/" + wadPath;
    }
    wadObject = Wad::loadWad(wadPath);



    argv[argc - 2] = argv[argc - 1];
    argc--;



    return fuse_main(argc, argv, &operations, wadObject);
}