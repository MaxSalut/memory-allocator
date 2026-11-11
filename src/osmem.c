// SPDX-License-Identifier: BSD-3-Clause

#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include "osmem.h"
#include "block_meta.h"

#define ALIGNMENT		8
#define ALIGN(size)		(((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))
#define META_SIZE		ALIGN(sizeof(struct block_meta))
#define MMAP_THRESHOLD		(128 * 1024)
#define HEAP_PREALLOC_SIZE	(128 * 1024)

/* Blocks allocated with brk(), kept in address order. */
static struct block_meta *heap_head;

static void *block_payload(struct block_meta *block)
{
	return (char *)block + META_SIZE;
}

static struct block_meta *payload_block(void *ptr)
{
	return (struct block_meta *)((char *)ptr - META_SIZE);
}

static struct block_meta *heap_last(void)
{
	struct block_meta *block = heap_head;

	while (block && block->next)
		block = block->next;

	return block;
}

/* Allocate a standalone block with mmap(). */
static struct block_meta *alloc_mapped(size_t size)
{
	void *addr = mmap(NULL, size + META_SIZE, PROT_READ | PROT_WRITE,
			  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	DIE(addr == MAP_FAILED, "mmap");

	struct block_meta *block = addr;

	block->size = size;
	block->status = STATUS_MAPPED;
	block->prev = NULL;
	block->next = NULL;

	return block;
}

/* Grow the heap with sbrk() and append a new block at its end. */
static struct block_meta *heap_append(size_t size)
{
	void *addr = sbrk(size + META_SIZE);

	DIE(addr == (void *)-1, "sbrk");

	struct block_meta *block = addr;
	struct block_meta *last = heap_last();

	block->size = size;
	block->status = STATUS_FREE;
	block->prev = last;
	block->next = NULL;

	if (last)
		last->next = block;
	else
		heap_head = block;

	return block;
}

/* First use of the heap: reserve 128 KiB at once. */
static void heap_preallocate(void)
{
	heap_append(HEAP_PREALLOC_SIZE - META_SIZE);
}

static struct block_meta *find_free_block(size_t size)
{
	for (struct block_meta *block = heap_head; block; block = block->next)
		if (block->status == STATUS_FREE && block->size >= size)
			return block;

	return NULL;
}

static struct block_meta *alloc_heap(size_t size)
{
	struct block_meta *block;

	if (!heap_head)
		heap_preallocate();

	block = find_free_block(size);
	if (!block)
		block = heap_append(size);

	block->status = STATUS_ALLOC;

	return block;
}

/*
 * Common allocation path: blocks whose total size (metadata + payload)
 * reaches the threshold are mapped, smaller ones go on the heap.
 */
static void *allocate(size_t size, size_t threshold)
{
	struct block_meta *block;

	if (size == 0)
		return NULL;

	size = ALIGN(size);

	if (size + META_SIZE >= threshold)
		block = alloc_mapped(size);
	else
		block = alloc_heap(size);

	return block_payload(block);
}

void *os_malloc(size_t size)
{
	return allocate(size, MMAP_THRESHOLD);
}

void os_free(void *ptr)
{
	struct block_meta *block;

	if (!ptr)
		return;

	block = payload_block(ptr);

	if (block->status == STATUS_MAPPED) {
		int ret = munmap(block, block->size + META_SIZE);

		DIE(ret == -1, "munmap");
		return;
	}

	block->status = STATUS_FREE;
}

void *os_calloc(size_t nmemb, size_t size)
{
	/* Implemented in a later stage. */
	(void)nmemb;
	(void)size;
	return NULL;
}

void *os_realloc(void *ptr, size_t size)
{
	/* Implemented in a later stage. */
	(void)ptr;
	(void)size;
	return NULL;
}
