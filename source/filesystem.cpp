#include "filesystem.h"
#include <iostream>
#include <filesystem>
#include <io.h>

// ToDo: Go lower and use stuff like CreateFile from windows to hopefully make these faster.
FileHandle_t FileSystem::OpenReadFile(const char* pFileName)
{
	return std::fstream(pFileName, std::ios::in | std::ios::binary);
}

FileHandle_t FileSystem::OpenWriteFile(const char* pFileName)
{
	return std::fstream(pFileName, std::ios::out | std::ios::binary);
}

FileHandle_t FileSystem::OpenFile(const char* pFileName)
{
	return std::fstream(pFileName, std::ios::in | std::ios::out | std::ios::binary);
}

FileHandle_t FileSystem::OpenReadFile(const std::string& pFileName)
{
	return std::fstream(pFileName, std::ios::in | std::ios::binary);
}

FileHandle_t FileSystem::OpenWriteFile(const std::string& pFileName)
{
	return std::fstream(pFileName, std::ios::out | std::ios::binary);
}

FileHandle_t FileSystem::OpenFile(const std::string& pFileName)
{
	return std::fstream(pFileName, std::ios::in | std::ios::out | std::ios::binary);
}

void FileSystem::CloseFile(FileHandle_t pFileHandle)
{

}

bool FileSystem::FileExists(const std::string& pFolderName)
{
	return std::filesystem::exists(pFolderName);
}

void FileSystem::CreateDirectory(const char* pFolderName)
{
	std::filesystem::create_directories(pFolderName);
}

void FileSystem::TurnaceFile(const std::string& pFileName, unsigned int fileSize)
{
#if defined(_WIN32)
	FILE* pFileHandle = fopen(pFileName.c_str(), "rb+");
	if (!pFileHandle)
		return;

	int pFileDescriptor = _fileno(pFileHandle);
	if (pFileDescriptor != -1)
		_chsize_s(pFileDescriptor, fileSize);

	fclose(pFileHandle);
#else
    truncate(pFileName.c_str(), fileSize);
#endif
}