import sys
from PIL import Image
out=sys.argv[1]; names=sys.argv[2:]
ims=[Image.open(f'/tmp/claude-0/ss/{n}.png').resize((640,400)) for n in names]
o=Image.new('RGB',(640*min(3,len(ims)),400*((len(ims)+2)//3)))
for i,im in enumerate(ims): o.paste(im,((i%3)*640,(i//3)*400))
o.save(f'/tmp/claude-0/ss/{out}.png')
