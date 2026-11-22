<?php
// ============================================================
// Poele MCZ : journal d'apprentissage.
// Pendant un enregistrement, chaque trame de la telecommande et chaque mesure de
// temperature sont notees avec l'heure et la consigne tapee par l'utilisateur.
// Sert a comprendre comment la telecommande regle la flamme en mode AUTO.
// Ne commande RIEN.
// ============================================================

function appr_tables($db) {
    $db->exec('CREATE TABLE IF NOT EXISTS apprentissage (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        t INTEGER NOT NULL,
        type TEXT NOT NULL,
        mode TEXT, marche INTEGER,
        flamme INTEGER, vent1 INTEGER, vent2 INTEGER, vent3 INTEGER,
        temperature REAL, humidite REAL, consigne REAL
    )');
    $cols = array_column($db->query('PRAGMA table_info(apprentissage)')->fetchAll(), 'name');
    if (!in_array('note', $cols, true)) { $db->exec('ALTER TABLE apprentissage ADD COLUMN note TEXT'); }
}

function appr_etat_cles($db) {
    $e = [];
    foreach ($db->query('SELECT cle, valeur FROM etat') as $l) { $e[$l['cle']] = $l['valeur']; }
    return $e;
}

// Ajoute une ligne au journal (seulement si un enregistrement est en cours, sauf $force)
function appr_noter($db, $type, $force = false, $note = null) {
    $e = appr_etat_cles($db);
    if (!$force && (($e['appr_actif'] ?? '0') !== '1')) { return; }
    appr_tables($db);
    $maintenant = time();
    $tempOk = isset($e['temperature'], $e['mesure_vue']) && $maintenant - (int)$e['mesure_vue'] < 120;
    $st = $db->prepare('INSERT INTO apprentissage (t, type, mode, marche, flamme, vent1, vent2, vent3, temperature, humidite, consigne, note)
                        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)');
    $n = function ($c) use ($e) { return isset($e[$c]) ? (int)$e[$c] : null; };
    $st->execute([
        $maintenant, $type, $e['mode'] ?? null,
        isset($e['poele']) ? ($e['poele'] === 'allume' ? 1 : 0) : null,
        $n('flamme'), $n('vent1'), $n('vent2'), $n('vent3'),
        $tempOk ? (float)$e['temperature'] : null,
        $tempOk && isset($e['humidite']) ? (float)$e['humidite'] : null,
        isset($e['appr_consigne']) && $e['appr_consigne'] !== '' ? (float)$e['appr_consigne'] : null,
        $note,
    ]);
}
