//  vector.h
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

typedef struct _vector {
    int c;      // count of items in the vector
    int cb;     // count of bytes per item
    int cSize;  // count of items of allocated space
    int cChunk; // number of items to allocate at a time

    char *pb;    // array of items
} vector;

// create a vector of items of size cb, initial size cInitial and grows by cChunk
// returns a pointer to the new vector or NULL on failure
vector *vector_create(int cb, int cInitial, int cChunk);

// free the vector
void vector_free(vector *pv);

// grow the vector by cChunk items
// returns true on success or false on failure
int vector_grow(vector *pv);

// insert item into the vector at index i
// returns the index of the item or -1 on failure
int vector_insert(vector *pv, int i, void *p);

// append item to the end of the vector
// returns the index to the item or -1 on failure
int vector_append(vector *pv, void *p);

// return a pointer to the ith item in the vector, don't hold this pointer
// across calls that could end up modifying the vector
void *vector_get(vector *pv, int i);
