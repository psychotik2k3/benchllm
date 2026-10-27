"""Génère le schéma électronique complet (SVG + PNG) avec schemdraw.

Usage : python generer_schema.py   (nécessite : pip install schemdraw matplotlib)
Les fichiers sont écrits dans le dossier parent : schema_complet.svg / .png
Les liaisons entre blocs se font par étiquettes de net (drapeaux bleus) :
deux drapeaux portant le même nom sont reliés électriquement.
"""
import os
import schemdraw
import schemdraw.elements as elm

schemdraw.use("matplotlib")
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")

P = elm.IcPin
GREY = "#666666"
NET = "#0b5394"


def tag(at, text, left=False):
    t = elm.Tag(width=0.2 * len(text) + 0.6).at(at).label(text, fontsize=9, color=NET)
    return t.left() if left else t.right()


def gnd(at):
    return elm.Ground().at(at)


def vdd(at, text="3V3"):
    return elm.Vdd().at(at).label(text, fontsize=9)


def text(at, s, size=8, color="black", halign="left"):
    return elm.Label().at(at).label(s, fontsize=size, color=color, halign=halign)


def deye_and_fans(d, x, y, cm, model, rail, tach_deye, tach_noc):
    """Connecteur ventilateur de l'onduleur + 2 Noctua alimentés en parallèle."""
    d += text((x - 1.2, y + 2.8), f"Connecteur ventilateur {cm} de l'onduleur + 2 x Noctua {model} en parallèle", 11)
    j = elm.Ic(pins=[P("3 TACH", side="R", anchorname="TACH"),
                     P("2 +12V", side="R", anchorname="V12"),
                     P("1 GND", side="R", anchorname="GND")],
               size=(2.8, 2.6), pinspacing=0.8, edgepadH=0.5).right().at((x, y)).anchor("center")
    j.label(f"Onduleur Deye\nprise FAN {cm}", loc="top", fontsize=9)
    d += j
    d += elm.Line().at(j.GND).right(0.8)
    d += gnd(d.here)
    d += elm.Line().at(j.V12).right(13.0)
    rail_end = d.here
    d += tag(rail_end, rail)
    d += elm.Line().at(j.TACH).right(0.6)
    d += elm.Line().down(2.2)
    d += tag(d.here, tach_deye, left=True)
    for k, dx in enumerate((5.6, 10.8)):
        fx, fy = x + dx, y - 3.4
        m = elm.Ic(pins=[P("+12V", side="T", anchorname="V12"),
                         P("GND", side="B", anchorname="GND"),
                         P("RPM", side="B", anchorname="RPM")],
                   size=(3.2, 1.9), pinspacing=1.2, edgepadW=0.5).right().at((fx, fy)).anchor("center")
        m.label(f"Noctua {model}\n#{k + 1}", loc="center", fontsize=8)
        d += m
        d += elm.Line().at(m.V12).toy(j.V12[1])
        d += elm.Dot().at((m.V12[0], j.V12[1]))
        d += elm.Line().at(m.GND).down(0.5)
        d += gnd(d.here)
        d += elm.Line().at(m.RPM).down(0.7)
        if k == 0:
            d += tag(d.here, tach_noc)
        else:
            d += elm.NoConnect().at(d.here)
            d += text((m.RPM[0] + 0.3, m.RPM[1] - 1.1), "non connecté", 7, GREY)


