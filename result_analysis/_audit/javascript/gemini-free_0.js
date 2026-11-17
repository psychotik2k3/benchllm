
setInterval(() => {
  fetch('/data').then(r => r.json()).then(d => {
    document.getElementById('in1').innerText = d.in1;
    document.getElementById('out1').innerText = d.out1;
    document.getElementById('in2').innerText = d.in2;
    document.getElementById('out2').innerText = d.out2;
  });
}, 1000);

fetch('/config').then(r => r.json()).then(d => {
  document.getElementById('r1').value = d.r1;
  document.getElementById('r2').value = d.r2;
  document.getElementById('ssid').value = d.ssid;
});
