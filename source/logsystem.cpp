#include "logsystem.h"
#include "filesystem.h"
#include "shared_mutex"
#include "util.h"
#include "algorithm"
#include <chrono>

typedef unsigned short EntrySize;

static constexpr double MAX_INDEX_LOADED_TIME = 30.0; // Time in seconds after which a index is unloaded. (based off the last time they were accessed)
static constexpr long long INDEX_LOADED_CHECK_INTERVALS = 1000; // How often we check for indexes to unload (in ms)

// Simplified it by every Index having a single unique name instead of this.
static constexpr int MAX_KEY_SIZE = 48;
#ifdef LOGSYSTEM_MULTIPLE_KEYS
static constexpr int MAX_KEY_COUNT = 16;
struct LogKey
{
	char nKeyName[MAX_KEY_SIZE] = {0};
	char nKeyValue[MAX_KEY_SIZE] = {0};
};
#endif

// Strings for filesystem stuff
static constexpr const char* pLogDataDir = "logdata/data/";
static constexpr int pLogDataDirLength = 13;
static constexpr const char* pLogIndexesDir = "logdata/indexes/";
static constexpr int pLogIndexesDirLength = 16;
static constexpr const char* pLogExtension = ".dat";
static constexpr int pLogExtensionLength = 4;

static constexpr int ENTRIES_TRIGGER_DELETION = 1 << 16; // This can safely be increased without needing a version change since the nEntriesData is at the end of the LogIndex
static constexpr int ENTRIES_DELETION_CYCLE = 1 << 12; // How many entries are deleted if we ever hit the limit.
// FileSystem::MAX_PATH is set to 256 since most OS filesystems only allow file names/paths up to that length.
struct LogIndex // This should NEVER have stuff like std::string, we write this entire sturcture straight to disk!
{
	unsigned int version = 1; // In case we change any of the structs in the future.
	UniqueFilenameId nIndexFileName; // FileName of the index file containing this LogIndex data.
	UniqueFilenameId nEntryFileName; // FileName of the data file containing all our entries.
	char nIndexName[MAX_KEY_SIZE] = {0}; // Unique name of this index that is given to use to find it.
#ifdef LOGSYSTEM_MULTIPLE_KEYS
	unsigned int nKeys = 0; // The total number of keys that refer to this log entry.
	LogKey nKeysData[MAX_KEY_COUNT] = {0}; // Directly after nKeys the keys follow, each one is null terminated.
#endif
	unsigned int nEntries = 0;

	void SetIndexName(const std::string& pKeyName)
	{
		if (nIndexName[0] != '\0') // Name is already set.
			return;

		std::strncpy(nIndexName, pKeyName.c_str(), MAX_KEY_SIZE - 1);
		nIndexName[MAX_KEY_SIZE - 1] = '\0';
	}
};

struct Log // This stuct will be in memory, and only the LogIndex is written to disk.
{
public:
	Log()
	{
		MarkTouched();
	}

	~Log()
	{

		FileHandle_t& pFile = OpenIndexFile();
		if (pFile.is_open())
		{
			pFile.write((char*)this, sizeof(LogIndex));
			pFile.close();
		}

		if (pEntryFile.is_open())
			pEntryFile.close();
	}

	FileHandle_t OpenIndexFile()
	{
		char nIndexFileName[FileSystem::MAX_PATH];
		std::memcpy(nIndexFileName, pLogIndexesDir, pLogIndexesDirLength);
		int nWritten = Util::WriteUniqueFilenameIntoBuffer(pIndex.nEntryFileName, nIndexFileName + pLogIndexesDirLength, sizeof(nIndexFileName) - pLogIndexesDirLength);
		std::memcpy(nIndexFileName + pLogIndexesDirLength + nWritten, pLogExtension, pLogExtensionLength);
		nIndexFileName[pLogIndexesDirLength + nWritten + pLogExtensionLength] = '\0';

		return FileSystem::OpenWriteFile(nIndexFileName);
	}

