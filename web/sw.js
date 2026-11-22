// Poele MCZ : copie de l'appli gardee sur l'appareil (s'ouvre meme si le Pi est en panne).
// Principe : on essaie TOUJOURS le Pi d'abord (donc jamais de page perimee) ; s'il ne repond pas
// en 3 secondes, on sert la derniere copie gardee. Les donnees (.php) ne sont jamais gardees.
var VERSION = 'poele-v1';
var COPIE = ['./', 'index.html', 'telecommande.html', 'aide.html', 'reglages.html',
             'apprentissage.html', 'mode.js', 'theme.js', 'manifest.webmanifest',
             'icons/icon-192.png', 'icons/icon-512.png', 'icons/apple-touch-icon.png'];

self.addEventListener('install', function (e) {
  e.waitUntil(caches.open(VERSION).then(function (c) {
    // un fichier absent ne doit pas empecher l'installation
    return Promise.all(COPIE.map(function (u) { return c.add(u).catch(function () {}); }));
  }).then(function () { return self.skipWaiting(); }));
});

self.addEventListener('activate', function (e) {
  e.waitUntil(caches.keys().then(function (noms) {
    return Promise.all(noms.filter(function (n) { return n !== VERSION; })
                            .map(function (n) { return caches.delete(n); }));
  }).then(function () { return self.clients.claim(); }));
});

self.addEventListener('fetch', function (e) {
  var r = e.request;
  if (r.method !== 'GET') return;
  var url = new URL(r.url);
  if (url.origin !== self.location.origin) return;       // rien d'externe
  if (/\.php(\?|$)/.test(url.pathname)) return;           // les donnees passent toujours par le reseau
  e.respondWith(reseauPuisCopie(r));
});

function reseauPuisCopie(r) {
  return new Promise(function (ok) {
    var fini = false;
    var minuteur = setTimeout(function () { copie(); }, 3000);
    function copie() {
      if (fini) return;
      caches.match(r, { ignoreSearch: true }).then(function (c) {
        if (c && !fini) { fini = true; ok(c); }
      });
    }
    fetch(r).then(function (rep) {
      clearTimeout(minuteur);
      if (fini) return;                                   // la copie a deja repondu
      fini = true;
      if (rep && rep.ok) {
        var dup = rep.clone();
        caches.open(VERSION).then(function (c) { c.put(r, dup); });
      }
      ok(rep);
    }).catch(function () {
      clearTimeout(minuteur);
      caches.match(r, { ignoreSearch: true }).then(function (c) {
        if (fini) return;
        fini = true;
        ok(c || new Response('Hors ligne', { status: 503, headers: { 'Content-Type': 'text/plain; charset=utf-8' } }));
      });
    });
  });
}
