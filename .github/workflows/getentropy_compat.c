// Local definition of getentropy() for the old-glibc linux-x86_64 build.
//
// That target is built inside ubuntu:20.04 (glibc 2.31) but must run on
// glibc 2.17. abseil 20240722 - pulled in by the gRPC version every target now
// shares - calls getentropy() whenever it is compiled against glibc >= 2.25:
//
//   // absl/random/internal/seed_material.cc
//   // glibc >= 2.25 has getentropy()
//   int result = getentropy(buffer, to_read);
//
// so the linked clangd imports getentropy@GLIBC_2.25 and
// lib_compat_test.py --lib=GLIBC_2.18 rejects it. (autobuild.yaml never sees
// this: it pins gRPC 1.36.3, whose abseil predates that code.)
//
// getentropy is unlike the symbols in lib_compat.h: it did not exist at all
// before glibc 2.25, so there is no older version to force with .symver. The
// fix is to define it here instead. With a definition on the link line the
// reference is resolved locally, so no versioned import is recorded and the
// binary keeps working on 2.17 systems. Everything used below has been in
// glibc since 2.2.5.
//
// This file is compiled by the workflow into getentropy_compat.o and appended
// to CMAKE_EXE_LINKER_FLAGS_RELEASE; it is not compiled into any library.

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <sys/syscall.h>
#include <unistd.h>

#if defined(SYS_getrandom) && !defined(__NR_getrandom)
#define __NR_getrandom SYS_getrandom
#endif

int getentropy(void *buffer, size_t length) {
  // Same contract as glibc: at most 256 bytes per call, EIO on failure.
  if (buffer == NULL || length > 256) {
    errno = EIO;
    return -1;
  }

  unsigned char *out = (unsigned char *)buffer;
  size_t done = 0;

#ifdef __NR_getrandom
  while (done < length) {
    long got = syscall(__NR_getrandom, out + done, length - done, 0);
    if (got > 0) {
      done += (size_t)got;
      continue;
    }
    if (got < 0 && errno == EINTR)
      continue;
    // ENOSYS (kernel < 3.17) or EPERM in a restricted sandbox: fall back.
    break;
  }
  if (done == length)
    return 0;
#endif

  int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
  if (fd < 0)
    return -1;
  while (done < length) {
    ssize_t got = read(fd, out + done, length - done);
    if (got > 0) {
      done += (size_t)got;
      continue;
    }
    if (got < 0 && errno == EINTR)
      continue;
    close(fd);
    errno = EIO;
    return -1;
  }
  close(fd);
  return 0;
}
