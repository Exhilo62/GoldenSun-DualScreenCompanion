import sys, os
sys.path.insert(0, '/d/gs3ds/tools')
from gs import GS
name = sys.argv[1]
os.environ['GS_SAV'] = '/d/gs3ds/build/saves/golden-sun.%s.sav' % name
g = GS()
g.run(400)
for i in range(14): g.tap('A', 4, 50)
g.run(120)
# back out of any menu/dialogue with B until the field is free
for i in range(8): g.tap('B', 4, 30)
g.run(60)
g.shot('field_%s.png' % name, 2)
g.save('field_%s.state' % name)
g.close()
