#ifndef DB_H
#define DB_H

String password_hash(String phrase);
int password_verify(String phrase, String hash);
void generate_token(String username, String pwd, String *tok);

void initdb(char *dbfile);
int RegisterUser(String username, String pwd, String *tok);
int LoginUser(String username, String pwd, String *tok);

#endif

