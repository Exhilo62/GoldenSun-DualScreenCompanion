import sys,re; sys.path.insert(0,'/d/gs3ds/tools')
from gs import GS
def u16(d,addr,n=48):
    o=addr-0x3000000; b=d[o:o+2*n]; s=''
    for i in range(0,len(b),2):
        c=b[i]|(b[i+1]<<8)
        if c==0: break
        s+=chr(c) if 32<=c<127 else '?'
    return s
def look(g,tag):
    g.run(10); d=g.dump()
    print(f'{tag:26s} cmd={d[0x8000+0x35c08]} sel={d[0x7dec]} tidx={d[0x7c8c]} desc="{u16(d,0x3007d00)[:48]}" tname="{u16(d,0x3007cc0)[:20]}"')
    return d
g=GS(); g.load('esw_b1.state'); g.run(5)
for i in range(40):
    if g.r(0x03007dec,8)==0: break
    g.tap('A',3,22)
g.tap('A',3,25); look(g,'command list (Attack)')
for name,rights in (('Djinn',2),('Summon',3),('Item',4)):
    g.load('esw_b1.state'); g.run(5)
    for i in range(40):
        if g.r(0x03007dec,8)==0: break
        g.tap('A',3,22)
    g.tap('A',3,25)
    for _ in range(rights): g.tap('R',4,14)
    look(g,f'{name} highlighted')
    g.tap('A',3,30); look(g,f'{name} list open')
    g.shot(f'bl_{name}.png',1)
    g.tap('D',4,14); look(g,f'{name} list, 2nd entry')
    g.tap('A',3,30); look(g,f'{name} after A (target?)'); g.shot(f'bl_{name}_t.png',1)
    g.tap('R',4,16); look(g,f'{name} target moved')
g.close()
