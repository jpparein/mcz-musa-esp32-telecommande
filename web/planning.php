<?php
// ============================================================
// Poele MCZ : minuteurs et programmation (fonctions communes a api.php et esp.php)
// Ces fonctions ne parlent JAMAIS au poele : elles ajoutent des demandes dans la
// file des commandes, comme le ferait un clic sur la page. L'ESP32 les envoie ensuite.
// ============================================================
date_default_timezone_set('Europe/Paris');

function planning_tables($db) {
    $db->exec('CREATE TABLE IF NOT EXISTS minuteurs (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        echeance INTEGER NOT NULL,
        action TEXT NOT NULL,
        statut TEXT NOT NULL DEFAULT "actif"
    )');
    $db->exec('CREATE TABLE IF NOT EXISTS programmes (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        jours TEXT NOT NULL,
        heure TEXT NOT NULL,
        action TEXT NOT NULL,
        flamme INTEGER, vent1 INTEGER, vent2 INTEGER, vent3 INTEGER,
        actif INTEGER NOT NULL DEFAULT 1,
        derniere TEXT
    )');
    // Colonnes ajoutees plus tard : mode (MAN/AUTO/ECO) et consigne
    $cols = array_column($db->query('PRAGMA table_info(programmes)')->fetchAll(), 'name');
    if (!in_array('mode', $cols, true)) { $db->exec('ALTER TABLE programmes ADD COLUMN mode TEXT'); }
    if (!in_array('consigne', $cols, true)) { $db->exec('ALTER TABLE programmes ADD COLUMN consigne INTEGER'); }
}

// Ajoute une demande dans la file (remplace une demande identique encore en attente)
function planning_commande($db, $type, $valeur, $now) {
    $st = $db->prepare('UPDATE commandes SET statut = "remplacee", date_statut = ? WHERE type = ? AND statut = "attente"');
    $st->execute([$now, $type]);
    $st = $db->prepare('INSERT INTO commandes (date_demande, type, valeur) VALUES (?, ?, ?)');
    $st->execute([$now, $type, (string)$valeur]);
}

function planning_lancer_action($db, $action, $reglages, $now) {
    if ($action === 'on') {
        // Les reglages d'abord (acceptes meme poele eteint), l'allumage en dernier
        if (!empty($reglages['mode'])) { planning_commande($db, 'mode', $reglages['mode'], $now); }
        if (isset($reglages['consigne']) && $reglages['consigne'] !== null) {
            $u = $db->prepare('INSERT INTO etat (cle, valeur) VALUES ("consigne_ecran", ?) ON CONFLICT(cle) DO UPDATE SET valeur = excluded.valeur');
            $u->execute([(string)(int)$reglages['consigne']]);
        }
        foreach (['vent1', 'vent2', 'vent3', 'flamme'] as $k) {
            if (isset($reglages[$k]) && $reglages[$k] !== null) { planning_commande($db, $k, $reglages[$k], $now); }
        }
        planning_commande($db, 'marche', 'on', $now);
    } else {
        planning_commande($db, 'marche', 'off', $now);
    }
}

// A appeler regulierement : declenche ce qui est arrive a echeance.
// Un ordre rate de plus de 2 minutes n'est PAS rattrape (un poele ne doit pas s'allumer en retard sans prevenir).
function planning_executer($db, $now) {
    planning_tables($db);

    // ----- Minuteurs -----
    $q = $db->prepare('SELECT id, echeance, action FROM minuteurs WHERE statut = "actif" AND echeance <= ?');
    $q->execute([$now]);
    foreach ($q->fetchAll() as $m) {
        if ($now - (int)$m['echeance'] <= 120) {
            planning_lancer_action($db, $m['action'], [], $now);
            $s = 'fait';
        } else {
            $s = 'manque';
        }
        $u = $db->prepare('UPDATE minuteurs SET statut = ? WHERE id = ?');
        $u->execute([$s, $m['id']]);
    }

    // ----- Programmation -----
    $jour = (int)date('N', $now);          // 1 = lundi ... 7 = dimanche
    $date = date('Y-m-d', $now);
    foreach ($db->query('SELECT * FROM programmes WHERE actif = 1')->fetchAll() as $p) {
        if (!in_array((string)$jour, explode(',', $p['jours']), true)) { continue; }
        if ($p['derniere'] === $date) { continue; }
        $t = strtotime($date . ' ' . $p['heure'] . ':00');
        if ($t === false || $now < $t || $now - $t > 120) { continue; }
        $u = $db->prepare('UPDATE programmes SET derniere = ? WHERE id = ?');
        $u->execute([$date, $p['id']]);
        planning_lancer_action($db, $p['action'], [
            'flamme' => $p['flamme'], 'vent1' => $p['vent1'], 'vent2' => $p['vent2'], 'vent3' => $p['vent3'],
            'mode' => $p['mode'] ?? null, 'consigne' => $p['consigne'] ?? null,
        ], $now);
    }
}
