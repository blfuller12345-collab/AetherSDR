#include "RotatorApplet.h"

#include "core/RotatorClient.h"
#include "core/ThemeManager.h"

#include <QAccessible>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace AetherSDR {

namespace {

constexpr int kNudgeDeg = 10;

// ThemeManager token templates, applied with applyStyleSheet() so they follow
// the theme (same idiom and tokens as GreenHeronApplet).
const QString kButtonStyle = QStringLiteral(
    "QPushButton { background: {{color.toggle.background}}; "
    "border: 1px solid {{color.toggle.border}}; border-radius: 3px; "
    "padding: 2px 4px; font-size: 10px; font-weight: bold; "
    "color: {{color.toggle.foreground}}; }"
    "QPushButton:hover { background: {{color.background.2}}; }"
    "QPushButton:checked { "
    "background-color: {{color.toggle.success.background.checked}}; "
    "color: {{color.toggle.success.foreground.checked}}; "
    "border: 1px solid {{color.toggle.success.border.checked}}; }"
    "QPushButton:disabled { color: {{color.toggle.foreground.disabled}}; "
    "background: {{color.toggle.background.disabled}}; "
    "border: 1px solid {{color.toggle.border.disabled}}; }");

const QString kLabelStyle = QStringLiteral(
    "color: {{color.text.label}}; font-size: 10px; font-weight: bold;");

const QString kFieldStyle = QStringLiteral(
    "background: {{color.background.1}}; border: 1px solid {{color.background.1}}; "
    "border-radius: 3px; padding: 1px 3px; color: {{color.text.primary}}; "
    "font-size: 10px;");

QString textStyle(const QString& token, int px, bool bold)
{
    return QStringLiteral("color: {{%1}}; font-size: %2px;%3")
        .arg(token)
        .arg(px)
        .arg(bold ? QStringLiteral(" font-weight: bold;") : QString());
}

QPushButton* makeButton(const QString& text, const QString& accessibleName,
                        const QString& description)
{
    auto* b = new QPushButton(text);
    b->setAccessibleName(accessibleName);
    b->setAccessibleDescription(description);
    b->setFixedHeight(20);
    ThemeManager::instance().applyStyleSheet(b, kButtonStyle);
    return b;
}

}  // namespace

RotatorApplet::RotatorApplet(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("rotatorApplet"));
    m_configs = Rotator::loadConfigs();
    buildUi();
    rebuildRows();
}

RotatorApplet::~RotatorApplet() = default;

void RotatorApplet::buildUi()
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(4, 4, 4, 4);
    outer->setSpacing(4);

    m_rowsLayout = new QVBoxLayout;
    m_rowsLayout->setSpacing(4);
    outer->addLayout(m_rowsLayout);

    m_emptyLabel = new QLabel(tr("No rotators set up yet. Click Setup to add one."));
    m_emptyLabel->setWordWrap(true);
    ThemeManager::instance().applyStyleSheet(
        m_emptyLabel, textStyle(QStringLiteral("color.text.label"), 10, false));
    outer->addWidget(m_emptyLabel);

    auto* footer = new QHBoxLayout;
    footer->addStretch(1);
    m_setupBtn = makeButton(tr("Setup"), tr("Rotator setup"),
                            tr("Add, change or remove rotators: name, address, "
                               "port and protocol."));
    m_setupBtn->setObjectName(QStringLiteral("rotatorSetup"));
    m_setupBtn->setCheckable(true);
    m_setupBtn->setFixedWidth(60);
    connect(m_setupBtn, &QPushButton::toggled, this, &RotatorApplet::showSetup);
    footer->addWidget(m_setupBtn);
    outer->addLayout(footer);

    // ── Setup (hidden until asked for) ─────────────────────────────────────
    m_setupSection = new QWidget;
    m_setupSection->setObjectName(QStringLiteral("rotatorSetupSection"));
    auto* setup = new QVBoxLayout(m_setupSection);
    setup->setContentsMargins(0, 2, 0, 0);
    setup->setSpacing(3);

    auto* hint = new QLabel(tr("GS-232A is for microHAM ARCO, ERC and other "
                               "GS-232 controllers on the network (port 4001 on "
                               "an ARCO). rotctld is Hamlib's rotator server, "
                               "also offered by PstRotator (port 4533)."));
    hint->setWordWrap(true);
    ThemeManager::instance().applyStyleSheet(
        hint, textStyle(QStringLiteral("color.text.label"), 10, false));
    setup->addWidget(hint);

    m_editLayout = new QVBoxLayout;
    m_editLayout->setSpacing(3);
    setup->addLayout(m_editLayout);

    auto* setupButtons = new QHBoxLayout;
    auto* add = makeButton(tr("Add"), tr("Add a rotator"), tr("Add a row for another rotator."));
    connect(add, &QPushButton::clicked, this, [this] {
        Rotator::Config c;
        QList<Rotator::Config> existing = m_configs;
        for (const EditRow& r : m_editRows) {
            Rotator::Config taken;
            taken.id = r.id;
            existing.append(taken);
        }
        c.id = Rotator::uniqueId(existing);
        addEditRow(c);
    });
    setupButtons->addWidget(add);
    setupButtons->addStretch(1);
    auto* save = makeButton(tr("Save"), tr("Save rotator setup"),
                            tr("Save the rotators and reconnect to them."));
    connect(save, &QPushButton::clicked, this, &RotatorApplet::saveSetup);
    setupButtons->addWidget(save);
    setup->addLayout(setupButtons);

    m_setupSection->hide();
    outer->addWidget(m_setupSection);

    m_noteLabel = new QLabel;
    m_noteLabel->setWordWrap(true);
    ThemeManager::instance().applyStyleSheet(
        m_noteLabel, textStyle(QStringLiteral("color.accent.warning"), 10, false));
    m_noteLabel->hide();
    outer->addWidget(m_noteLabel);
}

