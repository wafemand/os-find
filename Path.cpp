#include <utility>

#include "Path.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <syscall.h>
#include <memory>
#include <dirent.h>
#include <cstring>


using namespace std;


const char *Path::separator = "/";


void throwSysError(string const &msg) {
    throw runtime_error(string("Error: ") + msg + "\n" + strerror(errno));
}

void throwSysError() {
    throwSysError("");
}


static string getCWD() {
    char *c = get_current_dir_name();
    if (c == nullptr) {
        throwSysError("Cannot determine current directory");
    }
    string res = c;
    free(c);
    return res;
}


string normalizePath(string path) {
    if (path.substr(0, strlen(Path::separator)) != Path::separator) {
         path = getCWD() + Path::separator + path;
    }
    return path;
    auto st = realpath(path.c_str(), nullptr);

    if (st == nullptr) {
        throwSysError("Cannot normalize path");
    }
    string res(st);
    free(st);
    return res;
}


Path::Path(string pathname) : path(move(pathname)), isDirFlag(false) {
    path = normalizePath(path);
    isDirFlag = S_ISDIR(getStat().mode);
}


Path::Path(const Path &root, const string &pathname)
        : path(root.path + separator + pathname), isDirFlag(false) {
    path = normalizePath(path);
    isDirFlag = S_ISDIR(getStat().mode);
}


Path::Path(Path const &root, std::string const &pathname, bool isDir)
        : path(root.path + separator + pathname), isDirFlag(isDir) {
    path = normalizePath(path);
}


Path::Stat Path::getStat() const {
    struct stat st{};
    if (stat(path.c_str(), &st) == -1) {
        throwSysError("Cannot get stat of " + path);
    }
    Stat res{};
    res.nlinks = st.st_nlink;
    res.size = st.st_size;
    res.inum = st.st_ino;
    res.mode = st.st_mode;
    return res;
}


bool Path::isDir() const {
    return isDirFlag;
}


struct linux_dirent64 {
    ino64_t d_ino;    /* 64-bit inode number */
    off64_t d_off;    /* 64-bit offset to next structure */
    unsigned short d_reclen; /* Size of this dirent */
    unsigned char d_type;   /* File type */
    char d_name[]; /* Filename (null-terminated) */
};


struct FileDescriptorHolder {
    int fd;

    explicit FileDescriptorHolder(const string& path) {
        fd = open(path.c_str(), O_RDONLY | O_DIRECTORY);

        if (fd == -1) {
            throwSysError("Cannot open " + path);
        }
    }

    ~FileDescriptorHolder() {
        if (close(fd) == -1) {
            throwSysError("Cannot close descriptor " + to_string(fd));
        }
    }
};


std::vector<Path> Path::getSubDirs() const {
    static const size_t BUF_SIZE = 1024;
    static const string DOT = ".";
    static const string DOTDOT = "..";

    FileDescriptorHolder holder(path);

    vector<Path> res;
    unique_ptr<char[]> bufPtr(new char[BUF_SIZE]);
    char *buf = bufPtr.get();

    for (;;) {
        int nread = syscall(SYS_getdents64, holder.fd, buf, BUF_SIZE);
        if (nread == -1) {
            throwSysError("I give up :((. Report this, please!");
        }

        if (nread == 0) {
            break;
        }

        for (int bpos = 0; bpos < nread;) {
            auto d = (struct linux_dirent64 *) (buf + bpos);
            bool notDots = d->d_name != DOTDOT && d->d_name != DOT;
            bool regOrDir = d->d_type == DT_DIR || d->d_type == DT_REG;
            if (notDots && regOrDir) {
                Path curPath(*this, d->d_name, d->d_type == DT_DIR);
                res.push_back(curPath);
            }
            bpos += d->d_reclen;
        }
    }

    return res;
}

string Path::getPath() const {
    return path;
}

std::string Path::getName() const {
    size_t pos = path.size() - strlen(separator);
    while (path.substr(pos, strlen(separator)) != separator) {
        pos--;
    }
    return path.substr(pos + strlen(separator));
}

Path::Path() = default;
