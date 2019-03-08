#include "pch.h"
#include "jobsystem.h"
#include <algorithm>
#include <iostream>

jobsystem_t::jobsystem_t()
	:jobindex(0)
{
}


jobsystem_t::~jobsystem_t()
{
}

void jobsystem_t::addjob(job_t job)
{
	jobs.push_back(job);
}

void jobsystem_t::startjobs()
{
	jobcount = (int)jobs.size();
	size_t hardware_threads = (size_t)std::thread::hardware_concurrency();
	size_t threadcount = std::max((size_t)1, std::min(hardware_threads / 2, jobs.size()));
	std::cout << "there are " << hardware_threads << " hardware threads.\n";
	std::cout << threadcount << " threads is starting for " << jobcount << " jobs.\n";
	for (size_t i = 0; i < threadcount; i++)
	{
		threads.emplace_back(&jobsystem_t::threadentry, this);
	}
}

void jobsystem_t::wait()
{
	for (size_t i = 0; i < threads.size(); i++)
		threads[i].join();
}

void jobsystem_t::threadentry()
{
	while (1)
	{
		int fetchindex = jobindex++;
		if (fetchindex >= jobcount) break;
		job_t job = jobs[fetchindex];
		job();
	}
}
