
// Copyright (c) 2014-2016 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef GOVERANCE_CLASSES_H
#define GOVERANCE_CLASSES_H

//#define ENABLE_DASH_DEBUG

#include "util.h"
#include "main.h"
#include "sync.h"
#include "net.h"
#include "key.h"
#include "util.h"
#include "base58.h"
#include "masternode.h"
#include <boost/lexical_cast.hpp>
#include <boost/shared_ptr.hpp>
#include "init.h"
#include "governance.h"

using namespace std;

#define TRIGGER_UNKNOWN             -1
#define TRIGGER_SUPERBLOCK          1000

class CSuperblock;
class CGovernanceTrigger;
class CGovernanceTriggerManager;
class CSuperblockManager;

typedef boost::shared_ptr<CSuperblock> CSuperblock_sptr;

// DECLARE GLOBAL VARIABLES FOR GOVERNANCE CLASSES
extern CGovernanceTriggerManager triggerman;

// SPLIT A STRING UP - USED FOR SUPERBLOCK PAYMENTS
std::vector<std::string> SplitBy(std::string strCommand, std::string strDelimit);

/**
*   Trigger Mananger
*
*   - Track governance objects which are triggers
*   - After triggers are activated and executed, they can be removed
*/

class CGovernanceTriggerManager
{
    friend class CSuperblockManager;
    friend class CGovernanceManager;

public: // Typedefs
    typedef std::map<uint256, CSuperblock_sptr> trigger_m_t;

    typedef trigger_m_t::iterator trigger_m_it;

    typedef trigger_m_t::const_iterator trigger_m_cit;

private:
    trigger_m_t mapTrigger;

public:
    CGovernanceTriggerManager()
        : mapTrigger()
    {}

private:
    std::vector<CSuperblock_sptr> GetActiveTriggers();

    bool AddNewTrigger(uint256 nHash);

    void CleanAndRemove();

};

/**
*   Superblock Manager
*
*   Class for querying superblock information
*/

class CSuperblockManager
{
public:
    static bool IsBlockValid(const CTransaction& txNew)
    {
        // 12.1 -- todo
        // for(unsigned int i = 0; i < (int)txNew.vout.size(); i++)
        // {
        //     BOOST_FOREACH(const CTxOut& v, txNew[i].vout)
        //     {
        //         txNew.vout.push_back(v);
        //     }
        // }

        // bool nExecuted = true;
        return true;
    }

    static bool IsValidSuperblockHeight(int nBlockHeight);
    static bool IsSuperblockTriggered(int nBlockHeight);
    static void CreateSuperblock(CMutableTransaction& txNew, CAmount nFees, int nBlockHeight);


    static std::string GetRequiredPaymentsString(int nBlockHeight);
    static bool IsValid(const CTransaction& txNew, int nBlockHeight);

private:
    static bool GetBestSuperblock(CSuperblock_sptr& pBlock, int nBlockHeight);

};

/**
*   Governance Object Payment
*
*/

class CGovernancePayment
{
public:
    CScript script;
    CAmount nAmount;
    bool fValid; 

    CGovernancePayment()
    {
        SetNull();
    }

    void SetNull()
    {
        script = CScript();
        nAmount = 0;
        fValid = false;
    }

    bool IsValid()
    {
        return fValid;
    }

    CGovernancePayment(CBitcoinAddress addrIn, CAmount nAmountIn)
    {
        try
        {
            CTxDestination dest = addrIn.Get();
            script = GetScriptForDestination(dest);
            nAmount = nAmountIn;
        } catch(...) {
            SetNull(); //set fValid to false
        }
    }
};


/**
*   Trigger : Superblock
*
*   - Create payments on the network
*/

class CSuperblock : public CGovernanceObject
{

    /*

        object structure: 
        {
            "governance_object_id" : last_id,
            "type" : govtypes.trigger,
            "subtype" : "superblock",
            "superblock_name" : superblock_name,
            "start_epoch" : start_epoch,
            "payment_addresses" : "addr1|addr2|addr3",
            "payment_amounts"   : "amount1|amount2|amount3"
        }
    */

private:
    uint256 nGovObjHash;

    bool fError;
    std::string strError;

    int nEpochStart;
    int status;
    std::vector<CGovernancePayment> vecPayments;

public:

    CSuperblock()
        : nGovObjHash(),
          fError(true),
          strError(),
          nEpochStart(0),
          status(SEEN_OBJECT_UNKNOWN),
          vecPayments()
    {}

