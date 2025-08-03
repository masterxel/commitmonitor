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
    FILE* pipe = _wpopen(cmd.str().c_str(), L"rt");
    if (!pipe) return L"";
    wchar_t buffer[4096];
    if (fgetws(buffer, sizeof(buffer)/sizeof(wchar_t), pipe)) {
        std::wstring rootPath(buffer);
        // Remove trailing newline
        if (!rootPath.empty() && rootPath[rootPath.length()-1] == L'\n') {
            rootPath.erase(rootPath.length()-1);
        }
        _pclose(pipe);
        return rootPath;
    }
    _pclose(pipe);
    return L"";
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

    // Open output file
    FILE* outFile = _wfopen(outputfile.c_str(), bAppend ? L"a" : L"w");
    if (!outFile) return false;

    // Run git command and capture output
    FILE* pipe = _wpopen(cmd.str().c_str(), L"rt");
    if (!pipe) {
        fclose(outFile);
        return false;
    }

    // Copy output to file
    wchar_t buffer[4096];
    while (fgetws(buffer, sizeof(buffer)/sizeof(wchar_t), pipe)) {
        fputws(buffer, outFile);
    }
    
    _pclose(pipe);
    fclose(outFile);
    return true;
}

std::wstring Git::GetLastErrorMsg() {
    return m_lastError;
}

void Git::SetAndClearProgressInfo(CProgressDlg* pProgressDlg, bool bShowProgressBar) {
    // Not implemented for Git as we use command-line git which handles its own progress
}

bool Git::GetGitLog(const std::wstring& repoPath, const std::wstring& branch, std::vector<SCCSLogEntry>& logEntries, int maxCount) {
    // Check if Git is available
    FILE* testPipe = _wpopen(L"git --version", L"r");
    if (!testPipe) {
        m_lastError = L"Git is not available in PATH";
        return false;
    }
    _pclose(testPipe);

    // Use git CLI to get log
    std::wstringstream cmd;
    // Force Git to use UTF-8 output
    // Fetch all first to ensure we have latest changes
    std::wstringstream fetchCmd;
    fetchCmd << L"git -C \"" << repoPath << L"\" fetch --all";
    FILE* fetchPipe = _wpopen(fetchCmd.str().c_str(), L"r");
    if (fetchPipe) _pclose(fetchPipe);

    cmd << L"set PYTHONIOENCODING=utf-8 && git -C \"" << repoPath << L"\" -c core.quotepath=off log " << branch << L" --pretty=format:\"%h|%H|%P|%an|%at|%s\" -n " << maxCount;

    // Open pipe in text mode for proper UTF-8 to UTF-16 conversion
    FILE* pipe = _wpopen(cmd.str().c_str(), L"r");
    if (!pipe) {
        m_lastError = L"Failed to execute Git command";
        return false;
    }

    // Set the pipe to UTF-8 mode
    _setmode(_fileno(pipe), _O_U8TEXT);

    wchar_t buffer[4096];
    bool hasEntries = false;

    // Read output line by line, now properly handling UTF-8
    while (fgetws(buffer, sizeof(buffer)/sizeof(wchar_t), pipe)) {
        std::wstring line(buffer);
        // Trim newline characters
        while (!line.empty() && (line.back() == L'\n' || line.back() == L'\r')) {
            line.pop_back();
        }
        
        if (line.empty()) continue;

        size_t pos = 0;
        SCCSLogEntry entry;
        // Parse: %h|%H|%P|%an|%at|%s
        pos = line.find(L"|");
        if (pos == std::wstring::npos) continue;  // Invalid format, skip line
        
        entry.shortHash = line.substr(0, pos);  // Store short hash
        size_t pos2 = line.find(L"|", pos+1);
        if (pos2 == std::wstring::npos) continue;  // Invalid format, skip line
        
        entry.commitHash = line.substr(pos+1, pos2-pos-1);  // Store full hash
        size_t pos3 = line.find(L"|", pos2+1);
        if (pos3 == std::wstring::npos) continue;  // Invalid format, skip line
        
        entry.parentHashes.push_back(line.substr(pos2+1, pos3-pos2-1));
        size_t pos4 = line.find(L"|", pos3+1);
        entry.author = line.substr(pos3+1, pos4-pos3-1);
        size_t pos5 = line.find(L"|", pos4+1);
        // Convert Unix timestamp to APR time (microseconds since epoch)
        apr_time_t unixTime = _wtoi64(line.substr(pos4+1, pos5-pos4-1).c_str());
        entry.date = unixTime * APR_TIME_C(1000000); // Convert seconds to microseconds
        entry.message = line.substr(pos5+1);
        logEntries.push_back(entry);
    }
    _pclose(pipe);
    return true;
}

bool Git::GetGitDiff(const std::wstring& repoPath, const std::wstring& commitHash, std::wstring& diffText) {
    std::wstringstream cmd;
    // Force UTF-8 output and disable path quoting
    cmd << L"set PYTHONIOENCODING=utf-8 && git -C \"" << repoPath << L"\" -c core.quotepath=off show " << commitHash << L" --pretty=format: --no-color";
    FILE* pipe = _wpopen(cmd.str().c_str(), L"r");
    if (!pipe) {
        m_lastError = L"Failed to execute Git diff command";
        return false;
    }
    
    // Set the pipe to UTF-8 mode
    _setmode(_fileno(pipe), _O_U8TEXT);
    
    wchar_t buffer[4096];
    diffText.clear();
    while (fgetws(buffer, sizeof(buffer)/sizeof(wchar_t), pipe)) {
        diffText += buffer;
    }
    int status = _pclose(pipe);
    if (status != 0) {
        m_lastError = L"Git diff command failed";
        return false;
    }
    return true;
}

bool Git::GetCommit(const std::wstring& repoPath, const std::wstring& commitHash, SCCSLogEntry& entry) {
    std::wstringstream cmd;
    // Force UTF-8 output and disable path quoting
    cmd << L"set PYTHONIOENCODING=utf-8 && git -C \"" << repoPath << L"\" -c core.quotepath=off show " << commitHash << L" --pretty=format:\"%H|%P|%an|%at|%s\" --no-patch";
    FILE* pipe = _wpopen(cmd.str().c_str(), L"r");
    if (!pipe) {
        m_lastError = L"Failed to execute Git show command";
        return false;
    }

    // Set the pipe to UTF-8 mode
    _setmode(_fileno(pipe), _O_U8TEXT);
    
    wchar_t buffer[4096];
    if (fgetws(buffer, sizeof(buffer)/sizeof(wchar_t), pipe)) {
        std::wstring line(buffer);
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
    _pclose(pipe);
    return true;
}
