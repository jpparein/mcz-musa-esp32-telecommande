import re,sys
L=open('journal.txt',encoding='utf-8-sig').read().splitlines()
caps=[];cur=None
for l in L:
    ts=l[:12]; x=l[13:]
    if '== CAPTURE' in x: cur=[ts,[]]; caps.append(cur)
    elif cur is not None and re.search(r'[HB]\d+',x) and 'Histo' not in x and 'Premieres' not in x: cur[1].append(x)
T=422
def clean(toks):
    out=[]; i=0
    toks=[(l,int(d)) for l,d in toks]
    while i<len(toks):
        l,d=toks[i]
        if d<150 and out and i+1<len(toks):
            # parasite : on l'absorbe avec le suivant dans l'etat precedent
            pl,pd=out[-1]; out[-1]=(pl,pd+d+toks[i+1][1]); i+=2; continue
        out.append((l,d)); i+=1
    return [(l,str(d)) for l,d in out]
def dec(w): return ''.join('1' if w[i]!=w[i+1] else '0' for i in range(0,len(w)-1,2))
for i,(ts,lines) in enumerate(caps):
    toks=re.findall(r'([HB])(\d+)',' '.join(lines))
    toks=clean(toks)
    s=''
    for l,d in toks:
        n=round(int(d)/T); s+='|' if n>10 else ('1' if l=='H' else '0')*n
    fr=[f for f in s.split('|') if len(f)>=180]
    if not fr: print(i,ts,'incomplet'); continue
    def nb(x): return sum(1 for w in x.split('111') if len(w)>=22)
    cands=[]
    for fx in fr:
        g=''.join('1' if c=='0' else '0' for c in fx)
        cands.append(max((fx,g),key=nb))
    f=max(cands,key=nb)
    ws=[dec(w) for w in f.split('111') if len(w)>=22][:7]
    print(i,ts,' '.join(ws))
