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

UniValue vote(const UniValue& params, bool fHelp)
{
    string strCommand;
    if (params.size() >= 1)
        strCommand = params[0].get_str();

    if (fHelp  ||
        (strCommand != "many" && strCommand != "alias" && strCommand != "get" && strCommand != "one"))
        throw runtime_error(
                "vote \"command\"...\n"
                "Vote on proposals, contracts, switches or settings\n"
                "\nAvailable commands:\n"
                "              - Prepare governance object by signing and creating tx\n"
                "  alias         - Vote on a governance object by alias\n"
                "  get           - Show detailed votes list for governance object\n"
                "  many          - Vote on a governance object by all masternodes (using masternode.conf setup)\n"
                "  one           - Vote on a governance object by single masternode (using dash.conf setup)\n"
                "  raw           - Submit raw governance object vote (used in trustless governance implementations)\n"
                );



    if(strCommand == "get")
    {
        if (params.size() != 2)
            throw runtime_error("Correct usage is 'vote get <governance-hash>'");

        uint256 hash = ParseHashV(params[1], "Governance hash");

        UniValue obj(UniValue::VOBJ);

        governance.GetGovernanceTypeByHash(hash);

        if(governance.GetGovernanceTypeByHash(hash) == Error)
        {
            return "Unknown governance object hash";
        }
        else if(governance.GetGovernanceTypeByHash(hash) == FinalizedBudget)
        {       
            CFinalizedBudget* pfinalBudget = governance.FindFinalizedBudget(hash);

            if(pfinalBudget == NULL) return "Unknown budget hash";

            std::map<uint256, CGovernanceVote>::iterator it = pfinalBudget->mapVotes.begin();
            while(it != pfinalBudget->mapVotes.end()){

                UniValue bObj(UniValue::VOBJ);
                bObj.push_back(Pair("nHash",  (*it).first.ToString().c_str()));
                bObj.push_back(Pair("nTime",  (int64_t)(*it).second.nTime));
                bObj.push_back(Pair("fValid",  (*it).second.fValid));

                obj.push_back(Pair((*it).second.vin.prevout.ToStringShort(), bObj));

                it++;
            }
        } 
        else // proposals, contracts, switches and settings
        { 
            CGovernanceObject* pGovernanceObj = governance.FindGovernanceObject(hash);

            if(pGovernanceObj == NULL) return "Unknown governance object hash";

            std::map<uint256, CGovernanceVote>::iterator it = pGovernanceObj->mapVotes.begin();
            while(it != pGovernanceObj->mapVotes.end()){

                UniValue bObj(UniValue::VOBJ);
                bObj.push_back(Pair("nHash",  (*it).first.ToString().c_str()));
                bObj.push_back(Pair("Vote",  (*it).second.GetVoteString()));
                bObj.push_back(Pair("nTime",  (int64_t)(*it).second.nTime));
                bObj.push_back(Pair("fValid",  (*it).second.fValid));

                obj.push_back(Pair((*it).second.vin.prevout.ToStringShort(), bObj));

                it++;
            }
        }

        return obj;
    }
    if(strCommand == "one")
    {
        if (params.size() != 3)
            throw runtime_error("Correct usage is 'proposal vote <proposal-hash> <yes|no>'");

        uint256 hash = ParseHashV(params[1], "Proposal hash");
        std::string strVote = params[2].get_str();

        if(strVote != "yes" && strVote != "no") return "You can only vote 'yes' or 'no'";
        int nVote = VOTE_ABSTAIN;
        if(strVote == "yes") nVote = VOTE_YES;
        if(strVote == "no") nVote = VOTE_NO;

        CPubKey pubKeyMasternode;
        CKey keyMasternode;
        std::string errorMessage;

        if(!darkSendSigner.SetKey(strMasterNodePrivKey, errorMessage, keyMasternode, pubKeyMasternode))
            return "Error upon calling SetKey";

        CMasternode* pmn = mnodeman.Find(activeMasternode.vin);
        if(pmn == NULL)
        {
            return "Failure to find masternode in list : " + activeMasternode.vin.ToString();
        }

        CGovernanceObject* goParent = governance.FindGovernanceObject(hash);
        if(!goParent)
        {
            return "Couldn't find governance obj";
        }

        CGovernanceVote vote(goParent, activeMasternode.vin, hash, nVote);
        if(!vote.Sign(keyMasternode, pubKeyMasternode)){
            return "Failure to sign.";
        }

        std::string strError = "";
        if(governance.UpdateGovernanceObjectVotes(vote, NULL, strError)){
            governance.mapSeenGovernanceVotes.insert(make_pair(vote.GetHash(), vote));
            vote.Relay();
            return "Voted successfully";
        } else {
            return "Error voting : " + strError;
        }
    }


    if(strCommand == "many")
    {
        if(params.size() != 3)
            throw runtime_error("Correct usage is 'proposal vote-many <proposal-hash> <yes|no>'");

        uint256 hash;
        std::string strVote;

        hash = ParseHashV(params[1], "Proposal hash");
        strVote = params[2].get_str();

        if(strVote != "yes" && strVote != "no") return "You can only vote 'yes' or 'no'";
        int nVote = VOTE_ABSTAIN;
        if(strVote == "yes") nVote = VOTE_YES;
        if(strVote == "no") nVote = VOTE_NO;

        int success = 0;
        int failed = 0;

        std::vector<CMasternodeConfig::CMasternodeEntry> mnEntries;
        mnEntries = masternodeConfig.getEntries();

        UniValue resultsObj(UniValue::VOBJ);

        BOOST_FOREACH(CMasternodeConfig::CMasternodeEntry mne, masternodeConfig.getEntries()) {
            std::string errorMessage;
            std::vector<unsigned char> vchMasterNodeSignature;
            std::string strMasterNodeSignMessage;

            CPubKey pubKeyCollateralAddress;
            CKey keyCollateralAddress;
            CPubKey pubKeyMasternode;
            CKey keyMasternode;

            UniValue statusObj(UniValue::VOBJ);

            if(!darkSendSigner.SetKey(mne.getPrivKey(), errorMessage, keyMasternode, pubKeyMasternode)){
                failed++;
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("errorMessage", "Masternode signing error, could not set key correctly: " + errorMessage));
                resultsObj.push_back(Pair(mne.getAlias(), statusObj));
                continue;
            }

            CMasternode* pmn = mnodeman.Find(pubKeyMasternode);
            if(pmn == NULL)
            {
                failed++;
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("errorMessage", "Can't find masternode by pubkey"));
                resultsObj.push_back(Pair(mne.getAlias(), statusObj));
                continue;
            }

            CGovernanceObject* goParent = governance.FindGovernanceObject(hash);
            if(!goParent)
            {
                failed++;
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("errorMessage", "Can't find governance object."));
                resultsObj.push_back(Pair(mne.getAlias(), statusObj));
            }

            CGovernanceVote vote(goParent, pmn->vin, hash, nVote);
            if(!vote.Sign(keyMasternode, pubKeyMasternode)){
                failed++;
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("errorMessage", "Failure to sign."));
                resultsObj.push_back(Pair(mne.getAlias(), statusObj));
                continue;
            }


            std::string strError = "";
            if(governance.UpdateGovernanceObjectVotes(vote, NULL, strError)) {
                governance.mapSeenGovernanceVotes.insert(make_pair(vote.GetHash(), vote));
                vote.Relay();
                success++;
                statusObj.push_back(Pair("result", "success"));
            } else {
                failed++;
                statusObj.push_back(Pair("result", strError.c_str()));
            }

            resultsObj.push_back(Pair(mne.getAlias(), statusObj));
        }

        UniValue returnObj(UniValue::VOBJ);
        returnObj.push_back(Pair("overall", strprintf("Voted successfully %d time(s) and failed %d time(s).", success, failed)));
        returnObj.push_back(Pair("detail", resultsObj));

        return returnObj;
    }

    if(strCommand == "alias")
    {
        if(params.size() != 4)
            throw runtime_error("Correct usage is 'proposal vote-alias <proposal-hash> <yes|no> <alias-name>'");

        uint256 hash;
        std::string strVote;

        hash = ParseHashV(params[1], "Proposal hash");
        strVote = params[2].get_str();
        std::string strAlias = params[3].get_str();

        if(strVote != "yes" && strVote != "no") return "You can only vote 'yes' or 'no'";
        int nVote = VOTE_ABSTAIN;
        if(strVote == "yes") nVote = VOTE_YES;
        if(strVote == "no") nVote = VOTE_NO;

        int success = 0;
        int failed = 0;

        std::vector<CMasternodeConfig::CMasternodeEntry> mnEntries;
        mnEntries = masternodeConfig.getEntries();

        UniValue resultsObj(UniValue::VOBJ);

        BOOST_FOREACH(CMasternodeConfig::CMasternodeEntry mne, masternodeConfig.getEntries()) {

            if( strAlias != mne.getAlias()) continue;

            std::string errorMessage;
            std::vector<unsigned char> vchMasterNodeSignature;
            std::string strMasterNodeSignMessage;

            CPubKey pubKeyCollateralAddress;
            CKey keyCollateralAddress;
            CPubKey pubKeyMasternode;
            CKey keyMasternode;

            UniValue statusObj(UniValue::VOBJ);

            if(!darkSendSigner.SetKey(mne.getPrivKey(), errorMessage, keyMasternode, pubKeyMasternode)){
                failed++;
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("errorMessage", "Masternode signing error, could not set key correctly: " + errorMessage));
                resultsObj.push_back(Pair(mne.getAlias(), statusObj));
                continue;
            }

            CMasternode* pmn = mnodeman.Find(pubKeyMasternode);
            if(pmn == NULL)
            {
                failed++;
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("errorMessage", "Can't find masternode by pubkey"));
                resultsObj.push_back(Pair(mne.getAlias(), statusObj));
                continue;
            }
    
            CGovernanceObject* goParent = governance.FindGovernanceObject(hash);
            if(!goParent)
            {
                return "Couldn't find governance obj";
            }

            CGovernanceVote vote(goParent, pmn->vin, hash, nVote);
            if(!vote.Sign(keyMasternode, pubKeyMasternode)){
                failed++;
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("errorMessage", "Failure to sign."));
                resultsObj.push_back(Pair(mne.getAlias(), statusObj));
                continue;
            }


            std::string strError = "";
            if(governance.UpdateGovernanceObjectVotes(vote, NULL, strError)) {
                governance.mapSeenGovernanceVotes.insert(make_pair(vote.GetHash(), vote));
                vote.Relay();
                success++;
                statusObj.push_back(Pair("result", "success"));
            } else {
                failed++;
                statusObj.push_back(Pair("result", strError.c_str()));
            }

            resultsObj.push_back(Pair(mne.getAlias(), statusObj));
        }

        UniValue returnObj(UniValue::VOBJ);
        returnObj.push_back(Pair("overall", strprintf("Voted successfully %d time(s) and failed %d time(s).", success, failed)));
        returnObj.push_back(Pair("detail", resultsObj));

        return returnObj;
    }

    if(strCommand == "raw")
    {
        if (fHelp || params.size() != 6)
            throw runtime_error(
                    "vote raw <masternode-tx-hash> <masternode-tx-index> <proposal-hash> <yes|no> <time> <vote-sig>\n"
                    "Compile and relay a governance object vote with provided external signature instead of signing vote internally\n"
                    );

        uint256 hashMnTx = ParseHashV(params[1], "mn tx hash");
        int nMnTxIndex = boost::lexical_cast<int>(params[2].get_str());

        CTxIn vin = CTxIn(hashMnTx, nMnTxIndex);

        uint256 hashProposal = ParseHashV(params[3], "Governance hash");
        std::string strVote = params[4].get_str();

        if(strVote != "yes" && strVote != "no") return "You can only vote 'yes' or 'no'";
        int nVote = VOTE_ABSTAIN;
        if(strVote == "yes") nVote = VOTE_YES;
        if(strVote == "no") nVote = VOTE_NO;

        int64_t nTime = boost::lexical_cast<int64_t>(params[5].get_str());
        std::string strSig = params[6].get_str();
        bool fInvalid = false;
        vector<unsigned char> vchSig = DecodeBase64(strSig.c_str(), &fInvalid);

        if (fInvalid)
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Malformed base64 encoding");

        CMasternode* pmn = mnodeman.Find(vin);
        if(pmn == NULL)
        {
            return "Failure to find masternode in list : " + vin.ToString();
        }

        CGovernanceObject* goParent = governance.FindGovernanceObject(hashProposal);
        if(!goParent)
        {
            return "Couldn't find governance obj";
        }

        CGovernanceVote vote(goParent, vin, hashProposal, nVote);
        vote.nTime = nTime;
        vote.vchSig = vchSig;

        std::string strReason;
        if(!vote.IsValid(true, strReason)){
            return "Failure to verify vote - " + strReason;
        }

        std::string strError = "";
        if(governance.UpdateGovernanceObjectVotes(vote, NULL, strError)){
            governance.mapSeenGovernanceVotes.insert(make_pair(vote.GetHash(), vote));
            vote.Relay();
            return "Voted successfully";
        } else {
            return "Error voting : " + strError;
        }
    }


    return NullUniValue;
}


