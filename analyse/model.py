import subprocess
def px(w):
    o='';a=0
    for c in w:
        a^=int(c); o+=str(a)
    return o
def load():
    out=subprocess.run(['python','-I','dec.py'],capture_output=True,text=True).stdout.splitlines()
    fr=[]
    for l in out:
        p=l.split()
        if len(p)>=9: fr.append(tuple(p[5:9]))
    fr+=[("101100100010","101111100100","000101110010","01011101110"),("101001100010","101111100100","110110000100","11101010101")]
    return list(dict.fromkeys(fr))
def rows_of(fr):
    R=[]
    for a,b,c,d in fr:
        x=[int(ch) for ch in px(a)[:9]+px(b)[:9]]+[1]
        y=[int(ch) for ch in px(c)[:9]]+[int(ch) for ch in px(d+'0')[:8]]   # M6 9 bits + M7 8 bits
        R.append((x,y))
    return R
def predict(R,xnew):
    # find coefficients c (19) with sum c_i*row_i = xnew ; then y = sum c_i*y_i (valid if xnew in span)
    n=len(R); m=19
    # augment: rows as vectors over combination of samples: solve R^T c = xnew
    A=[[R[j][0][i] for j in range(n)]+[xnew[i]] for i in range(m)]
    piv=[];r=0
    for col in range(n):
        p=None
        for i in range(r,m):
            if A[i][col]: p=i;break
        if p is None: continue
        A[r],A[p]=A[p],A[r]
        for i in range(m):
            if i!=r and A[i][col]: A[i]=[u^v for u,v in zip(A[i],A[r])]
        piv.append(col); r+=1
    if any(all(v==0 for v in A[i][:n]) and A[i][n] for i in range(m)): return None
    c=[0]*n
    for k,col in enumerate(piv): c[col]=A[k][n]
    y=[0]*17
    for j in range(n):
        if c[j]: y=[u^v for u,v in zip(y,R[j][1])]
    return y
