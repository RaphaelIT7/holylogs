#include "logsystem.h"
#include "filesystem.h"
#include "shared_mutex"
#include "util.h"
#include "algorithm"
#include "chrono"
#include "cstring"
#include "climits"
#include "mutex"
#include <filesystem>

typedef unsigned short EntrySize;

static constexpr double MAX_INDEX_LOADED_TIME = 30.0; // Time in seconds after which a index is unloaded. (based off the last time they were accessed)
static constexpr long long INDEX_LOADED_CHECK_INTERVALS = 1000; // How often we check for indexes to unload (in ms)

// Strings for filesystem stuff
static constexpr const char* pLogDataDir = "logdata/data/";
static constexpr int pLogDataDirLength = 13;
static constexpr const char* pLogIndexesDir = "logdata/indexes/";
static constexpr int pLogIndexesDirLength = 16;
static constexpr const char* pLogExtension = ".dat";
static constexpr int pLogExtensionLength = 4;

static constexpr int ENTRIES_TRIGGER_DELETION = 1 << 14; // This can safely be increased without needing a version change since the nEntriesData is at the end of the LogIndex
static constexpr int ENTRIES_DELETION_CYCLE = 1 << 11; // How many entries are deleted if we ever hit the limit.
// FileSystem::MAX_PATH is set to 256 since most OS filesystems only allow file names/paths up to that length.

// Simplified it by every Index having a single unique name instead of this.
static constexpr int MAX_KEY_SIZE = 48;
static constexpr int MAX_KEY_COUNT = 16; // A soft limit - in theory we could have far more
struct LogKey
{
	char nKeyName[MAX_KEY_SIZE] = {0};
	char nKeyValue[MAX_KEY_SIZE] = {0};
};

// This should NEVER have stuff like std::string, we write this entire sturcture straight to disk!
static constexpr int INDEX_VERSION_1 = 1;
struct LogIndex_V1
{
	unsigned int version = 1; // In case we change any of the structs in the future.
	UniqueFilenameId nFileName; // FileName of the index file containing this LogIndex data.
	char nIndexName[48] = {0}; // Unique name of this index that is given to use to find it.
	unsigned int nEntries = 0;
	unsigned int nTotalSize = 0;
};

static constexpr int INDEX_VERSION_LATEST = 2;
struct LogIndex_V2 // This should NEVER have stuff like std::string, we write this entire sturcture straight to disk!
{
	unsigned int version = 2; // In case we change any of the structs in the future.
	UniqueFilenameId nFileName; // FileName of the index file containing this LogIndex data.
	char nIndexName[48] = {0}; // Unique name of this index that is given to use to find it.
	unsigned int nEntries = 0;
	unsigned int nTotalSize = 0;

	unsigned int nKeys = 0; // The total number of LogKey that refer to this log entry.
};

struct LogIndex : LogIndex_V2 // Implements all functions. Done like this simply to keep it more readable.
{
	LogIndex()
	{
		Util::GenerateUniqueFilename(nFileName);
	}

	void SetIndexName(const std::string& pKeyName)
	{
		if (nIndexName[0] != '\0') // Name is already set.
			return;

		std::strncpy(nIndexName, pKeyName.c_str(), MAX_KEY_SIZE - 1);
		nIndexName[MAX_KEY_SIZE - 1] = '\0';
	}

	LogIndex& operator=(const LogIndex_V1& other)
	{
		memcpy(this, &other, sizeof(LogIndex_V1));
		nKeys = 0;
	}
};

static inline bool LoadLogIndex(LogIndex& pIndex, FileHandle_t& pHandle)
{
	unsigned int indexVersion = -1;
	pHandle.read((char*)&indexVersion, sizeof(indexVersion));
	pHandle.seekg(-sizeof(indexVersion), std::ios_base::cur);

	if (indexVersion == INDEX_VERSION_1)
	{
		LogIndex_V1 pIndexV1;
		pHandle.read((char*)&pIndexV1, sizeof(LogIndex_V1));

		pIndex = pIndexV1;
		return true;
	}

	if (indexVersion == INDEX_VERSION_LATEST)
	{
		pHandle.read((char*)&pIndex, sizeof(LogIndex));
		return true;
	}

	return false;
}

struct Log // This stuct will be in memory, and only the LogIndex is written to disk.
{
public:
	enum FileMode
	{
		NONE = 0,
		READ = 1,
		WRITE = 2,
		APPEND = 3,
		READWRITE = 4,
	};

	Log()
	{
		MarkTouched();
	}

