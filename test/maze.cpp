/* FiberTaskingLib - A tasking library that uses fibers for efficient task switching
 *
 * This library was created as a proof of concept of the ideas presented by
 * Christian Gyrling in his 2015 GDC Talk 'Parallelizing the Naughty Dog Engine Using Fibers'
 *
 * http://gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine
 *
 * FiberTaskingLib is the legal property of Adrian Astley
 * Copyright Adrian Astley 2015
 */

#include "fiber_tasking_lib/task_scheduler.h"
#include "fiber_tasking_lib/global_args.h"

#include "maze10x10.h"

#include <cstdio>
#include <fstream>
#include <gtest/gtest.h>




struct MazeType {
	MazeType(char *data, int width, int height)
		: Data(data),
		  Width(width),
		  Height(height) {
	}

	char *Data;
	int Width;
	int Height;
};

void PrintMaze(MazeType &maze) {
	for (int y = 0; y < maze.Height; ++y) {
		for (int x = 0; x < maze.Width; ++x) {
			printf("%c", *(maze.Data + (y * maze.Width + x)));
		}
		printf("\n");
	}
}

struct BranchArgs {
	BranchArgs(MazeType *maze, int currX, int currY, FiberTaskingLib::AtomicCounter *completed)
		: Maze(maze),
		  CurrX(currX),
		  CurrY(currY),
		  Completed(completed) {
	}

	MazeType *Maze;
	int CurrX;
	int CurrY;
	FiberTaskingLib::AtomicCounter *Completed;
};

// Forward declare
TASK_ENTRY_POINT(CheckBranch);

bool CheckDirection(char *areaToCheck, MazeType *maze, int newX, int newY, FiberTaskingLib::AtomicCounter *completed, FiberTaskingLib::TaggedHeapBackedLinearAllocator *allocator, FiberTaskingLib::TaskScheduler *taskScheduler) {
	if (*areaToCheck == 'E') {
		// We found the end
		completed->store(1);
		return true;
	} else if (*areaToCheck == ' ') {
		*areaToCheck = '*';

		BranchArgs *newBranchArgs = new(allocator->allocate(sizeof(BranchArgs)))
			BranchArgs(maze, newX, newY, completed);

		FiberTaskingLib::Task newBranch = {CheckBranch, newBranchArgs};
		taskScheduler->AddTask(newBranch);
	}

	return false;
}

TASK_ENTRY_POINT(CheckBranch) {
	BranchArgs * branchArgs = (BranchArgs *)arg;

	// Check right
	if (branchArgs->CurrX + 1 < branchArgs->Maze->Width) {
		char *spaceToCheck = branchArgs->Maze->Data + (branchArgs->CurrY * branchArgs->Maze->Width + branchArgs->CurrX + 1);
		if (CheckDirection(spaceToCheck, branchArgs->Maze, branchArgs->CurrX + 1, branchArgs->CurrY, branchArgs->Completed, g_allocator, g_taskScheduler)) {
			return;
		}
	}

	// Check left
	if (branchArgs->CurrX - 1 >= 0) {
		char *spaceToCheck = branchArgs->Maze->Data + (branchArgs->CurrY * branchArgs->Maze->Width + branchArgs->CurrX - 1);
		if (CheckDirection(spaceToCheck, branchArgs->Maze, branchArgs->CurrX - 1, branchArgs->CurrY, branchArgs->Completed, g_allocator, g_taskScheduler)) {
			return;
		}
	}

	// Check up
	if (branchArgs->CurrY - 1 >= 0) {
		char *spaceToCheck = branchArgs->Maze->Data + ((branchArgs->CurrY - 1) * branchArgs->Maze->Width + branchArgs->CurrX);
		if (CheckDirection(spaceToCheck, branchArgs->Maze, branchArgs->CurrX, branchArgs->CurrY - 1, branchArgs->Completed, g_allocator, g_taskScheduler)) {
			return;
		}
	}

	// Check down
	if (branchArgs->CurrY + 1 < branchArgs->Maze->Height) {
		char *spaceToCheck = branchArgs->Maze->Data + ((branchArgs->CurrY + 1) * branchArgs->Maze->Width + branchArgs->CurrX);
		if (CheckDirection(spaceToCheck, branchArgs->Maze, branchArgs->CurrX, branchArgs->CurrY + 1, branchArgs->Completed, g_allocator, g_taskScheduler)) {
			return;
		}
	}
}


TEST(FunctionalTest, Maze10x10) {
	FiberTaskingLib::GlobalArgs *globalArgs = new FiberTaskingLib::GlobalArgs();
	globalArgs->TaskScheduler.Initialize(110, globalArgs);
	globalArgs->Allocator.init(&globalArgs->Heap, 1234);

	FiberTaskingLib::AtomicCounter *completed = new FiberTaskingLib::AtomicCounter();
	completed->store(0);

	char *mazeData = new char[21 *21];
	memcpy(mazeData, kMaze10x10, 21 * 21);

	BranchArgs *startBranch = new BranchArgs(new MazeType(mazeData, 21, 21), 
	                                         0, 1,
											 completed);
	PrintMaze(*startBranch->Maze);

	FiberTaskingLib::Task task = {CheckBranch, startBranch};
	globalArgs->TaskScheduler.AddTask(task);

	globalArgs->TaskScheduler.WaitForCounter(std::shared_ptr<FiberTaskingLib::AtomicCounter>(completed), 1);

	PrintMaze(*startBranch->Maze);

	// Cleanup
	globalArgs->TaskScheduler.Quit();
	globalArgs->Allocator.destroy();
	delete globalArgs;
}