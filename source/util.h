#include "unordered_map"
#include "string"

namespace Util
{
	// Reads a simple Json key-value pair that are both strings. (We don't actually use any real json library)
	extern std::unordered_map<std::string, std::string> ReadFakeJson(const std::string& pJson);
	extern uint32_t GetRandomNumber();
	extern std::string GenerateUniqueFilename();
	// Unlike the other one we write this directly into the buffer!
	// Always writes 28 bytes so ensure it's big enouth!
	// Returns the number of bytes written
	extern int GenerateUniqueFilename(char* pBuffer, int nBufferSize);
}