void RotatorApplet::rebuildRows()
{
    for (Row& row : m_rows) {
        delete row.widget;
        delete row.client;
    }
    m_rows.clear();

    for (const Rotator::Config& config : m_configs) {
        if (!config.enabled) continue;
        Row row;
        row.client = new RotatorClient(config, this);

        row.widget = new QWidget;
        auto* v = new QVBoxLayout(row.widget);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(2);

        auto* top = new QHBoxLayout;
        auto* name = new QLabel(config.name);
        name->setToolTip(QStringLiteral("%1:%2 (%3)")
                             .arg(config.host)
                             .arg(config.port)
                             .arg(config.protocol == Rotator::Protocol::Rotctld
                                      ? QStringLiteral("rotctld")
                                      : QStringLiteral("GS-232A")));
        ThemeManager::instance().applyStyleSheet(name, kLabelStyle);
        top->addWidget(name, 1);
        row.heading = new QLabel;
        row.heading->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row.heading->setAccessibleName(tr("%1 heading").arg(config.name));
        ThemeManager::instance().applyStyleSheet(
            row.heading, textStyle(QStringLiteral("color.text.primary"), 13, true));
        top->addWidget(row.heading);
        v->addLayout(top);

        auto* controls = new QHBoxLayout;
        controls->setSpacing(3);
        row.ccw = makeButton(QStringLiteral("◀"), tr("Turn %1 counter-clockwise").arg(config.name),
                             tr("Turn %1 degrees counter-clockwise.").arg(kNudgeDeg));
        row.ccw->setFixedWidth(24);
        controls->addWidget(row.ccw);
        row.target = new QSpinBox;
        row.target->setRange(0, 359);
        row.target->setWrapping(true);
        row.target->setSuffix(QStringLiteral("°"));
        row.target->setAccessibleName(tr("Heading for %1").arg(config.name));
        row.target->setAccessibleDescription(
            tr("Degrees. Typing here only proposes a heading; Go sends it."));
        ThemeManager::instance().applyStyleSheet(
            row.target, QStringLiteral("QSpinBox { %1 }").arg(kFieldStyle));
        controls->addWidget(row.target, 1);
        row.go = makeButton(tr("Go"), tr("Turn %1").arg(config.name),
                            tr("Turn the rotator to the heading in the box."));
        row.go->setFixedWidth(34);
        controls->addWidget(row.go);
        row.cw = makeButton(QStringLiteral("▶"), tr("Turn %1 clockwise").arg(config.name),
                            tr("Turn %1 degrees clockwise.").arg(kNudgeDeg));
        row.cw->setFixedWidth(24);
        controls->addWidget(row.cw);
        row.stop = makeButton(tr("Stop"), tr("Stop %1").arg(config.name),
                              tr("Stop the rotator now."));
        row.stop->setFixedWidth(40);
        controls->addWidget(row.stop);
        v->addLayout(controls);

        m_rowsLayout->addWidget(row.widget);
        m_rows.append(row);

        RotatorClient* client = row.client;
        const int index = m_rows.size() - 1;
        auto refresh = [this, index] {
            if (index < m_rows.size()) refreshRow(m_rows[index]);
        };
        connect(client, &RotatorClient::connectionChanged, this, refresh);
        connect(client, &RotatorClient::headingChanged, this, refresh);
        connect(client, &RotatorClient::targetChanged, this, refresh);
        connect(client, &RotatorClient::errorChanged, this, refresh);
        QSpinBox* target = row.target;
        connect(row.go, &QPushButton::clicked, client,
                [client, target] { client->moveTo(target->value()); });
        connect(target, &QSpinBox::editingFinished, client, [client, target] {
            // Enter in the box is a deliberate send, like Go; leaving the box
            // without Enter is not.
            if (target->hasFocus()) client->moveTo(target->value());
        });
        connect(row.ccw, &QPushButton::clicked, client, [client] { client->nudge(-kNudgeDeg); });
        connect(row.cw, &QPushButton::clicked, client, [client] { client->nudge(kNudgeDeg); });
        connect(row.stop, &QPushButton::clicked, client, &RotatorClient::halt);

        refreshRow(m_rows.last());
        client->start();
    }
    m_emptyLabel->setVisible(m_rows.isEmpty());
}

