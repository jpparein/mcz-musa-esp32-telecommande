# Protocole radio des télécommandes MCZ (Musa / Club / Suite, télécommande « MAX LCD »)

Aucune documentation publique de ce protocole n'existe à notre connaissance. Tout ce qui suit a été retrouvé par écoute et essais sur un poêle MCZ Musa Comfort Air 2.0 avec sa télécommande « MAX LCD », puis validé en émettant réellement vers le poêle.

Statut des informations : **[établi]** = vérifié sur des trames réelles et/ou à l'émission ; **[déduit]** = calculé par linéarité et vérifié sur au moins une trame réelle ; **[non vérifié]** = hypothèse.

> Votre télécommande a sa propre adresse (blocs 1 à 3). Les valeurs données ici sont celles de la télécommande de l'auteur : il faut capturer la vôtre (voir [REPRODUIRE.md](REPRODUIRE.md)). Le reste (structure, champs, tables de contrôle) devrait être commun à la gamme, mais ce n'est pas garanti : voir « Ce qui peut différer sur votre poêle ».

## 1. Couche radio

| Élément | Valeur |
|---|---|
| Fréquence | **434,5 MHz** (pas 433,92). Confirmée par la notice de la télécommande et par la mesure du RSSI. |
| Modulation | **OOK** (porteuse présente / absente). Le 2-FSK ne donne que du bruit. |
| Unité de temps | **422 µs** (durées observées : 1, 2 ou 3 unités) |
| Codage binaire | **biphase mark** : une transition à chaque frontière de bit ; une transition supplémentaire au milieu du bit = `1`, pas de transition = `0`. Un bit = 2 unités = 843 µs (≈ 1186 bit/s). |
| Synchronisation | 3 unités hautes (`111`) avant **chaque** bloc |
| Trame | 7 blocs de 12 bits = 84 bits = **189 unités** (7 × (3 + 24)) |
| Répétitions | la trame est envoyée **5 fois** de suite |
| Silence entre répétitions | **4586 µs** (≈ 10,9 unités) |
| Durée d'une émission | ≈ 410 ms (630 changements de niveau environ) |

Parasites : des impulsions de moins de 150 µs apparaissent de temps en temps et inversent la polarité apparente. Le décodeur les absorbe dans l'impulsion suivante et essaie les deux polarités (voir `firmware/etape9_emission/mcz_decode.h`).

Extrait d'une capture brute (durées en µs, H = haut, B = bas) — on reconnaît les 3 unités de synchro (`H1258` ≈ 3 × 422) puis des durées de 422 et 843 :

```
B421 H1258 B853 H843 B826 H427 B424 H840 B841 H859 B825 H808 B844 H830 B812
H1265 B430 H423 B839 H857 B826 H424 B424 H841 B841 H842 B807 H426 B410 ...
... B4601 H1253 ...      <- 4,6 ms de silence : fin d'une répétition, début de la suivante
```

La capture complète est dans [`captures/journal_captures_brutes.txt`](captures/journal_captures_brutes.txt).

## 2. Structure d'un bloc de 12 bits [établi]

Après le biphase mark on obtient 12 bits « bruts » `r0 … r11`. On calcule leur **XOR cumulé** : `c[n] = r0 ⊕ r1 ⊕ … ⊕ rn`. On obtient alors :

| Bits de `c` | Contenu |
|---|---|
| `c0 … c8` | **9 bits de données** (3 chiffres octaux) |
| `c9` | bit de **parité impaire** : `c0 + … + c9` doit être impair |
| `c10`, `c11` | toujours `1`, `1` |

Pour fabriquer un bloc : on se donne 9 bits de données `d0..d8`, `c9 = 1 ⊕ d0 ⊕ … ⊕ d8`, `c10 = c11 = 1`, puis `r0 = c0` et `rn = cn ⊕ c(n-1)`.

