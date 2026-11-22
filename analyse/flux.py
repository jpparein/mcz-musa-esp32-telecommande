import re,sys
exec(open('dec.py').read().split("for i,(ts,lines) in enumerate(caps):")[0])
def frames_of(i):
    ts,lines=caps[i]
    toks=clean(re.findall(r'([HB])(\d+)',' '.join(lines)))
    s=''
    for l,d in toks:
        n=round(int(d)/T); s+='|' if n>10 else ('1' if l=='H' else '0')*n
    return s
if __name__=='__main__':
    i=int(sys.argv[1]); s=frames_of(i)
    for f in s.split('|'):
        if f: print(len(f), f)
