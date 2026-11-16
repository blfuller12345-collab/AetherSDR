// Workspace export and import (#5276): the .aetherws file format and every
// merge rule, without a canvas or settings store.

#include "gui/workspace/WorkspaceExchange.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <cstdio>

using namespace AetherSDR;
namespace WX = AetherSDR::WorkspaceExchange;

namespace {

int g_failures = 0;

void report(const char* name, bool ok)
{
    std::printf("%s %s\n", ok ? "[ OK ]" : "[FAIL]", name);
    if (!ok) ++g_failures;
}

CanvasItem item(const QString& id, const NormRect& rect, int z)
{
    CanvasItem it;
    it.id = id;
    it.rect = rect;
    it.z = z;
    return it;
}

Workspace workspace(const QString& id, const QString& label, bool withExtraWindow = false)
{
    Workspace ws;
    ws.id = id;
    ws.label = label;
    WorkspaceSurface main;
    main.id = WorkspaceSurface::kMainId;
    main.items.append(item(QStringLiteral("pan:0"), NormRect{0.0, 0.0, 0.8, 1.0}, 0));
    main.items.append(item(QStringLiteral("applet:RX"), NormRect{0.8, 0.0, 0.2, 0.5}, 1));
    main.items.append(item(QStringLiteral("applet:FUTURE"), NormRect{0.8, 0.5, 0.2, 0.5}, 2));
    ws.surfaces.append(main);
    if (withExtraWindow) {
        WorkspaceSurface extra;
        extra.id = QStringLiteral("canvas2");
        extra.label = QStringLiteral("Second monitor");
        extra.windowGeometry = QByteArray("\x01\xd9\xd0\xcb machine-specific", 24);
        extra.items.append(item(QStringLiteral("applet:TX"), NormRect{0.0, 0.0, 0.5, 0.5}, 0));
        ws.surfaces.append(extra);
    }
    return ws;
}

WorkspaceDocument source()
{
    WorkspaceDocument doc;
    doc.workspaces.append(workspace(QStringLiteral("ws-1"), QStringLiteral("Classic")));
    doc.workspaces.append(workspace(QStringLiteral("ws-2"), QStringLiteral("Contest"), true));
    doc.workspaces.append(workspace(QStringLiteral("ws-3"), QStringLiteral("FT8")));
    doc.bindings.insert(QStringLiteral("CW Contest"), QStringLiteral("ws-2"));
    doc.bindings.insert(QStringLiteral("Digital"), QStringLiteral("ws-3"));
    doc.activeWorkspace = QStringLiteral("ws-1");
    doc.canvasEnabled = true;
    return doc;
}

WorkspaceDocument roundTrip(const QStringList& ids)
{
    const QByteArray bytes = WX::exportWorkspaces(source(), ids, QStringLiteral("26.11.3"),
                                                  QStringLiteral("2026-11-16T15:00:00Z"));
    WorkspaceDocument parsed;
    WX::parse(bytes, &parsed);
    return parsed;
}

const QStringList kKnownApplets{QStringLiteral("RX"), QStringLiteral("TX")};

}  // namespace

