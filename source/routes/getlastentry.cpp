#include "httpserver.h"
#include "logsystem.h"
#include "util.h"

class GetLastEntry : HttpRoute
{
public:
	virtual const char* GetName() { return "GetLastEntry"; };
	virtual void Setup(httplib::Server& pServer)
	{
		pServer.Get("/GetLastEntry", [&](const httplib::Request& req, httplib::Response& res)
		{
			std::string entryIndex = req.get_header_value("entryIndex");
			if (entryIndex.empty())
			{
				res.status = 400;
				return;
			}

			LogSystem::GetLastEntry(entryIndex, res.body);
			res.set_header("Content-Type", "text/plain");
			res.status = 200;
		});
	}
};
static GetLastEntry pGetLastEntry;