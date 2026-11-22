import sys,subprocess
sys.path.insert(0,'.')
import model
def raw_of_px(p):  # px (12 bits) -> raw bits
    return ''.join(str(int(p[i])^(int(p[i-1]) if i else 0)) for i in range(12))
def block_raw(data9):          # 9 bits de donnees (domaine cumule) -> 12 bits bruts
    par=1^(sum(data9)%2)
    p=list(data9)+[par,1,1]
    return raw_of_px(''.join(map(str,p)))
def units(words):
    s=''; 
    for w in words:
        s+='111'          # synchro haute
        lvl=1
        for b in w:
            lvl^=1; s+=str(lvl)
            if b=='1': lvl^=1
            s+=str(lvl)
    return s
if __name__=='__main__':
    import flux
    ok=bad=0
    for l in subprocess.run(['python','-I','dec.py'],capture_output=True,text=True).stdout.splitlines():
        t=l.split()
        if len(t)<9: continue
        idx=int(t[0]); words=t[2:8]+[ (t[8]+'0') ]   # M7 : 11 bits + dernier bit brut = 0
        e=units(words)
        s=flux.frames_of(idx)
        fr=[f for f in s.split('|') if len(f)>=180]
        # polarite : on compare aux deux
        res=False
        for f in fr:
            for g in (f,''.join('1' if c=='0' else '0' for c in f)):
                g2=g.lstrip('0') if g.startswith('0111') else g
                if e.startswith(g2) or g2.startswith(e[:len(g2)]) and abs(len(e)-len(g2))<=3: res=True
        ok+=res; bad+=(not res)
        if not res and bad<4: print('ECART',idx,'\n',e,'\n',fr[:1])
    print('reproduit',ok,'ecart',bad)
