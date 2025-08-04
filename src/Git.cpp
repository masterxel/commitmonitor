// Git.cpp - Git support for CommitMonitor
#include "stdafx.h"
#include "Git.h"
#include <sstream>
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <apr_time.h>

Git::Git() {}
Git::~Git() {}

void Git::SetAuthInfo(const std::wstring& username, const std::wstring& password) {
    m_username = username;
    m_password = password;
}

bool Git::GetFile(std::wstring sUrl, std::wstring sFile) {
    // Not implemented for Git
    return false;
}

std::wstring Git::GetRootUrl(const std::wstring& path) {
    // For Git, we'll return the repository root path
    std::wstringstream cmd;
    cmd << L"git -C \"" << path << L"\" rev-parse --show-toplevel";
    std::wstring output;
    if (!RunGitCommand(cmd.str(), output))
        return L"";

    // Remove trailing newline
    if (!output.empty() && output[output.length()-1] == L'\n') {
        output.erase(output.length()-1);
    }
    return output;
}

size_t Git::GetFileCount() {
    // Not implemented for Git as it's not directly comparable to SVN's concept
    return 0;
}

svn_revnum_t Git::GetHEADRevision(const std::wstring& repo, const std::wstring& url) {
    // Git doesn't use numeric revisions like SVN. Return 0 to indicate latest.
    return 0;
}

bool Git::GetLog(const std::wstring& repo, const std::wstring& url, svn_revnum_t startrev, svn_revnum_t endrev) {
    // For Git, the url parameter may contain either a path or a branch/ref name
    std::vector<SCCSLogEntry> logEntries;
    return GetGitLog(repo, url.empty() ? L"HEAD" : url, logEntries, 100);
}

bool Git::Diff(const std::wstring& url1, svn_revnum_t pegrevision, svn_revnum_t revision1,
        svn_revnum_t revision2, bool ignoreancestry, bool nodiffdeleted,
        bool ignorecontenttype, const std::wstring& options, bool bAppend,
        const std::wstring& outputfile, const std::wstring& errorfile) {
    // In Git, we'll use the commit hashes instead of revision numbers
    std::wstringstream cmd;
    cmd << L"git -C \"" << url1 << L"\" diff ";
    if (!options.empty()) {
        cmd << options << L" ";
    }
    if (ignoreancestry) {
        cmd << L"--no-renames ";
    }
    if (nodiffdeleted) {
        cmd << L"--diff-filter=d ";
    }
    cmd << revision1 << L".." << revision2;

    std::wstring output;
    if (!RunGitCommand(cmd.str(), output))
        return false;

    // Open output file and write the results
    FILE* outFile = _wfopen(outputfile.c_str(), bAppend ? L"a" : L"w");
    if (!outFile) return false;

    fputws(output.c_str(), outFile);
    fclose(outFile);
    return true;
}

bool Git::RunGitCommand(const std::wstring& cmd, std::wstring& output) {
    CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(" running Git command %s \n"), cmd.c_str());

    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE hReadPipe = NULL, hWritePipe = NULL;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        m_lastError = L"Failed to create pipe";
        return false;
    }

    STARTUPINFO si = { sizeof(STARTUPINFO) };
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi = { 0 };
    std::wstring cmdLine = L"cmd.exe /c " + cmd;

    BOOL success = CreateProcessW(NULL, (LPWSTR)cmdLine.c_str(),
        NULL, NULL, TRUE, CREATE_NO_WINDOW,
        NULL, NULL, &si, &pi);

    if (!success) {
        m_lastError = L"Failed to create process";
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return false;
    }

    CloseHandle(hWritePipe); // Close write end of pipe to get EOF when process completes

    // Read output
    char buffer[4096];
    DWORD bytesRead;
    std::string result;

    while (ReadFile(hReadPipe, buffer, sizeof(buffer)-1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = 0;
        result += buffer;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hReadPipe);

    // Convert result to wide string
    int size = MultiByteToWideChar(CP_UTF8, 0, result.c_str(), -1, NULL, 0);
    if (size > 0) {
        std::vector<wchar_t> buf(size);
        MultiByteToWideChar(CP_UTF8, 0, result.c_str(), -1, &buf[0], size);
        output = &buf[0];
    }

    CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(" Git command output: %s \n"), output.substr(0, 1023).c_str());

    return exitCode == 0;
}

std::wstring Git::GetLastErrorMsg() {
    return m_lastError;
}

void Git::SetAndClearProgressInfo(CProgressDlg* pProgressDlg, bool bShowProgressBar) {
    // Not implemented for Git as we use command-line git which handles its own progress
}

