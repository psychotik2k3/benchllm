
var RMIN=100,RMAX=2000,RSC=100;   /* doit rester aligne avec RATIO_MIN/RATIO_MAX/RATIO_SCALE */
var st={c0:{},c1:{}};
function P(i){return document.getElementById(i)}
function api(p){return fetch(p,{cache:'no-store'}).then(function(r){return r.json()}).then(function(j){st=j;paint()})}
function ctrl(s){return fetch('/control',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:s,cache:'no-store'}).then(function(r){return r.json()}).then(function(j){st=j;paint()})}
function paint(){
  for(var i=0;i<2;i++){var c=st['c'+i];if(!c||c.ratio===undefined)continue;
    P('rin'+i).textContent=c.rin;P('rout'+i).textContent=c.rout;
    P('per'+i).textContent=(c.med?c.med:0)+' / '+(c.out?c.out:0)+' tick (50 \u00b5s)';
    var s=P('st'+i);
    if(c.trap){s.className='badge err';s.textContent='SIGNAL TRAPP\u00c9 (sortie coup\u00e9e)';}
    else if(c.lost){s.className='badge err';s.textContent='TACH ABSENT';}
    else if(c.mode===0){s.className='badge no';s.textContent='ARRET\u00c9';}
    else if(c.sim){s.className='badge ok';s.textContent=c.mode===2?'BYPASS ACTIF':'SIMULATION ACTIVE';}
    else{s.className='badge no';s.textContent='ATTENTE DE SIGNAL';}
    var r=P('r'+i);if(document.activeElement!==r)r.value=(c.ratio/RSC).toFixed(2);
    var e=P('e'+i);e.textContent=c.en?'Sortie ACTIVE':'Sortie OFF';e.className=c.en?'':'hi';
    var m=P('m'+i);if(document.activeElement!==m)m.value=c.mode;
  }
  P('mst').textContent=st.mst?'ON':'OFF';P('mst').className='badge '+(st.mst?'ok':'no');
  P('ppr').value=st.ppr;P('mstsel').value=st.mst;
  var d=P('dir');if(document.activeElement!==d)d.value=st.dir;
  P('up').textContent='marche '+Math.floor(st.up/1000)+' s';
  P('apip').textContent=st.apip;
  P('ip').textContent=(st.wif==1)?('client '+st.ip+' ('+st.rssi+' dBm)'):'client hors ligne';
  P('wifi').textContent=(st.wif==1)?'WiFi client OK':'point d\'acces seul';
}
document.addEventListener('click',function(ev){
  var b=ev.target.closest?ev.target.closest('button'):null;if(!b)return;
  if(b.dataset.c!==undefined){
    var i=+b.dataset.c,c=st['c'+i],v=Math.round(c.ratio);
    if(b.dataset.d!==undefined){v+= +b.dataset.d}
    else if(b.dataset.set){var f=parseFloat(P('r'+i).value);if(!isNaN(f))v=Math.round(f*RSC)}
    else if(b.dataset.e!==undefined){ctrl('e'+i+'='+(c.en?0:1));return}
    if(v<RMIN)v=RMIN;if(v>RMAX)v=RMAX;ctrl('r'+i+'='+v);return}
  if(b.id==='apply'){ctrl('ppr='+P('ppr').value+'&mst='+P('mstsel').value+'&dir='+P('dir').value+'&m0='+P('m0').value+'&m1='+P('m1').value);return}
  if(b.id==='restart'){if(confirm('Redemarrer le simulateur ?'))fetch('/restart',{cache:'no-store'});return}
  if(b.id==='applywifi'){
    fetch('/wifi',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},
      body:'ssid='+encodeURIComponent(P('ssid').value)+'&pass='+encodeURIComponent(P('wpass').value)+
           '&apssid='+encodeURIComponent(P('apssid').value)+'&appass='+encodeURIComponent(P('appass').value),
      cache:'no-store'}).then(function(){setTimeout(function(){api('/state')},4000)});return}
});
['m0','m1'].forEach(function(id){P(id).addEventListener('change',function(){ctrl('m'+id[1]+'='+P(id).value)})});
setInterval(function(){api('/state')},1200);
api('/state');
