// Copyright (c) 2014-2016 The Dash Core developers

// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef MASTERNODE_BUDGET_H
#define MASTERNODE_BUDGET_H

#include "main.h"
#include "sync.h"
#include "net.h"
#include "key.h"
#include "util.h"
#include "base58.h"
#include "masternode.h"
#include <boost/lexical_cast.hpp>
#include "init.h"

using namespace std;

extern CCriticalSection cs_budget;

class CGovernanceManager;
class CFinalizedBudgetBroadcast;
class CFinalizedBudget;
class CGovernanceObject;
class CGovernanceObjectBroadcast;
class CGovernanceVote;
class CTxBudgetPayment;

#define VOTE_ABSTAIN  0
#define VOTE_YES      1
#define VOTE_NO       2

static const CAmount BUDGET_FEE_TX = (5*COIN);
static const int64_t BUDGET_FEE_CONFIRMATIONS = 6;
static const int64_t BUDGET_VOTE_UPDATE_MIN = 60*60;
static const int64_t CONTRACT_ACTIVATION_TIME = 60*60*24*14;

extern std::vector<CGovernanceObjectBroadcast> vecImmatureBudgetProposals;
extern std::vector<CFinalizedBudgetBroadcast> vecImmatureFinalizedBudgets;


/* 
    Voting Mechanism:
        CGovernanceVote: Votes for
            Proposal, Contract, FinalizedBudget

    Object Classes:
        CGovernanceObject
            Proposal, Contract
        CFinalizedBudget
            FinalizedBudget
*/

enum GovernanceObjectType {
    Error = -1,
    None,
    Proposal,
    Contract,
    FinalizedBudget,
    AllTypes = 999 //used for filtering
};

extern CGovernanceManager governance;
void DumpBudgets();

//Check the collateral transaction for the budget proposal/finalized budget
bool IsBudgetCollateralValid(uint256 nTxCollateralHash, uint256 nExpectedHash, std::string& strError, int64_t& nTime, int& nConf);

/** Save Budget Manager (budget.dat)
 */
class CBudgetDB
{
private:
    boost::filesystem::path pathDB;
    std::string strMagicMessage;
public:
    enum ReadResult {
        Ok,
        FileError,
        HashReadError,
        IncorrectHash,
        IncorrectMagicMessage,
        IncorrectMagicNumber,
        IncorrectFormat
    };

    CBudgetDB();
    bool Write(const CGovernanceManager &objToSave);
    ReadResult Read(CGovernanceManager& objToLoad, bool fDryRun = false);
};


//
// Governance Manager : Contains all proposals for the budget
//
class CGovernanceManager
{   // **** Objects and memory ****

private:

    //hold txes until they mature enough to use
    map<uint256, CTransaction> mapCollateral;
    // Keep track of current block index
    const CBlockIndex *pCurrentBlockIndex;

public:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;
    
    // hold governance objects (proposals, contracts, settings and switches)
    map<uint256, CGovernanceObject> mapGovernanceObjects;
    // finalized budgets are kept in their own object
    map<uint256, CFinalizedBudget> mapFinalizedBudgets;

    std::map<uint256, CGovernanceObjectBroadcast> mapSeenGovernanceObjects;
    std::map<uint256, CGovernanceVote> mapSeenGovernanceVotes;
    std::map<uint256, CGovernanceVote> mapOrphanGovernanceVotes;
    std::map<uint256, CFinalizedBudgetBroadcast> mapSeenFinalizedBudgets;

    // **** Initialization ****

    CGovernanceManager() {
        mapGovernanceObjects.clear();
        mapFinalizedBudgets.clear();
    }

    void Clear(){
        LOCK(cs);

        LogPrintf("Governance object cleared\n");
        mapGovernanceObjects.clear();
        mapFinalizedBudgets.clear();
        mapSeenGovernanceObjects.clear();
        mapSeenGovernanceVotes.clear();
        mapSeenFinalizedBudgets.clear();
        mapOrphanGovernanceVotes.clear();
    }

    void ClearSeen() {
        mapSeenGovernanceObjects.clear();
        mapSeenGovernanceVotes.clear();
        mapSeenFinalizedBudgets.clear();
    }

