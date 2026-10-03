// Contains useful memory management symbols.

#ifndef CORE_MEMORY_INCLUDED
#define CORE_MEMORY_INCLUDED

#include "std_types.h"
#include "assert.h"

#define BYTES(x) (x ## ULL)
#define KiB(x) (BYTES(x) * 1024)
#define MiB(x) (KiB(x) * 1024)
#define GiB(x) (MiB(x) * 1024)

#define RAW_ALLOC_DEFAULT_ALIGN (4) // Default alignment of non-typed memory allocations.

static void ia_memcpy(void* dest, const void* src, ui64 size);
static void ia_memzero(void* dest, ui64 size);
static void ia_memset(void* dest, ui8 val, ui64 size);

// Simple Arena allocator taking ownership over a piece of memory, holding a function pointer determining its allocation strategy.
// It is not possible to de-allocate from an arena, it can only be cleared.
// Structure itself contains the core usage functions. Construction and more functionality for querying or other special operations can be found outside.
// TODO(Marc): Add dynamic growth as required, using a paging system (address space remains static, extra memory is committable) on platforms that support it.
struct mem_arena
{
	ui8* mem_start; // Start of managed memory. May not be fully contiguous depending on allocation strategy.
	ui64 mem_size; // Size of managed memory.
	ui64 allocated_count; // Number of allocated bytes in total.

	// Allocates the given number of bytes, with a specifiable alignment value (4 by default).
	// Returns nullptr if allocation failed. Does NOT zero-out memory.
	inline void* alloc(ui64 size, ui32 align = RAW_ALLOC_DEFAULT_ALIGN) 
	{ 
		ASSERT(size > 0);
		ASSERT(mem_start != nullptr && mem_size > 0);

		// Simple stack allocation base on allocated_count.
		ui8* alloc_start = mem_start + allocated_count;

		// Advance to alignment boundary.
		ui32 align_advance = ((iptr)alloc_start) % align;
		if (align_advance > 0)
		{
			align_advance = align - align_advance;
			alloc_start += align_advance;
			size += align_advance;
		}

		if (mem_size - allocated_count < size)
		{
			// Out of memory.
			return nullptr;
		}
		
		allocated_count += size;
		return alloc_start;
	}

	// Shorthand for calling the allocation function, with size computed from size of item type * item count, and correct alignment.
	// Returns nullptr if allocation failed. By default zeroes-out memory.
	template<typename Type>
	inline Type* alloc(ui64 item_count = 1, bool zero_mem = true) 
	{ 
		ASSERT(item_count > 0); 
		Type* allocation = (Type*)alloc(sizeof(Type) * item_count, alignof(Type));
		if (allocation == nullptr)
		{
			return nullptr;
		}

		if (zero_mem)
		{
			ia_memzero(allocation, sizeof(Type) * item_count);
		}

		return allocation;
	}

	inline void clear() 
	{	
		ASSERT(mem_start != nullptr && mem_size > 0);

		// Reset number of allocated bytes.
		allocated_count = 0;
	}
};

static inline mem_arena mem_arena_create(ui8* owned_mem, ui64 owned_mem_size)
{
	ASSERT(owned_mem != nullptr && owned_mem_size > 0);

	mem_arena newArena = { 0 };
	newArena.mem_start = owned_mem;
	newArena.mem_size = owned_mem_size;
	
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

// Returns the address of the next allocatable byte in the arena memory. Returns nullptr if full.
static inline ui8* mem_arena_get_next_alloc(const mem_arena& arena)
{
	ASSERT(arena.mem_start != nullptr && arena.mem_size > 0);
	return arena.mem_start + arena.allocated_count;
}

// TODO(Marc): Optimize this.
static void ia_memcpy(void* dest, const void* src, ui64 size)
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
static void ia_memzero(void* dest,  ui64 size)
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
static void ia_memset(void* dest, ui8 val, ui64 size)
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

// TODO(Marc): optimize this.
static void ia_memmove(void* dest, const void* src, ui64 size)
{
	if (size == 0 || dest == src) return;

	ASSERT(dest != nullptr && src != nullptr);

	ui8* destMem = (ui8*)dest;
	const ui8* srcMem = (const ui8*)src;

	if (destMem < srcMem)
	{
		for (ui64 i = 0; i < size; i++) destMem[i] = srcMem[i];
	}
	else
	{
		for (ui64 i = size; i > 0; i--) destMem[i - 1] = srcMem[i - 1];
	}
}

// Static variant of the mem arena which contains its own memory statically.
template<ui64 Size>
struct static_mem_arena : public mem_arena
{
	ui8 _mem_static[Size];

	inline static_mem_arena() 
	{
		mem_arena temp = mem_arena_create(_mem_static, Size);
		*(mem_arena*)this = temp; // Move properties from temporary "standard" arena created over own memory.
	}

	// Copying would leave the copy's mem_start pointing into the original's memory.
	static_mem_arena(const static_mem_arena&) = delete;
	static_mem_arena& operator=(const static_mem_arena&) = delete;
};

// These exist for the compiler to call when there's no libc to provide them (freestanding builds).
// Use the ia_ versions above in source code.
#if !__STDC_HOSTED__

extern "C" void* memset(void* dest, int val, mem_size size)
{
	ia_memset(dest, (ui8)val, size);
	return dest;
}

extern "C" void* memcpy(void* dest, const void* src, mem_size size)
{
	ia_memcpy(dest, src, size);
	return dest;
}

extern "C" void* memmove(void* dest, const void* src, mem_size size)
{
	ia_memmove(dest, src, size);
	return dest;
}

#endif // !__STDC_HOSTED__

#endif // CORE_MEMORY_INCLUDED