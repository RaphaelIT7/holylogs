#include "string"
#include <fstream>
#undef MAX_PATH // Why tf does fstream have MAX_PATH xD Gotta change it below then later.

typedef std::fstream FileHandle_t;
namespace FileSystem
{
	constexpr int MAX_PATH = 260;

	extern FileHandle_t OpenReadFile(const std::string& pFileName);
	extern FileHandle_t OpenWriteFile(const std::string& pFileName);
	extern FileHandle_t OpenFile(const std::string& pFileName);

	extern bool FileExists(const char* pFileName);
	extern bool FileExists(const std::string& pFileName);

	extern void CloseFile(FileHandle_t);

	extern void TurnaceFile(const std::string& pFileName, unsigned int fileSize);

	extern void CreateDirectory(const char* pFolderName);
}