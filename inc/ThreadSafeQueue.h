#include <queue>
#include <memory>
#include <condition_variable>
#include <mutex>

template<typename T>
class threadsafe_queue
{
private:
	mutable std::mutex		_mut;
	std::queue<T>			_data_queue;//队里存储的是shared_ptr这样可以保证push和pop操作时不会引起构造或析构异常，队列更加高效
	std::condition_variable _cond;//采用条件变量同步入队和出队操作
public:
	threadsafe_queue() {}
	bool wait_and_pop(T& value, int mSec)//容器中有元素或等待超时
	{
		bool bRet(false);
		std::unique_lock<std::mutex> lk(_mut);
		bRet = _cond.wait_for(lk, std::chrono::milliseconds(mSec), [this] {return !_data_queue.empty(); });
		if (bRet)
		{
			value = _data_queue.front();
			_data_queue.pop();
		}
		else
			value = T();
		
		return bRet;
	}
	bool try_pop(T& value)//若队中无元素可以删除则直接返回false
	{
		std::lock_guard<std::mutex> lk(_mut);
		if (_data_queue.empty())
			return false;
		value = _data_queue.front();
		_data_queue.pop();
		return true;
	}
	T wait_and_pop()
	{
		std::unique_lock<std::mutex> lk(_mut);
		_cond.wait(lk, [this] {return !_data_queue.empty(); });
		T val = _data_queue.front();
		_data_queue.pop();
		return val;
	}
	void push(T data)
	{
		std::lock_guard<std::mutex> lk(_mut);
		_data_queue.push(data);
		_cond.notify_one();
	}
	bool empty() const
	{
		std::lock_guard<std::mutex> lk(_mut);
		return _data_queue.empty();
	}
};