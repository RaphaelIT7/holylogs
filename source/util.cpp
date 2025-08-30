#include "util.h"
#include <string>
#include <chrono>
#include <random>
#include <thread>
#include <sstream>
#include <iomanip>

#include <locale>
#include <codecvt>
#include <string>
#ifndef _WIN32
#include <pthread>
#else
#include <windows.h>
#include <processthreadsapi.h>
#endif

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

std::string Util::GenerateUniqueFilename()
{
	auto now = std::chrono::system_clock::now();
	auto duration = now.time_since_epoch();
	auto micros = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();

	auto tid = std::hash<std::thread::id>{}(std::this_thread::get_id());

	static thread_local std::mt19937 gen(std::random_device{}());
	std::uniform_int_distribution<int> dist(0, 9999);
	int random_suffix = dist(gen);

	std::ostringstream oss;
	oss << std::hex << micros << "_" << tid << "_" << std::setfill('0') << std::setw(4) << random_suffix;

	return oss.str();
}

int Util::GenerateUniqueFilename(char* pBuffer, int nBufferSize)
{
	auto now = std::chrono::system_clock::now();
	auto duration = now.time_since_epoch();
	auto micros = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();

	auto tid = std::hash<std::thread::id>{}(std::this_thread::get_id());

	static thread_local std::mt19937 gen(std::random_device{}());
	std::uniform_int_distribution<int> dist(0, 9999);
	int random_suffix = dist(gen);

	return std::snprintf(pBuffer, nBufferSize, "%llx_%zx_%04d", 
		static_cast<unsigned long long>(micros), 
		static_cast<size_t>(tid), 
		random_suffix
	);
}

void Util::GenerateUniqueFilename(UniqueFilenameId& id)
{
	auto now = std::chrono::system_clock::now();
	auto duration = now.time_since_epoch();
	id.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();

	id.threadHash = static_cast<uint32_t>(
		std::hash<std::thread::id>{}(std::this_thread::get_id())
	);

	static thread_local std::mt19937 gen(std::random_device{}());
	std::uniform_int_distribution<uint16_t> dist(0, 9999);
	id.randomSuffix = dist(gen);
}

int Util::WriteUniqueFilenameIntoBuffer(UniqueFilenameId& nFileID, char* pBuffer, int nBufferSize)
{
	return std::snprintf(pBuffer, nBufferSize, "%llx_%x_%04u",
		static_cast<unsigned long long>(nFileID.timestamp),
		nFileID.threadHash,
		nFileID.randomSuffix
	);
}

void Util::SetThreadName(std::thread& pThread, std::string strThreadName)
{
#ifdef _WIN32
	auto handle = (HANDLE)pThread.native_handle();
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	SetThreadDescription(handle, converter.from_bytes(strThreadName).c_str());
#else
	pthread_setname_np(pThread.native_handle(), strThreadName.c_str());
#endif
}