	~Log()
	{
		FileHandle_t pFile = OpenIndexFile();
		if (pFile.is_open())
		{
			pFile.write((char*)this, sizeof(LogIndex));
			pFile.close();
		}

		if (pEntryFile.is_open())
			pEntryFile.close();

		printf("Unloaded Log Index \"%s\" from memory\n", pIndex.nIndexName);
	}

	FileHandle_t OpenIndexFile()
	{
		char nIndexFileName[FileSystem::MAX_PATH];
		std::memcpy(nIndexFileName, pLogIndexesDir, pLogIndexesDirLength);
		int nWritten = Util::WriteUniqueFilenameIntoBuffer(pIndex.nFileName, nIndexFileName + pLogIndexesDirLength, sizeof(nIndexFileName) - pLogIndexesDirLength);
		std::memcpy(nIndexFileName + pLogIndexesDirLength + nWritten, pLogExtension, pLogExtensionLength);
		nIndexFileName[pLogIndexesDirLength + nWritten + pLogExtensionLength] = '\0';

		return FileSystem::OpenWriteFile(nIndexFileName);
	}

	FileHandle_t& OpenDataFile(FileMode pFileMode)
	{
		if (pEntryFile.is_open() && pFileMode != nEntryFileMode)
			pEntryFile.close();

		if (!pEntryFile.is_open())
		{
			char nEntryFileName[FileSystem::MAX_PATH];
			std::memcpy(nEntryFileName, pLogDataDir, pLogDataDirLength);
			int nWritten = Util::WriteUniqueFilenameIntoBuffer(pIndex.nFileName, nEntryFileName + pLogDataDirLength, sizeof(nEntryFileName) - pLogDataDirLength);
			std::memcpy(nEntryFileName + pLogDataDirLength + nWritten, pLogExtension, pLogExtensionLength);
			nEntryFileName[pLogDataDirLength + nWritten + pLogExtensionLength] = '\0';

			nEntryFileMode = pFileMode;
			if (nEntryFileMode == FileMode::APPEND) {
				pEntryFile = FileSystem::OpenAppendFile(nEntryFileName);
			} else if (nEntryFileMode == FileMode::READ) {
				pEntryFile = FileSystem::OpenReadFile(nEntryFileName);
			} else if (nEntryFileMode == FileMode::WRITE) {
				pEntryFile = FileSystem::OpenWriteFile(nEntryFileName);
			} else {
				pEntryFile = FileSystem::OpenFile(nEntryFileName);
			}
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
	
		FileHandle_t& pFile = OpenDataFile(FileMode::APPEND);
		if (!pFile.is_open())
			return;

		pFile.seekp(pIndex.nTotalSize); // We do this before we set the nSize since else it would also include the file were actively writing!

		EntrySize nSize = std::min(USHRT_MAX, (int)pEntryData.length()); // Limit to USHRT_MAX
		++pIndex.nEntries;
		pIndex.nTotalSize += nSize;
		pIndex.nTotalSize += sizeof(nSize);

		pFile.write((char*)&nSize, sizeof(nSize));
		pFile.write(pEntryData.c_str(), nSize);

		printf("Wrote a new Log Entry into \"%s\" (Size: %i)\n", pIndex.nIndexName, nSize);

		pFile.flush();
	}

	bool ShouldUnload(std::chrono::system_clock::time_point pTimePoint)
	{
		return std::chrono::duration<double>(pTimePoint - nLastTouched).count() > MAX_INDEX_LOADED_TIME;
	}

	void SetIndexName(const std::string& pKeyName)
	{
		pIndex.SetIndexName(pKeyName);
		pIndexHash = std::hash<std::string>{}(pKeyName);
	}

	void SetIndexHash(const std::size_t pHash)
	{
		pIndexHash = pHash;
	}

private:
	// This is VERY expensive!!!
	// Why is it expensive?
	// We restructure the entire LogIndex shifitng all entires and we write the entire data file again to remove old entires and update all offsets.
	void DoEntryDeletionCycle()
	{
		if (pIndex.nEntries <= ENTRIES_DELETION_CYCLE)
			return; // Not enouth entries!

		FileHandle_t& pFile = OpenDataFile(FileMode::READWRITE);
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

		char pBuffer[USHRT_MAX]; // 64kb buffer to use while were swapping things around.
		int i = ENTRIES_DELETION_CYCLE; // Skip these first as the lower their index the older they are.
		int nNewOffset = 0; // Offset for the moved data
		for (; i<pIndex.nEntries; ++i)
		{
			int nNewIndex = i - ENTRIES_DELETION_CYCLE;

			pFile.seekg(nCurrentOffset);

			EntrySize nSize;
			pFile.read((char*)&nSize, sizeof(nSize));
			pFile.read(pBuffer, nSize); // We don't need to check bounds since each entry has a size limit of USHRT_MAX

			pFile.seekp(nNewOffset);
			pFile.write((char*)&nSize, sizeof(nSize));
			pFile.write(pBuffer, nSize);

			// Now update our offsets
			nSize += sizeof(nSize); // increment by this since we also write it into the file.
			nCurrentOffset += nSize;
			nNewOffset += nSize;
		}

		pIndex.nEntries -= ENTRIES_DELETION_CYCLE;

		pIndex.nTotalSize = nNewOffset;

		// We need to close since else FileSystem::TurnaceFile will corrupt the handle and cause a crash
		// UPDATE: I had a crash INSIDE TurnaceFile and not because of this being flush... flush should be fine, right?
		pFile.flush();

		// Yeah, duplicate code... anyways, it helps memory usage.
		char nEntryFileName[FileSystem::MAX_PATH];
		std::memcpy(nEntryFileName, pLogDataDir, pLogDataDirLength);
		int nWritten = Util::WriteUniqueFilenameIntoBuffer(pIndex.nFileName, nEntryFileName + pLogDataDirLength, sizeof(nEntryFileName) - pLogDataDirLength);
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
	std::size_t pIndexHash = 0;

private:
	// We don't close the files instantly to heavily improve performance.
	FileHandle_t pEntryFile;
	FileMode nEntryFileMode = FileMode::NONE; // -1 = Read, 0 = None, 1 = Write

	// Last time we touched this Log entry.
	std::chrono::system_clock::time_point nLastTouched;
};

static std::vector<std::unique_ptr<Log>> g_pLogIndexes = {};
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
			for (auto& pLog : g_pLogIndexes)
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
				if (!it->get()->ShouldUnload(pCurrentTime))
				{
					it++;
					continue;
				}

				// Deleting the logs in here would block everything!
				// So we move it outside of our lock to unblock g_LogIndexesMutex
				pLogsToDelete.push_back(it->get());
				it->release();

				it = g_pLogIndexes.erase(it);
			}
		}

		for (Log* pLog : pLogsToDelete)
			delete pLog;

		{
			std::unique_lock<std::shared_mutex> writeLock(g_LogIndexesMutex);
			extern void CheckLogState();
			CheckLogState();
		}
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

