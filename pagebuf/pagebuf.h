/*******************************************************************************
 *  Copyright 2015, 2016 Nick Jones <nick.fa.jones@gmail.com>
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

#ifndef PAGEBUF_H
#define PAGEBUF_H


#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>


#ifdef __cplusplus
extern "C" {
#endif



/** Indicates the intended use of an allocated memory block. */
enum pb_allocator_type {
  pb_alloc_type_struct,
  pb_alloc_type_region,
};

/** Wrapper for allocation and freeing of blocks of memory
 *
 * Allocate and free memory blocks through these interfaces.  The allocated
 * blocks will be used to hold data structures (pb_alloc_type_struct) or
 * for use as memory regions (pb_alloc_type_region), as indicated by the
 * caller.
 *
 * Memory to be used for structs must be zero'd after allocation and before
 * returning to the caller, and zero'd again after being handed back to the
 * allocator and before being freed.
 *
 * Memory to be used as a data region, storage areas for user data, need not be
 * treated in any special way by an allocator.
 */
struct pb_allocator {
  /** Allocate a memory block.
   *
   * type indicates what the allocated memory block will be used for
   *
   * size is the size of the memory block to allocate.
   */
  void *(*alloc)(const struct pb_allocator *allocator,
                 enum pb_allocator_type type, size_t size);
  /** Free a memory block.
   *
   * type indicates how the memory block was used.
   *
   * obj is the address of beginning of the memory region.
   *
   * size indicates the size of the memory region that was allocated and now
   *      freed.
   */
  void  (*free)(const struct pb_allocator *allocator,
                enum pb_allocator_type type, void *obj, size_t size);
};

/** Functional interfaces for the pb_allocator class. */
void *pb_allocator_alloc(
                       const struct pb_allocator *allocator,
                       enum pb_allocator_type type, size_t size);
void pb_allocator_free(const struct pb_allocator *allocator,
                       enum pb_allocator_type type, void *obj, size_t size);




/** Get a built in, trivial heap based allocator. */
const struct pb_allocator *pb_get_trivial_allocator(void);






/** A structure used to describe a data region. */
struct pb_data_vec {
  /** The starting address of the region. */
  uint8_t *base;
  /** The length of the region. */
  size_t len;
};






/** The pb_data and its supporting classes and functions. */
struct pb_data;



/** The structure that holds the operations that implement pb_data
 *  functionality.
 */
struct pb_data_operations {
  /** Increment the use count of the pb_data instance. */
  void (*get)(struct pb_data * const data);

  /** Decrement the use count of the pb_data instance.
   *
   * Will destroy the instance if the use count becomes zero.
   * The memory region of the instance will be freed if it is owned by the
   * instance (see pb_data_responsibility below).
   * The memory block associated with the pb_data instance itself will be
   * freed.
   */
  void (*put)(struct pb_data * const data);
};



/** Indicates the responsibility of a pb_data instance has over its data region.
 *
 * Either owned exclusively by the pb_data instance (pb_data_owned), thus freed
 * when pb_data use count reaches zero, or merely referenced by the pb_data
 * instance (pb_data_referenced), whereby the data region is not freed when the
 * pb_data use_count reaches zero.
 */
enum pb_data_responsibility {
  pb_data_owned,
  pb_data_referenced,
};



/** Reference counted structure that represents a memory region.
 *
 * Each pb_data instance has a one-to-one relationship to its memory region,
 * whether that region is owned or referenced, and the description of the
 * bounds of the data region, the data_vec member, is immutable and will not
 * change during the lifetime of the pb_data instance.
 *
 * Where a pb_data instance owns its memory region, the instance and the
 * region should be created at the same time, with the same allocator,
 * ideally in a factory function (such as pb_trivial_data_create)
 * Where a pb_data instance merely referenced a pre-allocated memory region,
 * an association between the two should be created as soon as possible so
 * that the lifecycle and relevance of the memory region can be tracked by
 * the pb_data instance.
 *
 * Instances of pb_data must be created using the create routines below, but
 * they should never be explicitly destroyed.  Instead the get and put
 * functions should be used to maintain the use_count of pb_data instances:
 * pb_data_get is to be called by new owners of a pb_data_instance,
 * pb_data_put is to be called when an owner no longer needs a pb_data
 * instance.  When a call to pb_data_put finds a zero use_count it will
 * destroy.
 *
 * Seeing as the pb_data class has such a close relationship with the
 * memory region it references, pb_buffer subclasses that enact particular
 * internal behaviour will find it neccessary to also subclass pb_data.
 * pb_data is indeed fit for this purpose.
 */
struct pb_data {
  /** The bounds of the region: base memory address and size (length). Cannot
   *  be changed after creation.
   */
  struct pb_data_vec data_vec;

  /** Responsibility that the instance has over its memory region. */
  enum pb_data_responsibility responsibility;

  /** Use count, maintained with atomic operations. */
  uint16_t use_count;

  /** Operations for the pb_buffer instance.  Cannot be changed after creation. */
  const struct pb_data_operations *operations;

  /** The allocator used to allocate memory blocks for this struct and its
   *  owned memory region, in addition to freeing these same blocks.
   */
  const struct pb_allocator *allocator;
};



/** Functional interfaces for management of pb_data reference counts. */
void pb_data_get(struct pb_data * const data);
void pb_data_put(struct pb_data * const data);



/** Get a built in, trivial set of pb_data operations. */
const struct pb_data_operations *pb_get_trivial_data_operations(void);



/** Factory functions for a trivial implementation of pb_data using trivial
 *  operations.
 */
