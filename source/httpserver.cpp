#include "httpserver.h"
#include "commandline.h"

static httplib::Server g_pHttpServer;
static std::unordered_set<HttpRoute*> g_pRoutes = {};
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

	for (auto& pRoute : g_pRoutes)
	{
		printf("Setting up route %s\n", pRoute->GetName());
		pRoute->Setup(g_pHttpServer);
	}

	g_pHttpServer.set_payload_max_length(1024 * 32); // 32kb should be more than enouth for a single payload

	printf("Starting HttpServer on \"%s:%i\"\n", strAdress.c_str(), nPort);
	g_pHttpServer.listen(strAdress, nPort);

	return true;
}

void HttpServer::RegisterRoute(HttpRoute* pRoute)
{
	auto it = g_pRoutes.find(pRoute);
	if (it != g_pRoutes.end())
		return;

	g_pRoutes.insert(pRoute);
}

void HttpServer::UnregisterRoute(HttpRoute* pRoute)
{
	auto it = g_pRoutes.find(pRoute);
	if (it == g_pRoutes.end())
		return;

	g_pRoutes.erase(it);
}