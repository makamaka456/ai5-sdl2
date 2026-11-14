#include <stdio.h>
#include "nulib.h"
#include "ai5/arc.h"
#include "ai5/cg.h"
#include "ai5/game.h"
int main(int argc,char**argv){
  ai5_set_game("hanahebi");
  struct archive *a=archive_open(argv[1],ARCHIVE_RAW);
  for(int i=2;i<argc;i++){
    struct archive_data *d=archive_get(a,argv[i]);
    if(!d){printf("%s: none\n",argv[i]);continue;}
    struct cg *cg=cg_load_arcdata(d);
    if(!cg){printf("%s: decode fail\n",argv[i]);continue;}
    unsigned alpha=0; 
    printf("%s: x=%d y=%d w=%d h=%d bpp=%d pal=%p\n",argv[i],cg->metrics.x,cg->metrics.y,cg->metrics.w,cg->metrics.h,cg->metrics.bpp,cg->palette);
    if (argc>3 && i==2){ FILE*f=fopen("/tmp/claude-0/cg.raw","wb"); fwrite(cg->pixels,1,cg->metrics.w*cg->metrics.h*4,f); fclose(f);}
  }
  return 0;
}