struct LogState
{
	LogState()
	{
		CheckState();
	}

	Log* FindLog(const std::string entryKey, bool bSecondCall = false)
	{
		UniqueFilenameId pIndexID;
		std::size_t pKeyHash = std::hash<std::string>{}(entryKey);
		if (!GetIndex(pKeyHash, pIndexID))
		{
			printf("Failed to find Log index \"%s\" in state\n", entryKey.c_str());
			return nullptr;
		}

		char nIndexFileName[FileSystem::MAX_PATH];
		std::memcpy(nIndexFileName, pLogIndexesDir, pLogIndexesDirLength);
		int nWritten = Util::WriteUniqueFilenameIntoBuffer(pIndexID, nIndexFileName + pLogIndexesDirLength, sizeof(nIndexFileName) - pLogIndexesDirLength);
		std::memcpy(nIndexFileName + pLogIndexesDirLength + nWritten, pLogExtension, pLogExtensionLength);
		nIndexFileName[pLogIndexesDirLength + nWritten + pLogExtensionLength] = '\0';

		FileHandle_t pFile = FileSystem::OpenReadFile(nIndexFileName);
		if (!pFile.is_open())
		{
			printf("Failed to open Log index \"%s\" from state!\n", entryKey.c_str());
			if (bSecondCall)
				return nullptr;

			RebuildState();
			return FindLog(entryKey, true); // Recursion... Yay
		}

		Log* pLog = new Log();
		pFile.read((char*)&pLog->pIndex, sizeof(LogIndex));
		pFile.close();
		pLog->SetIndexHash(pKeyHash);

		return pLog;
	}