def power(d, x, y):
    d += text((x - 1.0, y + 3.0), "Alimentation : OU à diodes des 2 connecteurs + convertisseur DC-DC 5 V", 11)
    d += tag((x, y + 1.2), "+12V_6cm", left=True)
    d += elm.Schottky().at((x, y + 1.2)).right(2.4).label("D1  1N5819", fontsize=9)
    a = d.here
    d += tag((x, y - 0.8), "+12V_9cm", left=True)
    d += elm.Schottky().at((x, y - 0.8)).right(2.4).label("D2  1N5819", loc="bottom", fontsize=9)
    b = d.here
    d += elm.Line().at(a).right(0.8).dot()
    n1 = d.here
    d += elm.Wire("-|").at(b).to(n1)
    d += elm.Fuse().at(n1).right(2.6).label("F1 PTC 0,3 A\n(option)", fontsize=9)
    d += elm.Line().right(0.6).dot()
    vin = d.here
    d += text((vin[0] - 0.6, vin[1] + 0.5), "VIN", 9)
    d += elm.Capacitor(polar=True).at(vin).down(2.0)
    d += text((vin[0] + 0.5, vin[1] - 1.0), "C1\n220 µF 25 V", 8)
    d += gnd(d.here)
    d += elm.Line().at(vin).right(3.0).dot()
    c2 = d.here
    d += elm.Capacitor().at(c2).down(2.0)
    d += text((c2[0] + 0.5, c2[1] - 1.0), "C2\n100 nF", 8)
    d += gnd(d.here)
    u = elm.Ic(pins=[P("IN-", side="L", anchorname="INN"), P("IN+", side="L", anchorname="INP"),
                     P("OUT-", side="R", anchorname="OUTN"), P("OUT+", side="R", anchorname="OUTP")],
               size=(3.0, 2.2), pinspacing=1.2, edgepadH=0.5).right().anchor("INP").at((c2[0] + 3.2, c2[1]))
    d += u
    d += text((u.INN[0] - 0.4, u.INN[1] - 1.9),
              "U1 : DC-DC abaisseur MP1584EN réglé à 5,0 V\n(ou R-78E5.0-0.5 / TSR 1-2450 si Vin >= 7 V)", 8)
    d += elm.Line().at(c2).to(u.INP)
    d += elm.Line().at(u.INN).left(0.5)
    d += elm.Line().down(0.4)
    d += gnd(d.here)
    d += elm.Line().at(u.OUTP).right(1.4).dot()
    o = d.here
    d += elm.Capacitor().at(o).down(1.6)
    d += text((o[0] + 0.4, o[1] - 0.8), "C3 10 µF", 8)
    d += gnd(d.here)
    d += elm.Line().at(o).right(1.0)
    d += tag(d.here, "+5V")
    d += elm.Line().at(u.OUTN).right(0.5)
    d += gnd(d.here)


def wemos(d, x, y):
    w = elm.Ic(pins=[P("5V", side="L", anchorname="V5"), P("G", side="L", anchorname="G"),
                     P("3V3", side="L", anchorname="V33"),
                     P("D5 / GPIO14", side="R", anchorname="D5"), P("D6 / GPIO12", side="R", anchorname="D6"),
                     P("D1 / GPIO5", side="R", anchorname="D1"), P("D2 / GPIO4", side="R", anchorname="D2"),
                     P("D4 / GPIO2", side="R", anchorname="D4")],
               size=(5.4, 5.4), pinspacing=0.9, edgepadH=0.7).right().at((x, y)).anchor("center")
    w.label("WEMOS D1 mini V2.3.0 (ESP-12S)", loc="top", fontsize=10)
    d += w
    d += text((x - 4.0, y + 4.4), "Microcontrôleur et affectation des broches", 11)
    d += elm.Line().at(w.V5).left(0.9)
    d += tag(d.here, "+5V", left=True)
    d += elm.Line().at(w.G).left(0.9)
    d += gnd(d.here)
    d += elm.Line().at(w.V33).left(0.9)
    d += tag(d.here, "3V3", left=True)
    d += text((w.V33[0] - 4.6, w.V33[1] + 0.9), "3V3 = sortie du régulateur\nintégré du Wemos (pull-ups)", 7, GREY)
    for pin, net in (("D5", "IN_9cm"), ("D6", "IN_6cm"), ("D1", "OUT_9cm"), ("D2", "OUT_6cm")):
        d += elm.Line().at(getattr(w, pin)).right(0.9)
        d += tag(d.here, net)
    d += elm.Line().at(w.D4).right(0.9)
    d += text((w.D4[0] + 1.1, w.D4[1]), "LED bleue intégrée\n(LED d'état, active à 0)", 8)


def npn(d, base_at, name, ref):
    q = elm.BjtNpn(circle=True).right().anchor("base").at(base_at)
    d += q
    d += text((base_at[0] + 1.9, base_at[1] - 0.2), f"{name}\n{ref}", 8)
    return q


def input_stage(d, x, y, cm, tach_noc, gpio_net, qname, r, gpio_label):
    """Noctua (collecteur ouvert) -> NPN inverseur -> GPIO 3,3 V."""
    d += text((x - 1.2, y + 4.6), f"Entrée tach {cm} : Noctua -> {gpio_label}", 11)
    d += tag((x, y), tach_noc, left=True)
    d += elm.Line().at((x, y)).right(0.8).dot()
    t = d.here
    d += elm.Resistor().at(t).up(2.4).label(f"{r[0]}\n4,7 kΩ", loc="bottom", fontsize=9)
    d += vdd(d.here)
    d += elm.Resistor().at(t).right(2.6).label(f"{r[1]} 10 kΩ", fontsize=9)
    d += elm.Line().right(0.5).dot()
    bnode = d.here
    d += elm.Resistor().at(bnode).down(2.2).label(f"{r[2]}\n10 kΩ", loc="bottom", fontsize=9)
    d += gnd(d.here)
    d += elm.Line().at(bnode).right(1.0)
    q = npn(d, d.here, qname, "BC547B / 2N3904")
    d += elm.Line().at(q.emitter).down(0.5)
    d += gnd(d.here)
    d += elm.Line().at(q.collector).up(0.6).dot()
    c = d.here
    d += elm.Resistor().at(c).up(2.0).label(f"{r[3]}\n10 kΩ", loc="bottom", fontsize=9)
    d += vdd(d.here)
    d += elm.Line().at(c).right(2.2)
    d += tag(d.here, gpio_net)
    d += text((x - 1.2, y - 3.6),
              "Tach bas (Noctua conduit) -> NPN bloqué -> GPIO = 1.  Option CEM : 1 à 10 nF base-masse.\n"
              "Courant tiré sur le tach Noctua : 0,7 mA (max Noctua 5 mA).", 8, GREY)


