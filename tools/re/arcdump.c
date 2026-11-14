#include <stdio.h>
#include <string.h>
#include "nulib.h"
#include "ai5/arc.h"
#include "ai5/game.h"
int main(int argc,char**argv){
  ai5_set_game("isaku");
  struct archive *a=archive_open(argv[1],0);
  if(!a){puts("open fail");return 1;}
  if(argc==2){ for(unsigned i=0;i<a->files.n;i++) printf("%s %u\n",a->files.a[i].name,a->files.a[i].size); return 0;}
  struct archive_data *d=archive_get(a,argv[2]);
  if(!d){puts("nofile");return 1;}
  FILE*f=fopen(argv[3],"wb");fwrite(d->data,1,d->size,f);fclose(f);
  return 0;
}