	FileHandle_t& OpenDataFile()
	{
		if (!pEntryFile.is_open())
		{
			char nEntryFileName[FileSystem::MAX_PATH];
			std::memcpy(nEntryFileName, pLogDataDir, pLogDataDirLength);
			int nWritten = Util::WriteUniqueFilenameIntoBuffer(pIndex.nEntryFileName, nEntryFileName + pLogDataDirLength, sizeof(nEntryFileName) - pLogDataDirLength);
			std::memcpy(nEntryFileName + pLogDataDirLength + nWritten, pLogExtension, pLogExtensionLength);
			nEntryFileName[pLogDataDirLength + nWritten + pLogExtensionLength] = '\0';

			pEntryFile = FileSystem::OpenWriteFile(nEntryFileName);
		}

		return pEntryFile;
	}

	void AddEntry(const std::string& pEntryData)
	{
		// We need to do a full lock since currently we do not support multiple threads writing to the same log entry.
		std::lock_guard<std::mutex> lock(pMutex);

		MarkTouched();

		if (pIndex.nEntries >= ENTRIES_TRIGGER_DELETION) // Well... Now we got a problem.
		{
			DoEntryDeletionCycle();
		}

		int nLogIndex = pIndex.nEntries; // Will always be 1 above the last since Array's start at 0... (I messed this up already once...)
		if (nLogIndex >= ENTRIES_TRIGGER_DELETION)
			nLogIndex = ENTRIES_TRIGGER_DELETION-1; // Should never happen but it threw me a warning anyways!
	
		FileHandle_t& pFile = OpenDataFile();
		if (!pFile.is_open())
			return;

		pFile.seekp(nTotalSize); // We do this before we set the nSize since else it would also include the file were actively writing!

		EntrySize nSize = std::min(USHRT_MAX, (int)pEntryData.length()); // Limit to USHRT_MAX
		++pIndex.nEntries;
		nTotalSize += nSize;
		nTotalSize += sizeof(nSize);

		pFile.write((char*)&nSize, sizeof(nSize));
		pFile.write(pEntryData.c_str(), nSize);

		pFile.flush();
	}

	bool ShouldUnload(std::chrono::system_clock::time_point pTimePoint)
	{
		return std::chrono::duration<double>(pTimePoint - nLastTouched).count() > MAX_INDEX_LOADED_TIME;
	}

private:
	// This is VERY expensive!!!
	// Why is it expensive?
	// We restructure the entire LogIndex shifitng all entires and we write the entire data file again to remove old entires and update all offsets.
	void DoEntryDeletionCycle()
	{
		if (pIndex.nEntries <= ENTRIES_DELETION_CYCLE)
			return; // Not enouth entries!

		FileHandle_t& pFile = OpenDataFile();
		if (!pFile.is_open())
			return; // Failed to open? Ok...

		pFile.seekg(0);
		int nCurrentOffset = 0; // Total size of data that we will remove from the head of the file.
		for (int i=0; i<ENTRIES_DELETION_CYCLE; ++i)
		{
			EntrySize nSize;
			pFile.read((char*)&nSize, sizeof(nSize));
			nCurrentOffset += nSize; // Skipping these.
			nCurrentOffset += sizeof(nSize);
		}

		std::unique_ptr<char[]> pBuffer(new char[USHRT_MAX]); // 64kb buffer to use while were swapping things around.
		int i = ENTRIES_DELETION_CYCLE; // Skip these first as the lower their index the older they are.
		int nNewOffset = 0; // Offset for the moved data
		for (; i<pIndex.nEntries; ++i)
		{
			int nNewIndex = i - ENTRIES_DELETION_CYCLE;

			pFile.seekg(nCurrentOffset);

			EntrySize nSize;
			pFile.read((char*)&nSize, sizeof(nSize));
			pFile.read(pBuffer.get(), nSize); // We don't need to check bounds since each entry has a size limit of USHRT_MAX

			pFile.seekp(nNewOffset);
			pFile.write((char*)&nSize, sizeof(nSize));
			pFile.write(pBuffer.get(), nSize);

			// Now update our offsets
			nSize += sizeof(nSize); // increment by this since we also write it into the file.
			nCurrentOffset += nSize;
			nNewOffset += nSize;
		}

		pIndex.nEntries -= ENTRIES_DELETION_CYCLE;

		nTotalSize = nNewOffset;

		// We need to close since else FileSystem::TurnaceFile will corrupt the handle and cause a crash
		// UPDATE: I had a crash INSIDE TurnaceFile and not because of this being flush... flush should be fine, right?
		pFile.flush();

		// Yeah, duplicate code... anyways, it helps memory usage.
		char nEntryFileName[FileSystem::MAX_PATH];
		std::memcpy(nEntryFileName, pLogDataDir, pLogDataDirLength);
		int nWritten = Util::WriteUniqueFilenameIntoBuffer(pIndex.nEntryFileName, nEntryFileName + pLogDataDirLength, sizeof(nEntryFileName) - pLogDataDirLength);
		std::memcpy(nEntryFileName + pLogDataDirLength + nWritten, pLogExtension, pLogExtensionLength);
		nEntryFileName[pLogDataDirLength + nWritten + pLogExtensionLength] = '\0';

		FileSystem::TurnaceFile(nEntryFileName, nNewOffset);
	}

