#include "replica.h"

#include <time.h>
#include <stdlib.h>

#include "datamodel.h"
#include "DDSConsensusManager.h"
#include "DIO/DIO.h"
#include "state/train.h"
#include "state/DDSStateManager.h"
#include "leaderElection.h"
#include "spare.h"

#include "evaluation/evaluator.h"


void *runElectionTimer() {

    DDS_sequence_RevPiDDS_AppendEntries msgSeq  = {0, 0, DDS_OBJECT_NIL, FALSE};
    DDS_SampleInfoSeq                   infoSeq = {0, 0, DDS_OBJECT_NIL, FALSE};
    DDS_ReturnCode_t status;
    uint32_t received_Term = 0;
    uint8_t sender_ID;
    DDS_sequence_char entries_Seq;
    RevPiDDS_AppendEntriesReply* appendEntriesReply_message;


    while (true) {

        if (this_replica->role == SPARE) {
            printf("Wait until activated\n");
            bool is_active = spare_wait_until_activated();

            if (!is_active) {
                printf("Continued\n");
                continue;
            }
        }

        status = DDS_WaitSet_wait(appendEntries_WaitSet, appendEntries_GuardList, &this_replica->election_timeout);

        pthread_mutex_lock(&this_replica->consensus_mutex);


        if (status == DDS_RETCODE_OK) {
            status = RevPiDDS_AppendEntriesDataReader_take_w_condition(
                appendEntries_DataReader,
                &msgSeq,
                &infoSeq,
                DDS_LENGTH_UNLIMITED,
                electionTimer_ReadCondition
            );
            checkStatus(status, "RevPiDDS_AppendEntriesDataReader_take (election Timer)");
            if (this_replica->role == SPARE) {
                pthread_mutex_unlock(&this_replica->consensus_mutex);
                printf("Continue because of being spare\n");
                continue;
            }
            if (msgSeq._length > 0) {

                for (DDS_unsigned_long i = 0; i < msgSeq._length; ++i) {
                    if (infoSeq._buffer[i].valid_data) {
                        received_Term = msgSeq._buffer[i].term;
                        entries_Seq = msgSeq._buffer[i].entries;
                        sender_ID = msgSeq._buffer[i].senderID;

                        if (sender_ID == this_replica->ID) {
                            continue;
                        }

                        if (received_Term > this_replica->current_term) {
                            become_follower(received_Term);
                        }

                        bool success = false;

                        if (received_Term == this_replica->current_term) {
                            if (this_replica->role != FOLLOWER) {
                                become_follower(received_Term);
                            }

                            success = true;
                        }
                        if (entries_Seq._length > 0) {
                            printf("Got some new entries\n");
                            uint32_t id = msgSeq._buffer[i].id;

                            if (this_replica->ID >= ACTIVE_REPLICAS) {
                                bool is_active = activate_when_promted();
                                if (!is_active) {
                                    break;
                                }
                            }
                            for (DDS_unsigned_long entry_index = 0; entry_index < entries_Seq._length; ++entry_index) {
                                printf("Got the following entry: ID: %d\n", id);
                                appendEntriesReply_message = RevPiDDS_AppendEntriesReply__alloc();
                                appendEntriesReply_message->id = id;
                                appendEntriesReply_message->senderID = this_replica->ID;
                                appendEntriesReply_message->term = this_replica->current_term;
                                appendEntriesReply_message->success = success;

                                status = RevPiDDS_AppendEntriesReplyDataWriter_write(appendEntriesReply_DataWriter, appendEntriesReply_message, DDS_HANDLE_NIL);
                                checkStatus(status, "RevPiDDS_AppendEntriesReplyDataWriter write AppendEntriesReply");
                                DDS_free(appendEntriesReply_message);
                            }
                        }
                    }
                    printf("Election Timer received with term %d\n", this_replica->current_term);
                }
            } else {
                printf("Waitset election timer triggered again but DR is completely empty\n");
            }

            pthread_mutex_unlock(&this_replica->consensus_mutex);
            RevPiDDS_AppendEntriesDataReader_return_loan(appendEntries_DataReader, &msgSeq, &infoSeq);
        } else if (status == DDS_RETCODE_TIMEOUT) {
            if (this_replica->role == SPARE || this_replica->ID >= ACTIVE_REPLICAS) {
                printf("Election Timer timeouted but this is a spare unit\n");
                pthread_mutex_unlock(&this_replica->consensus_mutex);
                continue;
            }
            if (this_replica->role != FOLLOWER && this_replica->role != CANDIDATE) {
                printf("Election Timer timeouted but I'm leader\n");
                pthread_mutex_unlock(&this_replica->consensus_mutex);
                continue;
            }
#if MEASURE_NUM_ELECTION_TIMEOUTS
            evaluator_registered_election_timeout(this_replica->ID, this_replica->current_term);
#endif
            printf("No leader present in the system\n");
            evaluator_leader_election_started();
            run_leader_election();
        } else {
            checkStatus(status, "DDS_WaitSet_wait election Timer");
        }

    }

    pthread_exit(NULL);
}


