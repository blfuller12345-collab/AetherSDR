#pragma once

// Workspace export and import (#5276): one or more workspaces written to a
// portable, human-readable .aetherws file, and merged back into another
// install's workspace document. Widget-free, like WorkspaceDocument, so the
// file format and every merge rule are unit-tested without a canvas; see
// tests/workspace_exchange_test.cpp.
//
// File shape (JSON):
//   {
//     "format": "aether-workspace",
//     "formatVersion": 1,
//     "writerVersion": "<AetherSDR version>",
//     "exportedAt": "<ISO-8601 UTC>",
//     "document": { ...a WorkspaceDocument::toJson() of the exported set... }
//   }
// Reusing the document schema means the importer gets WorkspaceDocument's
// strict parser (newer schema refused, damage repaired and reported) for free.
//
// Never exported: window geometry hints (machine- and monitor-specific), the
// canvas on/off flag, and bindings for workspaces outside the set. The
// document holds no credentials, device ids or paths; the test pins that.

#include "gui/workspace/WorkspaceDocument.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <functional>

namespace AetherSDR {

namespace WorkspaceExchange {

inline constexpr int kFormatVersion = 1;
inline const QString kFormatName = QStringLiteral("aether-workspace");
inline const QString kFileSuffix = QStringLiteral("aetherws");

// The exported set: the named workspaces from `source`, in the source's order,
// with their radio-profile bindings. Unknown ids are ignored. Returns empty
// bytes when nothing matched.
QByteArray exportWorkspaces(const WorkspaceDocument& source,
                            const QStringList& workspaceIds,
                            const QString& writerVersion,
                            const QString& exportedAtUtc);

// A suggested file name for one workspace: its label with characters that
// are unsafe in file names replaced, plus ".aetherws".
QString suggestedFileName(const QString& workspaceLabel);

// Parse a file. Returns false with `error` set for anything that is not an
// AetherSDR workspace file this build can read (wrong format, newer format,
// unreadable document). Repaired damage is reported in `warnings`.
bool parse(const QByteArray& bytes, WorkspaceDocument* out,
           QString* writerVersion = nullptr,
           QString* error = nullptr,
           QStringList* warnings = nullptr);

// What to do with an incoming workspace whose label already exists.
enum class Conflict { Rename, Replace, Skip };

struct MergeOptions {
    // Asked once per incoming workspace whose label matches an existing one.
    // Unset = Rename (never overwrite without being told to).
    std::function<Conflict(const QString& label)> onConflict;
    // Applet ids this install has. "applet:<id>" items for other ids are
    // left out and reported. Empty = keep every applet item.
    QStringList knownAppletIds;
    // Radio global profiles this install knows. A binding to any other
    // profile is left out and reported. Empty = no profile is known.
    QStringList knownProfiles;
    // Fold extra canvas windows into the main canvas (single-monitor target).
    bool foldExtraSurfaces = false;
};

struct MergeReport {
    QStringList added;            // labels as they now appear
    QStringList replaced;         // labels replaced in place
    QStringList skipped;          // labels the operator chose to skip
    QStringList droppedApplets;   // "<workspace>: <applet id>"
    QStringList unboundProfiles;  // profiles in the file this install lacks
    QStringList keptBindings;     // profiles already bound here, left alone
    int foldedSurfaces = 0;       // extra windows folded into main
    bool changed() const { return !added.isEmpty() || !replaced.isEmpty(); }
};

// Merge every workspace in `incoming` into `target`. New workspaces get fresh
// ids; a Replace keeps the existing id (so its bindings and active state
// stay). Rects are clamped onto their surface. Never removes a workspace
// that is not being replaced, and never changes the active workspace.
MergeReport merge(WorkspaceDocument& target, const WorkspaceDocument& incoming,
                  const MergeOptions& options);

}  // namespace WorkspaceExchange

}  // namespace AetherSDR
