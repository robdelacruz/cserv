#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>
#include <crypt.h>

#include "sqlite3.h"
#include "clib.h"
#include "db.h"

extern sqlite3 *db;

#define CRYPTSALT "salt1234567890"

String password_hash(String phrase) {
    if (phrase.len > CRYPT_MAX_PASSPHRASE_SIZE)
        phrase.bs[CRYPT_MAX_PASSPHRASE_SIZE] = 0;

    struct crypt_data data;
    memset(&data, 0, sizeof(data));
    char *pz = crypt_r(phrase.bs, CRYPTSALT, &data);
    assert(pz != NULL);

    return StringNew(data.output);
}
int password_verify(String phrase, String hash) {
    if (phrase.len > CRYPT_MAX_PASSPHRASE_SIZE)
        phrase.bs[CRYPT_MAX_PASSPHRASE_SIZE] = 0;

    struct crypt_data data;
    memset(&data, 0, sizeof(data));
    char *pz = crypt_r(phrase.bs, CRYPTSALT, &data);
    assert(pz != NULL);

    return StringEquals(hash, data.output);
}

void generate_token(String username, String pwd, String *tok) {
    String s = StringDup(username);
    StringAppend(&s, pwd.bs);
    String hash = password_hash(s);

    StringAssign(tok, hash.bs);

    StringFree(&s);
    StringFree(&hash);
}

void initdb(char *dbfile) {
    char *s;
    char *errstr=NULL;
    int z = sqlite3_open(dbfile, &db);
    if (z != 0)
        panic((char *) sqlite3_errmsg(db));

    s = "CREATE TABLE IF NOT EXISTS user (userid INTEGER PRIMARY KEY NOT NULL, username TEXT NOT NULL, password TEXT NOT NULL);"
        "CREATE TABLE IF NOT EXISTS msg (msgid INTEGER PRIMARY KEY NOT NULL, date INTEGER, text TEXT NOT NULL, userid_from INTEGER NOT NULL, userid_to INTEGER NOT NULL);";
    z = sqlite3_exec(db, s, 0, 0, &errstr);
    if (z != 0)
        panic(errstr);
}

int RegisterUser(String username, String pwd, String *tok) {
    char *s;
    sqlite3_stmt *stmt;

    StringAssign(tok, "");

    // Return error if username already exists.
    s = "SELECT userid FROM user WHERE username = ?";
    sqlite3_prepare_v2(db, s, -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, username.bs, -1, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return -1;
    }

    // Create new user
    String pwdhash = password_hash(pwd);
    s = "INSERT INTO user (username, password) VALUES (?, ?);";
    sqlite3_prepare_v2(db, s, -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, username.bs, -1, NULL);
    sqlite3_bind_text(stmt, 2, pwdhash.bs, -1, NULL);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        StringFree(&pwdhash);
        return -2;
    }
    StringFree(&pwdhash);
    sqlite3_finalize(stmt);

    generate_token(username, pwd, tok);
    return 0;
}

int LoginUser(String username, String pwd, String *tok) {
    printf("LoginUser() username: '%s' pwd: '%s'\n", username.bs, pwd.bs);
    char *s;
    sqlite3_stmt *stmt;

    StringAssign(tok, "");

    // Return error if username doesn't exist.
    s = "SELECT password FROM user WHERE username = ?";
    sqlite3_prepare_v2(db, s, -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, username.bs, -1, NULL);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return -1;
    }

    // Return error if wrong password.
    char *pwdhash = (char *) sqlite3_column_text(stmt, 0);
    if (!password_verify(pwd, STRING(pwdhash))) {
        sqlite3_finalize(stmt);
        return -2;
    }

    sqlite3_finalize(stmt);
    generate_token(username, pwd, tok);
    return 0;
}

