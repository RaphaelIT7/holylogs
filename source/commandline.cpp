#include "commandline.h"
#include "unordered_map"

static int g_nDebugLevel = 0;
static std::unordered_map<std::string, std::string> g_pArguments = {};
void CommandLine::Init(int argNum, char* args[])
{
	for (int i = 0; i < argNum; ++i)
	{
		if (args[i][0] == '-')
		{
			std::string strArg = args[i];
			if (g_pArguments.find(strArg) != g_pArguments.end())
			{
				printf("The argument \"%s\" was added multiple times! Skipping...\n", strArg.c_str());
				continue; // Skipping
			}

			if ((i + 1) >= argNum || args[i + 1][0] == '-') // No more following arguments? or the next one is a new parameter? Ok
			{
				g_pArguments[strArg] = "1";
			} else {
				std::string strArgValue = args[++i];
				g_pArguments[strArg] = strArgValue;
			}
		}
	}

	g_nDebugLevel = CommandLine::GetParamInt("-debug");
}

int CommandLine::DebugLevel()
{
	return g_nDebugLevel;
}

bool CommandLine::HasParam(const char* pArgName)
{
	return g_pArguments.find(pArgName) != g_pArguments.end();
}

std::string CommandLine::GetParamString(const char* pArgName, std::string pFallback)
{
	auto it = g_pArguments.find(pArgName);
	if (it == g_pArguments.end())
		return pFallback;

	return it->second.c_str();
}

const char* CommandLine::GetParamChar(const char* pArgName, const char* pFallback)
{
	auto it = g_pArguments.find(pArgName);
	if (it == g_pArguments.end())
		return pFallback;

	return it->second.c_str();
}

int CommandLine::GetParamInt(const char* pArgName, int nFallback)
{
	auto it = g_pArguments.find(pArgName);
	if (it == g_pArguments.end())
		return nFallback;

	return std::stoi(it->second);
}