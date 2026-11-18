#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <stddef.h>
extern "C" {
#include <user_interface.h>
}

// Le Wemos ne mesure et ne cadence aucun front tach.
constexpr uint8_t CS=D1, SCK_PIN=D5, MISO_PIN=D6, MOSI_PIN=D7;
struct Settings {
  uint32_t version;
  char ap[33], key[64], sta[33], pass[64];
  uint16_t r9, r6;  // centièmes
  uint32_t crc;
};
Settings cfg{};
ESP8266WebServer http(80);
uint16_t periods[4]{};
bool connectedFPGA=false, pendingWifi=false, fsOK=false;
uint32_t wifiAt, lastPoll;
String token;

uint32_t digest(const Settings &s) {
  uint32_t crc=~0u;
  const uint8_t *p=reinterpret_cast<const uint8_t*>(&s);
  for(size_t i=0;i<offsetof(Settings,crc);i++) {
    crc^=p[i];
    for(int b=0;b<8;b++) crc=(crc>>1)^((crc&1)?0xEDB88320u:0);
  }
  return ~crc;
}

bool save(Settings &s) {
  if(!fsOK) return false;
  s.crc=digest(s);
  File f=LittleFS.open("/next","w");
  if(!f) return false;
  bool ok=f.write(reinterpret_cast<const uint8_t*>(&s),sizeof(s))==sizeof(s);
  f.close();
  return ok && LittleFS.rename("/next","/settings");
}

void loadSettings() {
  fsOK=LittleFS.begin();
  File f=fsOK?LittleFS.open("/settings","r"):File();
  if(f && f.size()==sizeof(cfg) &&
     f.read(reinterpret_cast<uint8_t*>(&cfg),sizeof(cfg))==sizeof(cfg) &&
     cfg.version==1 && cfg.crc==digest(cfg)) return;
  f.close();
  cfg={};
  cfg.version=1;
  snprintf(cfg.ap,sizeof(cfg.ap),"Deye-Tach-%06X",ESP.getChipId());
  snprintf(cfg.key,sizeof(cfg.key),"%08x%08x",os_random(),os_random());
  cfg.r9=cfg.r6=100;
  save(cfg);
  Serial.printf("\nPremier accès : %s / %s\n",cfg.ap,cfg.key);
}

void network() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(cfg.ap,cfg.key);
  WiFi.disconnect(false);
  if(cfg.sta[0]) WiFi.begin(cfg.sta,cfg.pass);
}

// Le SPI lent peut être interrompu entre deux bits, sans effet sur la conversion.
// Les ratios sont renvoyés périodiquement pour reprendre après un reset du FPGA.
void exchange() {
  uint8_t wire[9]={0xA5,uint8_t(cfg.r9>>8),uint8_t(cfg.r9),
                    uint8_t(cfg.r6>>8),uint8_t(cfg.r6),0,0,0,0};
  for(int i=0;i<5;i++) wire[5]^=wire[i];
  digitalWrite(CS,LOW);
  delayMicroseconds(3);
  for(uint8_t &value:wire) {
    uint8_t readback=0;
    for(uint8_t mask=128;mask;mask>>=1) {
      digitalWrite(MOSI_PIN,(value&mask)!=0);
      delayMicroseconds(3);
      digitalWrite(SCK_PIN,HIGH);
      delayMicroseconds(3);
      readback=(readback<<1)|digitalRead(MISO_PIN);
      digitalWrite(SCK_PIN,LOW);
      delayMicroseconds(3);
    }
    value=readback;
  }
  digitalWrite(CS,HIGH);
  connectedFPGA=wire[0]==0xD3;
  for(int i=0;i<4;i++) periods[i]=(wire[1+2*i]<<8)|wire[2+2*i];
}

bool authorized() {
  if(http.authenticate("admin",cfg.key)) return true;
  http.requestAuthentication();
  return false;
}

const char PAGE[] PROGMEM=R"HTML(<!doctype html><html lang="fr"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Ventilation Deye</title>
<style>body{font:1rem system-ui;max-width:40rem;margin:2rem auto;padding:0 1rem;color:#193943}
table{width:100%;text-align:left}td,th{padding:.5rem}label{display:block;margin:.7rem 0}
input{padding:.35rem}fieldset{margin:1rem 0}button{padding:.6rem}</style>
<h1>Ventilation Deye</h1><p id="state">Connexion…</p>
<table><tr><th>Canal<th>Lu (tr/min)<th>Simulé (tr/min)
<tr><th>9 cm<td id="v0">—<td id="v1">—
<tr><th>6 cm<td id="v2">—<td id="v3">—</table>
<form method="post" action="/save"><input type="hidden" name="token" value="@TOKEN@">
<fieldset><legend>Multiplicateurs</legend>
<label>9 cm <input required type="number" min="1" max="8" step=".01" name="r9" value="@R9@"></label>
<label>6 cm <input required type="number" min="1" max="8" step=".01" name="r6" value="@R6@"></label></fieldset>
<fieldset><legend>Réseau Wi-Fi</legend>
<label>Nom du point d’accès <input required maxlength="32" name="ap" value="@AP@"></label>
<label>Nouvelle clé AP et admin <input type="password" minlength="8" maxlength="63" name="key"></label>
<label>SSID du routeur <input maxlength="32" name="sta" value="@STA@"></label>
<label>Nouvelle clé du routeur <input type="password" maxlength="63" name="pass"></label>
<label><input type="checkbox" name="open"> Routeur sans mot de passe</label>
<p>Une clé vide conserve la clé mémorisée. Effacer le SSID déconnecte la station.</p></fieldset>
<button>Enregistrer</button></form><script>
async function poll(){try{let r=await fetch('/live',{cache:'no-store'});if(!r.ok)throw Error();
let d=await r.json();document.querySelector('#state').textContent=(d.ok?'Convertisseur connecté':'Convertisseur absent')+' · STA '+d.ip;
d.rpm.forEach((n,i)=>document.getElementById('v'+i).textContent=d.ok?n:'—')}
catch{document.querySelector('#state').textContent='Connexion interrompue';
for(let i=0;i<4;i++)document.getElementById('v'+i).textContent='—'}setTimeout(poll,750)}poll()</script></html>)HTML";

