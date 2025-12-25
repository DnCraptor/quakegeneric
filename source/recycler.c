#include "recycler.h"
#include "xipstream.h"
#include "quakedef.h"
#include "r_local.h"
#include "r_shared.h"

static void *zba_edgebuf_rover;

void RC_NewFrame() {
    // reset Z-Buffer allocator
    ZBA_Reset();

    // allocate edge buffer
    if (r_numallocatededges > NUMSTACKEDGES)
	{
		edgebuf_swap = auxedges;
	}
	else
	{
		edgebuf_swap = ZBA_Alloc(sizeof(edge_t)*(NUMSTACKEDGES+RESERVED_EDGES));
	}

    zba_edgebuf_rover = ZBA_GetRover();

    // allocate and load compressed PVS
    // TODO
}

void RC_EndFrame() {
    ZBA_Reset();
}

void RC_AbortNewFrame() {
    xipstream_abort();
    edgebuf_swap = NULL;        // :grins:
}

void RC_WaitForPreloadEnd() {
    xipstream_wait_blocking();
}