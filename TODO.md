# 📋 Feuille de Route & Spécifications Métier — Sprite Studio

Ce document détaille la planification des fonctionnalités métier de **Sprite Studio**.  
L'objectif est d'élever l'application d'un simple outil de découpe technique au rang d'**atelier complet de préparation, retouche et séquençage de sprites 2D pour le jeu vidéo et le pixel art**.

---

## 🗺️ Vue d'Ensemble des Chantiers Métier

| ID | Chantier | Priorité | Complexité | Statut |
|---|---|---|---|---|
| **M0** | [Assainissement Architectural & Dette Technique (Audit Critique)](#m0--assainissement-architectural--dette-technique-audit-critique) | **Haute** | Haute | 🟢 Validé (Pts 1, 2, 3, 4 & 5 validés & testés — 45 tests CTest 100%) |
| **M1** | [Édition Interactive des Bounding Boxes (Atlas Slicing)](#m1--édition-interactive-des-bounding-boxes-atlas-slicing) | **Haute** | Moyenne | 🟢 ~95% - Déblocages clavier/UX validés |
| **M2** | [Gestionnaire Complet d'Animations & Timeline](#m2--gestionnaire-complet-danimations--timeline) | **Haute** | Moyenne | 📝 Planifié |
| **M3** | [Points d'Ancrage & Pivots (Origins & Offsets)](#m3--points-dancrage--pivots-origins--offsets) | **Moyenne** | Faible | 📝 Planifié |
| **M4** | [Outil d'Édition de Pixels (Pixel Art Retouching)](#m4--outil-dédition-de-pixels-pixel-art-retouching) | **Moyenne** | Haute | 📝 Planifié |
| **M5** | [Format de Projet Natif (`.sps` - Sprite Studio Project)](#m5--format-de-projet-natif-sps---sprite-studio-project) | **Haute** | Faible | 📝 Planifié |
| **M6** | [Algorithme d'Empaquetage Avancé (MaxRects Bin-Packing)](#m6--algorithme-dempaquetage-avancé-maxrects-bin-packing) | **Basse** | Moyenne | 📝 Planifié |
| **M7** | [Suppression Avancée de Fond & Segmentation Robuste (JPEG Bruités, Anti-Halo)](#m7--suppression-avancée-darrière-plan--segmentation-robuste-planches-jpeg-bruit-anti-halo) | **Moyenne** | Moyenne | 📝 Notes & Pistes Techniques |

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

### 3. Gestion Mémoire & Pratiques Modernes C++17 — ✅ TERMINÉ
- **État :** ✅ **Réfracté & Validé par tests unitaires**
- **Réalisations :**
  - **Adoption de `std::unique_ptr` et élimination des `delete` manuels :**
    - `ExtractorRegistry` gère désormais ses instances d'extracteurs possédés via `std::vector<std::unique_ptr<Extractor>>`. Destructeur automatique et RAII garanti.
    - `MainWindow::ui` et `jsonExtractorDialog::ui` migrés vers `std::unique_ptr`, suppression intégrale des `delete ui;` manuels.
    - Hiérarchie d'ownership QObject clarifiée pour l'ensemble des sous-modèles (`ArrangementModel`, `FrameDelegate`, `AnimationPlayer`).
  - **Sécurisation du cycle de vie des objets `QGraphicsScene` :**
    - Unification du nettoyage dans `AtlasViewController::clearAtlas()` (détachement et libération explicite des items de preview `m_newSlicePreviewItem` et `m_selectionRectItem` avant l'appel à `m_scene->clear()`).
    - Destructeur `~AtlasViewController()` simplifié et sécurisé contre les doubles libérations et pointeurs pendants.
  - **Gestionnaire central de configuration maintenable (`AppConfig`) :**
    - Création de `include/config/appconfig.h` et `src/config/appconfig.cpp` produisant et chargeant un fichier JSON propre et indenté (`spritestudio_config.json`).
    - Localisation hybride : répertoire local/portable en priorité, puis chemin système standard `QStandardPaths::AppConfigLocation`.
    - Tolérance totale aux pannes (*fail-safe*) : en cas de syntaxe JSON corrompue ou de champs manquants suite à une édition manuelle par l'utilisateur, l'application ne crashe jamais et bascule automatiquement sur les valeurs par défaut saines en consignant un avertissement.
  - **Élimination complète des constantes magiques disséminées :**
    - `AtlasBoxItem` : dimensions des poignées (`handleSize`), marges de survol (`handleMargin`), taille minimale (`minSliceSize`) et palette de couleurs complète (boîtes sélectionnées, non sélectionnées, survol, badges) branchées sur `AppConfig`.
    - `AtlasViewController` : pas de zoom (`zoomStep`), bornes min/max (`zoomMin`, `zoomMax`), seuil alpha par défaut (`defaultAlphaThreshold`), pas de déplacement clavier (`nudgeStepSmall`, `nudgeStepLarge`), padding de cadrage (`fitViewPadding`) et couleurs d'aperçu lues depuis `AppConfig`.
    - `AnimationController` : cadence FPS par défaut (`defaultFps`), bornes min/max (`minFps`, `maxFps`) pilotées par `AppConfig`.
    - `ProjectController` : nombre maximum de fichiers récents (`maxRecentFiles`), tolérance de suppression d'arrière-plan (`backgroundRemovalTolerance`) et seuil alpha (`backgroundMinAlpha`) issus de `AppConfig`.
  - **Suite de tests automatisée étendue :**
    - 3 nouveaux tests unitaires dans `tests/test_controllers.cpp` (`testAppConfigDefaults`, `testAppConfigSaveAndLoad`, `testAppConfigCorruptJsonFallback`) portant la suite à 21 tests (100% de succès sous CTest).

### 4. Performance & Traitement d'Images sur le Thread Principal — ✅ TERMINÉ
- **État :** ✅ **Réfracté, Accéléré & Validé par tests unitaires**
- **Réalisations :**
  - **Accès direct en mémoire contiguë (`scanLine()` / `constScanLine()`) :**
    - `SpriteExtractor` : réécriture intégrale du flood-fill et du calcul de composantes connexes. Élimination des appels `QImage::pixel(x, y)` et de `QStack<QPoint>` au profit d'un pointeur par ligne `constScanLine(y)`, indexation 1D contiguë `y * w + x` et pile plate `std::vector<Point2D>`.
    - Découpage et extraction des frames (`outFrames`) par écriture directe en mémoire de scanline (`frame.scanLine(ly)`), supprimant les boucles imbriquées lentes.
    - `ProjectController::removeBackgroundFromImage` : passage à `constScanLine(y)`, échantillonnage par pas de 2 avec table de hachage $O(1)$ `QHash<QRgb, int>` et suppression directe en mémoire ligne par ligne (`scanLine(y)`).
    - **Résultat de benchmark :** Découpage de 64 composantes sur une image 512x512 exécuté en **7 ms** seulement !
  - **Déportation des traitements lourds en asynchrone (`QtConcurrent` / `QFutureWatcher`) :**
    - Ajout de `openFileAsync(filePath)` et `removeAtlasBackgroundAndRefreshAsync(...)` dans `ProjectController`.
    - Exécution du décodage d'image, du flood-fill, de la segmentation et de la suppression de fond sur un thread de travail en arrière-plan sans bloquer l'interface utilisateur.
    - Câblage non bloquant dans `MainWindow` : mise en place de curseurs d'attente dynamiques (`Qt::WaitCursor`), barre de progression réactive, notifications d'état et reconversion sécurisée des frames `QImage` en textures `QPixmap` uniquement sur le thread GUI principal lors de `onAsyncJobFinished()`.
  - **Optimisation mémoire de la pile Undo/Redo (`QUndoStack`) :**
    - Ajout du paramètre configurable `undoLimit` (50 par défaut) dans `AppConfig` (`projectConfig`).
    - Migration des structures de sauvegarde de commandes (`DeleteFramesCommand`, `MergeFramesCommand`) : remplacement des `QPixmap` par des `QImage` brutes, éliminant les fuites de descripteurs GDI/GPU en RAM lors des opérations d'annulation/rétablissement répétées.
  - **Découplage architectural de la vision par ordinateur (`SpriteDetector`) :**
    - Extraction intégrale de la détection de silhouettes et de composantes connexes hors de `SpriteExtractor` vers un moteur autonome `SpriteDetector` (`include/image/spritedetector.h` / `src/image/spritedetector.cpp`).
    - Respect strict du principe de responsabilité unique (SRP) : les `Extractor` redeviennent des codecs de formats de fichiers purs. `SpriteDetector` est directement utilisable par `ProjectController`, le futur éditeur de pixels ou tout codec sans couplage artificiel.
  - **Nettoyage d'atlas : Effacement destructif de pixels (`Shift + Suppr`) :**
    - Création de `EraseAtlasPixelsCommand` (`include/commands/commands.h`) : permet d'effacer les pixels de l'atlas sous les rectangles sélectionnés (remplissage à `alpha = 0`) tout en supprimant les tranches associées, avec support complet de l'annulation (`Ctrl+Z`) et du rétablissement (`Ctrl+Y`).
    - Intégration du raccourci clavier `Shift + Delete` et d'une action dédiée dans le menu contextuel clic-droit de l'atlas (*Erase Pixels from Atlas*).
  - **Fluidification du lecteur d'animation & Auto-play :**
    - **Maintien de la lecture active** : la modification de la sélection de frames n'interrompt plus brutalement la lecture en cours si le player tournait déjà.
    - **Auto-play configurable** : ajout de `autoPlayOnSelection` dans `AppConfig` (`AnimationConfig`). Dès qu'au moins 2 frames sont sélectionnées, le player démarre automatiquement la boucle. Sélectionner 1 seule frame affiche cette frame en pause.
    - **Raccourci universel `Espace`** : la barre d'espace bascule `Play / Pause` depuis n'importe où dans la fenêtre principale sans conflit avec les champs textuels.
  - **Suite de tests automatisée étendue :**
    - Nouveaux tests dans `tests/test_extractors.cpp` (`testExtractToImagesEquivalence`, `testExtractPerformance`, `testSpriteDetectorBasics`).
    - Nouveaux tests dans `tests/test_controllers.cpp` (`testProjectControllerOpenAsync`, `testProjectControllerRemoveBgAsync`, `testUndoStackLimitAndImageStorage`, `testAnimationControllerAutoPlay`, `testAtlasViewControllerErasePixels`, `testAtlasViewControllerMultiSelectAndDelete`).
    - Total de **43 tests unitaires individuels** sous CTest avec **100% de réussite**.

### 5. Standardisation de l'Ergonomie & Internationalisation (i18n) — ✅ TERMINÉ
- **État :** ✅ **Réfracté, Standardisé & Validé par tests unitaires**
- **Réalisations :**
  - **Zoom interactif centré sur la souris (`zoomAt`) :**
    - Suppression intégrale de `resetTransform()` qui réinitialisait la position de la vue au centre à chaque coup de molette.
    - Implémentation de `AtlasViewController::zoomAt(viewportPos, step)` : calcul précis du point de scène sous le pointeur (`mapToScene`), mise à l'échelle continue et compensation immédiate des barres de défilement (`horizontalScrollBar`, `verticalScrollBar`).
    - Déplacement et zoom parfaitement fluides, stabilité au pixel près validée par test unitaire automatisé (`diff = QPointF(0, 0)`).
  - **Standardisation stricte de l'internationalisation sous le format `KEY_...` :**
    - Remplacement de tous les libellés codés en dur, des chaînes bilingues (`Trim to Pixels / Ajuster aux pixels`) et des anciennes clés à underscores (`_file_error`) par une nomenclature claire en majuscules :
      - Menus & Actions : `KEY_MENU_FILE`, `KEY_ACTION_OPEN`, `KEY_ACTION_SAVE`, `KEY_ACTION_EXPORT`, `KEY_ACTION_EXIT`, `KEY_ACTION_UNDO`, `KEY_ACTION_REDO`, `KEY_ACTION_REMOVE_BG`...
      - Outils de slicing : `KEY_TOOL_SELECT`, `KEY_TOOL_ADD_SLICE`, `KEY_TOOL_TRIM`, `KEY_TOOL_REMOVE_BG` et leurs infobulles `KEY_TOOLTIP_...`.
      - Menus contextuels atlas & animation : `KEY_CTX_CREATE_ANIM`, `KEY_CTX_REVERSE_ANIM`, `KEY_CTX_DELETE_ANIM`, `KEY_CTX_TRIM_SLICE`, `KEY_CTX_MERGE_SLICES`, `KEY_CTX_DELETE_FRAMES`, `KEY_CTX_ERASE_PIXELS`, `KEY_CTX_REMOVE_BG`, `KEY_CTX_INVERT_SEL`.
      - Messages & dialogues : `KEY_DIALOG_OPEN_TITLE`, `KEY_DIALOG_ABOUT_TITLE`, `KEY_MSG_LOAD_ERROR`, `KEY_MSG_SAVE_ERROR`, `KEY_STATUS_READY`, `KEY_LABEL_TIMING`...
    - **Visibilité immédiate des manques :** Si une traduction est omise dans les fichiers `.ts`/`.qm`, la clé brute `KEY_...` s'affiche directement dans l'interface graphique, rendant toute régression ou oubli immédiatement détectable visuellement.
    - Synchronisation et traduction intégrale (100%) des catalogues linguistiques `sprite_studio_fr_FR.ts`, `sprite_studio_en_US.ts` et `sprite_studio_ja_JA.ts`.
    - Fallback automatique dans `main.cpp` vers la langue anglaise `sprite_studio_en_US` si la locale système de l'utilisateur n'est pas prise en charge.
  - **Intégration d'icônes modernes sur la barre d'outils de découpe :**
    - Ajout de 4 icônes PNG nettes 24x24 (`icons/tool_select.png`, `icons/tool_slice.png`, `icons/tool_trim.png`, `icons/tool_remove_bg.png`) compilées dans le fichier de ressource `images` (`:/drawer/...`).
    - Présentation visuelle soignée avec icônes aux côtés du texte (`Qt::ToolButtonTextBesideIcon`).
  - **Perspectives ergonomiques (Listes des Sprites et Animations) :**
    - Les chantiers d'ergonomie avancée sur les listes (redimensionnement dynamique des vignettes par curseur/Ctrl+Molette, badges animés, timeline filmstrip et drag-and-drop fluide) sont consignés pour le chantier d'enrichissement de l'animation **M2**.
  - **Suite de tests automatisée étendue :**
    - Deux nouveaux tests unitaires dans `tests/test_controllers.cpp` : `testAtlasViewControllerMouseCenteredZoom` (vérification de la stabilité de la position sous le curseur) et `testI18nKeyTranslations` (vérification du chargement et du comportement des dictionnaires FR, EN et du fallback sur les clés non traduites).
    - Suite de tests globale portée à **45 tests unitaires individuels** sous CTest avec **100% de succès**.

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

## M7 : Suppression Avancée d'Arrière-Plan & Segmentation Robuste (Planches JPEG, Bruit, Anti-Halo)

### Contexte & Problématique Observée (Exemple : Planche Street Fighter / Ryu)
Les planches de sprites récupérées sur le Web (rips d'émulateurs, archives) sont très fréquemment stockées au format **JPEG** :
- Absence totale de couche alpha native ($\alpha = 255$ partout).
- Compression à perte (DCT $8\times 8$ et sous-échantillonnage chromatique YUV 4:2:0).
- Fond aplat uniforme en théorie (souvent vert `#3b7b0a`, cyan ou magenta), mais fortement altéré et bruité en pratique autour des personnages.
- Sprites agencés de façon extrêmement compacte (espacement de 1 à 2 pixels seulement entre deux frames consécutives).
- Présence d'éléments parasites non graphiques : annotations textuelles de rippers ("Ryu ripped by..."), flèches explicatives, encadrés de texte, notes d'animation.
- Poses variées avec cavités corporelles complexes (jambes écartées lors des sauts, bras repliés) et effets spéciaux d'énergie (Hadouken, auras) dont les dégradés semi-transparents ont été fusionnés avec la couleur du fond.

---

### ⚠️ Inventaire des Problèmes et Risques d'Échec

1. **Artefacts de Compression JPEG & Bruit de Contour (Ringing / Mosquito Noise) :**
   - Aux abords des silhouettes à fort contraste (kimono blanc, cheveux noirs, bandeau rouge sur fond vert), la transformée en cosinus discrète (DCT) produit des ondulations de teinte. Le vert de fond fluctue localement de $\pm 15$ à $\pm 30$ en valeurs RVB.
   - **Conséquence :** Un seuil de tolérance trop bas laisse un "nuage de moustiques" de pixels verts flottant autour des sprites. Un seuil trop élevé commence à grignoter les pixels clairs ou colorés du personnage.

2. **Halo Résiduel & Frange de Transition (Color Spill / Green Fringe) :**
   - En raison de l'interpolation bilinéaire et du sous-échantillonnage chroma (4:2:0), les pixels à la frontière exacte du sprite sont un mélange optique de la couleur du trait du sprite et de la couleur du fond vert.
   - **Conséquence :** Une fois le fond supprimé au seuil strict, chaque sprite conserve un liseré verdâtre disgracieux (*green halo*) qui gâche le rendu dès qu'on place le sprite sur un fond sombre ou dans un moteur de jeu.

3. **Sur-fusion des Sprites Resserrés (Over-merging) :**
   - Entre deux frames d'animation très proches (ex: un coup de pied qui frôle la pose suivante à 1 pixel d'écart), le moindre pixel de bruit résiduel non éliminé sert de "pont" conducteur pour le flood-fill.
   - **Conséquence :** Deux ou trois frames distinctes se retrouvent agglutinées en une seule boîte englobante géante.

4. **Le Dilemme des Cavités Internes Closes (Holes & Enclosed Background Islands) :**
   - Les trous d'arrière-plan situés à l'intérieur du corps (triangle entre les jambes écartées lors d'un saut, espace sous l'aisselle, boucle d'un bras replié) posent un dilemme algorithmique :
     - *Inondation depuis l'extérieur (Flood-fill pur) :* Elle isole parfaitement la silhouette externe sans toucher au sprite, mais laisse tous les trous intérieurs remplis de vert opaque.
     - *Substitution globale de couleur (Color Replacement global) :* Elle vide correctement les trous intérieurs, mais risque de percer des trous dans le sprite si le personnage porte un vêtement ou un accessoire de teinte voisine du fond (ex: Blanka, gants, liserés).

5. **Pollution par les Micro-Composantes Textuelles (Stray Text & Credits) :**
   - Les crédits de ripping et flèches disséminés entre les rangées de sprites sont découpés en dizaines de micro-boîtes parasites ($2\times 3$ px, $5\times 5$ px), polluant la liste des frames et faussant les calculs de cadence ou d'alignement.

6. **Dégradation des Effets Semi-Transparents (Hadouken, Projectiles, Auras) :**
   - Les flammes et boules d'énergie bleues avec transparence d'origine ont été aplaties sur le vert lors de l'enregistrement JPEG, créant des pixels cyan/verts hybrides impossibles à isoler par un seuil binaire.

---

### 💡 Pistes Techniques & Solutions Envisagées

1. **Détection Colorimétrique Évoluée (Espace Perceptuel CIELAB / $\Delta E$) :**
   - Abandonner la simple distance Manhattan RVB ($|R_1-R_2| + |G_1-G_2| + |B_1-B_2|$) au profit de la distance euclidienne $\Delta E$ dans l'espace **CIELAB** ou en décomposition **YCbCr**.
   - En séparant la luminance ($Y/L$) de la chrominance ($Cb, Cr / a, b$), on peut appliquer une tolérance étroite sur la teinte du fond tout en autorisant les variations de luminosité induites par les blocs JPEG.
   - **Échantillonnage statistique :** Échantillonner les 4 coins et le périmètre extérieur pour calculer la médiane de la couleur de fond ainsi que son écart-type ($\sigma$), permettant de définir un seuil adaptatif automatique.

2. **Algorithme Hybride en 2 Passes (Silhouette Externe + Cavités Validées) :**
   - **Passe 1 (Masquage Extérieur) :** Flood-fill depuis les bords de l'image pour marquer tout l'arrière-plan externe continu sans jamais pénétrer dans le sprite.
   - **Passe 2 (Cavités Internes) :** Pour les îlots internes non connectés à l'extérieur :
     - Calculer la compacité et la proximité colorimétrique avec le fond extérieur.
     - Remplacer par la transparence uniquement si la couleur moyenne de l'îlot concorde avec le fond à $\Delta E < \text{seuil}$, ou proposer un mode interactif "clic pour déboucher la cavité".

3. **Traitement Anti-Halo / Dé-frangeage (Color Despill & Alpha Matte) :**
   - **Algorithme de Green Despill :** Sur les pixels de contour (bordure de transition de 1 pixel), calculer la proportion de vert parasite et la soustraire en ajustant la composante alpha (technique similaire au chromakey vidéo professionnel).
   - **Érosion morphologique optionnelle :** Permettre un rognage d'un demi-pixel ou 1 pixel sur le masque alpha pour éradiquer les franges bruitées tenaces.

4. **Filtrage Intelligent des Parasites & Débruitage Géométrique (Pruning) :**
   - **Seuils dimensionnels minimaux :** Ignorer automatiquement lors de la segmentation toutes les composantes connexes dont $\text{largeur} < \text{seuilMin}$ OU $\text{hauteur} < \text{seuilMin}$ (ex. $< 8$ px) ou surface $< 32\text{ px}^2$.
   - **Outil "Zone d'Exclusion / Masque Rectangulaire" :** Permettre à l'utilisateur de tracer un ou plusieurs rectangles rouges "Ignorer cette zone" sur l'atlas (ex. par-dessus le bloc de texte de crédits) avant de lancer la détection automatique.

5. **Désagglomération par Profils de Projection (Histogram Slicing / Watershed) :**
   - Calculer les histogrammes de projection de densité de pixels opaques selon les axes horizontaux (lignes) et verticaux (colonnes).
   - Détecter les "cols" et vallées étroites où deux sprites ne se touchent que par 1 ou 2 pixels aberrants pour couper automatiquement le lien et séparer les boîtes englobantes.

6. **Pipette Manuelle & Prévisualisation en Direct (Live Overlay) :**
   - Ajouter un outil pipette dans la barre d'outils pour sélectionner manuellement la couleur de fond sur l'atlas en cas de couleur non majoritaire.
   - Prévisualisation instantanée par damier de transparence ou masque binaire dynamique avec curseur de tolérance en direct avant d'appliquer définitivement la transformation sur le document.

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
8. **Étape 7 — Suppression Avancée de Fond & Débruitage Robuste (M7)** :
   Doter SpriteStudio d'un moteur de segmentation tolérant au bruit JPEG, anti-halo (*despill*), filtrage de textes parasites et désagglomération pour les planches de sprites complexes.
