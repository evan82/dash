#include <boost/lexical_cast.hpp>

void CAdminManager::ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    // lite mode is not supported
    if(fLiteMode) return;
    if(!masternodeSync.IsBlockchainSynced()) return;

    LOCK(cs_budget);

    // signatures are the first thing sent
    if (strCommand == NetMsgType::ADMIN_SIGNATURE) { //Network Administration Command SIGNATURE
        CAdminSignature sig;
        vRecv >> sig;

        if(mapSeenAdminSignatures.count(sig.GetHash())){
            //masternodeSync.AddedAdminItem(sig.GetHash());
            return;
        }

        // we should have the command
        if(adminman.HaveCommand(sig.GetParentHash())){

            std::string strError = "";

            CAdminCommand& cmd = CAdminManager.FindCommand(sig.GetParentHash());
            if(cmd)
            {
                if(!adminman.AddSignature(sig, strError))
                {
                    LogPrintf("admin", "CAdminManager::ProcessMessage :: Error %s\n", strError);
                } else if(cmd.Process())//check signatures and execution status
                {
                    cmd.Relay();
                }    
            }

            return;
        } else {
            LogPrintf("admin", "CAdminManager::ProcessMessage :: Missing command - %s, asked other node\n", cmd.ToString());
            adminman.RequestCommand(sig.GetParentHash());
        }
    }

    if (strCommand == NetMsgType::ADMIN_COMMAND) { //Network Administration Command
        CAdminCommand cmd;
        vRecv >> cmd;

        if(mapSeenAdminCommands.count(cmd.GetHash())){
            //masternodeSync.AddedAdminItem(budgetProposalBroadcast.GetHash());
            return;
        }

        if(!adminman.HaveCommand(cmd.nHash))
        {
            std::string strError = "";
            if(adminman.AddSignature(cmd, strError))
            {
                cmd.Relay();
            } else {
                LogPrintf("admin", "CAdminManager::ProcessMessage :: Error %s\n", strError);
            }
        }
    }

}

void CAdminCommand::Process()
{
    if(GetState() == NEW)
    {
        LogPrintf("admin", "CAdminManager::ProcessMessage :: Cmd %s - marked as NEW\n", ToString());
        return;
    }
    if(GetState() == AWAITING_SIGNATURES)
    {
        LogPrintf("admin", "CAdminManager::ProcessMessage :: Cmd %s waiting for more signatures - %d of 5\n", ToString(), SignatureCount());
        return;
    }

    if(GetState() == PENDING_EXECUTION)
    {
        Execute();
        LogPrintf("admin", "CAdminManager::ProcessMessage :: Executed cmd %s\n", ToString());
        return;
    }
    if(GetState() == EXECUTED)
    {
        Execute();
        LogPrintf("admin", "CAdminManager::ProcessMessage :: Command Already Executed - %s\n", ToString());
        return;
    }
}

bool CAdminManager::CheckSignature(CAdminCommand& cmd)
{
    //note: need to investigate why this is failing
    std::string strMessage = boost::lexical_cast<std::string>(cmd.nHash) + boost::lexical_cast<std::string>(cmd.nValue) + boost::lexical_cast<std::string>(cmd.nTimeSigned);
    CPubKey pubkey(ParseHex(Params().cmdKey()));

    std::string errorMessage = "";
    if(!darkSendSigner.VerifyMessage(pubkey, cmd.vchSig, strMessage, errorMessage)){
        return false;
    }

    return true;
}

bool CAdminManager::Sign(CAdminCommand& cmd)
{
    std::string strMessage = boost::lexical_cast<std::string>(cmd.nHash) + boost::lexical_cast<std::string>(cmd.nValue) + boost::lexical_cast<std::string>(cmd.nTimeSigned);

    CKey key2;
    CPubKey pubkey2;
    std::string errorMessage = "";

    if(!darkSendSigner.SetKey(strMasterPrivKey, errorMessage, key2, pubkey2))
    {
        LogPrintf("CMasternodePayments::Sign - ERROR: Invalid masternodeprivkey: '%s'\n", errorMessage);
        return false;
    }

    if(!darkSendSigner.SignMessage(strMessage, errorMessage, cmd.vchSig, key2)) {
        LogPrintf("CMasternodePayments::Sign - Sign message failed");
        return false;
    }

    if(!darkSendSigner.VerifyMessage(pubkey2, cmd.vchSig, strMessage, errorMessage)) {
        LogPrintf("CMasternodePayments::Sign - Verify message failed");
        return false;
    }

    return true;
}


