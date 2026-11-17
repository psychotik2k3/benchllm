
async function tick(){try{const s=await(await fetch('/api/status')).json();
r1.textContent=s.c1.in.toFixed(0);o1.textContent=s.c1.out.toFixed(0);x1.textContent=s.c1.ratio;
r2.textContent=s.c2.in.toFixed(0);o2.textContent=s.c2.out.toFixed(0);x2.textContent=s.c2.ratio;
i1.value=s.c1.ratio;i2.value=s.c2.ratio;
net.textContent='AP: '+s.ap+' | STA: '+s.sta;}catch(e){}}
setInterval(tick,1000);tick();
