#include "httpserver.h"
#include "commandline.h"
#include "filesystem.h"

int main(int c, char* v[])
{
	CommandLine::Init(c, v);
	FileSystem::Init();

	if (!HttpServer::Start())
	{
		printf("Failed to start HttpServer!\n");
		return -1;
	}

	return 0;
}