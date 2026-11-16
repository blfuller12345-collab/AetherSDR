#pragma once

// The ROT applet: one row per configured rotator (GS-232A over TCP, e.g.
// microHAM ARCO, or Hamlib rotctld), showing its live heading, with a typed
// heading + Go, 10-degree nudges and Stop. Setup lives in the tile (like the
// Green Heron applet), so there is no separate window to find.
//
// A rotator only ever turns from an explicit Go, nudge or menu action — never
// from a click on a readout, never on connect.

#include "core/RotatorProtocol.h"

#include <QList>
#include <QPair>
#include <QString>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QVBoxLayout;

namespace AetherSDR {

class RotatorClient;

class RotatorApplet : public QWidget {
    Q_OBJECT

public:
    explicit RotatorApplet(QWidget* parent = nullptr);
    ~RotatorApplet() override;

    // (id, name) of each enabled rotator, in the operator's order: the
    // targets "Turn beam here" offers on a spot.
    QList<QPair<QString, QString>> rotatorTargets() const;

    // Turn one rotator (spot menu). No-op when it is not connected.
    void turnRotator(const QString& rotatorId, int azimuthDeg);

    // For tests and the automation bridge.
    RotatorClient* client(const QString& rotatorId) const;

signals:
    // The configured rotators changed (setup saved).
    void rotatorsChanged();

private:
    struct Row {
        RotatorClient* client{nullptr};
        QWidget*     widget{nullptr};
        QLabel*      heading{nullptr};
        QSpinBox*    target{nullptr};
        QPushButton* go{nullptr};
        QPushButton* ccw{nullptr};
        QPushButton* cw{nullptr};
        QPushButton* stop{nullptr};
    };
    struct EditRow {
        QString      id;
        QWidget*     widget{nullptr};
        QLineEdit*   name{nullptr};
        QLineEdit*   host{nullptr};
        QSpinBox*    port{nullptr};
        QComboBox*   protocol{nullptr};
    };

    void buildUi();
    void rebuildRows();
    void refreshRow(Row& row);
    void showSetup(bool on);
    void addEditRow(const Rotator::Config& config);
    void saveSetup();
    void setNote(const QString& text);

    QList<Rotator::Config> m_configs;
    QList<Row> m_rows;
    QList<EditRow> m_editRows;

    QVBoxLayout* m_rowsLayout{nullptr};
    QWidget*     m_setupSection{nullptr};
    QVBoxLayout* m_editLayout{nullptr};
    QPushButton* m_setupBtn{nullptr};
    QLabel*      m_emptyLabel{nullptr};
    QLabel*      m_noteLabel{nullptr};
};

}  // namespace AetherSDR
