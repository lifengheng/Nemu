#include "nemu.h"

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <sys/types.h>
#include <regex.h>
#include <stdlib.h>

#define EVAL_ERROR 0x0fffffff

uint32_t eval(int p, int q);

enum {
  TK_NOTYPE = 256, TK_EQ, TK_NEQ, TK_DEC, TK_HEX, TK_AND, TK_OR, TK_REG, TK_DEREF, TK_MINUS

  /* TODO: Add more token types */

};

static struct rule {
  char *regex;
  int token_type;
  int priority;
} rules[] = {

  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */

  {" +", TK_NOTYPE, 0},    // spaces
  {"\\(", '(', 1},         // left parenthese
  {"\\)", ')', 1},         // right parenthese

  {"\\+", '+', 5},         // plus
  {"\\-", '-', 5},         // subtract
  {"\\*", '*', 3},         // multiple
  {"/", '/', 3},           // divide
  
  {"\\b[0-9]+\\b", TK_DEC, 0},   // Decimal
  {"\\b0[Xx][0-9A-Fa-f]+\\b", TK_HEX, 0}, //Hex

  {"==", TK_EQ, 7},        // equal 
  {"!=", TK_NEQ, 7},       // neq
  {"&&", TK_AND, 11},      // logic and
  {"\\|\\|", TK_OR, 12},   // logic or
  
  {"\\$[a-zA-Z]+", TK_REG, 0},  // register
};

#define NR_REGEX (sizeof(rules) / sizeof(rules[0]) )

static regex_t re[NR_REGEX];

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
  int priority;
} Token;

Token tokens[1080];
int nr_token;

static void my_strcpy(char *dest, char *src, int n) {
  strncpy(dest, src, n);
  dest[n] = '\0';
  return;
}

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        // char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        // Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
        //     i, rules[i].regex, position, substr_len, substr_len, substr_start);
        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

        switch (rules[i].token_type) {
          case TK_DEC: {
            tokens[nr_token].type = TK_DEC;
            my_strcpy(tokens[nr_token].str, e + position - substr_len, substr_len);
            // printf("fuck1!\n");
            nr_token += 1;
            break;
          };
          case TK_HEX: {
            tokens[nr_token].type = TK_HEX;
            my_strcpy(tokens[nr_token].str, e + position - substr_len, substr_len);
            // printf("fuck2!\n");
            nr_token += 1;
            break;
          }
          case TK_REG: {
            tokens[nr_token].type = TK_REG;
            my_strcpy(tokens[nr_token].str, e + position - substr_len + 1, substr_len - 1);
            // printf("fuck3!\n");
            nr_token += 1;
            break;
          }
          case TK_NOTYPE: break;
          default: {
            tokens[nr_token].type = rules[i].token_type;
            tokens[nr_token].priority = rules[i].priority;
            // printf("fuck4!\n");
            nr_token += 1;
          };
        }
        break;
      }
    }
    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  // for (i = 0 ; i < nr_token; ++i) {
  //   printf("%s ", tokens[i].str);
  // }
  // printf("\n");

  int nr_parentheses = 0;
  for (i = 0 ; i < nr_token ; ++i) {
    if (tokens[i].type == '(') nr_parentheses++;
    if (tokens[i].type == ')') nr_parentheses--;
    if (nr_parentheses < 0) { 
      // printf("1\n");
      return false;
    }
  }
  if (nr_parentheses != 0) {return false;}
  if (tokens[0].type == ')' || tokens[nr_token - 1].type == '(') {return false;}
  for (i = 0 ; i < nr_token - 1 ; ++i) {
    if (tokens[i + 1].type == ')' && !(tokens[i].type == TK_REG || tokens[i].type == TK_HEX || tokens[i].type == TK_DEC || tokens[i].type == ')')) {return false;}
    if (tokens[i].type == '(' && 
    !(tokens[i + 1].type == TK_REG || tokens[i + 1].type == TK_HEX || tokens[i + 1].type == TK_DEC || tokens[i + 1].type == '(' || tokens[i + 1].type == '*' || tokens[i + 1].type == '-' )) {return false;}
  }

  return true;
}

static inline bool check_parentheses(int p, int q) {
  int i, nr_parentheses = 0;
  if (!(tokens[p].type == '(' && tokens[q].type == ')')) return false;
  for (i = p ; i <= q; ++i) {
    if (tokens[i].type == '(') nr_parentheses++;
    if (tokens[i].type == ')') {
      nr_parentheses--;
      if (nr_parentheses == 0 && i != q) 
        return false;
    }
  }
  return true;
}