    void Sync(CNode* node, uint256 nProp, bool fPartial=false);
    void ResetSync();
    void MarkSynced();

    // **** Search / Statistics / Information ****

    int CountProposalInventoryItems() { return mapSeenGovernanceObjects.size() + mapSeenGovernanceVotes.size(); }
    int CountFinalizedInventoryItems() { return mapSeenFinalizedBudgets.size(); }

    CGovernanceObject *FindGovernanceObject(const std::string &strName);
    CGovernanceObject *FindGovernanceObject(uint256 nHash);
    CFinalizedBudget *FindFinalizedBudget(uint256 nHash);
    GovernanceObjectType GetGovernanceTypeByHash(uint256 nHash);
    std::vector<CGovernanceObject*> GetBudget();
    CAmount GetTotalBudget(int nHeight);
    bool HasNextFinalizedBudget(); // Do we have the next finalized budget?
    bool IsBudgetPaymentBlock(int nBlockHeight);
    bool IsTransactionValid(const CTransaction& txNew, int nBlockHeight);
    
    std::vector<CGovernanceObject*> FindMatchingGovernanceObjects(GovernanceObjectType type);
    std::vector<CFinalizedBudget*> GetFinalizedBudgets();
    std::string GetRequiredPaymentsString(int nBlockHeight);

    std::string ToString() const;

    // **** Update ****

    bool AddFinalizedBudget(CFinalizedBudget& finalizedBudget);
    bool AddGovernanceObject(CGovernanceObject& budgetProposal);
    bool AddOrphanGovernanceVote(CGovernanceVote& vote, CNode* pfrom);
    void CheckAndRemove();
    void CheckOrphanVotes();
    void FillBlockPayee(CMutableTransaction& txNew, CAmount nFees);
    void NewBlock();
    void SubmitFinalBudget();
    void ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);
    void UpdatedBlockTip(const CBlockIndex *pindex);
    bool UpdateGovernanceObjectVotes(CGovernanceVote& vote, CNode* pfrom, std::string& strError);

    // **** Serializer ****

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(mapSeenGovernanceObjects);
        READWRITE(mapSeenGovernanceVotes);
        READWRITE(mapSeenFinalizedBudgets);
        READWRITE(mapOrphanGovernanceVotes);

        READWRITE(mapGovernanceObjects);
        READWRITE(mapFinalizedBudgets);
    }

};

std::string GovernanceTypeToString(GovernanceObjectType type) {
    std::string s = "";
    if(type == Error) s = "Error";
    if(type == Proposal) s = "Proposal";
    if(type == Contract) s = "Contract";
    if(type == Setting) s = "Setting";
    if(type == Switch) s = "Switch";
    if(type == FinalizedBudget) s = "FinalizedBudget";
    if(type == AllTypes) s = "AllTypes";
    return s;
};

//
// Governance Object (Base) : Contains the masternode votes for each budget
//      This could be a Proposal or Contract
//

class CGovernanceObject
{   // **** Objects and memory ****

private:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;
    CAmount nAlloted;

public:
    bool fValid;
    std::string strName;

    /*
        json object with name, short-description, long-description, pdf-url and any other info
        This allows the proposal website to stay 100% decentralized
    */
    
    int nGovernanceType;
    int64_t nTime;
    uint256 nFeeTXHash;

    std::string strURL;
    int nBlockStart;
    int nBlockEnd;
    CAmount nAmount;
    CScript address;

    map<uint256, CGovernanceVote> mapVotes;

    // **** Initialization ****

    CGovernanceObject();
    CGovernanceObject(const CGovernanceObject& other);

    // **** Update ****
    
    bool AddOrUpdateVote(CGovernanceVote& vote, std::string& strError);
    void CleanAndRemove(bool fSignatureCheck);
    bool HasMinimumRequiredSupport();

    void SetAllotted(CAmount nAllotedIn) {nAlloted = nAllotedIn;}
    void SetNull();

    // **** Statistics / Information ****
    
    int GetAbsoluteYesCount();
    int GetAbstainCount();
    CAmount GetAllotted() {return nAlloted;}
    CAmount GetAmount() {return nAmount;}
    
