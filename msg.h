#ifndef MSG_H
#define MSG_H

#define REGISTER_REQUEST 10
#define LOGIN_REQUEST 11
#define LOGIN_RESPONSE 12

#define MSGNO(bs) (*((u8 *)bs))

// REGISTERMSG
typedef struct {
    u8 msgno;
    String username;
    String pwd;
} RegisterUserRequest;

// LOGINMSG
typedef struct {
    u8 msgno;
    String username;
    String pwd;
} LoginUserRequest;

// LOGINRESPMSG
typedef struct {
    u8 msgno;
    String tok;
    u8 retno;
    String errortext;
} LoginUserResponse;

typedef struct {
    u8 msgno;
    String tok;
} GetContactsRequest;

typedef struct {
    u8 msgno;
    String tok;
} GetOnlineContactsRequest;

typedef struct {
    u8 msgno;
    String tok;
    String searchtext;
} SearchUsernameRequest;

typedef struct {
    u8 msgno;
    String usernames;
} UsernamesResponse;

typedef struct {
    u8 msgno;
    String tok;
    String username;
    String message;
} AddContactRequest;

typedef struct {
    u8 msgno;
    u8 retno;
    String errortext;
} StatusResponse;

#endif