bool Git::GetGitLog(const std::wstring& repoPath, const std::wstring& branch, std::vector<SCCSLogEntry>& logEntries, int maxCount) {
    // Check if Git is available
    std::wstring output;
    if (!RunGitCommand(L"git --version", output)) {
        m_lastError = L"Git is not available in PATH";
        return false;
    }

    // Use git CLI to get log
    std::wstringstream cmd;

    // First get the remote tracking branch for the current or specified branch
    std::wstring remoteBranch;
    if (branch == L"HEAD" || branch.empty()) {
        // Get the remote tracking branch for the current branch
        std::wstringstream remoteCmd;
        remoteCmd << L"git -C \"" << repoPath << L"\" rev-parse --abbrev-ref --symbolic-full-name @{u}";
        if (!RunGitCommand(remoteCmd.str(), remoteBranch)) {
            // If there's no tracking branch, fallback to origin/HEAD
            remoteBranch = L"origin/HEAD";
        }
        // Trim any whitespace or newlines
        while (!remoteBranch.empty() && (remoteBranch.back() == L'\n' || remoteBranch.back() == L'\r')) {
            remoteBranch.pop_back();
        }
    } else {
        // For specified branches, try to get their remote tracking branch
        std::wstringstream remoteCmd;
        remoteCmd << L"git -C \"" << repoPath << L"\" rev-parse --abbrev-ref --symbolic-full-name " << branch << "@{u}";
        if (!RunGitCommand(remoteCmd.str(), remoteBranch)) {
            // If there's no tracking branch, try origin/<branch>
            remoteBranch = L"origin/" + branch;
        }
        // Trim any whitespace or newlines
        while (!remoteBranch.empty() && (remoteBranch.back() == L'\n' || remoteBranch.back() == L'\r')) {
            remoteBranch.pop_back();
        }
    }

    // Fetch all remotes to ensure we have the latest changes
    std::wstringstream fetchCmd;
    fetchCmd << L"git -C \"" << repoPath << L"\" fetch --all";
    RunGitCommand(fetchCmd.str(), output); // Ignore fetch errors

    // Now get the log from the remote tracking branch
    cmd << L"git -C \"" << repoPath << L"\" -c core.quotepath=off log " << remoteBranch 
        << L" --no-merges"
        << L" --first-parent"
        << L" --pretty=format:\"CommitStart%x1E%h|%H|%P|%an|%at|%B%x1E\" -n " << maxCount;

    std::wstring cmdOutput;
    if (!RunGitCommand(cmd.str(), cmdOutput)) {
        m_lastError = L"Failed to execute Git command";
        return false;
    }

    // Split output by commit separator (Record Separator control char)
    const std::wstring separator = L"CommitStart\x1E";
    size_t pos = 0;
    size_t nextPos;
    while ((nextPos = cmdOutput.find(separator, pos)) != std::wstring::npos) {
        // Extract commit data between separators
        size_t endPos = cmdOutput.find(L"\x1E", nextPos + separator.length());
        if (endPos == std::wstring::npos) break;
        
        std::wstring commitData = cmdOutput.substr(nextPos + separator.length(), 
                                                 endPos - (nextPos + separator.length()));
        if (commitData.empty()) {
            pos = endPos + 1;
            continue;
        }

        SCCSLogEntry entry;
        // Parse fields separated by |
        std::vector<std::wstring> fields;
        size_t fieldStart = 0;
        size_t fieldEnd;
        
        while ((fieldEnd = commitData.find(L'|', fieldStart)) != std::wstring::npos) {
            fields.push_back(commitData.substr(fieldStart, fieldEnd - fieldStart));
            fieldStart = fieldEnd + 1;
        }
        // Add the last field (commit message)
        fields.push_back(commitData.substr(fieldStart));
        
        if (fields.size() >= 6) {
            entry.shortHash = fields[0];
            entry.commitHash = fields[1];
            entry.parentHashes.push_back(fields[2]);
            entry.author = fields[3];
            // Convert Unix timestamp to APR time (microseconds since epoch)
            apr_time_t unixTime = _wtoi64(fields[4].c_str());
            entry.date = unixTime * APR_TIME_C(1000000);
            entry.message = fields[5];
            
            logEntries.push_back(entry);
        }
        
        pos = endPos + 1;
    }
    return true;
}

bool Git::GetGitDiff(const std::wstring& repoPath, const std::wstring& commitHash, std::wstring& diffText) {
    std::wstringstream cmd;
    // Force UTF-8 output and disable path quoting
    cmd << L"git -C \"" << repoPath << L"\" -c core.quotepath=off show " << commitHash << L" --pretty=format: --no-color";
    
    if (!RunGitCommand(cmd.str(), diffText)) {
        m_lastError = L"Git diff command failed";
        return false;
    }
    return true;
}

bool Git::GetCommit(const std::wstring& repoPath, const std::wstring& commitHash, SCCSLogEntry& entry) {
    std::wstringstream cmd;
    // Force UTF-8 output and disable path quoting
    cmd << L"git -C \"" << repoPath << L"\" -c core.quotepath=off show " << commitHash << L" --pretty=format:\"%H|%P|%an|%at|%s\" --no-patch";
    
    std::wstring output;
    if (!RunGitCommand(cmd.str(), output)) {
        m_lastError = L"Failed to execute Git show command";
        return false;
    }
    
    std::wstring line = output;
    // Trim newline characters
    while (!line.empty() && (line.back() == L'\n' || line.back() == L'\r')) {
        line.pop_back();
    }
    
    if (!line.empty()) {
        // Trim newline characters
        while (!line.empty() && (line.back() == L'\n' || line.back() == L'\r')) {
            line.pop_back();
        }
        size_t pos = 0;
        // Parse: %H|%P|%an|%at|%s
        pos = line.find(L"|");
        entry.commitHash = line.substr(0, pos);
        size_t pos2 = line.find(L"|", pos+1);
        entry.parentHashes.push_back(line.substr(pos+1, pos2-pos-1));
        size_t pos3 = line.find(L"|", pos2+1);
        entry.author = line.substr(pos2+1, pos3-pos2-1);
        size_t pos4 = line.find(L"|", pos3+1);
        // Convert Unix timestamp to APR time (microseconds since epoch)
        apr_time_t unixTime = _wtoi64(line.substr(pos3+1, pos4-pos3-1).c_str());
        entry.date = unixTime * APR_TIME_C(1000000); // Convert seconds to microseconds
        entry.message = line.substr(pos4+1);
    }
    return true;
}
