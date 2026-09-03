# BenchLLM — Benchmark de Coding LLM IoT

Benchmark comparatif de plusieurs LLM pour la résolution d'un problème concret d'électronique embarquée IoT.

## 🎯 Objectif

Évaluer et comparer les capacités de codage firmware de différents LLM sur un même cahier des charges technique exigeant : le développement d'un **simulateur de signal tachymètre RPM** pour onduleur solaire Deye, tournant sur un **LOLIN(WEMOS) D1 mini (ESP8266)**.

## 📦 Structure du projet

```
benchllm/
├── README.md                           ← Ce fichier
├── prompt_standard_deye_fan_simulator.md  ← Prompt standard utilisé par la plupart des compétiteurs
├── prompt_deye_fan_controller_by_claude.md  ← Prompt détaillé utilisé par un seul compétiteur
├── prompt_analysis.md                  ← Prompt pour l'analyse comparative
├── competitors/                        ← Dossiers de chaque modèle LLM (compétiteur)
│   ├── claude-sonnet5-free/
│   ├── deepseek4 flash/
│   ├── gemini-free/
│   ├── gemma4/
│   ├── glm5.3/
│   ├── gpt-free/
│   ├── qwen3.6/
│   ├── qwen3.6_prompt_from_claude/
│   └── qwen35/
└── result_analysis/                    ← Fichiers d'analyse des compétiteurs
    └── ANALYSE_GLOBALE_2026-09-04.md   ← Classement & recommandations de fusion
```

## ⚙️ Promptes utilisées

### `prompt_standard_deye_fan_simulator.md` (prompt standard)

C'est le prompt de base utilisé par **tous les compétiteurs sauf un**. Il décrit le contexte du projet et les fonctionnalités attendues, mais sans détails architecturaux poussés.

> Tous les sous-dossiers dans `competitors/` utilisent ce prompt — à l'exception de :
> **`qwen3.6_prompt_from_claude/`** qui utilise un prompt différent.

### `prompt_deye_fan_controller_by_claude.md` (prompt détaillé)

Prompt beaucoup plus élaboré et technique, contenant des **directives d'architecture strictes et non négociables** sur :

- Le format exact du signal tach (collecteur ouvert, 2 impulsions/tour)
- L'étage d'entrée avec pull-up et filtrage RC
- L'étage de sortie en collecteur ouvert avec transistor NPN
- L'alimentation par OR-ing de diodes Schottky + buck externe
- La **séparation stricte** entre monde temps-réel (ISR + Timer1) et monde best-effort (WiFi / serveur web)
- La gestion de la persistance via LittleFS
- Le plan de brochage

> **`qwen3.6_prompt_from_claude/`** est le seul compétiteur à avoir reçu ce prompt détaillé.

### `prompt_analysis.md` (analyse comparative)

Utilisé pour demander à **Qwen3.6** d'analyser chaque compétiteur dans `competitors/` et de produire un fichier d'analyse détaillé dans le dossier `results-analysis/`. Ce prompt demande :

1. La lecture complète de tous les fichiers sources (`*.ino`, `*.cpp`, `*.h`) et annexes (README, SCHEMATIC, etc.)
2. Pour chaque version, une analyse catégorisée des **points positifs** (temps-réel/ISR, génération Timer1, filtres, WiFi, gestion mémoire, interface web, persistance, documentation)
3. Les **points négatifs** tagués par criticité `[critique]`, `[majeur]`, `[mineur]`
4. Une note globale /10 avec verdict
5. Un tableau récapitulatif trié par note décroissante
6. Un classement final et une recommandation de fusion des meilleures pratiques

## 🧪 Compétiteurs (modèles LLM testés)

| # | Dossier | Modèle LLM | Prompt utilisé | Score | Analyse |
|---|---------|-----------|----------------|:-----:|---------|
| 🥇 1 | `deepseek4 flash/` | DeepSeek v4 Flash | Standard | **8,8** | ✅ |
| 🥈 2 | `claude-sonnet5-free/` | Claude Sonnet 5 (free) | Standard | **8,5** | ✅ |
| 🥉 3 | `qwen3.6_prompt_from_claude/` | Qwen 3.6 | **Détaillé (Claude)** | **8,2** | ✅ |
| 4 | `glm5.3/` | GLM 5.3 | Standard | **8,0** | ✅ |
| 5 | `gpt-free/` | GPT (open) | Standard | **7,5** | ✅ |
| 6 | `qwen3.6/` | Qwen 3.6 | Standard | **7,2** | ✅ |
| 7 | `gemini-free/` | Gemini (free) | Standard | **5,3** | ✅ |
| 8 | `qwen35/` | Qwen 3.5 | Standard | **4,8** | ✅ |
| 9 | `gemma4/` | Gemma 4 | Standard | **3,2** | ✅ |

