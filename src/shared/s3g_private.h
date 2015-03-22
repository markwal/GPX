/*  Copyright (c) 2015, Dan Newman <dan(dot)newman(at)mtbaldy(dot)us>
 *  All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 * 
 *  1. Redistributions of source code must retain the above copyright notice, this
 *     list of conditions and the following disclaimer. 
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 * 
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 *  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// s3g_private.h
// Private declarations for the s3g library

#ifndef S3G_PRIVATE_H_

#define S3G_PRIVATE_H_

#ifdef __cplusplus
extern "C" {
#endif

// read, write, and close procedure definitions for .s3g file drivers
typedef ssize_t s3g_read_proc_t(void *ctx, void *buf, size_t maxbuf, size_t nbytes);
typedef ssize_t s3g_write_proc_t(void *ctx, const void *buf, size_t nbytes);
typedef int s3g_close_proc_t(void *ctx);

// The actual s3g_context_t declaration

#ifndef S3G_CONTEXT_T_
#define S3G_CONTEXT_T_
typedef struct {
     s3g_read_proc_t  *read;     // File driver read procedure; req'd for reading
     s3g_write_proc_t *write;    // File driver write procedure; req'd for writing
     s3g_close_proc_t *close;    // File driver close procedure; optional
     void             *r_ctx;    // File driver private context
     void             *w_ctx;    // File driver private context
     size_t            nread;    // Bytes read
     size_t            nwritten; // Bytes written
} s3g_context_t;
#endif

// File driver open procedure; no need at present to keep this in the context

typedef int s3g_open_proc_t(s3g_context_t *ctx, const char *src, int create_file,
			    int mode);

#ifdef __cplusplus
}
#endif

#endif
