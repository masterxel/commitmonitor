// GitFactory.h - Factory for creating Git SCCS objects
#pragma once
#include "Git.h"
#include "SCCS.h"
#include "UrlInfo.h"

class SCCSFactory {
public:
    static SCCS* CreateSCCS(CUrlInfo::SCCS_TYPE type) {
        switch (type) {
        case CUrlInfo::SCCS_SVN:
            return new SVN();
        case CUrlInfo::SCCS_ACCUREV:
            // return new Accurev(); // If implemented
            return nullptr;
        case CUrlInfo::SCCS_GIT:
            return new Git();
        default:
            return nullptr;
        }
    }
};
