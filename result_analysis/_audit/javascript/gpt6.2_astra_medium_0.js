
async function poll(){try{let r=await fetch('/live',{cache:'no-store'});if(!r.ok)throw Error();
let d=await r.json();document.querySelector('#state').textContent=(d.ok?'Convertisseur connecté':'Convertisseur absent')+' · STA '+d.ip;
d.rpm.forEach((n,i)=>document.getElementById('v'+i).textContent=d.ok?n:'—')}
catch{document.querySelector('#state').textContent='Connexion interrompue';
for(let i=0;i<4;i++)document.getElementById('v'+i).textContent='—'}setTimeout(poll,750)}poll()