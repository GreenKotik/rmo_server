#ifndef HIGHLIGHTROWDELEGATE_H
#define HIGHLIGHTROWDELEGATE_H

#include <QStyledItemDelegate>
#include <QColor>

// Forward declarations — чтобы не тянуть тяжёлые include в этот header
class MeasurementsModel;
class JournalProxy;
class QSortFilterProxyModel;

// ============================================================================
// ✅ ДЕЛЕГАТ ДЛЯ ПОДСВЕТКИ ТЕКУЩЕЙ СТРОКИ
// Работает с любой QAbstractItemView. Не трогает модель — только рисует.
// ============================================================================
class HighlightRowDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    // ✅ Общий цвет подсветки текущей строки (светло-жёлтый).
    // Используется и в HighlightRowDelegate, и в JournalHeightDelegate.
    static QColor defaultHighlightColor() { return QColor(255, 250, 200); }

    explicit HighlightRowDelegate(QObject *parent = nullptr);

    void setSpecialColumnName(const QString &name) { m_specialColumnName = name; }
    void setSpecialColumnColor(const QColor &color) { m_specialColor = color; }

    void setHighlightColor(const QColor &color) { m_highlightColor = color; }
    QColor highlightColor() const { return m_highlightColor; }

    // ✅ Новое: ссылки для расчёта высоты по авариям
    void setProxyAndModel(QSortFilterProxyModel *proxy, MeasurementsModel *model) {
        m_proxy = proxy;
        m_model = model;
    }

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;   // ← новое

private:
    QString m_specialColumnName;
    QColor m_specialColor = QColor("#E8F0F8");
    QColor m_highlightColor = defaultHighlightColor();  // ← было 255,235,59

    QSortFilterProxyModel *m_proxy = nullptr;   // ← новое
    MeasurementsModel     *m_model = nullptr;   // ← новое

    int m_maxHeight = 200;                      // ← новое
    int m_linePadding = 0;
    int m_verticalPadding = 2;
};

// ============================================================================
// ✅ ДЕЛЕГАТ АВТОВЫСОТЫ СТРОК ЖУРНАЛА АВАРИЙ
//
// Логика:
//   height = baseHeight * max(1, errorCount)
// где errorCount = MeasurementsModel::rowErrorCount(row).
// Ограничение сверху — maxHeight (по умолчанию 5 строк).
//
// Работает только с JournalProxy, потому что индексы строк в прокси и
// в источнике (модели) не совпадают — нужен mapToSource.
// ============================================================================
class JournalHeightDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit JournalHeightDelegate(JournalProxy *proxy,
                                   MeasurementsModel *model,
                                   int maxHeight = 200,
                                   QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
        , m_proxy(proxy)
        , m_model(model)
        , m_maxHeight(maxHeight)
    {}

    // ✅ Имя столбца, который подсвечивается голубовато-серым
    void setSpecialColumnName(const QString &name) { m_specialColumnName = name; }
    void setSpecialColumnColor(const QColor &color) { m_specialColor = color; }

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

private:
    JournalProxy     *m_proxy = nullptr;
    MeasurementsModel *m_model = nullptr;
    int m_maxHeight = 200;
    int m_linePadding = 0;
    int m_verticalPadding = 2;
    int m_horizontalPadding = 6;

    QString m_specialColumnName;              // ← новое
    QColor  m_specialColor = QColor("#E8F0F8"); // ← новое
};

#endif // HIGHLIGHTROWDELEGATE_H
