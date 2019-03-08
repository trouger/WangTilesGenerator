#pragma once

#include <vector>
#include <functional>
#include <atomic>
#include <thread>

// a simple job system, where all jobs must be added before start the system
class jobsystem_t
{
public:
	jobsystem_t();
	~jobsystem_t();

	typedef std::function<void()> job_t;
	void addjob(job_t job);
	void startjobs();
	void wait();

private:
	void threadentry();

private:
	std::vector<job_t> jobs;
	std::atomic<int> jobindex;
	int jobcount;
	std::vector<std::thread> threads;
};

