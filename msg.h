#ifndef MSG_H
#define MSG_H

#define REGISTERMSG 10
#define LOGINMSG 11
#define LOGINRESPMSG 12

#define MSGNO(bs) (*((u8 *)bs))

// REGISTERMSG
//   u8 msgno;
//   String alias;
//   String pwd;

// LOGINMSG
//   u8 msgno;
//   String alias;
//   String pwd;

// LOGINRESPMSG
//   u8 msgno;
//   String tok;
//   u8 retno;
//   String errorstr;

typedef struct {
    u8 msgno;
    String tok;
    u8 retno;
    String errorstr;
} LoginResponse;

#endif
