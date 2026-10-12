#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>

static char *prog;
static struct stat t_st;
static int t_fd;

static char buf_t[65536];
static char buf_c[65536];


int read_full(int t_fd, char *buf, size_t n) {
    size_t c = 0;
    while(c < n) {
        ssize_t r = read(t_fd, buf + c, n - c);
        if(r < 0) {
            return -1;
        }
        if(r == 0) {
            break;
        }
        c += r;
    }
    return c;
}

int cmp_content(const char *path) {
    int c_fd, result = 1;
    c_fd = open(path, O_RDONLY);
    
    if(c_fd < 0) {
        fprintf(stderr, "%s: Warning: cannot open %s: %s\n", prog, path, strerror(errno));
        return -1;
    }

    if(lseek(t_fd, 0, SEEK_SET) < 0) {
        fprintf(stderr, "%s: Warning: cannot lseek target file: %s\n", prog, strerror(errno));
        close(c_fd);
        return -1;
    }

    while(1) {
        int rt = read_full(t_fd, buf_t, 65536);
        if(rt < 0) {
            fprintf(stderr, "%s: Warning: error reading target file: %s\n", prog, strerror(errno));
            result = -1;
            break;
        }
        
        int rc = read_full(c_fd, buf_c, 65536);
        if(rc < 0) {
            fprintf(stderr, "%s: Warning: error reading %s: %s\n", prog, path, strerror(errno));
            result = -1;
            break;
        }
        if(rt != rc || memcmp(buf_t, buf_c, rt) != 0) {
            result = 0;
            break;
        }    
        if(rt == 0) {
            break;
        }
    }

    if(close(c_fd) < 0) {
        fprintf(stderr, "%s: Warning: cannot close %s: %s\n", prog, path, strerror(errno));
    }
    return result;
}

void check_symlink(char *path) {
    struct stat rst;

    if(stat(path, &rst) < 0) {
        if(errno != ENOENT) {
            fprintf(stderr, "Warning: cannot resolve symlink.\n");
        }
        return;
    }
    if(!S_ISREG(rst.st_mode)){
        return;
    }

    if(rst.st_dev == t_st.st_dev && rst.st_ino == t_st.st_ino) {
        printf("%s SYMLINK RESOLVES TO TARGET\n", path);
        return;
    }
    if(rst.st_size != t_st.st_size){
        return;
    }
    if(cmp_content(path) != 1) {
        return;
    }
    char linkbuf[4096];
    int n = readlink(path, linkbuf, sizeof linkbuf - 1);
    if (n < 0) {
        fprintf(stderr, "%s: Warning: can't readlink %s: %s\n", prog, path, strerror(errno));
        return;
    }
    //add terminator
    linkbuf[n] = '\0';
    printf("%s SYMLINK (%s) RESOLVES TO DUPLICATE with nlink=%lu, dev 0x%llx, ino %llu\n",
           path, linkbuf,
           (unsigned long)rst.st_nlink,
           (unsigned long long)rst.st_dev,
           (unsigned long long)rst.st_ino);
}

void check_regular(char *path, struct stat *est) {
    if(est->st_dev == t_st.st_dev && est->st_ino == t_st.st_ino) {
        printf("%s HARD LINK TO TARGET\n", path);
        return;
    }

    //don't compare unless size is equal
    if(est->st_size != t_st.st_size) {
        return;
    }

    if (cmp_content(path) == 1) {
        printf("%s DUPLICATE OF TARGET (nlink=%lu, dev 0x%llx, ino %llu)\n", path,
        (unsigned long)est->st_nlink,
        (unsigned long long)est->st_dev,
        (unsigned long long)est->st_ino);
    }
}

void explore(char *dir) {
    DIR *d;
    struct dirent *de;
    d = opendir(dir);
    if(!d) {
        fprintf(stderr, "%s: Warning: cannot open directory %s: %s\n", prog, dir, strerror(errno));
        return;
    }
    while(1) {
        errno = 0;
        de = readdir(d);

        if(!de) {
            if(errno) {
                fprintf(stderr, "%s: Warning: error reading directory %s: %s\n", prog, dir, strerror(errno));
            }
            break;
        }
        // skip current and parent directories
        if(strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }

        char path[4096];
        strcpy(path, dir);
        strcat(path, "/");
        strcat(path, de->d_name);
        struct stat est;
        if(lstat(path, &est) < 0)  {
            fprintf(stderr, "%s: Warning: can't lstat %s: %s\n", prog, path, strerror(errno));
            continue;
        }
    
        if(S_ISDIR(est.st_mode)) {
            //go one directory down
            explore(path);
        }else if(S_ISREG(est.st_mode)){
            //compare files for equality
            check_regular(path, &est);
        }else if(S_ISLNK(est.st_mode)) {
            //check if symlink resolves to target
            check_symlink(path);
        }else {
            // printf("OTHER %s\n", path);
        }
        
    }
    if (closedir(d) < 0) {
        fprintf(stderr, "%s: Warning: can't close directory %s: %s\n", prog, dir, strerror(errno)); 
    }
}

int main(int argc, char *argv[]) {
    prog = argv[0];

    if (argc != 3) {
        fprintf(stderr, "Usage: %s TARGETFILE STARTDIR\n", prog);
        return 1;
    }

    t_fd = open(argv[1], O_RDONLY);
    if(t_fd < 0) {
        fprintf(stderr, "%s: Error: cannot open target file %s: %s\n", prog, argv[1], strerror(errno));
        return 1;
    }

    if (fstat(t_fd, &t_st) < 0) {
        fprintf(stderr, "%s: Error: can't fstat target file %s: %s\n", prog, argv[1], strerror(errno));
        return 1;
    }

    if(!S_ISREG(t_st.st_mode)) {
        fprintf(stderr, "%s: Error: target %s is not a regular file\n", prog, argv[1]);
        return 1;
    }
    
    // printf("Target %s: %lld bytes, dev 0x%llx, ino %llu, nlink %lu\n", argv[1],
    //     (long long)t_st.st_size, 
    //     (unsigned long long)t_st.st_dev,
    //     (unsigned long long)t_st.st_ino,
    //     (unsigned long)t_st.st_nlink);

    explore(argv[2]);

    if(close(t_fd) < 0) {
        fprintf(stderr, "%s: Error: can't close target file %s: %s\n", prog, argv[1], strerror(errno));
        return 1;
    }

    return 0;
}
