#include "logsystem.h"
#include "filesystem.h"
#include "shared_mutex"
#include "util.h"
#include "algorithm"

// We never expect the nFileOffset to exceed 4gb since at best all logs together should be less than 1gb.
struct LogEntry
{
	// Actually we don't even need to know the offset since every entry will be placed after each other.
	// Though for fetching results were just gonna keep it for now.
	// unsigned int nFileOffset = 0; // Offset inside the data file.
	unsigned short nSize = 0; // Total size of this entry. At maximum every entry can be 64kb
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
struct LogIndex // This should NEVER have stuff like std::string, we write this entire sturcture straight to disk!
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

	FileHandle_t OpenIndexFile()
	{
		if (nIndexFileName[0] == '\0')
		{
			std::string pFileName = "logdata/indexes/";
			pFileName.append(Util::GenerateUniqueFilename());
			pFileName.append(".dat");

			std::strncpy(nIndexFileName, pFileName.c_str(), sizeof(nIndexFileName) - 1);
			nIndexFileName[sizeof(nIndexFileName) - 1] = '\0';
		}

		return FileSystem::OpenWriteFile(nIndexFileName);
	}

	FileHandle_t OpenDataFile()
	{
		if (nEntryFileName[0] == '\0')
		{
			std::string pFileName = "logdata/data/";
			pFileName.append(Util::GenerateUniqueFilename());
			pFileName.append(".dat");

			std::strncpy(nEntryFileName, pFileName.c_str(), sizeof(nEntryFileName) - 1);
			nEntryFileName[sizeof(nEntryFileName) - 1] = '\0';
		}

		return FileSystem::OpenWriteFile(nEntryFileName);
	}

	int GetTotalEntrySize()
	{
		int nSize = 0;
		for (int i=0; i<nEntries; ++i)
		{
			nSize += nEntriesData[i].nSize;
		}

		return nSize;
	}
};

struct Log // This stuct will be in memory, and only the LogIndex is written to disk.
{
public:
	LogIndex pIndex;
	std::shared_mutex pMutex; // Used to lock this LogIndex while we write/read from it as we cannot guarantee safetry in our setup in any different way.
};

static std::vector<std::unique_ptr<Log>> g_pLogIndexes = {};
static std::shared_mutex g_LogIndexesMutex;
void LogSystem::Init()
{
	FileSystem::CreateDirectory("logdata/data");
	FileSystem::CreateDirectory("logdata/indexes");
}

#ifdef LOGSYSTEM_MULTIPLE_KEYS
static Log* FindOrCreateLogIndex(const std::unordered_map<std::string, std::string>& entryKeys)
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
static Log* FindOrCreateLogIndex(const std::string& entryKey)
{
	{
		std::shared_lock<std::shared_mutex> readLock(g_LogIndexesMutex);
		for (std::unique_ptr<Log>& pLog : g_pLogIndexes)
		{
			LogIndex& pIndex = pLog.get()->pIndex;
			if (std::strncmp(pIndex.nIndexName, entryKey.c_str(), sizeof(pIndex.nIndexName)) == 0)
				return pLog.get();
		}
	}

	std::unique_lock<std::shared_mutex> writeLock(g_LogIndexesMutex);
	g_pLogIndexes.emplace_back(std::make_unique<Log>());
	memset(&g_pLogIndexes.back().get()->pIndex, 0, sizeof(LogIndex)); // Init everything as 0
	return g_pLogIndexes.back().get();
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

// This is VERY expensive!!!
// Why is it expensive? We restructure the entire LogIndex shifitng all entires and we write the entire data file again to remove old entires and update all offsets.
static void DoEntryDeletionCycle(LogIndex& pIndex)
{
	if (pIndex.nEntries <= ENTRIES_DELETION_CYCLE)
		return; // Not enouth entries!

	int nCurrentOffset = 0; // Total size of data that we will remove from the head of the file.
	for (int i=0; i<ENTRIES_DELETION_CYCLE; ++i)
	{
		nCurrentOffset += pIndex.nEntriesData[i].nSize; // Skipping these.
	}

	FileHandle_t pFile = pIndex.OpenDataFile();
	if (!pFile.is_open())
		return; // Failed to open? Ok...

	std::unique_ptr<char[]> pBuffer(new char[USHRT_MAX]); // 64kb buffer to use while were swapping things around.
	int i = ENTRIES_DELETION_CYCLE; // Skip these first as the lower their index the older they are.
	int nNewOffset = 0; // Offset for the moved data
	for (; i<pIndex.nEntries; ++i)
	{
		int nNewIndex = i - ENTRIES_DELETION_CYCLE;
		LogEntry& pEntry = pIndex.nEntriesData[i];

		pFile.seekg(nCurrentOffset);
		pFile.read(pBuffer.get(), pEntry.nSize); // We don't need to check bounds since each entry has a size limit of USHRT_MAX

		pFile.seekp(nNewOffset);
		pFile.write(pBuffer.get(), pEntry.nSize);

		pIndex.nEntriesData[nNewIndex] = pIndex.nEntriesData[i]; // Gonna move it here already :3
		// pIndex.nEntriesData[nNewIndex].nFileOffset = nNewOffset;

		// Now update our offsets
		nCurrentOffset += pEntry.nSize;
		nNewOffset += pEntry.nSize;
	}

	pIndex.nEntries -= ENTRIES_DELETION_CYCLE;

	pFile.close();

	FileSystem::TurnaceFile(pIndex.nEntryFileName, nNewOffset);
}

static void AddNewEntryIntoLog(LogIndex& pIndex, const std::string& entryData)
{
	if (pIndex.nEntries >= MAX_ENTRIES) // Well... Now we got a problem.
	{
		DoEntryDeletionCycle(pIndex);
	}

	int nLogIndex = pIndex.nEntries; // Will always be 1 above the last since Array's start at 0... (I messed this up already once...)
	if (nLogIndex >= MAX_ENTRIES)
		nLogIndex = MAX_ENTRIES-1; // Should never happen but it threw me a warning anyways!

	LogEntry& pEntry = pIndex.nEntriesData[nLogIndex];
	
	FileHandle_t pFile = pIndex.OpenDataFile();
	if (!pFile.is_open())
		return;

	pFile.seekp(pIndex.GetTotalEntrySize()); // We do this before we set the nSize since else it would also include the file were actively writing!

	pEntry.nSize = std::min(USHRT_MAX, (int)entryData.length()); // Limit to USHRT_MAX
	++pIndex.nEntries;

	pFile.write(entryData.c_str(), pEntry.nSize);

	pFile.close();
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
	Log* pLog = FindOrCreateLogIndex(entryKey);

	std::unique_lock<std::shared_mutex> writeLock(pLog->pMutex);
	FillLogIndexKeys(pLog->pIndex, entryKey);
	AddNewEntryIntoLog(pLog->pIndex, entryData);

	return true;
}
#endif