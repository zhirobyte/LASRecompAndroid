#include "command_topology.h"
#include <stdio.h>
#include <string.h>
static int checks, failures;
#define CHECK(x) do { checks++; if(!(x)) {failures++;fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);} } while(0)
typedef struct {GlesRecordCoordinatorKey a,b;bool a_live,b_live;} Live;
static bool is_live(GlesRecordCoordinatorKey k,void *p) {
    Live *s=p;
    if(k.address==s->a.address && k.generation==s->a.generation) return s->a_live;
    if(k.address==s->b.address && k.generation==s->b.generation) return s->b_live;
    return false;
}
int main(void) {
    GlesCommandTopology g={0};GlesSubmissionManifest m={0};
    GlesTopologyListKey root={0x1000,1},jump={0x2000,2},call={0x3000,3},other={0x4000,4};
    GlesRecordCoordinatorKey a={.address=0x1010,.generation=11},b={.address=0x3010,.generation=12};
    Live live={.a=a,.b=b,.a_live=true,.b_live=true};
    CHECK(gles_topology_add_list(&g,root));
    CHECK(gles_topology_add_list(&g,jump));
    CHECK(gles_topology_add_list(&g,call));
    CHECK(gles_topology_add_list(&g,other));
    CHECK(!gles_topology_add_list(&g,(GlesTopologyListKey){0x1000,9}));
    CHECK(gles_topology_add_rt(&g,root,a));
    CHECK(gles_topology_add_link(&g,root,GLES_TOPOLOGY_JUMP,jump));
    CHECK(gles_topology_add_link(&g,jump,GLES_TOPOLOGY_CALL,call));
    CHECK(gles_topology_add_rt(&g,call,b));
    CHECK(gles_topology_manifest(&g,root,is_live,&live,&m));
    CHECK(m.count==2);
    CHECK(gles_submission_manifest_contains(&m,a));
    CHECK(gles_submission_manifest_contains(&m,b));
    CHECK(!gles_submission_manifest_contains(&m,(GlesRecordCoordinatorKey){.address=a.address,.generation=99}));
    CHECK(gles_topology_manifest(&g,other,is_live,&live,&m));
    CHECK(m.count==0);
    live.a_live=false;
    CHECK(!gles_topology_manifest(&g,root,is_live,&live,&m));
    CHECK(!m.active && m.count==0);
    live.a_live=true;live.b_live=false;
    CHECK(!gles_topology_manifest(&g,root,is_live,&live,&m));
    live.b_live=true;
    CHECK(gles_topology_add_link(&g,call,GLES_TOPOLOGY_CALL,root));
    CHECK(!gles_topology_manifest(&g,root,is_live,&live,&m));
    CHECK(!m.active);
    CHECK(!gles_topology_add_rt(&g,root,(GlesRecordCoordinatorKey){.address=a.address,.generation=99}));
    CHECK(!gles_topology_add_link(&g,root,GLES_TOPOLOGY_RT,call));
    CHECK(!gles_topology_manifest(&g,(GlesTopologyListKey){.address=root.address,.generation=99},is_live,&live,&m));
    CHECK(!gles_topology_manifest(&g,root,NULL,&live,&m));
    CHECK(!gles_topology_manifest(&g,root,is_live,&live,NULL));
    printf("R8D-4E4F-3A: %d/%d checks passed\n",checks-failures,checks);
    return failures?1:0;
}