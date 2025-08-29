#include "util.h"
#include <sstream>
#include "random"

std::unordered_map<std::string, std::string> Util::ReadFakeJson(const std::string& jsonStr)
{
	// Limits since the logging server might be exposed to the public.
	constexpr size_t nMaxKeyValueLength = 64;
	constexpr size_t kMaxInputLength = 512;

	std::unordered_map<std::string, std::string> result;
	if (jsonStr.length() > kMaxInputLength)
        return result;

	if (jsonStr.empty() || jsonStr.front() != '{' || jsonStr.back() != '}')
		return result;

    size_t start = 1;
    size_t end = jsonStr.length() - 1;
	while (start < end)
    {
        size_t colon = jsonStr.find(':', start);
        if (colon == std::string::npos || colon >= end)
            break;

        size_t comma = jsonStr.find(',', colon);
        if (comma == std::string::npos || comma > end)
            comma = end;

        size_t keyStart = start;
        size_t keyEnd = colon;

        if (jsonStr[keyStart] == '\"')
            ++keyStart;

        if (keyEnd > keyStart && jsonStr[keyEnd - 1] == '\"')
            --keyEnd;

        size_t keyLen = keyEnd > keyStart ? keyEnd - keyStart : 0;
        if (keyLen == 0 || keyLen > nMaxKeyValueLength)
        {
            start = comma + 1; // Too long
            continue;
        }

        size_t valStart = colon + 1;
        size_t valEnd = comma;

        if (valStart < valEnd && jsonStr[valStart] == '\"')
            ++valStart;

        if (valEnd > valStart && jsonStr[valEnd - 1] == '\"')
            --valEnd;

        size_t valLen = valEnd > valStart ? valEnd - valStart : 0;
        if (valLen == 0 || valLen > nMaxKeyValueLength)
        {
            start = comma + 1; // Too long again!
            continue;
        }

        std::string key = jsonStr.substr(keyStart, keyLen);
        std::string value = jsonStr.substr(valStart, valLen);

        result[key] = value;
        start = comma + 1;
    }

	return result;
}

uint32_t Util::GetRandomNumber()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distr(0, UINT32_MAX);

    return distr(gen);
}