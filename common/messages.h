/*
  0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
|               CODEREQ                |  ID |EQ|
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
*/

typedef struct MessageHeader {
    uint16_t header_line;
} MessageHeader;

#define SET_CODEREQ(header, codereq) \
    (header)->header_line = ((header)->header_line & 0xE000) | codereq

#define GET_CODEREQ(header) \
    (header)->header_line & 0x1FFF

#define SET_ID(header, id) \
    (header)->header_line = ((header)->header_line & 0x9FFF) | (id << 13)

#define GET_ID(header) \
    ((header)->header_line & 0x6000) >> 13

#define SET_EQ(header, eq) \
    (header)->header_line = ((header)->header_line & 0x7FFF) | (eq << 15)

#define GET_EQ(header) \
    ((header)->header_line & 0x8000) >> 15

/*
  0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
|               CODEREQ                |  ID |EQ|
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
|                  NUM                 | ACTION |
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
*/

typedef struct ActionMessage {
    MessageHeader message_header;
    uint16_t action_identifier;
} ActionMessage;

#define NUM_MAX 8191

// We use this constant such that a message with num [0, OVERFLOW_DETECTION_BUFFER]
// is considered greater than a message with num [NUM_MAX - OVERFLOW_DETECTION_BUFFER, NUM_MAX]
#define OVERFLOW_DETECTION_BUFFER 16

/* Possible actions */

#define MOVE_NORTH 0
#define MOVE_EAST 1
#define MOVE_SOUTH 2
#define MOVE_WEST 3
#define DROP_BOMB 4
#define CANCEL_LATEST_MOVE 5
#define TCHAT_MESSAGE 6

#define SET_NUM(message, num) \
    (message)->action_identifier = ((message)->action_identifier & 0xE000) | num

#define GET_NUM(message) \
    (message)->action_identifier & 0x1FFF

#define SET_ACTION(message, action) \
    (message)->action_identifier = ((message)->action_identifier & 0x1FFF) | (action << 13)

#define GET_ACTION(message) \
    ((message)->action_identifier & 0xE000) >> 13

/*
  0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
|               CODEREQ                |  ID |EQ|
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
|          LEN          |         DATA ...
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
*/

#define SIZE_MAX_MESSAGE 150

typedef struct TChatHeader {
    MessageHeader header;
    uint8_t data_len;
} TChatHeader;


/*
    Message sent to the client with the match information.
    
    Parameters (all in big-endian fromat):
        - port_udp: the port used for the UDP connection
        - port_mdiff: the port used for the MDIFF connection
        - adr_mdiff: the address used for the MDIFF connection

      0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |               CODEREQ                |  ID |EQ|
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                     PORTUDP                   |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                    PORTMDIFF                  |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |            ADRMDIFF ... (16 octets)
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
*/

typedef struct NewMatchMessage {
    MessageHeader header;
    uint16_t port_udp;
    uint16_t port_mdiff;
    uint8_t adr_mdiff[16];
} NewMatchMessage;

/*
    Message sent to the client with the match status
    that encodes all of the grid information.
    
    After the header, each following byte describes the status
    of a cell in the grid. The status is encoded as follows:            
        - 0 if the cell is empty,
        - 1 if the cell is an indestructible wall,
        - 2 if the cell is a destructible wall,
        - 3 if the cell contains a bomb,
        - 4 if the cell is exploded by a bomb,
        - 5 + i if the cell contains the player with identifier i, where 0 <= i <= 3.

      0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |               CODEREQ                |  ID |EQ|
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                      NUM                      |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |         HAUTEUR       |        LARGEUR        |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |         CASE0         |          ...
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
*/

typedef struct MatchFullUpdateHeader {
    MessageHeader header;
    uint16_t num;
    uint8_t height;
    uint8_t width;
} MatchFullUpdateHeader;
    
#define EMPTY_CELL 0
#define INDESTRUCTIBLE_WALL 1
#define DESTRUCTIBLE_WALL 2
#define BOMB 3
#define EXPLODED_BY_BOMB 4

#define PLAYER_OFFSET 5

#define ENCODE_PLAYER(player) \
    PLAYER_OFFSET + player

#define DECODE_PLAYER(cell) \
    cell - PLAYER_OFFSET

typedef struct CellStatusUpdate {
    uint8_t row;
    uint8_t col;
    uint8_t status;
} CellStatusUpdate;

typedef struct MatchUpdateHeader {
    MessageHeader header;
    uint16_t num;
    uint8_t count;
} MatchUpdateHeader;

/* CODEREQ definitions */
#define NEW_MATCH_4_OPPONENTS 1
#define NEW_MATCH_2_TEAMS 2

#define CLIENT_READY_TO_PLAY_4_OPPONENTS 3
#define CLIENT_READY_TO_PLAY_2_TEAMS 4

#define ACTION_MESSAGE_4_OPPONENTS 5
#define ACTION_MESSAGE_2_TEAMS 6

#define T_CHAT_ALL_PLAYERS 7
#define T_CHAT_TEAM 8

#define SERVER_RESPONSE_MATCH_START_4_OPPONENTS 9
#define SERVER_RESPONSE_MATCH_START_2_TEAMS 10

#define SERVER_FULL_MATCH_STATUS 11
#define SERVER_PARTIAL_MATCH_UPDATE 12

#define SERVER_TCHAT_SENT_ALL_PLAYERS 13
#define SERVER_TCHAT_SENT_TEAM 14

#define SERVER_RESPONSE_MATCH_END_4_OPPONENTS 15
#define SERVER_RESPONSE_MATCH_END_2_TEAMS 16
