#ifndef rapidjson_internal_stack_h_
#define rapidjson_internal_stack_h_

namespace rapidjson {
namespace internal {

///////////////////////////////////////////////////////////////////////////////
// stack

//! a type-unsafe stack for storing different types of data.
/*! \tparam allocator allocator for allocating stack memory.
*/
template <typename allocator>
class stack {
public:
	stack(allocator* allocator, size_t stack_capacity) : allocator_(allocator), own_allocator_(0), stack_(0), stack_top_(0), stack_end_(0), stack_capacity_(stack_capacity) {
		rapidjson_assert(stack_capacity_ > 0);
		if (!allocator_)
			own_allocator_ = allocator_ = new allocator();
		stack_top_ = stack_ = (char*)allocator_->malloc(stack_capacity_);
		stack_end_ = stack_ + stack_capacity_;
	}

	~stack() {
		allocator::free(stack_);
		delete own_allocator_; // only delete if it is owned by the stack
	}

	void clear() { /*stack_top_ = 0;*/ stack_top_ = stack_; }

	template<typename t>
	t* push(size_t count = 1) {
		 // expand the stack if needed
		if (stack_top_ + sizeof(t) * count >= stack_end_) {
			size_t new_capacity = stack_capacity_ * 2;
			size_t size = getsize();
			size_t new_size = getsize() + sizeof(t) * count;
			if (new_capacity < new_size)
				new_capacity = new_size;
			stack_ = (char*)allocator_->realloc(stack_, stack_capacity_, new_capacity);
			stack_capacity_ = new_capacity;
			stack_top_ = stack_ + size;
			stack_end_ = stack_ + stack_capacity_;
		}
		t* ret = (t*)stack_top_;
		stack_top_ += sizeof(t) * count;
		return ret;
	}

	template<typename t>
	t* pop(size_t count) {
		rapidjson_assert(getsize() >= count * sizeof(t));
		stack_top_ -= count * sizeof(t);
		return (t*)stack_top_;
	}

	template<typename t>
	t* top() { 
		rapidjson_assert(getsize() >= sizeof(t));
		return (t*)(stack_top_ - sizeof(t));
	}

	template<typename t>
	t* bottom() { return (t*)stack_; }

	allocator& getallocator() { return *allocator_; }
	size_t getsize() const { return stack_top_ - stack_; }
	size_t getcapacity() const { return stack_capacity_; }

private:
	allocator* allocator_;
	allocator* own_allocator_;
	char *stack_;
	char *stack_top_;
	char *stack_end_;
	size_t stack_capacity_;
};

} // namespace internal
} // namespace rapidjson

#endif // rapidjson_stack_h_
