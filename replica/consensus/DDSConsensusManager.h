#ifndef __DDSCONSENSUSMANAGER_H__
#define __DDSCONSENSUSMANAGER_H__

#include "dds_dcps.h"
#include "DDSCreator/CheckStatus.h"
#include "DDSCreator/DDSEntitiesCreator.h"


/**
 * All DDS Entities used in the consensus module
 */
DDS_Topic appendEntries_Topic, appendEntriesReply_Topic, requestVote_Topic, requestVoteReply_Topic, activateSpare_Topic;
DDS_WaitSet appendEntries_WaitSet, appendEntriesReply_WaitSet, collectVotes_WaitSet, leaderElection_WaitSet, activateSpare_WaitSet;
DDS_ReadCondition electionTimer_ReadCondition, requestVote_ReadCondition, requestVoteReply_ReadCondition, appendEntriesReply_ReadCondition, activateSpare_ReadCondition;
DDS_Subscriber appendEntries_Subscriber, appendEntriesReply_Subscriber, requestVote_Subscriber, requestVoteReply_Subscriber, activateSpare_Subscriber;
DDS_DataReader appendEntries_DataReader, appendEntriesReply_DataReader, requestVote_DataReader, requestVoteReply_DataReader, activateSpare_DataReader;
DDS_Publisher appendEntries_Publisher, appendEntriesReply_Publisher, requestVote_Publisher, requestVoteReply_Publisher, activateSpare_Publisher;
DDS_DataWriter appendEntries_DataWriter, appendEntriesReply_DataWriter, requestVote_DataWriter, requestVoteReply_DataWriter, activateSpare_DataWriter;
DDS_ConditionSeq *appendEntries_GuardList, *appendEntriesReply_GuardList, *collectVotes_GuardList, *leaderElection_GuardList, *activateSpare_GuardList;

/**
 * Create all DDS Entities for the consensus module
 */
void DDSSetupConsensus();

/**
 * Delete all DDS Entities of the consensus module
 */
void DDSConsensusCleanup();

#endif