UniValue proposal(const UniValue& params, bool fHelp)
{
    string strCommand;
    if (params.size() >= 1)
        strCommand = params[0].get_str();

    if (fHelp  ||
        (strCommand != "prepare" && strCommand != "submit" && strCommand != "get" && 
            strCommand != "gethash" && strCommand != "list"))
        throw runtime_error(
                "proposal \"command\"...\n"
                "Manage proposals\n"
                "\nAvailable commands:\n"
                "  prepare            - Prepare proposal by signing and creating tx\n"
                "  submit             - Submit proposal to network\n"
                "  list               - List all proposals - (list valid|all|extended)\n"
                "  get                - get proposal\n"
                "  gethash            - Get proposal hash(es) by proposal name\n"
                );

    if(strCommand == "prepare")
    {
        if (params.size() != 7)
            throw runtime_error("Correct usage is 'proposal prepare <proposal-name> <url> <payment-count> <block-start> <dash-address> <monthly-payment-dash>'");

        int nBlockMin = 0;
        LOCK(cs_main);
        CBlockIndex* pindex = chainActive.Tip();

        std::vector<CMasternodeConfig::CMasternodeEntry> mnEntries;
        mnEntries = masternodeConfig.getEntries();

        std::string strName = SanitizeString(params[1].get_str());
        std::string strURL = SanitizeString(params[2].get_str());
        int nPaymentCount = params[3].get_int();
        int nBlockStart = params[4].get_int();

        //set block min
        if(pindex != NULL) nBlockMin = pindex->nHeight;

        if(nBlockStart < nBlockMin)
            return "Invalid block start, must be more than current height.";

        CBitcoinAddress address(params[5].get_str());
        if (!address.IsValid())
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Dash address");

        // Parse Dash address
        CScript scriptPubKey = GetScriptForDestination(address.Get());
        CAmount nAmount = AmountFromValue(params[6]);

        //*************************************************************************

        // create transaction 15 minutes into the future, to allow for confirmation time
        CGovernanceObjectBroadcast budgetProposalBroadcast;
        budgetProposalBroadcast.CreateProposal(strName, strURL, nPaymentCount, scriptPubKey, nAmount, nBlockStart, uint256());

        std::string strError = "";
        if(!budgetProposalBroadcast.IsValid(pindex, strError, false))
            return "Proposal is not valid - " + budgetProposalBroadcast.GetHash().ToString() + " - " + strError;

        bool useIX = false; //true;
        // if (params.size() > 7) {
        //     if(params[7].get_str() != "false" && params[7].get_str() != "true")
        //         return "Invalid use_ix, must be true or false";
        //     useIX = params[7].get_str() == "true" ? true : false;
        // }

        CWalletTx wtx;
        if(!pwalletMain->GetBudgetSystemCollateralTX(wtx, budgetProposalBroadcast.GetHash(), useIX)){
            return "Error making collateral transaction for proposal. Please check your wallet balance and make sure your wallet is unlocked.";
        }

        // make our change address
        CReserveKey reservekey(pwalletMain);
        //send the tx to the network
        pwalletMain->CommitTransaction(wtx, reservekey, useIX ? NetMsgType::IX : NetMsgType::TX);

        return wtx.GetHash().ToString();
    }

    if(strCommand == "submit")
    {
        if (params.size() != 8)
            throw runtime_error("Correct usage is 'proposal submit <proposal-name> <url> <payment-count> <block-start> <dash-address> <monthly-payment-dash> <fee-tx>'");

        if(!masternodeSync.IsBlockchainSynced()){
            return "Must wait for client to sync with masternode network. Try again in a minute or so.";
        }

        int nBlockMin = 0;
        LOCK(cs_main);
        CBlockIndex* pindex = chainActive.Tip();

        std::vector<CMasternodeConfig::CMasternodeEntry> mnEntries;
        mnEntries = masternodeConfig.getEntries();

        std::string strName = SanitizeString(params[1].get_str());
        std::string strURL = SanitizeString(params[2].get_str());
        int nPaymentCount = params[3].get_int();
        int nBlockStart = params[4].get_int();

        //set block min
        if(pindex != NULL) nBlockMin = pindex->nHeight;

        if(nBlockStart < nBlockMin)
            return "Invalid payment count, must be more than current height.";

        CBitcoinAddress address(params[5].get_str());
        if (!address.IsValid())
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Dash address");

        // Parse Dash address
        CScript scriptPubKey = GetScriptForDestination(address.Get());
        CAmount nAmount = AmountFromValue(params[6]);
        uint256 hash = ParseHashV(params[7], "Proposal hash");

        //create the proposal incase we're the first to make it
        CGovernanceObjectBroadcast budgetProposalBroadcast;
        budgetProposalBroadcast.CreateProposal(strName, strURL, nPaymentCount, scriptPubKey, nAmount, nBlockStart, hash);

        std::string strError = "";

        if(!budgetProposalBroadcast.IsValid(pindex, strError)){
            return "Proposal is not valid - " + budgetProposalBroadcast.GetHash().ToString() + " - " + strError;
        }

        int nConf = 0;
        if(!IsBudgetCollateralValid(hash, budgetProposalBroadcast.GetHash(), strError, budgetProposalBroadcast.nTime, nConf)){
            return "Proposal FeeTX is not valid - " + hash.ToString() + " - " + strError;
        }

        governance.mapSeenGovernanceObjects.insert(make_pair(budgetProposalBroadcast.GetHash(), budgetProposalBroadcast));
        budgetProposalBroadcast.Relay();
        governance.AddGovernanceObject(budgetProposalBroadcast);

        return budgetProposalBroadcast.GetHash().ToString();

    }

    if(strCommand == "list")
    {
        if (params.size() > 2)
            throw runtime_error("Correct usage is 'proposal list [valid|all|extended]'");

        std::string strShow = "valid";
        if (params.size() == 2) strShow = params[1].get_str();

        CBlockIndex* pindex;
        {
            LOCK(cs_main);
            pindex = chainActive.Tip();
        }

        UniValue resultObj(UniValue::VOBJ);
        int64_t nTotalAllotted = 0;

        std::vector<CGovernanceObject*> winningProps = governance.FindMatchingGovernanceObjects(Proposal);
        BOOST_FOREACH(CGovernanceObject* pbudgetProposal, winningProps)
        {
            if(strShow == "valid" && !pbudgetProposal->fValid) continue;

            nTotalAllotted += pbudgetProposal->GetAllotted();

            CTxDestination address1;
            ExtractDestination(pbudgetProposal->GetPayee(), address1);
            CBitcoinAddress address2(address1);

            UniValue bObj(UniValue::VOBJ);
            bObj.push_back(Pair("Name",  pbudgetProposal->GetName()));

            if(strShow == "extended") bObj.push_back(Pair("URL",  pbudgetProposal->GetURL()));
            bObj.push_back(Pair("Hash",  pbudgetProposal->GetHash().ToString()));

            if(strShow == "extended")
            {
                bObj.push_back(Pair("FeeHash",  pbudgetProposal->nFeeTXHash.ToString()));
                bObj.push_back(Pair("BlockStart",  (int64_t)pbudgetProposal->GetBlockStart()));
                bObj.push_back(Pair("BlockEnd",    (int64_t)pbudgetProposal->GetBlockEnd()));
                bObj.push_back(Pair("TotalPaymentCount",  (int64_t)pbudgetProposal->GetTotalPaymentCount()));
                bObj.push_back(Pair("RemainingPaymentCount",  (int64_t)pbudgetProposal->GetRemainingPaymentCount(pindex->nHeight)));
                bObj.push_back(Pair("PaymentAddress",   address2.ToString()));
                bObj.push_back(Pair("Ratio",  pbudgetProposal->GetRatio()));
            }
        
            bObj.push_back(Pair("AbsoluteYesCount",  (int64_t)pbudgetProposal->GetYesCount()-(int64_t)pbudgetProposal->GetNoCount()));
            bObj.push_back(Pair("YesCount",  (int64_t)pbudgetProposal->GetYesCount()));
            bObj.push_back(Pair("NoCount",  (int64_t)pbudgetProposal->GetNoCount()));
            
            if(strShow == "extended")
            {
                bObj.push_back(Pair("AbstainCount",  (int64_t)pbudgetProposal->GetAbstainCount()));
                bObj.push_back(Pair("TotalPayment",  ValueFromAmount(pbudgetProposal->GetAmount()*pbudgetProposal->GetTotalPaymentCount())));
            }

            bObj.push_back(Pair("MonthlyPayment",  ValueFromAmount(pbudgetProposal->GetAmount())));

            if(strShow == "extended")
            {
                bObj.push_back(Pair("IsEstablished",  pbudgetProposal->IsEstablished()));

                std::string strError = "";
                bObj.push_back(Pair("IsValid",  pbudgetProposal->IsValid(pindex, strError)));
                bObj.push_back(Pair("IsValidReason",  strError.c_str()));
                bObj.push_back(Pair("fValid",  pbudgetProposal->fValid));
            }

            resultObj.push_back(Pair(pbudgetProposal->GetName(), bObj));
        }

        return resultObj;
    }

    if(strCommand == "get")
    {
        if (params.size() != 2)
            throw runtime_error("Correct usage is 'proposal get <proposal-hash>'");

        uint256 hash = ParseHashV(params[1], "Proposal hash");

        CGovernanceObject* pbudgetProposal = governance.FindGovernanceObject(hash);

        if(pbudgetProposal == NULL) return "Unknown proposal";

        CBlockIndex* pindex;
        {
            LOCK(cs_main);
            pindex = chainActive.Tip();
        }

        CTxDestination address1;
        ExtractDestination(pbudgetProposal->GetPayee(), address1);
        CBitcoinAddress address2(address1);

        LOCK(cs_main);
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("Name",  pbudgetProposal->GetName()));
        obj.push_back(Pair("Hash",  pbudgetProposal->GetHash().ToString()));
        obj.push_back(Pair("FeeHash",  pbudgetProposal->nFeeTXHash.ToString()));
        obj.push_back(Pair("URL",  pbudgetProposal->GetURL()));
        obj.push_back(Pair("BlockStart",  (int64_t)pbudgetProposal->GetBlockStart()));
        obj.push_back(Pair("BlockEnd",    (int64_t)pbudgetProposal->GetBlockEnd()));
        obj.push_back(Pair("TotalPaymentCount",  (int64_t)pbudgetProposal->GetTotalPaymentCount()));
        obj.push_back(Pair("RemainingPaymentCount",  (int64_t)pbudgetProposal->GetRemainingPaymentCount(pindex->nHeight)));
        obj.push_back(Pair("PaymentAddress",   address2.ToString()));
        obj.push_back(Pair("Ratio",  pbudgetProposal->GetRatio()));
        obj.push_back(Pair("AbsoluteYesCount",  (int64_t)pbudgetProposal->GetYesCount()-(int64_t)pbudgetProposal->GetNoCount()));
        obj.push_back(Pair("YesCount",  (int64_t)pbudgetProposal->GetYesCount()));
        obj.push_back(Pair("NoCount",  (int64_t)pbudgetProposal->GetNoCount()));
        obj.push_back(Pair("AbstainCount",  (int64_t)pbudgetProposal->GetAbstainCount()));
        obj.push_back(Pair("TotalPayment",  ValueFromAmount(pbudgetProposal->GetAmount()*pbudgetProposal->GetTotalPaymentCount())));
        obj.push_back(Pair("MonthlyPayment",  ValueFromAmount(pbudgetProposal->GetAmount())));
        
        obj.push_back(Pair("IsEstablished",  pbudgetProposal->IsEstablished()));

        std::string strError = "";
        obj.push_back(Pair("IsValid",  pbudgetProposal->IsValid(chainActive.Tip(), strError)));
        obj.push_back(Pair("fValid",  pbudgetProposal->fValid));

        return obj;
    }


    if(strCommand == "gethash")
    {
        if (params.size() != 2)
            throw runtime_error("Correct usage is 'proposal gethash <proposal-name>'");

        std::string strName = SanitizeString(params[1].get_str());

        CGovernanceObject* pbudgetProposal = governance.FindGovernanceObject(strName);

        if(pbudgetProposal == NULL) return "Unknown proposal";

        UniValue resultObj(UniValue::VOBJ);

        std::vector<CGovernanceObject*> winningProps = governance.FindMatchingGovernanceObjects(Proposal);
        BOOST_FOREACH(CGovernanceObject* pbudgetProposal, winningProps)
        {
            if(pbudgetProposal->GetName() != strName) continue;
            if(!pbudgetProposal->fValid) continue;

            CTxDestination address1;
            ExtractDestination(pbudgetProposal->GetPayee(), address1);
            CBitcoinAddress address2(address1);

            resultObj.push_back(Pair(pbudgetProposal->GetHash().ToString(), pbudgetProposal->GetHash().ToString()));
        }

        if(resultObj.size() > 1) return resultObj;

        return pbudgetProposal->GetHash().ToString();
    }

    return NullUniValue;
}