void RotatorApplet::refreshRow(Row& row)
{
    RotatorClient* c = row.client;
    const bool up = c->isConnected();
    QString text;
    if (!up) {
        text = tr("offline");
    } else if (!c->hasHeading()) {
        text = QStringLiteral("…");
    } else if (c->target() >= 0) {
        text = QStringLiteral("%1° → %2°").arg(c->heading()).arg(c->target());
    } else {
        text = QStringLiteral("%1°").arg(c->heading());
    }
    if (row.heading->text() != text) {
        row.heading->setText(text);
        QAccessibleEvent ev(row.heading, QAccessible::NameChanged);
        QAccessible::updateAccessibility(&ev);
    }
    const QString reason = up ? QString()
                              : (c->lastError().isEmpty()
                                     ? tr("Not connected to %1:%2.")
                                           .arg(c->config().host).arg(c->config().port)
                                     : tr("Not connected: %1").arg(c->lastError()));
    row.heading->setToolTip(reason);
    // Dimmed with a stated reason, never hidden (docs/agents/gui.md).
    for (QWidget* w : {static_cast<QWidget*>(row.go), static_cast<QWidget*>(row.ccw),
                       static_cast<QWidget*>(row.cw), static_cast<QWidget*>(row.stop),
                       static_cast<QWidget*>(row.target)}) {
        w->setEnabled(up);
        w->setToolTip(reason);
        if (!up) w->setAccessibleDescription(reason);
    }
    // Nudges turn from the heading, so they wait for the first reading.
    row.ccw->setEnabled(up && c->hasHeading());
    row.cw->setEnabled(up && c->hasHeading());
}

void RotatorApplet::showSetup(bool on)
{
    if (on) {
        for (EditRow& r : m_editRows) delete r.widget;
        m_editRows.clear();
        for (const Rotator::Config& c : m_configs) addEditRow(c);
        if (m_configs.isEmpty()) {
            Rotator::Config c;
            c.id = Rotator::uniqueId(m_configs);
            addEditRow(c);
        }
    }
    m_setupSection->setVisible(on);
}

