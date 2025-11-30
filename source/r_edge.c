/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// r_edge.c

#include "quakedef.h"
#include "r_local.h"

#if 0
// FIXME
the complex cases add new polys on most lines, so dont optimize for keeping them the same
have multiple free span lists to try to get better coherence?
low depth complexity -- 1 to 3 or so

this breaks spans at every edge, even hidden ones (bad)

have a sentinal at both ends?
#endif


edge_t	*auxedges;
edge_t	*r_edges, *edge_p, *edge_max;

surf_t	*surfaces, *surface_p, *surf_max;

// surfaces are generated in back to front order by the bsp, so if a surf
// pointer is greater than another one, it should be drawn in front
// surfaces[1] is the background, and is used as the active surface stack

short newedges[MAXHEIGHT];
short removeedges[MAXHEIGHT];

espan_t	*span_p, *max_span_p;

int		r_currentkey;

extern	int	screenwidth;

int	current_iv;

int	edge_head_u_shift20, edge_tail_u_shift20;

static void (*pdrawfunc)(void);

float	fv;

void R_GenerateSpans (void);
void R_GenerateSpansBackward (void);

void R_LeadingEdge (edge_t *edge);
void R_LeadingEdgeBackwards (edge_t *edge);
void R_TrailingEdge (surf_t *surf, edge_t *edge);


//=============================================================================


/*
==============
R_DrawCulledPolys
==============
*/
void R_DrawCulledPolys (void)
{
	surf_t			*s;
	msurface_t		*pface;

	currententity = &cl_entities[0];

	if (r_worldpolysbacktofront)
	{
		for (s=surface_p-1 ; s>&surfaces[1] ; s--)
		{
			if (!s->spans)
				continue;

			if (!(s->flags & SURF_DRAWBACKGROUND))
			{
				pface = (msurface_t *)s->data;
				R_RenderPoly (pface, 15);
			}
		}
	}
	else
	{
		for (s = &surfaces[1] ; s<surface_p ; s++)
		{
			if (!s->spans)
				continue;

			if (!(s->flags & SURF_DRAWBACKGROUND))
			{
				pface = (msurface_t *)s->data;
				R_RenderPoly (pface, 15);
			}
		}
	}
}


/*
==============
R_BeginEdgeFrame
==============
*/
void R_BeginEdgeFrame (void)
{
	int		v;

	edge_p = r_edges;
	edge_max = &r_edges[r_numallocatededges];

	surface_p = &surfaces[2];	// background is surface 1,
								//  surface 0 is a dummy
	surfaces[1].spans = NULL;	// no background spans yet
	surfaces[1].flags = SURF_DRAWBACKGROUND;

// put the background behind everything in the world
	if (r_draworder.value)
	{
		pdrawfunc = R_GenerateSpansBackward;
		surfaces[1].key = 0;
		r_currentkey = 1;
	}
	else
	{
		pdrawfunc = R_GenerateSpans;
		surfaces[1].key = 0x7FFFFFFF;
		r_currentkey = 0;
	}

// FIXME: set with memset
	for (v=r_refdef.vrect.y ; v<r_refdef.vrectbottom ; v++)
	{
		newedges[v] = removeedges[v] = 0;
	}
}

/*
==============
R_InsertNewEdges

Adds the edges in the linked list edgestoadd, adding them to the edges in the
linked list edgelist.  edgestoadd is assumed to be sorted on u, and non-empty (this is actually newedges[v]).  edgelist is assumed to be sorted on u, with a
sentinel at the end (actually, this is the active edge table starting at
edge_head.next).
==============
*/
void __not_in_flash_func(R_InsertNewEdges) (edge_t *edgestoadd, edge_t *edgelist)
{
	short    next_edge_idx;
	edge_t   *ebuf=edgebuf;
	do
	{
		next_edge_idx = edgestoadd->next;
edgesearch:
		if (edgelist->u >= edgestoadd->u)
			goto addedge;
		edgelist=ebuf+edgelist->next;
		if (edgelist->u >= edgestoadd->u)
			goto addedge;
		edgelist=ebuf+edgelist->next;
		if (edgelist->u >= edgestoadd->u)
			goto addedge;
		edgelist=ebuf+edgelist->next;
		if (edgelist->u >= edgestoadd->u)
			goto addedge;
		edgelist=ebuf+edgelist->next;
		goto edgesearch;

	// insert edgestoadd before edgelist
addedge:
		edgestoadd->next = edgelist-ebuf;
		edgestoadd->prev = edgelist->prev;
		(ebuf+edgelist->prev)->next = edgestoadd-ebuf;
		edgelist->prev = edgestoadd-ebuf;
		if (next_edge_idx == NULL) break;
		edgestoadd = ebuf+ next_edge_idx;
	} while (true);
}

