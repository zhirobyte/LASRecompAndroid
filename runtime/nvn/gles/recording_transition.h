#ifndef GLES_RECORDING_TRANSITION_H
#define GLES_RECORDING_TRANSITION_H

#include "command_topology.h"

typedef struct {
    GlesCommandTopology graph;
    GlesTopologyListKey active;
    bool finalized[GLES_TOPOLOGY_MAX_NODES];
    bool recording;
    bool failed;
} GlesRecordingTransition;

bool gles_transition_begin(
    GlesRecordingTransition *state,
    GlesTopologyListKey root);

bool gles_transition_rt(
    GlesRecordingTransition *state,
    GlesRecordCoordinatorKey record);

bool gles_transition_call(
    GlesRecordingTransition *state,
    GlesTopologyListKey target);

bool gles_transition_chunk(
    GlesRecordingTransition *state,
    GlesTopologyListKey continuation);

bool gles_transition_end(
    GlesRecordingTransition *state);

bool gles_transition_manifest(
    const GlesRecordingTransition *state,
    GlesTopologyListKey root,
    GlesTopologyLive live,
    void *context,
    GlesSubmissionManifest *out);

#endif