String escapeAttr(String s) {
  s.replace("&","&amp;"); s.replace("\"","&quot;"); s.replace("<","&lt;"); s.replace(">","&gt;");
  s.replace("@","&#64;"); return s;
}

void page() {
  if(!authorized()) return;
  String s=FPSTR(PAGE);
  s.replace("@TOKEN@",token);
  s.replace("@R9@",String(cfg.r9/100.0,2)); s.replace("@R6@",String(cfg.r6/100.0,2));
  s.replace("@AP@",escapeAttr(cfg.ap)); s.replace("@STA@",escapeAttr(cfg.sta));
  http.send(200,"text/html; charset=utf-8",s);
}

void live() {
  if(!authorized()) return;
  String s=String("{\"ok\":")+(connectedFPGA?"true":"false")+",\"ip\":\"";
  s+=(WiFi.status()==WL_CONNECTED?WiFi.localIP().toString():String("hors ligne"));
  s+="\",\"rpm\":[";
  for(int i=0;i<4;i++) {
    if(i) s+=',';
    // 60 s / (2 impulsions par tour x tick de 8 µs).
    s+=String(periods[i]?(3750000UL+periods[i]/2)/periods[i]:0UL);
  }
  http.sendHeader("Cache-Control","no-store");
  http.send(200,"application/json",s+"]}");
}

bool ratioValue(const String &s,uint16_t &result) {
  if(s.length()<1 || s.length()>4 || s[0]<'1' || s[0]>'8') return false;
  result=(s[0]-'0')*100;
  if(s.length()==1) return true;
  if(s[1]!='.' || s.length()==2) return false;
  for(unsigned i=2;i<s.length();i++) {
    if(s[i]<'0'||s[i]>'9') return false;
    result+=(s[i]-'0')*(i==2?10:1);
  }
  return result<=800;
}

void configure() {
  if(!authorized()) return;
  if(http.arg("token")!=token){http.send(403,"text/plain","Session invalide");return;}
  Settings next=cfg;
  String ap=http.arg("ap"), sta=http.arg("sta"), key=http.arg("key"), pass=http.arg("pass");
  if(!http.hasArg("sta") || !ap.length() || ap.length()>32 || sta.length()>32 ||
     (key.length() && (key.length()<8 || key.length()>63)) ||
     (pass.length() && (pass.length()<8 || pass.length()>63)) ||
     !ratioValue(http.arg("r9"),next.r9) || !ratioValue(http.arg("r6"),next.r6)) {
    http.send(400,"text/plain; charset=utf-8","Paramètres invalides");return;
  }
  ap.toCharArray(next.ap,sizeof(next.ap)); sta.toCharArray(next.sta,sizeof(next.sta));
  if(key.length()) key.toCharArray(next.key,sizeof(next.key));
  if(pass.length()) pass.toCharArray(next.pass,sizeof(next.pass));
  if(!sta.length() || http.hasArg("open")) next.pass[0]=0;
  if(!save(next)){http.send(500,"text/plain","Erreur de stockage LittleFS");return;}
  bool changed=strcmp(cfg.ap,next.ap)||strcmp(cfg.sta,next.sta)||
               strcmp(cfg.key,next.key)||strcmp(cfg.pass,next.pass);
  cfg=next;
  exchange();
  if(changed){pendingWifi=true;wifiAt=millis()+1200;}
  http.sendHeader("Location","/"); http.send(303,"text/plain","");
}

void setup() {
  Serial.begin(115200);
  digitalWrite(CS,HIGH); pinMode(CS,OUTPUT);
  pinMode(SCK_PIN,OUTPUT); digitalWrite(SCK_PIN,LOW);
  pinMode(MOSI_PIN,OUTPUT); pinMode(MISO_PIN,INPUT);
  loadSettings();
  exchange();
  network();
  token=String(os_random(),HEX)+String(os_random(),HEX);
  http.on("/",HTTP_GET,page); http.on("/live",HTTP_GET,live);
  http.on("/save",HTTP_POST,configure); http.begin();
}
void loop() {
  if(millis()-lastPoll>=250){lastPoll=millis();exchange();}
  if(pendingWifi && int32_t(millis()-wifiAt)>=0){pendingWifi=false;network();}
  http.handleClient();
}