/*
==============
R_RemoveEdges
==============
*/
void __not_in_flash_func(R_RemoveEdges) (edge_t *pedge)
{
	edge_t   *ebuf=edgebuf;
	do
	{
		(ebuf+pedge->next)->prev = pedge->prev;
		(ebuf+pedge->prev)->next = pedge->next;
		if (pedge->nextremove == 0) break;
		pedge = ebuf+pedge->nextremove;
	} while (true);
}

/*
==============
R_StepActiveU
==============
*/
void R_StepActiveU (edge_t *pedge)
{
	edge_t		*pnext_edge, *pwedge;
	edge_t   *ebuf=edgebuf;

	while (1)
	{
nextedge:
		pedge->u += pedge->u_step;
		if (pedge->u < (ebuf+pedge->prev)->u)
			goto pushback;
		pedge = ebuf+pedge->next;
			
		pedge->u += pedge->u_step;
		if (pedge->u < (ebuf+pedge->prev)->u)
			goto pushback;
		pedge = ebuf+pedge->next;
			
		pedge->u += pedge->u_step;
		if (pedge->u < (ebuf+pedge->prev)->u)
			goto pushback;
		pedge = ebuf+pedge->next;
			
		pedge->u += pedge->u_step;
		if (pedge->u < (ebuf+pedge->prev)->u)
			goto pushback;
		pedge = ebuf+pedge->next;
			
		goto nextedge;		
		
pushback:
		if (pedge == &edge_aftertail)
			return;
			
	// push it back to keep it sorted		
		pnext_edge = ebuf+pedge->next;

	// pull the edge out of the edge list
		(ebuf+pedge->next)->prev = pedge->prev;
		(ebuf+pedge->prev)->next = pedge->next;

	// find out where the edge goes in the edge list
		pwedge = ebuf+(ebuf+pedge->prev)->prev;

		while (pwedge->u > pedge->u)
		{
			pwedge = ebuf+pwedge->prev;
		}

	// put the edge back into the edge list
		pedge->next = pwedge->next;
		pedge->prev = pwedge-ebuf;
		(ebuf+pedge->next)->prev = pedge-ebuf;
		pwedge->next = pedge-ebuf;

		pedge = pnext_edge;
		if (pedge == &edge_tail)
			return;
	}
}

/*
==============
R_CleanupSpan
==============
*/
void __not_in_flash_func(R_CleanupSpan) ()
{
	surf_t	*surf;
	int		iu;
	espan_t	*span;

// now that we've reached the right edge of the screen, we're done with any
// unfinished surfaces, so emit a span for whatever's on top
	surf = surfaces[1].next;
	iu = edge_tail_u_shift20;
	if (iu > surf->last_u)
	{
		span = span_p++;
		span->u = surf->last_u;
		span->count = iu - span->u;
		span->v = current_iv;
		span->pnext = surf->spans;
		surf->spans = span;
	}

// reset spanstate for all surfaces in the surface stack
	do
	{
		surf->spanstate = 0;
		surf = surf->next;
	} while (surf != &surfaces[1]);
}


/*
==============
R_LeadingEdgeBackwards
==============
*/
void R_LeadingEdgeBackwards (edge_t *edge)
{
	espan_t			*span;
	surf_t			*surf, *surf2;
	int				iu;

// it's adding a new surface in, so find the correct place
	surf = &surfaces[edge->surfs[1]];

// don't start a span if this is an inverted span, with the end
// edge preceding the start edge (that is, we've already seen the
// end edge)
	if (++surf->spanstate == 1)
	{
		surf2 = surfaces[1].next;

		if (surf->key > surf2->key)
			goto newtop;

	// if it's two surfaces on the same plane, the one that's already
	// active is in front, so keep going unless it's a bmodel
		if (surf->insubmodel && (surf->key == surf2->key))
		{
		// must be two bmodels in the same leaf; don't care, because they'll
		// never be farthest anyway
			goto newtop;
		}

continue_search:

		do
		{
			surf2 = surf2->next;
		} while (surf->key < surf2->key);

		if (surf->key == surf2->key)
		{
		// if it's two surfaces on the same plane, the one that's already
		// active is in front, so keep going unless it's a bmodel
			if (!surf->insubmodel)
				goto continue_search;

		// must be two bmodels in the same leaf; don't care which is really
		// in front, because they'll never be farthest anyway
		}

		goto gotposition;

newtop:
	// emit a span (obscures current top)
		iu = edge->u >> 20;

		if (iu > surf2->last_u)
		{
			span = span_p++;
			span->u = surf2->last_u;
			span->count = iu - span->u;
			span->v = current_iv;
			span->pnext = surf2->spans;
			surf2->spans = span;
		}

		// set last_u on the new span
		surf->last_u = iu;
				
gotposition:
	// insert before surf2
		surf->next = surf2;
		surf->prev = surf2->prev;
		surf2->prev->next = surf;
		surf2->prev = surf;
	}
}


