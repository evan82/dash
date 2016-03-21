Protocol Documentation - 0.12.1
=====================================

This document describes the protocol extensions for all additional functionality build into the Dash protocol. This doesn't include any of the Bitcoin procotol, which has been left in tact in the Dash project. For more information about the core protocol, please see https://en.bitcoin.it/w/index.php?title#Protocol_documentation&action#edit

## Common Structures

### COutpoint

Bitcoin Input

| Field Size | Description | Data type | Comments |
| ---------- | ----------- | --------- | -------- |
| # | hash | uint256 | Hash of transactional output which is being referenced
| # | n  | uint32_t | Index of transaction which is being referenced


### CTXIn

Bitcoin Input

| Field Size | Description | Data type | Comments |
| ---------- | ----------- | --------- | -------- |
| # | prevout | COutPoint | The previous output from an existing transaction, an the form of an unspent output
| # | script | CScript | The script which is validated for this input to be spent
| # | nSequence | uint_32t | 

### CPubkey

Bitcoin Public Key

| Field Size | Description | Data type | Comments |
| ---------- | ----------- | --------- | -------- |
| 1-65 | vch | char[] | Encapcilated public key of masternode in serialized varchar form

### Masternode Winner

When a new block is found on the network, a masternode quorum will be determined and those 10 selected masternodes will issue a masternode winner command to pick the next winning node. 

| Field Size | Description | Data type | Comments |
| ---------- | ----------- | --------- | -------- |
| 41 | vinMasternode | tx_in | The unspent output of the masternode which is signing the message
| # | nBlockHeight | int | The blockheight which the payee should be paid
| # | payeeAddress | cscript | The address to pay to
| # | sig | char[] | Signature of the masternode)

## Message Types

### Masternode Winner

When a new block is found on the network, a masternode quorum will be determined and those 10 selected masternodes will issue a masternode winner command to pick the next winning node. 

| Field Size | Description | Data type | Comments |
| ---------- | ----------- | --------- | -------- |
| 41 | vinMasternode | tx_in | The unspent output of the masternode which is signing the message
| # | nBlockHeight | int | The blockheight which the payee should be paid
| # | payeeAddress | cscript | The address to pay to
| # | sig | char[] | Signature of the masternode)

### Governance Vote

Masternodes use governance voting in response to new proposals, contracts, settings or finalized budgets.

| Field Size | Description | Data type | Comments |
| ---------- | ----------- | --------- | -------- |
| # | Unspent Output | tx_in | Unspent output for the masternode which is voting
| # | nParentHash | uint256 | Object which we're voting on (proposal, contract, setting or final budget)
| # | nVote | int | Yes, No or Abstain
| # | nTime | int_64t | Time which the vote was created
| # | vchSig | int | Signature of the masternode

### Governance Object

A proposal, contract or setting.

| Field Size | Description | Data type | Comments |
| ---------- | ----------- | --------- | -------- |
| # | strName | std::string | Name of the governance object
| # | strURL | std::string | URL where detailed information about the governance object can be found
| # | nTime | int_64t | Time which this object was created
| # | nBlockStart | int | Starting block, which the first payment will occur
| # | nBlockEnd | int | Ending block, which the last payment will occur
| # | nAmount | int_64t | The amount in satoshi's that will be paid out each time
| # | payee | cscript | Address which will be paid out to
| # | nFeeTXHash | uint256 | Signature of the masternode

### Finalized Budget

Contains a finalized list of the order in which the next budget will be paid. 

| Field Size | Description | Data type | Comments |
| ---------- | ----------- | --------- | -------- |
| 41 | strBudgetName | tx_in | The unspent output of the masternode which is signing the message
| # | nBlockStart | int | The blockheight which the payee should be paid
| # | vecBudgetPayments | cscript | The address to pay to
| # | nFeeTXHash | char[] | Signature of the masternode

### Masternode Announce

Whenever a masternode comes online or a client is syncing, they will send this message which describes the masternode entry and how to validate messages from it. 

| Field Size | Description | Data type | Comments |
| ---------- | ----------- | --------- | -------- |
| # | vin | tx_in | The unspent output of the masternode which is signing the message
| # | addr | CService | Address of the main 1000 DASH unspent output
| # | pubkey | CPubkey | CPubKey of the main 1000 DASH unspent output
| # | pubkey2 | CPubkey | CPubkey of the secondary signing key (For all other messaging other than announce message)
| # | sig | cscript | Signature of 
| # | sigTime | cscript | Time which the signature was created
| # | protocolVersion | cscript | The protocol version of the masternode
| # | lastPing | cscript | The last time the masternode pinged the network
| # | nLastDsq | char[] | The last time the masternode sent a DSQ message (for darksend mixing)

### Masternode Ping

Every few minutes, masternodes ping the network with a message that propagates the whole network.

| Field Size | Description | Data type | Comments |
| ---------- | ----------- | --------- | -------- |
| # | vin | tx_in | The unspent output of the masternode which is signing the message
| # | blockHash | uint256 | Current chaintip blockhash minus 12
| # | sigTime | int_64t | Signature time for this ping
| # | vchSig | char[] | Signature of the masternode (pubkey2)

### Masternode DSTX

Masternodes can broadcast subsidised transactions without fees for the sake of security in Darksend. This is done via the DSTX message.

| Field Size | Description | Data type | Comments |
| ---------- | ----------- | --------- | -------- |
| # | tx | uint256 | The unspent output of the masternode which is signing the message
| # | vin | tx_in | Masternode unspent output
| # | vchSig | char[] | Signature of the masternode
| # | sigTime | int_64_t | Time this message was created

### DSSTATUSUPDATE - DSSU

Darksend pool status update

| Field Size | Description | Data type | Comments |
| ---------- | ----------- | --------- | -------- |
| # | sessionID | int | The unspent output of the masternode which is signing the message
| # | GetState | int | Masternode unspent output
| # | GetEntriesCount | int | Signature of the masternode
| # | Status | int | Time this message was created
| # | errorID | int | Time this message was created

### DSSTATUSUPDATE - DSQ

Asks users to sign final Darksend tx message.

| Field Size | Description | Data type | Comments |
| ---------- | ----------- | --------- | -------- |
| # | vDenom | int | Which denominations are allowed in this mixing session
| # | vin | int | unspend output from masternode which is hosting this session
| # | time | int | the time this DSQ was created
| # | ready | int | if the mixing pool is ready to be executed
| # | vchSig | int | signature from the masternode

### DSSTATUSUPDATE - DSA

Response to DSQ message which allows the user to join a Darksend mixing pool

| Field Size | Description | Data type | Comments |
| ---------- | ----------- | --------- | -------- |
| # | sessionDenom | int | denomination that will be exclusively used when submitting inputs into the pool
| # | txCollateral | int | unspend output from masternode which is hosting this session

### DSSTATUSUPDATE - DSS

User's signed inputs for a group transaction in a Darksend session

| Field Size | Description | Data type | Comments |
| ---------- | ----------- | --------- | -------- |
| # | inputs | tx_in[] | signed inputs for Darksend session