    int GetBlockCurrentCycle(const CBlockIndex* pindex);
    int GetBlockEnd() {return nBlockEnd;}
    int GetBlockEndCycle();
    int GetBlockStart() {return nBlockStart;}
    int GetBlockStartCycle();
    
    GovernanceObjectType GetGovernanceType();
    std::string GetGovernanceTypeAsString();

    // for values
    std::string GetSuggestedValueAsString(){return strSuggestedValue;}
    int GetSuggestedValueAsInt(){return boost::lexical_cast<int>(strSuggestedValue);}

    // for contracts (should we subclass a contract object??)
    int64_t GetContractActivationTime()
    bool IsContractActivated();


    uint256 GetHash(){
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << strName;
        ss << strURL;
        ss << nBlockStart;
        ss << nBlockEnd;
        ss << nAmount;
        ss << *(CScriptBase*)(&address);
        ss << nGovernanceType;
        uint256 h1 = ss.GetHash();

        return h1;
    }

    std::string GetName() {return strName; }
    int GetNoCount();
    int GetRemainingPaymentCount(int nBlockHeight);
    double GetRatio();
    CScript GetPayee() {return address;}
    int GetTotalPaymentCount();
    std::string GetURL() {return strURL; }
    int64_t GetValidEndTimestamp();
    int64_t GetValidStartTimestamp();
    std::pair<std::string, std::string> GetVotes();
    int GetYesCount();

    bool IsValid(const CBlockIndex* pindex, std::string& strError, bool fCheckCollateral=true);
    bool IsEstablished();

    // **** Serializer ****

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        // TODO : For testnet version bump
        READWRITE(nGovernanceType);

        READWRITE(LIMITED_STRING(strName, 20));
        READWRITE(LIMITED_STRING(strURL, 64));
        READWRITE(nBlockStart);
        READWRITE(nBlockEnd);
        READWRITE(nAmount);

        READWRITE(*(CScriptBase*)(&address));
        READWRITE(nTime);
        READWRITE(nFeeTXHash);

        //for saving to the serialized db
        READWRITE(mapVotes);
    }
};

// Proposals are cast then sent to peers with this object, which leaves the votes out
class CGovernanceObjectBroadcast : public CGovernanceObject
{
public:
    CGovernanceObjectBroadcast() : CGovernanceObject(){}
    CGovernanceObjectBroadcast(const CGovernanceObject& other) : CGovernanceObject(other){}
    CGovernanceObjectBroadcast(const CGovernanceObjectBroadcast& other) : CGovernanceObject(other){}

    void CreateProposalOrContract(GovernanceObjectType nTypeIn, std::string strNameIn, std::string strURLIn, int nPaymentCount, CScript addressIn, CAmount nAmountIn, int nBlockStartIn, uint256 nFeeTXHashIn);
    void CreateProposal(std::string strNameIn, std::string strURLIn, int nPaymentCount, CScript addressIn, CAmount nAmountIn, int nBlockStartIn, uint256 nFeeTXHashIn);
    void CreateContract(std::string strNameIn, std::string strURLIn, int nPaymentCount, CScript addressIn, CAmount nAmountIn, int nBlockStartIn, uint256 nFeeTXHashIn);
    void CreateSetting(std::string strNameIn, std::string strURLIn, std::string strSuggestedValueIn, uint256 nFeeTXHashIn);

    void swap(CGovernanceObjectBroadcast& first, CGovernanceObjectBroadcast& second) // nothrow
    {
        // enable ADL (not necessary in our case, but good practice)
        using std::swap;

        // by swapping the members of two classes,
        // the two classes are effectively swapped
        swap(first.nGovernanceType, second.nGovernanceType);
        swap(first.strName, second.strName);
        swap(first.nBlockStart, second.nBlockStart);
        swap(first.strURL, second.strURL);
        swap(first.nBlockEnd, second.nBlockEnd);
        swap(first.nAmount, second.nAmount);
        swap(first.address, second.address);
        swap(first.nTime, second.nTime);
        swap(first.nFeeTXHash, second.nFeeTXHash);
        swap(first.nGovernanceType, second.nGovernanceType);
        first.mapVotes.swap(second.mapVotes);
    }