uint32_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    printf("Bad expression.\n");
    return 0;
  }

  int i;

  // for (i = 0 ; i < nr_token; ++i) {
  //   printf("%s ", tokens[i].str);
  // }
  // printf("\n");

  for (i = 0; i < nr_token; i ++) {
    int t = tokens[i - 1].type;
    if (tokens[i].type == '*' && (i == 0 || !(t == ')' || t == TK_REG || t == TK_DEC || t == TK_HEX))) {
      tokens[i].type = TK_DEREF;
      tokens[i].priority = 2;
    }
    if (tokens[i].type == '-' && (i == 0 || !(t == ')' || t == TK_REG || t == TK_DEC || t == TK_HEX))) {
      tokens[i].type = TK_MINUS;
      tokens[i].priority = 2;
    }
  }

  /* TODO: Insert codes to evaluate the expression. */
  uint32_t res = eval(0, nr_token - 1);
  *success = res != EVAL_ERROR;
  if (*success) { 
    // printf("%x\n", res);
    return res;
  }
  else printf("Failed\n");
  return 0;
}

static int find_main_op(int p, int q) {
  int i ;
  int pos = 0;
  int priority = -1;
  int in_parentheses = 0;
  
  for (i = p; i < q; ++i) {
    int t = tokens[i].type;
    int p = tokens[i].priority;
    if (t == TK_DEC || t == TK_HEX) continue;
    else if (t == '(') {
      in_parentheses++;
      continue;
    } else if (t == ')') {
      in_parentheses--;
      continue;
    } else {
      if (p >= priority && in_parentheses == 0) {
        priority = p;
        pos = i;
      }
    }
  }
  return pos;
}

uint32_t eval(int p, int q) {
  // printf("%d, %d\n", p, q);
  if (p > q) {
    printf("Bad expression.\n");
    return EVAL_ERROR;
  } else if (p == q) {

    assert(tokens[p].type == TK_DEC || tokens[p].type == TK_HEX || tokens[p].type == TK_REG);
    // printf("%u\n", atoi(tokens[p].str));
    uint32_t num;
    switch (tokens[p].type) {
      case TK_DEC: num = (uint32_t)atoi(tokens[p].str); break;
      case TK_HEX: sscanf(tokens[p].str, "%x", &num); break;
      case TK_REG: {
        int i, len;
        int l = strlen(tokens[p].str);
        if (l == 3) len = 4;
        else if (l == 2) {
          if (tokens[p].str[l - 1] == 'h' || tokens[p].str[l - 1] == 'l') len = 1;
          else len = 2;
        } else Assert(0, "Reg length error!\n");

        for (i = R_EAX ; i <= R_EDI; ++i) {
          // printf("%s\n", reg_name(i, len));
          // printf("%d\n" ,strcmp(tokens[p].str, reg_name(i, len)));
          if (strcmp(tokens[p].str, reg_name(i, len)) == 0) {
            num = reg_value(i, len);
            break;
          }
        }
        if (i > R_EDI && strcmp(tokens[p].str, "eip") == 0) 
          num = cpu.eip;
        else if (i > R_EDI) Assert(0, "Register name error!\n");
        break;
      }
      default: Assert(0, "Element type error!\n");
    }
    return num;
  } else if (check_parentheses(p, q)) {
    return eval(p + 1, q - 1);
  } else {
    int op_pos = find_main_op(p, q);
    // printf("main op pos: %d\n", op_pos);
    if (p == op_pos && (tokens[op_pos].type == TK_MINUS || tokens[op_pos].type == TK_DEREF)) {
      uint32_t val = eval (p + 1,q);
      switch (tokens[op_pos].type) {
        case TK_MINUS: return -val;
        case TK_DEREF: return vaddr_read(val, 4);
        default: break;
      }
    }
    uint32_t val_l = eval(p, op_pos - 1);
    uint32_t val_r = eval(op_pos + 1, q);
    if (val_l == EVAL_ERROR || val_r == EVAL_ERROR) {
      // printf("main op pos: %d\n", op_pos);printf("%u %u\n", val_l, val_r); 
      return EVAL_ERROR;
    }
    switch(tokens[op_pos].type) {
      case '+': return val_l + val_r;
      case '-': return val_l - val_r;
      case '*': return val_l * val_r;
      case '/': if (val_r == 0) return EVAL_ERROR; else return val_l / val_r;
      case TK_EQ: return val_l == val_r;
      case TK_NEQ: return val_l != val_r;
      case TK_OR: return val_l || val_r;
      case TK_AND: return val_l && val_r;
      default : return EVAL_ERROR;
    }
  }
}
