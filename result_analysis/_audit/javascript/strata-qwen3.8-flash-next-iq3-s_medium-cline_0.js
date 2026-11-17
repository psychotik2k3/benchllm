
var filled=false,tst=0;
function T(i,v){var e=document.getElementById(i);if(e)e.textContent=v}
function V(i){return document.getElementById(i).value}
function stat(d){
 for(var k=0;k<2;k++){var c=d.ch[k];
  T('i'+k,c.inRpm);T('s'+k,c.simRpm);T('r'+k,'x'+(c.ratio/100).toFixed(2));
  T('f'+k,c.floor?c.floor:'-');T('p'+k,c.inPul+' / '+c.simPul);
  T('q'+k,c.outHz+' Hz / '+c.pulseUs+' us');
  var a=document.getElementById('a'+k);
  a.textContent=c.alive?'simulation active':'attente de signal';
  a.className='badge '+(c.alive?'on':'off');}
 T('ver',d.ver);T('up',d.up);T('heap',d.heap);T('isr',d.isrHz+' Hz / '+d.isrUs+' us');
 T('ipap',d.apIp);T('sta',d.staOk?'connecte ('+d.rssi+' dBm)':'deconnecte');T('ipsta',d.staIp);
 if(!filled){filled=true;
  for(var j=0;j<2;j++){var e=d.ch[j];
   document.getElementById('ratio'+j).value=(e.cfgRatio/100).toFixed(2);
   document.getElementById('off'+j).value=e.cfgOff;
   document.getElementById('pprin'+j).value=e.pprIn;
   document.getElementById('pprout'+j).value=e.pprOut;
   document.getElementById('hold'+j).value=e.cfgHold;}
  document.getElementById('toff').value=d.toff;
  document.getElementById('testrpm').value=d.testRpm;
  document.getElementById('apssid').value=d.apSsid;
  document.getElementById('stassid').value=d.staSsid;
  tst=d.testMode;
  document.getElementById('btest').textContent=tst?'Arreter le test':'Demarrer le test';}}
function poll(){fetch('/api/status',{cache:'no-store'}).then(function(r){return r.json()}).then(stat)
 .catch(function(){T('sta','erreur reseau')})}
function get(u){return fetch(u,{cache:'no-store'}).then(function(r){return r.json()})
 .then(function(j){return j.ok}).catch(function(){return false})}
function Q(o){return Object.keys(o).map(function(k){return k+'='+encodeURIComponent(o[k])}).join('&')}
setInterval(poll,1000);poll();
document.getElementById('btnsave').onclick=function(){
 get('/cfg?'+Q({ratio0:V('ratio0'),off0:V('off0'),pprin0:V('pprin0'),pprout0:V('pprout0'),hold0:V('hold0'),
 ratio1:V('ratio1'),off1:V('off1'),pprin1:V('pprin1'),pprout1:V('pprout1'),hold1:V('hold1'),
 toff:V('toff'),testrpm:V('testrpm')})).then(function(o){alert(o?'Reglages enregistres et appliques':'Echec de lenregistrement')})};
document.getElementById('btest').onclick=function(){tst=tst?0:1;
 get('/test?on='+tst+'&rpm='+V('testrpm'));
 document.getElementById('btest').textContent=tst?'Arreter le test':'Demarrer le test'};
document.getElementById('btnwifi').onclick=function(){
 var o={apssid:V('apssid'),stassid:V('stassid')};
 if(V('appass'))o.appass=V('appass');
 if(V('stapass'))o.stapass=V('stapass');
 get('/wifi?'+Q(o)).then(function(o2){alert(o2?'WiFi enregistre, redemarrage en cours':'Echec')})};
document.getElementById('btnfac').onclick=function(){
 if(confirm('Effacer la configuration enregistree ?'))
  get('/factory').then(function(){alert('Redemarrage sur les reglages d usine')})};
