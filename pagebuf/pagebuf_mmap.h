/*******************************************************************************
 *  Copyright 2015 - 2017 Nick Jones <nick.fa.jones@gmail.com>
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 ******************************************************************************/

#ifndef PAGEBUF_MMAP_H
#define PAGEBUF_MMAP_H


#include <pagebuf/pagebuf.h>
#include <pagebuf/pagebuf_protected.h>


#ifdef __cplusplus
extern "C" {
#endif



/** The mmap buffer.
 *
 * The mmap buffer has a specific strategy and customised operations that
 * allow it to use mmap'd memory regions backed by a file on a block storage
 * device as its data storage backend.
 *
 * The mmap buffer will make use of a supplied allocator for the purpose of
 * allocating structs, however data regions will be allocated using an
 * internal allocator.  If no allocator is supplied, the trivial heap based
 * allocator will be used for struct allocations.
 *
 * The mmap buffer uses a trivial buffer internally as a structure to store
 * mapped pages, but the trivial buffer will only represent the current
 * runtime state of the mmap buffer and will not necessarily represent the
 * state of the backing file, thus it should not be accessed directly by
 * a user.
 */
struct pb_mmap_buffer {
  struct pb_trivial_buffer trivial_buffer;
};



/** Indicates which actions to take when opening and closing mmap'd files. */
enum pb_mmap_open_action {
  pb_mmap_open_action_read =                              1,
  pb_mmap_open_action_append =                            2,
  pb_mmap_open_action_overwrite =                         3,
};

enum pb_mmap_close_action {
  pb_mmap_close_action_retain =                           1,
  pb_mmap_close_action_remove =                           2,
};



/** Factory functions for the mmap buffer implementation of pb_buffer.
 *
 * The mmap buffer requires some parameters for initialisation:
 * file_path: the full path and file name of the file that the mmap buffer is
 *            to use as storage.  It is up to the user of the mmap buffer to
 *            ensure that the path either:
 *            exists and is readale or,
 *            doesn't exist and the directory writable
 * open_action: the action to perform when the buffer is opened:
 *              append: the file unchanged writes are appended to the end
 *              overwrite: file is cleared if it exists already
 * close_action: is the action to perform when the buffer is closed:
 *               retain: leaves the file and its data as is
 *               remove: clears all data and deletes the file
 *
 * Parameter validation errors during mmap buffer create will cause errno to
 * be set to EINVAL.
 * System errors during mmap buffer create will cause errno to be set to the
 * appropriate non zero value by the system call.
 */
struct pb_mmap_buffer *pb_mmap_buffer_create(const char *file_path,
    enum pb_mmap_open_action open_action,
    enum pb_mmap_close_action close_action);
struct pb_mmap_buffer *pb_mmap_buffer_create_with_alloc(const char *file_path,
    enum pb_mmap_open_action open_action,
    enum pb_mmap_close_action close_action,
    const struct pb_allocator *allocator);



/** The mmap buffers' open status. */
bool pb_mmap_buffer_is_open(
                          const struct pb_mmap_buffer *mmap_buffer);

/** The mmap buffers' backing file path and name. */
const char *pb_mmap_buffer_get_file_path(
                          const struct pb_mmap_buffer *mmap_buffer);

/** The mmap buffers' backing file descriptor. */
int pb_mmap_buffer_get_fd(const struct pb_mmap_buffer *mmap_buffer);

/** Query or set the mmap buffers' closing action.
 *
 * The set operator allows the close action policy to be updated after a
 * buffer is created.
 */
enum pb_mmap_close_action pb_mmap_buffer_get_close_action(
                                   const struct pb_mmap_buffer *mmap_buffer);
void pb_mmap_buffer_set_close_action(
                                   struct pb_mmap_buffer * const mmap_buffer,
                                   enum pb_mmap_close_action close_action);

/** mmap buffer conversion function. */
struct pb_buffer *pb_mmap_buffer_to_buffer(
                                   struct pb_mmap_buffer * const mmap_buffer);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PAGEBUF_MMAP_H */
