#include "logsystem.h"
#include "filesystem.h"
#include <shared_mutex>
#include "util.h"

// We never expect the nFileOffset to exceed 4gb since at best all logs together should be less than 1gb.
struct LogEntry
{
	unsigned int nFileOffset = 0; // Offset inside the data file.
	unsigned int nSize = 0; // Total size of this entry
};


// Simplified it by every Index having a single unique name instead of this.
static constexpr int MAX_KEY_SIZE = 64;
#ifdef LOGSYSTEM_MULTIPLE_KEYS
static constexpr int MAX_KEY_COUNT = 16;
struct LogKey
{
	char nKeyName[MAX_KEY_SIZE] = {0};
	char nKeyValue[MAX_KEY_SIZE] = {0};
};
#endif

static constexpr int MAX_ENTRIES = 4096; // This can safely be increased without needing a version change since the nEntriesData is at the end of the LogIndex
static constexpr int ENTRIES_DELETION_CYCLE = MAX_ENTRIES / 8; // How many entries are deleted if we ever hit the limit.
// FileSystem::MAX_PATH is set to 256 since most OS filesystems only allow file names/paths up to that length.
struct LogIndex
{
	unsigned int version = 1; // In case we change any of the structs in the future.
	char nIndexFileName[FileSystem::MAX_PATH] = {0}; // FileName of the index file containing this LogIndex data.
	char nEntryFileName[FileSystem::MAX_PATH] = {0}; // FileName of the data file containing all our entries.
	char nIndexName[MAX_KEY_SIZE] = {0}; // Unique name of this index that is given to use to find it.
#ifdef LOGSYSTEM_MULTIPLE_KEYS
	unsigned int nKeys = 0; // The total number of keys that refer to this log entry.
	LogKey nKeysData[MAX_KEY_COUNT] = {0}; // Directly after nKeys the keys follow, each one is null terminated.
#endif
	unsigned int nEntries = 0;
	LogEntry nEntriesData[MAX_ENTRIES] = {0}; // Directly after nEntries the entries follow.
};

struct Log // This stuct will be in memory, and only the LogIndex is written to disk.
{
	LogIndex pIndex;
	std::shared_mutex pMutex; // Used to lock this LogIndex while we write/read from it as we cannot guarantee safetry in our setup in any different way.
};

static std::vector<Log> g_pLogIndexes = {};
static std::shared_mutex g_LogIndexesMutex;
void LogSystem::Init()
{
	FileSystem::CreateDirectory("logdata/data");
	FileSystem::CreateDirectory("logdata/indexes");
}

#ifdef LOGSYSTEM_MULTIPLE_KEYS
static Log& FindOrCreateLogIndex(const std::unordered_map<std::string, std::string>& entryKeys)
{
	{
		std::shared_lock<std::shared_mutex> readLock(g_LogIndexesMutex);
		for (Log& pLog : g_pLogIndexes)
		{
			LogIndex& pIndex = pLog.pIndex;
			if (pIndex.nKeys == 0)
				continue; // Useless.

			if (pIndex.nKeys != entryKeys.size())
				continue; // Different entryKeys!

			bool bMatches = true; // All the keys have to match!
			for (int nKey = 0; nKey < pIndex.nKeys; ++nKey)
			{
				const LogKey& pKey = pIndex.nKeysData[nKey];
				auto it = entryKeys.find(pKey.nKeyName);
				if (it == entryKeys.end())
				{
					bMatches = false;
					break; // Key doesn't match. GG
				}

				if (it->second != pKey.nKeyValue)
				{
					bMatches = false;
					break; // Value doesn't match. Another GG
				}
			}

			if (bMatches)
				return pLog;
		}
	}

	std::unique_lock<std::shared_mutex> writeLock(g_LogIndexesMutex);
	return g_pLogIndexes.emplace_back();
}
#else
static Log& FindOrCreateLogIndex(const std::string& entryKey)
{
	{
		std::shared_lock<std::shared_mutex> readLock(g_LogIndexesMutex);
		for (Log& pLog : g_pLogIndexes)
		{
			LogIndex& pIndex = pLog.pIndex;
			if (std::strncmp(pIndex.nIndexName, entryKey.c_str(), sizeof(pIndex.nIndexName)) == 0)
				return pLog;
		}
	}

	std::unique_lock<std::shared_mutex> writeLock(g_LogIndexesMutex);
	return g_pLogIndexes.emplace_back();
}
#endif

#ifdef LOGSYSTEM_MULTIPLE_KEYS
static void FillLogIndexKeys(LogIndex& pIndex, const std::unordered_map<std::string, std::string>& entryKeys)
{
	// If it already has keys, we can skip since for the LogIndex to be picked all keys had to match
	if (pIndex.nKeys > 0)
		return;

	for (auto& [key, val] : entryKeys)
	{
		const LogKey& pKey = pIndex.nKeysData[nKey];
		auto it = entryKeys.find(pKey.nKeyName);
		if (it == entryKeys.end())
			continue; // Key doesn't match. GG

		if (it->second == pKey.nKeyValue)
			return pLog; // We found our index.
	}
}
#else
static void FillLogIndexKeys(LogIndex& pIndex, const std::string& entryKey)
{
	if (pIndex.nIndexName[0] != '\0') // Name is already set.
		return;

	std::strncpy(pIndex.nIndexName, entryKey.c_str(), MAX_KEY_SIZE - 1);
	pIndex.nIndexName[MAX_KEY_SIZE - 1] = '\0';
}
#endif

static std::string CreateNewIndexFile()
{
	
}

static std::string CreateNewDataFile()
{
	// ToDo
}

// This is VERY expensive!!!
// Why is it expensive? We restructure the entire LogIndex shifitng all entires and we write the entire data file again to remove old entires and update all offsets.
static void DoEntryDeletionCycle(LogIndex& pIndex)
{
	std::vector<char*> pEntryData;
	int i = ENTRIES_DELETION_CYCLE; // Skip these first as the lower their index the older they are.
	for (; i<pIndex.nEntries; ++i)
	{
		
	}
}

static void AddNewEntryIntoLog(LogIndex& pIndex, const std::string& entryData)
{
	if (pIndex.nEntries >= MAX_ENTRIES) // Well... Now we got a problem.
	{
		DoEntryDeletionCycle(pIndex);
	}
}

#ifdef LOGSYSTEM_MULTIPLE_KEYS
// Every entryKeys is limited by Util::ReadFakeJson to 64 characters per key or value & in total they can be 512 characters.
// The entryData is limited to around 32kb by the httpserver payload limit inside of HttpServer::Start
bool LogSystem::AddEntry(const std::unordered_map<std::string, std::string>& entryKeys, const std::string& entryData)
{
	Log& pIndex = FindOrCreateLogIndex(entryKeys);

	std::unique_lock<std::shared_mutex> writeLock(pIndex.pMutex);
}
#else
// The entryData is limited to around 32kb by the httpserver payload limit inside of HttpServer::Start
bool LogSystem::AddEntry(const std::string& entryKey, const std::string& entryData)
{
	Log& pLog = FindOrCreateLogIndex(entryKey);

	std::unique_lock<std::shared_mutex> writeLock(pLog.pMutex);
	FillLogIndexKeys(pLog.pIndex, entryKey);
}
#endif