    CGovernanceObjectBroadcast& operator=(CGovernanceObjectBroadcast from)
    {
        swap(*this, from);
        return *this;
    }

    void Relay();

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        // TODO : For testnet version bump
        READWRITE(nGovernanceType);

        READWRITE(LIMITED_STRING(strName, 20));
        READWRITE(LIMITED_STRING(strURL, 64));
        READWRITE(nTime);
        READWRITE(nBlockStart);
        READWRITE(nBlockEnd);
        READWRITE(nAmount);
        READWRITE(*(CScriptBase*)(&address));
        READWRITE(nFeeTXHash);
    }
};

//
// CGovernanceVote - Allow a masternode node to vote and broadcast throughout the network
//

class CGovernanceVote
{   // **** Objects and memory ****************************************************
    
public:
    int nGovernanceType; //GovernanceObjectType
    bool fValid; //if the vote is currently valid / counted
    bool fSynced; //if we've sent this to our peers
    CTxIn vin;
    uint256 nParentHash; //Object hash which is the parent (proposal, setting, contract, final budget)
    int nVote;
    int64_t nTime;
    std::vector<unsigned char> vchSig;

    // Parent is one of these objects
    CGovernanceObject* pParent1;
    CFinalizedBudget* pParent2;
    
    // **** Initialization ********************************************************

    CGovernanceVote();
    CGovernanceVote(CGovernanceObject* pBudgetObjectParent, CTxIn vin, uint256 nParentHashIn, int nVoteIn);

    // **** Update ****************************************************************

    bool Sign(CKey& keyMasternode, CPubKey& pubKeyMasternode);
    void SetParent(CGovernanceObject* pGovObjectParent);
    void SetParent(CFinalizedBudget* pGovObjectParent);
    
    // **** Statistics / Information **********************************************

    GovernanceObjectType GetGovernanceType()
    {
        return (GovernanceObjectType)nGovernanceType;
    }
    
    int64_t GetValidStartTimestamp()
    {
        if(pParent1) {return pParent1->GetValidStartTimestamp();}
        if(pParent2) {return pParent2->GetValidStartTimestamp();}
        return -1;
    }

    int64_t GetValidEndTimestamp()
    {
        if(pParent1) {return pParent1->GetValidEndTimestamp();}
        if(pParent2) {return pParent2->GetValidEndTimestamp();}
        return -1;
    }

    bool IsValid(bool fSignatureCheck, std::string& strReason);

    std::string GetVoteString() {
        std::string ret = "ABSTAIN";
        if(nVote == VOTE_YES) ret = "YES";
        if(nVote == VOTE_NO) ret = "NO";
        return ret;
    }

    uint256 GetHash(){
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << nGovernanceType;
        ss << vin;
        ss << nParentHash;
        ss << nVote;
        ss << nTime;
        ss << nGovernanceType;
        return ss.GetHash();
    }

    void Relay();

    // **** Serializer ************************************************************

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(vin);
        READWRITE(nParentHash);
        READWRITE(nVote);
        READWRITE(nTime);
        READWRITE(vchSig);

        // TODO : For testnet version bump
        READWRITE(nGovernanceType);
    }

};

class CTxBudgetPayment
{
public:
    uint256 nProposalHash;
    CScript payee;
    CAmount nAmount;

    CTxBudgetPayment() {
        payee = CScript();
        nAmount = 0;
        nProposalHash = uint256();
    }

    ADD_SERIALIZE_METHODS;

    //for saving to the serialized db
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(*(CScriptBase*)(&payee));
        READWRITE(nAmount);
        READWRITE(nProposalHash);
    }
};

//
// Finalized Budget : Contains the suggested proposals to pay on a given block
//

class CFinalizedBudget
{   // **** Objects and memory ****

private:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;
    bool fAutoChecked; //If it matches what we see, we'll auto vote for it (masternode only)

public:
    bool fValid;
    std::string strBudgetName;
    int nBlockStart;
    std::vector<CTxBudgetPayment> vecBudgetPayments;
    map<uint256, CGovernanceVote> mapVotes;
    uint256 nFeeTXHash;
    int64_t nTime;

    // **** Initialization ****

