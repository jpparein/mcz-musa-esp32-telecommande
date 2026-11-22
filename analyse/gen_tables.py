import sys,itertools,random; sys.path.insert(0,'.')
import lin,genref,enc
SK = "../firmware/"   # dossier de sortie des tables (mcz_tables.h, mcz_autotest.h)
bs=lambda v: ''.join(map(str,v))
noms={'fan1':'DY_FAN1','flame':'DY_FLAMME','mode':'DY_MODE','fan2':'DY_FAN2','fan3':'DY_FAN3'}
nmax={'fan1':7,'flame':8,'mode':8,'fan2':7,'fan3':7}
o=["// ============================================================",
"// Tables de controle des trames MCZ (fichier genere automatiquement)",
"// Pour chaque champ et chaque valeur : l'effet sur les 18 bits de controle",
"// (blocs 6 et 7). Calcule a partir de 47 trames reelles de la telecommande.",
"// ============================================================",
"#pragma once"]
for f in ['fan1','flame','mode','fan2','fan3']:
    o.append(f"// {f} : reference = {genref.REF[f]}")
    o.append(f"static const char* const {noms[f]}[{nmax[f]}] = {{")
    for v in range(nmax[f]):
        dy=genref.DY[f].get(v)
        o.append(f'  {chr(34)+bs(dy)+chr(34) if dy is not None else "nullptr"},  // {v}')
    o.append("};")
o.append("// Retourner le drapeau du bloc 5 (bit 9), que la telecommande bascule en AUTO/ECO")
d9=[0]*18; d9[9]=1
o.append(f'static const char DY_DRAPEAU[] = "{bs(lin.apply(genref.R,genref.piv,d9))}";')
open(SK+'mcz_tables.h','w',encoding='utf-8').write('\n'.join(o)+'\n')

# ---- vecteurs d'autotest : de vraies trames A -> B (un seul champ change) ----
M123=("000100000000","100010000100","011111001010")
random.seed(3)
paires=[]
for A,B in itertools.permutations(genref.F,2):
    xa,ya=lin.xy(A); xb,yb=lin.xy(B)
    fa=lin.fields(xa); fb=lin.fields(xb)
    diff=[k for k in genref.REF if fa[k]!=fb[k]]
    if len(diff)==1 and diff[0] in('fan1','fan2','fan3','flame') and xa[9]==xb[9]: paires.append((A,B,diff[0],fb[diff[0]]))
random.shuffle(paires)
choisis={}
for p in paires:
    choisis.setdefault(p[2],[])
    if len(choisis[p[2]])<3: choisis[p[2]].append(p)
chmap={'fan1':'CH_FAN1','flame':'CH_FLAMME','fan2':'CH_FAN2','fan3':'CH_FAN3'}
t=["// Vecteurs d'autotest (generes) : trame reelle A, champ change, trame reelle B attendue",
   "#pragma once","struct VecteurTest { const char* a[7]; int champ; int valeur; const char* b[7]; };",
   "static const VecteurTest VECTEURS[] = {"]
n=0
for k,v in choisis.items():
    for A,B,champ,val in v:
        wa=list(M123)+[A[0],A[1],A[2],A[3]+'0']
        wb=list(M123)+[B[0],B[1],B[2],B[3]+'0']
        t.append("  { {"+','.join('"'+w+'"' for w in wa)+"}, "+chmap[champ]+", "+str(val)+", {"+','.join('"'+w+'"' for w in wb)+"} },")
        n+=1
t.append("};")
t.append(f"static const int NB_VECTEURS = {n};")
open(SK+'mcz_autotest.h','w',encoding='utf-8').write('\n'.join(t)+'\n')
print('tables ecrites, vecteurs :',n)