UniValue contract(const UniValue& params, bool fHelp)
{
    string strCommand;
    if (params.size() >= 1)
        strCommand = params[0].get_str();

    if (fHelp  ||
        (strCommand != "prepare" && strCommand != "submit" &&
         strCommand != "get" && strCommand != "gethash" && strCommand != "list"))
        throw runtime_error(
                "contract \"command\"...\n"
                "Manage contracts\n"
                "\nAvailable commands:\n"
                "  prepare            - Prepare contract by signing and creating tx\n"
                "  submit             - Submit contract to network\n"
                "  list               - List all contracts\n"
                "  get                - get contract\n"
                "  gethash            - Get contract hash(es) by contract name\n"
                );

    if(strCommand == "prepare")
    {
        if (params.size() != 7)
            throw runtime_error("Correct usage is 'contract prepare <contract-name> <url> <month-count> <block-start> <dash-address> <monthly-payment-dash>'");

        int nBlockMin = 0;
        LOCK(cs_main);
        CBlockIndex* pindex = chainActive.Tip();

        std::vector<CMasternodeConfig::CMasternodeEntry> mnEntries;
        mnEntries = masternodeConfig.getEntries();

        std::string strName = SanitizeString(params[1].get_str());
        std::string strURL = SanitizeString(params[2].get_str());
        int nMonthCount = params[3].get_int();
        int nBlockStart = params[4].get_int();

        //set block min
        if(pindex != NULL) nBlockMin = pindex->nHeight;

        if(nBlockStart < nBlockMin)
            return "Invalid block start, must be more than current height.";

        CBitcoinAddress address(params[5].get_str());
        if (!address.IsValid())
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Dash address");

        // Parse Dash address
        CScript scriptPubKey = GetScriptForDestination(address.Get());
        CAmount nAmount = AmountFromValue(params[6]);

        //*************************************************************************

        // create transaction 15 minutes into the future, to allow for confirmation time
        CGovernanceObjectBroadcast contract;
        contract.CreateContract(strName, strURL, nMonthCount, scriptPubKey, nAmount, nBlockStart, uint256());

        std::string strError = "";
        if(!contract.IsValid(pindex, strError, false))
            return "Contract is not valid - " + contract.GetHash().ToString() + " - " + strError;

        CWalletTx wtx;
        if(!pwalletMain->GetBudgetSystemCollateralTX(wtx, contract.GetHash(), false)){
            return "Error making collateral transaction for proposal. Please check your wallet balance and make sure your wallet is unlocked.";
        }

        // make our change address
        CReserveKey reservekey(pwalletMain);
        //send the tx to the network
        pwalletMain->CommitTransaction(wtx, reservekey, NetMsgType::TX);

        return wtx.GetHash().ToString();
    }

    if(strCommand == "submit")
    {
        if (params.size() != 8)
            throw runtime_error("Correct usage is 'contract submit <contract-name> <url> <payment-count> <block-start> <dash-address> <monthly-payment-dash> <fee-tx>'");

        if(!masternodeSync.IsBlockchainSynced()){
            return "Must wait for client to sync with masternode network. Try again in a minute or so.";
        }

        int nBlockMin = 0;
        LOCK(cs_main);
        CBlockIndex* pindex = chainActive.Tip();

        std::vector<CMasternodeConfig::CMasternodeEntry> mnEntries;
        mnEntries = masternodeConfig.getEntries();

        std::string strName = SanitizeString(params[1].get_str());
        std::string strURL = SanitizeString(params[2].get_str());
        int nPaymentCount = params[3].get_int();
        int nBlockStart = params[4].get_int();

        //set block min
        if(pindex != NULL) nBlockMin = pindex->nHeight;

        if(nBlockStart < nBlockMin)
            return "Invalid payment count, must be more than current height.";

        CBitcoinAddress address(params[5].get_str());
        if (!address.IsValid())
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Dash address");

        // Parse Dash address
        CScript scriptPubKey = GetScriptForDestination(address.Get());
        CAmount nAmount = AmountFromValue(params[6]);
        uint256 hash = ParseHashV(params[7], "Contract hash");

        //create the contract incase we're the first to make it
        CGovernanceObjectBroadcast contractBroadcast;
        contractBroadcast.CreateContract(strName, strURL, nPaymentCount, scriptPubKey, nAmount, nBlockStart, hash);

        std::string strError = "";

        if(!contractBroadcast.IsValid(pindex, strError)){
            return "Contract is not valid - " + contractBroadcast.GetHash().ToString() + " - " + strError;
        }

        int nConf = 0;
        if(!IsBudgetCollateralValid(hash, contractBroadcast.GetHash(), strError, contractBroadcast.nTime, nConf)){
            return "Contract FeeTX is not valid - " + hash.ToString() + " - " + strError;
        }

        governance.mapSeenGovernanceObjects.insert(make_pair(contractBroadcast.GetHash(), contractBroadcast));
        contractBroadcast.Relay();
        governance.AddGovernanceObject(contractBroadcast);

        return contractBroadcast.GetHash().ToString();

    }

    if(strCommand == "list")
    {
        if (params.size() > 2)
            throw runtime_error("Correct usage is 'proposal list [valid]'");

        std::string strShow = "valid";
        if (params.size() == 2) strShow = params[1].get_str();

        CBlockIndex* pindex;
        {
            LOCK(cs_main);
            pindex = chainActive.Tip();
        }

        UniValue resultObj(UniValue::VOBJ);
        int64_t nTotalAllotted = 0;

        std::vector<CGovernanceObject*> winningProps = governance.FindMatchingGovernanceObjects(Proposal);
        BOOST_FOREACH(CGovernanceObject* pbudgetProposal, winningProps)
        {
            if(strShow == "valid" && !pbudgetProposal->fValid) continue;

            nTotalAllotted += pbudgetProposal->GetAllotted();

            CTxDestination address1;
            ExtractDestination(pbudgetProposal->GetPayee(), address1);
            CBitcoinAddress address2(address1);

            UniValue bObj(UniValue::VOBJ);
            bObj.push_back(Pair("Name",  pbudgetProposal->GetName()));
            bObj.push_back(Pair("URL",  pbudgetProposal->GetURL()));
            bObj.push_back(Pair("Hash",  pbudgetProposal->GetHash().ToString()));
            bObj.push_back(Pair("FeeHash",  pbudgetProposal->nFeeTXHash.ToString()));
            bObj.push_back(Pair("BlockStart",  (int64_t)pbudgetProposal->GetBlockStart()));
            bObj.push_back(Pair("BlockEnd",    (int64_t)pbudgetProposal->GetBlockEnd()));
            bObj.push_back(Pair("TotalPaymentCount",  (int64_t)pbudgetProposal->GetTotalPaymentCount()));
            bObj.push_back(Pair("RemainingPaymentCount",  (int64_t)pbudgetProposal->GetRemainingPaymentCount(pindex->nHeight)));
            bObj.push_back(Pair("PaymentAddress",   address2.ToString()));
            bObj.push_back(Pair("Ratio",  pbudgetProposal->GetRatio()));
            bObj.push_back(Pair("AbsoluteYesCount",  (int64_t)pbudgetProposal->GetYesCount()-(int64_t)pbudgetProposal->GetNoCount()));
            bObj.push_back(Pair("YesCount",  (int64_t)pbudgetProposal->GetYesCount()));
            bObj.push_back(Pair("NoCount",  (int64_t)pbudgetProposal->GetNoCount()));
            bObj.push_back(Pair("AbstainCount",  (int64_t)pbudgetProposal->GetAbstainCount()));
            bObj.push_back(Pair("TotalPayment",  ValueFromAmount(pbudgetProposal->GetAmount()*pbudgetProposal->GetTotalPaymentCount())));
            bObj.push_back(Pair("MonthlyPayment",  ValueFromAmount(pbudgetProposal->GetAmount())));

            bObj.push_back(Pair("IsEstablished",  pbudgetProposal->IsEstablished()));

            std::string strError = "";
            bObj.push_back(Pair("IsValid",  pbudgetProposal->IsValid(pindex, strError)));
            bObj.push_back(Pair("IsValidReason",  strError.c_str()));
            bObj.push_back(Pair("fValid",  pbudgetProposal->fValid));

            resultObj.push_back(Pair(pbudgetProposal->GetName(), bObj));
        }

        return resultObj;
    }

    if(strCommand == "get")
    {
        if (params.size() != 2)
            throw runtime_error("Correct usage is 'proposal get <proposal-hash>'");

        uint256 hash = ParseHashV(params[1], "Proposal hash");

        CGovernanceObject* pbudgetProposal = governance.FindGovernanceObject(hash);

        if(pbudgetProposal == NULL) return "Unknown proposal";

        CBlockIndex* pindex;
        {
            LOCK(cs_main);
            pindex = chainActive.Tip();
        }

        CTxDestination address1;
        ExtractDestination(pbudgetProposal->GetPayee(), address1);
        CBitcoinAddress address2(address1);

        LOCK(cs_main);
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("Name",  pbudgetProposal->GetName()));
        obj.push_back(Pair("Hash",  pbudgetProposal->GetHash().ToString()));
        obj.push_back(Pair("FeeHash",  pbudgetProposal->nFeeTXHash.ToString()));
        obj.push_back(Pair("URL",  pbudgetProposal->GetURL()));
        obj.push_back(Pair("BlockStart",  (int64_t)pbudgetProposal->GetBlockStart()));
        obj.push_back(Pair("BlockEnd",    (int64_t)pbudgetProposal->GetBlockEnd()));
        obj.push_back(Pair("TotalPaymentCount",  (int64_t)pbudgetProposal->GetTotalPaymentCount()));
        obj.push_back(Pair("RemainingPaymentCount",  (int64_t)pbudgetProposal->GetRemainingPaymentCount(pindex->nHeight)));
        obj.push_back(Pair("PaymentAddress",   address2.ToString()));
        obj.push_back(Pair("Ratio",  pbudgetProposal->GetRatio()));
        obj.push_back(Pair("AbsoluteYesCount",  (int64_t)pbudgetProposal->GetYesCount()-(int64_t)pbudgetProposal->GetNoCount()));
        obj.push_back(Pair("YesCount",  (int64_t)pbudgetProposal->GetYesCount()));
        obj.push_back(Pair("NoCount",  (int64_t)pbudgetProposal->GetNoCount()));
        obj.push_back(Pair("AbstainCount",  (int64_t)pbudgetProposal->GetAbstainCount()));
        obj.push_back(Pair("TotalPayment",  ValueFromAmount(pbudgetProposal->GetAmount()*pbudgetProposal->GetTotalPaymentCount())));
        obj.push_back(Pair("MonthlyPayment",  ValueFromAmount(pbudgetProposal->GetAmount())));
        
        obj.push_back(Pair("IsEstablished",  pbudgetProposal->IsEstablished()));

        std::string strError = "";
        obj.push_back(Pair("IsValid",  pbudgetProposal->IsValid(chainActive.Tip(), strError)));
        obj.push_back(Pair("fValid",  pbudgetProposal->fValid));

        return obj;
    }


    if(strCommand == "gethash")
    {
        if (params.size() != 2)
            throw runtime_error("Correct usage is 'proposal gethash <proposal-name>'");

        std::string strName = SanitizeString(params[1].get_str());

        CGovernanceObject* pbudgetProposal = governance.FindGovernanceObject(strName);

        if(pbudgetProposal == NULL) return "Unknown proposal";

        UniValue resultObj(UniValue::VOBJ);

        std::vector<CGovernanceObject*> winningProps = governance.FindMatchingGovernanceObjects(Proposal);
        BOOST_FOREACH(CGovernanceObject* pbudgetProposal, winningProps)
        {
            if(pbudgetProposal->GetName() != strName) continue;
            if(!pbudgetProposal->fValid) continue;

            CTxDestination address1;
            ExtractDestination(pbudgetProposal->GetPayee(), address1);
            CBitcoinAddress address2(address1);

            resultObj.push_back(Pair(pbudgetProposal->GetHash().ToString(), pbudgetProposal->GetHash().ToString()));
        }

        if(resultObj.size() > 1) return resultObj;

        return pbudgetProposal->GetHash().ToString();
    }

    return NullUniValue;
}

