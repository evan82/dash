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

UniValue admin(const UniValue& params, bool fHelp)
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
                "admin \"command\"...\n"
                "Issue administrative network actions\n"
                "\nAvailable commands:\n"
                "  remove_proposal             - Remove proposal from the governance system, by hash\n"
                "  remove_contract             - Remove contract from the governance system, by hash\n"
                "  update_proposal_field       - Update a given field of a proposal\n"
                "  update_contract_field       - Update a given field of a contract\n"
                "  create_new_setting          - Create new setting for the network\n"
                "  update_setting_field        - Update a given field of a setting\n"
                "  create_new_switch          - Create new setting for the network\n"
                "  update_switch_field        - Update a given field of a setting\n"
                );


    if(strCommand == "remove_proposal")
    {
        if (params.size() != 2 || strSubCommand == "help")
            throw runtime_error("Correct usage is 'admin remove_proposal <proposal-hash>'");

        //*************************************************************************

        return "stub";
    }

    if(strCommand == "remove_contract")
    {
        if (params.size() != 2 || strSubCommand == "help")
            throw runtime_error("Correct usage is 'admin remove_contract <proposal-hash>'");

        //*************************************************************************

        return "stub";
    }

    if(strCommand == "update_proposal_field")
    {
        if (params.size() != 2 || strSubCommand == "help")
            throw runtime_error("Correct usage is 'admin update_proposal_field <proposal-hash> <field-name> <new-value>'");

        //*************************************************************************

        return "stub";
    }

    if(strCommand == "update_contract_field")
    {
        if (params.size() != 2 || strSubCommand == "help")
            throw runtime_error("Correct usage is 'admin update_contract_field <contract-hash> <field-name> <new-value>'");

        //*************************************************************************

        return "stub";
    }

    if(strCommand == "update_setting_field")
    {
        if (params.size() != 2 || strSubCommand == "help")
            throw runtime_error("Correct usage is 'admin update_setting_field <setting-hash> <field-name> <new-value>'");

        //*************************************************************************

        return "stub";
    }

    if(strCommand == "create_new_switch")
    {
        if (params.size() != 2 || strSubCommand == "help")
            throw runtime_error("Correct usage is 'admin update_switch_field <switch-hash> <switch-name> <possible-values> <default-value>'");

        //*************************************************************************

        /*
            Notes on creating new switches!

            - lower_case only
            - a-z, "_" and "|" only for possible values

            For forks or any boolean operations please use: possible-values == "on|off", default-value = "off"

            ---

            Multiple opinions, such as enums should be represented as a switch:

            Example:

                enum AdminCommandState {
                    NEW = -1,
                    AWAITING_SIGNATURES,
                    PENDING_EXECUTION,
                    EXECUTED
                };

            possible-values = "new|awaiting_signatures|pending_execution|executed"
            default-value = "new"

        */

        return "stub";
    }

    if(strCommand == "create_new_setting")
    {
        if (params.size() != 4 || strSubCommand == "help")
            throw runtime_error("Correct usage is 'admin create_new_setting <setting-name> <setting-value-type> <setting-value>'");

        //*************************************************************************

        return "stub";
    }

    if(strCommand == "update_setting_field")
    {
        if (params.size() != 4 || strSubCommand == "help")
            throw runtime_error("Correct usage is 'admin update_setting_field <setting-name> <setting-value>'");

        //*************************************************************************

        return "stub";
    }

    return NullUniValue;
}


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
