#pragma once

#include<iostream>
#include<thread>
#include<mutex>
#include<future>
#include<condition_variable>
#include<queue>
#include<functional>


class ThreadPool
{
private:
	void worker();//线程执行内容
	bool isstop;//表示 当前进程池是不是停止 true停止
	std::condition_variable cv;//条件变量
	std::mutex mtx;//互斥锁
	std::vector<std::thread> workers;//线程集合（线程池）
	std::queue<std::function<void()>> myque;//任务队列 存储返回值为void的函数
public:
	ThreadPool(int thead_nums);

	template<class F,class ...Arg> //...Arg 是可变参数 接受一个或多个参数
	auto enques(F&& f, Arg&&... arg) -> std::future<typename std::result_of<F(Arg...)>::type>;
	//&&右值引用 模板中万能应用
	//auto ... -> ... 返回类型后置
	//std::result_of<...>::type 用于获得参数返回值类型并以type返回
	//

	~ThreadPool();
};

ThreadPool::ThreadPool(int thread_num) :isstop(false)
{
	for (int i = 0; i < thread_num; i++)
	{
		workers.emplace_back([this]() {
			this->worker();
			});
	}
}

ThreadPool::~ThreadPool() {
	{
		//更改停止标识
		std::unique_lock<std::mutex> lk(mtx);
		isstop = true;
	}

	//通知所有阻塞中的线程
	cv.notify_all();

	for (std::thread& onethread : workers)
	{
		onethread.join();//确保线程执行完毕
	}
}

template <class F, class ...Arg>
auto ThreadPool::enques(F&& f, Arg&& ...arg) ->std::future<typename std::result_of<F(Arg...)>::type>
{
	//获取f执行后类型
	using functype = typename std::result_of<F(Arg...)>::type;

	//获得一个智能指针 指向一个被包装成functype()的task
	auto task = std::make_shared<std::packaged_task<functype()>>(
		std::bind(std::forward<F>(f), std::forward<Arg>(arg)...)//std::forward 完美转发 将接受的完整传入
		);

	std::future<functype> rsfuture = task->get_future();

	{
		std::lock_guard<std::mutex> lockguard(this->mtx);
		if (isstop) 
			throw std::runtime_error("出错: 线程池已经停止");

		//将任务添加到队列
		myque.emplace([task]() {
			(*task)();//task是指向任务的指针 *task表示指向的任务 (*task)() 就是调用指向任务 ()调用符
			});
	}

	cv.notify_one();

	return rsfuture;
}

void ThreadPool::worker()
{
	while (true)
	{
		//定义任务
		std::function<void()> task;

		//从队列取得一个任务
		{
			std::unique_lock<std::mutex> lock(mtx);
			cv.wait(lock, [this] {return this->isstop || !this->myque.empty(); });

			if (isstop && myque.empty()) return;
			task = std::move(this->myque.front());
			this->myque.pop();
		}
		task();
	}
}