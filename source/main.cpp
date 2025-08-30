#include "httpserver.h"
#include "commandline.h"
#include "logsystem.h"
#include <thread>

void testfunc(int index)
{
	std::this_thread::sleep_for(std::chrono::seconds(1));
	std::string strIndex = "reallyunessesarylongtexttoseehowwelltheindexesworkwiththem-";
	strIndex.append(std::to_string(index));
	for (int i=0; i<100000; ++i)
	{
		LogSystem::AddEntry(strIndex, "reallyunessesaryongentrynamejusttoseehowwellitperformsnwithlongerpiecesofdatawhilealsotryingtoseehowwellthewholeunloadingstuffworksandsoon");
	}
	printf("Done");
}

int main(int c, char* v[])
{
	CommandLine::Init(c, v);
	LogSystem::Init();

	for (int k=0; k<1; ++k)
	{
		std::string strIndex = "reallyunes-";
		strIndex.append(std::to_string(k));

		for (int i=0;i<10;++i)
			LogSystem::AddEntry(strIndex, "reallyunessesaryongentrynamejusttoseehowwellitperformsnwithlongerpiecesofdatawhilealsotryingtoseehowwellthewholeunloadingstuffworksandsoon");
	}

	if (!HttpServer::Start())
	{
		printf("Failed to start HttpServer!\n");
		return -1;
	}

	return 0;
}