UniValue settings(const UniValue& params, bool fHelp)
{
    string strCommand;
    if (params.size() >= 1)
        strCommand = params[0].get_str();

    if (fHelp  ||
        (strCommand != "prepare" && strCommand != "submit" &&
         strCommand != "get" && strCommand != "gethash" && strCommand != "list"))
        throw runtime_error(
                "setting \"command\"...\n"
                "Manage contracts\n"
                "\nAvailable commands:\n"
                "  prepare            - Prepare setting by signing and creating tx\n"
                "  submit             - Submit setting to network\n"
                "  list               - List all contracts\n"
                "  get                - get setting\n"
                "  gethash            - Get setting hash(es) by setting name\n"
                "  show               - Show current setting values\n"
                );

    if(strCommand == "prepare")
    {
        if (params.size() != 7)
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

        // create transaction 15 minutes into the future, to allow for confirmation time
        CGovernanceObjectBroadcast settingBroadcast;
        settingBroadcast.CreateSetting(strName, strURL, strSuggestedValue, uint256());

        std::string strError = "";
        if(!settingBroadcast.IsValid(pindex, strError, false))
            return "Switch is not valid - " + settingBroadcast.GetHash().ToString() + " - " + strError;

        CWalletTx wtx;
        if(!pwalletMain->GetBudgetSystemCollateralTX(wtx, settingBroadcast.GetHash(), false)){
            return "Error making collateral transaction for proposal. Please check your wallet balance and make sure your wallet is unlocked.";
        }

        // make our change address
        CReserveKey reservekey(pwalletMain);
        //send the tx to the network
        pwalletMain->CommitTransaction(wtx, reservekey, NetMsgType::TX);

        return wtx.GetHash().ToString();
    }

    if(strCommand == "submit")
    {
        if (params.size() != 7)
            throw runtime_error("Correct usage is 'setting prepare <setting-name> <url> <suggested-value> <colateral-hash>'");

        if(!masternodeSync.IsBlockchainSynced()){
            return "Must wait for client to sync with masternode network. Try again in a minute or so.";
        }

        int nBlockMin = 0;
        LOCK(cs_main);
        CBlockIndex* pindex = chainActive.Tip();

        std::vector<CMasternodeConfig::CMasternodeEntry> mnEntries;
        mnEntries = masternodeConfig.getEntries();

        std::string strName = SanitizeString(params[1].get_str());
        std::string strURL = SanitizeString(params[2].get_str());
        std::string strSuggestedValue = SanitizeString(params[3].get_str());

        uint256 hash = ParseHashV(params[7], "Switch hash");

        //create the setting incase we're the first to make it
        CGovernanceObjectBroadcast settingBroadcast;
        settingBroadcast.CreateSetting(strName, strURL, strSuggestedValue, uint256());

        std::string strError = "";

        if(!settingBroadcast.IsValid(pindex, strError)){
            return "Switch is not valid - " + settingBroadcast.GetHash().ToString() + " - " + strError;
        }

        int nConf = 0;
        if(!IsBudgetCollateralValid(hash, settingBroadcast.GetHash(), strError, settingBroadcast.nTime, nConf)){
            return "Switch FeeTX is not valid - " + hash.ToString() + " - " + strError;
        }

        governance.mapSeenGovernanceObjects.insert(make_pair(settingBroadcast.GetHash(), settingBroadcast));
        settingBroadcast.Relay();
        governance.AddGovernanceObject(settingBroadcast);

        return settingBroadcast.GetHash().ToString();

    }

    if(strCommand == "list")
    {
        if (params.size() > 2)
            throw runtime_error("Correct usage is 'proposal list [valid]'");

        std::string strShow = "valid";
        if (params.size() == 2) strShow = params[1].get_str();

        CBlockIndex* pindex;
        {
            LOCK(cs_main);
            pindex = chainActive.Tip();
        }

        UniValue resultObj(UniValue::VOBJ);
        int64_t nTotalAllotted = 0;

        std::vector<CGovernanceObject*> winningProps = governance.FindMatchingGovernanceObjects(Proposal);
        BOOST_FOREACH(CGovernanceObject* pbudgetProposal, winningProps)
        {
            if(strShow == "valid" && !pbudgetProposal->fValid) continue;

            nTotalAllotted += pbudgetProposal->GetAllotted();

            CTxDestination address1;
            ExtractDestination(pbudgetProposal->GetPayee(), address1);
            CBitcoinAddress address2(address1);

            UniValue bObj(UniValue::VOBJ);
            bObj.push_back(Pair("Name",  pbudgetProposal->GetName()));
            bObj.push_back(Pair("URL",  pbudgetProposal->GetURL()));
            bObj.push_back(Pair("Hash",  pbudgetProposal->GetHash().ToString()));
            bObj.push_back(Pair("FeeHash",  pbudgetProposal->nFeeTXHash.ToString()));
            bObj.push_back(Pair("BlockStart",  (int64_t)pbudgetProposal->GetBlockStart()));
            bObj.push_back(Pair("BlockEnd",    (int64_t)pbudgetProposal->GetBlockEnd()));
            bObj.push_back(Pair("TotalPaymentCount",  (int64_t)pbudgetProposal->GetTotalPaymentCount()));
            bObj.push_back(Pair("RemainingPaymentCount",  (int64_t)pbudgetProposal->GetRemainingPaymentCount(pindex->nHeight)));
            bObj.push_back(Pair("PaymentAddress",   address2.ToString()));
            bObj.push_back(Pair("Ratio",  pbudgetProposal->GetRatio()));
            bObj.push_back(Pair("AbsoluteYesCount",  (int64_t)pbudgetProposal->GetYesCount()-(int64_t)pbudgetProposal->GetNoCount()));
            bObj.push_back(Pair("YesCount",  (int64_t)pbudgetProposal->GetYesCount()));
            bObj.push_back(Pair("NoCount",  (int64_t)pbudgetProposal->GetNoCount()));
            bObj.push_back(Pair("AbstainCount",  (int64_t)pbudgetProposal->GetAbstainCount()));
            bObj.push_back(Pair("TotalPayment",  ValueFromAmount(pbudgetProposal->GetAmount()*pbudgetProposal->GetTotalPaymentCount())));
            bObj.push_back(Pair("MonthlyPayment",  ValueFromAmount(pbudgetProposal->GetAmount())));

            bObj.push_back(Pair("IsEstablished",  pbudgetProposal->IsEstablished()));

            std::string strError = "";
            bObj.push_back(Pair("IsValid",  pbudgetProposal->IsValid(pindex, strError)));
            bObj.push_back(Pair("IsValidReason",  strError.c_str()));
            bObj.push_back(Pair("fValid",  pbudgetProposal->fValid));

            resultObj.push_back(Pair(pbudgetProposal->GetName(), bObj));
        }

        return resultObj;
    }

    if(strCommand == "get")
    {
        if (params.size() != 2)
            throw runtime_error("Correct usage is 'proposal get <proposal-hash>'");

        uint256 hash = ParseHashV(params[1], "Proposal hash");

        CGovernanceObject* pbudgetProposal = governance.FindGovernanceObject(hash);

        if(pbudgetProposal == NULL) return "Unknown proposal";

        CBlockIndex* pindex;
        {
            LOCK(cs_main);
            pindex = chainActive.Tip();
        }

        CTxDestination address1;
        ExtractDestination(pbudgetProposal->GetPayee(), address1);
        CBitcoinAddress address2(address1);

        LOCK(cs_main);
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("Name",  pbudgetProposal->GetName()));
        obj.push_back(Pair("Hash",  pbudgetProposal->GetHash().ToString()));
        obj.push_back(Pair("FeeHash",  pbudgetProposal->nFeeTXHash.ToString()));
        obj.push_back(Pair("URL",  pbudgetProposal->GetURL()));
        obj.push_back(Pair("BlockStart",  (int64_t)pbudgetProposal->GetBlockStart()));
        obj.push_back(Pair("BlockEnd",    (int64_t)pbudgetProposal->GetBlockEnd()));
        obj.push_back(Pair("TotalPaymentCount",  (int64_t)pbudgetProposal->GetTotalPaymentCount()));
        obj.push_back(Pair("RemainingPaymentCount",  (int64_t)pbudgetProposal->GetRemainingPaymentCount(pindex->nHeight)));
        obj.push_back(Pair("PaymentAddress",   address2.ToString()));
        obj.push_back(Pair("Ratio",  pbudgetProposal->GetRatio()));
        obj.push_back(Pair("AbsoluteYesCount",  (int64_t)pbudgetProposal->GetYesCount()-(int64_t)pbudgetProposal->GetNoCount()));
        obj.push_back(Pair("YesCount",  (int64_t)pbudgetProposal->GetYesCount()));
        obj.push_back(Pair("NoCount",  (int64_t)pbudgetProposal->GetNoCount()));
        obj.push_back(Pair("AbstainCount",  (int64_t)pbudgetProposal->GetAbstainCount()));
        obj.push_back(Pair("TotalPayment",  ValueFromAmount(pbudgetProposal->GetAmount()*pbudgetProposal->GetTotalPaymentCount())));
        obj.push_back(Pair("MonthlyPayment",  ValueFromAmount(pbudgetProposal->GetAmount())));
        
        obj.push_back(Pair("IsEstablished",  pbudgetProposal->IsEstablished()));

        std::string strError = "";
        obj.push_back(Pair("IsValid",  pbudgetProposal->IsValid(chainActive.Tip(), strError)));
        obj.push_back(Pair("fValid",  pbudgetProposal->fValid));

        return obj;
    }


    if(strCommand == "gethash")
    {
        if (params.size() != 2)
            throw runtime_error("Correct usage is 'proposal gethash <proposal-name>'");

        std::string strName = SanitizeString(params[1].get_str());

        CGovernanceObject* pbudgetProposal = governance.FindGovernanceObject(strName);

        if(pbudgetProposal == NULL) return "Unknown proposal";

        UniValue resultObj(UniValue::VOBJ);

        std::vector<CGovernanceObject*> winningProps = governance.FindMatchingGovernanceObjects(Proposal);
        BOOST_FOREACH(CGovernanceObject* pbudgetProposal, winningProps)
        {
            if(pbudgetProposal->GetName() != strName) continue;
            if(!pbudgetProposal->fValid) continue;

            CTxDestination address1;
            ExtractDestination(pbudgetProposal->GetPayee(), address1);
            CBitcoinAddress address2(address1);

            resultObj.push_back(Pair(pbudgetProposal->GetHash().ToString(), pbudgetProposal->GetHash().ToString()));
        }

        if(resultObj.size() > 1) return resultObj;

        return pbudgetProposal->GetHash().ToString();
    }

    if(strCommand == "show")
    {

    }


    return NullUniValue;
}

