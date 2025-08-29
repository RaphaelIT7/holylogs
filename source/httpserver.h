#include "httplib.h"

class HttpRoute;
namespace HttpServer
{
	// Returns false if it failed to start.
	// NOTE: Currently this will block the main thread!
	extern bool Start();

	// Registers the given Route
	// NOTE: Since it's called so early where things just get constructed you CANNOT call/access any data from the Route in here!
	extern void RegisterRoute(HttpRoute* pRoute);

	// Unregisters the given Route
	extern void UnregisterRoute(HttpRoute* pRoute);
}

class HttpRoute
{
public:
	HttpRoute()
	{
		HttpServer::RegisterRoute(this);
	}

	~HttpRoute()
	{
		HttpServer::UnregisterRoute(this);
	}

	virtual const char* GetName() = 0;
	virtual void Setup(httplib::Server& pServer) = 0;
};