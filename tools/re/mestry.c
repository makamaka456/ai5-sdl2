#include <stdio.h>
#include <stdlib.h>
#include "nulib.h"
#include "nulib/file.h"
#include "ai5/game.h"
#include "ai5/mes.h"
int main(int argc,char**argv){
  extern bool mes_text_cp949; mes_text_cp949 = getenv("CP949") != NULL;
  ai5_set_game(argv[1]);
  int ok=0,bad=0;
  for(int i=2;i<argc;i++){
    size_t sz; uint8_t *d=file_read(argv[i],&sz);
    mes_statement_list l=vector_initializer;
    if(mes_parse_statements(d,sz,&l)) ok++; else bad++;
  }
  printf("%-14s ok=%d bad=%d\n",argv[1],ok,bad);
  return 0;
}
