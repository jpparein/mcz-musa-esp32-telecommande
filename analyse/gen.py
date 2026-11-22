import model
def bits(v,n): return [(v>>(n-1-i))&1 for i in range(n)]
def xbits(a,b):   # 18 bits donnees M4 + M5
    return [int(c) for c in model.px(a)[:9]+model.px(b)[:9]]
def fields_to_delta(f_old,f_new):
    """f = dict(fan1,flame,fan2,fan3) -> delta sur les 18 bits"""
    d=[0]*18
    def put(start,width,old,new):
        for i,(o,n) in enumerate(zip(bits(old,width),bits(new,width))): d[start+i]^=o^n
    put(0,3,f_old['fan1']^2,f_new['fan1']^2)
    put(3,3,f_old['flame']^5,f_new['flame']^5)
    put(9+2,4,(2*f_old['fan2'])^5,(2*f_new['fan2'])^5)
    put(9+5,4,(2*f_old['fan3'])^2,(2*f_new['fan3'])^2)
    return d
def decode_fields(a,b):
    x=xbits(a,b)
    g=lambda s,w: int(''.join(map(str,x[s:s+w])),2)
    fan1=g(0,3)^2; flame=g(3,3)^5
    v3=g(9+5,4)
    # idx5 de M5 = fan2.bit0 (toujours 1) XOR fan3.bit3 : on reconstruit
    fan3=((v3^1<<3)^2)//2 if False else None
    return fan1,flame
