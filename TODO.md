# 📋 Feuille de Route & Spécifications Métier — Sprite Studio

Ce document détaille la planification des fonctionnalités métier de **Sprite Studio**.  
L'objectif est d'élever l'application d'un simple outil de découpe technique au rang d'**atelier complet de préparation, retouche et séquençage de sprites 2D pour le jeu vidéo et le pixel art**.

---

## 🗺️ Vue d'Ensemble des Chantiers Métier

| ID | Chantier | Priorité | Complexité | Statut |
|---|---|---|---|---|
| **M0** | [Assainissement Architectural & Dette Technique (Audit Critique)](#m0--assainissement-architectural--dette-technique-audit-critique) | **Haute** | Haute | 🟡 En cours (Pts 1 & 2 validés & testés) |
| **M1** | [Édition Interactive des Bounding Boxes (Atlas Slicing)](#m1--édition-interactive-des-bounding-boxes-atlas-slicing) | **Haute** | Moyenne | 🟢 ~95% - Déblocages clavier/UX validés |
| **M2** | [Gestionnaire Complet d'Animations & Timeline](#m2--gestionnaire-complet-danimations--timeline) | **Haute** | Moyenne | 📝 Planifié |
| **M3** | [Points d'Ancrage & Pivots (Origins & Offsets)](#m3--points-dancrage--pivots-origins--offsets) | **Moyenne** | Faible | 📝 Planifié |
| **M4** | [Outil d'Édition de Pixels (Pixel Art Retouching)](#m4--outil-dédition-de-pixels-pixel-art-retouching) | **Moyenne** | Haute | 📝 Planifié |
| **M5** | [Format de Projet Natif (`.sps` - Sprite Studio Project)](#m5--format-de-projet-natif-sps---sprite-studio-project) | **Haute** | Faible | 📝 Planifié |
| **M6** | [Algorithme d'Empaquetage Avancé (MaxRects Bin-Packing)](#m6--algorithme-dempaquetage-avancé-maxrects-bin-packing) | **Basse** | Moyenne | 📝 Planifié |

---

## M0 : Assainissement Architectural & Dette Technique (Audit Critique)

### Contexte & Constat d'Audit
L'application souffre d'une transition inachevée entre un code impératif legacy et une architecture orientée document. Pour garantir la maintenabilité des futurs modules (timeline M2, éditeur pixel M4), les faiblesses structurelles suivantes doivent être traitées :

### 1. Dualité des Modèles de Données (`SpriteDocument` vs `Extractor`) — ✅ TERMINÉ
- **État :** ✅ **Réfracté & Validé par tests unitaires**
- **Réalisations :**
  - Élimination complète de l'état interne dans `Extractor` (`m_frames`, `m_atlas`, `m_atlas_index`, `m_animationsData` supprimés).
  - Suppression intégrale de la synchronisation bidirectionnelle fragile (`syncToDocument()` / `syncFromDocument()` obsolète).
  - `SpriteDocument` est désormais l'**unique source de vérité** pour toute l'application.
  - Standardisation du contrat de codec d'I/O pur :
    - `read(filePath, outDoc, &error)`
    - `write(filePath, inDoc, options, &error)`
    - `canDecode(filePath)`
    - Structure typée d'erreur `ExtractorError` (codes `FileNotFound`, `CorruptedData`, `ParsingFailed`, etc.).
  - Tous les 4 codecs refactorisés et testables en mode sans interface graphique (headless) :
    - `SpriteExtractor` (PNG, JPG, BMP)
    - `GifExtractor` (GIF animé via `QImageReader` + `AtlasPacker`)
    - `JsonExtractor` (TexturePacker & Aseprite JSON import/export)
    - `GodotExtractor` (Godot 4 `.tres` SpriteFrames import/export)
  - Suite de tests unitaires automatisée `tests/test_extractors.cpp` intégrée à CMake/CTest (10 tests, 100% succès).

### 2. Monolithe "God Object" `MainWindow` — ✅ TERMINÉ
- **État :** ✅ **Réfracté & Validé par tests unitaires (18 tests, 100% succès)**
- **Réalisations :**
  - Démantèlement complet du "God Object" `MainWindow` en 3 contrôleurs autonomes :
    - `AtlasViewController` (`include/controller/atlasviewcontroller.h` / `src/controller/atlasviewcontroller.cpp`) : gestion exclusive de `QGraphicsView`, zoom/pan, dessin interactif de découpe (`ToolAddSlice`), synchronisation des `AtlasBoxItem`, sélection par boîte et marquee selection, commandes de découpe (`Trim`, `Merge`, `Delete`, `Nudge`).
    - `AnimationController` (`include/controller/animationcontroller.h` / `src/controller/animationcontroller.cpp`) : intégration du lecteur `AnimationPlayer`, arborescence `QTreeWidget`, rendu de frame sur scène d'aperçu, cadences FPS, synchronisation de l'animation active et commandes d'animation (`CreateAnimationCommand`, `ReverseAnimationCommand`, `DeleteAnimationCommand`).
    - `ProjectController` (`include/controller/projectcontroller.h` / `src/controller/projectcontroller.cpp`) : chargement/sauvegarde de fichiers via les codecs `ExtractorRegistry`, gestion persistante de l'historique des fichiers récents (`QSettings`), suppression automatique d'arrière-plan de l'atlas.
  - Transformation de `MainWindow` en orchestrateur léger reliant les signaux/slots des contrôleurs et déléguant l'ensemble de la logique métier (taille réduite de 72%, suppression des membres monolithiques).
  - Résolution du segfault lors de la sélection au lasso / marquee :
    - Détection et coupure de la récursion infinie de signaux entre `AtlasViewController::selectionChanged` et `AnimationController::updateCurrentAnimation` / `selectAnimation("current")`.
    - Optimisation de la sélection par glissement : mise à jour visuelle légère des contours en cours de drag, application effective du document au relâchement de la souris (`endMarqueeSelection`).
    - Suppression du rafraîchissement destructif de colonnes (`m_treeWidget->clear()` et `resizeColumnToContents` en boucle qui surchargeaient le heap via `QTextEngine::itemize` / `RtlAllocateHeap`).
  - Suite de tests unitaires dédiée `tests/test_controllers.cpp` (18 tests couvrant les 3 contrôleurs en mode headless/offscreen, la sélection rectangulaire avec modificateurs Ctrl/Shift et la non-récursion des signaux croisés, 100% succès sous CTest).

### 3. Gestion Mémoire & Pratiques Modernes C++17
- **Problème :** Omniprésence de pointeurs bruts nus (`new`/`delete` manuels sur `extractor`), absence de smart pointers (`std::unique_ptr`), et gestion hasardeuse du cycle de vie des objets graphiques de scène (risque de pointeurs pendants après `scene->clear()`).
- **Solution Cible :**
  - Généraliser `std::unique_ptr` pour tous les objets possédés.
  - Sécuriser le graphe de scène Qt en respectant strictement la hiérarchie parent-enfant et l'ownership de `QGraphicsScene`.
  - Éliminer les constantes magiques disséminées (`margin = 16.0`, `tolerance = 10`, `minSize = 3.0`) au profit d'un fichier de constantes ou de configuration typé.

### 4. Performance & Traitement d'Images sur le Thread Principal
- **Problème :** Le flood-fill de `SpriteExtractor` et la suppression d'arrière-plan par balayage de pixels s'exécutent de façon synchrone sur le thread UI via des appels lents à `QImage::pixel(x, y)` et une pile `QStack<QPoint>`. Sur de grands atlas (2K/4K), l'interface freeze totalement.
- **Solution Cible :**
  - Remplacer les accès `pixel(x, y)` par des accès directs en mémoire contiguë (`scanLine()` / `constScanLine()`).
  - Déporter les algorithmes d'extraction lourds sur un thread travailleur (`QThread` ou `QtConcurrent`) avec barre de progression non bloquante.
  - Optimiser la pile Undo/Redo (`DeleteFramesCommand`) pour éviter de dupliquer des textures `QPixmap` entières en RAM.

### 5. Standardisation de l'Ergonomie & Internationalisation (i18n)
- **Problème :**
  - Le zoom molette utilise un `resetTransform()` brutal qui recentre la vue au lieu de zoomer sous le pointeur de la souris.
  - L'i18n est chaotique : clés opaques avec underscores (`tr("_file_error")`), libellés bilingues codés en dur (`tr("Trim to Pixels / Ajuster aux pixels")`), mélange anglais/français dans l'interface.
  - Boutons d'outils textuels bruts sans icônes vectorielles cohérentes avec le reste de l'UI.
- **Solution Cible :**
  - Implémenter un zoom interactif centré sur la position de la souris dans le viewport.
  - Normaliser toutes les chaînes en anglais propre et déléguer la traduction au système standard Qt Linguist (`.ts`).
  - Intégrer un jeu d'icônes homogène pour la barre d'outils de slicing.

### 6. DevOps, Tests Automatisés & Qualité
- **Problème :** Couverture de test à 0%. Aucun test unitaire automatique pour les algorithmes critiques (`computeTrimmedRect`, `AtlasPacker`, commandes Undo/Redo). Aucune intégration continue (CI).
- **Solution Cible :**
  - Mettre en place un projet de test unitaire via `QTest` / `CTest` dans CMake.
  - Ajouter un workflow GitHub Actions multi-plateforme (Ubuntu, Windows, macOS) validant le build à chaque commit/PR.

---

## M1 : Édition Interactive des Bounding Boxes (Atlas Slicing)

### Contexte & Objectif
La détection automatique par seuil alpha ou tolérance de couleur est efficace pour des planches simples, mais montre ses limites sur des sprites découpés en plusieurs morceaux disjoints (ex. un projectile séparé du personnage, des effets de particules, des membres détachés).  
L'utilisateur doit pouvoir ajuster visuellement et manuellement les boîtes de découpe directement sur la vue de l'atlas.

### Spécifications Fonctionnelles Initiales
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

### 📊 Point d'Étape & Bilan de Conformité (Avancement : ~95%)

| Spécification M1 | Statut | Composant / Fichier | Diagnostic & Observations |
|---|:---:|---|---|
| **Poignées de redimensionnement (8 Handles)** | ✅ Fait | `atlasboxitem.h` / `.cpp` | 8 poignées fonctionnelles avec curseurs directionnels. |
| **Déplacement souris (Drag & Drop)** | ✅ Fait | `atlasboxitem.cpp` | Fonctionne avec contrainte aux bornes de l'atlas. |
| **Déplacement clavier (Flèches, Shift)** | ✅ **RÉSOLU** | `mainwindow.cpp` / `mainwindow_events.cpp` | Priorisation établie : le stepping de l'animation cède le pas dès qu'une boîte est sélectionnée pour permettre le déplacement fin au pixel. |
| **Création manuelle (Outil Add Slice)** | ✅ **RÉSOLU** | `mainwindow_events.cpp` | Rebasculement automatique sur `ToolSelect` et sélection de la nouvelle tranche dès libération de la souris. |
| **Génération instantanée de la frame** | ✅ Fait | `spritedocument.cpp`, `mainwindow_atlas.cpp` | La frame est créée directement dans `SpriteDocument` avec notification par signaux. |
| **Trim to Pixels (Shrink to Alpha)** | ✅ Fait | `spritedocument.cpp`, `mainwindow_atlas.cpp` | Calcul de boîte englobante opaque opérationnel (mono et multi-sélection). |
| **Merge Slices (Fusion)** | ✅ Fait | `mainwindow_atlas.cpp`, `commands.cpp` | Opérationnel via clic droit (si ≥ 2 boîtes). Utilise `MergeFramesCommand`. |
| **Suppression (Touche Suppr)** | ✅ **RÉSOLU** | `mainwindow.cpp` | Raccourci `Delete` globalisé sur `MainWindow` pour supprimer la boîte active directement depuis l'atlas. |
| **Intégration Undo / Redo** | ✅ Fait | `commands.h` / `commands.cpp` | Toutes les modifications géométriques, fusions et suppressions passent par `QUndoStack`. |

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

1. **Étape 0 — Stabilisation & Clôture de M1 (M1-Fix)** :
   Corriger immédiatement les conflits de raccourcis (flèches et touche Suppr), le workflow du mode Slice et le bug visuel des poignées avant d'entamer de nouveaux chantiers.
2. **Étape 1 — Sauvegarde & Projet Natif (M5)** :
   Sécuriser le travail de l'utilisateur dès le départ en lui permettant de sauvegarder et recharger son document complet (`.sps`), évitant toute perte de données lors des crashs ou fermetures.
3. **Étape 2 — Séquençage & Multi-Animations (M2)** :
   Donner toute la dimension "studio d'animation" avec la création d'animations multiples, le réglage de cadence et les boucles via une timeline ergonomique.
4. **Étape 3 — Points d'Ancrage / Pivots (M3)** :
   Assurer la cohérence physique des animations avant l'export dans les moteurs de jeux.
5. **Étape 4 — Assainissement Architectural & Performance (M0)** :
   Unification définitive du modèle sur `SpriteDocument`, élimination du state parallèle d'`Extractor`, multithreading des extractions et tests unitaires.
6. **Étape 5 — Outil d'Édition de Pixels (M4)** :
   Offrir l'atelier de retouche pixel art autonome directement au cœur du workflow.
7. **Étape 6 — Optimisation du Packing (M6)** :
   Perfectionner le rendement de l'atlas PNG final pour la production avec MaxRects.