struct pb_data *pb_trivial_data_create(size_t len,
                                       const struct pb_allocator *allocator);

struct pb_data *pb_trivial_data_create_ref(
                                       const uint8_t *buf, size_t len,
                                       const struct pb_allocator *allocator);

/** Trivial implementations of pb_data reference count management functions.
 *  pb_trivial_data_put also performs object cleanup amd memory block frees
 *  through the associated allocator.
 */
void pb_trivial_data_get(struct pb_data * const data);
void pb_trivial_data_put(struct pb_data * const data);



/** Non-exclusive owner of a pb_data instance, maintaining a modifiable
 *  reference to the pb_data memory region.
 *
 * This class embodies the consumption of the memory region referenced by the
 * member pb_data instance.  A pb_page instance holds a use_count reference
 * against its pb_data instance member, thus influencing its lifecycle.
 * The data_vec of the pb_page is initialised as a (non-proper) subset of the
 * data_vec of the pb_data, depending what the context of the pb_page
 * initialisation and data usage patterns in the running application.
 * The pb_page data_vec will always reside inside the pb_data data vec, in
 * terms of both base position and length (from the base position).  Any
 * deviation from this is an error.
 *
 * There is a many-to-one relationship between pb_page instances and pb_data
 * instances in some types of pb_buffer.  Each pb_page holds a use_count
 * reference on the same pb_data, collectively influencing its lifecyle.
 *
 * There is a one-to-one relationship between pb_buffer instances and
 * pb_page instances.  pb_pages instances will never travel outside a
 * pb_buffer and the pb_buffer will maintain full control of the lifecycle of
 * its own pages, by means of the allocator associated with the pb_buffer.
 * pb_page therefore does not required reference counting, nor is it
 * required to retain its allocator.
 * pb_data refererenced by a pb_buffer through a pb_page may be passed from
 * one pb_buffer to another, depending on the behaviour built into the target
 * pb_buffer.
 *
 * The pb_page class is thoroughly lightweigh and internal to pb_buffer so
 * it should never be subclassed to provide buffer class specific behaviour.
 */
struct pb_page {
  /** The offset into the referenced region, and the length of the reference */
  struct pb_data_vec data_vec;

  /** The reference to the pb_data instance. */
  struct pb_data *data;

  /** Previous page in a buffer structure. */
  struct pb_page *prev;
  /** Next page in a buffer structure */
  struct pb_page *next;
};



/** Create a pb_page instance.
 *
 * The pb_data instance has its use count incremented.
 */
struct pb_page *pb_page_create(struct pb_data *data,
                               const struct pb_allocator *allocator);

/** Create a pb_page using properties of another.
 *
 * The new pb_page instance will duplicate the base and len values of the
 * source page, and reference pb_data instance of the source page.
 */
struct pb_page *pb_page_transfer(const struct pb_page *src_page,
                                 size_t len, size_t src_off,
                                 const struct pb_allocator *allocator);

/** Destroy a pb_page instance.
 *
 * The pb_data instance will be de-referenced once.
 */
void pb_page_destroy(struct pb_page *page,
                     const struct pb_allocator *allocator);






/** The pb_buffer and its supporting classes and functions. */
struct pb_buffer;



/** A structure used to iterate over memory regions in a pb_buffer,
 *  one page at a time.
 *
 * The iterator may point to either pages managed internally by the pb_buffer
 * instance, that represent memory regions and thus data inside the buffer,
 * or it may point to a special 'end' page, that indicates the end pf buffer
 * data has been reached following iteration, or it may point to the special
 * 'end' page after initialisation, indicating there is no data in the buffer.
 */
struct pb_buffer_iterator {
  struct pb_page *page;
};



/** Functional interfaces for accessing a memory region through the
 *  pb_buffer_iterator class.
 */
uint8_t *pb_buffer_iterator_get_base(
                             const struct pb_buffer_iterator *buffer_iterator);
size_t pb_buffer_iterator_get_len(
                             const struct pb_buffer_iterator *buffer_iterator);



/** A structure used to iterate over memory regions in a pb_buffer,
 *  one byte at a time. */
struct pb_buffer_byte_iterator {
  struct pb_buffer_iterator buffer_iterator;

  size_t page_offset;

  const char *current_byte;
};



/** Functional interfaces for accessing a byte in a memory region through the
 *  pb_buffer_byte_iterator class. */
const char *pb_buffer_byte_iterator_get_current_byte(
                   const struct pb_buffer_byte_iterator *buffer_byte_iterator);



/** The default size of pb_buffer memory regions. */
#define PB_BUFFER_DEFAULT_PAGE_SIZE                       4096



/** A structure that describes the internal strategy of a pb_buffer.
 *
 * A buffer strategy describes properties, such as page_size, and also
 * describes ways that a buffer will carry out actions such as writing of data
 * and management of memory regions.
 *
 * Some buffer classes may accept strategies as parameters to their factory
 * methods or constructors, in which case behaviour may be tuned on an instance
 * by instance basis.  Some buffer classes may not allow modification of
 * strategy due to their specific internal implementation or intended use.
 *
 * Once a strategy is implanted in a pb_buffer instance (its values are copied
 * into the pb_buffer instance), then these strategy values will not be changed
 * internally and certainly shouldnt be changed externally.
 */
struct pb_buffer_strategy {
  /** The size of memory regions that the pb_buffer will internally dynamically
   *  allocate.
   *
   * If this value is zero, there will be no limit on fragment size,
   * therefore write operations will cause allocations of memory regions equal
   * in size to the length of the source data.
   */
  size_t page_size;

