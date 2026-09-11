#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <QObject>
#include <QString>
#include <QColor>

/**
 * @brief Configuration settings for the atlas view, zooming, slicing, and nudging.
 */
struct AtlasConfig
{
    double zoomMin = 0.1;
    double zoomMax = 10.0;
    double zoomStep = 1.15;
    int    minSliceSize = 3;
    int    defaultAlphaThreshold = 1;
    int    defaultVerticalTolerance = 0;
    int    nudgeStepSmall = 1;
    int    nudgeStepLarge = 10;
    int    fitViewPadding = 20;
};

/**
 * @brief Visual styling settings (box colors, handles, marquee preview).
 */
struct VisualConfig
{
    double handleSize = 8.0;
    double handleMargin = 16.0;
    QColor selectedBoxColor = QColor(255, 200, 0);          // Gold
    QColor unselectedBoxColor = QColor(0, 180, 255, 180);    // Cyan outline
    QColor selectedBoxFillColor = QColor(0, 160, 255, 60);   // Translucent blue fill
    QColor hoveredBoxFillColor = QColor(0, 180, 255, 30);    // Light translucent blue
    QColor marqueeColor = QColor(0, 120, 215);               // Blue dashed marquee
    QColor newSlicePreviewColor = QColor(0, 220, 100);       // Green dashed slice preview
};

/**
 * @brief Configuration settings for animation playback and FPS boundaries.
 */
struct AnimationConfig
{
    int defaultFps = 12;
    int minFps = 1;
    int maxFps = 60;
    bool autoPlayOnSelection = true;
};

/**
 * @brief Configuration settings for project management, recent files, and image filters.
 */
struct ProjectConfig
{
    int maxRecentFiles = 10;
    int backgroundRemovalTolerance = 10;
    int backgroundMinAlpha = 10;
    int undoLimit = 50;
};

/**
 * @brief Central configuration manager for SpriteStudio.
 *
 * Persists and loads user and default application settings to/from a structured
 * JSON file (spritestudio_config.json). Designed with full fail-safe resilience:
 * if the JSON file is missing, corrupt, or contains invalid keys, safe defaults
 * are automatically preserved without throwing exceptions or crashing.
 */
class AppConfig : public QObject
{
    Q_OBJECT

public:
    static AppConfig& instance();

    // Accessors
    const AtlasConfig& atlas() const { return m_atlas; }
    AtlasConfig& atlas() { return m_atlas; }

    const VisualConfig& visuals() const { return m_visuals; }
    VisualConfig& visuals() { return m_visuals; }

    const AnimationConfig& animation() const { return m_animation; }
    AnimationConfig& animation() { return m_animation; }

    const ProjectConfig& project() const { return m_project; }
    ProjectConfig& project() { return m_project; }

    // File operations
    bool load(const QString &filePath = QString());
    bool save(const QString &filePath = QString()) const;
    void resetToDefaults();

    QString configFilePath() const;
    void setConfigFilePath(const QString &path);

signals:
    void configChanged();

private:
    AppConfig();
    ~AppConfig() override = default;
    Q_DISABLE_COPY(AppConfig)

    QString resolveDefaultConfigPath() const;

    AtlasConfig     m_atlas;
    VisualConfig    m_visuals;
    AnimationConfig m_animation;
    ProjectConfig   m_project;

    mutable QString m_customConfigPath;
};

#endif // APPCONFIG_H
