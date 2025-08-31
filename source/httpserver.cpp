#include "httpserver.h"
#include "commandline.h"

static std::vector<HttpRoute*>& GetRoutes()
{
	static std::vector<HttpRoute*> routes;
	return routes;
}

static httplib::Server g_pHttpServer;
bool HttpServer::Start()
{
	if (!CommandLine::HasParam("-address"))
	{
		printf("Missing \"-address\" command line argument!\n");
		return false;
	}

	if (!CommandLine::HasParam("-port"))
	{
		printf("Missing \"-port\" command line argument!\n");
		return false;
	}

	std::string strAdress = CommandLine::GetParamString("-address");
	int nPort = CommandLine::GetParamInt("-port");

	for (auto& pRoute : GetRoutes())
	{
		printf("Setting up route %s\n", pRoute->GetName());
		pRoute->Setup(g_pHttpServer);
	}

	g_pHttpServer.set_payload_max_length(1024 * 32); // 32kb should be more than enouth for a single payload
	g_pHttpServer.new_task_queue = [] { return new httplib::ThreadPool(4, 4); }; // Don't need many as we really don't expect much traffic

	printf("Starting HttpServer on \"%s:%i\"\n", strAdress.c_str(), nPort);
	g_pHttpServer.listen(strAdress, nPort);

	return true;
}

void HttpServer::RegisterRoute(HttpRoute* pRoute)
{
	GetRoutes().push_back(pRoute);
}

void HttpServer::UnregisterRoute(HttpRoute* pRoute)
{
	for (auto it = GetRoutes().begin(); it != GetRoutes().end(); )
	{
		if ((*it) == pRoute)
		{
			GetRoutes().erase(it);
			return;
		}

		it++;
	}
}