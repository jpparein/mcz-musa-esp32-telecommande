<?php
// Mesure : on note les requetes qui prennent plus d'une seconde (diagnostic des coupures de l'ESP32)
$__t0 = microtime(true);
register_shutdown_function(function () use ($__t0) {
    $d = microtime(true) - $__t0;
    if ($d > 1.0) { @file_put_contents('/home/rpi/poele-mcz-data/lent.log', date('d/m H:i:s') . ' ' . 'esp.php' . ' ' . ($_GET['action'] ?? '') . ' ' . round($d, 2) . "s\n", FILE_APPEND); }
});
// ============================================================
// Poele MCZ : point d'entree reserve a l'ESP32.
// L'ESP32 y depose ce qu'il ecoute de la telecommande.
// Il faut le bon jeton (fichier token.txt, hors du site web).
// Ce fichier ne commande RIEN : il ne fait que ranger l'etat recu.
// ============================================================
header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store');

function reponse($donnees, $code = 200) {
    http_response_code($code);
    echo json_encode($donnees, JSON_UNESCAPED_UNICODE);
    exit;
}

$dossier = '/home/rpi/poele-mcz-data';
$fichierJeton = $dossier . '/token.txt';
if (!is_file($fichierJeton)) { reponse(['erreur' => 'jeton absent'], 500); }
$jeton = trim(file_get_contents($fichierJeton));
$recu = $_SERVER['HTTP_X_TOKEN'] ?? '';
if ($jeton === '' || !hash_equals($jeton, $recu)) { reponse(['erreur' => 'refuse'], 403); }
if ($_SERVER['REQUEST_METHOD'] !== 'POST') { reponse(['erreur' => 'POST obligatoire'], 405); }

$db = new PDO('sqlite:' . $dossier . '/poele.db');
$db->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
$db->setAttribute(PDO::ATTR_DEFAULT_FETCH_MODE, PDO::FETCH_ASSOC);
$db->exec('PRAGMA busy_timeout = 3000');
$db->exec('CREATE TABLE IF NOT EXISTS etat (cle TEXT PRIMARY KEY, valeur TEXT)');

require_once __DIR__ . '/apprentissage.php';

function ranger($db, $cle, $valeur) {
    $st = $db->prepare('INSERT INTO etat (cle, valeur) VALUES (?, ?) ON CONFLICT(cle) DO UPDATE SET valeur = excluded.valeur');
    $st->execute([$cle, (string)$valeur]);
}

$action = $_GET['action'] ?? '';
$corps = json_decode(file_get_contents('php://input'), true);
if (!is_array($corps)) { $corps = []; }

// L'ESP32 signale qu'il est en vie
if ($action === 'ping') {
    ranger($db, 'esp_vu', time());
    reponse(['ok' => true]);
}

// Une trame de la telecommande vient d'etre decodee
if ($action === 'trame') {
    $modes = ['MAN', 'AUTO', 'ECO', ''];
    $mode = $corps['mode'] ?? '';
    $flamme = $corps['flamme'] ?? null;
    $v1 = $corps['vent1'] ?? null; $v2 = $corps['vent2'] ?? null; $v3 = $corps['vent3'] ?? null;
    $niveauOk = function ($v, $max) { return is_int($v) && $v >= 0 && $v <= $max; };

    if (!in_array($mode, $modes, true) || !is_bool($corps['marche'] ?? null)
        || !$niveauOk($flamme, 5) || !$niveauOk($v1, 6) || !$niveauOk($v2, 6) || !$niveauOk($v3, 6)) {
        reponse(['erreur' => 'donnees invalides'], 400);
    }
    ranger($db, 'poele', $corps['marche'] ? 'allume' : 'eteint');
    if ($mode !== '') { ranger($db, 'mode', $mode); }
    ranger($db, 'flamme', $flamme);
    ranger($db, 'vent1', $v1); ranger($db, 'vent2', $v2); ranger($db, 'vent3', $v3);
    ranger($db, 'derniere_trame', time());
    ranger($db, 'esp_vu', time());
    appr_noter($db, 'trame');
    reponse(['ok' => true]);
}

