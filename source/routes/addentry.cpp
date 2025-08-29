#include "httpserver.h"
#include "logsystem.h"
#include "util.h"

class AddEntry : HttpRoute
{
public:
	virtual const char* GetName() { return "AddEntry"; };
	virtual void Setup(httplib::Server& pServer)
	{
		pServer.Post("/AddEntry", [&](const httplib::Request& req, httplib::Response& res)
		{
#ifdef LOGSYSTEM_MULTIPLE_KEYS
			std::string entryNameJson = req.get_param_value("entryKeys");
			if (entryNameJson.empty())
			{
				res.status = 400;
				return;
			}

			auto& entryKeys = Util::ReadFakeJson(entryNameJson);
			LogSystem::AddEntry(entryKeys, req.body);
#else
			std::string entryIndex = req.get_param_value("entryIndex");
			if (entryIndex.empty())
			{
				res.status = 400;
				return;
			}

			LogSystem::AddEntry(entryIndex, req.body);
#endif

			res.status = 200;
		});
	}
};
static AddEntry pAddEntry;