	bool GetIndex(const std::size_t pKeyHash, UniqueFilenameId& pIndexID)
	{
		std::shared_lock<std::shared_mutex> readLock(pMutex);
		if (!EnsureOpen())
			return false;

		pReadStateFile.seekg(0);
		while (true)
		{
			std::size_t pHash;
			pReadStateFile.read((char*)&pHash, sizeof(pHash));
			if (pReadStateFile.gcount() != sizeof(pHash))
				break;

			if (pHash == pKeyHash)
			{
				pReadStateFile.read((char*)&pIndexID, sizeof(pIndexID));
				if (pReadStateFile.gcount() != sizeof(pIndexID))
					break;

				pReadStateFile.close();
				return true;
			}

			pReadStateFile.seekg(sizeof(UniqueFilenameId), std::ios::cur);
		}
		pReadStateFile.close();

		return false;
	}

	void AddEntryToList(const std::string& pIndexName, const UniqueFilenameId& pIndexFileID)
	{
		// I bet Append is causing stuff to be corrupted, like this is the only file where we use append and its the only file where corruption happens
		// ToDo: Fix it, though for now this should still work.
		RebuildState();
		/*std::unique_lock<std::shared_mutex> writeLock(pMutex);
		if (pReadStateFile.is_open())
			pReadStateFile.close();

		FileHandle_t pHandle = FileSystem::OpenAppendFile("logdata/state.dat");
		if (pHandle.is_open())
		{
			pHandle.seekp(0, std::ios::end);

			std::size_t pHash = std::hash<std::string>{}(pIndexName);

			pHandle.write((char*)&pHash, sizeof(pHash));
			pHandle.write((char*)&pIndexFileID, sizeof(pIndexFileID));

			pHandle.close();
		}*/
	}

private:
	FileHandle_t pReadStateFile;
	std::shared_mutex pMutex;

	bool EnsureOpen()
	{
		if (pReadStateFile.is_open())
			return true;

		pReadStateFile = FileSystem::OpenReadFile("logdata/state.dat");
		return pReadStateFile.is_open();
	}

	/*
		In a current unknown race condition our state file can become corrupted.
		If that happens, the hashes and filenameIDs won't match causing possible issues.
	*/
	void RebuildState()
	{
		std::unique_lock<std::shared_mutex> writeLock(pMutex);
		printf("Rebuilding Log state...\n");

		pReadStateFile.close();
		FileHandle_t pHandle = FileSystem::OpenWriteFile("logdata/state.dat");
		if (!pHandle.is_open())
		{
			printf("Failed to rebuild log state! It's already open!\n");
			return;
		}

		pHandle.seekp(0);
		for(auto& pFile : std::filesystem::recursive_directory_iterator(pLogIndexesDir))
		{
			if (!pFile.is_regular_file())
				continue;

			FileHandle_t pIndexHandle = FileSystem::OpenReadFile(pFile.path().string());
			if (!pIndexHandle.is_open())
			{
				printf("Failed to open index \"%s\"\n", pFile.path().string().c_str());
				continue;
			}

			LogIndex pLogIndex;
			if (!LoadLogIndex(pLogIndex, pIndexHandle))
			{
				printf("Failed to read index \"%s\"\n", pFile.path().string().c_str());
				continue;
			}

			std::string pFileName = pFile.path().string().substr(pLogIndexesDirLength);
			pFileName.erase(pFileName.size() - pLogExtensionLength); // Nuke .dat

			UniqueFilenameId pIndexFileID;
			Util::ReadUniqueFilenameFromBuffer(pFileName.c_str(), pIndexFileID);

			for (size_t idx = 0; idx < pLogIndex.nKeys; ++idx)
			{
				std::size_t pHash = std::hash<std::string>{}(pLogIndex.nKeysData[idx].nKeyValue);
				pHandle.write((char*)&pHash, sizeof(pHash));
				pHandle.write((char*)&pIndexFileID, sizeof(UniqueFilenameId));
			}
		}
		pHandle.close();
	}

public:
	void CheckState()
	{
		if (!EnsureOpen())
			return;

		RebuildState();

		/*printf("Checking Log state...\n");
		bool bBroken = false;
		int pIndexes = 0;
		for(auto& pFile : std::filesystem::recursive_directory_iterator(pLogIndexesDir))
			++pIndexes;

		{
			std::shared_lock<std::shared_mutex> readLock(pMutex);
			pReadStateFile.seekg(0);
			int nFoundIndexes = 0;
			while (true)
			{
				std::size_t pHash;
				pReadStateFile.read((char*)&pHash, sizeof(pHash));
				if (pReadStateFile.gcount() != sizeof(pHash))
					break;

				UniqueFilenameId pIndexID;
				pReadStateFile.read((char*)&pIndexID, sizeof(pIndexID));
				// printf("Found hash %u\n", pHash);
				char nIndexFileName[FileSystem::MAX_PATH];
				std::memcpy(nIndexFileName, pLogIndexesDir, pLogIndexesDirLength);
				int nWritten = Util::WriteUniqueFilenameIntoBuffer(pIndexID, nIndexFileName + pLogIndexesDirLength, sizeof(nIndexFileName) - pLogIndexesDirLength);
				std::memcpy(nIndexFileName + pLogIndexesDirLength + nWritten, pLogExtension, pLogExtensionLength);
				nIndexFileName[pLogIndexesDirLength + nWritten + pLogExtensionLength] = '\0';

				FileHandle_t pFile = FileSystem::OpenReadFile(nIndexFileName);
				if (!pFile.is_open())
				{
					printf("Failed to open Log index \"%u\" - %llu %u %u from state!\n", pHash, pIndexID.timestamp, pIndexID.threadHash, pIndexID.randomSuffix);
					bBroken = true;
					break;
				}
				pFile.close();
				++nFoundIndexes;
			}

			if (!bBroken && pIndexes != nFoundIndexes)
				bBroken = true;
		}

		if (bBroken)
			RebuildState();
		else
			printf("Log state is fine :3\n");*/
	}
};

