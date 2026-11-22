// Page du mode secours et configuration : servie par l'ESP32 lui-meme (aucune dependance externe).
// Meme look que la telecommande de l'appli. Les echanges passent par /api/... (POST + en-tete X-Secours).
#pragma once
#include <pgmspace.h>

static const char PAGE_SECOURS[] PROGMEM = R"=====(<!doctype html>
<html lang="fr"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="theme-color" content="#e8662a">
<title>Poêle MCZ : secours et configuration</title>
<style>
.bandeau,.cadre,.carte{width:100%;max-width:440px}
:root{--fond:#f4efe9;--carte:#fff;--texte:#2a211b;--doux:#7a6c61;--ligne:#e6ddd3;--accent:#e8662a;--ok:#2e9e5b;--alerte:#c0392b}
@media (prefers-color-scheme:dark){:root{--fond:#15110e;--carte:#221c17;--texte:#f1e9e1;--doux:#a99a8d;--ligne:#3a3028;--accent:#f07a3a}}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
[hidden]{display:none!important}
body{margin:0;background:var(--fond);color:var(--texte);font:16px/1.4 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
main{max-width:640px;margin:0 auto;padding:14px 14px 40px;display:grid;grid-gap:12px;gap:12px;justify-items:center}
.note{color:var(--doux);font-size:.85rem;text-align:center;margin:0;max-width:460px}
.bandeau{width:min(100%,440px);background:#fff3e8;border:1px solid var(--accent);color:#7a3a14;border-radius:12px;padding:10px 12px;font-size:.92rem}
@media (prefers-color-scheme:dark){.bandeau{background:#3a2518;color:#f3c9a8}}
.cadre{container-type:inline-size;width:min(100%,440px)}
.appareil{--u:1cqw;width:100%;user-select:none;-webkit-user-select:none;touch-action:manipulation;background:linear-gradient(160deg,#34343a 0%,#232327 60%,#1c1c20 100%);color:#ececf0;border-radius:calc(11*var(--u));padding:calc(6*var(--u)) calc(6*var(--u)) calc(7*var(--u));display:grid;grid-gap:calc(5*var(--u));gap:calc(5*var(--u));box-shadow:0 calc(2.5*var(--u)) calc(7*var(--u)) rgba(0,0,0,.35),inset 0 calc(.4*var(--u)) calc(.4*var(--u)) rgba(255,255,255,.12)}
.tete{display:grid;grid-template-columns:1fr auto 1fr;grid-column-gap:calc(3*var(--u));column-gap:calc(3*var(--u));align-items:center;padding:0 calc(1.5*var(--u));font-size:calc(3.6*var(--u));letter-spacing:.22em;color:#a3a3ad;font-weight:700}
.horloge{text-align:center;font-size:calc(3.1*var(--u));letter-spacing:.12em;font-weight:700;color:#ff9a8c;white-space:nowrap}
.lien{justify-self:end;display:inline-flex;align-items:center;gap:calc(1.6*var(--u));letter-spacing:.04em;font-weight:600;font-size:calc(3.2*var(--u))}
.lien i{width:calc(2.4*var(--u));height:calc(2.4*var(--u));border-radius:50%;background:var(--alerte)}
.lien.ok i{background:#4cc27c;box-shadow:0 0 calc(1.6*var(--u)) #4cc27c}
.ecran{position:relative;border-radius:calc(5*var(--u));padding:calc(4.5*var(--u)) calc(4.5*var(--u)) calc(4*var(--u));color:#14231a;display:grid;grid-gap:calc(3.2*var(--u));gap:calc(3.2*var(--u));background:linear-gradient(180deg,#d3dfcf 0%,#bdcab8 100%);box-shadow:inset 0 calc(.8*var(--u)) calc(2.2*var(--u)) rgba(0,0,0,.35),inset 0 0 0 calc(.5*var(--u)) rgba(0,0,0,.28)}
.ecran.eteint .vif{opacity:.22}
.modes{display:flex;gap:calc(1.6*var(--u))}
.mode-puce{flex:1;text-align:center;font:inherit;font-size:calc(3.5*var(--u));font-weight:800;letter-spacing:.06em;padding:calc(1.1*var(--u)) 0;border-radius:calc(1.8*var(--u));border:calc(.45*var(--u)) solid rgba(20,35,26,.28);color:rgba(20,35,26,.4);background:transparent;cursor:pointer}
.mode-puce.actif{background:#14231a;color:#cfe0c9;border-color:#14231a}
.mode-puce:disabled{opacity:.45;cursor:not-allowed}
.centre{display:grid;grid-template-columns:1fr auto;grid-gap:calc(3*var(--u));gap:calc(3*var(--u));align-items:end}
#blocCons{display:grid;justify-items:end;text-align:right}
.etiq{font-size:calc(3*var(--u));font-weight:800;letter-spacing:.12em;opacity:.62}
.grand{font-size:calc(21*var(--u));font-weight:800;line-height:.95;letter-spacing:-.03em;font-variant-numeric:tabular-nums;white-space:nowrap}
.grand sup,.moyen sup{font-size:.38em;font-weight:700;vertical-align:top;position:relative;top:.25em;margin-left:.05em}
.moyen{font-size:calc(11.5*var(--u));font-weight:800;line-height:1;font-variant-numeric:tabular-nums;white-space:nowrap}
.moyen.fige{opacity:.5}
.prochain{font-size:calc(3.1*var(--u));font-weight:800;letter-spacing:.03em;border-top:calc(.3*var(--u)) solid rgba(20,35,26,.25);padding-top:calc(2*var(--u))}
.bas{display:grid;grid-template-columns:1.1fr 1fr;grid-gap:calc(3*var(--u));gap:calc(3*var(--u));align-items:end;border-top:calc(.45*var(--u)) solid rgba(20,35,26,.22);padding-top:calc(3*var(--u))}
.barres{display:flex;gap:calc(1.4*var(--u));align-items:flex-end;height:calc(11*var(--u));margin-top:calc(1.2*var(--u))}
.barres span{flex:1;border-radius:calc(.8*var(--u));background:rgba(20,35,26,.16)}
.barres span:nth-child(1){height:28%}.barres span:nth-child(2){height:46%}.barres span:nth-child(3){height:64%}.barres span:nth-child(4){height:82%}.barres span:nth-child(5){height:100%}
.barres span.on{background:#14231a}
.barres.auto span{background:transparent;box-shadow:inset 0 0 0 calc(.4*var(--u)) rgba(20,35,26,.35);opacity:.6}
.barres.auto span.on{background:#14231a;box-shadow:none;opacity:1}
.vents{display:grid;grid-gap:calc(.8*var(--u));gap:calc(.8*var(--u));margin-top:calc(.8*var(--u))}
.vent-l{display:grid;grid-template-columns:calc(5.6*var(--u)) 1fr calc(4*var(--u));align-items:center;grid-gap:calc(1.6*var(--u));gap:calc(1.6*var(--u));width:100%;padding:calc(.9*var(--u)) calc(1.3*var(--u));border:calc(.4*var(--u)) solid transparent;border-radius:calc(1.8*var(--u));background:transparent;color:inherit;font:inherit;cursor:pointer;text-align:left}
.vent-l small{font-size:calc(2.7*var(--u));font-weight:800;letter-spacing:.08em;opacity:.62}
.vent-l em{font-style:normal;font-size:calc(3*var(--u));font-weight:700;opacity:.55;text-align:right}
.vb{display:flex;gap:calc(.9*var(--u));align-items:flex-end;height:calc(4.6*var(--u))}
.vb i{flex:1;border-radius:calc(.4*var(--u));background:rgba(20,35,26,.16)}
.vb i:nth-child(1){height:30%}.vb i:nth-child(2){height:45%}.vb i:nth-child(3){height:62%}.vb i:nth-child(4){height:80%}.vb i:nth-child(5){height:100%}
.vb i.on{background:#14231a}
.vent-l.auto .vb i{background:transparent;box-shadow:inset 0 0 0 calc(.4*var(--u)) #14231a}
.vent-l.sel{border-color:#14231a;background:rgba(20,35,26,.1)}
.lune{display:inline-flex;align-items:center;gap:calc(1.4*var(--u));font:inherit;font-weight:800;font-size:calc(3.4*var(--u));background:transparent;border:calc(.4*var(--u)) solid transparent;border-radius:calc(1.8*var(--u));padding:calc(.6*var(--u)) calc(1.2*var(--u));color:#14231a;cursor:pointer;line-height:0;margin-top:calc(.8*var(--u))}
.lune svg{width:calc(6*var(--u));height:calc(6*var(--u));fill:none;stroke:#14231a;stroke-width:1.8;opacity:.35}
.lune.on svg{fill:#14231a;opacity:1}
.lune span{font-size:calc(2.6*var(--u));letter-spacing:.06em;opacity:.4;line-height:1}
.lune.on span{opacity:1}
.lune.sel{border-color:#14231a;background:rgba(20,35,26,.1)}
.clavier{display:grid;grid-template-columns:1fr calc(27*var(--u)) 1fr;grid-gap:calc(4*var(--u));gap:calc(4*var(--u));align-items:center}
.touche{position:relative;border:0;font:inherit;color:#f0f0f4;cursor:pointer;display:grid;place-items:center;align-content:center;grid-gap:calc(.6*var(--u));gap:calc(.6*var(--u));border-radius:calc(6*var(--u));min-height:calc(18*var(--u));font-size:calc(4.6*var(--u));font-weight:800;letter-spacing:.04em;background:linear-gradient(180deg,#46464e 0%,#323238 100%);box-shadow:0 calc(1*var(--u)) 0 #18181c,0 calc(1.6*var(--u)) calc(2.4*var(--u)) rgba(0,0,0,.45),inset 0 calc(.35*var(--u)) calc(.3*var(--u)) rgba(255,255,255,.18);transition:transform .08s,box-shadow .08s}
.touche:active{transform:translateY(calc(.9*var(--u)));box-shadow:0 calc(.1*var(--u)) 0 #18181c,inset 0 calc(.35*var(--u)) calc(.3*var(--u)) rgba(255,255,255,.12)}
.touche small{font-size:calc(2.9*var(--u));font-weight:700;color:#b4b4be;letter-spacing:.1em}
.touche.signe{font-size:calc(11*var(--u));font-weight:600;line-height:1}
.touche.power{width:calc(27*var(--u));height:calc(27*var(--u));min-height:0;border-radius:50%;color:#fff;background:radial-gradient(circle at 35% 28%,#5a5a63 0%,#2f2f35 70%)}
.touche.power svg{width:calc(11*var(--u));height:calc(11*var(--u))}
.touche.power.on{background:radial-gradient(circle at 35% 28%,#ffae6b 0%,#e8662a 62%,#b8431a 100%);box-shadow:0 calc(1*var(--u)) 0 #7e2c0e,0 0 calc(6*var(--u)) rgba(232,102,42,.65),inset 0 calc(.5*var(--u)) calc(.5*var(--u)) rgba(255,255,255,.35)}
.rang-mode{display:grid;grid-template-columns:1fr 1fr;grid-gap:calc(4*var(--u));gap:calc(4*var(--u))}
.touche.large{min-height:calc(15*var(--u));grid-auto-flow:column;grid-gap:calc(3*var(--u));gap:calc(3*var(--u))}
.touche.large b{color:#cfe0c9}.touche.large b:empty{display:none}
.touche.grisee{opacity:.35;cursor:not-allowed}
.touche.verrou{opacity:.4}
.carte{width:min(100%,440px);background:var(--carte);border:1px solid var(--ligne);border-radius:16px;padding:14px}
.carte h2{font-size:.78rem;letter-spacing:.06em;text-transform:uppercase;color:var(--doux);margin:0 0 10px;font-weight:600}
.carte select,.carte input{font:inherit;width:100%;padding:10px;border-radius:10px;border:1px solid var(--ligne);background:var(--fond);color:var(--texte);margin-top:6px}
.carte button{font:inherit;color:var(--texte);background:var(--fond);border:1px solid var(--ligne);border-radius:12px;padding:10px 14px;min-height:44px;cursor:pointer;width:100%;font-weight:600}
.carte details.tout{margin:0}.carte summary.titre{font-size:.78rem;letter-spacing:.06em;text-transform:uppercase;color:var(--doux);font-weight:600}.actuel{margin-top:10px;font-size:.9rem;color:var(--doux);margin-bottom:6px}details{margin:8px 0}summary{cursor:pointer;color:var(--accent);font-weight:600;font-size:.9rem}.expl{color:var(--doux);font-size:.86rem;margin:6px 0}.mdpl{display:flex;gap:8px;align-items:center;margin-top:6px}.mdpl input{margin:0!important}.carte .pet{width:auto;flex:none}.carte .plein{background:var(--accent);color:#fff;border-color:var(--accent);margin-top:8px}.carte #scan{margin-top:4px}.msg{min-height:1.3em;font-size:.88rem;margin-top:6px;color:var(--doux)}
.vieux .modes>*+*{margin-left:calc(1.6*var(--u))}.vieux .barres>*+*{margin-left:calc(1.4*var(--u))}.vieux .vb>*+*{margin-left:calc(.9*var(--u))}.vieux .lien>*+*{margin-left:calc(1.6*var(--u))}.vieux .lune>*+*{margin-left:calc(1.4*var(--u))}.vieux .mdpl>*+*{margin-left:8px}
</style></head><body><main>
<div class="bandeau" id="bandeau" hidden></div>
<div class="cadre"><div class="appareil">
  <div class="tete"><span>MCZ</span><span class="horloge">SECOURS</span><span class="lien" id="lien"><i></i><span id="texteLien"></span></span></div>
  <div class="ecran eteint" id="ecran">
    <div class="modes" id="modes">
      <button class="mode-puce" type="button" data-m="MAN">MAN</button>
      <button class="mode-puce" type="button" data-m="AUTO">AUTO</button>
      <button class="mode-puce" type="button" data-m="ECO">ECO</button>
    </div>
    <div class="centre vif">
      <div><div class="etiq">AMBIANCE</div><div class="grand" id="tAmb">--,-<sup>°C</sup></div></div>
      <div id="blocCons">
        <div class="etiq">CONSIGNE</div>
        <div class="moyen" id="tCons">--<sup>°C</sup></div>
        <button class="lune" id="lune" type="button" aria-label="Mode nuit" aria-pressed="false"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M20 14.5A8.5 8.5 0 0 1 9.5 4 8.5 8.5 0 1 0 20 14.5z"/></svg><span>NO AIR</span></button>
      </div>
    </div>
    <div class="prochain" id="hum" hidden></div>
    <div class="bas vif">
      <div><div class="etiq" id="flammeEtiq">FLAMME</div><div class="barres" id="barres"><span></span><span></span><span></span><span></span><span></span></div></div>
      <div><div class="etiq">VENTILOS</div>
        <div class="vents" id="vents">
          <button class="vent-l" type="button" data-v="1"><small>V1</small><span class="vb"><i></i><i></i><i></i><i></i><i></i></span><em>–</em></button>
          <button class="vent-l" type="button" data-v="2"><small>V2</small><span class="vb"><i></i><i></i><i></i><i></i><i></i></span><em>–</em></button>
          <button class="vent-l" type="button" data-v="3"><small>V3</small><span class="vb"><i></i><i></i><i></i><i></i><i></i></span><em>–</em></button>
        </div></div>
    </div>
  </div>
  <div class="clavier">
    <button class="touche signe" id="kMoins" type="button" aria-label="Diminuer">−</button>
    <button class="touche power" id="kPower" type="button" aria-label="Marche ou arrêt"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.4" stroke-linecap="round" aria-hidden="true"><path d="M12 3v9"/><path d="M6.3 6.8a8 8 0 1 0 11.4 0"/></svg></button>
    <button class="touche signe" id="kPlus" type="button" aria-label="Augmenter">+</button>
  </div>
  <div class="rang-mode">
    <button class="touche large grisee" type="button" disabled aria-label="Minuteur indisponible en secours"><small>TIMER</small></button>
    <button class="touche large" id="kVent" type="button" aria-label="Choisir un ventilateur" aria-pressed="false"><small>VENTILO</small><b id="kVentVal"></b></button>
  </div>
</div></div>
<p class="note" id="aide"></p>
<p class="note" id="wifi"></p>
<p class="note" id="msg"></p>
<section class="carte" id="reseau" hidden>
<details class="tout"><summary class="titre">Réseau de l'ESP32 : changer le Wi-Fi</summary>
  <div class="actuel">Wi-Fi actuel : <b id="ssidAct">?</b></div>
  <details><summary>À quoi ça sert ?</summary>
    <p class="expl">Tu changes ici le Wi-Fi que l'ESP32 utilise à la maison : nouvelle box, nouveau mot de passe, ou poêle installé ailleurs. Aucun câble ni ordinateur. Si le nom ou le mot de passe est faux, le mode secours revient seul au bout de 30 secondes et tu peux recommencer.</p></details>
  <button id="scan" type="button">Chercher les Wi-Fi proches</button>
  <select id="liste" hidden></select>
  <input id="ssid" placeholder="Nom exact du Wi-Fi caché" autocomplete="off" autocapitalize="off" spellcheck="false" hidden>
  <div class="mdpl" id="mdpl" hidden>
    <input id="mdp" type="password" placeholder="Mot de passe du Wi-Fi" autocomplete="off" autocapitalize="off" spellcheck="false">
    <button id="voir" type="button" class="pet">Voir</button></div>
  <button id="ok" type="button" class="plein" hidden>Enregistrer et redémarrer</button>
  <div class="msg" id="msgr"></div>
  <details><summary>Adresse du Raspberry Pi</summary>
    <p class="expl">L'adresse que l'ESP32 appelle pour joindre l'appli. À changer seulement si le Pi change d'adresse (autre box, autre réseau).</p>
    <div class="mdpl"><input id="adr" placeholder="192.168.1.20" autocomplete="off" inputmode="decimal"><button id="okpi" type="button" class="pet">Enregistrer</button></div></details>
</details></section>
</main>
<script>
(function(){
if(!(window.CSS&&CSS.supports&&CSS.supports('width','1cqw'))){var a=document.querySelector('.appareil'),c=a.parentNode;var m=function(){a.style.setProperty('--u',(c.clientWidth/100)+'px')};m();window.addEventListener('resize',m);window.addEventListener('load',m)}
var d=document.createElement('div');d.style.cssText='display:flex;flex-direction:column;row-gap:1px;position:absolute;visibility:hidden';d.appendChild(document.createElement('i'));d.appendChild(document.createElement('i'));document.body.appendChild(d);var ok=d.scrollHeight===1;document.body.removeChild(d);
if(!ok)document.documentElement.className+=' vieux';
})();
var S=null,selV=0,selT=null,pend={},tm={},chaine=Promise.resolve(),att=0,tc=null;
function $(i){return document.getElementById(i)}
function api(c,o){return fetch(c,{method:'POST',headers:{'X-Secours':'1','Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(o||{}).toString()}).then(function(r){return r.json()})}
function msg(t,id){$(id||'msg').textContent=t||''}
function fin(r){att--;msg(r&&r.ok?'':(r&&r.m)||'Pas de réponse de l\'ESP32.');setTimeout(charger,700)}
function envoyer(t,v){
  if(t==='marche')S.marche=(v==='on');else S[t]=v;
  att++;clearTimeout(tm[t]);pend[t]=v;
  if(tm[t])att--;
  tm[t]=setTimeout(function(){var x=pend[t];tm[t]=0;
    chaine=chaine.then(function(){return api('/api/cmd',{t:t,v:x})}).then(fin,function(){fin(null)});},450);
  dessiner();
}
function dessiner(){
  var e=S;if(!e)return;
  var nuit=!!e.nuit,mode=e.mode,man=mode==='MAN',marche=!!e.marche;
  $('lien').className='lien ok';
  $('texteLien').textContent=e.ap?'Wi-Fi secours':(e.pi?'Pi joignable':'Pi absent');
  var bd=$('bandeau');
  if(e.ap){bd.hidden=false;bd.textContent='Mode secours : l\'ESP32 pilote le poêle seul. Pour revenir au fonctionnement normal : BOOT 3 secondes, ou EN.';}
  else if(!e.pi){bd.hidden=false;bd.textContent='Le Raspberry Pi ne répond pas : l\'ESP32 pilote le poêle seul.';}
  else bd.hidden=true;
  $('ecran').classList.toggle('eteint',!marche);
  $('kPower').classList.toggle('on',marche);
  document.querySelectorAll('.mode-puce').forEach(function(p){p.classList.toggle('actif',p.dataset.m===mode);p.disabled=nuit;});
  $('lune').classList.toggle('on',nuit);$('lune').classList.toggle('sel',selV===4);
  $('lune').setAttribute('aria-pressed',nuit);
  $('tAmb').innerHTML=(e.temp===null?'--,-':e.temp.toFixed(1).replace('.',','))+'<sup>°C</sup>';
  $('tCons').innerHTML=Math.round(e.consigne)+'<sup>°C</sup>';
  $('tCons').classList.toggle('fige',man||nuit);
  document.querySelectorAll('#blocCons .etiq,#tCons').forEach(function(x){x.style.visibility=(man||nuit)?'hidden':'visible'});
  $('hum').hidden=e.hum===null;if(e.hum!==null)$('hum').textContent='HUMIDITÉ '+Math.round(e.hum)+' %';
  var f=e.flamme,fa=!man&&!nuit;
  $('barres').classList.toggle('auto',fa);
  $('flammeEtiq').textContent=!fa?'FLAMME':(mode==='ECO'&&f===0)?'ECO : poêle en veille':mode+' : puissance max '+(f?f:'-')+'/5';
  document.querySelectorAll('#barres span').forEach(function(b,i){b.classList.toggle('on',f!==null&&i<f)});
  document.querySelectorAll('.vent-l').forEach(function(b){
    var i=+b.dataset.v,n=e['vent'+i],au=n===6,nv=(n===null||n===undefined)?0:n;
    b.classList.toggle('auto',au);b.classList.toggle('sel',selV===i);
    b.querySelectorAll('.vb i').forEach(function(x,k){x.classList.toggle('on',!au&&k<nv)});
    b.querySelector('em').textContent=(n===null||n===undefined)?'–':(au?'A':n);
  });
  $('kVentVal').textContent=selV===4?'LUNE':(selV?'V'+selV:'');
  var vr=nuit&&selV!==4;
  $('kMoins').classList.toggle('verrou',vr);$('kPlus').classList.toggle('verrou',vr);
  $('aide').textContent=!e.base?'État du poêle inconnu : touche le bouton marche pour envoyer un ordre.':
    vr?'Mode nuit (NO AIR) actif : flamme, ventilos et mode sont verrouillés. Pour sortir : touche la lune, ou VENTILO puis −.':
    selV===4?'LUNE (NO AIR) : + l\'active, − la désactive.':
    selV?'Ventilateur '+selV+' choisi : + et − règlent son niveau (0 à 5, puis A).':
    (man?'En MAN, + et − règlent la flamme. ':'En '+mode+', + et − règlent la consigne. ')+'TIMER, programmes et journal ne sont pas disponibles en secours.';
  $('wifi').textContent=e.ap?'Wi-Fi de secours Poele-MCZ (sans box ni Pi)':(e.wifi?'Wi-Fi : '+(e.ssid||'?')+' ('+e.rssi+' dBm)':'Pas de Wi-Fi');
  $('reseau').hidden=!e.ap;
  $('ssidAct').textContent=e.ssid||'aucun';if(e.ap&&!$('adr').dataset.fait){$('adr').value=e.piAdr;$('adr').dataset.fait='1';}
}
function armer(){clearTimeout(selT);if(selV)selT=setTimeout(function(){selV=0;dessiner()},8000)}
function bnuit(a){if(!S||!!S.nuit===a)return;S.nuit=a;att++;dessiner();
  api('/api/nuit',{v:a?1:0}).then(fin,function(){fin(null)})}
function pm(d){
  if(!S)return;if(S.nuit&&selV!==4)return;
  if(selV===4){bnuit(d>0);armer();return}
  if(selV){var v=S['vent'+selV];envoyer('vent'+selV,Math.min(6,Math.max(0,(v||0)+d)));armer();return}
  if(S.mode==='MAN'){envoyer('flamme',Math.min(5,Math.max(1,(S.flamme||1)+d)));return}
  var c=Math.min(30,Math.max(5,Math.round(S.consigne)+d));S.consigne=c;att++;dessiner();
  clearTimeout(tc);tc=setTimeout(function(){api('/api/consigne',{v:c}).then(fin,function(){fin(null)})},500);
}
function charger(){
  if(att>0)return;
  fetch('/api/etat',{headers:{'X-Secours':'1'},cache:'no-store'}).then(function(r){return r.json()}).then(function(j){if(att>0)return;S=j;dessiner()}).catch(function(){$('lien').className='lien';$('texteLien').textContent='ESP32 ne répond pas'});
}
$('kPower').onclick=function(){if(S)envoyer('marche',S.marche?'off':'on')};
document.querySelectorAll('.mode-puce').forEach(function(b){b.onclick=function(){if(S&&!S.nuit)envoyer('mode',b.dataset.m)}});
$('kMoins').onclick=function(){pm(-1)};$('kPlus').onclick=function(){pm(1)};
$('lune').onclick=function(){if(S)bnuit(!S.nuit)};
$('kVent').onclick=function(){if(!S)return;selV=S.nuit?(selV===4?0:4):(selV+1)%5;armer();dessiner()};
document.querySelectorAll('.vent-l').forEach(function(b){b.onclick=function(){if(!S||S.nuit)return;var i=+b.dataset.v;selV=selV===i?0:i;armer();dessiner()}});
$('scan').onclick=function(){
  msg('Recherche en cours (quelques secondes, la connexion peut se couper un instant)…','msgr');
  fetch('/api/reseaux',{method:'POST',headers:{'X-Secours':'1'}}).then(function(r){return r.json()}).then(function(j){
    var l=$('liste');l.innerHTML='';
    j.r.forEach(function(w){var o=document.createElement('option');o.value=w.s;o.textContent=w.s+'  ('+w.q+' %'+(w.o?'':', sans mot de passe')+')';l.appendChild(o)});
    var o=document.createElement('option');o.value='';o.textContent='Autre réseau (caché) : taper le nom…';l.appendChild(o);
    l.hidden=false;$('mdpl').hidden=false;$('ok').hidden=false;choisi();
    msg(j.r.length?'':'Aucun réseau trouvé : choisis « Autre réseau » et tape le nom.','msgr');
  }).catch(function(){msg('La recherche a échoué, recommence.','msgr')});
};
function choisi(){var c=$('liste').value==='';$('ssid').hidden=!c;if(c)$('ssid').focus()}
$('liste').onchange=choisi;
$('voir').onclick=function(){var t=$('mdp').type==='password';$('mdp').type=t?'text':'password';this.textContent=t?'Cacher':'Voir'};
$('ok').onclick=function(){
  var s=$('liste').value===''?$('ssid').value.trim():$('liste').value;
  if(!s){msg('Tape le nom du Wi-Fi.','msgr');return}
  api('/api/wifi',{s:s,m:$('mdp').value}).then(function(r){
    msg(r.ok?'Enregistré. La carte redémarre (environ 30 s) et cherche ce Wi-Fi.':(r.m||'Refusé.'),'msgr');
  }).catch(function(){msg('Enregistré ? La carte redémarre.','msgr')});
};
$('okpi').onclick=function(){
  api('/api/pi',{a:$('adr').value}).then(function(r){msg(r.ok?'Adresse enregistrée. Redémarrage…':(r.m||'Refusé.'),'msgr')}).catch(function(){msg('Enregistrée ? Redémarrage.','msgr')});
};
charger();setInterval(charger,3000);
</script></body></html>
)=====";
