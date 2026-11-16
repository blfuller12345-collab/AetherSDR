#include "gui/workspace/WorkspaceExchange.h"

#include "gui/workspace/WorkspaceGeometry.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <algorithm>

namespace AetherSDR {

namespace WorkspaceExchange {

namespace {

const QString kAppletPrefix = QStringLiteral("applet:");

// Copy of a workspace safe to leave this machine: window geometry hints are
// per-machine (and per-monitor), so they stay home.
Workspace portable(const Workspace& ws)
{
    Workspace out = ws;
    for (WorkspaceSurface& surf : out.surfaces) {
        surf.windowGeometry.clear();
    }
    return out;
}

int labelIndex(const WorkspaceDocument& doc, const QString& label)
{
    for (int i = 0; i < doc.workspaces.size(); ++i) {
        if (doc.workspaces.at(i).label == label) {
            return i;
        }
    }
    return -1;
}

}  // namespace

QByteArray exportWorkspaces(const WorkspaceDocument& source,
                            const QStringList& workspaceIds,
                            const QString& writerVersion,
                            const QString& exportedAtUtc)
{
    WorkspaceDocument set;
    for (const Workspace& ws : source.workspaces) {
        if (workspaceIds.contains(ws.id)) {
            set.workspaces.append(portable(ws));
        }
    }
    if (set.workspaces.isEmpty()) {
        return {};
    }
    for (auto it = source.bindings.constBegin(); it != source.bindings.constEnd(); ++it) {
        if (set.contains(it.value())) {
            set.bindings.insert(it.key(), it.value());
        }
    }
    set.activeWorkspace = set.workspaces.first().id;
    set.canvasEnabled = false;   // a per-install choice, not part of a layout

    QJsonObject root;
    root[QStringLiteral("format")] = kFormatName;
    root[QStringLiteral("formatVersion")] = kFormatVersion;
    root[QStringLiteral("writerVersion")] = writerVersion;
    root[QStringLiteral("exportedAt")] = exportedAtUtc;
    root[QStringLiteral("document")] = set.toJson();
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

QString suggestedFileName(const QString& workspaceLabel)
{
    QString base = workspaceLabel.trimmed();
    base.replace(QRegularExpression(QStringLiteral(R"([\\/:*?"<>|\x00-\x1f])")),
                 QStringLiteral("_"));
    base.replace(QRegularExpression(QStringLiteral(R"(\s+)")), QStringLiteral("-"));
    if (base.isEmpty()) {
        base = QStringLiteral("Workspace");
    }
    return base + QLatin1Char('.') + kFileSuffix;
}

bool parse(const QByteArray& bytes, WorkspaceDocument* out,
           QString* writerVersion, QString* error, QStringList* warnings)
{
    auto fail = [error](const QString& why) {
        if (error) *error = why;
        return false;
    };
    QJsonParseError jsonError{};
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &jsonError);
    if (jsonError.error != QJsonParseError::NoError || !doc.isObject()) {
        return fail(QStringLiteral("the file is not valid JSON"));
    }
    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("format")).toString() != kFormatName) {
        return fail(QStringLiteral("the file is not an AetherSDR workspace export"));
    }
    const QJsonValue version = root.value(QStringLiteral("formatVersion"));
    if (!version.isDouble()) {
        return fail(QStringLiteral("the file has no format version"));
    }
    if (version.toInt() > kFormatVersion) {
        return fail(QStringLiteral("the file was written by a newer AetherSDR "
                                   "(format %1); this version reads format %2")
                        .arg(version.toInt())
                        .arg(kFormatVersion));
    }
    if (!root.value(QStringLiteral("document")).isObject()) {
        return fail(QStringLiteral("the file holds no workspaces"));
    }
    WorkspaceDocument parsed;
    QString why;
    if (!WorkspaceDocument::fromJson(root.value(QStringLiteral("document")).toObject(),
                                     &parsed, &why, warnings)) {
        return fail(why.isEmpty() ? QStringLiteral("the workspaces could not be read")
                                  : why);
    }
    if (parsed.workspaces.isEmpty()) {
        return fail(QStringLiteral("the file holds no workspaces"));
    }
    if (writerVersion) {
        *writerVersion = root.value(QStringLiteral("writerVersion")).toString();
    }
    *out = parsed;
    return true;
}

MergeReport merge(WorkspaceDocument& target, const WorkspaceDocument& incoming,
                  const MergeOptions& options)
{
    MergeReport report;
    for (const Workspace& source : incoming.workspaces) {
        Workspace ws = portable(source);

        // Fold extra canvas windows into main (single-monitor target). The
        // document's own removeSurface() is the one rule for where items go.
        if (options.foldExtraSurfaces && ws.surfaces.size() > 1) {
            WorkspaceDocument scratch;
            scratch.workspaces.append(ws);
            QStringList extra;
            for (const WorkspaceSurface& s : ws.surfaces) {
                if (s.id != WorkspaceSurface::kMainId) extra.append(s.id);
            }
            for (const QString& sid : extra) {
                if (scratch.removeSurface(ws.id, sid)) ++report.foldedSurfaces;
            }
            ws = scratch.workspaces.first();
        }

        // Applets this install doesn't have stay out; everything else, and
        // every rect, is kept, pulled onto its surface if it hangs off it.
        for (WorkspaceSurface& surf : ws.surfaces) {
            QList<CanvasItem> kept;
            for (CanvasItem item : surf.items) {
                if (!options.knownAppletIds.isEmpty()
                    && item.id.startsWith(kAppletPrefix)
                    && !options.knownAppletIds.contains(item.id.mid(kAppletPrefix.size()))) {
                    report.droppedApplets.append(
                        QStringLiteral("%1: %2").arg(ws.label, item.id.mid(kAppletPrefix.size())));
                    continue;
                }
                item.rect = clampToBounds(item.rect);
                kept.append(item);
            }
            // Dense z again after any drop or fold, stacking order kept.
            std::stable_sort(kept.begin(), kept.end(),
                             [](const CanvasItem& a, const CanvasItem& b) {
                                 return a.z < b.z;
                             });
            for (int z = 0; z < kept.size(); ++z) {
                kept[z].z = z;
            }
            surf.items = kept;
        }

        // Name conflicts: never overwrite unless told to.
        QString targetId;
        const int existing = labelIndex(target, ws.label);
        Conflict choice = Conflict::Rename;
        if (existing >= 0 && options.onConflict) {
            choice = options.onConflict(ws.label);
        }
        if (existing >= 0 && choice == Conflict::Skip) {
            report.skipped.append(ws.label);
            continue;
        }
        if (existing >= 0 && choice == Conflict::Replace) {
            targetId = target.workspaces.at(existing).id;
            ws.id = targetId;
            target.workspaces[existing] = ws;
            report.replaced.append(ws.label);
        } else {
            targetId = target.uniqueWorkspaceId();
            ws.id = targetId;
            ws.label = target.uniqueLabel(ws.label);
            target.workspaces.append(ws);
            report.added.append(ws.label);
        }

        // Bindings that pointed at this workspace in the file.
        for (auto it = incoming.bindings.constBegin(); it != incoming.bindings.constEnd(); ++it) {
            if (it.value() != source.id) {
                continue;
            }
            const QString& profile = it.key();
            if (!options.knownProfiles.contains(profile)) {
                report.unboundProfiles.append(profile);
            } else if (target.bindings.contains(profile)
                       && target.bindings.value(profile) != targetId) {
                report.keptBindings.append(profile);
            } else {
                target.bindings.insert(profile, targetId);
            }
        }
    }
    return report;
}

}  // namespace WorkspaceExchange

}  // namespace AetherSDR
