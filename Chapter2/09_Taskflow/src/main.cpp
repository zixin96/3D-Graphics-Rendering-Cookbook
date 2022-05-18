#include <stdio.h>
#include <stdint.h>

#include <iostream>
#include <fstream>
#include <thread>
#include <vector>

#include <taskflow/taskflow.hpp>

// Let's create and run a set of concurrent dependent tasks via the for_each() algorithm

int main()
{
	// The tf::Taskflow class is the main place to create a task dependency graph
	tf::Taskflow taskflow;

	// create a data vector to process
	std::vector<int> items{1, 2, 3, 4, 5, 6, 7, 8};

	// The for_each() member function returns a task that implements a parallel for loop algorithm
	auto task = taskflow.for_each(
		items.begin(), items.end(), [](int item)
		{
			std::cout << item;
		}
	);

	// attach work before the parallel-for task: start message
	taskflow.emplace([]() { std::cout << "\nS - Start\n"; }).name("S").precede(task);
	// attach work after the parallel-for task: end message
	taskflow.emplace([]() { std::cout << "\nT - End\n"; }).name("T").succeed(task);

	// Save the generated tasks dependency graph in .dot format that can be used by GraphViz tool
	{
		std::ofstream os("taskflow.dot");
		taskflow.dump(os);
	}

	// create an executor object
	tf::Executor executor;
	// run the constructed taskflow graph 
	executor.run(taskflow).wait();

	// One important part to mention here is that the dependency graph can only be constructed
	// once.Then, it can be reused in every frame to run concurrent tasks efficiently

	return 0;
}
