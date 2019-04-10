#pragma once

#include <string>
#include <vector>


class Path {
public:
    static const char *separator;

    explicit Path(std::string pathname);
    Path(Path const &root, std::string const &pathname);
    Path();

    struct Stat {
        ino_t inum;         /* Inode number */
        mode_t mode;         /* File type and mode */
        nlink_t nlinks;       /* Number of hard links */
        off_t size;         /* Total size, in bytes */
    };

    Stat getStat() const;

    bool isDir() const;

    std::vector<Path> getSubDirs() const;

    std::string getPath() const;

    std::string getName() const;
private:
    Path(Path const &root, std::string const &pathname, bool isDirFlag);

    bool isDirFlag = false;
    std::string path;
};