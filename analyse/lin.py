import sys,subprocess,itertools,json
sys.path.insert(0,'.')
import model
def px(w): return [int(c) for c in model.px(w)]
def frames():
    fr=set()
    for l in subprocess.run(['python','-I','dec.py'],capture_output=True,text=True).stdout.splitlines():
        t=l.split()
        if len(t)>=9 and model.px(t[8])[10]=='1': fr.add((t[5],t[6],t[7],t[8]))   # on ecarte les captures dont la fin est abimee
    fr|={("101100100010","101111100100","000101110010","01011101110"),("101001100010","101111100100","110110000100","11101010101")}
    return sorted(fr)
def xy(f):
    a,b,c,d=f
    x=px(a)[:9]+px(b)[:9]
    y=px(c)[:9]+px(d+'0')[:9]       # M7 : 11 bits + bit brut final 0 -> 12 bits
    return x,y
# ----- champs -----
def bits(v,n): return [(v>>(n-1-i))&1 for i in range(n)]
def fields(x):
    g=lambda s,w: int(''.join(map(str,x[s:s+w])),2)
    fan1=g(0,3)^2; flame=g(3,3)^5; mode=g(6,3)
    v2=(x[11]<<3)|(x[12]<<2)|(x[13]<<1)|1; fan2=(v2^5)>>1
    v3=((x[14]^1)<<3)|(x[15]<<2)|(x[16]<<1)|x[17]; fan3=(v3^2)>>1
    return dict(fan1=fan1,flame=flame,mode=mode,fan2=fan2,fan3=fan3)
def delta(field,old,new):
    d=[0]*18
    def put(s,w,o,n):
        for i,(a,b) in enumerate(zip(bits(o,w),bits(n,w))): d[s+i]^=a^b
    if field=='fan1': put(0,3,old^2,new^2)
    elif field=='flame': put(3,3,old^5,new^5)
    elif field=='mode': put(6,3,old,new)
    elif field=='fan2': put(11,4,(2*old)^5,(2*new)^5)
    elif field=='fan3': put(14,4,(2*old)^2,(2*new)^2)
    return d
# ----- resolution GF(2) : y = A x (partie lineaire sur les differences) -----
def solve(F):
    base=xy(F[0]); 
    rows=[]
    for f in F[1:]:
        x,y=xy(f)
        rows.append(([a^b for a,b in zip(x,base[0])],[a^b for a,b in zip(y,base[1])]))
    # elimination
    n=len(rows); piv=[]; r=0
    R=[(list(a),list(b)) for a,b in rows]
    for col in range(18):
        p=next((i for i in range(r,n) if R[i][0][col]),None)
        if p is None: continue
        R[r],R[p]=R[p],R[r]
        for i in range(n):
            if i!=r and R[i][0][col]:
                R[i]=([u^v for u,v in zip(R[i][0],R[r][0])],[u^v for u,v in zip(R[i][1],R[r][1])])
        piv.append(col); r+=1
    incoh=sum(1 for i in range(r,n) if any(R[i][1]))
    return R[:r],piv,incoh,base
def apply(R,piv,dx):
    """retourne dy si dx est dans l'espace des differences connues, sinon None"""
    dx=list(dx); dy=[0]*18
    for k,col in enumerate(piv):
        if dx[col]:
            dx=[u^v for u,v in zip(dx,R[k][0])]; dy=[u^v for u,v in zip(dy,R[k][1])]
    return dy if not any(dx) else None
if __name__=='__main__':
    F=frames(); print('trames distinctes',len(F))
    R,piv,incoh,base=solve(F); print('rang',len(piv),'incoherences',incoh)
    # test : toutes les paires, deltas de champs
    good=bad=none=0; multi=0
    for A,B in itertools.permutations(F,2):
        xa,ya=xy(A); xb,yb=xy(B)
        dx=[u^v for u,v in zip(xa,xb)]
        dy=apply(R,piv,dx)
        if dy is None: none+=1; continue
        if [u^v for u,v in zip(ya,dy)]==yb: good+=1
        else: bad+=1
    print('paires:',good,'justes',bad,'fausses',none,'hors espace')
