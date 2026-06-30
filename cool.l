%option noyywrap nodefault nounput noinput

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cool.tab.h"
#include "driver.h"

#define MAX_STRING_LENGTH 1024

static char string_buffer[MAX_STRING_LENGTH + 1];
static size_t string_length = 0U;
static const char *string_error_message = NULL;
static int comment_depth = 0;

static void string_start(void) {
    string_length = 0U;
    string_buffer[0] = '\0';
    string_error_message = NULL;
}

static int string_append(char character) {
    if (string_length >= MAX_STRING_LENGTH) {
        string_error_message = "String constant too long";
        return 0;
    }
    string_buffer[string_length++] = character;
    string_buffer[string_length] = '\0';
    return 1;
}

static int lexical_error(const char *message) {
    yylval.str = ast_strdup(message);
    set_lexical_error(yylval.str, curr_lineno);
    return T_ERROR;
}

static int invalid_character_error(const char *text) {
    char message[2];
    message[0] = text[0];
    message[1] = '\0';
    return lexical_error(message);
}
%}

%x COMMENT STRING STRING_ERROR

WS              [ \t\f\r\v]+
TYPE_IDENTIFIER [A-Z][A-Za-z0-9_]*
OBJECT_IDENTIFIER [a-z][A-Za-z0-9_]*
INTEGER         [0-9]+

CLASS_K         [cC][lL][aA][sS][sS]
ELSE_K          [eE][lL][sS][eE]
FI_K            [fF][iI]
IF_K            [iI][fF]
IN_K            [iI][nN]
INHERITS_K      [iI][nN][hH][eE][rR][iI][tT][sS]
ISVOID_K        [iI][sS][vV][oO][iI][dD]
LET_K           [lL][eE][tT]
LOOP_K          [lL][oO][oO][pP]
POOL_K          [pP][oO][oO][lL]
THEN_K          [tT][hH][eE][nN]
WHILE_K         [wW][hH][iI][lL][eE]
CASE_K          [cC][aA][sS][eE]
ESAC_K          [eE][sS][aA][cC]
NEW_K           [nN][eE][wW]
OF_K            [oO][fF]
NOT_K           [nN][oO][tT]
TRUE_K          t[rR][uU][eE]
FALSE_K         f[aA][lL][sS][eE]

REPIT        [rR][eE][pP][iI][tT]

UNTIL   [uU][nN][tT][iI][lL]

%%

<INITIAL>{WS}                   { /* Ignore horizontal whitespace. */ }
<INITIAL>\n                     { ++curr_lineno; }
<INITIAL>"--"[^\n]*            { /* One-line comment. */ }
<INITIAL>"(*"                   { comment_depth = 1; BEGIN(COMMENT); }
<INITIAL>"*)"                   { return lexical_error("Unmatched *)"); }

<COMMENT>"(*"                   { ++comment_depth; }
<COMMENT>"*)"                   { if (--comment_depth == 0) BEGIN(INITIAL); }
<COMMENT>\n                     { ++curr_lineno; }
<COMMENT>.                      { /* Ignore nested-comment content. */ }
<COMMENT><<EOF>>                { BEGIN(INITIAL); return lexical_error("EOF in comment"); }

<INITIAL>\"                    { string_start(); BEGIN(STRING); }
<STRING>\"                     {
                                    BEGIN(INITIAL);
                                    yylval.str = ast_strdup(string_buffer);
                                    return STR_CONST;
                                 }
<STRING>\n                      {
                                    ++curr_lineno;
                                    BEGIN(INITIAL);
                                    return lexical_error("Unterminated string constant");
                                 }
<STRING>\\\n                   { ++curr_lineno; if (!string_append('\n')) BEGIN(STRING_ERROR); }
<STRING>\\[b]                  { if (!string_append('\b')) BEGIN(STRING_ERROR); }
<STRING>\\[t]                  { if (!string_append('\t')) BEGIN(STRING_ERROR); }
<STRING>\\[n]                  { if (!string_append('\n')) BEGIN(STRING_ERROR); }
<STRING>\\[f]                  { if (!string_append('\f')) BEGIN(STRING_ERROR); }
<STRING>\\[0]                  { if (!string_append('0')) BEGIN(STRING_ERROR); }
<STRING>\\.                    {
                                    if ((unsigned char)yytext[1] == 0U) {
                                        string_error_message = "String contains null character";
                                        BEGIN(STRING_ERROR);
                                    } else if (!string_append(yytext[1])) {
                                        BEGIN(STRING_ERROR);
                                    }
                                 }
<STRING>.                       {
                                    if ((unsigned char)yytext[0] == 0U) {
                                        string_error_message = "String contains null character";
                                        BEGIN(STRING_ERROR);
                                    } else if (!string_append(yytext[0])) {
                                        BEGIN(STRING_ERROR);
                                    }
                                 }
<STRING><<EOF>>                 { BEGIN(INITIAL); return lexical_error("EOF in string constant"); }

<STRING_ERROR>\"               {
                                    BEGIN(INITIAL);
                                    return lexical_error(string_error_message);
                                 }
<STRING_ERROR>\n                {
                                    ++curr_lineno;
                                    BEGIN(INITIAL);
                                    return lexical_error(string_error_message);
                                 }
<STRING_ERROR>\\\n             { ++curr_lineno; }
<STRING_ERROR>\\.              { /* Continue until a real terminator. */ }
<STRING_ERROR>.                 { /* Continue until a real terminator. */ }
<STRING_ERROR><<EOF>>           { BEGIN(INITIAL); return lexical_error(string_error_message); }

<INITIAL>{CLASS_K}               { return CLASS; }
<INITIAL>{ELSE_K}                { return ELSE; }
<INITIAL>{FI_K}                  { return FI; }
<INITIAL>{IF_K}                  { return IF; }
<INITIAL>{IN_K}                  { return IN; }
<INITIAL>{INHERITS_K}            { return INHERITS; }
<INITIAL>{ISVOID_K}              { return ISVOID; }
<INITIAL>{LET_K}                 { return LET; }
<INITIAL>{LOOP_K}                { return LOOP; }
<INITIAL>{POOL_K}                { return POOL; }
<INITIAL>{THEN_K}                { return THEN; }
<INITIAL>{WHILE_K}               { return WHILE; }
<INITIAL>{CASE_K}                { return CASE; }
<INITIAL>{ESAC_K}                { return ESAC; }
<INITIAL>{NEW_K}                 { return NEW; }
<INITIAL>{OF_K}                  { return OF; }
<INITIAL>{NOT_K}                 { return NOT; }
<INITIAL>{TRUE_K}                { return TRUE; }
<INITIAL>{FALSE_K}               { return FALSE; }
<INITIAL>{REPIT}                 { return REPIT; }
<INITIAL>{UNTIL}                 { return UNTIL; }

<INITIAL>"<-"                   { return ASSIGN; }
<INITIAL>"=>"                   { return DARROW; }
<INITIAL>"<="                   { return LE; }
<INITIAL>"^^"                   { return POW; }
<INITIAL>{TYPE_IDENTIFIER}       { yylval.str = ast_strdup(yytext); return TYPEID; }
<INITIAL>{OBJECT_IDENTIFIER}     { yylval.str = ast_strdup(yytext); return OBJECTID; }
<INITIAL>{INTEGER}               { yylval.str = ast_strdup(yytext); return INT_CONST; }
<INITIAL>[{}();:,.@+*/~<>=-]     { return yytext[0]; }
<INITIAL>.                       { return invalid_character_error(yytext); }

%%