	// We got touched >:3
	void MarkTouched()
	{
		nLastTouched = std::chrono::system_clock::now();
	}

public:
	LogIndex pIndex;
	std::mutex pMutex; // Used to lock this LogIndex while we write/read from it as we cannot guarantee safetry in our setup in any different way.

private:
	// We don't close the files instantly to heavily improve performance.
	FileHandle_t pEntryFile;

	// Same as pIndex.GetTotalSize() though we keep direct track of it for faster access.
	unsigned int nTotalSize = 0;

	std::chrono::system_clock::time_point nLastTouched;
};

// We use a unordered_map for hashing since its way faster for many indexes
static std::unordered_map<std::string, std::unique_ptr<Log>> g_pLogIndexes = {};
static std::shared_mutex g_LogIndexesMutex;
static void UnloadAnyNonTouchedIndexes()
{
	while (true)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(INDEX_LOADED_CHECK_INTERVALS));

		bool bHasIndexesToDelete = false;
		{
			std::shared_lock<std::shared_mutex> readLock(g_LogIndexesMutex);
			auto pCurrentTime = std::chrono::system_clock::now();
			for (auto& [strLogName, pLog] : g_pLogIndexes)
			{
				if (!pLog.get()->ShouldUnload(pCurrentTime))
					continue;

				bHasIndexesToDelete = true;
				break;
			}
		}

		if (!bHasIndexesToDelete)
			continue;


		std::vector<Log*> pLogsToDelete;
		{
			std::unique_lock<std::shared_mutex> writeLock(g_LogIndexesMutex);
			auto pCurrentTime = std::chrono::system_clock::now();
			for (auto it = g_pLogIndexes.begin(); it != g_pLogIndexes.end(); )
			{
				if (!it->second.get()->ShouldUnload(pCurrentTime))
				{
					it++;
					continue;
				}

				// Deleting the logs in here would block everything!
				// So we move it outside of our lock to unblock g_LogIndexesMutex
				pLogsToDelete.push_back(it->second.get());
				it->second.release();

				it = g_pLogIndexes.erase(it);
			}
			g_pLogIndexes.rehash(g_pLogIndexes.size());
		}

		for (Log* pLog : pLogsToDelete)
			delete pLog;
	}
}

static std::thread g_pLoggingIndexesThread(UnloadAnyNonTouchedIndexes);
void LogSystem::Init()
{
	FileSystem::CreateDirectory("logdata/data");
	FileSystem::CreateDirectory("logdata/indexes");

	Util::SetThreadName(g_pLoggingIndexesThread, "UnloadAnyNonTouchedIndexes");
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
		auto it = g_pLogIndexes.find(entryKey);
		if (it != g_pLogIndexes.end())
			return it->second.get();
	}

	std::string pKey = entryKey.substr(0, MAX_KEY_SIZE - 1); // Limit it to MAX_KEY_SIZE
	std::unique_lock<std::shared_mutex> writeLock(g_LogIndexesMutex);
	Log* pLog = new Log();
	pLog->pIndex.SetIndexName(pKey);
	g_pLogIndexes[pKey] = std::unique_ptr<Log>(pLog);
	return pLog;
}
#endif

#ifdef LOGSYSTEM_MULTIPLE_KEYS
// Every entryKeys is limited by Util::ReadFakeJson to 64 characters per key or value & in total they can be 512 characters.
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
#endif

// The entryData is limited to around 32kb by the httpserver payload limit inside of HttpServer::Start
bool LogSystem::AddEntry(const std::string& entryKey, const std::string& entryData)
{
	Log* pLog = FindOrCreateLogIndex(entryKey);

	pLog->AddEntry(entryData);

	return true;
}