// Mesure de temperature de la piece (capteur DHT22 branche sur l'ESP32)
if ($action === 'mesure') {
    $t = $corps['t'] ?? null; $h = $corps['h'] ?? null;
    if (!is_numeric($t) || !is_numeric($h) || $t < 0 || $t > 45 || $h < 0 || $h > 100) {
        reponse(['erreur' => 'donnees invalides'], 400);
    }
    ranger($db, 'temperature', round((float)$t, 1));
    ranger($db, 'humidite', round((float)$h, 1));
    ranger($db, 'mesure_vue', time());
    ranger($db, 'esp_vu', time());
    appr_noter($db, 'mesure');
    reponse(['ok' => true]);
}

// L'ESP32 demande le dernier etat connu du poele (pour fabriquer ses trames
// sans avoir besoin d'entendre la telecommande)
if ($action === 'etat') {
    $e = [];
    foreach ($db->query('SELECT cle, valeur FROM etat') as $l) { $e[$l['cle']] = $l['valeur']; }
    ranger($db, 'esp_vu', time());
    reponse([
        'poele'  => $e['poele'] ?? 'inconnu',
        'mode'   => $e['mode'] ?? 'MAN',
        'flamme' => (int)($e['flamme'] ?? 3),
        'vent1'  => (int)($e['vent1'] ?? 1),
        'vent2'  => (int)($e['vent2'] ?? 1),
        'vent3'  => (int)($e['vent3'] ?? 1),
        'consigne'   => isset($e['consigne_ecran']) ? (float)$e['consigne_ecran'] : 20.0,
        'mode_fonct' => $e['mode_fonct'] ?? 'telecommande',
        'nuit'   => (int)($e['nuit'] ?? 0),
    ]);
}

// Journal des evenements de l'ESP32 (garde les 500 dernieres lignes)
if ($action === 'journal') {
    $db->exec('CREATE TABLE IF NOT EXISTS journal (id INTEGER PRIMARY KEY AUTOINCREMENT, t INTEGER NOT NULL, msg TEXT NOT NULL)');
    $l = $corps['l'] ?? [];
    if (!is_array($l)) { reponse(['erreur' => 'donnees invalides'], 400); }
    $now = time();
    $st = $db->prepare('INSERT INTO journal (t, msg) VALUES (?, ?)');
    $db->exec('BEGIN');
    foreach (array_slice($l, 0, 40) as $x) {
        if (!is_array($x) || !isset($x['m'])) { continue; }
        $age = isset($x['a']) && is_numeric($x['a']) ? max(0, (int)$x['a']) : 0;
        $st->execute([$now - $age, mb_substr(preg_replace('/[\x00-\x1f]/', ' ', (string)$x['m']), 0, 120)]);
    }
    $db->exec('COMMIT');
    $db->exec('DELETE FROM journal WHERE id < (SELECT MAX(id) FROM journal) - 500');
    ranger($db, 'esp_vu', time());
    reponse(['ok' => true]);
}

// Compte rendu du thermostat de l'ESP32 (mode remplacement uniquement)
if ($action === 'thermo') {
    $p = $corps['puissance'] ?? null;
    $actif = !empty($corps['actif']);
    $raison = preg_replace('/[^a-z_]/', '', (string)($corps['raison'] ?? ''));
    ranger($db, 'thermo_actif', $actif ? 1 : 0);
    ranger($db, 'thermo_raison', $raison);
    ranger($db, 'thermo_vu', time());
    if (is_int($p) && (($p >= 1 && $p <= 5) || ($p === 0 && $raison === 'eco_veille'))) {
        ranger($db, 'thermo_puissance', $p);
        if ($actif) { ranger($db, 'flamme', $p); }
    }
    ranger($db, 'esp_vu', time());
    reponse(['ok' => true]);
}

