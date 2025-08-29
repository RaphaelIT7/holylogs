#include "string"

struct FileHandle_t
{
	void* pHandle;
	std::string pFileName;
};

namespace FileSystem
{
	constexpr int MAX_PATH = 256;
	extern FileHandle_t OpenForReadFile(const char* pFileName);
	extern FileHandle_t OpenForReadFile(const std::string& pFileName);

	extern FileHandle_t OpenForWriteFile(const char* pFileName);
	extern FileHandle_t OpenForWriteFile(const std::string& pFileName);

	extern bool FileExists(const char* pFileName);
	extern bool FileExists(const std::string& pFileName);

	extern void CloseFile(FileHandle_t);

	extern void CreateDirectory(const char* pFolderName);
}