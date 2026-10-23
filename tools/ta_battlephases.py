"""Walk a Lost Age battle (start state build/tb1_b0.state) through menu phases and save RAM dumps: build/bp_<phase>.bin plus screenshots.
phases: fight (round menu), cmd (hero command bar, Attack), cmdpsy (cursor on Psynergy), list (Psynergy list open), target (Attack's target choice)."""
import sys, os
os.environ['GS_ROM'] = '/d/gs3ds/build/ta.gba'
sys.path.insert(0, '/d/gs3ds/tools')
from gs import GS
def phases(save=True):
    g = GS(); g.load('tb1_b0.state'); g.run(60)
    D = {}
    def snap(name):
        g.run(25)
        D[name] = g.dump(); g.shot(f'bp_{name}.png', 2); g.save(f'bp_{name}.state')
        if save: open(f'bp_{name}.bin', 'wb').write(D[name])
    g.tap('A', 3, 30); g.tap('A', 3, 30)      # dismiss "appeared" / "attacks first"
    snap('fight')
    g.tap('A', 3, 30); snap('cmd')            # Fight -> Felix's command bar
    g.tap('R', 3, 20); snap('cmdpsy')         # cursor right: Psynergy
    g.tap('A', 3, 30); snap('list')           # open the Psynergy list
    g.tap('B', 3, 30); g.tap('L', 3, 20)      # back to the bar, cursor to Attack
    g.tap('A', 3, 30); snap('target')         # Attack -> choose target
    g.close()
    return D
if __name__ == '__main__': phases()
