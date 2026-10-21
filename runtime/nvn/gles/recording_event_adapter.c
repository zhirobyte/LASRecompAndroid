#include "recording_event_adapter.h"
#include <string.h>

static bool valid_list(GlesTopologyListKey key)
{
    return key.address != 0 && key.generation != 0;
}

static bool ready(GlesRecordingEventAdapter *adapter)
{
    return adapter && adapter->recording && !adapter->failed;
}

bool gles_recording_event_begin(
    GlesRecordingEventAdapter *adapter,
    GlesTopologyListKey list)
{
    if (!adapter || adapter->recording || adapter->failed ||
        !valid_list(list))
        return false;

    if (!gles_topology_add_list(&adapter->graph, list)) {
        adapter->failed = true;
        return false;
    }

    adapter->active = list;
    adapter->recording = true;
    return true;
}

bool gles_recording_event_rt(
    GlesRecordingEventAdapter *adapter,
    GlesRecordCoordinatorKey record)
{
    if (!ready(adapter))
        return false;

    if (!gles_topology_add_rt(
            &adapter->graph, adapter->active, record)) {
        adapter->failed = true;
        return false;
    }

    return true;
}

bool gles_recording_event_call(
    GlesRecordingEventAdapter *adapter,
    GlesTopologyListKey target)
{
    if (!ready(adapter))
        return false;

    if (!gles_topology_add_link(
            &adapter->graph,
            adapter->active,
            GLES_TOPOLOGY_CALL,
            target)) {
        adapter->failed = true;
        return false;
    }

    return true;
}

bool gles_recording_event_jump(
    GlesRecordingEventAdapter *adapter,
    GlesTopologyListKey target)
{
    if (!ready(adapter))
        return false;

    /*
     * Diagnostic model: target must already be registered.
     * Real recorder integration will need a separate
     * atomic chunk-transition API.
     */
    if (!gles_topology_add_link(
            &adapter->graph,
            adapter->active,
            GLES_TOPOLOGY_JUMP,
            target)) {
        adapter->failed = true;
        return false;
    }

    adapter->recording = false;
    adapter->active = (GlesTopologyListKey){0};
    return true;
}

bool gles_recording_event_end(
    GlesRecordingEventAdapter *adapter)
{
    if (!ready(adapter))
        return false;

    adapter->recording = false;
    adapter->active = (GlesTopologyListKey){0};
    return true;
}

bool gles_recording_event_manifest(
    const GlesRecordingEventAdapter *adapter,
    GlesTopologyListKey root,
    GlesTopologyLive live,
    void *context,
    GlesSubmissionManifest *out)
{
    if (!out)
        return false;

    memset(out, 0, sizeof(*out));

    if (!adapter || adapter->failed ||
        adapter->recording || !live)
        return false;

    return gles_topology_manifest(
        &adapter->graph, root, live, context, out);
}