UniValue budget(const UniValue& params, bool fHelp)
{
    string strCommand;
    if (params.size() >= 1)
        strCommand = params[0].get_str();

    if (fHelp  ||
        (strCommand != "check" && strCommand != "get" && strCommand != "all" && 
            strCommand != "valid" && strCommand != "extended" && strCommand != "projection"))
        throw runtime_error(
                "budget \"command\"...\n"
                "Manage proposals\n"
                "\nAvailable commands:\n"
                "  check              - Scan proposals and remove invalid from proposals list\n"
                "  get                - Get a proposals|contract by hash\n"
                "  all                - Get all proposals\n"
                "  valid              - Get only valid proposals\n"
                "  extended           - Get all proposals in extended form\n"
                "  projection         - Show the projection of which proposals will be paid the next superblocks\n"
                );

    if(strCommand == "check")
    {
        governance.CheckAndRemove();

        return "Success";
    }

    if(strCommand == "projection")
    {
        CBlockIndex* pindex;
        {
            LOCK(cs_main);
            pindex = chainActive.Tip();
        }

        UniValue resultObj(UniValue::VOBJ);
        CAmount nTotalAllotted = 0;

        std::vector<CGovernanceObject*> winningProps = governance.GetBudget();
        BOOST_FOREACH(CGovernanceObject* pbudgetProposal, winningProps)
        {
            nTotalAllotted += pbudgetProposal->GetAllotted();

            CTxDestination address1;
            ExtractDestination(pbudgetProposal->GetPayee(), address1);
            CBitcoinAddress address2(address1);

            UniValue bObj(UniValue::VOBJ);
            bObj.push_back(Pair("URL",  pbudgetProposal->GetURL()));
            bObj.push_back(Pair("Hash",  pbudgetProposal->GetHash().ToString()));
            bObj.push_back(Pair("BlockStart",  (int64_t)pbudgetProposal->GetBlockStart()));
            bObj.push_back(Pair("BlockEnd",    (int64_t)pbudgetProposal->GetBlockEnd()));
            bObj.push_back(Pair("TotalPaymentCount",  (int64_t)pbudgetProposal->GetTotalPaymentCount()));
            bObj.push_back(Pair("RemainingPaymentCount",  (int64_t)pbudgetProposal->GetRemainingPaymentCount(pindex->nHeight)));
            bObj.push_back(Pair("PaymentAddress",   address2.ToString()));
            bObj.push_back(Pair("Ratio",  pbudgetProposal->GetRatio()));
            bObj.push_back(Pair("AbsoluteYesCount",  (int64_t)pbudgetProposal->GetYesCount()-(int64_t)pbudgetProposal->GetNoCount()));
            bObj.push_back(Pair("YesCount",  (int64_t)pbudgetProposal->GetYesCount()));
            bObj.push_back(Pair("NoCount",  (int64_t)pbudgetProposal->GetNoCount()));
            bObj.push_back(Pair("AbstainCount",  (int64_t)pbudgetProposal->GetAbstainCount()));
            bObj.push_back(Pair("TotalPayment",  ValueFromAmount(pbudgetProposal->GetAmount()*pbudgetProposal->GetTotalPaymentCount())));
            bObj.push_back(Pair("MonthlyPayment",  ValueFromAmount(pbudgetProposal->GetAmount())));
            bObj.push_back(Pair("Alloted",  ValueFromAmount(pbudgetProposal->GetAllotted())));

            std::string strError = "";
            bObj.push_back(Pair("IsValid",  pbudgetProposal->IsValid(pindex, strError)));
            bObj.push_back(Pair("IsValidReason",  strError.c_str()));
            bObj.push_back(Pair("fValid",  pbudgetProposal->fValid));

            resultObj.push_back(Pair(pbudgetProposal->GetName(), bObj));
        }
        resultObj.push_back(Pair("TotalBudgetAlloted",  ValueFromAmount(nTotalAllotted)));

        return resultObj;
    }

    if(strCommand == "all" || strCommand == "valid" || strCommand == "extended" || strCommand == "get")
    {
        uint256 nMatchHash = uint256();
        std::string strMatchName = "";
        bool fMissing = true;

        if(strCommand == "get" && params.size() == 2)
        {

            if (IsHex(params[1].get_str()))
            {
                nMatchHash = ParseHashV(params[1], "GovObj hash");
            } else {
                strMatchName = params[1].get_str();
            }
            fMissing = false;
        }

        // no command options
        if(strCommand == "all" || strCommand == "valid" || strCommand == "extended") fMissing = false;

        if(fMissing)
        {
            throw runtime_error(
                    "budget (all|valid|extended|get)"
                    "Show budget items in various ways\n"
                    "\nAvailable commands:\n"
                    "  all              - Scan proposals and remove invalid from proposals list\n"
                    "  valid                - Get a proposals|contract by hash\n"
                    "  all                - Get all proposals\n"
                    "  valid              - Get only valid proposals\n"
                    "  extended           - Get all proposals in extended form\n"
                    "  projection         - Show the projection of which proposals will be paid the next superblocks\n"
                    );
        }

        CBlockIndex* pindex;
        {
            LOCK(cs_main);
            pindex = chainActive.Tip();
        }

        UniValue resultObj(UniValue::VOBJ);
        int64_t nTotalAllotted = 0;

        std::vector<CGovernanceObject*> winningProps = governance.FindMatchingGovernanceObjects(Proposal);
        
        BOOST_FOREACH(CGovernanceObject* pbudgetProposal, winningProps)
        {
            if(strCommand == "valid" && !pbudgetProposal->fValid) continue;

            nTotalAllotted += pbudgetProposal->GetAllotted();

            CTxDestination address1;
            ExtractDestination(pbudgetProposal->GetPayee(), address1);
            CBitcoinAddress address2(address1);

            UniValue bObj(UniValue::VOBJ);
            bObj.push_back(Pair("Name",  pbudgetProposal->GetName()));
            
            if(strCommand == "extended")
            {
                bObj.push_back(Pair("URL",  pbudgetProposal->GetURL()));
                bObj.push_back(Pair("Hash",  pbudgetProposal->GetHash().ToString()));
                bObj.push_back(Pair("FeeHash",  pbudgetProposal->nFeeTXHash.ToString()));
                bObj.push_back(Pair("BlockStart",  (int64_t)pbudgetProposal->GetBlockStart()));
                bObj.push_back(Pair("BlockEnd",    (int64_t)pbudgetProposal->GetBlockEnd()));
                bObj.push_back(Pair("TotalPaymentCount",  (int64_t)pbudgetProposal->GetTotalPaymentCount()));
                bObj.push_back(Pair("RemainingPaymentCount",  (int64_t)pbudgetProposal->GetRemainingPaymentCount(pindex->nHeight)));
                bObj.push_back(Pair("PaymentAddress",   address2.ToString()));
                bObj.push_back(Pair("Ratio",  pbudgetProposal->GetRatio()));
            }

            bObj.push_back(Pair("AbsoluteYesCount",  (int64_t)pbudgetProposal->GetYesCount()-(int64_t)pbudgetProposal->GetNoCount()));
            bObj.push_back(Pair("YesCount",  (int64_t)pbudgetProposal->GetYesCount()));
            bObj.push_back(Pair("NoCount",  (int64_t)pbudgetProposal->GetNoCount()));
            
            if(strCommand == "extended")
            {
                bObj.push_back(Pair("AbstainCount",  (int64_t)pbudgetProposal->GetAbstainCount()));
                bObj.push_back(Pair("TotalPayment",  ValueFromAmount(pbudgetProposal->GetAmount()*pbudgetProposal->GetTotalPaymentCount())));
            }

            bObj.push_back(Pair("MonthlyPayment",  ValueFromAmount(pbudgetProposal->GetAmount())));
            bObj.push_back(Pair("IsEstablished",  pbudgetProposal->IsEstablished()));

            if(strCommand == "extended")
            {
                std::string strError = "";
                bObj.push_back(Pair("IsValid",  pbudgetProposal->IsValid(pindex, strError)));
                bObj.push_back(Pair("IsValidReason",  strError.c_str()));
                bObj.push_back(Pair("fValid",  pbudgetProposal->fValid));
            }

            resultObj.push_back(Pair(pbudgetProposal->GetName(), bObj));
        }

        return resultObj;
    }

    return NullUniValue;
}