def output_stage(d, x, y, cm, gpio_net, tach_deye, qname, r):
    """GPIO -> NPN collecteur ouvert -> broche tach de l'onduleur (pull-up interne Deye)."""
    d += text((x - 1.2, y + 4.6), f"Sortie tach {cm} : GPIO -> onduleur (collecteur ouvert)", 11)
    d += tag((x, y), gpio_net, left=True)
    d += elm.Resistor().at((x, y)).right(2.6).label(f"{r[0]} 2,2 kΩ", fontsize=9)
    d += elm.Line().right(0.5).dot()
    bnode = d.here
    d += elm.Resistor().at(bnode).down(2.2).label(f"{r[1]}\n47 kΩ", loc="bottom", fontsize=9)
    d += gnd(d.here)
    d += elm.Line().at(bnode).right(1.0)
    q = npn(d, d.here, qname, "2N2222A / BC337 / S8050")
    d += elm.Line().at(q.emitter).down(0.5)
    d += gnd(d.here)
    d += elm.Line().at(q.collector).up(1.0)
    c = d.here
    d += elm.Line().at(c).right(2.2).dot()
    p = d.here
    d += elm.Line().at(p).right(2.4)
    d += tag(d.here, tach_deye)
    d += elm.Resistor().at(p).up(2.0).color(GREY).linestyle("--")
    d += text((p[0] + 0.5, p[1] + 1.2), "Rpu interne de l'onduleur\n(non fournie) vers 3,3 / 5 / 12 V", 7, GREY)
    d += text((x - 1.2, y - 3.6),
              "GPIO = 1 -> NPN saturé -> ligne tach à 0 V (~0,1 V).  GPIO = 0 -> ligne libérée.\n"
              "Fonctionne quelle que soit la tension de pull-up (VCEO >= 25 V).\n"
              "Seulement si 0 V mesuré sur TACH Deye au repos : ajouter 10 kΩ entre TACH_DEYE et +12V de la même prise.",
              8, GREY)


def build(d):
    d.config(fontsize=10, unit=2.0, lw=1.1)
    d += text((-1.5, 39.5),
              "Simulateur tach pour onduleur Deye SUN-8K-SG05LP1-EU-AM2-P : schéma complet", 15)
    d += text((-1.5, 38.6),
              "Toutes les masses (GND) sont communes. Deux drapeaux bleus de même nom = même connexion.", 9, GREY)
    deye_and_fans(d, 1.5, 33.5, "6 cm", "NF-A6x25 FLX", "+12V_6cm", "TACH_DEYE_6cm", "TACH_NOC_6cm")
    deye_and_fans(d, 1.5, 24.5, "9 cm", "NF-A9 FLX", "+12V_9cm", "TACH_DEYE_9cm", "TACH_NOC_9cm")
    power(d, 23.0, 33.3)
    wemos(d, 29.0, 23.0)
    input_stage(d, 1.5, 12.5, "9 cm", "TACH_NOC_9cm", "IN_9cm", "Q1", ("R1", "R2", "R3", "R4"), "D5 (GPIO14)")
    input_stage(d, 1.5, 3.0, "6 cm", "TACH_NOC_6cm", "IN_6cm", "Q2", ("R5", "R6", "R7", "R8"), "D6 (GPIO12)")
    output_stage(d, 23.0, 12.5, "9 cm", "OUT_9cm", "TACH_DEYE_9cm", "Q3", ("R9", "R10"))
    output_stage(d, 23.0, 3.0, "6 cm", "OUT_6cm", "TACH_DEYE_6cm", "Q4", ("R11", "R12"))


if __name__ == "__main__":
    for ext in ("svg", "png"):
        with schemdraw.Drawing(show=False) as d:
            build(d)
            d.save(os.path.join(OUT, f"schema_complet.{ext}"), dpi=130)
    print("schema_complet.svg / .png générés")