    CFinalizedBudget();
    CFinalizedBudget(const CFinalizedBudget& other);

    // **** Update ****

    bool AddOrUpdateVote(CGovernanceVote& vote, std::string& strError);
    void AutoCheckSuperBlockVoting(); //check to see if we should vote on new superblock proposals
    void CleanAndRemove(bool fSignatureCheck);
    void SubmitVote(); //vote on this finalized budget as a masternode

    // **** Statistics / Information ****
    int GetBlockStart() {return nBlockStart;}
    int GetBlockEnd() {return nBlockStart + (int)(vecBudgetPayments.size() - 1);}
    bool GetBudgetPaymentByBlock(int64_t nBlockHeight, CTxBudgetPayment& payment)
    {
        LOCK(cs);

        int i = nBlockHeight - GetBlockStart();
        if(i < 0) return false;
        if(i > (int)vecBudgetPayments.size() - 1) return false;
        payment = vecBudgetPayments[i];
        return true;
    }

    uint256 GetHash(){
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << strBudgetName;
        ss << nBlockStart;
        ss << vecBudgetPayments;

        uint256 h1 = ss.GetHash();
        return h1;
    }

    std::string GetName() {return strBudgetName; }
    bool GetPayeeAndAmount(int64_t nBlockHeight, CScript& payee, CAmount& nAmount)
    {
        LOCK(cs);

        int i = nBlockHeight - GetBlockStart();
        if(i < 0) return false;
        if(i > (int)vecBudgetPayments.size() - 1) return false;
        payee = vecBudgetPayments[i].payee;
        nAmount = vecBudgetPayments[i].nAmount;
        return true;
    }
    std::string GetProposals();
    double GetScore();
    string GetStatus();
    CAmount GetTotalPayout(); //total dash paid out by this budget
    int64_t GetValidEndTimestamp() {return 0;}
    int64_t GetValidStartTimestamp() {return 32503680000;}
    int GetVoteCount() {return (int)mapVotes.size();}

    bool HasMinimumRequiredSupport();
    bool IsValid(const CBlockIndex* pindex, std::string& strError, bool fCheckCollateral=true);
    bool IsTransactionValid(const CTransaction& txNew, int nBlockHeight);

    // **** Serializer ****

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        // TODO: Do we need names for these? I don't think so
        READWRITE(LIMITED_STRING(strBudgetName, 20));
        READWRITE(nFeeTXHash);
        READWRITE(nTime);
        READWRITE(nBlockStart);
        READWRITE(vecBudgetPayments);
        READWRITE(fAutoChecked);

        READWRITE(mapVotes);
    }
};

// FinalizedBudget are cast then sent to peers with this object, which leaves the votes out
class CFinalizedBudgetBroadcast : public CFinalizedBudget
{
private:
    std::vector<unsigned char> vchSig;

public:
    CFinalizedBudgetBroadcast();
    CFinalizedBudgetBroadcast(const CFinalizedBudget& other);
    CFinalizedBudgetBroadcast(std::string strBudgetNameIn, int nBlockStartIn, std::vector<CTxBudgetPayment> vecBudgetPaymentsIn, uint256 nFeeTXHashIn);

    void swap(CFinalizedBudgetBroadcast& first, CFinalizedBudgetBroadcast& second) // nothrow
    {
        // enable ADL (not necessary in our case, but good practice)
        using std::swap;

        // by swapping the members of two classes,
        // the two classes are effectively swapped
        swap(first.strBudgetName, second.strBudgetName);
        swap(first.nBlockStart, second.nBlockStart);
        first.mapVotes.swap(second.mapVotes);
        first.vecBudgetPayments.swap(second.vecBudgetPayments);
        swap(first.nFeeTXHash, second.nFeeTXHash);
        swap(first.nTime, second.nTime);
    }

    CFinalizedBudgetBroadcast& operator=(CFinalizedBudgetBroadcast from)
    {
        swap(*this, from);
        return *this;
    }

    void Relay();

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(LIMITED_STRING(strBudgetName, 20));
        READWRITE(nBlockStart);
        READWRITE(vecBudgetPayments);
        READWRITE(nFeeTXHash);
    }
};


#endif
