#include "unordered_map"
#include "string"

namespace Util
{
	// Reads a simple Json key-value pair that are both strings. (We don't actually use any real json library)
	extern std::unordered_map<std::string, std::string> ReadFakeJson(const std::string& pJson);
	extern uint32_t GetRandomNumber();
}