  /** Indicates whether data written into the buffer (from another buffer) is
   *  to be referenced or copied.
   *
   * Available behaviours:
   * not_cloned (false):reference to the pb_data instance is incremented.
   *
   * cloned     (true): new pb_data instance created and memory regions copied.
   */
  bool clone_on_write;

  /** Indicates how data written to the pb_buffer will be fragmented.
   *
   * Available behaviours:
   * as source (false): source data fragmentation takes precedence.
   *
   *                    clone_on_write (false):
   *                    When clone_on_write is false, source pages are
   *                    moved to the target as is and pb_data references are
   *                    incremented.  Source pages are only fragmented due to
   *                    truncation to keep the final page within the write
   *                    operation length value.
   *                    
   *                    clone_on_write (true):
   *                    When clone_on_write is true, source pages are
   *                    fragmented according to the lower of the source
   *                    page size and the target page_size, which may be
   *                    zero, in which case the source fragment size is used.
   *
   * as target  (true): target pb_buffer page_size takes precedence.
   *
   *                    clone_on_write (false):
   *                    When clone_on_write is false, source pages that
   *                    are larger than the target page_size limit are split
   *                    into multiple pages in the target.
   *
   *                    clone_on_write (true):
   *                    When clone_on_write is true, source pages will be
   *                    coppied and packed into target fragments up to the
   *                    target page_size in size.
   */
  bool fragment_as_target;

  /** Indicates whether a pb_buffer rejects (fails to support) insert operations.
   *  That is: operations that write to places in the buffer other than the end.
   *
   * Available behaviours:
   * no reject (false): insert operations can be performed wuth expected results.
   *
   * reject     (true): insert operations will immediately return 0.
   */
  bool rejects_insert;
};



/** The structure that holds the operations that implement pb_buffer
 *  functionality.
 */
struct pb_buffer_operations {
  /** Return a revision stamp of the data.
   *
   * The data revision is a counter that is increased every time that data
   * already inside the buffer is modified.
   *
   * Operations that cause a change in data revision include:
   * seeking, rewinding, trimming, inserting, overwriting
   *
   * Operations that don't cause a change in data revision include:
   * expanding, writing to the end of the buffer, reading
   *
   * External readers such as line readers can use changes in this value to
   * determine whether their view on the buffer data is still relevant, thus
   * allowing them to keep reading, or whether their data view is invalidated
   * thus requiring them to restart.
   */
  uint64_t (*get_data_revision)(struct pb_buffer * const buffer);
  /** Increment the data revision.
   *
   * This is a protected function and should not be called externally.
   */
  void (*increment_data_revision)(
                                struct pb_buffer * const buffer);

  /** Return the amount of data in the buffer, in bytes. */
  uint64_t (*get_data_size)(struct pb_buffer * const buffer);


  /** Initialise an iterator to point to the first data in the buffer, or
   *  to the 'end' page if the buffer is empty.
   *
   * The iterator parameter is best to be a pointer to a stack object.
   */
  void (*get_iterator)(struct pb_buffer * const buffer,
                       struct pb_buffer_iterator * const buffer_iterator);
  /** Initialise an iterator to point to the 'end' of the buffer.
   *
   * The iterator parameter is best to be a pointer to a stack object.
   */
  void (*get_iterator_end)(struct pb_buffer * const buffer,
                           struct pb_buffer_iterator * const buffer_iterator);
  /** Indicates whether an iterator is currently pointing to the 'end' of
   *  a buffer or not.
   *
   * The iterator passed to this function must be initialised using one of the
   * get_iterator* functions above.
   */
  bool (*iterator_is_end)(struct pb_buffer * const buffer,
                          struct pb_buffer_iterator * const buffer_iterator);

  /** Compare two iterators and indicate whether they are equal where equal is
   *  defined as pointing to the same page in the same buffer.
   *
   * The iterators passed to this function must be initialised using one of the
   * get_iterator* functions above.
   */
  bool (*iterator_cmp)(struct pb_buffer * const buffer,
                       const struct pb_buffer_iterator *lvalue,
                       const struct pb_buffer_iterator *rvalue);

  /** Moves an iterator to the next page in the data sequence, according to
   *  the buffers implementation specific arrangement of that sequence.
   *
   * The iterator passed to this function must be initialised using one of the
   * get_iterator* functions above.
   */
  void (*iterator_next)(struct pb_buffer * const buffer,
                        struct pb_buffer_iterator * const buffer_iterator);
  /** Moves an iterator to the previous page in the data sequence, according to
   *  the buffers implementation specific arrangement of that sequence.
   *
   * The iterator passed to this function must be initialised using one of the
   * get_iterator* functions above.
   */
  void (*iterator_prev)(struct pb_buffer * const buffer,
                        struct pb_buffer_iterator * const buffer_iterator);


  /** Initialise a byte iterator to the start of the pb_buffer instance data.
   *
   * The byte iterator should be a pointer to an object on the stack that must
   * be manipulated only by the iterator methods of the same buffer instance.
   */
  void (*get_byte_iterator)(struct pb_buffer * const buffer,
                            struct pb_buffer_byte_iterator * const
                              buffer_byte_iterator);
  /** Initialise an iterator to the end of the pb_buffer instance data.
   *
   * The iterator should be a pointer to an object on the stack that must
   * be manipulated only by the iterator methods of the same buffer instance.
   */
  void (*get_byte_iterator_end)(struct pb_buffer * const buffer,
                                struct pb_buffer_byte_iterator * const
                                  buffer_byte_iterator);
  /** Indicates whether an iterator has traversed to the end of a buffers
   *  internal chain of pages.
   *
   * This function must always be called, and return false, before the data
   * vector of the pb_page of the iterator can be used.  The value of the
   * pb_page pointer is undefined when the iterator end function returns true.
   */
  bool (*byte_iterator_is_end)(struct pb_buffer * const buffer,
                               struct pb_buffer_byte_iterator * const
                                 buffer_byte_iterator);

