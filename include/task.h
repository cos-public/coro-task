#ifndef TASK_H
#define TASK_H

#include <experimental/coroutine>


template<typename T> class task;


class task_promise_base {
public:
	std::experimental::suspend_never initial_suspend() noexcept {
		return {};
	}
	auto final_suspend() noexcept {
		struct final_awaiter {
			std::experimental::coroutine_handle<> waiter;
			bool await_ready() { return false; }
			void await_resume() { }
			void await_suspend(std::experimental::coroutine_handle<> coroutine) {
				if (waiter)
					waiter.resume();
			}
		};

		return final_awaiter{_waiter};
	}

	void set_waiter(std::experimental::coroutine_handle<> & waiter) {
		_waiter = waiter;
	}

	void unhandled_exception() noexcept {
		_e = std::current_exception();
	}

	std::experimental::coroutine_handle<> _waiter;
	std::exception_ptr _e;
};


template<typename T>
class task_promise final : public task_promise_base {
public:
	task_promise() noexcept = default;
	~task_promise() {
	}

	task<T> get_return_object() noexcept;

	void return_value(T value) {
		m_value = value;
	}

	T result() {
		if (_e)
			std::rethrow_exception(_e);
		return m_value;
	}

private:
	T m_value;
};


template<>
class task_promise<void> final : public task_promise_base {
public:
	task_promise() noexcept = default;

	task<void> get_return_object() noexcept;

	void return_void() {
	}

	void result() {
		if (_e)
			std::rethrow_exception(_e);
	}
};


template <typename T = void>
class task {
public:
	using promise_type = task_promise<T>;

	explicit task(std::experimental::coroutine_handle<promise_type> & coro) : _coro(coro) {
	}

	bool await_ready() {
		return (_coro.done());
	}

	//another coroutine wants to co_await this coroutine.
	//set it to promise object, so it will continue at final_suspend point
	void await_suspend(std::experimental::coroutine_handle<> waiter) {
		_coro.promise().set_waiter(waiter);
	}

	// returns the result of co_await (co await returns whatever await_resume returns)
	//this returns the result to the awaiting coroutine's co_await. we get result from
	//this coroutine's promise
	T await_resume() {
		return _coro.promise().result();
	}

private:
	std::experimental::coroutine_handle<promise_type> _coro;
};



template<typename T>
task<T> task_promise<T>::get_return_object() noexcept {
	auto h = std::experimental::coroutine_handle<task_promise>::from_promise(*this);
	return task{h};
}

inline task<void> task_promise<void>::get_return_object() noexcept {
	auto h = std::experimental::coroutine_handle<task_promise>::from_promise(*this);
	return task{h};
}


#endif //TASK_H