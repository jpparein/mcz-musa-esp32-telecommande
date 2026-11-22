# Generateur de reference (Python) : meme algorithme que celui du programme de l'ESP32
import sys,itertools; sys.path.insert(0,'.')
import lin,enc
F=lin.frames()
R,piv,inc,base=lin.solve(F)
REF={'fan1':2,'fan2':2,'fan3':2,'flame':3,'mode':0}
VALS={'fan1':range(0,7),'fan2':range(0,7),'fan3':range(0,7),'flame':range(1,6),'mode':[0,2,4,6]}
DY={f:{v:lin.apply(R,piv,lin.delta(f,REF[f],v)) for v in VALS[f]} for f in REF}
def changer(x,y,champ,nouveau):
    """x : 18 bits M4+M5, y : 18 bits M6+M7 (domaine cumule). Retourne (x',y')."""
    old=lin.fields(x)[champ]
    dx=lin.delta(champ,old,nouveau)
    dy=[a^b for a,b in zip(DY[champ][old],DY[champ][nouveau])]
    return [a^b for a,b in zip(x,dx)],[a^b for a,b in zip(y,dy)]
def mots(x,y,m123):
    """construit les 7 blocs bruts de 12 bits"""
    return list(m123)+[enc.block_raw(x[:9]),enc.block_raw(x[9:]),enc.block_raw(y[:9]),enc.block_raw(y[9:])]
if __name__=='__main__':
    M123=("000100000000","100010000100","011111001010")
    ok=bad=n=0
    for A,B in itertools.permutations(F,2):
        xa,ya=lin.xy(A); xb,yb=lin.xy(B)
        fa=lin.fields(xa); fb=lin.fields(xb)
        diff=[c for c in REF if fa[c]!=fb[c]]
        if len(diff)!=1: continue                      # un seul champ change
        if diff[0] not in('fan1','fan2','fan3','flame'): continue
        n+=1
        x2,y2=changer(xa,ya,diff[0],fb[diff[0]])
        w=mots(x2,y2,M123)
        reel=list(M123)+list(B[:3])+[B[3]]
        w[6]=w[6][:11]                                  # le reel n'a que 11 bits pour M7
        if w==reel: ok+=1
        else: bad+=1
    print('changements d un seul champ :',n,'| trames reconstruites identiques au reel :',ok,'| differentes :',bad)
