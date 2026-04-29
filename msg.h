#ifndef MSG_H
#define MSG_H

#define REGISTER_REQUEST 10
#define LOGIN_REQUEST 11
#define LOGIN_RESPONSE 12

#define MSGNO(bs) (*((u8 *)bs))

// REGISTERMSG
typedef struct {
    u8 msgno;
    String alias;
    String pwd;
} RegisterRequest;

// LOGINMSG
typedef struct {
    u8 msgno;
    String alias;
    String pwd;
} LoginRequest;

// LOGINRESPMSG
typedef struct {
    u8 msgno;
    String tok;
    u8 retno;
    String errorstr;
} LoginResponse;

typedef struct {
    u8 msgno;
    String alias;
    String message;
} ContactRequest;

#endif