bool CNetworkAdministration::CreateNewSetting(CAdminCommand& cmd, std::string& strError)
{
    if(cmd.GetState() != PENDING_EXECUTION)
    {
        strError = "Incorrect state.";
        return false;
    }

    // restore command parameters from char vector 
    CDataStream vRecv(cmd.GetCommand());

    std::string strSettingName;
    std::string strValueType;
    std::string strValue;
    vRecv >> strSettingName;
    vRecv >> strValueType;
    vRecv >> strValue;

    if(mapNetworkMemory.count(strSettingName))
    {
        strError = "Setting " + strSettingName + " already exists!";
        return false;
    }

    if(!adminman.IsValueParsable(strValueType, strValue))
    {
        strError = "Value not parsable. Type is " + strValueType + ", value=" + strValue;
        return false;
    }

    mapNetworkMemory.insert(make_pair(strSettingName, strValue));
    mapNetworkMemory.insert(make_pair(strSettingName+".type", strValueType));
}

bool CNetworkAdministration::LoadDefaultSettings(CAdminCommand& cmd, std::string& strError)
{
    /*
        Last Updated : March 29, 2016
    */

    /*
        Name: fee-per-kb
        Desc: We want to be able to set a flat fee for the network. Target a specific dollar amount.        
        Value: 102380

        ----
        102380 satoshi per KB, with 2.1kb average tx size.
        215000 average tx cost, $0.015 @ $7.20
    */

    int price = 7; //round to closet int

    mapNetworkMemory.insert(make_pair("fee-per-kb", "102380"));
    mapNetworkMemory.insert(make_pair("fee-per-kb.type", "int"));

    /*
        Name: proposal-fee
        Desc: Target $25. Overpaid fees remain valid. In satoshis.       
        Value: 3
    */

    mapNetworkMemory.insert(make_pair("proposal-fee", boost::lexical_cast<std::string>((25/price)*COIN)));
    mapNetworkMemory.insert(make_pair("proposal-fee.type", "int"));
    /*
        Name: contract-fee
        Desc: Target $75. Overpaid fees remain valid. In satoshis.       
        Value: 10
    */

    mapNetworkMemory.insert(make_pair("contract-fee", boost::lexical_cast<std::string>((75/price)*COIN)));
    mapNetworkMemory.insert(make_pair("contract-fee.type", "int"));
}

bool CNetworkAdministration::UpdateSetting(CAdminCommand& cmd, std::string& strError)
{
    if(cmd.GetState() != PENDING_EXECUTION)
    {
        strError = "Incorrect state.";
        return false;
    }

    // restore command parameters from char vector 
    CDataStream vRecv(cmd.GetCommand());

    std::string strSettingName;
    std::string strValue;
    vRecv >> strSettingName;
    vRecv >> strValue;

    if(!mapNetworkMemory.count(strSettingName))
    {
        strError = "Setting " + strSettingName + " doesn't exist!";
        return false;
    }

    if(!adminman.IsValueParsable(mapNetworkMemory[strSettingName+"-value"], strValue))
    {
        strError = "Value not parsable. Type is " + strValueType + ", value=" + strValue;
        return false;
    }

    mapNetworkMemory[strSettingName] = strValue;
}

int CNetworkAdministration::GetSetting(std::string strSettingName)
{
    if(!mapNetworkMemory.count(strSettingName))
    {
        return GetDefaultSetting(strSettingName);
    }

    nValue = boost::lexical_cast<int>( mapNetworkMemory[strSettingName] );
    return true;
}

int CNetworkAdministration::GetDefaultSetting(std::string strSettingName)
{
    if(strSettingName == "fee-per-kb") return 1250; //1250 satoshi's per kb
    if(strSettingName == "proposal-fee") return 5*COIN; //5 DASH per proposal
    if(strSettingName == "contract-fee") return 5*COIN; //5 DASH per contract
}

CAdminCommand& CAdminManager::FindCommand(uint256 nHash)
{
    LOCK(cs);

    if(mapSeenAdminCommands.count(nHash))
        return &mapSeenAdminCommands[nHash];
    
    return NULL;   
}

bool CAdminManager::HaveCommand(uint256 nHash)
{
    return mapSeenAdminCommands.count(nHash);
}

bool CAdminManager::AddSignature(CAdminSignature& sig, std::string& strError)
{
    if(!HaveCommand(sig.GetParentHash)) return false;
    return mapSeenAdminCommands[sig.GetParentHash()].AddSignature(sig, strError);
}

bool CAdminManager::RequestCommand(uint256 nHash)
{
    if()
}


