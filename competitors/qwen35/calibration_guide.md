# 📐 Guide de Calibration des Ratios

## 🎯 Objectif

Ce guide vous aide à déterminer les ratios optimaux pour vos ventilateurs Noctua NF-A9-flx et NF-A6x25 flx afin d'obtenir un RPM simulé qui correspond aux attentes de l'onduleur Deye.

---

## 📊 Comprendre les RPM des Ventilateurs Noctua

### NF-A9-flx (9cm)

| Vitesse | RPM Réel | Notes |
|---------|----------|-------|
| Min | ~1000-1200 | Mode silencieux |
| Moyen | ~1500-1800 | Équilibre bruit/performance |
| Max | ~2000-2200 | Pleine puissance |

### NF-A6x25 flx (6cm)

| Vitesse | RPM Réel | Notes |
|---------|----------|-------|
| Min | ~800-1000 | Mode silencieux |
| Moyen | ~1200-1400 | Équilibre bruit/performance |
| Max | ~1600-1800 | Pleine puissance |

---

## 🔧 Méthode de Calibration

### Étape 1: Mesurer les RPM Réels

1. **Obtenez un tachymètre** (application smartphone ou outil physique)
2. **Mesurez les RPM** de chaque ventilateur à différentes vitesses
3. **Notez les valeurs** dans un tableau

### Étape 2: Déterminer le RPM Simulé Désiré

L'onduleur Deye s'attend généralement à des RPM dans la plage:
- **Minimum:** 1500-2000 RPM
- **Maximum:** 7000 RPM (pour les ventilateurs rapides)

### Étape 3: Calculer le Ratio

```
Ratio = RPM_simulé_désiré / RPM_réel_mesuré
```

#### Exemple 1: Ventilateur 9cm à pleine vitesse

```
RPM réel mesuré: 2000
RPM simulé désiré: 4000
Ratio = 4000 / 2000 = 2.0
```

#### Exemple 2: Ventilateur 6cm à pleine vitesse

```
RPM réel mesuré: 1600
RPM simulé désiré: 4000
Ratio = 4000 / 1600 = 2.5
```

---

## 📋 Tableau de Calibration Rapide

### Ventilateur 9cm (NF-A9-flx)

| RPM Réel | Ratio 2.0 | Ratio 2.5 | Ratio 3.0 | Ratio 3.5 | Ratio 4.0 |
|----------|-----------|-----------|-----------|-----------|-----------|
| 1000 | 2000 | 2500 | 3000 | 3500 | 4000 |
| 1200 | 2400 | 3000 | 3600 | 4200 | 4800 |
| 1500 | 3000 | 3750 | 4500 | 5250 | 6000 |
| 1800 | 3600 | 4500 | 5400 | 6300 | 7200 |
| 2000 | 4000 | 5000 | 6000 | 7000 | 8000 |

### Ventilateur 6cm (NF-A6x25 flx)

| RPM Réel | Ratio 1.5 | Ratio 2.0 | Ratio 2.5 | Ratio 3.0 | Ratio 4.0 |
|----------|-----------|-----------|-----------|-----------|-----------|
| 800 | 1200 | 1600 | 2000 | 2400 | 3200 |
| 1000 | 1500 | 2000 | 2500 | 3000 | 4000 |
| 1200 | 1800 | 2400 | 3000 | 3600 | 4800 |
| 1400 | 2100 | 2800 | 3500 | 4200 | 5600 |
| 1600 | 2400 | 3200 | 4000 | 4800 | 6400 |

---

## 🎨 Scénarios d'Utilisation

### Scénario 1: Refroidissement Standard

**Objectif:** RPM simulé stable autour de 3000-4000

```
Ventilateur 9cm:
  - RPM réel: ~1500 (vitesse moyenne)
  - Ratio recommandé: 2.0-2.5
  
Ventilateur 6cm:
  - RPM réel: ~1200 (vitesse moyenne)
  - Ratio recommandé: 2.5
```

**Résultat:**
```
RPM simulé 9cm: 1500 × 2.0 = 3000
RPM simulé 6cm: 1200 × 2.5 = 3000
RPM total: (3000 + 3000) / 2 = 3000
```

### Scénario 2: Refroidissement Intensif

**Objectif:** RPM simulé élevé pour maximiser le flux d'air

```
Ventilateur 9cm:
  - RPM réel: ~2000 (pleine vitesse)
  - Ratio recommandé: 2.0-2.5
  
Ventilateur 6cm:
  - RPM réel: ~1600 (pleine vitesse)
  - Ratio recommandé: 2.5-3.0
```

**Résultat:**
```
RPM simulé 9cm: 2000 × 2.5 = 5000
RPM simulé 6cm: 1600 × 2.5 = 4000
RPM total: (5000 + 4000) / 2 = 4500
```

### Scénario 3: Mode Économique

**Objectif:** RPM simulé bas pour réduire le bruit

```
Ventilateur 9cm:
  - RPM réel: ~1000 (vitesse minimale)
  - Ratio recommandé: 2.0-2.5
  
Ventilateur 6cm:
  - RPM réel: ~800 (vitesse minimale)
  - Ratio recommandé: 2.5
```

**Résultat:**
```
RPM simulé 9cm: 1000 × 2.0 = 2000
RPM simulé 6cm: 800 × 2.5 = 2000
RPM total: (2000 + 2000) / 2 = 2000
```

