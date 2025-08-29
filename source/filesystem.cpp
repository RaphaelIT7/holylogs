#include "filesystem.h"
#include <iostream>
#include <filesystem>

FileHandle_t FileSystem::OpenForWriteFile(const std::string& pFileName)
{
	return nullptr;
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