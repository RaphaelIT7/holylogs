#include "httpserver.h"
#include "logsystem.h"
#include "util.h"

class GetEntries : HttpRoute
{
public:
	virtual const char* GetName() { return "GetEntries"; };
	virtual void Setup(httplib::Server& pServer)
	{
		pServer.Get("/GetEntries", [&](const httplib::Request& req, httplib::Response& res)
		{
			std::string entryIndex = req.get_header_value("entryIndex");
			if (entryIndex.empty())
			{
				res.status = 400;
				return;
			}

			LogSystem::GetEntries(entryIndex, res.body);
			res.set_header("Content-Type", "text/plain");
			res.status = 200;
		});
	}
};
static GetEntries pGetEntries;