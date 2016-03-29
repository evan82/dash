

// 3 of 5 multisig is required for any action
#define NETWORK_ADMIN_KEYS_COUNT = 5
#define NETWORK_ADMIN_KEYS_REQUIRED = 3 //to execute a command
// those encumbered with responsability
#define NETWORK_ADMIN_KEY_EDUFFIELD = 0
#define NETWORK_ADMIN_KEY_DDIAZ = 1
#define NETWORK_ADMIN_KEY_UDJINM6 = 2
#define NETWORK_ADMIN_KEY_RTAYLOR = 3
#define NETWORK_ADMIN_KEY_RWIECKO = 4

enum NetworkCommandType {
    NONE = -1,
    UPDATE_SETTING = 1,
    UPDATE_SWITCH = 2,
    SET_PROPOSAL_VARIABLE = 3
};

NetworkCommandType GetNetworkTypeByString(std::string strNetworkType)
{
    if(strNetworkType == "UPDATE_SETTING")
        return NetworkCommandType.UPDATE_SETTING;

    if(strNetworkType == "UPDATE_SWITCH")
        return NetworkCommandType.UPDATE_SWITCH;

    if(strNetworkType == "SET_PROPOSAL_VARIABLE")
        return NetworkCommandType.SET_PROPOSAL_VARIABLE;

    return NetworkCommandType.NONE;
}

std::string GetNetworkTypeStr(NetworkCommandType type)
{
    if(type == UPDATE_SWITCH) return "UPDATE_SWITCH";
    if(type == UPDATE_SETTING) return "UPDATE_SETTING";
    if(type == SET_PROPOSAL_VARIABLE) return "SET_PROPOSAL_VARIABLE";

    return "NONE";
}

enum AdminCommandState {
    NEW = -1,
    AWAITING_SIGNATURES,
    PENDING_EXECUTION,
    EXECUTED
};

/*
Dash Network Administration:
Settings and Switches

Abstract

    A process for running specialized maintainance functions securely in a decentralized environment, with a multisignature code execution system. 

Introduction

    Developers of a currency will have the capability of managing core operational tasks of the underlying cryptocurrency Dash, via a set of highly secure multisignature modules added to Dash Core. These core modules will be able to set network behaviors, trigger hardforks and other network critical operations.

    Members of the group use a single key, which will be configured into the Dash software and allow them to execute commands and propagate special messages throughout the network. 

Collection of Signatures

    Commands are ran by each administrator separately, then collected and saved in a parent object. Each time a signature is saved, the parent 
    object is checked, if enough signatures have been collected the function will be called, with the parameters associated. 

Parameterization

    Functions are called with the dataPayload object, which is deserialized using the standard bitcoin serializers. 

Security Model

    Administrators are unable to perform tasks alone, so the network is safe from abuse. Standard private/public key ec encryption will be used to allow only
    specific people to have rights to execute commands.

Objects:
    map<int, CNetworkCommand> nNetworkCommands;
    map<std::string, std::string> mapNetworkMemory;

    CAdminSignature()
        uint256 nCommandHash
        pubkey
        signature

    CAdminCommand()
        int nCommand
        vector<CAdminSignature> vSigs;
        bool fExecuted

        HasEnoughSignatures()
        Execute()
        etc

    CAdminManager()
        vector<CAdminCommand>

        ProcessMessage(strCommand..)

    UpdateSetting //network wide settings
    UpdateSwitch //network switches to allow rapid hardforking


Special Commands:

    setting \command\...
    Manage contracts
    Available commands:
      new                - Create a new network setting
      update             - Update an existing network setting
      list               - List settings in various ([all|valid|extended|active|help])

    examples:

        fee-per-kb
        contract-fee
        proposal-fee
        contract-3mo-minimum-support
        contract-6mo-minimum-support
        contract-12mo-minimum-support

    --------------------------------------------

    switch \command\...
    Manage switches (used for hardforks, etc)
    Available commands:
      new                - Create a new switch
      update             - Update an existing switch with different values
      activate           - Activate a switch at a specific network time (+5m, +1h, etc)
      list               - Show all network switches

    examples:

        fee-per-kb
        contract-fee
        proposal-fee
        contract-3mo-minimum-support
        contract-6mo-minimum-support
        contract-12mo-minimum-support


    --------------------------------------------

    proposal \command\...
    Available commands:
      remove             - Terminate an existing governance object

    contract \command\...
    Available commands:
      remove             - Terminate an existing governance object

    examples:

        - when a contractor walks away from an active network contract (fraud?)


    --------------------------------------------


Process:

    - These groups of administers will be elected by the network and can also be fired
    -


*/