/*
==============
R_TrailingEdge
==============
*/
void __not_in_flash_func(R_TrailingEdge) (surf_t *surf, edge_t *edge)
{
	espan_t			*span;
	int				iu;

// don't generate a span if this is an inverted span, with the end
// edge preceding the start edge (that is, we haven't seen the
// start edge yet)
	if (--surf->spanstate == 0)
	{
		if (surf->insubmodel)
			r_bmodelactive--;

		if (surf == surfaces[1].next)
		{
		// emit a span (current top going away)
			iu = edge->u >> 20;
			if (iu > surf->last_u)
			{
				span = span_p++;
				span->u = surf->last_u;
				span->count = iu - span->u;
				span->v = current_iv;
				span->pnext = surf->spans;
				surf->spans = span;
			}

		// set last_u on the surface below
			surf->next->last_u = iu;
		}

		surf->prev->next = surf->next;
		surf->next->prev = surf->prev;
	}
}

/*
==============
R_LeadingEdge
==============
*/
void __not_in_flash_func(R_LeadingEdge) (edge_t *edge)
{
	espan_t			*span;
	surf_t			*surf, *surf2;
	int				iu;
#ifdef Q_ALIAS_DOUBLE_TO_FLOAT_RENDER
	float			fu, newzi, testzi, newzitop, newzibottom;
#else
	double			fu, newzi, testzi, newzitop, newzibottom;
#endif

	if (edge->surfs[1])
	{
	// it's adding a new surface in, so find the correct place
		surf = &surfaces[edge->surfs[1]];

	// don't start a span if this is an inverted span, with the end
	// edge preceding the start edge (that is, we've already seen the
	// end edge)
		if (++surf->spanstate == 1)
		{
			if (surf->insubmodel)
				r_bmodelactive++;

			surf2 = surfaces[1].next;

			if (surf->key < surf2->key)
				goto newtop;

		// if it's two surfaces on the same plane, the one that's already
		// active is in front, so keep going unless it's a bmodel
			if (surf->insubmodel && (surf->key == surf2->key))
			{
			// must be two bmodels in the same leaf; sort on 1/z
#ifdef Q_ALIAS_DOUBLE_TO_FLOAT_RENDER
				fu = (float)(edge->u - 0xFFFFF) * (1.0f / 0x100000);
				newzi = surf->d_ziorigin + fv*surf->d_zistepv +
						fu*surf->d_zistepu;
				newzibottom = newzi * 0.99f;
#else
				fu = (float)(edge->u - 0xFFFFF) * (1.0 / 0x100000);
				newzi = surf->d_ziorigin + fv*surf->d_zistepv +
						fu*surf->d_zistepu;
				newzibottom = newzi * 0.99;
#endif

				testzi = surf2->d_ziorigin + fv*surf2->d_zistepv +
						fu*surf2->d_zistepu;

				if (newzibottom >= testzi)
				{
					goto newtop;
				}
#ifdef Q_ALIAS_DOUBLE_TO_FLOAT_RENDER
				newzitop = newzi * 1.01f;
#else
				newzitop = newzi * 1.01;
#endif
				if (newzitop >= testzi)
				{
					if (surf->d_zistepu >= surf2->d_zistepu)
					{
						goto newtop;
					}
				}
			}

continue_search:

			do
			{
				surf2 = surf2->next;
			} while (surf->key > surf2->key);

			if (surf->key == surf2->key)
			{
			// if it's two surfaces on the same plane, the one that's already
			// active is in front, so keep going unless it's a bmodel
				if (!surf->insubmodel)
					goto continue_search;

			// must be two bmodels in the same leaf; sort on 1/z
#ifdef Q_ALIAS_DOUBLE_TO_FLOAT_RENDER
				fu = (float)(edge->u - 0xFFFFF) * (1.0f / 0x100000);
				newzi = surf->d_ziorigin + fv*surf->d_zistepv +
						fu*surf->d_zistepu;
				newzibottom = newzi * 0.99f;
#else
				fu = (float)(edge->u - 0xFFFFF) * (1.0 / 0x100000);
				newzi = surf->d_ziorigin + fv*surf->d_zistepv +
						fu*surf->d_zistepu;
				newzibottom = newzi * 0.99;
#endif

				testzi = surf2->d_ziorigin + fv*surf2->d_zistepv +
						fu*surf2->d_zistepu;

				if (newzibottom >= testzi)
				{
					goto gotposition;
				}

#ifdef Q_ALIAS_DOUBLE_TO_FLOAT_RENDER
				newzitop = newzi * 1.01f;
#else
				newzitop = newzi * 1.01;
#endif
				if (newzitop >= testzi)
				{
					if (surf->d_zistepu >= surf2->d_zistepu)
					{
						goto gotposition;
					}
				}

				goto continue_search;
			}

			goto gotposition;

newtop:
		// emit a span (obscures current top)
			iu = edge->u >> 20;

			if (iu > surf2->last_u)
			{
				span = span_p++;
				span->u = surf2->last_u;
				span->count = iu - span->u;
				span->v = current_iv;
				span->pnext = surf2->spans;
				surf2->spans = span;
			}

			// set last_u on the new span
			surf->last_u = iu;
				
gotposition:
		// insert before surf2
			surf->next = surf2;
			surf->prev = surf2->prev;
			surf2->prev->next = surf;
			surf2->prev = surf;
		}
	}
}