  bool (*byte_iterator_cmp)(struct pb_buffer * const buffer,
                            const struct pb_buffer_byte_iterator *lvalue,
                            const struct pb_buffer_byte_iterator *rvalue);

  /** Increments an iterator to the next pb_page in a buffer's internal chain. */
  void (*byte_iterator_next)(struct pb_buffer * const buffer,
                             struct pb_buffer_byte_iterator * const
                               buffer_byte_iterator);
  /** Decrements an iterator to the previous pb_page in a buffer's internal
   *  chain.
   *
   * It is valid to call this function on an iterator that is the end
   * iterator, according to is_iterator_end.  If this function is called on such
   * an iterator, the buffer implementation must correctly decrement back to the
   * position before end in this case. */
  void (*byte_iterator_prev)(struct pb_buffer * const buffer,
                             struct pb_buffer_byte_iterator * const
                               buffer_byte_iterator);


  /** Create a pb_page instance with accompanying pb_data.
   *
   * This is a private function and should not be called externally.
   *
   * len is the size of the memory region to allocate.
   * is_rewind indicates whether the region is to be placed at the beginning
   *           of the buffer in a rewind situation.
   */
  struct pb_page *(*page_create)(
                             struct pb_buffer * const buffer,
                             struct pb_buffer_iterator * const buffer_iterator,
                             size_t len,
                             bool is_rewind);
  /** Create a pb_page instance with accompanying pb_data.
   *
   * This is a private function and should not be called externally.
   *
   * buf is the memory region to reference.
   * len is the size of the memory region to allocate.
   * is_rewind indicates whether the region is to be placed at the beginning
   *           of the buffer in a rewind situation.
   *
   * Memory region buf will not owned by the accompanying pb_data instance.
   */
  struct pb_page *(*page_create_ref)(
                             struct pb_buffer * const buffer,
                             struct pb_buffer_iterator * const buffer_iterator,
                             const uint8_t *buf, size_t len,
                             bool is_rewind);


  /** Append a pb_page instance to the pb_buffer.
   *
   * This is a private function and should not be called externally.
   *
   * buffer_iterator is the position in the buffer to update the pb_buffer.  The
   *                 pb_page will be inserted in front of the iterator position.
   * offset is the offset within the iterator page to insert the new pb_page
   *        into.
   * page is the page to insert.  The data base and len values must be set
   *      before this operation is called.
   *
   * If the offset is zero, the new pb_page will be inserted before the
   * iterator page.  If the offset is non zero, then the iterator page may be
   * split according to the pb_buffer instances internal implementation.
   * If the offset is greater than or equal to the iterator page len, then the
   * new pb_page will be inserted after the iterator page, or at the head of
   * the pb_buffer if the iterator is an end iterator.
   *
   * This operation will affect the data size of the pb_buffer instance.
   *
   * The return value is the amount of data actually inserted to the
   * pb_buffer instance.  Users of the insert operation must reflect the value
   * returned back to their own callers.
   */
  uint64_t (*insert)(
                   struct pb_buffer * const buffer,
                   struct pb_buffer_iterator * const buffer_iterator,
                   size_t offset,
                   struct pb_page * const page);
  /** Increase the size of the buffer by adding len bytes of data to the end.
   *
   * This is a private function and should not be called externally.
   *
   * len indicates the amount of data to add in bytes.
   *
   * Depending on the buffer implementation details, the total len may be split
   * across multiple pages.
   */
  uint64_t (*extend)(
                   struct pb_buffer * const buffer,
                   uint64_t len);
  /** Increases the size of the buffer by adding len bytes of data to the head.
   *
   * This is a private function and should not be called externally.
   *
   * len indicates the amount of data to ad in bytes.
   *
   * Depending on the buffer implementation details, the total len may be split
   * across multiple pages.
   */
  uint64_t (*rewind)(
                   struct pb_buffer * const buffer,
                   uint64_t len);
  /** Seek the buffer by len bytes starting at the beginning of the buffer.
   *
   * This is a private function and should not be called externally.
   *
   * len indicates the amount of data to seek, in bytes.
   *
   * The seek operation may cause internal pages to be consumed depending on
   * the buffer implementation details.  These pb_pages will be destroyed during
   * the seek operation and their respective data 'put'd.
   */
  uint64_t (*seek)(struct pb_buffer * const buffer,
                   uint64_t len);
  /** Trim the buffer by len bytes starting at the end of the buffer.
   *
   * This is a private function and should not be called externally.
   *
   * len indicates the amount of data to trim, in bytes.
   *
   * The trim operation may cause internal pages to be consumed depending on
   * the buffer implementation details.  These pb_pages will be destroyed during
   * the trim operation and their respective data 'put'd.
   */
  uint64_t (*trim)(struct pb_buffer * const buffer,
                   uint64_t len);

