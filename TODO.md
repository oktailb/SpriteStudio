# 📋 Feuille de Route & Spécifications Métier — Sprite Studio

Ce document détaille la planification des fonctionnalités métier de **Sprite Studio**.  
L'objectif est d'élever l'application d'un simple outil de découpe technique au rang d'**atelier complet de préparation, retouche et séquençage de sprites 2D pour le jeu vidéo et le pixel art**.

---

## 🗺️ Vue d'Ensemble des Chantiers Métier

| ID | Chantier | Priorité | Complexité | Statut |
|---|---|---|---|---|
| **M1** | [Édition Interactive des Bounding Boxes (Atlas Slicing)](#m1--édition-interactive-des-bounding-boxes-atlas-slicing) | **Haute** | Moyenne | 📝 Planifié |
| **M2** | [Gestionnaire Complet d'Animations & Timeline](#m2--gestionnaire-complet-danimations--timeline) | **Haute** | Moyenne | 📝 Planifié |
| **M3** | [Points d'Ancrage & Pivots (Origins & Offsets)](#m3--points-dancrage--pivots-origins--offsets) | **Moyenne** | Faible | 📝 Planifié |
| **M4** | [Outil d'Édition de Pixels (Pixel Art Retouching)](#m4--outil-dédition-de-pixels-pixel-art-retouching) | **Moyenne** | Haute | 📝 Planifié |
| **M5** | [Format de Projet Natif (`.sps` - Sprite Studio Project)](#m5--format-de-projet-natif-sps---sprite-studio-project) | **Haute** | Faible | 📝 Planifié |
| **M6** | [Algorithme d'Empaquetage Avancé (MaxRects Bin-Packing)](#m6--algorithme-dempaquetage-avancé-maxrects-bin-packing) | **Basse** | Moyenne | 📝 Planifié |

---

## M1 : Édition Interactive des Bounding Boxes (Atlas Slicing)

### Contexte & Objectif
La détection automatique par seuil alpha ou tolérance de couleur est efficace pour des planches simples, mais montre ses limites sur des sprites découpés en plusieurs morceaux disjoints (ex. un projectile séparé du personnage, des effets de particules, des membres détachés).  
L'utilisateur doit pouvoir ajuster visuellement et manuellement les boîtes de découpe directement sur la vue de l'atlas.

### Spécifications Fonctionnelles
1. **Interaction Directe sur la Vue Atlas (`QGraphicsScene`) :**
   - **Poignées de redimensionnement (Handles) :** Chaque boîte sélectionnée affiche 8 poignées (coins et milieux des côtés) avec changement dynamique du curseur (`Qt::SizeHorCursor`, `Qt::SizeVerCursor`, `Qt::SizeFDiagCursor`, `Qt::SizeBDiagCursor`).
   - **Déplacement à la souris :** Glisser-déposer une boîte pour corriger son positionnement.
   - **Déplacement au clavier :** Déplacement fin au pixel près via les flèches directionnelles (`Shift + Flèches` pour un pas de 10 px).
2. **Création Manuelle de Boîte :**
   - Outil *Rectangle / New Slice* dans la barre d'outils : cliquer-glisser pour tracer une nouvelle boîte de découpe sur l'atlas.
   - Génération instantanée de la frame correspondante dans la liste des frames.
3. **Opérations Contextuelles (Menu Clic Droit & Raccourcis) :**
   - **Ajuster aux pixels (*Trim / Shrink to Alpha*) :** Réduire automatiquement le rectangle sélectionné au bounding box exact des pixels non transparents contenus à l'intérieur.
   - **Fusionner les boîtes sélectionnées (*Merge Slices*) :** Englober plusieurs rectangles en une seule boîte englobante commune.
   - **Supprimer (*Delete*) :** Supprimer la boîte et la frame associée (touche `Suppr`).
4. **Intégration Undo / Redo :**
   - Chaque déplacement, redimensionnement ou création doit passer par `QUndoCommand` pour permettre l'annulation (`Ctrl+Z`).

### Fichiers & Composants Cibles
- `SpriteStudio/include/atlasboxitem.h` / `src/atlasboxitem.cpp` : Objet graphique dérivé de `QGraphicsRectItem` avec gestion des poignées et des événements souris.
- `SpriteStudio/src/mainwindow_atlas.cpp` : Branchement avec les signaux de `QGraphicsScene` et la synchronisation du `SpriteDocument`.

---

## M2 : Gestionnaire Complet d'Animations & Rôle de `animationList`

### Contexte & Rôle Actuel de `animationList` dans `animationArea`
Dans l'interface actuelle, le bloc de droite `animationArea` combine :
1. La vue de prévisualisation (`graphicsViewResult`) avec les boutons de lecture (`Play`, `Pause`), le champ `FPS` et un indicateur de timing.
2. Des contrôles temporels (`sliderFrom`, `timeFrom`, `timeTo`) initialement pensés pour le scrubbing temporel.
3. Le widget `animationList` (`QTreeWidget`), situé en bas de `animationArea`, avec 3 colonnes : `Name`, `FPS`, `Frames`.

**Rôle actuel de `animationList` :**
- Sert de sélecteur d'animation active pour le lecteur : cliquer sur un item déclenche immédiatement la lecture de l'animation correspondante.
- Héberge l'animation spéciale pseudo-dynamique `"current"` (qui reflète à la volée la sélection active dans la liste centrale des frames `frameList`).
- Dispose d'un menu contextuel (clic droit) permettant de supprimer, renommer ou inverser l'ordre des frames d'une animation.

### Limites Identifiées & Axes d'Évolution de `animationList`
1. **Affichage textuel brut des frames :**  
   La colonne `Frames` affiche une chaîne de texte séparée par des virgules (`1, 2, 3, 4, 5...`), ce qui devient illisible dès qu'une animation dépasse une dizaine de frames et n'offre aucune interaction visuelle.
2. **Ambiguïté entre sélection temporaire et animation sauvegardée :**  
   L'item `"current"` cohabite avec les animations réelles du projet (`idle`, `walk`), ce qui peut prêter à confusion. Il faut rendre cette distinction évidente (ex. statut visuel distinct, icône dédiée, ou bouton explicite "Créer une animation depuis la sélection").
3. **Contrôles d'actions manquants :**  
   L'ajout d'une animation dépend actuellement d'une sélection puis d'une commande indirecte ; il manque une barre d'outils compacte au-dessus ou en pied de `animationList` avec boutons d'action visibles : `+ Nouveau`, `- Supprimer`, `Dupliquer`, `Renommer`.
4. **Clarification de la zone `DataTiming` (`sliderFrom` / `timeFrom`) :**  
   Remplacer les champs `QTimeEdit` (peu adaptés aux frames de jeux vidéo) par un véritable curseur de scrubbing image par image (`Frame X / Total`, curseur de tête de lecture) connecté au player.

### Spécifications Fonctionnelles Cibles
1. **Évolution du widget `animationList` :**
   - **Barre d'actions intégrée :** Boutons iconiques `+` (nouvelle animation vide ou depuis sélection), `-` (supprimer), `Dupliquer`, `Inverser`.
   - **Édition directe (Inline) :** Double-clic sur le nom pour renommer, double-clic sur le FPS pour le modifier directement dans le tableau.
   - **Propriétés étendues par animation :**
     - Colonne Mode de boucle : `Loop` (boucle standard), `Once` (lecture unique avec arrêt sur la dernière frame), `Ping-Pong` (aller-retour).
     - Nombre total de frames et durée en millisecondes.
2. **Visualisation / Timeline de l'Animation Active :**
   - Plutôt que d'écrire les numéros en texte brut dans la colonne, le clic sur une animation affiche sous le lecteur (ou dans un volet rétractable) un **ruban de vignettes (Filmstrip)** montrant les sprites ordonnés de la séquence.
   - Glisser-déposer sur ce ruban pour réordonner, insérer ou supprimer des frames de l'animation sélectionnée.
3. **Contrôles du Lecteur :**
   - Barre de progression pas-à-pas synchrone avec la frame en cours de lecture.
   - Raccourcis clavier : `Espace` (Play/Pause), `Flèche Gauche / Droite` (Frame step).

### Fichiers & Composants Cibles
- `SpriteStudio/src/mainwindow.ui` : Ajustement du layout dans `animationArea` (barre d'outils pour `animationList`, clarification du slider).
- `SpriteStudio/include/model/spritedocument.h` : `SpriteAnimation` enrichie (`loopMode`, `fps`).
- `SpriteStudio/src/mainwindow_animation.cpp` & `src/mainwindow_callbacks.cpp` : Logique de synchronisation et d'édition de `animationList`.

---

## M3 : Points d'Ancrage & Pivots (Origins & Offsets)

### Contexte & Objectif
Lorsqu'un personnage donne un coup d'épée ou saute, la boîte de découpe de chaque frame change souvent de taille. Si les frames sont centrées arbitrairement sans point d'ancrage commun, le personnage "saute" ou glisse visuellement dans le moteur de jeu.  
Le point d'ancrage (ou pivot) définit le point de référence (souvent au niveau des pieds ou au centre du corps) pour aligner rigoureusement les frames.

### Spécifications Fonctionnelles
1. **Édition Visuelle du Pivot :**
   - Affichage d'un réticule / mire (croix colorée semi-transparente) sur la vue de la frame ou dans le lecteur d'animation.
   - Déplacement interactif à la souris du point de pivot.
   - Préréglages rapides en un clic :
     - `Bottom-Center` (standard pour personnages au sol).
     - `Center` (standard pour projectiles, vaisseaux, effets visuels).
     - `Top-Left` (standard pour éléments d'interface).
     - `Custom (X, Y)` avec champs numériques spinbox.
2. **Portée d'Application :**
   - Bouton "Appliquer à toute l'animation" ou "Appliquer à tous les sprites".
3. **Export dans les Moteurs de Jeux :**
   - **Godot 4 :** Enregistrement dans le champ d'offset ou la sous-ressource de l'AtlasTexture.
   - **JSON (TexturePacker / Aseprite) :** Calcul des champs `spriteSourceSize`, `sourceSize` et offset correspondant.

### Fichiers & Composants Cibles
- `SpriteStudio/include/model/spritedocument.h` : Ajout de `QPoint origin` dans `SpriteBox`.
- `SpriteStudio/src/extractor/godotextractor.cpp` et `jsonextractor.cpp` : Prise en compte dans la sérialisation.

---

## M4 : Outil d'Édition de Pixels (Pixel Art Retouching)

### Contexte & Objectif
Les utilisateurs perdent un temps précieux s'ils doivent rouvrir Aseprite ou Photoshop pour corriger un unique pixel oublié, enlever un artefact de compression, ou boucher un trou transparent.  
Sprite Studio doit intégrer un mini-éditeur de pixels intégré dédié à la retouche rapide de frames.

### Spécifications Fonctionnelles
1. **Espace de Travail Pixel Art :**
   - Dialogue ou dock dédié s'ouvrant sur la frame active (double-clic sur une frame).
   - Niveau de zoom élevé (de 100% à 3200%) avec affichage optionnel de la **grille de pixels (*Pixel Grid*)**.
   - Fond en damier pour visualiser la transparence.
2. **Boîte à Outils Fondamentale :**
   - **Crayon (*Pencil*) :** Dessin au pixel (taille 1px ou brosses carrées 2px, 3px).
   - **Gomme (*Eraser*) :** Efface en restaurant l'alpha à 0.
   - **Pipette (*Eyedropper*) :** Prélèvement de couleur sur la frame courante ou sur la palette.
   - **Remplissage (*Paint Bucket*) :** Remplissage par flot (Flood Fill) des pixels contigus de même couleur/alpha.
3. **Gestion de la Palette & Couleurs :**
   - Sélecteur de couleur avec canaux RGBA et code Hexadécimal.
   - Bandeau d'historique des couleurs récemment utilisées.
   - Palette auto-extraite des couleurs uniques présentes dans le sprite en cours d'édition.
4. **Synchronisation & Undo/Redo :**
   - Historique Undo/Redo dédié à l'éditeur de pixels.
   - Dès validation ou en temps réel : mise à jour de la frame dans la liste, dans l'atlas composite et dans le lecteur d'animation.

### Fichiers & Composants Cibles
- `SpriteStudio/include/pixeleditor/pixeleditordialog.h` (ou `pixeleditorwidget.h`).
- `SpriteStudio/src/pixeleditor/pixeleditorcanvas.cpp` : Canvas dérivé de `QGraphicsView` ou `QWidget` gérant le tracé pixelisé sans lissage (filtrage `Qt::FastTransformation`).

---

## M5 : Format de Projet Natif (`.sps` - Sprite Studio Project)

### Contexte & Objectif
Actuellement, si un utilisateur découpe 50 frames, crée 4 animations, règle des FPS et retire le fond, toutes ces métadonnées de montage sont perdues à la fermeture de l'application s'il n'a pas exporté dans un format compatible. De plus, les formats d'export finaux (comme Godot) ne conservent pas forcément toute la disposition d'origine.  
Un format de sauvegarde de session de travail (`.sps`) est indispensable.

### Spécifications Fonctionnelles
1. **Structure du Fichier `.sps` :**
   - Format JSON clair et lisible.
   - Contenu sauvegardé :
     - Chemin ou données relatives de l'image source / atlas.
     - Liste complète des Bounding Boxes (rectangles, indices, groupes, pivots).
     - Dictionnaire de toutes les animations créées (noms, séquences, FPS, modes de boucle).
     - Paramètres de projet (seuil alpha, tolérance verticale, stratégie de découpe).
     - Derniers réglages d'export utilisés.
2. **Intégration Menu Fichier :**
   - `Fichier -> Nouveau Projet` (`Ctrl+N`).
   - `Fichier -> Ouvrir Projet...` (`Ctrl+O`).
   - `Fichier -> Enregistrer Projet` (`Ctrl+S`).
   - `Fichier -> Enregistrer Sous...` (`Ctrl+Shift+S`).
   - Détection automatique à l'ouverture : si l'extension est `.sps`, ouvrir directement le projet.
   - Historique *Fichiers Récents* étendu aux projets `.sps`.
3. **Avertissement de modifications non enregistrées :**
   - Indicateur `*` dans la barre de titre (`Sprite Studio - MonProjet.sps *`).
   - Dialogue de confirmation à la fermeture de l'application si le projet a été modifié.

### Fichiers & Composants Cibles
- `SpriteStudio/include/project/projectmanager.h` / `src/project/projectmanager.cpp`.
- `SpriteStudio/src/mainwindow.cpp` : Routines d'ouverture, sauvegarde et détection de modification (`isWindowModified`).

---

## M6 : Algorithme d'Empaquetage Avancé (MaxRects Bin-Packing)

### Contexte & Objectif
L'exportation actuelle vers Godot ou TexturePacker utilise un placement en grille ou un packing basique. Pour minimiser l'espace mémoire vidéo (VRAM) et optimiser la taille des atlas de sprites en production, un algorithme d'empaquetage 2D de type MaxRects est requis.

### Spécifications Fonctionnelles
1. **Algorithme MaxRects (Best Short Side Fit / Best Area Fit) :**
   - Réorganisation optimale des rectangles pour produire l'atlas le plus compact possible (forme carrée ou puissance de deux : $512\times 512$, $1024\times 1024$, $2048\times 2048$).
2. **Options d'Empaquetage :**
   - **Padding / Spacing :** Espacement configurable entre les frames (ex. 1px ou 2px) pour éviter le saignement de texture (*texture bleeding*).
   - **Extrude :** Répétition des pixels de bordure sur 1 pixel pour le filtrage bilinéaire dans les moteurs 3D/2D.
   - **Deduplication :** Détection des frames strictement identiques pour ne les stocker qu'une seule fois dans l'atlas tout en conservant les références dans les animations.

### Fichiers & Composants Cibles
- `SpriteStudio/include/packer/atlaspacker.h` / `src/packer/atlaspacker.cpp`.

---

## 📅 Ordre de Déploiement Recommandé

1. **Étape 1 — Sauvegarde & Projet (M5)** :
   Sécuriser le travail de l'utilisateur dès le départ en lui permettant de sauvegarder et recharger son document complet (`.sps`).
2. **Étape 2 — Séquençage & Multi-Animations (M2)** :
   Donner toute la dimension "studio d'animation" avec la création d'animations multiples, le réglage de cadence et les boucles.
3. **Étape 3 — Points d'Ancrage / Pivots (M3)** :
   Assurer la cohérence physique des animations avant l'export dans les moteurs de jeux.
4. **Étape 4 — Édition Manuelle des Boîtes de Découpe (M1)** :
   Permettre à l'utilisateur de corriger visuellement les découpes à la souris sur l'atlas.
5. **Étape 5 — Outil d'Édition de Pixels (M4)** :
   Offrir l'atelier de retouche pixel art autonome directement au cœur du workflow.
6. **Étape 6 — Optimisation du Packing (M6)** :
   Perfectionner le rendement de l'atlas PNG final pour la production.
