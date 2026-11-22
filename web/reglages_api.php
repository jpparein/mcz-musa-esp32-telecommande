<?php
// ============================================================
// Poele MCZ : API de la page Reglages.
// GET  : etat (mode de fonctionnement, ESP32, capteur)
// POST : mode (meme site seulement)
// ============================================================
$dossier = '/home/rpi/poele-mcz-data';
$action = $_GET['action'] ?? 'etat';

function rep($donnees, $code = 200) {
    http_response_code($code);
    header('Content-Type: application/json; charset=utf-8');
    header('Cache-Control: no-store');
    echo json_encode($donnees, JSON_UNESCAPED_UNICODE);
    exit;
}

$db = new PDO('sqlite:' . $dossier . '/poele.db');
$db->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
$db->setAttribute(PDO::ATTR_DEFAULT_FETCH_MODE, PDO::FETCH_ASSOC);
$db->exec('PRAGMA busy_timeout = 3000');
$db->exec('CREATE TABLE IF NOT EXISTS etat (cle TEXT PRIMARY KEY, valeur TEXT)');

$e = [];
foreach ($db->query('SELECT cle, valeur FROM etat') as $l) { $e[$l['cle']] = $l['valeur']; }
$maintenant = time();

if ($action === 'etat') {
    rep([
        'mode_fonct'  => $e['mode_fonct'] ?? 'telecommande',
        'esp_connecte'=> isset($e['esp_vu']) && $maintenant - (int)$e['esp_vu'] < 60,
        'capteur_ok'  => isset($e['mesure_vue']) && $maintenant - (int)$e['mesure_vue'] < 300,
        'temperature' => isset($e['temperature']) ? (float)$e['temperature'] : null,
        'consigne'    => isset($e['consigne_ecran']) ? (float)$e['consigne_ecran'] : 20.0,
    ]);
}

if ($action === 'journal') {
    $db->exec('CREATE TABLE IF NOT EXISTS journal (id INTEGER PRIMARY KEY AUTOINCREMENT, t INTEGER NOT NULL, msg TEXT NOT NULL)');
    $lignes = [];
    foreach ($db->query('SELECT t, msg FROM journal ORDER BY id DESC LIMIT 150') as $l) {
        $lignes[] = (new DateTime('@' . (int)$l['t']))->setTimezone(new DateTimeZone('Europe/Paris'))->format('d/m H:i:s') . '  ' . $l['msg'];
    }
    rep(['lignes' => $lignes]);
}

if ($_SERVER['REQUEST_METHOD'] !== 'POST' || ($_SERVER['HTTP_X_REQUESTED_WITH'] ?? '') !== 'poele') {
    rep(['erreur' => 'requete refusee'], 400);
}
$corps = json_decode(file_get_contents('php://input'), true);
if (!is_array($corps)) { $corps = []; }

if ($action === 'mode') {
    $v = $corps['valeur'] ?? '';
    if ($v !== 'telecommande' && $v !== 'remplacement') { rep(['erreur' => 'valeur invalide'], 400); }
    $st = $db->prepare('INSERT INTO etat (cle, valeur) VALUES ("mode_fonct", ?) ON CONFLICT(cle) DO UPDATE SET valeur = excluded.valeur');
    $st->execute([$v]);
    rep(['ok' => true]);
}
// Consigne de la page Telecommande (memorisee, pas encore appliquee au poele)
if ($action === 'consigne') {
    $v = $corps['valeur'] ?? null;
    if (!is_numeric($v) || $v < 5 || $v > 30) { rep(['erreur' => 'consigne entre 5 et 30'], 400); }
    $st = $db->prepare('INSERT INTO etat (cle, valeur) VALUES ("consigne_ecran", ?) ON CONFLICT(cle) DO UPDATE SET valeur = excluded.valeur');
    $st->execute([round((float)$v * 2) / 2]);
    rep(['ok' => true]);
}
// Mode nuit : memorise l'etat et les reglages a restaurer au reveil
if ($action === 'nuit') {
    $actif = !empty($corps['actif']);
    $st = $db->prepare('INSERT INTO etat (cle, valeur) VALUES (?, ?) ON CONFLICT(cle) DO UPDATE SET valeur = excluded.valeur');
    $st->execute(['nuit', $actif ? '1' : '0']);
    if ($actif && isset($corps['sauve']) && is_array($corps['sauve'])) {
        $sv = [];
        foreach (['flamme', 'vent1', 'vent2', 'vent3'] as $k) { $sv[$k] = isset($corps['sauve'][$k]) && is_numeric($corps['sauve'][$k]) ? (int)$corps['sauve'][$k] : null; }
        $st->execute(['nuit_sauve', json_encode($sv)]);
    }
    rep(['ok' => true]);
}
rep(['erreur' => 'action inconnue'], 404);