---

## 🔬 Calibration Avancée

### Méthode de Calibration Progressive

1. **Commencez avec les ratios par défaut:**
   - 9cm: 2.0
   - 6cm: 2.5

2. **Mesurez le RPM simulé total** dans l'interface web

3. **Ajustez progressivement:**
   - Augmentez un ratio à la fois
   - Observez l'impact sur le RPM total
   - Arrêtez-vous quand vous atteignez la plage désirée

4. **Testez à différentes vitesses** des ventilateurs

5. **Enregistrez les ratios optimaux** pour chaque usage

### Ajustement Fin

Si le RPM simulé est:
- **Trop élevé:** Réduisez le ratio
- **Trop bas:** Augmentez le ratio
- **Instable:** Vérifiez les connexions et le lissage

---

## 📐 Calculs Personnalisés

### Formule Générale

```
RPM_simulé_total = (RPM_9cm × Ratio_9cm + RPM_6cm × Ratio_6cm) / 2
```

### Exemple de Calcul

```
Données:
  - RPM_9cm = 1500
  - RPM_6cm = 1200
  - Ratio_9cm = 2.0
  - Ratio_6cm = 2.5

Calcul:
  RPM_simulé_9cm = 1500 × 2.0 = 3000
  RPM_simulé_6cm = 1200 × 2.5 = 3000
  RPM_total = (3000 + 3000) / 2 = 3000
```

---

## 🎯 Objectifs de Calibration

### Plage Recommandée

| Usage | RPM Simulé Total | Ratio 9cm | Ratio 6cm |
|-------|------------------|-----------|-----------|
| Silencieux | 1500-2000 | 2.0-2.5 | 2.5 |
| Standard | 2500-3500 | 2.0-2.5 | 2.5-3.0 |
| Performance | 4000-5000 | 2.5-3.0 | 2.5-3.0 |
| Max | 6000-7000 | 3.0-4.0 | 3.0-4.0 |

### Ajustement par Température

Vous pouvez ajuster les ratios en fonction de la température:

```
Température ambiante < 25°C:
  - Ratio 9cm: +0.1 (plus de refroidissement)
  - Ratio 6cm: +0.1

Température ambiante > 30°C:
  - Ratio 9cm: -0.1 (réduire le bruit)
  - Ratio 6cm: -0.1
```

---

## 📝 Checklist de Calibration

- [ ] Mesurer les RPM réels des deux ventilateurs
- [ ] Déterminer la plage de RPM simulé désirée
- [ ] Calculer les ratios initiaux
- [ ] Appliquer les ratios dans l'interface web
- [ ] Observer le comportement à différentes vitesses
- [ ] Ajuster les ratios si nécessaire
- [ ] Tester avec l'onduleur Deye
- [ ] Enregistrer les ratios optimaux

---

## 💡 Conseils

### Pour un Meilleur Résultat

1. **Utilisez des ventilateurs de la même marque** pour une réponse cohérente
2. **Calibrez à pleine vitesse** d'abord, puis ajustez pour les vitesses intermédiaires
3. **Testez avec l'onduleur réel** pour valider le comportement
4. **Enregistrez vos configurations** pour chaque usage

### Éviter les Problèmes

- ❌ Ne pas utiliser de ratios trop différents entre les canaux
- ❌ Ne pas dépasser 4.67 comme ratio maximum
- ❌ Ne pas changer les ratios pendant que l'onduleur est en fonctionnement
- ✅ Changer les ratios lorsque l'onduleur est éteint

---

## 🔄 Mise à Jour des Ratios

### Via l'Interface Web

1. Ouvrez l'interface web
2. Modifiez le ratio dans le champ correspondant
3. La modification est appliquée immédiatement (pas de rechargement)
4. Observez les RPM simulés se mettre à jour

### Via le Firmware

Pour une mise à jour permanente:

```cpp
// Dans firmware_main.cpp, modifier les valeurs par défaut
#define RATIO_9CM_DEFAULT  2.0f
#define RATIO_6CM_DEFAULT  2.5f
```

---

## 📊 Suivi des Performances

### Tableau de Suivi

| Date | Usage | RPM 9cm | Ratio 9cm | RPM 6cm | Ratio 6cm | RPM Total | Notes |
|------|-------|---------|-----------|---------|-----------|-----------|-------|
|      |       |         |           |         |           |           |       |

### Analyse des Données

- **RPM moyen:** Pour évaluer la performance globale
- **Variations:** Pour détecter les problèmes de ventilation
- **Tendances:** Pour optimiser le refroidissement

---

## 🎓 Conclusion

La calibration des ratios est un processus itératif:

1. **Mesurez** les RPM réels
2. **Calculez** les ratios initiaux
3. **Testez** avec l'onduleur
4. **Ajustez** si nécessaire
5. **Enregistrez** la configuration optimale

N'hésitez pas à expérimenter pour trouver le compromis optimal entre:
- Refroidissement efficace
- Niveau de bruit acceptable
- Compatibilité avec l'onduleur Deye

---

## 📞 Besoin d'Aide?

Si vous rencontrez des difficultés:
- Consultez la documentation principale
- Vérifiez les exemples de calibration
- Contactez le développeur pour assistance personnalisée