void send_heartbeat(int signum, siginfo_t* info, void* args) {
    (void) signum;
    (void) info;
    (void) args;

    DDS_ReturnCode_t status;
    RevPiDDS_AppendEntries* appendEntries_message;

    appendEntries_message = RevPiDDS_AppendEntries__alloc();
    appendEntries_message->term = this_replica->current_term;
    appendEntries_message->senderID = this_replica->ID;

    printf("About to send heartbeat with term %d\n", appendEntries_message->term);
    status = RevPiDDS_AppendEntriesDataWriter_write(appendEntries_DataWriter, appendEntries_message, DDS_HANDLE_NIL);
    checkStatus(status, "RevPiDDS_AppendEntriesDataWriter_write appendEntries_message");

    update_train_state();

    DDS_free(appendEntries_message);
}

void become_leader() {

    if (this_replica->role == LEADER) {
        return;
    }

    printf("Make leader\n");
    // digital_write("O_1", 0);
    // digital_write("O_2", 1);
    this_replica->role = LEADER;
    this_replica->voted_for = VOTED_NONE;

    if (setitimer(ITIMER_REAL, &this_replica->heartbeat_timer, NULL) == -1) {
        perror("Error calling setitimer()");
        return;
    }
}

void become_follower(const uint32_t term) {

    printf("Make follower\n");
    // digital_write("O_1", 1);
    // digital_write("O_2", 0);
    this_replica->current_term = term;
    this_replica->role = FOLLOWER;
    this_replica->voted_for = VOTED_NONE;

    if (setitimer(ITIMER_REAL, NULL, NULL) == -1) {
        perror("Error calling setitimer()");
        return;
    }

    printf("Became follower with term: %d\n", this_replica->current_term);

}

void become_spare() {
    printf("Becoming a spare unit\n");
    this_replica->role = SPARE;

    if (setitimer(ITIMER_REAL, NULL, NULL) == -1) {
        perror("Error calling setitimer()");
        return;
    }
}

void initialize_replica(const uint8_t id) {

    DDSSetupConsensus();
    createLeaderElectionDDSFeatures(id);
    DDSSetupState();


    srand(time(NULL));
    DDS_unsigned_long id_dependent_timeout_sec = 0;
    DDS_unsigned_long id_dependent_timeout_nanosec = 700000000 + (id * 100000000);

    if (id_dependent_timeout_nanosec >= 1000000000) {
        id_dependent_timeout_sec = id_dependent_timeout_nanosec / 1000000000;
        id_dependent_timeout_nanosec = id_dependent_timeout_nanosec % 1000000000;
    }

    printf("Nanosec: %d Sec: %d", id_dependent_timeout_nanosec, id_dependent_timeout_sec);

    this_replica = malloc(sizeof(replica_t));

    this_replica->ID = id;

    sigemptyset(&this_replica->heartbeat_action.sa_mask);
    this_replica->heartbeat_action.sa_sigaction = &send_heartbeat;
    sigaction (SIGALRM, &this_replica->heartbeat_action, NULL);

    this_replica->heartbeat_timer.it_value.tv_sec = 0;
    this_replica->heartbeat_timer.it_value.tv_usec = 500000;

    this_replica->heartbeat_timer.it_interval.tv_sec = 0;
    this_replica->heartbeat_timer.it_interval.tv_usec = 500000;

    this_replica->election_timeout.sec = id_dependent_timeout_sec;
    this_replica->election_timeout.nanosec = id_dependent_timeout_nanosec;

    pthread_mutex_init(&this_replica->consensus_mutex, NULL);

    if (id < ACTIVE_REPLICAS) {
        become_follower(0);
    } else {
        printf("Replica is a spare unit\n");
        become_spare();
    }

    if (pthread_create(&this_replica->election_timer_thread, NULL, runElectionTimer, NULL) != 0) {
        printf("Error creating election timer thread\n");
        exit (-1);
    }

    pthread_create(&this_replica->request_vote_thread, NULL, receive_vote_requests, NULL);

}

