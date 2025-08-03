// Git.h - Git support for CommitMonitor
#pragma once
#include <string>
#include <vector>
#include "SCCS.h"

class Git : public SCCS {
public:
    Git();
    ~Git();

    void SetAuthInfo(const std::wstring& username, const std::wstring& password) override;
    bool GetFile(std::wstring sUrl, std::wstring sFile) override;
    std::wstring GetRootUrl(const std::wstring& path) override;
    size_t GetFileCount() override;
    svn_revnum_t GetHEADRevision(const std::wstring& repo, const std::wstring& url) override;
    bool GetLog(const std::wstring& repo, const std::wstring& url, svn_revnum_t startrev, svn_revnum_t endrev) override;
    std::wstring GetLastErrorMsg() override;
    void SetAndClearProgressInfo(CProgressDlg* pProgressDlg, bool bShowProgressBar = false) override;

    bool Diff(const std::wstring& url1, svn_revnum_t pegrevision, svn_revnum_t revision1,
            svn_revnum_t revision2, bool ignoreancestry, bool nodiffdeleted,
            bool ignorecontenttype, const std::wstring& options, bool bAppend,
            const std::wstring& outputfile, const std::wstring& errorfile) override;

    // Git-specific operations
    bool GetGitLog(const std::wstring& repoPath, const std::wstring& branch, std::vector<SCCSLogEntry>& logEntries, int maxCount = 100);
    bool GetGitDiff(const std::wstring& repoPath, const std::wstring& commitHash, std::wstring& diffText);
    bool GetCommit(const std::wstring& repoPath, const std::wstring& commitHash, SCCSLogEntry& entry);

private:
    bool RunGitCommand(const std::wstring& cmd, std::wstring& output);
    std::wstring m_username;
    std::wstring m_password;
    std::wstring m_lastError;
};
