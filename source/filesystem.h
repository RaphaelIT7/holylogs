#include "string"
#include <fstream>
#undef MAX_PATH // Why tf does fstream have MAX_PATH xD Gotta change it below then later.

typedef std::fstream FileHandle_t;
namespace FileSystem
{
	constexpr int MAX_PATH = 64; // 260; // NOTE: We don't use any long or absolute file paths so 48 should be more than enouth.

	extern FileHandle_t OpenReadFile(const char* pFileName);
	extern FileHandle_t OpenWriteFile(const char* pFileName);
	extern FileHandle_t OpenAppendFile(const char* pFileName);
	extern FileHandle_t OpenFile(const char* pFileName);

	extern FileHandle_t OpenReadFile(const std::string& pFileName);
	extern FileHandle_t OpenWriteFile(const std::string& pFileName);
	extern FileHandle_t OpenAppendFile(const std::string& pFileName);
	extern FileHandle_t OpenFile(const std::string& pFileName);

	extern bool FileExists(const char* pFileName);
	extern bool FileExists(const std::string& pFileName);

	extern void CloseFile(FileHandle_t);

	extern void TurnaceFile(const std::string& pFileName, unsigned int fileSize);

	extern void CreateDirectory(const char* pFolderName);
}