// Contains useful memory management symbols.

#ifndef MEMORY_INCLUDED
#define MEMORY_INCLUDED

#include "core.h"

#define BYTES(x) (x ## ULL)
#define KiB(x) (BYTES(x) * 1024)
#define MiB(x) (KiB(x) * 1024)
#define GiB(x) (MiB(x) * 1024)

#define RAW_ALLOC_DEFAULT_ALIGN (4) // Default alignment of non-typed memory allocations.

// Simple Arena allocator taking ownership over a piece of memory, holding a function pointer determining its allocation strategy.
// It is not possible to de-allocate from an arena. Simply discard it and reuse the memory.
struct mem_arena
{
	ui8* mem_start; // Start of managed memory. May not be fully contiguous depending on allocation strategy.
	ui64 mem_size; // Size of managed memory.
	ui64 allocated_count; // Number of allocated bytes in total.

	// Defines an allocation strategy function for a memory arena allocator. The arena itself is passed so the function may check its state and cast
	// it to a specialized type if needed.
	// Do not call directly ! Use alloc() so the proper assertions can run. And it's more convenient anyway :)
	typedef void*(*alloc_func_ptr)(mem_arena& arena, ui64 size, ui32 align);
	alloc_func_ptr _alloc_func;

	// Shortand for calling the internal allocation function.
	// Returns nullptr if allocation failed.
	inline void* alloc(ui64 size, ui32 align = RAW_ALLOC_DEFAULT_ALIGN) { ASSERT(_alloc_func != nullptr); return _alloc_func(*this, size, align); }

	// Shorthand for calling the internal allocation function, with size computed from size of item type * item count, and correct alignment.
	// Returns nullptr if allocation failed.
	template<typename Type>
	inline Type* alloc(ui64 item_count = 1) { ASSERT(_alloc_func != nullptr && item_count > 0); return (Type*)_alloc_func(*this, sizeof(Type) * item_count, alignof(Type)); }
};

void ia_memcpy(void* dest, const void* src, ui64 size);
void ia_memzero(void* dest, ui64 size);
void ia_memset(void* dest, ui8 val, ui64 size);

// Default allocation strategy given to a new arena. Assumes owned memory is contiguous, pre-allocated and non-extendable.
// Can be replaced by any valid function within a specific arena, allowing varying allocation strategies (for example, the ability to grow).
// TODO(Marc): Create alternative strategies. 
static void* mem_arena_alloc_default(mem_arena& arena, ui64 size, ui32 align)
{
	// Simple stack allocation base on allocated_count.
	ui8* alloc_start = arena.mem_start + arena.allocated_count;

	// Advance to alignment boundary.
	ui32 align_advance = (((iptr)alloc_start) % align) % align;
	if (align_advance > 0)
	{
		align_advance = align - align_advance;
		alloc_start += align_advance;
		size += align_advance;
	}

	if (arena.mem_size - arena.allocated_count < size)
	{
		// Out of memory.
		return nullptr;
	}

	arena.allocated_count += size;
	return alloc_start;
}

static inline mem_arena mem_arena_create(ui8* owned_mem, ui64 owned_mem_size)
{
	ASSERT(owned_mem != nullptr && owned_mem_size > 0);

	mem_arena newArena = { 0 };
	newArena.mem_start = owned_mem;
	newArena.mem_size = owned_mem_size;
	
	newArena._alloc_func = mem_arena_alloc_default;

	// Zero out all owned memory.
	ia_memzero(owned_mem, owned_mem_size);

	return newArena;
}

// Creates a new mem arena, assigning it memory allocated from a "parent" arena.
// ASSERTS that the parent arena has enough memory, so take care to check if there's any reason it could be too big to fit !
static inline mem_arena mem_arena_create_sub(mem_arena& parent, ui64 owned_mem_size)
{
	ASSERT(parent.mem_start != nullptr && parent.mem_size > 0);

	ui8* memStart = (ui8*)parent.alloc(owned_mem_size);
	ASSERT_MSG(memStart != nullptr, "Child arena of size %llu could not fit in parent arena of size %llu with %lld remaining bytes.", owned_mem_size, parent.mem_size, parent.mem_size - parent.allocated_count);

	return mem_arena_create(memStart, owned_mem_size);
}

// TODO(Marc): Optimize this.
void ia_memcpy(void* dest, const void* src, ui64 size)
{
	if (size == 0) return;

	ASSERT(dest != nullptr && src != nullptr);

	ui8* destMem = (ui8*)dest;
	ui8* srcMem = (ui8*)src;

	for (ui64 i = 0; i < size; i++)
	{
		destMem[i] = srcMem[i];
	}
}

// TODO(Marc): optimize this.
void ia_memzero(void* dest,  ui64 size)
{
	if (size == 0) return;

	ASSERT(dest != nullptr);

	ui8* destMem = (ui8*)dest;

	for (ui64 i = 0; i < size; i++)
	{
		if (size - i > 8)
		{
			*(ui64*)(destMem + i) = 0;
			i += 7; // advance i by 7 + the increment post loop, advancing writing by 8 bytes in one loop.
			continue;
		}
		destMem[i] = 0;
	}
}

// TODO(Marc): optimize this.
void ia_memset(void* dest, ui8 val, ui64 size)
{
	if (size == 0) return;

	ASSERT(dest != nullptr);

	if (val == 0)
	{
		ia_memzero(dest, size);
		return;
	}

	ui8* destMem = (ui8*)dest;

	for (ui64 i = 0; i < size; i++)
	{
		destMem[i] = val;
	}
}

#endif // MEMORY_INCLUDED