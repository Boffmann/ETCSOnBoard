#include "publisher.h"
#include "CheckStatus.h"

// TODO Set default value of has_dataWriter to false
// TODO Also add has_dataReader to listener

publisher_t publisher_create_new(const domain_participant_t* domain_participant, const DDS_PublisherQos* pub_qos) {

    publisher_t new_publisher;

    /* Create a Publisher for the application. */
    new_publisher.dds_publisher = DDS_DomainParticipant_create_publisher(
        domain_participant->dds_domainParticipant,
        pub_qos,
        NULL,
        DDS_STATUS_MASK_NONE
    );
    checkHandle(new_publisher.dds_publisher, "DDS_DomainParticipant_create_publisher");

    return new_publisher;
}

void publisher_dataWriter_create_new(publisher_t* publisher, const DDS_DataWriterQos* dw_qos, const topic_t* topic) {

    DDS_DataWriter dataWriter;

    dataWriter = DDS_Publisher_create_datawriter(
        publisher->dds_publisher,
        topic->dds_topic,
        dw_qos,
        NULL,
        DDS_STATUS_MASK_NONE
    );
    checkHandle(dataWriter, "DDS_Publisher_create_datawriter");

    // DDS_free(dataWriter_qos);

    publisher->has_dataWriter = true;
    publisher->dds_dataWriter = dataWriter;
}

void publisher_cleanup(publisher_t* publisher, const domain_participant_t* domain_participant) {

    DDS_ReturnCode_t status;

    if (publisher->has_dataWriter) {
        status = DDS_Publisher_delete_datawriter(publisher->dds_publisher, publisher->dds_dataWriter);
        checkStatus(status, "DDS_Publisher_delete_datawriter");
        publisher->has_dataWriter = false;
    }
    
    status = DDS_DomainParticipant_delete_publisher(
        domain_participant->dds_domainParticipant,
        publisher->dds_publisher
    );
    checkStatus(status, "DDS_DomainParticipant_delete_publisher");

}

DDS_DataWriterQos* dw_qos_copy_from_topic_qos(const publisher_t* publisher, const topic_t* topic) {

    DDS_TopicQos *topic_qos = DDS_TopicQos__alloc();
    DDS_DataWriterQos *dataWriter_qos = DDS_DataWriterQos__alloc();
    checkHandle(dataWriter_qos, "DDS_DataWriterQos__alloc");
    DDS_ReturnCode_t status = DDS_Publisher_get_default_datawriter_qos(publisher->dds_publisher, dataWriter_qos);
    checkStatus(status, "DDS_Publisher_get_default_datawriter_qos");
    status = DDS_Topic_get_qos(topic->dds_topic, topic_qos);
    checkStatus(status, "DDS_Topic_get_qos");
    status = DDS_Publisher_copy_from_topic_qos(publisher->dds_publisher, dataWriter_qos, topic_qos);
    checkStatus(status, "DDS_Publisher_copy_from_topic_qos");
    dataWriter_qos->writer_data_lifecycle.autodispose_unregistered_instances = FALSE;

    DDS_free(topic_qos);

    return dataWriter_qos;
}

DDS_PublisherQos* get_default_publisher_qos(const domain_participant_t* domain_participant) {

    DDS_PublisherQos* publisher_qos = DDS_PublisherQos__alloc();
    checkHandle(publisher_qos, "DDS_PublisherQos__alloc");
    DDS_ReturnCode_t status = DDS_DomainParticipant_get_default_publisher_qos(domain_participant->dds_domainParticipant, publisher_qos);
    checkStatus(status, "DDS_DomainParticipant_get_default_publisher_qos");
    publisher_qos->partition.name._length = 1;
    publisher_qos->partition.name._maximum = 1;
    publisher_qos->partition.name._release = TRUE;
    publisher_qos->partition.name._buffer = DDS_StringSeq_allocbuf(1);
    checkHandle(publisher_qos->partition.name._buffer, "DDS_StringSeq_allocbuf");
    publisher_qos->partition.name._buffer[0] = DDS_string_dup(domain_participant->partition_name);
    checkHandle(publisher_qos->partition.name._buffer[0], "DDS_string_dup");

    return publisher_qos;

}