static LogState g_pLogState;
void CheckLogState()
{
	g_pLogState.CheckState();
}

static Log* FindOrCreateLogIndex(const std::string& entryKey, bool bCreate = true)
{
	std::string pKey = entryKey.substr(0, MAX_KEY_SIZE - 1); // Limit it to MAX_KEY_SIZE
	std::size_t pKeyHash = std::hash<std::string>{}(pKey);
	{
		std::shared_lock<std::shared_mutex> readLock(g_LogIndexesMutex);
		for (auto& pLog : g_pLogIndexes)
		{
			if (pLog.get()->pIndexHash == pKeyHash)
				return pLog.get();
		}

		Log* pLog = g_pLogState.FindLog(pKey);
		if (pLog)
		{
			printf("Loaded Log Index \"%s\" from state\n", pLog->pIndex.nIndexName);
			g_pLogIndexes.push_back(std::unique_ptr<Log>(pLog));
			return pLog;
		}
	}

	if (!bCreate)
		return nullptr;

	std::unique_lock<std::shared_mutex> writeLock(g_LogIndexesMutex);

	Log* pLog = new Log();
	pLog->SetIndexName(pKey);
	g_pLogIndexes.push_back(std::unique_ptr<Log>(pLog));
	g_pLogState.AddEntryToList(pKey, pLog->pIndex.nFileName); // Save it into our state for disk based lookups
	printf("Created a new Log Index \"%s\"\n", pKey.c_str());

	return pLog;
}

// The entryData is limited to around 32kb by the httpserver payload limit inside of HttpServer::Start
bool LogSystem::AddEntry(const std::string& entryKey, const std::string& entryData)
{
	Log* pLog = FindOrCreateLogIndex(entryKey);

	pLog->AddEntry(entryData);

	return true;
}

void LogSystem::GetEntries(const std::string& entryKey, std::string& pOutput)
{
	Log* pLog = FindOrCreateLogIndex(entryKey, false);
	if (!pLog)
	{
		pOutput = "";
		return;
	}

	std::unique_lock<std::mutex> writeLock(pLog->pMutex); // Lock it just in case any writes try to come in.
	FileHandle_t& pFile = pLog->OpenDataFile(Log::FileMode::READ);
	if (!pFile.is_open())
	{
		pOutput = "";
		return;
	}

	pFile.seekg(0);

	// Just to be safe we add 8 more bytes per nEntries
	pOutput.resize(2 + pLog->pIndex.nTotalSize + (8 * pLog->pIndex.nEntries));

	std::size_t nPos = 0;
	for (size_t i=0; i<pLog->pIndex.nEntries; ++i)
	{
		EntrySize nSize;
		pFile.read((char*)&nSize, sizeof(nSize));

		char pFileBuffer[USHRT_MAX];
		pFile.read(pFileBuffer, nSize);

		std::string strNumber = std::to_string(nSize);
		memcpy(&pOutput[nPos], strNumber.c_str(), strNumber.size());
		nPos += strNumber.size();
		pOutput[nPos++] = '\0';

		memcpy(&pOutput[nPos], pFileBuffer, nSize);
		nPos += nSize;
		pOutput[nPos++] = '\0';
	}

	pOutput.resize(nPos);

	pFile.close();
}