La parité et le suffixe `11` sont vérifiés sur chaque bloc de chaque trame : c'est le contrôle d'intégrité de la réception (les trames dont le dernier bloc est tronqué sont écartées pour fabriquer des trames, mais gardées pour l'affichage).

## 3. Les 7 blocs

| Bloc | Rôle | Exemple (télécommande de l'auteur, bits bruts) |
|---|---|---|
| 1 | adresse | `000100000000` |
| 2 | adresse | `100010000100` |
| 3 | adresse | `011111001010` |
| 4 | état : ventilateur 1 (3 bits), flamme (3 bits), mode (3 bits) | variable |
| 5 | état : drapeau (1 bit), ventilateur 2 (4 bits), ventilateur 3 (4 bits) — voir ci-dessous | variable |
| 6 | contrôle (9 bits) | variable |
| 7 | contrôle (9 bits) | variable |

Les blocs 1 à 3 sont constants pour une télécommande donnée : ils identifient l'émetteur. Le poêle ne réagit qu'à la télécommande qu'il a **appairée** (poêle sur REMOTE, bouton G du panneau d'urgence, puis marche/arrêt de la télécommande). Pour piloter votre poêle avec l'ESP32 il suffit de **réutiliser l'adresse de la vraie télécommande** : il n'y a rien à appairer.

### Les 18 bits d'état `x` (données des blocs 4 et 5, après XOR cumulé)

On note `x0 … x8` les 9 bits de données du bloc 4 et `x9 … x17` ceux du bloc 5.

| Bits | Champ | Codage | Valeurs |
|---|---|---|---|
| `x0 x1 x2` | **ventilateur 1** | niveau ⊕ 2 (3 bits) | 0 à 5, **6 = AUTO** |
| `x3 x4 x5` | **puissance de flamme** | niveau ⊕ 5 (3 bits) | 1 à 5 ; **0 = arrêt en mode ECO (veille)** |
| `x6 x7 x8` | **mode** | valeur directe (3 bits) | **0 = MAN, 2 = arrêt, 4 = ECO, 6 = AUTO** |
| `x9` | **drapeau « action de l'utilisateur »** | 1 bit | 1 = appui sur une touche (le poêle fait « bip »), 0 = envoi automatique du thermostat |
| `x11 x12 x13 (x14)` | **ventilateur 2** | `(2 × niveau) ⊕ 5` sur 4 bits ; le 4e bit est une constante (1) que le code ne lit pas dans la trame ; `x14` appartient en pratique au ventilateur 3 | 0 à 5, 6 = AUTO |
| `x14 x15 x16 x17` | **ventilateur 3** | `(2 × niveau) ⊕ 2` sur 4 bits ; `x14` est stocké inversé | 0 à 5, 6 = AUTO |

Note sur la numérotation, telle qu'elle est écrite dans le code (`mcz_decode.h`) : le décodeur utilise `d[0..17]` = `x0..x17` et lit
- `fan2 = ((d11<<3 | d12<<2 | d13<<1 | 1) ⊕ 5) >> 1`
- `fan3 = (((d14⊕1)<<3 | d15<<2 | d16<<1 | d17) ⊕ 2) >> 1`

Le bit `d14` est partagé : bit de poids faible constant du ventilateur 2 et bit de poids fort (inversé) du ventilateur 3 ; c'est lui qui fait alterner 7/3 le dernier chiffre octal du bloc 5 selon les niveaux. Le bit `d9` est le drapeau. Le bit `d10` n'a pas été vu changer.

Pourquoi des « ⊕ 2 », « ⊕ 5 » : ce sont des décalages constants du codage, trouvés en comparant des trames où un seul réglage change. On les garde tels quels, ils n'ont pas d'autre signification connue.

### Les 18 bits de contrôle `y` (données des blocs 6 et 7) [établi]

`y` est une **fonction XOR-linéaire** de `x` : chaque bit de `y` est le XOR de certains bits de `x` (plus éventuellement une constante). Conséquence essentielle : **changer un champ de `x` ajoute toujours la même différence à `y`**, quelle que soit la valeur des autres champs. On n'a donc pas besoin de connaître la formule : il suffit d'une trame réelle de départ et de la table des différences ci-dessous.

Vérifications : sur 47 trames réelles, chacune est retrouvée exactement à partir des autres ; 156 changements de réglage reproduits à l'identique par le générateur ; autotest de 12 trames au démarrage du firmware.

## 4. Table des différences de contrôle

Pour passer un champ de la valeur `a` à la valeur `b`, on calcule `Δ = T[a] ⊕ T[b]` (XOR bit à bit des deux chaînes de 18 bits) et on l'applique à `y`. Dans le même temps on remplace les bits de `x` du champ. Les valeurs de référence sont celles de la colonne « référence ».

(Fichier source : `firmware/etape9_emission/mcz_tables.h`, généré par `analyse/gen_tables.py` à partir de 47 trames réelles.)

### Ventilateur 1 (référence : niveau 2)

| Niveau | Δ sur `y` (18 bits) |
|---|---|
| 0 | `000011010110011000` |
| 1 | `000010110001010100` |
| 2 | `000000000000000000` |
| 3 | `000001100111001100` |
| 4 | `000101100010101000` |
| 5 | `000100000101100100` |
| 6 (AUTO) | `000110110100110000` |

### Puissance de flamme (référence : niveau 3)

| Niveau | Δ sur `y` | Remarque |
|---|---|---|
| 0 | `010001010011011010` | arrêt en ECO (veille) — **[déduit]**, vérifié sur la trame réelle d'arrêt ECO |
| 1 | `100010010101010010` | |
| 2 | `110011000110001000` | |
| 3 | `000000000000000000` | |
| 4 | `010001100000111100` | |
| 5 | `100010100110110100` | |

### Mode (référence : 0 = MAN)

| Mode | Valeur | Δ sur `y` |
|---|---|---|
| MAN | 0 | `000000000000000000` |
| Arrêt | 2 | `001100110001100010` |
| ECO | 4 | `011001100011000100` |
| AUTO | 6 | `010101010010100110` |

### Ventilateur 2 (référence : niveau 2)

| Niveau | Δ sur `y` |
|---|---|
| 0 | `000100100001100010` |
| 1 | `100100110001110010` |
| 2 | `000000000000000000` |
| 3 | `100000010000010000` |
| 4 | `001101100010100110` |
| 5 | `101101110010110110` |
| 6 (AUTO) | `001001000011000100` |

### Ventilateur 3 (référence : niveau 2)

| Niveau | Δ sur `y` |
|---|---|
| 0 | `001000000010000100` |
| 1 | `001100000011000110` |
| 2 | `000000000000000000` |
| 3 | `000100000001000010` |
| 4 | `011000000110001100` |
| 5 | `011100000111001110` |
| 6 (AUTO) | `010000000100001000` |

### Drapeau « action de l'utilisateur » (bit `x9`)

Basculer `x9` ajoute à `y` la différence `100100010100010000`.

## 5. Exemple pas à pas

Trame réelle (blocs 4 à 7, bits bruts), télécommande en MAN :

```
bloc 4 = 101100100010   bloc 5 = 101111100100   bloc 6 = 000101110010   bloc 7 = 010111011100
```

Décodage : XOR cumulé de chaque bloc, on garde 9 bits.
`x = 110111000 110101000`, donc :
- ventilateur 1 = `110` ⊕ `010` = 4
- flamme = `111` ⊕ `101` = 2
- mode = `000` = MAN
- drapeau `x9` = 1 (appui sur une touche)
- ventilateur 2 = 0, ventilateur 3 = 1

On veut la **flamme à 5**. Δ = `T_flamme[2] ⊕ T_flamme[5]` = `110011000110001000` ⊕ `100010100110110100` = `010001100000111100`. On remplace les 3 bits de flamme dans `x` (`111` → `000`), on applique Δ à `y`, puis on refabrique les 4 blocs (parité, XOR cumulé inverse). Résultat, identique à la trame réellement émise par la télécommande :

```
bloc 4 = 101000000100   bloc 5 = 101111100100   bloc 6 = 011100100100   bloc 7 = 010011001100
```

Les 12 couples « trame réelle A → champ changé → trame réelle B » de l'autotest sont dans `firmware/etape9_emission/mcz_autotest.h` et sont rejoués à chaque démarrage de l'ESP32 (« autotest 12/12 »). C'est le meilleur jeu de test si vous portez le code dans un autre langage.

## 6. Comportements observés de la télécommande

- Les réglages sont envoyés **environ 7 s après le dernier appui** (plusieurs appuis rapides = une seule émission avec la valeur finale).
- **MAN** : ▲▼ règle la flamme (1 à 5) ; ventilateurs 1, 2, 3 de 0 à 5 ou AUTO.
- **AUTO** : la télécommande est le **thermostat**. La consigne et la température ne sont **pas** dans le signal, seule la puissance (1 à 5) y est. Règle mesurée : si la consigne est supérieure à la température affichée (partie entière), la télécommande monte d'**un cran par minute** jusqu'à 5 ; sinon elle descend d'un cran par minute jusqu'à 1 (jamais d'arrêt). Sans changement elle renvoie sa trame environ toutes les 20 minutes (parfois toutes les 30 s au début). La puissance émise est un **maximum** : le poêle module sa flamme réelle seul, et le niveau des ventilateurs limite la flamme maximale.
- **ECO** (appui long de 5 s sur MODE) : mode chiffre 4. Se comporte comme AUTO ; quand la consigne est atteinte et tenue **20 minutes**, la télécommande envoie une trame **flamme 0, drapeau 0** : le poêle passe en veille (afficheur « On » clignotant, ventilateurs encore ~12 min). Redémarrage quand la température atteint **consigne − 2 °C** et au moins 5 minutes après l'arrêt : trame ECO flamme 3, drapeau 0. Mesuré : arrêt ≈ 20 min après avoir atteint la consigne, rallumage ≈ 20 min plus tard.
- **NO AIR** (la « lune ») : la télécommande envoie (1) une trame AUTO, flamme 1, drapeau 1 (bip) ; (2) **5 minutes plus tard** une trame silencieuse (drapeau 0) avec les **3 ventilateurs à 0**, ce qui coupe les ventilateurs ; (3) à la sortie du mode : les ventilateurs d'avant, la flamme d'avant, drapeau 1. Le poêle ne coupe pas ses ventilateurs seul : c'est la trame des 5 minutes qui le fait.
- **Marche/arrêt** : la marche est une trame MAN normale (mode 0) ; l'arrêt est la même trame avec le mode `2`.
- Le **drapeau `x9`** : à 1, le poêle accepte l'ordre comme une action de l'utilisateur et fait un bip de confirmation ; à 0 (envoi automatique du thermostat) il ne bipe pas. L'ESP32 met le drapeau à 1 pour les ordres de l'application, à 0 pour son thermostat.
- Le **poêle n'émet rien** : impossible de lui demander son état. L'état affiché par l'application est ce qui a été entendu ou envoyé.

## 7. Notice du poêle (rappels utiles)

Source : notice MCZ Suite / Club / Musa 2.0, partie « Fonctionnement et nettoyage », REV.0 21/12/2015.

- **ECO** : arrêt si la température voulue est atteinte et tenue 20 min ; redémarrage à 2 °C sous la consigne et au moins 5 min après l'arrêt.
- **SLEEP** : fixe une **heure** d'extinction (pas une durée), par pas de 10 min, en MAN et AUTO. **NO AIR** : même menu, puissance 1 et ventilation arrêtée après environ 5 min. La lune affichée = NO AIR.
- **Comfort Air** (3 ventilateurs) en ventilation manuelle : puissance maximale = somme des niveaux ÷ 2.
- Alarmes : A01 à A14, SER (entretien après 2000 h). Sortie d'alarme : sélecteur du panneau de secours sur OFF jusqu'à disparition du code, puis REMOTE, éteindre et rallumer la télécommande.

## 8. Ce qui reste incertain

- Mode **TIMER** (programmes de la télécommande) : non décodé, non utilisé (la programmation est faite par le serveur ou l'ESP32).
- Niveaux AUTO des ventilateurs 1 et 3 : couverts par les tables, peu éprouvés à l'émission.
- Le bit `x10` et le sens exact de certains décalages (`⊕ 2`, `⊕ 5`).
- Le dernier bit du bloc 7 est souvent coupé dans les captures ; le firmware le force à 0 quand il manque (la parité du bloc le confirme).

## 9. Ce qui peut différer sur votre poêle

- L'adresse (blocs 1 à 3) : à capturer.
- Une autre génération de télécommande ou de carte peut changer les champs ou la table de contrôle. Méthode pour vérifier : capturer une dizaine de trames en changeant **un seul réglage à la fois**, vérifier que `y` reste XOR-linéaire en `x` (script `analyse/lin.py`) et que les Δ correspondent aux tables ci-dessus (script `analyse/gen_tables.py` les recalcule).
