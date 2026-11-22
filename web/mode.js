// Mode de fonctionnement (avec telecommande / remplacement) : memorise sur le Pi, commun a tous les appareils.
// Chaque page declare quel mode elle montre (data-page="poele" ou "remote") ; si le mode memorise est different,
// on est envoye vers la bonne page. La bascule en haut de la page change le mode.
(function () {
  var racine = document.documentElement;
  var voulu = { poele: 'telecommande', remote: 'remplacement' }[racine.getAttribute('data-page')];
  var courant = null;
  racine.classList.add('verif');                       // masque la page le temps de verifier (evite un flash)
  var fin = setTimeout(function () { racine.classList.remove('verif'); }, 1500);

  function marquer() {
    document.querySelectorAll('.bascule button').forEach(function (b) {
      var on = b.getAttribute('data-mode') === courant;
      b.classList.toggle('actif', on);
      b.setAttribute('aria-pressed', on ? 'true' : 'false');
    });
  }
  function page(mode) { return mode === 'remplacement' ? 'telecommande.html' : 'index.html'; }

  fetch('reglages_api.php?action=etat', { cache: 'no-store' })
    .then(function (r) { return r.json(); })
    .then(function (d) {
      courant = d.mode_fonct;
      if (voulu && courant !== voulu) { location.replace(page(courant)); return; }
      clearTimeout(fin); racine.classList.remove('verif'); marquer();
    })
    .catch(function () { /* hors ligne : la page s'affiche telle quelle apres 1,5 s */ });

  document.addEventListener('DOMContentLoaded', function () {
    marquer();
    document.querySelectorAll('.bascule button').forEach(function (b) {
      b.addEventListener('click', function () {
        var mode = b.getAttribute('data-mode');
        if (mode === courant) return;
        fetch('reglages_api.php?action=mode', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json', 'X-Requested-With': 'poele' },
          body: JSON.stringify({ valeur: mode })
        }).then(function (r) { if (r.ok) location.href = page(mode); });
      });
    });
  });
})();
