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
├── prompt_analysis.md                  ← Prompt pour l'analyse comparative de Qwen3.6
├── competitors/                        ← Dossiers de chaque modèle LLM (compétiteur)
│   ├── claude-sonnet5-free/
│   ├── deepseek4-flash/
│   ├── gemini-free/
│   ├── gemma4/
│   ├── gpt-free/
│   ├── qwen3.6/
│   ├── qwen3.6_prompt_from_claude/
│   └── qwen35/
└── results-analysis/                   ← Fichiers d'analyse générés par Qwen3.6
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

| Dossier | Modèle LLM | Prompt utilisé |
|---------|-----------|----------------|
| `qwen35/` | Qwen 3.5 | Standard |
| `gemma4/` | Gemma 4 | Standard |
| `gpt-free/` | GPT (open) | Standard |
| `claude-sonnet5-free/` | Claude Sonnet 5 (free) | Standard |
| `deepseek4 flash/` | DeepSeek v4 Flash | Standard |
| `gemini-free/` | Gemini (free) | Standard |
| `qwen3.6/` | Qwen 3.6 | Standard |
| `qwen3.6_prompt_from_claude/` | Qwen 3.6 | **Détaillé (Claude)** |

Remarque : le dernier concurrent (`qwen3.6_prompt_from_claude`) permet de mesurer l'impact du niveau de détail du prompt sur la qualité du code produit par Qwen 3.6.

## 📊 Résultats

Les analyses détaillées produites par Qwen3.6 via `prompt_analysis.md` sont stockées dans le dossier :

```
results-analysis/
```

Chaque fichier contient l'analyse complète d'un compétiteur et inclut les notes, points forts/faibles, et recommandations.

## 🏁 Conclusion

Ce benchmark permet de comparer objectivement la qualité du code firmware produit par différents LLM sur un projet IoT réel aux contraintes techniques exigeantes (temps-réel strict, isolation matérielle/logicielle, gestion WiFi/ISR). Il met également en évidence l'impact du niveau de détail et de la précision du prompt sur le résultat final.
