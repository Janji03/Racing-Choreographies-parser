grammar RacingChoreo;

program
  : procDef* mainDef EOF
  ;

mainDef
  : MAIN block
  ;

procDef
  : PROC procName LPAREN procParams RPAREN block
  ;

procParams
  : process (COMMA process)*
  ;

block
  : LBRACE stmt* RBRACE
  ;

stmt
  : interactionStmt
  | callStmt
  | ifLocalStmt
  | ifRaceStmt
  ;

ifLocalStmt
  : IF LPAREN procExpr RPAREN block ELSE block
  ;

ifRaceStmt
  : IF LPAREN raceId RPAREN block ELSE block
  ;

callStmt
  : CALL procName LPAREN procArgs RPAREN SEMI
  ;

procArgs
  : process (COMMA process)*
  ;

interactionStmt
  : interaction SEMI
  ;

interaction
  : comm
  | select
  | assign
  | race
  | discharge
  ;

comm
  : procExpr ARROW procVar
  ;

select
  : process ARROW process LBRACK label RBRACK
  ;

assign
  : procVar ASSIGN expr
  ;

race
  : RACE raceId COLON procExpr COMMA procExpr ARROW procVar
  ;

discharge
  : DISCHARGE raceId COLON process ARROW procVar
  ;

procExpr
  : process DOT expr
  ;

expr
  : value
  | var
  ;

procVar
  : process DOT var
  ;

procName : ID ;
process  : ID ;
var      : ID ;
label    : ID ;


raceId
  : process LBRACK raceKey RBRACK
  ;

raceKey
  : ID
  ;


value
  : INT
  | TRUE
  | FALSE
  ;


MAIN      : 'main' ;
PROC      : 'proc' ;
CALL      : 'call' ;
IF        : 'if' ;
ELSE      : 'else' ;
RACE      : 'race' ;
DISCHARGE : 'discharge' ;

TRUE      : 'true' ;
FALSE     : 'false' ;


ARROW  : '->' ;
ASSIGN : '=' ;
DOT    : '.' ;
COMMA  : ',' ;
COLON  : ':' ;
SEMI   : ';' ;

LPAREN : '(' ;
RPAREN : ')' ;
LBRACE : '{' ;
RBRACE : '}' ;
LBRACK : '[' ;
RBRACK : ']' ;


INT : [0-9]+ ;


ID : [a-zA-Z] [a-zA-Z0-9_]* ;

WS            : [ \t\r\n]+ -> skip ;
LINE_COMMENT  : '//' ~[\r\n]* -> skip ;
BLOCK_COMMENT : '/*' .*? '*/' -> skip ;