// L'ESP32 demande la prochaine commande a executer (la plus ancienne en attente).
// Elle passe a "envoi" pour ne jamais etre donnee deux fois.
if ($action === 'commande') {
    $db->exec('CREATE TABLE IF NOT EXISTS commandes (
        id INTEGER PRIMARY KEY AUTOINCREMENT, date_demande INTEGER NOT NULL,
        type TEXT NOT NULL, valeur TEXT NOT NULL, statut TEXT NOT NULL DEFAULT "attente", date_statut INTEGER)');
    ranger($db, 'esp_vu', time());
    require __DIR__ . '/planning.php';
    // Minuteurs et programmation : verifies au plus toutes les 5 secondes
    $lu = $db->query('SELECT valeur FROM etat WHERE cle = "planning_check"')->fetchColumn();
    if (!$lu || time() - (int)$lu >= 5) {
        ranger($db, 'planning_check', time());
        planning_executer($db, time());
    }
    // Une commande restee "en cours" depuis plus de 30 s : l'ESP32 n'a jamais repondu
    $st = $db->prepare('UPDATE commandes SET statut = "echec", date_statut = ? WHERE statut = "envoi" AND date_statut < ?');
    $st->execute([time(), time() - 30]);
    // Cadence : l'ESP32 refuse deux emissions a moins de 3 s d'ecart, on espace de 4 s
    $enCours = (int)$db->query('SELECT COUNT(*) FROM commandes WHERE statut = "envoi"')->fetchColumn();
    $dernier = (int)$db->query('SELECT COALESCE(MAX(date_statut), 0) FROM commandes WHERE statut IN ("envoyee","echec","ignoree")')->fetchColumn();
    if ($enCours > 0 || time() - $dernier < 4) { reponse(new stdClass()); }
    // Une demande de plus de 2 minutes est trop vieille : on ne l'envoie pas
    $st = $db->prepare('UPDATE commandes SET statut = "ignoree", date_statut = ? WHERE statut = "attente" AND date_demande < ?');
    $st->execute([time(), time() - 120]);
    $db->exec('BEGIN IMMEDIATE');
    $c = $db->query('SELECT id, type, valeur FROM commandes WHERE statut = "attente" ORDER BY id ASC LIMIT 1')->fetch();
    if ($c) {
        $st = $db->prepare('UPDATE commandes SET statut = "envoi", date_statut = ? WHERE id = ?');
        $st->execute([time(), $c['id']]);
    }
    $db->exec('COMMIT');
    reponse($c ? ['id' => (int)$c['id'], 'type' => $c['type'], 'valeur' => (string)$c['valeur']] : new stdClass());
}

// L'ESP32 dit ce qu'il a fait de la commande
if ($action === 'fait') {
    $id = (int)($corps['id'] ?? 0);
    $statut = $corps['statut'] ?? '';
    if (!in_array($statut, ['envoyee', 'echec', 'ignoree'], true) || $id < 1) { reponse(['erreur' => 'donnees invalides'], 400); }
    $st = $db->prepare('UPDATE commandes SET statut = ?, date_statut = ? WHERE id = ? AND statut = "envoi"');
    $st->execute([$statut, time(), $id]);
    // Ordre bien envoye : la page affiche maintenant l'etat voulu
    if ($statut === 'envoyee') {
        $q = $db->prepare('SELECT type, valeur FROM commandes WHERE id = ?');
        $q->execute([$id]);
        $c = $q->fetch();
        if ($c) {
            if ($c['type'] === 'marche') { ranger($db, 'poele', $c['valeur'] === 'on' ? 'allume' : 'eteint'); }
            elseif ($c['type'] === 'mode') { ranger($db, 'mode', $c['valeur']); }
            elseif (in_array($c['type'], ['flamme', 'vent1', 'vent2', 'vent3'], true)) { ranger($db, $c['type'], (int)$c['valeur']); }
        }
    }
    reponse(['ok' => true]);
}

reponse(['erreur' => 'action inconnue'], 404);
