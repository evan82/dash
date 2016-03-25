


/*
Dash Network Administration:
Settings and Switches

Abstract

    A process for running specialized functions of maintainance code, with a multisignature code execution system. 

Introduction

    Developers of a currency will have the capability of managing core operational tasks of the underlying cryptocurrency Dash, via a set of highly secure multisignature modules added to Dash Core. These core modules will be able to set network behaviors, trigger hardforks and other network critical operations.

    Members of the group will be given a single key, which will be configured into the Dash software and allow them to execute commands and propagate special messages throughout the network. 

Collection of Signatures

    Commands are ran by each administer separately, then collected and saved in a admin.dat. 
    Each time a signature is saved, the parent object is checked, if enough signatures have been collected the function 
    will be called, with the parameters associated. 

Parameterization

    Functions are called with the dataPayload object, which is deserialized using the standard bitcoin serializers. 

Security Model

    Administrators are unable to perform tasks alone, so the network is safe from abuse. Standard private/public key ec encryption will be used to allow only
    specific people to have rights to execute commands.

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


class CNetworkSettings
{
    void ProcessMessages();
    
};





// class CSporkMessage;
// class CSporkManager;

// extern std::map<uint256, CSporkMessage> mapSporks;
// extern std::map<int, CSporkMessage> mapSporksActive;
// extern CSporkManager sporkManager;

// void ProcessSpork(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);
// int64_t GetSporkValue(int nSporkID);
// bool IsSporkActive(int nSporkID);
// void ExecuteSpork(int nSporkID, int nValue);
// void ReprocessBlocks(int nBlocks);

// //
// // Spork Class
// // Keeps track of all of the network spork settings
// //

// class CSporkMessage
// {
// public:
//     std::vector<unsigned char> vchSig;
//     int nSporkID;
//     int64_t nValue;
//     int64_t nTimeSigned;

//     uint256 GetHash(){
//         uint256 n = HashX11(BEGIN(nSporkID), END(nTimeSigned));
//         return n;
//     }

//     ADD_SERIALIZE_METHODS;

//     template <typename Stream, typename Operation>
//     inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
//         READWRITE(nSporkID);
//         READWRITE(nValue);
//         READWRITE(nTimeSigned);
//         READWRITE(vchSig);
//     }
// };


// class CSporkManager
// {
// private:
//     std::vector<unsigned char> vchSig;
//     std::string strMasterPrivKey;

// public:

//     CSporkManager() {
//     }

//     std::string GetSporkNameByID(int id);
//     int GetSporkIDByName(std::string strName);
//     bool UpdateSpork(int nSporkID, int64_t nValue);
//     bool SetPrivKey(std::string strPrivKey);
//     bool CheckSignature(CSporkMessage& spork);
//     bool Sign(CSporkMessage& spork);
//     void Relay(CSporkMessage& msg);

// };





























