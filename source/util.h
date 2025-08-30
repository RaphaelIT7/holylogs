#include "unordered_map"
#include "string"
#include "thread"

#if _WIN32
__pragma(pack(push, 2))
#else
#pragma pack(push, 2)
#endif
struct UniqueFilenameId
{
	uint64_t timestamp = 0;
	uint32_t threadHash = 0;
	uint16_t randomSuffix = 0;
};
#if _WIN32
__pragma(pack(pop))
#else
#pragma pack(pop)
#endif

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
	extern void GenerateUniqueFilename(UniqueFilenameId& pFilename);
	extern int WriteUniqueFilenameIntoBuffer(const UniqueFilenameId& nFileID, char* pBuffer, int nBufferSize);
	extern void SetThreadName(std::thread& pThread, std::string strThreadName);
}