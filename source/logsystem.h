#include "unordered_map"
#include "string"

//#define LOGSYSTEM_MULTIPLE_KEYS
namespace LogSystem
{
	extern void Init();
#ifdef LOGSYSTEM_MULTIPLE_KEYS
	extern bool AddEntry(const std::unordered_map<std::string, std::string>& entryKeys, const std::string& entryData);
#else
	extern bool AddEntry(const std::string& entryKey, const std::string& entryData);
#endif
	extern void GetEntries(const std::string& entryKey, std::string& pOutput);
}