  /** Write data from a memory region to the pb_buffer instance.
   *
   * buf indicates the start of the source memory region.
   * len indicates the amount of data to write, in bytes.
   *
   * returns the amount of data written.
   *
   * Data is appended to the tail of the buffer, allocating new storage if
   * necessary.
   */
  uint64_t (*write_data)(struct pb_buffer * const buffer,
                         const uint8_t *buf,
                         uint64_t len);
  /** Write data from a referenced memory region to the pb_buffer instance.
   *
   * buf indicates the start of the source referenced memory region.
   * len indicates the amount of data to write, in bytes.
   *
   * returns the amount of data written.
   *
   * Data is appended to the tail of the buffer.  As the memory region is
   * referenced, no storage needs to be allocated unless the buffer strategy
   * indicates clone_on_write or fragment_as_target is true.
   */
  uint64_t (*write_data_ref)(struct pb_buffer * const buffer,
                             const uint8_t *buf,
                             uint64_t len);
  /** Write data from a source pb_buffer instance to the pb_buffer instance.
   *
   * src_buffer is the buffer to write from.  This pb_buffer instance will not
   *            have it's data modified, but it is not const because iterator
   *            operations of this pb_buffer may cause internal state changes.
   * len indicates the amount of data to write, in bytes.
   *
   * returns the amount of data written.  This buffer will not be altered by
   * this operation.
   *
   * Data is appended to the tail of the buffer, allocating new storage if
   * necessary.
   */
  uint64_t (*write_buffer)(struct pb_buffer * const buffer,
                           struct pb_buffer * const src_buffer,
                           uint64_t len);

  /** Write data from a memory region to the pb_buffer instance.
   *
   * buf indicates the start of the source memory region.
   * len indicates the amount of data to write, in bytes.
   *
   * returns the amount of data written.
   *
   * Data is appended to the tail of the buffer, allocating new storage if
   * necessary.
   */
  uint64_t (*insert_data)(struct pb_buffer * const buffer,
                          struct pb_buffer_iterator * const buffer_iterator,
                          size_t offset,
                          const uint8_t *buf,
                          uint64_t len);
  /** Write data from a referenced memory region to the pb_buffer instance.
   *
   * buf indicates the start of the source referenced memory region.
   * len indicates the amount of data to write, in bytes.
   *
   * returns the amount of data written.
   *
   * Data is appended to the tail of the buffer.  As the memory region is
   * referenced, no storage needs to be allocated unless the buffer strategy
   * indicates clone_on_write or fragment_as_target is true.
   */
  uint64_t (*insert_data_ref)(struct pb_buffer * const buffer,
                              struct pb_buffer_iterator * const buffer_iterator,
                              size_t offset,
                              const uint8_t *buf,
                              uint64_t len);
  /** Write data from a source pb_buffer instance to the pb_buffer instance.
   *
   * src_buffer is the buffer to write from.  This pb_buffer instance will not
   *            have it's data modified, but it is not const because iterator
   *            operations of this pb_buffer may cause internal state changes.
   * len indicates the amount of data to write, in bytes.
   *
   * returns the amount of data written.  This buffer will not be altered by
   * this operation.
   *
   * Data is appended to the tail of the buffer, allocating new storage if
   * necessary.
   */
  uint64_t (*insert_buffer)(struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator,
                            size_t offset,
                            struct pb_buffer * const src_buffer,
                            uint64_t len);

  /** Write data from a memory region to the pb_buffer instance.
   *
   * buf indicates the start of the source memory region.
   * len indicates the amount of data to write, in bytes.
   *
   * returns the amount of data written.
   *
   * Data is written from the head of the source pb_buffer.  No new storage will
   * be allocated if len is greater than the size of the target pb_buffer.
   */
  uint64_t (*overwrite_data)(struct pb_buffer * const buffer,
                             const uint8_t *buf,
                             uint64_t len);

  /** Read data from the start of a pb_buffer instance to a data region.
   *
   * buf indicates the start of the target memory region.
   * len indicates the amount of data to read, in bytes.
   *
   * returns the amount of data read into the target buffer.
   */
  uint64_t (*read_data)(struct pb_buffer * const buffer,
                        uint8_t * const buf,
                        uint64_t len);

  /** Clear all data in the buffer. */
  void (*clear)(struct pb_buffer * const buffer);

  /** Destroy a pb_buffer. */
  void (*destroy)(struct pb_buffer * const buffer);
};



/** Represents a buffer of pages, and operations for the manipulation of the
 * pages and the buffers of data therein.
 *
 * The buffer also represents the strategy for allocation and freeing of
 * data buffers.
 */
struct pb_buffer {
  /** Strategy for the pb_buffer instance.  Cannot be changed after creation. */
  const struct pb_buffer_strategy *strategy;

  /** Operations for the pb_buffer instance.  Cannot be changed after creation. */
  const struct pb_buffer_operations *operations;

  /** Allocator used to allocate storage for this struct and its pages. */
  const struct pb_allocator *allocator;
};



/** Functional interfaces for the generic pb_buffer class. */
uint64_t pb_buffer_get_data_revision(struct pb_buffer * const buffer);

uint64_t pb_buffer_get_data_size(struct pb_buffer * const buffer);


void pb_buffer_get_iterator(struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator);
void pb_buffer_get_iterator_end(
                            struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator);
bool pb_buffer_iterator_is_end(
                            struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator);
bool pb_buffer_iterator_cmp(struct pb_buffer * const buffer,
                            const struct pb_buffer_iterator *lvalue,
                            const struct pb_buffer_iterator *rvalue);
void pb_buffer_iterator_next(
                            struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator);
void pb_buffer_iterator_prev(
                            struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator);


void pb_buffer_get_byte_iterator(
                         struct pb_buffer * const buffer,
                         struct pb_buffer_byte_iterator * const byte_iterator);
void pb_buffer_get_byte_iterator_end(
                         struct pb_buffer * const buffer,
                         struct pb_buffer_byte_iterator * const byte_iterator);