void cluster_process(RevPiDDS_Input* handle, void(*on_result)(RevPiDDS_Input* handle, const replica_result_t* result, const size_t length), void(*on_fail)(void)) {
    RevPiDDS_AppendEntries* appendEntries_message;
    DDS_sequence_RevPiDDS_AppendEntriesReply msgSeq  = {0, 0, DDS_OBJECT_NIL, FALSE};
    DDS_SampleInfoSeq                   infoSeq = {0, 0, DDS_OBJECT_NIL, FALSE};
    DDS_ReturnCode_t status;
    uint32_t received_Term = 0;
    uint32_t reply_ID;
    uint8_t sender_ID;
    bool success = false;
    uint8_t num_replies = 0;
    uint8_t num_timeouts = 0;
    replica_result_t replies[ACTIVE_REPLICAS + NUM_SPARES];
    // Minimal number of available results to get a majority in the cluster
    uint8_t min_required_results = (uint8_t)(floor((double) ACTIVE_REPLICAS / 2.0)) + 1;
    DDS_Duration_t timeout = {0 , (long)500000000 / min_required_results};
    uint32_t input_ID = handle->id;


    appendEntries_message = RevPiDDS_AppendEntries__alloc();
    appendEntries_message->senderID = this_replica->ID;
    appendEntries_message->term = this_replica->current_term;
    appendEntries_message->id = input_ID;
    // uint8_t payload_size = 1;
    appendEntries_message->entries = handle->data;
    // appendEntries_message->entries._length = payload_size;
    // appendEntries_message->entries._maximum = payload_size;
    // appendEntries_message->entries._buffer = DDS_sequence_char_allocbuf(payload_size);
    // appendEntries_message->entries._buffer = handle->data._buffer;
    status = RevPiDDS_AppendEntriesDataWriter_write(appendEntries_DataWriter, appendEntries_message, DDS_HANDLE_NIL);
    checkStatus(status, "RevPiDDS_AppendEntriesDataWriter_write appendEntries_message");

    replica_result_t reply;
    reply.replica_id = this_replica->ID;
    reply.term = this_replica->current_term;
    replies[num_replies] = reply;
    num_replies++;

    // TODO Wait until enough data is ready.
    // If timeouts, see how many data there is. When arrived data is sufficient, call "on_result", if not call "on_fail"

    while (true) {
        status = DDS_WaitSet_wait(appendEntriesReply_WaitSet, appendEntriesReply_GuardList, &timeout);
        printf("Waitset collect results triggered\n");

        if (status == DDS_RETCODE_OK) {
            status = RevPiDDS_AppendEntriesReplyDataReader_take_w_condition(
                appendEntriesReply_DataReader,
                &msgSeq,
                &infoSeq,
                DDS_LENGTH_UNLIMITED,
                appendEntriesReply_ReadCondition
            );
            checkStatus(status, "RevPiDDS_AppendEntriesReplyDataReader_take");

            if (msgSeq._length > 0) {

                for (DDS_unsigned_long i = 0; i < msgSeq._length; ++i) {
                    if (infoSeq._buffer[i].valid_data) {
                        received_Term = msgSeq._buffer[i].term;
                        sender_ID = msgSeq._buffer[i].senderID;
                        reply_ID = msgSeq._buffer[i].id;
                        success = msgSeq._buffer[i].success;

                        if (sender_ID == this_replica->ID) {
                            continue;
                        }

                        printf("Got some new appendEntriesReply Data from %d - ReplyID: %d Term: %d Success: %d\n", sender_ID, reply_ID, received_Term, success);
                        if (success && reply_ID == input_ID) {
                            replica_result_t reply;
                            reply.replica_id = sender_ID;
                            reply.term = received_Term;
                            replies[num_replies] = reply;
                            num_replies++;
                        }
                    }
                }

            }
            RevPiDDS_AppendEntriesReplyDataReader_return_loan(appendEntriesReply_DataReader, &msgSeq, &infoSeq);

        } else {
            num_timeouts++;
            if (num_timeouts + num_replies > min_required_results) {
                break;
            }
        }
    }

    if (num_replies >= min_required_results) {

        if (num_replies != ACTIVE_REPLICAS) {
            bool activate_spare = num_replies < ACTIVE_REPLICAS;

            printf("Activating spare: %d\n", activate_spare);

            RevPiDDS_ActivateSpare *activateSpare_message = RevPiDDS_ActivateSpare__alloc();
            activateSpare_message->term = this_replica->current_term;
            activateSpare_message->activate = activate_spare;
            status = RevPiDDS_ActivateSpareDataWriter_write(activateSpare_DataWriter, activateSpare_message, DDS_HANDLE_NIL);
            checkStatus(status, "RevPiDDS_ActivateSpareDataWriter_write activateSpare!");
        }

        pthread_mutex_lock(&this_replica->consensus_mutex);
        on_result(handle, replies, num_replies);
        pthread_mutex_unlock(&this_replica->consensus_mutex);
    } else {
        on_fail();
    }

    DDS_free(appendEntries_message);
}

void teardown_replica() {
    DDSConsensusCleanup();

    pthread_cancel(this_replica->request_vote_thread);

    free(this_replica);
}