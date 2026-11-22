<?php
// ============================================================
// Poele MCZ : API de la page Apprentissage.
// Lecture (GET) : etat de l'enregistrement, export CSV.
// Actions (POST, meme site) : demarrer, arreter, consigne, effacer.
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
require __DIR__ . '/apprentissage.php';
appr_tables($db);

function ranger($db, $cle, $valeur) {
    $st = $db->prepare('INSERT INTO etat (cle, valeur) VALUES (?, ?) ON CONFLICT(cle) DO UPDATE SET valeur = excluded.valeur');
    $st->execute([$cle, (string)$valeur]);
}

if ($action === 'export') {
    header('Content-Type: text/csv; charset=utf-8');
    header('Content-Disposition: attachment; filename="apprentissage-poele-' . date('Y-m-d-His') . '.csv"');
    echo "date_heure,t_unix,type,mode,allume,flamme,vent1,vent2,vent3,temperature,humidite,consigne,note\n";
    foreach ($db->query('SELECT * FROM apprentissage ORDER BY id') as $l) {
        echo implode(',', [
            date('Y-m-d H:i:s', (int)$l['t']), $l['t'], $l['type'], $l['mode'], $l['marche'],
            $l['flamme'], $l['vent1'], $l['vent2'], $l['vent3'], $l['temperature'], $l['humidite'], $l['consigne'],
            ($l['note'] === null || $l['note'] === '') ? '' : '"' . str_replace('"', '""', $l['note']) . '"',
        ]) . "\n";
    }
    exit;
}

if ($action === 'etat') {
    $e = appr_etat_cles($db);
    $nb = (int)$db->query('SELECT COUNT(*) FROM apprentissage')->fetchColumn();
    $derniers = $db->query('SELECT t, type, note, mode, flamme, vent1, vent2, vent3, temperature, consigne FROM apprentissage ORDER BY id DESC LIMIT 8')->fetchAll();
    $maintenant = time();
    $mesure = isset($e['mesure_vue']) && $maintenant - (int)$e['mesure_vue'] < 120;
    rep([
        'maintenant' => $maintenant,
        'actif'      => ($e['appr_actif'] ?? '0') === '1',
        'debut'      => isset($e['appr_debut']) ? (int)$e['appr_debut'] : null,
        'consigne'   => isset($e['appr_consigne']) && $e['appr_consigne'] !== '' ? (float)$e['appr_consigne'] : null,
        'nb'         => $nb,
        'poele'      => $e['poele'] ?? 'inconnu',
        'mode'       => $e['mode'] ?? 'inconnu',
        'flamme'     => isset($e['flamme']) ? (int)$e['flamme'] : null,
        'vent1'      => isset($e['vent1']) ? (int)$e['vent1'] : null,
        'vent2'      => isset($e['vent2']) ? (int)$e['vent2'] : null,
        'vent3'      => isset($e['vent3']) ? (int)$e['vent3'] : null,
        'temperature'=> $mesure ? (float)$e['temperature'] : null,
        'humidite'   => $mesure && isset($e['humidite']) ? (float)$e['humidite'] : null,
        'derniers'   => $derniers,
    ]);
}

// ---------- Actions : POST du meme site seulement ----------
if ($_SERVER['REQUEST_METHOD'] !== 'POST' || ($_SERVER['HTTP_X_REQUESTED_WITH'] ?? '') !== 'poele') {
    rep(['erreur' => 'requete refusee'], 400);
}
$corps = json_decode(file_get_contents('php://input'), true);
if (!is_array($corps)) { $corps = []; }

if ($action === 'consigne') {
    $v = $corps['valeur'] ?? null;
    if (!is_numeric($v) || $v < 5 || $v > 30) { rep(['erreur' => 'consigne entre 5 et 30'], 400); }
    ranger($db, 'appr_consigne', round((float)$v, 1));
    appr_noter($db, 'consigne');
    rep(['ok' => true]);
}
if ($action === 'demarrer') {
    $e = appr_etat_cles($db);
    if (!isset($e['appr_consigne']) || $e['appr_consigne'] === '') { rep(['erreur' => 'Indique d\'abord la consigne de la telecommande.'], 400); }
    ranger($db, 'appr_actif', '1');
    ranger($db, 'appr_debut', time());
    appr_noter($db, 'debut');
    rep(['ok' => true]);
}
if ($action === 'note') {
    $e = appr_etat_cles($db);
    if (($e['appr_actif'] ?? '0') !== '1') { rep(['erreur' => 'Demarre d\'abord l\'enregistrement.'], 400); }
    $t = trim((string)($corps['texte'] ?? ''));
    if ($t === '' || mb_strlen($t) > 200) { rep(['erreur' => 'Note vide ou trop longue.'], 400); }
    appr_noter($db, 'note', false, $t);
    rep(['ok' => true]);
}
if ($action === 'arreter') {
    appr_noter($db, 'fin');
    ranger($db, 'appr_actif', '0');
    rep(['ok' => true]);
}
if ($action === 'effacer') {
    $db->exec('DELETE FROM apprentissage');
    ranger($db, 'appr_actif', '0');
    rep(['ok' => true]);
}
rep(['erreur' => 'action inconnue'], 404);