bool pb_buffer_byte_iterator_is_end(
                         struct pb_buffer * const buffer,
                         struct pb_buffer_byte_iterator * const byte_iterator);
bool pb_buffer_byte_iterator_cmp(
                         struct pb_buffer * const buffer,
                         const struct pb_buffer_byte_iterator *lvalue,
                         const struct pb_buffer_byte_iterator *rvalue);
void pb_buffer_byte_iterator_next(
                         struct pb_buffer * const buffer,
                         struct pb_buffer_byte_iterator * const byte_iterator);
void pb_buffer_byte_iterator_prev(
                         struct pb_buffer * const buffer,
                         struct pb_buffer_byte_iterator * const byte_iterator);


uint64_t pb_buffer_insert(
                        struct pb_buffer * const buffer,
                        struct pb_buffer_iterator * const buffer_iterator,
                        size_t offset,
                        struct pb_page * const page);
uint64_t pb_buffer_extend(
                        struct pb_buffer * const buffer, uint64_t len);
uint64_t pb_buffer_rewind(
                        struct pb_buffer * const buffer, uint64_t len);
uint64_t pb_buffer_seek(struct pb_buffer * const buffer, uint64_t len);
uint64_t pb_buffer_trim(struct pb_buffer * const buffer, uint64_t len);


uint64_t pb_buffer_write_data(struct pb_buffer * const buffer,
                              const uint8_t *buf,
                              uint64_t len);
uint64_t pb_buffer_write_data_ref(
                              struct pb_buffer * const buffer,
                              const uint8_t *buf,
                              uint64_t len);
uint64_t pb_buffer_write_buffer(
                              struct pb_buffer * const buffer,
                              struct pb_buffer * const src_buffer,
                              uint64_t len);

uint64_t pb_buffer_insert_data(struct pb_buffer * const buffer,
                               struct pb_buffer_iterator * const buffer_iterator,
                               size_t offset,
                               const uint8_t *buf,
                               uint64_t len);
uint64_t pb_buffer_insert_data_ref(
                               struct pb_buffer * const buffer,
                               struct pb_buffer_iterator * const buffer_iterator,
                               size_t offset,
                               const uint8_t *buf,
                               uint64_t len);
uint64_t pb_buffer_insert_buffer(
                               struct pb_buffer * const buffer,
                               struct pb_buffer_iterator * const buffer_iterator,
                               size_t offset,
                               struct pb_buffer * const src_buffer,
                               uint64_t len);

uint64_t pb_buffer_overwrite_data(
                              struct pb_buffer * const buffer,
                              const uint8_t *buf,
                              uint64_t len);


uint64_t pb_buffer_read_data(struct pb_buffer * const buffer,
                             uint8_t * const buf,
                             uint64_t len);


void pb_buffer_clear(struct pb_buffer * const buffer);
void pb_buffer_destroy(
                     struct pb_buffer * const buffer);



/** The trivial buffer implementation and its supporting functions.
 *
 * The trivial buffer defines a classic strategy and operations set, and uses
 * the trivial heap based allocator by default.
 */
struct pb_trivial_buffer {
  struct pb_buffer buffer;

  struct pb_page page_end;

  uint64_t data_revision;
  uint64_t data_size;
};



/** Get a trivial buffer strategy.
 *
 * page_size is 4096
 *
 * clone_on_write is false
 *
 * fragment_as_source is false
 *
 * rejects_insert is false
 */
const struct pb_buffer_strategy *pb_get_trivial_buffer_strategy(void);



/** Get a trivial buffer operations structure. */
const struct pb_buffer_operations *pb_get_trivial_buffer_operations(void);



/** An implementation of pb_buffer using the default or supplied allocator. */
struct pb_buffer *pb_trivial_buffer_create(void);
struct pb_buffer *pb_trivial_buffer_create_with_strategy(
                                    const struct pb_buffer_strategy *strategy);
struct pb_buffer *pb_trivial_buffer_create_with_alloc(
                                    const struct pb_allocator *allocator);
struct pb_buffer *pb_trivial_buffer_create_with_strategy_with_alloc(
                                    const struct pb_buffer_strategy *strategy,
                                    const struct pb_allocator *allocator);



/** Trivial buffer operations. */
uint64_t pb_trivial_buffer_get_data_revision(
                                         struct pb_buffer * const buffer);
void pb_trivial_buffer_increment_data_revision(
                                         struct pb_buffer * const buffer);


uint64_t pb_trivial_buffer_get_data_size(struct pb_buffer * const buffer);

void pb_trivial_buffer_increment_data_size(
                                         struct pb_buffer * const buffer,
                                         uint64_t size);
void pb_trivial_buffer_decrement_data_size(
                                         struct pb_buffer * const buffer,
                                         uint64_t size);


void pb_trivial_buffer_get_iterator(
                            struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator);
void pb_trivial_buffer_get_iterator_end(
                            struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator);
bool pb_trivial_buffer_iterator_is_end(
                            struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator);
bool pb_trivial_buffer_iterator_cmp(struct pb_buffer * const buffer,
                            const struct pb_buffer_iterator *lvalue,
                            const struct pb_buffer_iterator *rvalue);
void pb_trivial_buffer_iterator_next(
                            struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator);
void pb_trivial_buffer_iterator_prev(
                            struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator);


void pb_trivial_buffer_get_byte_iterator(
                         struct pb_buffer * const buffer,
                         struct pb_buffer_byte_iterator * const byte_iterator);
void pb_trivial_buffer_get_byte_iterator_end(
                         struct pb_buffer * const buffer,
                         struct pb_buffer_byte_iterator * const byte_iterator);