int main()
{
    // ── Export ──────────────────────────────────────────────────────────
    {
        const QByteArray bytes = WX::exportWorkspaces(
            source(), {QStringLiteral("ws-2")}, QStringLiteral("26.11.3"),
            QStringLiteral("2026-11-16T15:00:00Z"));
        const QJsonObject root = QJsonDocument::fromJson(bytes).object();
        report("export writes the format envelope",
               root.value(QStringLiteral("format")).toString() == WX::kFormatName
                   && root.value(QStringLiteral("formatVersion")).toInt() == WX::kFormatVersion
                   && root.value(QStringLiteral("writerVersion")).toString()
                          == QStringLiteral("26.11.3"));
        WorkspaceDocument parsed;
        QString writer;
        report("export parses back", WX::parse(bytes, &parsed, &writer));
        report("export holds only the chosen workspace",
               parsed.workspaces.size() == 1
                   && parsed.workspaces.first().label == QStringLiteral("Contest"));
        report("export keeps the chosen workspace's binding only",
               parsed.bindings.size() == 1
                   && parsed.bindings.value(QStringLiteral("CW Contest")) == QStringLiteral("ws-2"));
        report("export keeps extra canvas windows and their items",
               parsed.workspaces.first().surfaces.size() == 2);
        report("export drops window geometry hints",
               parsed.workspaces.first().surfaces.at(1).windowGeometry.isEmpty()
                   && !bytes.contains("machine-specific"));
        report("export leaves the canvas on/off choice out", !parsed.canvasEnabled);
        report("export of an unknown id is empty",
               WX::exportWorkspaces(source(), {QStringLiteral("nope")}, {}, {}).isEmpty());

        const QByteArray all = WX::exportWorkspaces(
            source(), {QStringLiteral("ws-1"), QStringLiteral("ws-2"), QStringLiteral("ws-3")},
            {}, {});
        WorkspaceDocument many;
        WX::parse(all, &many);
        report("several workspaces export together, in order",
               many.workspaces.size() == 3
                   && many.workspaces.at(2).label == QStringLiteral("FT8"));

        // Safe to post publicly: nothing credential-like or path-like.
        const QString text = QString::fromUtf8(all).toLower();
        bool clean = true;
        for (const char* needle : {"password", "token", "apikey", "api_key", "secret",
                                   "keychain", "smartlink", "c:\\\\", "/home/", "/users/"}) {
            clean &= !text.contains(QString::fromLatin1(needle));
        }
        report("export holds no credentials or paths", clean);
    }

    {
        report("suggested file name is safe",
               WX::suggestedFileName(QStringLiteral("CW / Contest: \"Night\""))
                   == QStringLiteral("CW-_-Contest_-_Night_.aetherws"));
        report("suggested file name for a blank label",
               WX::suggestedFileName(QStringLiteral("  ")) == QStringLiteral("Workspace.aetherws"));
    }

    // ── Parse refusals ──────────────────────────────────────────────────
    {
        WorkspaceDocument out;
        QString why;
        report("not JSON is refused", !WX::parse("not json", &out, nullptr, &why) && !why.isEmpty());
        report("another JSON file is refused",
               !WX::parse(R"({"format":"something-else","formatVersion":1})", &out));
        report("a newer format is refused",
               !WX::parse(R"({"format":"aether-workspace","formatVersion":99,"document":{}})",
                          &out, nullptr, &why)
                   && why.contains(QStringLiteral("newer")));
        report("a file with no workspaces is refused",
               !WX::parse(R"({"format":"aether-workspace","formatVersion":1})", &out));
    }

    // ── Merge ───────────────────────────────────────────────────────────
    {
        WorkspaceDocument target;
        target.workspaces.append(workspace(QStringLiteral("ws-1"), QStringLiteral("Classic")));
        target.activeWorkspace = QStringLiteral("ws-1");

        WX::MergeOptions opts;
        opts.knownAppletIds = kKnownApplets;
        opts.knownProfiles = {QStringLiteral("CW Contest")};
        const WX::MergeReport r = WX::merge(
            target, roundTrip({QStringLiteral("ws-2"), QStringLiteral("ws-3")}), opts);
        report("new workspaces are added", target.workspaces.size() == 3
                                               && r.added.size() == 2);
        report("added workspaces get fresh ids",
               target.workspaces.at(1).id != target.workspaces.at(0).id
                   && target.workspaces.at(2).id != target.workspaces.at(1).id);
        report("the active workspace is left alone",
               target.activeWorkspace == QStringLiteral("ws-1"));
        report("applets this install lacks are left out and reported",
               r.droppedApplets.contains(QStringLiteral("Contest: FUTURE"))
                   && r.droppedApplets.contains(QStringLiteral("FT8: FUTURE")));
        bool futureGone = true;
        for (const Workspace& ws : target.workspaces.mid(1)) {
            for (const WorkspaceSurface& s : ws.surfaces) {
                for (const CanvasItem& it : s.items) {
                    futureGone &= it.id != QStringLiteral("applet:FUTURE");
                }
            }
        }
        report("dropped applets are really gone", futureGone);
        report("a known profile's binding follows the workspace",
               target.bindings.value(QStringLiteral("CW Contest")) == target.workspaces.at(1).id);
        report("an unknown profile is reported, not invented",
               r.unboundProfiles == QStringList{QStringLiteral("Digital")}
                   && !target.bindings.contains(QStringLiteral("Digital")));
    }

    {
        // Name conflicts: Rename (default), Replace, Skip.
        auto base = [] {
            WorkspaceDocument t;
            t.workspaces.append(workspace(QStringLiteral("ws-7"), QStringLiteral("Contest")));
            t.workspaces.first().surfaces.first().items.removeLast();   // differ from the file
            t.bindings.insert(QStringLiteral("CW Contest"), QStringLiteral("ws-7"));
            t.activeWorkspace = QStringLiteral("ws-7");
            return t;
        };
        const WorkspaceDocument file = roundTrip({QStringLiteral("ws-2")});

        WorkspaceDocument renamed = base();
        WX::MergeReport r1 = WX::merge(renamed, file, {});
        report("a clashing name is renamed by default, never overwritten",
               renamed.workspaces.size() == 2
                   && renamed.workspaces.at(1).label == QStringLiteral("Contest (2)")
                   && r1.added == QStringList{QStringLiteral("Contest (2)")});

        WorkspaceDocument replaced = base();
        WX::MergeOptions rep;
        rep.onConflict = [](const QString&) { return WX::Conflict::Replace; };
        rep.knownProfiles = {QStringLiteral("CW Contest")};
        WX::MergeReport r2 = WX::merge(replaced, file, rep);
        report("Replace keeps the existing id and swaps the arrangement",
               replaced.workspaces.size() == 1
                   && replaced.workspaces.first().id == QStringLiteral("ws-7")
                   && replaced.workspaces.first().surfaces.size() == 2
                   && r2.replaced == QStringList{QStringLiteral("Contest")});
        report("Replace keeps the binding on the same workspace",
               replaced.bindings.value(QStringLiteral("CW Contest")) == QStringLiteral("ws-7"));

        WorkspaceDocument skipped = base();
        WX::MergeOptions skip;
        skip.onConflict = [](const QString&) { return WX::Conflict::Skip; };
        WX::MergeReport r3 = WX::merge(skipped, file, skip);
        report("Skip leaves the existing workspace untouched",
               skipped.workspaces.size() == 1
                   && skipped.workspaces.first().surfaces.size() == 1
                   && r3.skipped == QStringList{QStringLiteral("Contest")} && !r3.changed());

        // A different workspace already owns the profile here: keep it.
        WorkspaceDocument owned = base();
        owned.workspaces.first().label = QStringLiteral("Mine");
        WX::MergeOptions keep;
        keep.knownProfiles = {QStringLiteral("CW Contest")};
        WX::MergeReport r4 = WX::merge(owned, file, keep);
        report("an existing binding here is kept, and reported",
               owned.bindings.value(QStringLiteral("CW Contest")) == QStringLiteral("ws-7")
                   && r4.keptBindings == QStringList{QStringLiteral("CW Contest")});
    }

    {
        // Single-monitor target: extra windows fold into main.
        WorkspaceDocument target;
        target.workspaces.append(workspace(QStringLiteral("ws-1"), QStringLiteral("Classic")));
        WX::MergeOptions fold;
        fold.foldExtraSurfaces = true;
        WX::MergeReport r = WX::merge(target, roundTrip({QStringLiteral("ws-2")}), fold);
        const Workspace& ws = target.workspaces.last();
        bool txOnMain = false;
        for (const CanvasItem& it : ws.surfaces.first().items) {
            txOnMain |= it.id == QStringLiteral("applet:TX");
        }
        report("extra canvas windows fold into the main canvas",
               ws.surfaces.size() == 1 && txOnMain && r.foldedSurfaces == 1);
        bool dense = true;
        const auto& items = ws.surfaces.first().items;
        for (int i = 0; i < items.size(); ++i) dense &= items.at(i).z == i;
        report("stacking stays dense after folding", dense);
    }

    {
        // Rects hanging off the surface are pulled back on.
        WorkspaceDocument file;
        Workspace ws = workspace(QStringLiteral("ws-1"), QStringLiteral("Wide"));
        ws.surfaces.first().items[1].rect = NormRect{1.4, -0.3, 0.3, 0.4};
        file.workspaces.append(ws);
        file.activeWorkspace = ws.id;
        WorkspaceDocument target;
        target.workspaces.append(workspace(QStringLiteral("ws-1"), QStringLiteral("Classic")));
        WX::merge(target, file, {});
        const NormRect r = target.workspaces.last().surfaces.first().items.at(1).rect;
        report("an item off the surface is pulled back on",
               r.x >= 0.0 && r.y >= 0.0 && r.right() <= 1.0 + 1e-9 && r.bottom() <= 1.0 + 1e-9);
    }

    if (g_failures) {
        std::printf("%d failure(s)\n", g_failures);
    }
    return g_failures ? 1 : 0;
}
