#include "httpserver.h"
#include "commandline.h"
#include "logsystem.h"
#include <thread>

void testfunc(int index)
{
	std::this_thread::sleep_for(std::chrono::seconds(1));
	std::string strIndex = "SexyAssIndexIsHot-";
	strIndex.append(std::to_string(index));
	for (int i=0; i<100000; ++i)
	{
		LogSystem::AddEntry(strIndex, "sexyassbody");
	}
	printf("Done");
}

int main(int c, char* v[])
{
	CommandLine::Init(c, v);
	LogSystem::Init();

	for (int k=0; k<50000; ++k)
	{
		std::string strIndex = "SexyAssIndexIsHot-";
		strIndex.append(std::to_string(k));

		LogSystem::AddEntry(strIndex, "sexyassbody");
	}

	std::vector<std::thread> threads;
	for (int i=0; i<8; ++i)
	{
		threads.emplace_back(testfunc, i);
	}

	while (true) {}

	//if (!HttpServer::Start())
	{
		printf("Failed to start HttpServer!\n");
		return -1;
	}

	return 0;
}