bool pb_trivial_buffer_byte_iterator_is_end(
                         struct pb_buffer * const buffer,
                         struct pb_buffer_byte_iterator * const byte_iterator);
bool pb_trivial_buffer_byte_iterator_cmp(struct pb_buffer * const buffer,
                         const struct pb_buffer_byte_iterator *lvalue,
                         const struct pb_buffer_byte_iterator *rvalue);
void pb_trivial_buffer_byte_iterator_next(
                         struct pb_buffer * const buffer,
                         struct pb_buffer_byte_iterator * const byte_iterator);
void pb_trivial_buffer_byte_iterator_prev(
                         struct pb_buffer * const buffer,
                         struct pb_buffer_byte_iterator * const byte_iterator);


struct pb_page *pb_trivial_buffer_page_create(
                              struct pb_buffer * const buffer,
                              struct pb_buffer_iterator * const buffer_iterator,
                              size_t len,
                              bool is_rewind);
struct pb_page *pb_trivial_buffer_page_create_ref(
                              struct pb_buffer * const buffer,
                              struct pb_buffer_iterator * const buffer_iterator,
                              const uint8_t *buf, size_t len,
                              bool is_rewind);


uint64_t pb_trivial_buffer_insert(
                              struct pb_buffer * const buffer,
                              struct pb_buffer_iterator * const buffer_iterator,
                              size_t offset,
                              struct pb_page * const page);
uint64_t pb_trivial_buffer_extend(
                              struct pb_buffer * const buffer,
                              uint64_t len);
uint64_t pb_trivial_buffer_rewind(
                              struct pb_buffer * const buffer,
                              uint64_t len);
uint64_t pb_trivial_buffer_seek(struct pb_buffer * const buffer,
                              uint64_t len);
uint64_t pb_trivial_buffer_trim(struct pb_buffer * const buffer,
                              uint64_t len);


uint64_t pb_trivial_buffer_write_data(struct pb_buffer * const buffer,
                                      const uint8_t *buf,
                                      uint64_t len);
uint64_t pb_trivial_buffer_write_data_ref(
                                      struct pb_buffer * const buffer,
                                      const uint8_t *buf,
                                      uint64_t len);
uint64_t pb_trivial_buffer_write_buffer(
                                      struct pb_buffer * const buffer,
                                      struct pb_buffer * const src_buffer,
                                      uint64_t len);


uint64_t pb_trivial_buffer_insert_data(
                              struct pb_buffer * const buffer,
                              struct pb_buffer_iterator * const buffer_iterator,
                              size_t offset,
                              const uint8_t *buf,
                              uint64_t len);
uint64_t pb_trivial_buffer_insert_data_ref(
                              struct pb_buffer * const buffer,
                              struct pb_buffer_iterator * const buffer_iterator,
                              size_t offset,
                              const uint8_t *buf,
                              uint64_t len);
uint64_t pb_trivial_buffer_insert_buffer(
                              struct pb_buffer * const buffer,
                              struct pb_buffer_iterator * const buffer_iterator,
                              size_t offset,
                              struct pb_buffer * const src_buffer,
                              uint64_t len);


uint64_t pb_trivial_buffer_overwrite_data(struct pb_buffer * const buffer,
                                          const uint8_t *buf,
                                          uint64_t len);


uint64_t pb_trivial_buffer_read_data(struct pb_buffer * const buffer,
                                     uint8_t * const buf,
                                     uint64_t len);


void pb_trivial_buffer_clear(struct pb_buffer * const buffer);
void pb_trivial_pure_buffer_clear(
                             struct pb_buffer * const buffer);
void pb_trivial_buffer_destroy(
                             struct pb_buffer * const buffer);






/** The pb_data_reader and its supporting functions. */
struct pb_data_reader;



/**
 *
 * The operations structure of the data reader.
 */
struct pb_data_reader_operations {
  /** Read data from the pb_buffer instance, into a memory region.
   *
   * buf indicates the start of the target memory region.
   * len indicates the amount of data to read, in bytes.
   *
   * returns the amount of data read.
   *
   * Data is read from the head of the source pb_buffer.  The amount of data read
   * is the lower of the size of the source pb_buffer and the value of len.
   *
   * Following a data read, the data reader will retain the position of the
   * end of the read, thus, a subsequent call to read_buffer will continue from
   * where the last read left off.
   *
   * However, if the pb_buffer undergoes an operation that alters its data
   * revision, a subsequent call to read_buffer will read from the beginning
   */
  uint64_t (*read)(struct pb_data_reader * const data_reader,
                   uint8_t * const buf, uint64_t len);

  /** Reset the data reader back to the beginning of the pb_buffer instance. */
  void (*reset)(struct pb_data_reader * const data_reader);

  /** Destroy the pb_data_reader instance. */
  void (*destroy)(struct pb_data_reader * const data_reader);
};



/** An interface for reading data from a pb_buffer. */
struct pb_data_reader {
  const struct pb_data_reader_operations *operations;

  struct pb_buffer *buffer;
};



/** Functional interfaces for the generic pb_data_reader class. */
uint64_t pb_data_reader_read(struct pb_data_reader * const data_reader,
                             uint8_t * const buf, uint64_t len);

void pb_data_reader_reset(struct pb_data_reader * const data_reader);
void pb_data_reader_destroy(
                          struct pb_data_reader * const data_reader);



/** The trivial data reader implementation and its supporting functions. */



/** Get a trivial data reader operations structure. */
const struct pb_data_reader_operations
  *pb_get_trivial_data_reader_operations(void);



/** A trivial data reader implementation that reads data via iterators
 */