std::map<int, CNetworkCommand> mapNetworkCommands;
std::map<std::string, std::string> mapNetworkMemory;

class CAdminSignature
{
private:
    uint256 nHash;
    uint256 nParentHash; //CAdminCommand.nHash
    int nKeyIndex; //the corresponding NETWORK_ADMIN_KEY index

public:

    CAdminSignature(uint256 nParentHashIn, int nKeyIndexIn)
    {
        nParentHash = nParentHashIn;
        nKeyIndex = nKeyIndexIn;
        nHash = uint256();
        nParentHash = uint256();
    }

    uint256 GetParentHash()
    {
        return nParentHash;
    }

    uint256 GetHash(){
        if(nHash) return nHash;

        // missing info
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << nParentHash;
        ss << nKeyIndexIn;
        nHash = ss.GetHash();
        return nHash;
    }

};

class CAdminCommand
{
    uint256 nHash;

    //admin signature hash, sig
    std::map<uint256, CAdminSignature> mapSignatures;
    NetworkCommandType type;
    AdminCommandState state;
    std::vector<char> vchCommand;

    CAdminCommand()
    {
        nHash = uint256();
        state = NEW;
        type = NONE;
    }

    CAdminCommand(NetworkCommandType typeIn, uint256 nHashIn, std::vector<char>& vchCommandIn)
    {
        type = typeIn;
        state = NEW;
        vchCommand = vchCommandIn;
    }

    std::vector<char>& GetCommand()
    {
        return vchCommand;
    }

    uint256 GetHash(){
        if(nHash) return nHash;

        // missing info
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << type;
        ss << vchCommand;
        nHash = ss.GetHash();
        return nHash;
    }

    AdminCommandState GetState()
    {
        return state;
    }

    bool AddSignature(CAdminSignature sig, std::string& strError)
    {
        if(mapSignatures.count(sig.GetHash()))
        {
            strError = "Error adding signature. Found duplicate.";
            return false;
        }

        if(!sig.IsValid())
        {
            strError = "Error adding signature. Invalid.";
            return false;
        }
        
        //add the signature
        mapSignatures.insert(make_pair(sig.GetHash(), sig));
    }

    bool HaveEnoughSignatures()
    {
        if(mapSignatures.size() >= NETWORK_ADMIN_KEYS_REQUIRED) //mult
        {
            return true;
        }

        return false;
    }

    void Process();
    void Relay();

};

enum AdminCommandState {
    NEW = -1,
    AWAITING_SIGNATURES,
    PENDING_EXECUTION,
    EXECUTED
};

class CAdminManager
{   
    // command hash, obj
    std::map<uint256, CAdminCommand> mapCommands;

    // track which messages we've processed so far
    std::map<uint256, CAdminCommand> mapSeenAdminCommands;
    std::map<uint256, CAdminSignature> mapSeenAdminSignatures;

    void ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);

    bool CheckSignature(CAdminSignature& admsig);
    bool Sign(CAdminSignature& admsig);

    CAdminCommand& FindCommand(uint256 nHash);

    bool HaveCommand(uint256 nHash);
    bool AddSignature(CAdminSignature& sig, std::string& strError);
    bool RequestCommand(uint256 nHash);
};

class CNetworkAdministration
{
    // Command Execution
    static bool RemoveProposal(CAdminCommand& cmd, std::string& strError);
    static bool RemoveContract(CAdminCommand& cmd, std::string& strError);
};
