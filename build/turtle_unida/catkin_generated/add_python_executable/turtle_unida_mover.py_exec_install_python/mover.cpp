//
// Software License Agreement (BSD License)
//
// Copyright (c) 2011, Yujin Robot
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above
//   copyright notice, this list of conditions and the following
//   disclaimer in the documentation and/or other materials provided
//   with the distribution.
// * Neither the name of Yujin Robot nor the names of its
//   contributors may be used to endorse or promote products derived
//   from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//

/*
 * @file python_win32_wrapper.cpp
 *
 * @brief Launch a Python script located in the same location and have the same name as the compiled executable
 *
 * @date March 2011
 */

#ifdef _WIN32

#include <fstream>
#include <iostream>
#include <string>

#include <windows.h>

static const std::string SHEBANG = "#!";

static std::string dirnameOf(const std::string& fname)
{
    size_t pos = fname.find_last_of("\\/");
    if (std::string::npos == pos)
    {
        throw ERROR_PATH_NOT_FOUND;
    }
    return fname.substr(0, pos);
}

int main(int argc, char* argv[]) try
{
    const auto GetCurrentModuleName = []() -> std::string
    {
        char moduleName[MAX_PATH];

        // retrieves the path of the executable file of the current process
        auto result = ::GetModuleFileName(nullptr, moduleName, MAX_PATH);
        if (!result || result == MAX_PATH)
        {
            // If the function fails, the return value is 0
            // If the buffer is too small to hold the module name, the return value is MAX_PATH
            throw ::GetLastError();
        }

#if defined(DEBUG)
        fprintf(stderr, "[DEBUG] current module name: %s\n", moduleName);
#endif

        return moduleName;
    };

    const auto FindPythonScript = [](const std::string& exeFullPath) -> std::string
    {
        std::string scriptName;
        scriptName += dirnameOf(exeFullPath);
        scriptName += "\\mover.py";

#if defined(DEBUG)
        fprintf(stderr, "[DEBUG] Python script name: %s\n", scriptName.c_str());
#endif

        return scriptName;
    };

    const auto GetPythonExecutable = [](const std::string& exeFullPath) -> std::string
    {
        std::string scriptName;
        scriptName += dirnameOf(exeFullPath);
        scriptName += "\\mover.py";

        std::ifstream infile(scriptName);

        std::string firstLine;
        if (std::getline(infile, firstLine) && firstLine.find(SHEBANG) == 0)
        {
            firstLine.erase(0, SHEBANG.size());

            // spaces & tabs are considered as extra
            std::string extra_chars(" \t");

            // left trim extra characters.
            auto ltrim = firstLine.find_first_not_of(extra_chars);
            if (std::string::npos != ltrim)
            {
                firstLine.erase(0, ltrim);
            }

            // right trim extra characters.
            auto rtrim = firstLine.find_last_not_of(extra_chars);
            if (std::string::npos != rtrim)
            {
                firstLine.erase(rtrim + 1);
            }
        }

        // Default to Python executable registered in runtime environment
        std::string pythonExecutable = "python.exe";
        if (!firstLine.empty() && 
            (INVALID_FILE_ATTRIBUTES != GetFileAttributes(firstLine.c_str()))) // check file existence
        {
            pythonExecutable = firstLine;
        }

#if defined(DEBUG)
        fprintf(stderr, "[DEBUG] Python executable: %s\n", pythonExecutable.c_str());
#endif

        return pythonExecutable;
    };

    const auto GetDoubleQuotedString = [](const std::string &input) -> std::string
    {
        // use quoted string to ensure the correct path is used
        return "\"" + input + "\"";
    };

    const auto ExecuteCommand = [](std::string command, bool printError = false) -> unsigned long
    {
#if defined(DEBUG)
        fprintf(stderr, "[DEBUG] command: %s\n", command.c_str());
#endif

        STARTUPINFO startup_info;
        PROCESS_INFORMATION process_info;
        ::memset(&startup_info, 0, sizeof(startup_info));
        ::memset(&process_info, 0, sizeof(process_info));
        startup_info.cb = sizeof(startup_info);

        if (!::CreateProcess(
            nullptr,                // program to execute (nullptr = execute command line)
            &command[0],            // command line to execute
            nullptr,                // process security attributes
            nullptr,                // thread security attributes
            false,                  // determines if handles from parent process are inherited
            0,                      // no creation flags
            nullptr,                // enviornment (nullptr = use parent's)
            nullptr,                // current directory (nullptr = use parent's)
            &startup_info,          // startup info
            &process_info           // process info
        ))
        {
            const auto error = ::GetLastError();
            switch (error)
            {
            case ERROR_FILE_NOT_FOUND:
            {
                if (printError)
                {
                    fprintf(stderr, "Error! Python executable in [%s] cannot be found.\n", command.c_str());
                }
                break;
            }
            default:
                if (printError)
                {
                    fprintf(stderr, "Error! CreateProcess for [%s] failed with error code: %ld\n", command.c_str(), error);
                }
                break;
            }
            throw error;
        }
        ::WaitForSingleObject(process_info.hProcess, INFINITE);
        unsigned long exitCode = NO_ERROR;
        ::GetExitCodeProcess(process_info.hProcess, &exitCode);
        ::CloseHandle(process_info.hProcess);
        ::CloseHandle(process_info.hThread);

#if defined(DEBUG)
        fprintf(stderr, "[DEBUG] process exist code: %ld\n", exitCode);
#endif

        return exitCode;
    };

#if defined(DEBUG)
    fprintf(stderr, "[DEBUG] argv:\n");
    for (auto i = 0; i < argc; ++i)
    {
        fprintf(stderr, "[DEBUG] %d:\t%s\n", i, argv[i]);
    }
#endif

    const std::string exeFullPath = GetCurrentModuleName();
    const auto pythonExecutable = GetPythonExecutable(exeFullPath);
    const auto pythonScript = FindPythonScript(exeFullPath);
    std::string command = GetDoubleQuotedString(pythonExecutable) + " " + GetDoubleQuotedString(pythonScript);
    for (auto i = 1; i < argc; ++i)
    {
        command += " ";
        // use quoted strings to handle spaces within each argument
        command += " \"" + std::string(argv[i]) + "\"";
    }
    return ExecuteCommand(command, true);
}
catch (...)
{
    fprintf(stderr, "Failed to execute the Python script...\n");
    return 1;
}

#else

#error This wrapper should only be created on Windows.

int main()
{
    // deliberately add syntax error when the file is not supposed to get compiled
    return 0
}

#endif