Remarques :
- **`qwen3.6_prompt_from_claude/`** utilise un prompt détaillé avec directives d'architecture strictes (permet de mesurer l'impact du niveau de détail du prompt).
- Le dossier `glm5.3/` contient un sous-dossier supplémentaire `deye_fan/` où se trouve le firmware.
- Tous les scores sont basés sur une analyse catégorisée dans 11 catégories : ISR/temps-réel, boot, génération sortie, fidélicité protocole, filtres, WiFi, gestion mémoire, interface web, persistance, documentation hardware, qualité de code.

## 📊 Résultats

Les analyses détaillées de chaque compétiteur sont stockées dans le dossier :

```
result_analysis/
├── ANALYSE_GLOBALE_2026-09-04.md    ← Classement complet & recommandations de fusion
├── analyse_claude-sonnet5-free.md   ← 8,5/10 — Timer1 TIM_LOOP, GPOS/GPOC, checksum pondéré
├── analyse_deepseek4_flash.md       ← 8,8/10 — NMI FRC1 + registres bare-metal (vainqueur)
├── analyse_gemini-free.md           ← 5,3/10 — StaticJsonDocument, LittleFS, docs manquantes
├── analyse_gemma4.md                ← 3,2/10 — Signal simulé non implémenté
├── analyse_glm5.3.md                ← 8,0/10 — Timer FRC1 + CCOUNT, buffer circulaire, zéro bug
├── analyse_gpt-free.md              ← 7,5/10 — TachChannel réutilisable, IIR correct, CCOUNT asm
├── analyse_qwen3.6.md               ← 7,2/10 — Timer1 matériel, EEPROM magic, digitalWrite ISR
├── analyse_qwen3.6_prompt_from_claude.md  ← 8,2/10 — GPOS/GPOC, triple EMA, OTA+mDNS
└── analyse_qwen35.md                ← 4,8/10 — IRAM_ATTR, Web UI fonctionnelle, GPOS indef.
```

### 🔑 Enseignements clés

1. **Le NMI (Non-Maskable Interrupt) FRC1** est la meilleure approche pour l'immunité WiFi sur ESP8266 (deepseek4 flash).
2. **Zéro allocation heap** pour le JSON (pile + `snprintf_P`) élimine la fragmentation mémoire.
3. **L'absence de filtrage temporel** (EMA/IIR) sur les RPM mesurés est un facteur de jitter critique — tous les projets < 7/10 en ont.
4. **Le watchdog software et l'authentification web** sont les seuls manquements communs à **tous** les compétiteurs, même les mieux notés.
5. La **documentation hardware** (schémas, BOM, calculs) est corrélée avec les scores élevés — aucun projet < 6/10 n'en avait.

### 🧩 Architecture idéale par fusion

Le meilleur firmware combiné proviendrait de :
- **ISR sortie** : deepseek4 flash (NMI FRC1 + registres bare-metal W1TS/W1TC)
- **Filtrage RPM** : qwen3.6_prompt_from_claude (triple EMA IIR) / glm5.3 (EMA α=0,25)
- **Mesure entrée** : glm5.3 (buffer circulaire 128 timestamps + CCOUNT)
- **Persistance** : deepseek4 flash (EEPROM + CRC8 intégral)
- **Reconnexion WiFi** : deepseek4 flash (périodique non-bloquante 20s)
- **mDNS + OTA** : glm5.3 / qwen3.6_prompt_from_claude
- **JSON HEAP-safe** : deepseek4 flash (pile statique + `snprintf_P`)
- **Web UI** : qwen3.6_prompt_from_claude (sliders AJAX + password masqué + reboot différé)
- **Structure code** : gpt-free (`TachChannel` réutilisable)

## 🏁 Conclusion

Ce benchmark permet de comparer objectivement la qualité du code firmware produit par différents LLM sur un projet IoT réel aux contraintes techniques exigeantes (temps-réel strict, isolation matérielle/logicielle, gestion WiFi/ISR). Il met également en évidence l'impact du niveau de détail et de la précision du prompt sur le résultat final.
