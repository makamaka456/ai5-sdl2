#include <stdio.h>
#include <stdlib.h>
#include "nulib.h"
#include "nulib/file.h"
#include "nulib/port.h"
#include "ai5/game.h"
#include "ai5/mes.h"
int main(int argc,char**argv){
  extern bool mes_text_cp949; mes_text_cp949 = getenv("CP949") != NULL;
  ai5_set_game(argv[1]);
  size_t sz; uint8_t *d=file_read(argv[2],&sz);
  mes_statement_list l=vector_initializer;
  if(!mes_parse_statements(d,sz,&l)) { puts("parse fail"); return 1; }
  struct port p; port_file_init(&p, stdout);
  mes_flat_statement_list_print(l,&p); port_flush(&p);
  return 0;
}
