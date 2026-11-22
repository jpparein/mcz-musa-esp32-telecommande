<?php
// Mesure : on note les requetes qui prennent plus d'une seconde (diagnostic des coupures de l'ESP32)
$__t0 = microtime(true);
register_shutdown_function(function () use ($__t0) {
    $d = microtime(true) - $__t0;
    if ($d > 1.0) { @file_put_contents('/home/rpi/poele-mcz-data/lent.log', date('d/m H:i:s') . ' ' . 'api.php' . ' ' . ($_GET['action'] ?? '') . ' ' . round($d, 2) . "s\n", FILE_APPEND); }
});
// ============================================================
// Poele MCZ : interface de pilotage (version 1, SANS emission radio)
// Ce fichier gere : l'etat du poele (reporte plus tard par l'ESP32)
// et la file des commandes demandees depuis la page web.
// Il n'envoie RIEN au poele : l'ESP32 viendra lire la file plus tard.
// ============================================================
header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store');

// La base est rangee HORS du site web, pour qu'on ne puisse pas la telecharger.
$dossier = '/home/rpi/poele-mcz-data';
if (!is_dir($dossier)) { reponse(['erreur' => 'dossier de donnees absent'], 500); }

$db = new PDO('sqlite:' . $dossier . '/poele.db');
$db->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
$db->setAttribute(PDO::ATTR_DEFAULT_FETCH_MODE, PDO::FETCH_ASSOC);
$db->exec('PRAGMA busy_timeout = 3000');
$db->exec('CREATE TABLE IF NOT EXISTS etat (cle TEXT PRIMARY KEY, valeur TEXT)');
$db->exec('CREATE TABLE IF NOT EXISTS commandes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    date_demande INTEGER NOT NULL,
    type TEXT NOT NULL,
    valeur TEXT NOT NULL,
    statut TEXT NOT NULL DEFAULT "attente",
    date_statut INTEGER
)');

require __DIR__ . '/planning.php';
planning_tables($db);

function reponse($donnees, $code = 200) {
    http_response_code($code);
    echo json_encode($donnees, JSON_UNESCAPED_UNICODE);
    exit;
}

function lireEtat($db) {
    $etat = [];
    foreach ($db->query('SELECT cle, valeur FROM etat') as $l) { $etat[$l['cle']] = $l['valeur']; }
    return $etat;
}

$action = $_GET['action'] ?? 'etat';

// ---------- Lecture de l'etat et des commandes recentes ----------
if ($action === 'etat') {
    $e = lireEtat($db);
    $maintenant = time();
    $derniere = isset($e['derniere_trame']) ? (int)$e['derniere_trame'] : 0;
    $esp = isset($e['esp_vu']) ? (int)$e['esp_vu'] : 0;

    $cmds = [];
    $q = $db->query('SELECT id, date_demande, type, valeur, statut FROM commandes ORDER BY id DESC LIMIT 15');
    foreach ($q as $l) { $cmds[] = $l; }
    $attente = (int)$db->query('SELECT COUNT(*) FROM commandes WHERE statut = "attente"')->fetchColumn();

    $minuteurs = [];
    foreach ($db->query('SELECT id, echeance, action FROM minuteurs WHERE statut = "actif" ORDER BY echeance') as $l) {
        $minuteurs[] = ['id' => (int)$l['id'], 'echeance' => (int)$l['echeance'], 'action' => $l['action']];
    }
    $programmes = [];
    foreach ($db->query('SELECT * FROM programmes ORDER BY heure, id') as $l) {
        $n = function ($v) { return $v === null ? null : (int)$v; };
        $programmes[] = [
            'id' => (int)$l['id'], 'jours' => array_map('intval', explode(',', $l['jours'])),
            'heure' => $l['heure'], 'action' => $l['action'],
            'flamme' => $n($l['flamme']), 'vent1' => $n($l['vent1']), 'vent2' => $n($l['vent2']), 'vent3' => $n($l['vent3']),
            'mode' => $l['mode'] ?? null, 'consigne' => $n($l['consigne'] ?? null),
            'actif' => (int)$l['actif'] === 1,
        ];
    }
    reponse([
        'minuteurs'      => $minuteurs,
        'programmes'     => $programmes,
        'maintenant'     => $maintenant,
        'temperature'    => isset($e['temperature']) ? (float)$e['temperature'] : null,
        'humidite'       => isset($e['humidite']) ? (float)$e['humidite'] : null,
        'mesure_age'     => isset($e['mesure_vue']) ? $maintenant - (int)$e['mesure_vue'] : null,
        'poele'          => $e['poele'] ?? 'inconnu',       // allume / eteint / inconnu
        'mode'           => $e['mode'] ?? 'inconnu',        // MAN / AUTO / ECO / inconnu
        'flamme'         => isset($e['flamme']) ? (int)$e['flamme'] : null,
        'vent1'          => isset($e['vent1']) ? (int)$e['vent1'] : null,
        'vent2'          => isset($e['vent2']) ? (int)$e['vent2'] : null,
        'vent3'          => isset($e['vent3']) ? (int)$e['vent3'] : null,
        'age_trame'      => $derniere ? $maintenant - $derniere : null,
        'nuit'           => ($e['nuit'] ?? '0') === '1',
        'nuit_sauve'     => isset($e['nuit_sauve']) ? json_decode($e['nuit_sauve'], true) : null,
        'esp_connecte'   => $esp && ($maintenant - $esp) < 60,
        'en_attente'     => $attente,        'commandes'      => $cmds,
    ]);
}

