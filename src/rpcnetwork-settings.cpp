// Copyright (c) 2014-2016 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "db.h"
#include "init.h"
#include "activemasternode.h"
#include "masternode-governance.h"
#include "masternode-payments.h"
#include "masternode-sync.h"
#include "masternodeconfig.h"
#include "masternodeman.h"
#include "rpcserver.h"
#include "utilmoneystr.h"
#include <boost/lexical_cast.hpp>

#include <fstream>
#include <univalue.h>
#include <iostream>
#include <sstream>

using namespace std;

UniValue setting(const UniValue& params, bool fHelp)
{
    string strCommand;
    if (params.size() >= 1)
        strCommand = params[0].get_str();

    string strSubCommand = "default";
    if (params.size() >= 2)
        strSubCommand = params[1].get_str();

    if (fHelp  ||
        (strCommand != "prepare" && strCommand != "submit" &&
         strCommand != "get" && strCommand != "gethash" && strCommand != "list"))
        throw runtime_error(
                "setting \"command\"...\n"
                "Manage contracts\n"
                "\nAvailable commands:\n"
                "  update             - Update an existing network setting\n"
                "  list               - List settings in various ([all|valid|extended|active|help])\n"
                );


    if(strCommand == "update")
    {
        if (params.size() != 7 || strSubCommand == "help")
            throw runtime_error("Correct usage is 'setting prepare <setting-name> <url> <suggested-value>'");

        int nBlockMin = 0;
        LOCK(cs_main);
        CBlockIndex* pindex = chainActive.Tip();

        std::vector<CMasternodeConfig::CMasternodeEntry> mnEntries;
        mnEntries = masternodeConfig.getEntries();

        std::string strName = SanitizeString(params[1].get_str());
        std::string strURL = SanitizeString(params[2].get_str());
        std::string strSuggestedValue = SanitizeString(params[3].get_str());

        //*************************************************************************

        return "stub";
    }

    return NullUniValue;
}
