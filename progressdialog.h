#ifndef PROGRESSDIALOG_H
#define PROGRESSDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCloseEvent>

// ============================================================================
// ✅ МОДАЛЬНЫЙ ДИАЛОГ ПРОГРЕССА С КНОПКОЙ "ОТМЕНА"
//
// Использование:
//   ProgressDialog dlg(parent);
//   dlg.setTitle("Загрузка данных...");
//   dlg.setRange(0, 100);
//   dlg.show();                       // НЕ exec() — чтобы не блокировать
//   ...
//   dlg.setValue(50);
//   dlg.setPhase("Чтение строк...");
//   QApplication::processEvents();    // раз в несколько шагов
//   ...
//   if (dlg.wasCancelled()) { ... }
// ============================================================================
class ProgressDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ProgressDialog(QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle("Загрузка");
        setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
        setModal(true);
        setMinimumWidth(420);
        setFixedHeight(140);

        // Не закрывать по Esc, не закрывать по [X] — только по кнопке "Отмена"
        setWindowFlags(windowFlags() & ~Qt::WindowCloseButtonHint);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setSpacing(12);
        mainLayout->setContentsMargins(20, 20, 20, 20);

        // Заголовок
        m_titleLabel = new QLabel("Загрузка...", this);
        m_titleLabel->setStyleSheet(
            "font-size: 13px; font-weight: bold; color: #005A9E;"
        );
        mainLayout->addWidget(m_titleLabel);

        // Текущая фаза (динамически меняется)
        m_phaseLabel = new QLabel("Подготовка...", this);
        m_phaseLabel->setStyleSheet("color: #555; font-size: 11px;");
        mainLayout->addWidget(m_phaseLabel);

        // Прогресс-бар
        m_progressBar = new QProgressBar(this);
        m_progressBar->setAlignment(Qt::AlignCenter);
        m_progressBar->setTextVisible(true);
        m_progressBar->setFormat("%v из %m (%p%)");
        QFont progressFont = m_progressBar->font();
        progressFont.setPointSize(progressFont.pointSize() + 1);
        m_progressBar->setFont(progressFont);
        mainLayout->addWidget(m_progressBar);

        // Кнопка "Отмена"
        QHBoxLayout *btnLayout = new QHBoxLayout();
        btnLayout->addStretch();
        m_cancelBtn = new QPushButton("Отмена", this);
        m_cancelBtn->setFixedSize(100, 30);
        m_cancelBtn->setStyleSheet(
            "QPushButton { padding: 6px 16px; border: 1px solid #B0B0B0; "
            "border-radius: 4px; }"
            "QPushButton:hover { background-color: #E0E0E0; }"
        );
        connect(m_cancelBtn, &QPushButton::clicked,
                this, &ProgressDialog::onCancelClicked);
        btnLayout->addWidget(m_cancelBtn);
        mainLayout->addLayout(btnLayout);
    }

    // --- Установка заголовка окна/верхней строки ---
    void setTitle(const QString &title) {
        m_titleLabel->setText(title);
    }

    // --- Установка текста текущей фазы ---
    void setPhase(const QString &phase) {
        m_phaseLabel->setText(phase);
    }

    // --- Диапазон прогресс-бара ---
    void setRange(int minimum, int maximum) {
        m_progressBar->setRange(minimum, maximum);
        // Если максимум = 0 — неопределённый режим
        m_progressBar->setTextVisible(maximum > 0);
    }

    // --- Значение прогресс-бара ---
    void setValue(int value) {
        m_progressBar->setValue(value);
    }

    // --- Формат текста прогресс-бара ---
    void setFormat(const QString &format) {
        m_progressBar->setFormat(format);
    }

    // --- Отменить? ---
    bool wasCancelled() const { return m_cancelled; }

    // --- Сбросить флаг отмены ---
    void resetCancelled() { m_cancelled = false; }

protected:
    void closeEvent(QCloseEvent *event) override {
        // Запрещаем закрытие окна пользователем (крестик/Alt+F4)
        event->ignore();
    }

    void keyPressEvent(QKeyEvent *event) override {
        // Запрещаем закрытие по Esc
        if (event->key() == Qt::Key_Escape) {
            event->ignore();
            return;
        }
        QDialog::keyPressEvent(event);
    }

private slots:
    void onCancelClicked() {
        m_cancelled = true;
        m_cancelBtn->setEnabled(false);
        m_cancelBtn->setText("Отмена...");
        m_phaseLabel->setText("Останавливаем загрузку...");
    }

private:
    QLabel       *m_titleLabel = nullptr;
    QLabel       *m_phaseLabel = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QPushButton  *m_cancelBtn = nullptr;
    bool          m_cancelled = false;
};

#endif // PROGRESSDIALOG_H