void RotatorApplet::addEditRow(const Rotator::Config& config)
{
    EditRow r;
    r.id = config.id;
    r.widget = new QWidget;
    auto* grid = new QHBoxLayout(r.widget);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(3);

    r.name = new QLineEdit(config.name);
    r.name->setPlaceholderText(tr("Name"));
    r.name->setAccessibleName(tr("Rotator name"));
    r.host = new QLineEdit(config.host);
    r.host->setPlaceholderText(tr("Address"));
    r.host->setAccessibleName(tr("Rotator address"));
    r.port = new QSpinBox;
    r.port->setRange(1, 65535);
    r.port->setValue(config.port);
    r.port->setAccessibleName(tr("Rotator port"));
    r.protocol = new QComboBox;
    r.protocol->addItem(QStringLiteral("GS-232A"), Rotator::protocolKey(Rotator::Protocol::Gs232A));
    r.protocol->addItem(QStringLiteral("rotctld"), Rotator::protocolKey(Rotator::Protocol::Rotctld));
    r.protocol->setCurrentIndex(config.protocol == Rotator::Protocol::Rotctld ? 1 : 0);
    r.protocol->setAccessibleName(tr("Rotator protocol"));
    QSpinBox* port = r.port;
    connect(r.protocol, &QComboBox::currentIndexChanged, port, [port](int i) {
        // Follow the protocol's usual port unless the operator changed it.
        const int gs = Rotator::defaultPort(Rotator::Protocol::Gs232A);
        const int rc = Rotator::defaultPort(Rotator::Protocol::Rotctld);
        if (port->value() == gs || port->value() == rc) port->setValue(i == 1 ? rc : gs);
    });
    for (QWidget* w : {static_cast<QWidget*>(r.name), static_cast<QWidget*>(r.host),
                       static_cast<QWidget*>(r.port), static_cast<QWidget*>(r.protocol)}) {
        ThemeManager::instance().applyStyleSheet(
            w, QStringLiteral("%1 { %2 }").arg(QString::fromLatin1(w->metaObject()->className()),
                                               kFieldStyle));
    }
    auto* remove = makeButton(QStringLiteral("✕"), tr("Remove this rotator"),
                              tr("Remove this rotator from the list when you save."));
    remove->setFixedWidth(22);
    grid->addWidget(r.name, 3);
    grid->addWidget(r.host, 4);
    grid->addWidget(r.port, 2);
    grid->addWidget(r.protocol, 3);
    grid->addWidget(remove);
    m_editLayout->addWidget(r.widget);
    m_editRows.append(r);

    QWidget* rowWidget = r.widget;
    connect(remove, &QPushButton::clicked, this, [this, rowWidget] {
        for (int i = 0; i < m_editRows.size(); ++i) {
            if (m_editRows.at(i).widget == rowWidget) {
                m_editRows.removeAt(i);
                break;
            }
        }
        rowWidget->deleteLater();
    });
}

void RotatorApplet::saveSetup()
{
    QList<Rotator::Config> configs;
    for (const EditRow& r : m_editRows) {
        Rotator::Config c;
        c.id = r.id;
        c.host = r.host->text().trimmed();
        if (c.host.isEmpty()) continue;
        c.name = r.name->text().trimmed().isEmpty() ? c.host : r.name->text().trimmed();
        c.port = static_cast<quint16>(r.port->value());
        c.protocol = Rotator::protocolFromKey(r.protocol->currentData().toString());
        configs.append(c);
    }
    // Round-trip through the stored form, so what runs is what was saved
    // (ids made unique, names defaulted).
    configs = Rotator::fromJson(Rotator::toJson(configs));
    Rotator::saveConfigs(configs);
    m_configs = configs;
    rebuildRows();
    m_setupBtn->setChecked(false);
    setNote(QString());
    emit rotatorsChanged();
}

void RotatorApplet::setNote(const QString& text)
{
    m_noteLabel->setText(text);
    m_noteLabel->setVisible(!text.isEmpty());
}

QList<QPair<QString, QString>> RotatorApplet::rotatorTargets() const
{
    QList<QPair<QString, QString>> out;
    for (const Row& row : m_rows) {
        out.append({row.client->config().id, row.client->config().name});
    }
    return out;
}

void RotatorApplet::turnRotator(const QString& rotatorId, int azimuthDeg)
{
    if (RotatorClient* c = client(rotatorId)) {
        if (!c->isConnected()) {
            setNote(tr("%1 is not connected, so it was not turned.").arg(c->config().name));
            return;
        }
        c->moveTo(azimuthDeg);
    }
}

RotatorClient* RotatorApplet::client(const QString& rotatorId) const
{
    for (const Row& row : m_rows) {
        if (row.client->config().id == rotatorId) return row.client;
    }
    return nullptr;
}

}  // namespace AetherSDR
