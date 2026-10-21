#include "command_topology.h"
#include <string.h>
static bool same(GlesTopologyListKey a, GlesTopologyListKey b) { return a.address==b.address && a.generation==b.generation; }
static bool valid(GlesTopologyListKey k) { return k.address && k.generation; }
static int index_of(const GlesCommandTopology *g,GlesTopologyListKey k) {
    for(size_t i=0;i<g->nodes;i++) if(same(g->keys[i],k)) return (int)i;
    return -1;
}
bool gles_topology_add_list(GlesCommandTopology *g,GlesTopologyListKey k) {
    if(!g||!valid(k)||g->nodes>=GLES_TOPOLOGY_MAX_NODES||index_of(g,k)>=0) return false;
    for(size_t i=0;i<g->nodes;i++) if(g->keys[i].address==k.address) return false;
    g->keys[g->nodes++]=k; return true;
}
bool gles_topology_add_rt(GlesCommandTopology *g,GlesTopologyListKey owner,GlesRecordCoordinatorKey r) {
    if(!g||index_of(g,owner)<0||!r.address||!r.generation||g->edge_count>=GLES_TOPOLOGY_MAX_EDGES) return false;
    for(size_t i=0;i<g->edge_count;i++) {
        if(g->edges[i].kind==GLES_TOPOLOGY_RT &&
           g->edges[i].record.address==r.address) return false;
        if(same(g->edges[i].owner,owner) &&
           g->edges[i].kind==GLES_TOPOLOGY_JUMP) return false;
    }
    g->edges[g->edge_count++]=(GlesTopologyEdge){.owner=owner,.kind=GLES_TOPOLOGY_RT,.record=r}; return true;
}
bool gles_topology_add_link(GlesCommandTopology *g,GlesTopologyListKey owner,GlesTopologyKind kind,GlesTopologyListKey target) {
    if(!g||index_of(g,owner)<0||!valid(target)||
       index_of(g,target)<0||
       (kind!=GLES_TOPOLOGY_CALL&&kind!=GLES_TOPOLOGY_JUMP)||
       g->edge_count>=GLES_TOPOLOGY_MAX_EDGES) return false;
    for(size_t i=0;i<g->edge_count;i++)
        if(same(g->edges[i].owner,owner) &&
           g->edges[i].kind==GLES_TOPOLOGY_JUMP) return false;
    g->edges[g->edge_count++]=(GlesTopologyEdge){.owner=owner,.kind=kind,.target=target}; return true;
}
typedef struct {const GlesCommandTopology *g;GlesTopologyLive live;void *ctx;GlesSubmissionManifest *manifest;bool visiting[GLES_TOPOLOGY_MAX_NODES];size_t steps;} Walk;
static bool walk(Walk *w,GlesTopologyListKey key,unsigned depth) {
    int idx=index_of(w->g,key);
    if(idx<0||depth>GLES_TOPOLOGY_MAX_DEPTH||w->visiting[idx]||++w->steps>GLES_TOPOLOGY_MAX_STEPS) return false;
    w->visiting[idx]=true;
    for(size_t i=0;i<w->g->edge_count;i++) {
        GlesTopologyEdge e=w->g->edges[i];
        if(!same(e.owner,key)) continue;
        if(++w->steps>GLES_TOPOLOGY_MAX_STEPS) return false;
        if(e.kind==GLES_TOPOLOGY_RT) {
            if(!w->live(e.record,w->ctx)) return false;
            if(!gles_submission_manifest_contains(w->manifest,e.record) && !gles_submission_manifest_add(w->manifest,e.record)) return false;
        } else if(e.kind==GLES_TOPOLOGY_CALL) {
            if(!walk(w,e.target,depth+1)) return false;
        } else if(e.kind==GLES_TOPOLOGY_JUMP) {
            if(!walk(w,e.target,depth+1)) return false;
            w->visiting[idx]=false;
            return true;
        } else return false;
    }
    w->visiting[idx]=false;
    return true;
}
bool gles_topology_manifest(const GlesCommandTopology *g,GlesTopologyListKey root,GlesTopologyLive live,void *ctx,GlesSubmissionManifest *out) {
    if(!out) return false;
    memset(out,0,sizeof(*out));
    if(!g||!live||g->nodes>GLES_TOPOLOGY_MAX_NODES||g->edge_count>GLES_TOPOLOGY_MAX_EDGES||index_of(g,root)<0) return false;
    if(!gles_submission_manifest_begin(out)) return false;
    Walk w={.g=g,.live=live,.ctx=ctx,.manifest=out};
    if(!walk(&w,root,0)||!gles_submission_manifest_validate(out)) {gles_submission_manifest_clear(out);return false;}
    return true;
}