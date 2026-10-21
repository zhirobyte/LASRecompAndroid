#include "recording_transition.h"
#include <string.h>

static bool same(
    GlesTopologyListKey a,
    GlesTopologyListKey b)
{
    return a.address == b.address &&
           a.generation == b.generation;
}

static int find_list(
    const GlesRecordingTransition *state,
    GlesTopologyListKey key)
{
    if (!state || !key.address || !key.generation ||
        state->graph.nodes > GLES_TOPOLOGY_MAX_NODES)
        return -1;

    for (size_t i = 0; i < state->graph.nodes; ++i) {
        if (same(state->graph.keys[i], key))
            return (int)i;
    }

    return -1;
}

static bool ready(GlesRecordingTransition *state)
{
    return state && state->recording && !state->failed;
}

bool gles_transition_begin(
    GlesRecordingTransition *state,
    GlesTopologyListKey root)
{
    if (!state || state->recording || state->failed)
        return false;

    if (!gles_topology_add_list(&state->graph, root)) {
        state->failed = true;
        return false;
    }

    state->active = root;
    state->recording = true;
    return true;
}

bool gles_transition_rt(
    GlesRecordingTransition *state,
    GlesRecordCoordinatorKey record)
{
    if (!ready(state))
        return false;

    if (!gles_topology_add_rt(
            &state->graph, state->active, record)) {
        state->failed = true;
        return false;
    }

    return true;
}

bool gles_transition_call(
    GlesRecordingTransition *state,
    GlesTopologyListKey target)
{
    if (!ready(state))
        return false;

    int index = find_list(state, target);

    if (index < 0 || !state->finalized[index] ||
        !gles_topology_add_link(
            &state->graph,
            state->active,
            GLES_TOPOLOGY_CALL,
            target)) {
        state->failed = true;
        return false;
    }

    return true;
}

bool gles_transition_chunk(
    GlesRecordingTransition *state,
    GlesTopologyListKey continuation)
{
    if (!ready(state))
        return false;

    /*
     * Preflight all checks before modifying the graph.
     * This avoids partially registering a new chunk.
     */
    if (!continuation.address || !continuation.generation ||
        state->graph.nodes >= GLES_TOPOLOGY_MAX_NODES ||
        state->graph.edge_count >= GLES_TOPOLOGY_MAX_EDGES ||
        find_list(state, continuation) >= 0) {
        state->failed = true;
        return false;
    }

    for (size_t i = 0; i < state->graph.nodes; ++i) {
        if (state->graph.keys[i].address ==
            continuation.address) {
            state->failed = true;
            return false;
        }
    }

    /*
     * The existing topology API requires the destination
     * to be registered before adding a link.
     */
    if (!gles_topology_add_list(
            &state->graph, continuation)) {
        state->failed = true;
        return false;
    }

    if (!gles_topology_add_link(
            &state->graph,
            state->active,
            GLES_TOPOLOGY_JUMP,
            continuation)) {
        state->failed = true;
        return false;
    }

    int old = find_list(state, state->active);

    if (old < 0) {
        state->failed = true;
        return false;
    }

    state->finalized[old] = true;
    state->active = continuation;

    return true;
}

bool gles_transition_end(
    GlesRecordingTransition *state)
{
    if (!ready(state))
        return false;

    int index = find_list(state, state->active);

    if (index < 0) {
        state->failed = true;
        return false;
    }

    state->finalized[index] = true;
    state->recording = false;
    state->active = (GlesTopologyListKey){0};

    return true;
}

bool gles_transition_manifest(
    const GlesRecordingTransition *state,
    GlesTopologyListKey root,
    GlesTopologyLive live,
    void *context,
    GlesSubmissionManifest *out)
{
    if (!out)
        return false;

    memset(out, 0, sizeof(*out));

    if (!state || state->recording || state->failed ||
        !live || state->graph.nodes > GLES_TOPOLOGY_MAX_NODES)
        return false;

    /*
     * Conservative policy: require every registered list
     * to be finalized, not merely reachable lists.
     */
    for (size_t i = 0; i < state->graph.nodes; ++i) {
        if (!state->finalized[i])
            return false;
    }

    return gles_topology_manifest(
        &state->graph, root, live, context, out);
}