// ---------- Les actions suivantes demandent une requete POST en JSON ----------
if ($_SERVER['REQUEST_METHOD'] !== 'POST' || ($_SERVER['HTTP_X_REQUESTED_WITH'] ?? '') !== 'poele') {
    reponse(['erreur' => 'requete refusee'], 400);
}
$corps = json_decode(file_get_contents('php://input'), true);
if (!is_array($corps)) { reponse(['erreur' => 'donnees illisibles'], 400); }

// ---------- Nouvelle commande ----------
if ($action === 'commande') {
    $type = $corps['type'] ?? '';
    $valeur = null;

    if ($type === 'marche') {                       // allumer / eteindre
        $v = $corps['valeur'] ?? '';
        if ($v === 'on' || $v === 'off') { $valeur = $v; }
    } elseif ($type === 'mode') {                   // MAN / AUTO / ECO
        $v = $corps['valeur'] ?? '';
        if ($v === 'MAN' || $v === 'AUTO' || $v === 'ECO') { $valeur = $v; }
    } elseif ($type === 'flamme') {                 // puissance de flamme 1 a 5
        $v = (int)($corps['valeur'] ?? 0);
        if ($v >= 1 && $v <= 5) { $valeur = (string)$v; }
    } elseif ($type === 'vent1' || $type === 'vent2' || $type === 'vent3') {
        $v = (int)($corps['valeur'] ?? -1);         // 0 a 5, 6 = AUTO
        if ($v >= 0 && $v <= 6) { $valeur = (string)$v; }
    }
    if ($valeur === null) { reponse(['erreur' => 'commande invalide'], 400); }

    // Une meme commande du meme type encore en attente est remplacee par la nouvelle
    $st = $db->prepare('UPDATE commandes SET statut = "remplacee", date_statut = ? WHERE type = ? AND statut = "attente"');
    $st->execute([time(), $type]);

    $st = $db->prepare('INSERT INTO commandes (date_demande, type, valeur) VALUES (?, ?, ?)');
    $st->execute([time(), $type, $valeur]);
    reponse(['ok' => true, 'id' => (int)$db->lastInsertId()]);
}

// ---------- Minuteur : "eteindre / allumer dans X minutes" ----------
if ($action === 'minuteur') {
    $a = $corps['action'] ?? '';
    $min = (int)($corps['minutes'] ?? 0);
    if (($a !== 'on' && $a !== 'off') || $min < 1 || $min > 1440) { reponse(['erreur' => 'minuteur invalide'], 400); }
    $nb = (int)$db->query('SELECT COUNT(*) FROM minuteurs WHERE statut = "actif"')->fetchColumn();
    if ($nb >= 10) { reponse(['erreur' => 'trop de minuteurs'], 400); }
    $st = $db->prepare('INSERT INTO minuteurs (echeance, action) VALUES (?, ?)');
    $st->execute([time() + $min * 60, $a]);
    reponse(['ok' => true, 'id' => (int)$db->lastInsertId()]);
}
if ($action === 'minuteur_annuler') {
    $st = $db->prepare('UPDATE minuteurs SET statut = "annule" WHERE id = ? AND statut = "actif"');
    $st->execute([(int)($corps['id'] ?? 0)]);
    reponse(['ok' => true]);
}

