//  vector.c
//
//  Simple implementation of a dynamic array
//
//  Created by Mark Walker (markwal@hotmail.com)
//
//  Copyright (c) 2015 Mark Walker, All rights reserved.
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software Foundation,
//  Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#include <stdlib.h>
#include <string.h>

#include "vector.h"

// create a vector of items of size cb, initial size cInitial and grows by cChunk
// returns a pointer to the new vector or NULL on failure
vector *vector_create(int cb, int cInitial, int cChunk)
{
    vector *pv = (vector *)malloc(sizeof(vector));
    if (pv == NULL)
        return NULL;

    pv->c = 0;
    pv->cb = cb;
    pv->cSize = cInitial;
    pv->cChunk = cChunk;

    pv->pb = (char *)malloc((size_t)cb * cInitial);
    if (pv->pb == NULL) {
        free(pv);
        return NULL;
    }

    return pv;
}

// free the vector
void vector_free(vector *pv)
{
    free(pv->pb);
    free(pv);
}

// grow the vector by cChunk items
// returns true on success or false on failure
int vector_grow(vector *pv)
{
    if (pv->cSize + pv->cChunk < pv->cSize) // overflow?
        return 0;
    if ((size_t)(pv->cSize + pv->cChunk) * pv->cb < (size_t)pv->cSize * pv->cb)
        return 0;

    char *pb = (char *)realloc(pv->pb, (size_t)(pv->cSize + pv->cChunk) * pv->cb);
    if (pb == NULL)
        return 0;

    pv->pb = pb;
    pv->cSize += pv->cChunk;
    return 1;
}

// insert item into the vector at index i
// returns the index of the item or -1 on failure
int vector_insert(vector *pv, int i, void *p)
{
    if (i < 0 || i > pv->c)
        return -1;

    if (pv->c >= pv->cSize && !vector_grow(pv))
        return -1;

    if (i < pv->c)
        memmove(pv->pb + ((size_t)(i + 1) * pv->cb), pv->pb + ((size_t)i * pv->cb), (size_t)(pv->c - i) * pv->cb);
    memcpy(pv->pb + ((size_t)i * pv->cb), p, pv->cb);
    (pv->c)++;
    return i;
}

// append item to the end of the vector
// returns the index to the item or -1 on failure
int vector_append(vector *pv, void *p)
{
    return vector_insert(pv, pv->c, p);
}

// return a pointer to the ith item in the vector, don't hold this pointer
// across calls that could end up modifying the vector
void *vector_get(vector *pv, int i)
{
    if (pv->c == 0 || i < 0 || i >= pv->c)
        return NULL;

    return pv->pb + ((size_t)i * pv->cb);
}
