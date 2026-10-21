#ifndef H1_DESCRIPTOR_PROBE_H
#define H1_DESCRIPTOR_PROBE_H

#ifdef __ANDROID__

#include "../nvn.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>

typedef struct {
    uintptr_t texture_pool;
    uintptr_t sampler_pool;
    unsigned attempts;
    unsigned successes;
} H1DescriptorProbe;

static H1DescriptorProbe h1_descriptor_probe;

static bool h1_probe_read(uintptr_t address, void *dst, size_t size)
{
    if (!address || !dst || !size || size > 4096)
        return false;

    if (address > UINTPTR_MAX - size)
        return false;

    struct iovec local = { dst, size };
    struct iovec remote = { (void *)address, size };

    ssize_t result = process_vm_readv(
        getpid(), &local, 1, &remote, 1, 0
    );

    return result == (ssize_t)size;
}

static bool h1_probe_pool(
    uintptr_t address,
    uint32_t index,
    size_t element_size,
    NvnDescPool *pool,
    uintptr_t *element_address)
{
    if (!h1_probe_read(address, pool, sizeof(*pool)))
        return false;

    if (pool->count <= 0 ||
        pool->count > 1048576 ||
        index >= (uint32_t)pool->count ||
        !pool->entries)
        return false;

    if ((size_t)index > SIZE_MAX / element_size)
        return false;

    size_t offset = (size_t)index * element_size;
    uintptr_t base = (uintptr_t)pool->entries;

    if (base > UINTPTR_MAX - offset)
        return false;

    *element_address = base + offset;
    return true;
}

static void h1_probe_descriptor_record(const CmdRec *r)
{
    if (!r)
        return;

    switch (r->op) {
    case CMD_SET_TEXTURE_POOL:
        h1_descriptor_probe.texture_pool = (uintptr_t)r->x[0];
        return;

    case CMD_SET_SAMPLER_POOL:
        h1_descriptor_probe.sampler_pool = (uintptr_t)r->x[0];
        return;

    case CMD_BIND_TEXTURES:
        break;

    default:
        return;
    }

    /* Capture only the first four valid-looking binding attempts. */
    if (h1_descriptor_probe.attempts >= 4)
        return;

    if (r->size < sizeof(CmdRec))
        return;

    uint64_t payload_size = r->size - sizeof(CmdRec);
    uint64_t count = r->x[2];

    if (!count || count > 32 || count > payload_size / sizeof(uint64_t))
        return;

    for (uint64_t i = 0;
         i < count && h1_descriptor_probe.attempts < 4;
         ++i) {

        uint64_t handle = 0;
        memcpy(&handle, r->extra + i * sizeof(handle), sizeof(handle));

        if (!handle)
            continue;

        h1_descriptor_probe.attempts++;

        uint32_t tic = (uint32_t)(handle & 0xFFFFFu);
        uint32_t tsc = (uint32_t)((handle >> 20) & 0xFFFu);

        fprintf(stderr,
            "[h1-descriptor] stage=%" PRIu64
            " slot=%" PRIu64
            " handle=0x%016" PRIx64
            " tic=%u tsc=%u\n",
            r->x[0], r->x[1] + i, handle, tic, tsc);

        NvnDescPool tpool = {0};
        NvnDescPool spool = {0};
        uintptr_t texture_entry_address = 0;
        uintptr_t sampler_entry_address = 0;

        bool texture_index_valid = h1_probe_pool(
            h1_descriptor_probe.texture_pool,
            tic,
            sizeof(NvnTexEntry),
            &tpool,
            &texture_entry_address
        );

        bool sampler_index_valid = h1_probe_pool(
            h1_descriptor_probe.sampler_pool,
            tsc,
            sizeof(NvnSamplerDesc),
            &spool,
            &sampler_entry_address
        );

        fprintf(stderr,
            "[h1-descriptor] texture_pool_index_valid=%u "
            "texture_pool_count=%d "
            "sampler_pool_index_valid=%u "
            "sampler_pool_count=%d\n",
            texture_index_valid ? 1u : 0u,
            texture_index_valid ? tpool.count : -1,
            sampler_index_valid ? 1u : 0u,
            sampler_index_valid ? spool.count : -1);

        if (!texture_index_valid)
            continue;

        NvnTexEntry entry = {0};

        if (!h1_probe_read(
                texture_entry_address,
                &entry,
                sizeof(entry))) {
            fprintf(stderr,
                "[h1-descriptor] texture_entry_read=FAIL\n");
            continue;
        }

        if (!entry.tex) {
            fprintf(stderr,
                "[h1-descriptor] texture_entry_null=1\n");
            continue;
        }

        NvnTexture texture = {0};

        if (!h1_probe_read(
                (uintptr_t)entry.tex,
                &texture,
                sizeof(texture))) {
            fprintf(stderr,
                "[h1-descriptor] texture_metadata_read=FAIL\n");
            continue;
        }

        h1_descriptor_probe.successes++;

        fprintf(stderr,
            "[h1-descriptor] texture_resolved=1 "
            "width=%u height=%u depth=%u "
            "format=0x%x target=%u "
            "levels=%u samples=%u "
            "storage=0x%016" PRIx64
            " stride=%" PRIu64
            " has_view=%u\n",
            texture.width,
            texture.height,
            texture.depth,
            texture.format,
            texture.target,
            texture.levels,
            texture.samples,
            texture.addr,
            texture.stride,
            entry.has_view ? 1u : 0u);
    }
}

#endif
#endif