#ifndef TASK_H
#define TASK_H

#include <iostream>
#include <experimental/coroutine>
#include <experimental/resumable>

static int task_counter = 0;


template<typename T> class task;


class task_promise_base {
public:
	std::experimental::suspend_never initial_suspend() noexcept {
		std::cout << "task::promise_type::initial_suspend()\n";
		return {};
	}
	auto final_suspend() noexcept {
		std::cout << "task::promise_type::final_suspend()\n";

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
		std::cout << "task::promise_type::set_waiter() " << "\n";
		_waiter = waiter;
	}

	void unhandled_exception() noexcept {
		std::cout << "task::promise_type::unhandled_exception()\n";
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
		std::cout << "task::promise_type::return_value()\n";
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
		std::cout << "task::promise_type::return_void()\n";
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
		std::cout << "task()" << " " << _id << std::endl;
	}

	~task() {
		std::cout << "~task()" << " " << _id << std::endl;
	}

	bool await_ready() {
		return (_coro.done());
	}

	//another coroutine wants to co_await this coroutine.
	//set it to promise object, so it will continue at final_suspend point
	void await_suspend(std::experimental::coroutine_handle<> waiter) {
		std::cout << "task::await_suspend() " << _id << "\n";
		_coro.promise().set_waiter(waiter);
	}

	// returns the result of co_await (co await returns whatever await_resume returns)
	//this returns the result to the awaiting coroutine's co_await. we get result from
	//this coroutine's promise
	T await_resume() {
		std::cout << "task::await_resume() " << _id << "\n";
		return _coro.promise().result();
	}

private:
	std::experimental::coroutine_handle<promise_type> _coro;
	int _id = task_counter++;
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