///////////////////////////////////////////////////////////////////////////////
// Licensed Materials - Property of IBM
// ZOSLIB
// (C) Copyright IBM Corp. 2020. All Rights Reserved.
// US Government Users Restricted Rights - Use, duplication
// or disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
///////////////////////////////////////////////////////////////////////////////

#define _AE_BIMODAL 1
#include "zos-base.h"

#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

namespace {
const char MEMLOG_LEVEL_WARNING = '1';
const char MEMLOG_LEVEL_ALL = '2';

char __gMemoryUsageLogFile[PATH_MAX] = "";
bool __gLogMemoryUsage = false;
bool __gLogMemoryAll = false;
bool __gLogMemoryWarning = false;
FILE *fp_memprintf = nullptr;
bool __gLogMemoryShowPid = true;
}

#ifdef __cplusplus
extern "C" {
#endif

void __console(const void *p_in, int len_i) {
  const unsigned char *p = (const unsigned char *)p_in;
  int len = len_i;
  while (len > 0 && p[len - 1] == 0x15) {
    --len;
  }
  typedef struct wtob {
    unsigned short sz;
    unsigned short flags;
    unsigned char msgarea[130];
  } wtob_t;
  wtob_t *m = (wtob_t *)__malloc31(sizeof(wtob));
  while (len > 126) {
    m->sz = 130;
    m->flags = 0x8000;
    memcpy(m->msgarea, p, 126);
    memcpy(m->msgarea + 126, "\x20\x00\x00\x20", 4);
    __asm volatile(" la  0,0 \n"
                   " lr  1,%0 \n"
                   " svc 35 \n"
                   :
                   : "r"(m)
                   : "r0", "r1", "r15");
    p += 126;
    len -= 126;
  }
  if (len > 0) {
    m->sz = len + 4;
    m->flags = 0x8000;
    memcpy(m->msgarea, p, len);
    memcpy(m->msgarea + len, "\x20\x00\x00\x20", 4);
    __asm volatile(" la  0,0 \n"
                   " lr  1,%0 \n"
                   " svc 35 \n"
                   :
                   : "r"(m)
                   : "r0", "r1", "r15");
  }
  __free31(m, sizeof(wtob));
}

int __console_printf(const char *fmt, ...) {
  va_list ap;
  int len;
  va_start(ap, fmt);
  va_list ap1;
  va_list ap2;
  va_copy(ap1, ap);
  va_copy(ap2, ap);
  int bytes;
  int ccsid;
  int am;
  strlen_ae((const unsigned char *)fmt, &ccsid, strlen(fmt) + 1, &am);
  int mode;
  if (ccsid == 819) {
    mode = __ae_thread_swapmode(__AE_ASCII_MODE);
    bytes = __vsnprintf_a(0, 0, fmt, ap1);
    char buf[bytes + 1];
    len = __vsnprintf_a(buf, bytes + 1, fmt, ap2);
    __a2e_l(buf, len);
    va_end(ap2);
    va_end(ap1);
    va_end(ap);
    if (len > 0)
      __console(buf, len);
  } else {
    mode = __ae_thread_swapmode(__AE_EBCDIC_MODE);
    bytes = __vsnprintf_e(0, 0, fmt, ap1);
    char buf[bytes + 1];
    len = __vsnprintf_e(buf, bytes + 1, fmt, ap2);
    va_end(ap2);
    va_end(ap1);
    va_end(ap);
    if (len > 0)
      __console(buf, len);
  }
  __ae_thread_swapmode(mode);
  return len;
}

int vdprintf(int fd, const char *fmt, va_list ap) {
  int ccsid;
  int am;
  strlen_ae((const unsigned char *)fmt, &ccsid, strlen(fmt) + 1, &am);
  int mode;
  int len;
  int bytes;
  va_list ap1;
  va_list ap2;
  va_copy(ap1, ap);
  va_copy(ap2, ap);
  if (ccsid == 819) {
    mode = __ae_thread_swapmode(__AE_ASCII_MODE);
    bytes = __vsnprintf_a(0, 0, fmt, ap1);
    char buf[bytes + 1];
    len = __vsnprintf_a(buf, bytes + 1, fmt, ap2);
    if (len > 0)
      len = write(fd, buf, len);
  } else {
    mode = __ae_thread_swapmode(__AE_EBCDIC_MODE);
    bytes = __vsnprintf_e(0, 0, fmt, ap1);
    char buf[bytes + 1];
    len = __vsnprintf_e(buf, bytes + 1, fmt, ap2);
    if (len > 0)
      len = write(fd, buf, len);
  }
  __ae_thread_swapmode(mode);
  return len;
}

int dprintf(int fd, const char *fmt, ...) {
  va_list ap;
  int len;
  va_start(ap, fmt);
  va_list ap1;
  va_list ap2;
  va_copy(ap1, ap);
  va_copy(ap2, ap);
  int bytes;
  int ccsid;
  int am;
  strlen_ae((const unsigned char *)fmt, &ccsid, strlen(fmt) + 1, &am);
  int mode;
  if (ccsid == 819) {
    mode = __ae_thread_swapmode(__AE_ASCII_MODE);
    bytes = __vsnprintf_a(0, 0, fmt, ap1);
    char buf[bytes + 1];
    len = __vsnprintf_a(buf, bytes + 1, fmt, ap2);
    va_end(ap2);
    va_end(ap1);
    va_end(ap);
    if (len > 0)
      len = write(fd, buf, len);
  } else {
    mode = __ae_thread_swapmode(__AE_EBCDIC_MODE);
    bytes = __vsnprintf_e(0, 0, fmt, ap1);
    char buf[bytes + 1];
    len = __vsnprintf_e(buf, bytes + 1, fmt, ap2);
    va_end(ap2);
    va_end(ap1);
    va_end(ap);
    if (len > 0)
      len = write(fd, buf, len);
  }
  __ae_thread_swapmode(mode);
  return len;
}

void __dump_title(int fd, const void *addr, size_t len, size_t bw,
                  const char *format, ...) {
  static const unsigned char *atbl = (unsigned char *)"................"
                                                      "................"
                                                      " !\"#$%&'()*+,-./"
                                                      "0123456789:;<=>?"
                                                      "@ABCDEFGHIJKLMNO"
                                                      "PQRSTUVWXYZ[\\]^_"
                                                      "`abcdefghijklmno"
                                                      "pqrstuvwxyz{|}~."
                                                      "................"
                                                      "................"
                                                      "................"
                                                      "................"
                                                      "................"
                                                      "................"
                                                      "................"
                                                      "................";
  static const unsigned char *etbl = (unsigned char *)"................"
                                                      "................"
                                                      "................"
                                                      "................"
                                                      " ...........<(+|"
                                                      "&.........!$*);^"
                                                      "-/.........,%_>?"
                                                      ".........`:#@'=\""
                                                      ".abcdefghi......"
                                                      ".jklmnopqr......"
                                                      ".~stuvwxyz...[.."
                                                      ".............].."
                                                      "{ABCDEFGHI......"
                                                      "}JKLMNOPQR......"
                                                      "\\.STUVWXYZ......"
                                                      "0123456789......";
  const unsigned char *p = (const unsigned char *)addr;
  if (format) {
    va_list ap;
    va_start(ap, format);
    vdprintf(fd, format, ap);
    va_end(ap);
  } else {
    dprintf(fd, "Dump: \"Address: Content in Hexdecimal, ASCII, EBCDIC\"\n");
  }
  unsigned char line[2048];
  const unsigned char *buffer;
  long offset = 0;
  long b = 0;
  size_t sz = 0;
  size_t i;
  int c;
  __auto_ascii _a;
  while (len > 0) {
    sz = (len > (bw - 1)) ? bw : len;
    buffer = p + offset;
    b = 0;
    b += __snprintf_a((char *)line + b, 2048 - b, "%*p:", 16, buffer);
    for (i = 0; i < sz; ++i) {
      if ((i & 3) == 0)
        line[b++] = ' ';
      c = buffer[i];
      line[b++] = "0123456789abcdef"[(0xf0 & c) >> 4];
      line[b++] = "0123456789abcdef"[(0x0f & c)];
    }
    for (; i < bw; ++i) {
      if ((i & 3) == 0)
        line[b++] = ' ';
      line[b++] = ' ';
      line[b++] = ' ';
    }
    line[b++] = ' ';
    line[b++] = '|';
    for (i = 0; i < sz; ++i) {
      c = buffer[i];
      if (c == -1) {
        line[b++] = '*';
      } else {
        line[b++] = atbl[c];
      }
    }
    for (; i < bw; ++i) {
      line[b++] = ' ';
    }
    line[b++] = '|';
    line[b++] = ' ';
    line[b++] = '|';
    for (i = 0; i < sz; ++i) {
      c = buffer[i];
      if (c == -1) {
        line[b++] = '*';
      } else {
        line[b++] = etbl[c];
      }
    }
    for (; i < bw; ++i) {
      line[b++] = ' ';
    }
    line[b++] = '|';
    line[b++] = 0;
    dprintf(fd, "%-.*s\n", b, line);
    offset += sz;
    len -= sz;
  }
}

void __dump(int fd, const void *addr, size_t len, size_t bw) {
  __dump_title(fd, addr, len, bw, 0);
}

#if TRACE_ON // for debugging use

class Fdtype {
  char buffer[64];

public:
  Fdtype(int fd) {
    struct stat st;
    int rc = fstat(fd, &st);
    if (-1 == rc) {
      snprintf(buffer, 64, "fstat %d failed errno is %d", fd, errno);
      return;
    }
    if (S_ISBLK(st.st_mode)) {
      snprintf(buffer, 64, "fd %d is %s", fd, "S_ISBLK");
    } else if (S_ISDIR(st.st_mode)) {
      snprintf(buffer, 64, "fd %d is %s", fd, "S_ISDIR");
    } else if (S_ISCHR(st.st_mode)) {
      snprintf(buffer, 64, "fd %d is %s", fd, "S_ISCHR");
    } else if (S_ISFIFO(st.st_mode)) {
      snprintf(buffer, 64, "fd %d is %s", fd, "S_ISFIFO");
    } else if (S_ISREG(st.st_mode)) {
      snprintf(buffer, 64, "fd %d is %s", fd, "S_ISREG");
    } else if (S_ISLNK(st.st_mode)) {
      snprintf(buffer, 64, "fd %d is %s", fd, "S_ISLNK");
    } else if (S_ISSOCK(st.st_mode)) {
      snprintf(buffer, 64, "fd %d is %s", fd, "S_ISSOCK");
    } else if (S_ISVMEXTL(st.st_mode)) {
      snprintf(buffer, 64, "fd %d is %s", fd, "S_ISVMEXTL");
    } else {
      snprintf(buffer, 64, "fd %d st_mode is x%08x", fd, st.st_mode);
    }
  }
  const char *toString(void) { return buffer; }
};

// TODO(gabylb): replace 1024 and 1025 in this .cc by PATH_MAX...

void __fdinfo(int fd) {
  struct stat st;
  int rc;

  char buf[1024];
  struct tm tm;

  rc = fstat(fd, &st);
  if (-1 == rc) {
    __console_printf("fd %d invalid, errno=%d", fd, errno);
    return;
  }
  if (S_ISBLK(st.st_mode)) {
    __console_printf("fd %d IS_BLK", fd);
  } else if (S_ISDIR(st.st_mode)) {
    __console_printf("fd %d IS_DIR", fd);
  } else if (S_ISCHR(st.st_mode)) {
    __console_printf("fd %d IS_CHR", fd);
  } else if (S_ISFIFO(st.st_mode)) {
    __console_printf("fd %d IS_FIFO", fd);
  } else if (S_ISREG(st.st_mode)) {
    __console_printf("fd %d IS_REG", fd);
  } else if (S_ISLNK(st.st_mode)) {
    __console_printf("fd %d IS_LNK", fd);
  } else if (S_ISSOCK(st.st_mode)) {
    __console_printf("fd %d IS_SOCK", fd);
  } else if (S_ISVMEXTL(st.st_mode)) {
    __console_printf("fd %d IS_VMEXTL", fd);
  }
  __console_printf("fd %d perm %04x\n", fd, 0xffff & st.st_mode);
  __console_printf("fd %d ino %d", fd, st.st_ino);
  __console_printf("fd %d dev %d", fd, st.st_dev);
  __console_printf("fd %d rdev %d", fd, st.st_rdev);
  __console_printf("fd %d nlink %d", fd, st.st_nlink);
  __console_printf("fd %d uid %d", fd, st.st_uid);
  __console_printf("fd %d gid %d", fd, st.st_gid);
  __console_printf("fd %d atime %s", fd,
                   asctime_r(localtime_r(&st.st_atime, &tm), buf));
  __console_printf("fd %d mtime %s", fd,
                   asctime_r(localtime_r(&st.st_mtime, &tm), buf));
  __console_printf("fd %d ctime %s", fd,
                   asctime_r(localtime_r(&st.st_ctime, &tm), buf));
  __console_printf("fd %d createtime %s", fd,
                   asctime_r(localtime_r(&st.st_createtime, &tm), buf));
  __console_printf("fd %d reftime %s", fd,
                   asctime_r(localtime_r(&st.st_reftime, &tm), buf));
  __console_printf("fd %d auditoraudit %d", fd, st.st_auditoraudit);
  __console_printf("fd %d useraudit %d", fd, st.st_useraudit);
  __console_printf("fd %d blksize %d", fd, st.st_blksize);
  __console_printf("fd %d auditid %-.*s", fd, 16, st.st_auditid);
  __console_printf("fd %d ccsid %d", fd, st.st_tag.ft_ccsid);
  __console_printf("fd %d txt %d", fd, st.st_tag.ft_txtflag);
  __console_printf("fd %d blkcnt  %ld", fd, st.st_blocks);
  __console_printf("fd %d genvalue %d", fd, st.st_genvalue);
  __console_printf("fd %d fid %-.*s", fd, 8, st.st_fid);
  __console_printf("fd %d filefmt %d", fd, st.st_filefmt);
  __console_printf("fd %d fspflag2 %d", fd, st.st_fspflag2);
  __console_printf("fd %d seclabel %-.*s", fd, 8, st.st_seclabel);
}

void __perror(const char *str) {
  char buf[1024];
  int err = errno;
  int rc = strerror_r(err, buf, sizeof(buf));
  if (rc == EINVAL) {
    __console_printf("%s: %d is not a valid errno", str, err);
  } else {
    __console_printf("%s: %s", str, buf);
  }
  errno = err;
}

static int __eventinfo(char *buffer, size_t size, short poll_event) {
  size_t bytes = 0;
  if (size > 0 && ((poll_event & POLLRDNORM) == POLLRDNORM)) {
    bytes += snprintf(buffer + bytes, size - bytes, "%s ", "POLLRDNORM");
  }
  if (size > 0 && ((poll_event & POLLRDBAND) == POLLRDBAND)) {
    bytes += snprintf(buffer + bytes, size - bytes, "%s ", "POLLRDBAND");
  }
  if (size > 0 && ((poll_event & POLLWRNORM) == POLLWRNORM)) {
    bytes += snprintf(buffer + bytes, size - bytes, "%s ", "POLLWRNORM");
  }
  if (size > 0 && ((poll_event & POLLWRBAND) == POLLWRBAND)) {
    bytes += snprintf(buffer + bytes, size - bytes, "%s ", "POLLWRBAND");
  }
  if (size > 0 && ((poll_event & POLLIN) == POLLIN)) {
    bytes += snprintf(buffer + bytes, size - bytes, "%s ", "POLLIN");
  }
  if (size > 0 && ((poll_event & POLLPRI) == POLLPRI)) {
    bytes += snprintf(buffer + bytes, size - bytes, "%s ", "POLLPRI");
  }
  if (size > 0 && ((poll_event & POLLOUT) == POLLOUT)) {
    bytes += snprintf(buffer + bytes, size - bytes, "%s ", "POLLOUT");
  }
  if (size > 0 && ((poll_event & POLLERR) == POLLERR)) {
    bytes += snprintf(buffer + bytes, size - bytes, "%s ", "POLLERR");
  }
  if (size > 0 && ((poll_event & POLLHUP) == POLLHUP)) {
    bytes += snprintf(buffer + bytes, size - bytes, "%s ", "POLLHUP");
  }
  if (size > 0 && ((poll_event & POLLNVAL) == POLLNVAL)) {
    bytes += snprintf(buffer + bytes, size - bytes, "%s ", "POLLNVAL");
  }
  return bytes;
}

int __dpoll(void *array, unsigned int count, int timeout) {
  void *reg15 = __uss_base_address()[932 / 4]; // BPX4POL offset is 932
  int rv, rc, rn;
  int inf = (timeout == -1);
  int tid = (int)(pthread_self().__ & 0x7fffffffUL);

  typedef struct pollitem {
    int msg_fd;
    short events;
    short revents;
  } pollitem_t;

  pollitem_t *item;
  int fd_cnt = count & 0x0ffff;
  int msg_cnt = (count >> 16) & 0x0ffff;

  int cnt = 9999;
  if (inf)
    timeout = 60 * 1000;
  const void *argv[] = {&array, &count, &timeout, &rv, &rc, &rn};
  __asm volatile(" basr 14,%0\n"
                 : "+NR:r15"(reg15)
                 : "NR:r1"(&argv)
                 : "r0", "r14");
  if (rv != 0 && rv != -1) {
    int fd_res_cnt = rv & 0x0ffff;
  }
  while (rv == 0 && inf && cnt > 0) {
    char event_msg[128];
    char revent_msg[128];
    __console_printf("%s:%s:%d end tid %d count %08x timeout %d rv %08x rc %d "
                     "timeout count-down %d",
                     __FILE__, __FUNCTION__, __LINE__,
                     (int)(pthread_self().__ & 0x7fffffffUL), count, timeout,
                     rv, rc, cnt);
    pollitem_t *fds = (pollitem_t *)array;
    int i;
    i = 0;
    for (; i < fd_cnt; ++i) {
      if (fds[i].msg_fd != -1) {
        size_t s1 = __eventinfo(event_msg, 128, fds[i].events);
        size_t s2 = __eventinfo(revent_msg, 128, fds[i].revents);
        __console_printf("%s:%s:%d tid:%d ary-i:%d %s %d/0x%04x/0x%04x",
                         __FILE__, __FUNCTION__, __LINE__, tid, i, "fd",
                         fds[i].msg_fd, fds[i].events, fds[i].revents);
        __console_printf(
            "%s:%s:%d tid:%d ary-i:%d %s %d event:%-.*s revent:%-.*s", __FILE__,
            __FUNCTION__, __LINE__, tid, i, "fd", fds[i].msg_fd, s1, event_msg,
            s2, revent_msg);
      }
    }
    for (; i < (fd_cnt + msg_cnt); ++i) {
      if (fds[i].msg_fd != -1) {
        size_t s1 = __eventinfo(event_msg, 128, fds[i].events);
        size_t s2 = __eventinfo(revent_msg, 128, fds[i].revents);
        __console_printf("%s:%s:%d tid:%d ary-i:%d %s %d/0x%04x/0x%04x",
                         __FILE__, __FUNCTION__, __LINE__, tid, i, "msgq",
                         fds[i].msg_fd, fds[i].events, fds[i].revents);
        __console_printf(
            "%s:%s:%d tid:%d ary-i:%d %s %d event:%-.*s revent:%-.*s", __FILE__,
            __FUNCTION__, __LINE__, tid, i, "msgq", fds[i].msg_fd, s1,
            event_msg, s2, revent_msg);
      }
    }
    reg15 = __uss_base_address()[932 / 4]; // BPX4POL offset is 932
    __asm volatile(" basr 14,%0\n"
                   : "+NR:r15"(reg15)
                   : "NR:r1"(&argv)
                   : "r0", "r14");
    --cnt;
  }
  if (-1 == rv) {
    errno = rc;
    __perror("poll");
  }
  return rv;
}

// for debugging use
ssize_t __write(int fd, const void *buffer, size_t sz) {
  void *reg15 = __uss_base_address()[220 / 4]; // BPX4WRT offset is 220
  int rv, rc, rn;
  void *alet = 0;
  unsigned int size = sz;
  const void *argv[] = {&fd, &buffer, &alet, &size, &rv, &rc, &rn};
  __asm volatile(" basr 14,%0\n"
                 : "+NR:r15"(reg15)
                 : "NR:r1"(&argv)
                 : "r0", "r14");
  if (-1 == rv) {
    errno = rc;
    __perror("write");
  }
  if (rv > 0) {
    __console_printf("%s:%s:%d fd %d sz %d return %d type is %s\n", __FILE__,
                     __FUNCTION__, __LINE__, fd, sz, rv, Fdtype(fd).toString());
  } else {
    __console_printf("%s:%s:%d fd %d sz %d return %d errno %d type is %s\n",
                     __FILE__, __FUNCTION__, __LINE__, fd, sz, rv, rc,
                     Fdtype(fd).toString());
  }
  return rv;
}

// for debugging use
ssize_t __read(int fd, void *buffer, size_t sz) {
  void *reg15 = __uss_base_address()[176 / 4]; // BPX4RED offset is 176
  int rv, rc, rn;
  void *alet = 0;
  unsigned int size = sz;
  const void *argv[] = {&fd, &buffer, &alet, &size, &rv, &rc, &rn};
  __asm volatile(" basr 14,%0\n"
                 : "+NR:r15"(reg15)
                 : "NR:r1"(&argv)
                 : "r0", "r14");
  if (-1 == rv) {
    errno = rc;
    __perror("read");
  }
  if (rv > 0) {
    __console_printf("%s:%s:%d fd %d sz %d return %d type is %s\n", __FILE__,
                     __FUNCTION__, __LINE__, fd, sz, rv, Fdtype(fd).toString());
  } else {
    __console_printf("%s:%s:%d fd %d sz %d return %d errno %d type is %s\n",
                     __FILE__, __FUNCTION__, __LINE__, fd, sz, rv, rc,
                     Fdtype(fd).toString());
  }
  return rv;
}

// for debugging use
int __close(int fd) {
  void *reg15 = __uss_base_address()[72 / 4]; // BPX4CLO offset is 72
  int rv = -1, rc = -1, rn = -1;
  const char *fdtype = Fdtype(fd).toString();
  const void *argv[] = {&fd, &rv, &rc, &rn};
  __asm volatile(" basr 14,%0\n"
                 : "+NR:r15"(reg15)
                 : "NR:r1"(&argv)
                 : "r0", "r14");
  if (-1 == rv) {
    errno = rc;
    __perror("close");
  }
  __console_printf("%s:%s:%d fd %d return %d errno %d type was %s\n", __FILE__,
                   __FUNCTION__, __LINE__, fd, rv, rc, fdtype);
  return rv;
}

// for debugging use
int __open(const char *file, int oflag, int mode) asm("@@A00144");
int __open(const char *file, int oflag, int mode) {
  void *reg15 = __uss_base_address()[156 / 4]; // BPX4OPN offset is 156
  int rv, rc, rn, len;
  char name[1024];
  strncpy(name, file, 1024);
  __a2e_s(name);
  len = strlen(name);
  const void *argv[] = {&len, name, &oflag, &mode, &rv, &rc, &rn};
  __asm volatile(" basr 14,%0\n"
                 : "+NR:r15"(reg15)
                 : "NR:r1"(&argv)
                 : "r0", "r14");
  if (-1 == rv) {
    errno = rc;
    __perror("open");
  }
  __console_printf("%s:%s:%d fd %d errno %d open %s (part-1)\n", __FILE__,
                   __FUNCTION__, __LINE__, rv, rc, file);
  __console_printf("%s:%s:%d fd %d oflag %08x mode %08x type is %s (part-2)\n",
                   __FILE__, __FUNCTION__, __LINE__, rv, oflag, mode,
                   Fdtype(rv).toString());
  return rv;
}
#endif // if TRACE_ON - for debugging use

static int return_abspath(char *out, int size, const char *path_file) {
  char buffer[1025];
  char *res = 0;
  if (path_file[0] != '/')
    res = __realpath_a(path_file, buffer);
  return __snprintf_a(out, size, "%s", res ? buffer : path_file);
}

int __find_file_in_path(char *out, int size, const char *envvar,
                        const char *file) {
  char *start = (char *)envvar;
  char path[1025];
  char path_file[1025];
  char *p = path;
  int len = 0;
  struct stat st;
  while (*start && (p < (path + 1024))) {
    if (*start == ':') {
      p = path;
      ++start;
      if (len > 0) {
        for (; len > 0 && path[len - 1] == '/'; --len)
          ;
        __snprintf_a(path_file, 1025, "%-.*s/%s", len, path, file);
        if (0 == __stat_a(path_file, &st)) {
          return return_abspath(out, size, path_file);
        }
        len = 0;
      }
    } else {
      ++len;
      *p++ = *start++;
    }
  }
  if (len > 0) {
    for (; len > 0 && path[len - 1] == '/'; --len)
      ;
    __snprintf_a(path_file, 1025, "%-.*s/%s", len, path, file);
    if (0 == __stat_a(path_file, &st)) {
      return return_abspath(out, size, path_file);
    }
  }
  return 0;
}

int __chgfdccsid(int fd, unsigned short ccsid) {
  attrib_t attr;
  memset(&attr, 0, sizeof(attr));
  attr.att_filetagchg = 1;
  attr.att_filetag.ft_ccsid = ccsid;
  if (ccsid != FT_BINARY) {
    attr.att_filetag.ft_txtflag = 1;
  }
  return __fchattr(fd, &attr, sizeof(attr));
}

int __setfdccsid(int fd, int t_ccsid) {
  attrib_t attr;
  memset(&attr, 0, sizeof(attr));
  attr.att_filetagchg = 1;
  attr.att_filetag.ft_txtflag = (t_ccsid >> 16);
  attr.att_filetag.ft_ccsid = (t_ccsid & 0x0ffff);
  return __fchattr(fd, &attr, sizeof(attr));
}

int __getfdccsid(int fd) {
  struct stat st;
  int rc;
  rc = fstat(fd, &st);
  if (rc != 0)
    return -1;
  unsigned short ccsid = st.st_tag.ft_ccsid;
  if (st.st_tag.ft_txtflag) {
    return 65536 + ccsid;
  }
  return ccsid;
}

static void getMemUsageLogFilename(char* outName, const char *nameInEnv,
                                   size_t maxlen) {
  std::string str(nameInEnv);
  size_t s = str.find("%PID%");
  if (s != std::string::npos) {
    str.replace(s, 5, std::to_string(getpid()));
    __gLogMemoryShowPid = false;
  }
  s = str.find("%PPID%");
  if (s != std::string::npos)
    str.replace(s, 6, std::to_string(getppid()));
  strncpy(outName, str.c_str(), maxlen);
}

void update_memlogging(__zinit *zinit_ptr, const char *envar, bool memacnt) {
  if (!zinit_ptr)
    return;
  zoslib_config_t &config = zinit_ptr->config;

  char *p;
  if (envar)
    getMemUsageLogFilename(__gMemoryUsageLogFile, envar, sizeof(__gMemoryUsageLogFile));
  else if (p = getenv(config.MEMORY_USAGE_LOG_FILE_ENVAR))
    getMemUsageLogFilename(__gMemoryUsageLogFile, p, sizeof(__gMemoryUsageLogFile));
  else if (memacnt)
    strncpy(__gMemoryUsageLogFile, "stderr", sizeof(__gMemoryUsageLogFile));

  if (*__gMemoryUsageLogFile)
    __gLogMemoryUsage = true;
  else
    __gLogMemoryUsage = false;
}

void update_memlogging_level(__zinit *zinit_ptr, const char *envar) {
  if (!zinit_ptr)
    return;
  zoslib_config_t &config = zinit_ptr->config;

  char *penv = getenv(config.MEMORY_USAGE_LOG_LEVEL_ENVAR);
  if (penv && __doLogMemoryUsage()) {
    // Errors and start/terminating messages are always displayed.
    if (*penv == MEMLOG_LEVEL_ALL)
      __gLogMemoryAll = true;  // display all messages
    else if (*penv == MEMLOG_LEVEL_WARNING)
      __gLogMemoryWarning = true; // warnings only
  }
}

extern "C" int __doLogMemoryUsage() { return __gLogMemoryUsage; }

extern "C" void __setLogMemoryUsage(bool v) { __gLogMemoryUsage = v; }

extern "C" char *__getMemoryUsageLogFile() { return __gMemoryUsageLogFile; }

extern "C" int __doLogMemoryAll() { return __gLogMemoryAll; }

extern "C" int __doLogMemoryWarning() {
  return __gLogMemoryAll || __gLogMemoryWarning;
}

// Defined in zos.cc, no need to expose it:
extern void __setLogMemoryUsage(bool value);

int __getLogMemoryFileNo() {
  static int fn = fileno(fp_memprintf);
  return fn;
}

void __memprintf(const char *format, ...) {
  if (!__doLogMemoryUsage())
    return;

  va_list args;
  va_start(args, format);

  static const char *fname = __getMemoryUsageLogFile();
  static bool isstderr = !strcmp(fname, "stderr");
  static bool isstdout = !strcmp(fname, "stdout");
  if (!fp_memprintf) {
    fp_memprintf = isstderr ? stderr : \
                   isstdout ? stdout : \
                   fopen(fname, "a+");
  }
  if (!fp_memprintf) {
    va_end(args);
    perror(fname);
    __setLogMemoryUsage(false);
    return;
  }
  char buf[PATH_MAX*2];
  vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);
  if (__gLogMemoryShowPid)
    fprintf(fp_memprintf, "MEM pid=%d tid=%d %s", getpid(), gettid(), buf);
  else
    fprintf(fp_memprintf, "tid=%d %s", gettid(), buf);
}


#if defined(ZOSLIB_OVERRIDE_CLIB) || defined(ZOSLIB_OVERRIDE_CLIB_SOCKET)

int __getsockname_orig(int, struct sockaddr * __restrict__, socklen_t * __restrict__) asm("@@A00409");

int __getsockname_fixed(int sockfd, struct sockaddr * __restrict__ addr,
                        socklen_t * __restrict__ addrlen) {
  union {
    struct sockaddr sa;
    char tmpbuf[256]; /* because sa_len is unsigned char */
  } u;
  socklen_t addr_alloclen = *addrlen;

  memset(&u, 0, sizeof(u));
  int rc = __getsockname_orig(sockfd, &u.sa, addrlen);
  if (rc == -1) {
    return rc;
  }
  if (*addrlen > 0) {
    memcpy(addr, &u.sa, std::min(addr_alloclen, *addrlen));
  }
  return 0;
}

#endif  // ZOSLIB_OVERRIDE_CLIB || ZOSLIB_OVERRIDE_CLIB_SOCKET

#ifdef __cplusplus
}
#endif
