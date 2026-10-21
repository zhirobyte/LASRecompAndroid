#ifndef H1_GUEST_MEMORY_READ_H
#define H1_GUEST_MEMORY_READ_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __ANDROID__
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#endif

/*
 * Diagnostic-only bounded guest memory snapshot.
 *
 * Does not directly dereference the guest pointer.
 * Does not alter guest memory.
 * Returns false on short reads or inaccessible memory.
 *
 * The caller must validate that the address belongs to an
 * expected guest allocation before requesting a snapshot.
 */
static inline bool h1_guest_memory_read(
    uint64_t address,
    void *destination,
    size_t length)
{
#ifdef __ANDROID__
    if (!destination || !address || !length || length > 4096)
        return false;

    if (address > UINTPTR_MAX ||
        length - 1 > UINTPTR_MAX - (uintptr_t)address)
        return false;

    struct iovec local = {
        .iov_base = destination,
        .iov_len = length
    };

    struct iovec remote = {
        .iov_base = (void *)(uintptr_t)address,
        .iov_len = length
    };

    ssize_t result = process_vm_readv(
        getpid(), &local, 1, &remote, 1, 0);

    return result == (ssize_t)length;
#else
    (void)address;
    (void)destination;
    (void)length;
    return false;
#endif
}

#endif