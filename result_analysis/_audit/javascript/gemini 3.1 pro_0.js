
    setInterval(function(){
      fetch('/status').then(r=>r.json()).then(d=>{
        document.getElementById('r9in').innerText = d.rpm9_in;
        document.getElementById('r9out').innerText = d.rpm9_out;
        document.getElementById('r6in').innerText = d.rpm6_in;
        document.getElementById('r6out').innerText = d.rpm6_out;
      });
    }, 1000);
  