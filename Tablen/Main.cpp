#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include "dirent.h"
#include <fstream>

struct AppSettings
{
  std::string TargetDirectory;
  std::vector<std::string> IgnoredDirectories;
  std::vector<std::string> Extensions = {".h", ".c", ".hpp", ".cpp"};
  int MaxCharactersPerLine = 120;
};

struct Range
{
  int Start;
  int Stop;
};

bool GetOptionArgument(int argc, char* argv[], int& LoopControlVariable, std::vector<std::string>& SupportedOptions, std::string& OptionArgument);
bool ParseCommandLineArguments(int argc, char* argv[], AppSettings& OutSettings);
void GetFilePathListFromDirectory(const std::string& DirectoryPath, AppSettings& Settings, std::vector<std::string>& OutFiles);
bool HasValidExtension(std::string& FilePath, const AppSettings& Settings);
void ProcessFile(const std::string& FilePath, const AppSettings& Settings, std::vector<std::string>& ErrorList);

int main(int argc, char *argv[])
{
  AppSettings Settings;

  if (ParseCommandLineArguments(argc, argv, Settings))
  {
    std::vector<std::string> FileList;
    std::vector<std::string> ErrorList;
    GetFilePathListFromDirectory(Settings.TargetDirectory, Settings, FileList);

    for (auto FilePath : FileList)
    {
      ProcessFile(FilePath, Settings, ErrorList);
    }

    for (auto Error : ErrorList)
    {
      std::cerr << Error << std::endl;
    }

    if (ErrorList.size() > 0 || FileList.size() > 0)
    {
      return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
  }

  printf("Not enough arguments. use \"--help\" or \"-h\" for more information.");
  return EXIT_FAILURE;
}

bool GetOptionArgument(int argc, char* argv[], int& LoopControlVariable, std::vector<std::string>& SupportedOptions, std::string& OptionArgument)
{
  OptionArgument = "";
  LoopControlVariable++;

  // If the expected argument is inside of the list.
  if (LoopControlVariable < argc)
  {
    OptionArgument = argv[LoopControlVariable];

    // And it isn't another command line option.
    if (auto Iter = std::find(SupportedOptions.begin(), SupportedOptions.end(), OptionArgument) == SupportedOptions.end())
    {
      return true;
    }
  }

  // Throw that we are missing an argument for option.
  LoopControlVariable--;
  return false;
}

bool ParseCommandLineArguments(int argc, char* argv[], AppSettings& OutSettings)
{
  std::vector<std::string> CommandLineOptions;
  CommandLineOptions.push_back("-h");
  CommandLineOptions.push_back("--help");
  CommandLineOptions.push_back("-p");
  CommandLineOptions.push_back("--path");
  CommandLineOptions.push_back("-e");
  CommandLineOptions.push_back("--extensions");
  CommandLineOptions.push_back("-l");
  CommandLineOptions.push_back("--line_size");
  CommandLineOptions.push_back("-i");
  CommandLineOptions.push_back("--ignore");

  if (argc > 1)
  {
    for (int i = 1; i < argc; ++i)
    {
      std::string Arg = argv[i];

      if (Arg == "-h" || Arg == "--help")
      {
        return false;
      }
      else if (Arg == "-p" || Arg == "--path")
      {
        std::string OptionArgument;
        if (GetOptionArgument(argc, argv, i, CommandLineOptions, OptionArgument))
        {
          OutSettings.TargetDirectory = OptionArgument;
        }
      }
      else if (Arg == "-e" || Arg == "--extensions")
      {
        std::string OptionArgument;
        while (GetOptionArgument(argc, argv, i, CommandLineOptions, OptionArgument))
        {
          OutSettings.Extensions.push_back(OptionArgument);
        }
      }
      else if (Arg == "-l" || Arg == "--line_size")
      {
        std::string OptionArgument;
        if (GetOptionArgument(argc, argv, i, CommandLineOptions, OptionArgument))
        {
          std::stringstream Stream(OptionArgument);
          Stream >> OutSettings.MaxCharactersPerLine;

          if (OutSettings.MaxCharactersPerLine < 20)
          {
            OutSettings.MaxCharactersPerLine = 20;
          }
        }
      }
      else if (Arg == "-i" || Arg == "--ignore")
      {
        std::string OptionArgument;
        while (GetOptionArgument(argc, argv, i, CommandLineOptions, OptionArgument))
        {
          OutSettings.IgnoredDirectories.push_back(OptionArgument);
        }
      }
    }
  }

  return true;
}

void GetFilePathListFromDirectory(const std::string& DirectoryPath, AppSettings& Settings, std::vector<std::string>& OutFiles)
{
  DIR* dir = opendir(DirectoryPath.c_str());

  if (dir != nullptr)
  {
    dirent* ent;

    while ((ent = readdir(dir)) != nullptr)
    {
      std::string FileName = ent->d_name;

      if (ent->d_type == DT_REG && HasValidExtension(FileName, Settings))
      {
        OutFiles.push_back(DirectoryPath + '/' + FileName);
      }
      else if (ent->d_type == DT_DIR)
      {
        if (FileName != "." && FileName != "..")
        {
          if (std::find(Settings.IgnoredDirectories.begin(), Settings.IgnoredDirectories.end(), FileName) == Settings.IgnoredDirectories.end())
          {
            GetFilePathListFromDirectory(DirectoryPath + '/' + FileName, Settings, OutFiles);
          }
        }
      }
    }

    closedir(dir);
  }
}


bool HasValidExtension(std::string& FilePath, const AppSettings& Settings)
{
  if (Settings.Extensions.empty())
  {
    return true;
  }

  size_t Pos = FilePath.find_last_of('.');

  if (std::string::npos != Pos)
  {
    std::string FileExtension = FilePath.substr(Pos);

    for (size_t i = 0; i < Settings.Extensions.size(); ++i)
    {
      if (Settings.Extensions[i] == FileExtension)
      {
        return true;
      }
    }
  }

  return false;
}

void ProcessFile(const std::string& FilePath, const AppSettings& Settings, std::vector<std::string>& ErrorList)
{
  std::ifstream File(FilePath, std::ios::binary | std::ios::ate);


  if (File.is_open())
  {
    std::string FileContents;
    int Length = File.tellg();
    File.seekg(0, std::ios::beg);
    std::vector<char> Buffer;
    Buffer.resize(Length + 1);

    File.read(&Buffer[0], Length);
    Buffer[Buffer.size() - 1] = '\0';
    FileContents = &Buffer[0];

    int CurrentLineCount = 1;
    int LineCharacterCount = 0;
    Range TabRange = {-1, -1};
    bool TabFoundOnLine = false;
    bool AlreadyExceededLineLength = false;

    for (auto Character : FileContents)
    {
      LineCharacterCount++;

      if (Character == '\t')
      {
        TabFoundOnLine = true;
      }
      else if (Character == '\n')
      {
        if (TabFoundOnLine)
        {
          if (TabRange.Start == -1)
          {
            TabRange.Start = CurrentLineCount;
            TabRange.Stop = CurrentLineCount;
          }
          else
          {
            TabRange.Stop = CurrentLineCount;
          }
        }
        else if (TabRange.Start != -1 && TabRange.Stop != -1)
        {
          if (TabRange.Start == TabRange.Stop)
          {
            ErrorList.push_back(FilePath + "(" + std::to_string(TabRange.Start) + "): " + "Tab Found!");
          }
          else
          {
            ErrorList.push_back(FilePath + "(" + std::to_string(TabRange.Start) + " - " + std::to_string(TabRange.Stop) + "): " + "Tabs Found!");
          }

          TabRange = { -1, -1 };
        }

        CurrentLineCount++;
        LineCharacterCount = 0;
        TabFoundOnLine = false;
        AlreadyExceededLineLength = false;
      }
      else if (LineCharacterCount > Settings.MaxCharactersPerLine && !AlreadyExceededLineLength)
      {
        AlreadyExceededLineLength = true;
        ErrorList.push_back(FilePath + "(" + std::to_string(CurrentLineCount) + "): " + "Line is greater than " + std::to_string(Settings.MaxCharactersPerLine) + " characters!");
      }
    }
  }
  else
  {
    ErrorList.push_back(std::string("Unable to process file: " + FilePath));
  }
}