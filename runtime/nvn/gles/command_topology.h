#ifndef GLES_COMMAND_TOPOLOGY_H
#define GLES_COMMAND_TOPOLOGY_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "submission_manifest.h"
#define GLES_TOPOLOGY_MAX_NODES 128u
#define GLES_TOPOLOGY_MAX_EDGES 256u
#define GLES_TOPOLOGY_MAX_DEPTH 8u
#define GLES_TOPOLOGY_MAX_STEPS 4096u

typedef struct { uintptr_t address; uint64_t generation; } GlesTopologyListKey;
typedef enum { GLES_TOPOLOGY_RT=1, GLES_TOPOLOGY_JUMP=2, GLES_TOPOLOGY_CALL=3 } GlesTopologyKind;
typedef struct {
    GlesTopologyListKey owner;
    GlesTopologyKind kind;
    GlesRecordCoordinatorKey record;
    GlesTopologyListKey target;
} GlesTopologyEdge;
typedef struct {
    GlesTopologyListKey keys[GLES_TOPOLOGY_MAX_NODES];
    GlesTopologyEdge edges[GLES_TOPOLOGY_MAX_EDGES];
    size_t nodes, edge_count;
} GlesCommandTopology;

typedef bool (*GlesTopologyLive)(GlesRecordCoordinatorKey key, void *ctx);
bool gles_topology_add_list(GlesCommandTopology *graph, GlesTopologyListKey key);
bool gles_topology_add_rt(GlesCommandTopology *graph, GlesTopologyListKey owner, GlesRecordCoordinatorKey record);
bool gles_topology_add_link(GlesCommandTopology *graph, GlesTopologyListKey owner, GlesTopologyKind kind, GlesTopologyListKey target);
bool gles_topology_manifest(const GlesCommandTopology *graph, GlesTopologyListKey root, GlesTopologyLive live, void *ctx, GlesSubmissionManifest *out);
#endif