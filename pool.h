#pragma once

#include <vector>
#include <numeric>
#include <iostream>
#include <print>
#include <new>
#include <cassert>

template <typename T, typename Allocator = std::allocator<T>>
class ObjectPool {
public:
	ObjectPool() = default;
	explicit ObjectPool(const Allocator& allocator);
	virtual ~ObjectPool();

	//Allow move construction and move assignment
	ObjectPool(ObjectPool&& src) noexcept = default;
	ObjectPool& operator= (ObjectPool&& rhs) = default;

	//Prevent copy construction and copy assignment
	ObjectPool(const ObjectPool& src) = delete;
	ObjectPool& operator=(const ObjectPool& rhs) = delete;

	//Reserves and returns an object from the pool. Arguments can be
	//provided which are perfectly forwarded to a constructor of T
	template<typename... Args>
	std::shared_ptr<T> acquireObject(Args&&... args);

private:
	//Contains chunks of memory in which instances of T will be created
	//For each chunk, the pointer to its first object is stored
	std::vector<T*> m_pool;

	//Contains pointers to all free instances of T that are 
	//available in the pool
	std::vector<T*> m_freeObjects;

	//The number of T instances that should fit in the first allocated chunk
	static const size_t ms_initialChunkSize{ 5 };

	//The number of T instances that should fit in a newly allocated chunk
	//This value is doubled after each newly created chunk
	size_t m_newChunkSize{ ms_initialChunkSize };

	//Create a new block of uninitialized memory big enough to hold 
	//m_newChunkSize instances of T
	void addChunk();

	//The allocator to use for allocating and deallocating chunks
	Allocator m_allocator;
};



//Function definitions

template <typename T, typename Allocator>
ObjectPool<T, Allocator>::ObjectPool(const Allocator& allocator) :
	m_allocator{ allocator }
{}


template <typename T, typename Allocator>
void ObjectPool<T, Allocator>::addChunk() {
	std::println("Allocating new chunk...");

	//Allocate a new chunk of unintialized memory big enough to hold
	//m_newChunkSize instances of T, and add the chunk to the pool
	//Care is taken that everything is cleared up in the event of an excption
	m_pool.push_back(nullptr);
	try {
		m_pool.back() = m_allocator.allocate(m_newChunkSize);
	}
	catch (...) {
		m_pool.pop_back();
		throw;
	}
	//Create pointers to each individual object in the new chunk
	//and store them in the list of free objects
	auto oldFreeObjectsSize{ m_freeObjects.size() };
	m_freeObjects.resize(oldFreeObjectsSize + m_newChunkSize);
	std::iota(std::begin(m_freeObjects) + oldFreeObjectsSize, std::end(m_freeObjects), m_pool.back());
	
	//Double the chunk size for next time
	m_newChunkSize *= 2;
}

//acquireObject returns a free object from the pool, allocating a new chunk if there are no more
//free objects available. Adding a new chunk just allocates a block of uninitialized memory. It
//is the responsibility of acquireObject() to properly construct a new instance of T at the right
//place in memory. This is done using a placement new operator. If type T contains any const
//or reference members, accessing the newly constructed object through the original pointer
//triggers undefined behavior. To turn this into defined behavior, the memory is laundered using
//std::launder(), defined in <new>.
//Finally, the laundered T* pointer is wrapped in a shared_ptr with a custom deleter. The 
//deleter does not deallocate any memory; instead, it manually calls the destructor by using
//std::destroy_at() and then puts the pointer back on the list of available objects
template <typename T, typename Allocator>
template <typename... Args>
std::shared_ptr<T> ObjectPool<T, Allocator>::acquireObject(Args&&... args) {
	//if there are no free objects, allocate a new chunk
	if (m_freeObjects.empty()) {
		addChunk();
	}
	//get a free object
	T* object{ m_freeObjects.back() };

	//Initialize an instance of T in an unitialized block of memory using placement
	//new, and perfectly forward any provided arguments to the constructor
	::new(object) T{ std::forward<Args>(args)... };

	//launder the object pointer
	T* constructedObject{ std::launder(object) };

	//Remove the object from the list of free objects
	m_freeObjects.pop_back();

	//wrap the constructed object and return it.
	return std::shared_ptr<T>{constructedObject, [this](T* object) {
		std::destroy_at(object);
		m_freeObjects.push_back(object);
		}};
}


//The destructor of the pool must deallocate any allocated memory using the given
//allocator
template <typename T, typename Allocator>
ObjectPool<T, Allocator>::~ObjectPool() {
	//Note: this implementation assumes that all objects handed out by this
	//pool have been returned to the pool before the pool is destroyed.
	//The following statement asserts if that is not the case
	assert(m_freeObjects.size() ==
		ms_initialChunkSize * (std::pow(2, m_pool.size()) - 1));
	//Deallocate all allocated memory
	std::size_t chunkSize{ ms_initialChunkSize };
	for(auto* chunk : m_pool) {
		m_allocator.deallocate(chunk, chunkSize);
		chunkSize *= 2;
	}
	m_pool.clear();
}