// ---------- Programmation ----------
if ($action === 'programme_enregistrer') {
    $jours = $corps['jours'] ?? [];
    $heure = $corps['heure'] ?? '';
    $a = $corps['action'] ?? '';
    if (!is_array($jours) || !count($jours)) { reponse(['erreur' => 'jours manquants'], 400); }
    $j = [];
    foreach ($jours as $x) {
        if (!is_int($x) || $x < 1 || $x > 7) { reponse(['erreur' => 'jour invalide'], 400); }
        $j[$x] = $x;
    }
    ksort($j);
    if (!is_string($heure) || !preg_match('/^([01]\d|2[0-3]):[0-5]\d$/', $heure)) { reponse(['erreur' => 'heure invalide'], 400); }
    if ($a !== 'on' && $a !== 'off') { reponse(['erreur' => 'action invalide'], 400); }
    // Reglages facultatifs (seulement pour un allumage). null = on ne change rien.
    $lim = ['flamme' => [1, 5], 'vent1' => [0, 6], 'vent2' => [0, 6], 'vent3' => [0, 6]];
    $r = [];
    foreach ($lim as $k => $b) {
        $v = $corps[$k] ?? null;
        if ($a === 'off' || $v === null) { $r[$k] = null; continue; }
        if (!is_int($v) || $v < $b[0] || $v > $b[1]) { reponse(['erreur' => $k . ' invalide'], 400); }
        $r[$k] = $v;
    }
    $mode = $corps['mode'] ?? null; $cons = $corps['consigne'] ?? null;
    if ($a === 'off' || $mode === null) { $mode = null; $cons = null; }
    elseif (!in_array($mode, ['MAN', 'AUTO', 'ECO'], true)) { reponse(['erreur' => 'mode invalide'], 400); }
    if ($mode === 'MAN') { $cons = null; }
    if ($cons !== null && (!is_int($cons) || $cons < 5 || $cons > 30)) { reponse(['erreur' => 'consigne invalide'], 400); }
    $actif = ($corps['actif'] ?? true) ? 1 : 0;
    $id = (int)($corps['id'] ?? 0);
    if ($id > 0) {
        $st = $db->prepare('UPDATE programmes SET jours=?, heure=?, action=?, flamme=?, vent1=?, vent2=?, vent3=?, actif=?, mode=?, consigne=?, derniere=NULL WHERE id=?');
        $st->execute([implode(',', $j), $heure, $a, $r['flamme'], $r['vent1'], $r['vent2'], $r['vent3'], $actif, $mode, $cons, $id]);
    } else {
        if ((int)$db->query('SELECT COUNT(*) FROM programmes')->fetchColumn() >= 20) { reponse(['erreur' => 'trop de programmes'], 400); }
        $st = $db->prepare('INSERT INTO programmes (jours, heure, action, flamme, vent1, vent2, vent3, actif, mode, consigne) VALUES (?,?,?,?,?,?,?,?,?,?)');
        $st->execute([implode(',', $j), $heure, $a, $r['flamme'], $r['vent1'], $r['vent2'], $r['vent3'], $actif, $mode, $cons]);
        $id = (int)$db->lastInsertId();
    }
    reponse(['ok' => true, 'id' => $id]);
}
if ($action === 'programme_actif') {
    $st = $db->prepare('UPDATE programmes SET actif = ?, derniere = NULL WHERE id = ?');
    $st->execute([($corps['actif'] ?? false) ? 1 : 0, (int)($corps['id'] ?? 0)]);
    reponse(['ok' => true]);
}
if ($action === 'programme_supprimer') {
    $st = $db->prepare('DELETE FROM programmes WHERE id = ?');
    $st->execute([(int)($corps['id'] ?? 0)]);
    reponse(['ok' => true]);
}

// ---------- Annuler une commande encore en attente ----------
if ($action === 'annuler') {
    $id = (int)($corps['id'] ?? 0);
    $st = $db->prepare('UPDATE commandes SET statut = "annulee", date_statut = ? WHERE id = ? AND statut = "attente"');
    $st->execute([time(), $id]);
    reponse(['ok' => true]);
}

reponse(['erreur' => 'action inconnue'], 404);
