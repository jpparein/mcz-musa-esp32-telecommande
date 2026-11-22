// Apparence (automatique, clair, sombre) : memorisee sur cet appareil, appliquee a toutes les pages.
(function () {
  try {
    var t = localStorage.getItem('poele-theme');
    if (t === 'light' || t === 'dark') document.documentElement.setAttribute('data-theme', t);
  } catch (e) {}
})();

// Copie de l'appli sur l'appareil (seulement en HTTPS)
(function () {
  try {
    if ('serviceWorker' in navigator && (location.protocol === 'https:' || location.hostname === 'localhost')) {
      window.addEventListener('load', function () {
        navigator.serviceWorker.register('sw.js', { scope: './' }).catch(function () {});
      });
    }
  } catch (e) {}
})();
