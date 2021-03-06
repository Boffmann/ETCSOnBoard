#ifndef __DDSSTATEMANAGER_H__
#define __DDSSTATEMANAGER_H__

#include "dds_dcps.h"
#include "DDSCreator/CheckStatus.h"
#include "DDSCreator/DDSEntitiesCreator.h"

/**
 * DDS Entities used in the state component
 */
DDS_Topic linkedBalises_Topic, movementAuthority_Topic, trainState_Topic;
DDS_Subscriber linkedBalises_Subscriber, movementAuthority_Subscriber, trainState_Subscriber;
DDS_DataReader linkedBalises_DataReader, movementAuthority_DataReader, trainState_DataReader;
DDS_Publisher linkedBalises_Publisher, movementAuthority_Publisher, trainState_Publisher;
DDS_DataWriter linkedBalises_DataWriter, movementAuthority_DataWriter, trainState_DataWriter;

/**
 * Setup state component DDS Entities
 */
void DDSSetupState();

/**
 * Delete used DDS Entities and free utilized memory
 */
void DDSStateCleanup();

#endif