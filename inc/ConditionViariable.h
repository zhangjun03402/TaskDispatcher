class my_mutex
public:
	my_mutex (bool be_initial_owner = false)
	{
		mutex_ = CreateMutexA (NULL, be_initial_owner, NULL);
	}
	~my_mutex (void)
	{
		CloseHandle (mutex_);
	}
public:
	int acquire (void)
	{
		DWORD ret = WaitForSingleObject (mutex_, INFINITE);
		return ret == WAIT_OBJECT_0 ? 0 : -1;
	}
	int release (void)
	{
		BOOL bret = ReleaseMutex (mutex_);
		return bret ? 0 : -1;
	}
	HANDLE handle (void)
	{
		return mutex_;
	}
protected:
	HANDLE mutex_;
};
class my_semaphore
{
public:
	my_semaphore (long init_count, long max_count = (std::numeric_limits<long>::max)())
	{
		assert (init_count >= 0 && max_count > 0 && init_count <= max_count);
		sema_ = CreateSemaphoreA (NULL, init_count, max_count, NULL);
	}
	~my_semaphore (void)
	{
		CloseHandle (sema_);
	}
public:
	int post (long count = 1)
	{
		BOOL bret = ReleaseSemaphore (sema_, count, NULL);
		return bret ? 0 : -1;
	}
	int wait (long timeout = -1)
	{
		DWORD ret = WaitForSingleObject (sema_, timeout);
		return ret == WAIT_OBJECT_0 ? 0 : -1;
	}
	HANDLE handle (void)
	{
		return sema_;
	}
protected:
	HANDLE sema_;
};
template<typename MUTEX>
class my_condition
{
public:
	my_condition (MUTEX &m)
		: mutex_ (m)
		, waiters_ (0)
		, sema_ (0)
	{
	}
	~my_condition (void)
	{
	}
public:
	/// Returns a reference to the underlying mutex_;
	MUTEX &mutex (void)
	{
		return mutex_;
	}
	/// Signal one waiting thread.
	int signal (void)
	{
		// must hold the external mutex before enter
		if ( waiters_ > 0 )
			sema_.post ();
		return 0;
	}
	/// Signal *all* waiting threads.
	int broadcast (void)
	{
		// must hold the external mutex before enter
		if ( waiters_ > 0 )
			sema_.post (waiters_);
		return 0;
	}
	int wait (unsigned long wait_time = -1)
	{
		// must hold the external mutex before enter
		int ret = 0;
		waiters_++;
		ret = SignalObjectAndWait (mutex_.handle (), sema_.handle (), wait_time, FALSE);
		mutex_.acquire ();
		waiters_ --;
		return ret == WAIT_OBJECT_0 ? 0 : -1;
	}
protected:
	MUTEX &mutex_;
	/// Number of waiting threads.
	long waiters_;
	/// Queue up threads waiting for the condition to become signaled.
	my_semaphore sema_;
};
