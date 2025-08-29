#include "string"

namespace CommandLine
{
	// Initializes the command line and fills a internal map with all arguments after which all other CommandLine functions will be usable.
	// NOTE: Command line arguments with no value like -debug -test and such, will have a default value of "1"
	extern void Init(int argNum, char* args[]);

	// Debug level set by the command line -debug option.
	extern int DebugLevel();
	extern bool HasParam(const char* pParam);

	extern std::string GetParamString(const char* pParam, std::string pFallback = "");
	extern const char* GetParamChar(const char* pParam, const char* pFallback = nullptr);
	extern int GetParamInt(const char* pParam, int nFallback = 0);
}