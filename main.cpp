#include <utility>

#include <iostream>
#include <limits>
#include <functional>
#include <zconf.h>
#include <wait.h>
#include <bits/unique_ptr.h>
#include <cstring>
#include "Path.h"
#include "dirent.h"


using namespace std;


class Parameters {
public:
    const string inumCommand = "-inum";
    const string nameCommand = "-name";
    const string sizeCommand = "-size";
    const string nlinksCommand = "-nlinks";
    const string execCommand = "-exec";

    Parameters(int argc, char *argv[]) {
        for (int i = 0; i < argc; i++) {
            if (argv[i] == inumCommand) {
                try {
                    inum.set(stoull(argv[i + 1]));
                } catch (logic_error const &e) {
                    throw invalid_argument(inumCommand + ": " + string(e.what()));
                }
            } else if (argv[i] == nameCommand) {
                name.set(argv[i + 1]);
            } else if (argv[i] == sizeCommand) {
                try {
                    string sizeStr(argv[i + 1] + 1);
                    sizeConstraints.update(stoll(argv[i + 1]), argv[i + 1][0]);
                } catch (logic_error const &e) {
                    throw invalid_argument(sizeCommand + ": " + string(e.what()));
                }
            } else if (argv[i] == nlinksCommand) {
                try {
                    nlinks.set(stoll(argv[i + 1]));
                } catch (logic_error const &e) {
                    throw invalid_argument(nlinksCommand + ": " + string(e.what()));
                }
            } else if (argv[i] == execCommand) {
                exec.set(argv[i + 1]);
            } else if (argv[i][0] == '-') {
                throw invalid_argument("Unknown flag " + string(argv[i]));
            }
        }
        if (argc < 2) {
            throw invalid_argument("too few arguments");
        } else {
            root = Path(argv[1]);
        }
    }

    bool check(Path const &path) const {
        Path::Stat pathStat = path.getStat();
        bool checkSize = sizeConstraints.match(pathStat.size);
        bool checkInum = !inum.has || inum.value == pathStat.inum;
        bool checkName = !name.has || path.getName() == name.value;
        bool checkNlinks = !nlinks.has || pathStat.nlinks == nlinks.value;

        return checkSize && checkInum && checkName && checkNlinks;
    }

    bool hasExec() const {
        return exec.has;
    }

    string getExec() const {
        return exec.value;
    }

    Path getRoot() const {
        return root;
    }

private:
    template<typename T>
    struct optionalArg {
        T value = T();
        bool has = false;

        void set(T val) {
            has = true;
            value = val;
        }
    };

    struct SizeConstraints {
        size_t from = 0;
        size_t to = numeric_limits<size_t>::max();

        void update(size_t value, char type) {
            if (type == '=') {
                from = value;
                to = value;
            } else if (type == '-') {
                to = min(to, value);
            } else if (type == '+') {
                from = max(from, value);
            }
        }

        bool match(size_t value) const {
            return from <= value && value <= to;
        }
    };

    Path root;
    optionalArg<ino_t> inum;
    optionalArg<string> name;
    SizeConstraints sizeConstraints;
    optionalArg<int> nlinks;
    optionalArg<string> exec;
};


class Walker {
public:
    using Consumer = function<void(string)>;

    Walker(Parameters parameters, Consumer consumer)
            : parameters(std::move(parameters)), consumer(std::move(consumer)) {}

    void walk(const Path &curPath) {
        vector<Path> subDirs;

        try {
            subDirs = curPath.getSubDirs();
        } catch (runtime_error const &e) {
            cerr << e.what() << endl;
        }

        for (auto const &path : subDirs) {
            if (path.isDir()) {
                walk(path);
            }
            if (parameters.check(path)) {
                consumer(path.getPath());
            }
        }
    }

private:
    Consumer consumer;
    Parameters parameters;
};


void print_error(const string &message) {
    cerr << message << endl;
    cerr << strerror(errno) << endl;
}


function<void(string)> getExecutor(string const &programName, char *envp[]) {
    return [programName, envp](string path) {
        pid_t pid = fork();
        if (pid == 0) {
            char *argv[3] = {
                    const_cast<char *>(programName.data()),
                    const_cast<char *>(path.data()),
                    nullptr
            };
            if (execve(programName.c_str(), argv, envp) == -1) {
                print_error("Cannot execute \'" + programName + "\':");
                exit(EXIT_FAILURE);
            }
        } else if (pid == -1) {
            print_error("System error:");
        } else {
            int status;
            pid_t result = waitpid(pid, &status, 0);
            if (result == -1) {
                print_error("Error during execution \'" + programName + "\'");
            }
        }
    };
}


void print(string const &st) {
    cout << st << endl;
}


int main(int argc, char *argv[], char *envp[]) {
    try {
        Parameters parameters(argc, argv);

        function<void(string)> action;
        if (parameters.hasExec()) {
            action = getExecutor(parameters.getExec(), envp);
        } else {
            action = print;
        }

        Walker walker(parameters, action);
        walker.walk(parameters.getRoot());
    } catch (exception const &e) {
        cerr << e.what() << endl;
    }
    return 0;
}