struct pb_data_reader *pb_trivial_data_reader_create(
                                              struct pb_buffer * const buffer);

uint64_t pb_trivial_data_reader_read(struct pb_data_reader * const data_reader,
                                     uint8_t * const buf, uint64_t len);

void pb_trivial_data_reader_reset(struct pb_data_reader * const data_reader);
void pb_trivial_data_reader_destroy(
                                  struct pb_data_reader * const data_reader);






/** The pb_line_reader and its supporting functions. */
struct pb_line_reader;



/** An interface for discovering, reading and consuming LF or CRLF terminated
 *  lines in pb_buffer instances.
 *
 * The operations structure of the line reader.
 */
struct pb_line_reader_operations {
  /** Indicates whether a line exists at the head of a pb_buffer instance. */
  bool (*has_line)(struct pb_line_reader * const line_reader);

  /** Returns the length of the line discovered by has_line. */
  size_t (*get_line_len)(struct pb_line_reader * const line_reader);
  /** Extracts the data of the line discovered by has_line.
   *
   * base indicates the start of the destination memory region
   * len indicates the shorter of the number of bytes to read or the length of
   *     the memory region.
   */
  size_t (*get_line_data)(struct pb_line_reader * const line_reader,
                          uint8_t * const base, uint64_t len);

  size_t (*seek_line)(struct pb_line_reader * const line_reader);

  /** Marks the present position of line discovery as a line end.
   *
   * If the character proceeding the termination point is a cr, it will be
   * ignored in the line length calculation and included in the line. */
  void (*terminate_line)(struct pb_line_reader * const line_reader);
  /** Marks the present position of line discovery as a line end.
   *
   * If the character proceeding the termination point is a cr, it will be
   * considered in the line length calculation. */
  void (*terminate_line_check_cr)(struct pb_line_reader * const line_reader);

  /** Indicates whether the line discovered in has_line is:
   *    LF (false) or CRLF (true) */
  bool (*is_crlf)(struct pb_line_reader * const line_reader);
  /** Indicates whether the line discovery has reached the end of the buffer.
   *
   * Buffer end may be used as line end if terminate_line is called. */
  bool (*is_end)(struct pb_line_reader * const line_reader);

  /** Clone the state of the line reader into a new instance. */
  struct pb_line_reader *(*clone)(struct pb_line_reader * const line_reader);

  /** Reset the current line discovery back to an initial state. */
  void (*reset)(struct pb_line_reader * const line_reader);

  /** Destroy a line reader instance. */
  void (*destroy)(struct pb_line_reader * const line_reader);
};



/** Lines in a buffer will be limited by implementations to no longer than
 * PB_TRIVIAL_LINE_READER_DEFAULT_LINE_MAX bytes.  Any lines that are longer
 * will be truncated to that length.
 */
#define PB_LINE_READER_DEFAULT_LINE_MAX                   16777216L

struct pb_line_reader {
  const struct pb_line_reader_operations *operations;

  struct pb_buffer *buffer;
};




/** Functional infterface for the generic pb_buffer class. */
bool pb_line_reader_has_line(struct pb_line_reader * const line_reader);

size_t pb_line_reader_get_line_len(struct pb_line_reader * const line_reader);
size_t pb_line_reader_get_line_data(
                                   struct pb_line_reader * const line_reader,
                                   uint8_t * const buf, uint64_t len);

bool pb_line_reader_is_crlf(
                           struct pb_line_reader * const line_reader);
bool pb_line_reader_is_end(struct pb_line_reader * const line_reader);

void pb_line_reader_terminate_line(struct pb_line_reader * const line_reader);
void pb_line_reader_terminate_line_check_cr(
                                   struct pb_line_reader * const line_reader);

size_t pb_line_reader_seek_line(struct pb_line_reader * const line_reader);

struct pb_line_reader *pb_line_reader_clone(
                                     struct pb_line_reader * const line_reader);

void pb_line_reader_reset(struct pb_line_reader * const line_reader);
void pb_line_reader_destroy(
                          struct pb_line_reader * const line_reader);



/** The trivial line reader implementation and its supporting functions. */



/** Get a trivial line reader operations structure. */
const struct pb_line_reader_operations
  *pb_get_trivial_line_reader_operations(void);



/** A trivial line reader implementation that searches for lines via iterators.
 */
struct pb_line_reader *pb_trivial_line_reader_create(
                                          struct pb_buffer * const buffer);

bool pb_trivial_line_reader_has_line(
                                    struct pb_line_reader * const line_reader);

size_t pb_trivial_line_reader_get_line_len(
                                    struct pb_line_reader * const line_reader);
size_t pb_trivial_line_reader_get_line_data(
                                    struct pb_line_reader * const line_reader,
                                    uint8_t * const buf, uint64_t len);

size_t pb_trivial_line_reader_seek_line(
                                    struct pb_line_reader * const line_reader);

bool pb_trivial_line_reader_is_crlf(struct pb_line_reader * const line_reader);
bool pb_trivial_line_reader_is_end(struct pb_line_reader * const line_reader);

void pb_trivial_line_reader_terminate_line(
                                    struct pb_line_reader * const line_reader);
void pb_trivial_line_reader_terminate_line_check_cr(
                                    struct pb_line_reader * const line_reader);

struct pb_line_reader *pb_trivial_line_reader_clone(
                                    struct pb_line_reader * const line_reader);

void pb_trivial_line_reader_reset(struct pb_line_reader * const line_reader);
void pb_trivial_line_reader_destroy(
                                  struct pb_line_reader * const line_reader);



#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PAGEBUF_H */
