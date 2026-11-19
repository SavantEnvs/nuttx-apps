/* bas_env.h — force-included (-include) into every interpreters/bas file.
 *
 * A BASIC program can create, overwrite, rename and delete files and
 * directories (OPEN ... FOR OUTPUT, SAVE, KILL, NAME, MKDIR, CHDIR, ...).
 * Under the fuzzer every input runs in the same container, so these calls
 * go through bas_env.c, which confines changes to a scratch directory made
 * for this process (and removed when it exits):
 *
 *   open()     read-only opens are unchanged; an open that can write
 *              (O_WRONLY, O_RDWR, O_CREAT, O_TRUNC, O_APPEND) is allowed
 *              for "/dev/null" (the default LPRINT device) and for a
 *              relative path with no ".." component, i.e. inside the
 *              scratch directory, the working directory;
 *   unlink(), rename(), mkdir()   the same relative-path rule;
 *   chdir()    fails with EACCES, so the working directory stays put.
 *
 * Refused calls fail with EACCES, which the interpreter reports as a BASIC
 * error like any other I/O failure.
 */
#ifndef __MAYHEM_BAS_ENV_H
#define __MAYHEM_BAS_ENV_H
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
int bas_env_open(const char *path, int flags, ...);
int bas_env_unlink(const char *path);
int bas_env_rename(const char *from, const char *to);
int bas_env_mkdir(const char *path, mode_t mode);
int bas_env_chdir(const char *path);
#define open(...)        bas_env_open(__VA_ARGS__)
#define unlink(p)        bas_env_unlink(p)
#define rename(f, t)     bas_env_rename(f, t)
#define mkdir(p, m)      bas_env_mkdir(p, m)
#define chdir(p)         bas_env_chdir(p)
#endif
