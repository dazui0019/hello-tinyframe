#ifndef COMM_PROTOCOL_H_
#define COMM_PROTOCOL_H_

/* Application command types (TinyFrame TYPE field). */
#define COMM_TYPE_ECHO     0x01
#define COMM_TYPE_LED_CTRL 0x02

/* Shared application command results; wire values are one byte. */
typedef enum {
    COMM_RESULT_OK           = 0x00,
    COMM_RESULT_FAILED       = 0x01,
    COMM_RESULT_INVALID      = 0x02,
    COMM_RESULT_NOT_EXECUTED = 0x03
} comm_result_t;

#endif /* COMM_PROTOCOL_H_ */
