#ifndef GLES_RECORDING_EVENT_ADAPTER_H
#define GLES_RECORDING_EVENT_ADAPTER_H

#include "command_topology.h"

typedef struct {
    GlesCommandTopology graph;
    GlesTopologyListKey active;
    bool recording;
    bool failed;
} GlesRecordingEventAdapter;

bool gles_recording_event_begin(
    GlesRecordingEventAdapter *adapter,
    GlesTopologyListKey list);

bool gles_recording_event_rt(
    GlesRecordingEventAdapter *adapter,
    GlesRecordCoordinatorKey record);

bool gles_recording_event_call(
    GlesRecordingEventAdapter *adapter,
    GlesTopologyListKey target);

bool gles_recording_event_jump(
    GlesRecordingEventAdapter *adapter,
    GlesTopologyListKey target);

bool gles_recording_event_end(
    GlesRecordingEventAdapter *adapter);

bool gles_recording_event_manifest(
    const GlesRecordingEventAdapter *adapter,
    GlesTopologyListKey root,
    GlesTopologyLive live,
    void *context,
    GlesSubmissionManifest *out);

#endif