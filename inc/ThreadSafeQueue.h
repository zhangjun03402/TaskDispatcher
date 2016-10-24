#include <queue>
#include <memory>
#include <condition_variable>
#include <mutex>

template<typename T>
class threadsafe_queue
{
private:
	mutable std::mutex		_mut;
	std::queue<T>			_data_queue;//����洢����shared_ptr�������Ա�֤push��pop����ʱ����������������쳣�����и��Ӹ�Ч
	std::condition_variable _cond;//������������ͬ����Ӻͳ��Ӳ���
public:
	threadsafe_queue() {}
	bool wait_and_pop(T& value, int mSec)//��������Ԫ�ػ�ȴ���ʱ
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
	bool try_pop(T& value)//��������Ԫ�ؿ���ɾ����ֱ�ӷ���false
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