/*
==============
R_GenerateSpans
==============
*/
void __not_in_flash_func(R_GenerateSpans) (void)
{
	short			edgeidx;
	edge_t			*edge;
	surf_t			*surf;
	edge_t			*ebuf = edgebuf;

	r_bmodelactive = 0;

// clear active surfaces to just the background surface
	surfaces[1].next = surfaces[1].prev = &surfaces[1];
	surfaces[1].last_u = edge_head_u_shift20;

// generate spans
	for (edgeidx=edge_head.next ; edgeidx != edge_tail_idx/*&edge_tail*/; edgeidx=edge->next)
	{			
		edge = ebuf+edgeidx;
		if (edge->surfs[0])
		{
		// it has a left surface, so a surface is going away for this span
			surf = &surfaces[edge->surfs[0]];

			R_TrailingEdge (surf, edge);

			if (!edge->surfs[1])
				continue;
		}

		R_LeadingEdge (edge);
	}

	R_CleanupSpan ();
}

/*
==============
R_GenerateSpansBackward
==============
*/
void __not_in_flash_func(R_GenerateSpansBackward) (void)
{
	short			edgeidx;
	edge_t			*edge;

	r_bmodelactive = 0;

// clear active surfaces to just the background surface
	surfaces[1].next = surfaces[1].prev = &surfaces[1];
	surfaces[1].last_u = edge_head_u_shift20;

// generate spans
	for (edgeidx=edge_head.next ; edgeidx != edge_tail_idx/*&edge_tail*/; edgeidx=edge->next)
	{		
		edge = edgebuf+edgeidx;

		if (edge->surfs[0])
			R_TrailingEdge (&surfaces[edge->surfs[0]], edge);

		if (edge->surfs[1])
			R_LeadingEdgeBackwards (edge);
	}

	R_CleanupSpan ();
}