UniValue superblock(const UniValue& params, bool fHelp)
{
    string strCommand;
    if (params.size() >= 1)
        strCommand = params[0].get_str();

    if (fHelp  ||
        (strCommand != "vote-many" && strCommand != "vote" && strCommand != "show" && strCommand != "getvotes" && strCommand != "prepare" && strCommand != "submit"))
        throw runtime_error(
                "superblock \"command\"...\n"
                "Get information about the next superblocks\n"
                "\nAvailable commands:\n"
                "  info   - Get info about the next superblock\n"
                "  getvotes    - Get vote information for each finalized budget\n"
                "  prepare     - Manually prepare a finalized budget\n"
                "  submit      - Manually submit a finalized budget\n"
                );


    if(strCommand == "nextblock")
    {
        LOCK(cs_main);
        CBlockIndex* pindex = chainActive.Tip();
        if(!pindex) return "unknown";

        int nNext = pindex->nHeight - pindex->nHeight % Params().GetConsensus().nBudgetPaymentsCycleBlocks + Params().GetConsensus().nBudgetPaymentsCycleBlocks;
        return nNext;
    }

    if(strCommand == "nextsuperblocksize")
    {
        LOCK(cs_main);
        CBlockIndex* pindex = chainActive.Tip();
        if(!pindex) return "unknown";

        int nHeight = pindex->nHeight - pindex->nHeight % Params().GetConsensus().nBudgetPaymentsCycleBlocks + Params().GetConsensus().nBudgetPaymentsCycleBlocks;

        CAmount nTotal = governance.GetTotalBudget(nHeight);
        return nTotal;
    }

    if(strCommand == "get")
    {
        UniValue resultObj(UniValue::VOBJ);

        std::vector<CFinalizedBudget*> winningFbs = governance.GetFinalizedBudgets();
        LOCK(cs_main);
        BOOST_FOREACH(CFinalizedBudget* finalizedBudget, winningFbs)
        {
            UniValue bObj(UniValue::VOBJ);
            bObj.push_back(Pair("FeeTX",  finalizedBudget->nFeeTXHash.ToString()));
            bObj.push_back(Pair("Hash",  finalizedBudget->GetHash().ToString()));
            bObj.push_back(Pair("BlockStart",  (int64_t)finalizedBudget->GetBlockStart()));
            bObj.push_back(Pair("BlockEnd",    (int64_t)finalizedBudget->GetBlockEnd()));
            bObj.push_back(Pair("Proposals",  finalizedBudget->GetProposals()));
            bObj.push_back(Pair("VoteCount",  (int64_t)finalizedBudget->GetVoteCount()));
            bObj.push_back(Pair("Status",  finalizedBudget->GetStatus()));

            std::string strError = "";
            bObj.push_back(Pair("IsValid",  finalizedBudget->IsValid(chainActive.Tip(), strError)));
            bObj.push_back(Pair("IsValidReason",  strError.c_str()));

            resultObj.push_back(Pair(finalizedBudget->GetName(), bObj));
        }

        return resultObj;

    }

    /* TODO 
        Switch the preparation to a public key which the core team has
        The budget should be able to be created by any high up core team member then voted on by the network separately. 
    */

    if(strCommand == "prepare")
    {
        if (params.size() != 2)
            throw runtime_error("Correct usage is 'mnfinalbudget prepare comma-separated-hashes'");

        std::string strHashes = params[1].get_str();
        std::istringstream ss(strHashes);
        std::string token;

        std::vector<CTxBudgetPayment> vecTxBudgetPayments;

        while(std::getline(ss, token, ',')) {
            uint256 nHash = uint256S(token);
            CGovernanceObject* prop = governance.FindGovernanceObject(nHash);

            CTxBudgetPayment txBudgetPayment;
            txBudgetPayment.nProposalHash = prop->GetHash();
            txBudgetPayment.payee = prop->GetPayee();
            txBudgetPayment.nAmount = prop->GetAllotted();
            vecTxBudgetPayments.push_back(txBudgetPayment);
        }

        if(vecTxBudgetPayments.size() < 1) {
            return "Invalid finalized proposal";
        }

        LOCK(cs_main);
        CBlockIndex* pindex = chainActive.Tip();
        if(!pindex) return "invalid chaintip";

        int nBlockStart = pindex->nHeight - pindex->nHeight % Params().GetConsensus().nBudgetPaymentsCycleBlocks + Params().GetConsensus().nBudgetPaymentsCycleBlocks;

        CFinalizedBudgetBroadcast tempBudget("main", nBlockStart, vecTxBudgetPayments, uint256());
        // if(mapSeenFinalizedBudgets.count(tempBudget.GetHash())) {
        //     return "already exists"; //already exists
        // }

        //create fee tx
        CTransaction tx;
        
        CWalletTx wtx;
        if(!pwalletMain->GetBudgetSystemCollateralTX(wtx, tempBudget.GetHash(), false)){
            printf("Can't make collateral transaction\n");
            return "can't make colateral tx";
        }
        
        // make our change address
        CReserveKey reservekey(pwalletMain);
        //send the tx to the network
        pwalletMain->CommitTransaction(wtx, reservekey, NetMsgType::IX);

        return wtx.GetHash().ToString();
    }

    if(strCommand == "submit")
    {
        if (params.size() != 3)
            throw runtime_error("Correct usage is 'mnfinalbudget submit comma-separated-hashes collateralhash'");

        std::string strHashes = params[1].get_str();
        std::istringstream ss(strHashes);
        std::string token;

        std::vector<CTxBudgetPayment> vecTxBudgetPayments;

        uint256 nColHash = uint256S(params[2].get_str());

        while(std::getline(ss, token, ',')) {
            uint256 nHash = uint256S(token);
            CGovernanceObject* prop = governance.FindGovernanceObject(nHash);

            CTxBudgetPayment txBudgetPayment;
            txBudgetPayment.nProposalHash = prop->GetHash();
            txBudgetPayment.payee = prop->GetPayee();
            txBudgetPayment.nAmount = prop->GetAllotted();
            vecTxBudgetPayments.push_back(txBudgetPayment);

            printf("%lld\n", txBudgetPayment.nAmount);
        }

        if(vecTxBudgetPayments.size() < 1) {
            return "Invalid finalized proposal";
        }

        LOCK(cs_main);
        CBlockIndex* pindex = chainActive.Tip();
        if(!pindex) return "invalid chaintip";

        int nBlockStart = pindex->nHeight - pindex->nHeight % Params().GetConsensus().nBudgetPaymentsCycleBlocks + Params().GetConsensus().nBudgetPaymentsCycleBlocks;
      
        // CTxIn in(COutPoint(nColHash, 0));
        // int conf = GetInputAgeIX(nColHash, in);
        
        //     Wait will we have 1 extra confirmation, otherwise some clients might reject this feeTX
        //     -- This function is tied to NewBlock, so we will propagate this budget while the block is also propagating
        
        // if(conf < BUDGET_FEE_CONFIRMATIONS+1){
        //     printf ("Collateral requires at least %d confirmations - %s - %d confirmations\n", BUDGET_FEE_CONFIRMATIONS, nColHash.ToString().c_str(), conf);
        //     return "invalid collateral";
        // }

        //create the proposal incase we're the first to make it
        CFinalizedBudgetBroadcast finalizedBudgetBroadcast("main", nBlockStart, vecTxBudgetPayments, nColHash);

        std::string strError = "";
        if(!finalizedBudgetBroadcast.IsValid(pindex, strError)){
            printf("CGovernanceManager::SubmitFinalBudget - Invalid finalized budget - %s \n", strError.c_str());
            return "invalid finalized budget";
        }

        finalizedBudgetBroadcast.Relay();
        governance.AddFinalizedBudget(finalizedBudgetBroadcast);

        return finalizedBudgetBroadcast.GetHash().ToString();
    }

    return NullUniValue;
}