    CSuperblock(uint256& nHash)
        : nGovObjHash(nHash),
          fError(true),
          strError(),
          nEpochStart(0),
          status(SEEN_OBJECT_UNKNOWN),
          vecPayments()
    {
        DBG( cout << "CSuperblock Constructor Start" << endl; );

        CGovernanceObject* pGovObj = GetGovernanceObject();

        if(!pGovObj) {
            DBG( cout << "CSuperblock Constructor pGovObjIn is NULL, returning" << endl; );
            strError = "Failed to find Governance Object";
            return;
        }

        DBG( cout << "CSuperblock Constructor pGovObj : "
             << pGovObj->GetDataAsString()
             << ", nObjectType = " << pGovObj->nObjectType
             << endl; );

        if(pGovObj->GetObjectType() != GOVERNANCE_OBJECT_TRIGGER)  {
            DBG( cout << "CSuperblock Constructor pHoObj not a trigger, returning" << endl; );
            strError = "Governance Object not a trigger";
            return;
        }

        UniValue obj = pGovObj->GetJSONObject();
        
        try  {
            // FIRST WE GET THE START EPOCH, THE DATE WHICH THE PAYMENT SHALL OCCUR
            strError = "Error parsing start epoch";
            std::string nEpochStartStr = obj["event_block_height"].get_str();
            if(!ParseInt32(nEpochStartStr, &nEpochStart))  {
                throw runtime_error("Parse error parsing event_block_height");
            }

            // NEXT WE GET THE PAYMENT INFORMATION AND RECONSTRUCT THE PAYMENT VECTOR
            strError = "Missing payment information";
            std::string strAddresses = obj["payment_addresses"].get_str();
            std::string strAmounts = obj["payment_amounts"].get_str();
            ParsePaymentSchedule(strAddresses, strAmounts);

            fError = false;
            strError = "";
        }
        catch(...)  {
            fError = true;
            strError = "Unparsable";
            DBG( cout << "CSuperblock Constructor A parse error occurred" 
                      << ", obj = " << obj.write()
                      << endl; );
        }

        DBG( cout << "CSuperblock Constructor End" << endl; );
    }

    int GetStatus()  {
        return status;
    }

    void SetStatus(int status_)  {
        status = status_;
    }

    int GetErrorState()  {
        return fError;
    }

    std::string GetErrorMessage()  {
        return strError;
    }

    CGovernanceObject* GetGovernanceObject()  {
        AssertLockHeld(governance.cs);
        CGovernanceObject* pObj = governance.FindGovernanceObject(nGovObjHash);
        return pObj;
    }

    int GetBlockStart()
    {
        /* // 12.1 TRIGGER EXECUTION */
        /* // NOTE : Is this over complicated? */

        /* //int nRet = 0; */
        /* int nTipEpoch = 0; */
        /* int nTipBlock = chainActive.Tip()->nHeight+1; */

        /* // GET TIP EPOCK / BLOCK */

        /* // typically it should be more than the current time */
        /* int nDiff = nEpochStart - nTipEpoch; */
        /* int nBlockDiff = nDiff / (2.6*60); */

        /* // calculate predicted block height */
        /* int nMod = (nTipBlock + nBlockDiff) % Params().GetConsensus().nSuperblockCycle; */

        /* return (nTipBlock + nBlockDiff)-nMod; */
        return nEpochStart;
    }

    bool ParsePaymentSchedule(std::string& strPaymentAddresses, std::string& strPaymentAmounts)
    {
        // SPLIT UP ADDR/AMOUNT STRINGS AND PUT IN VECTORS

        std::vector<std::string> vecParsed1;
        std::vector<std::string> vecParsed2;
        vecParsed1 = SplitBy(strPaymentAddresses, "|");
        vecParsed2 = SplitBy(strPaymentAmounts, "|");

        // IF THESE DONT MATCH, SOMETHING IS WRONG

        if(vecParsed1.size() != vecParsed2.size()) 
        {
            strError = "Mismatched payments and amounts";
            return false;
        }

        // LOOP THROUGH THE ADDRESSES/AMOUNTS AND CREATE PAYMENTS
        /*
            ADDRESSES = [ADDR1|2|3|4|5\6]
            AMOUNTS = [AMOUNT1|2|3|4|5\6]
        */

        for(int i = 0; i < (int)vecParsed1.size(); i++)
        {
            CBitcoinAddress address(vecParsed1[i]);
            if (!address.IsValid())
            {
                strError = "Invalid Dash Address : " +  vecParsed1[i];
                return false;
            }
            int nAmount = boost::lexical_cast<int>(vecParsed2[i]);

            CGovernancePayment payment(address, nAmount);
            if(payment.IsValid())
            {
                vecPayments.push_back(payment);   
            }
        }

        return false;
    }

    // IS THIS TRIGGER ALREADY EXECUTED?
    bool IsExecuted()
    {
        return (status == SEEN_OBJECT_EXECUTED);
    };

    // TELL THE ENGINE WE EXECUTED THIS EVENT
    void SetExecuted()
    {
        status = SEEN_OBJECT_EXECUTED;
    };

    int CountPayments()
    {
        return (int)vecPayments.size();
    }

    bool GetPayment(int nPaymentIndex, CGovernancePayment& paymentOut)
    {
        if(nPaymentIndex >= (int)vecPayments.size()) 
        {
            return false;
        }

        paymentOut = vecPayments[nPaymentIndex];
        return true;
    }

    bool IsValid(const CTransaction& txNew);
};

#endif