/*
==============
R_ScanEdges

Input: 
newedges[] array
	this has links to edges, which have links to surfaces

Output:
Each surface has a linked list of its visible spans
==============
*/
void __no_inline_not_in_flash_func(R_ScanEdges) ()
{
	int		iv, bottom;
	surf_t	*s;
#if 0
	byte	basespans[MAXSPANS*sizeof(espan_t)+CACHE_SIZE];
#else
	byte   *basespans = (byte*)malloc(MAXSPANS*sizeof(espan_t)+CACHE_SIZE);
	if (basespans == NULL) {
		Sys_Error("R_ScanEdges: unable to allocate basespans (wanted %d bytes)\n",
			MAXSPANS*sizeof(espan_t)+CACHE_SIZE
		);
	}
#endif

	espan_t* basespan_p = (espan_t *)
			((intptr_t)(basespans + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));
	max_span_p = &basespan_p[MAXSPANS - r_refdef.vrect.width];

	span_p = basespan_p;

// clear active edges to just the background edges around the whole screen
// FIXME: most of this only needs to be set up once
	edge_head.u = r_refdef.vrect.x << 20;
	edge_head_u_shift20 = edge_head.u >> 20;
	edge_head.u_step = 0;
	edge_head.prev = 0;
	edge_head.next = edge_tail_idx;
	edge_head.surfs[0] = 0;
	edge_head.surfs[1] = 1;
	
	edge_tail.u = (r_refdef.vrectright << 20) + 0xFFFFF;
	edge_tail_u_shift20 = edge_tail.u >> 20;
	edge_tail.u_step = 0;
	edge_tail.prev = edge_head_idx;
	edge_tail.next = edge_aftertail_idx;
	edge_tail.surfs[0] = 1;
	edge_tail.surfs[1] = 0;
	
	edge_aftertail.u = -1;		// force a move
	edge_aftertail.u_step = 0;
	edge_aftertail.next = edge_sentinel_idx;
	edge_aftertail.prev = edge_tail_idx;

// FIXME: do we need this now that we clamp x in r_draw.c?
	edge_sentinel.u = 2000 << 24;		// make sure nothing sorts past this
	edge_sentinel.prev = edge_aftertail_idx;

	edge_t			*ebuf = edgebuf;		//
	int 			zbuf_limit = ZBA_GetZBufferMaxRow();

//	
// process all scan lines
//
	bottom = r_refdef.vrectbottom - 1;

	for (iv=r_refdef.vrect.y ; iv<bottom ; iv++)
	{
		current_iv = iv;
		fv = (float)iv;

	// mark that the head (background start) span is pre-included
		surfaces[1].spanstate = 1;

		if (newedges[iv])
		{
			R_InsertNewEdges (ebuf+newedges[iv], ebuf+edge_head.next);
		}

		(*pdrawfunc) ();

	// flush the span list if we can't be sure we have enough spans left for
	// the next scan, and on the z-buffer allocatior watermark so we could defer the flush-to-PSRAM as late as possible
		if ((span_p >= max_span_p) || ((edgebuf_swap != auxedges) && (iv == zbuf_limit)))
		{
			if (iv > zbuf_limit && edgebuf_swap != auxedges) {
				// UH OH! we are about to overwrite the Z-buffer
				// relocate edge list to the auxedges
				memcpy(auxedges, edgebuf_swap, sizeof(edge_t)*(NUMSTACKEDGES+4+1));
				r_edges  	+= (auxedges - edgebuf_swap);
				edge_p   	+= (auxedges - edgebuf_swap);
				edge_max    += (auxedges - edgebuf_swap);
				edgebuf_swap = auxedges;
				ebuf      	 = edgebuf_swap;
				edgebuf      = edgebuf_swap;
			}

			VID_UnlockBuffer ();
			S_ExtraUpdate ();	// don't let sound get messed up if going slow
			VID_LockBuffer ();
		
			if (r_drawculledpolys)
			{
				// wbcbz7 note: dead code
				R_DrawCulledPolys ();
			}
			else
			{
				D_DrawSurfaces ();
			}

		// clear the surface span pointers
			for (s = &surfaces[1] ; s<surface_p ; s++)
				s->spans = NULL;

			span_p = basespan_p;
		}

		if (removeedges[iv])
			R_RemoveEdges (ebuf+removeedges[iv]);

		if (ebuf+edge_head.next != edge_tail_idx)
			R_StepActiveU (ebuf+edge_head.next);
	}

// do the last scan (no need to step or sort or remove on the last scan)

	current_iv = iv;
	fv = (float)iv;

// mark that the head (background start) span is pre-included
	surfaces[1].spanstate = 1;

	if (newedges[iv]) {
		R_InsertNewEdges (ebuf+newedges[iv], ebuf+edge_head.next);
	}

	(*pdrawfunc) ();

// draw whatever's left in the span list
	if (r_drawculledpolys) {
		// wbcbz7 note: dead code
		R_DrawCulledPolys ();
	} else {
		D_DrawSurfaces ();
	}

#if 0
#else
	free(basespans);
#endif
}


