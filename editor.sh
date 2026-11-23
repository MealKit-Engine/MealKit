#!/usr/bin/env bash
# Opens the MealKit editor. Builds the editor first if needed.
# Usage: ./editor.sh                    MealKit Manager when it is installed, otherwise a project list (recent projects,
#                                       the games beside the engine, the sample, or a folder you pick)
#        ./editor.sh <project folder>   that project
#        ./editor.sh sample [scene]     the engine's sample project (samples/sandbox), optionally another scene
#        ./editor.sh --install-shortcut "MealKit Editor" in the app menu and on the desktop (run it again after moving the engine)
#        ./editor.sh --remove-shortcut  removes both
set -e
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exe="$here/build/Editor/bin/MealKitEditor"
workspace="$(dirname "$here")"
userdir="${XDG_DATA_HOME:-$HOME/.local/share}/MealKit"
manager="$userdir/manager/MealKitManager"
apps="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
launcher="mealkit-editor.desktop"
desktop_dir="$(xdg-user-dir DESKTOP 2>/dev/null || echo "$HOME/Desktop")"

have_gui() { command -v zenity >/dev/null && [ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]; }
fail() {
    if [ ! -t 1 ] && have_gui; then zenity --error --title="MealKit Editor" --text="$1" 2>/dev/null || true; fi
    echo "$1" >&2
    exit 1
}

# A project folder is one with a mealkit.project file
is_project() { [ -f "$1/mealkit.project" ]; }

desktop_entry() {
    local exec="\"$here/editor.sh\""
    cat <<EOF
[Desktop Entry]
Type=Application
Version=1.0
Name=MealKit Editor
Comment=Open a MealKit project
Exec=$exec %f
Icon=$here/engine/res/MealKitEditor.png
Path=$here
Terminal=false
Categories=Development;
StartupNotify=true
StartupWMClass=MealKitEditor
EOF
    local actions="sample" ids=() names=()
    for dir in "$workspace"/games/*/; do
        dir="${dir%/}"
        is_project "$dir" || continue
        local id; id="game-$(basename "$dir" | tr -cd 'A-Za-z0-9-')"
        actions="$actions;$id"; ids+=("$id"); names+=("$dir")
    done
    echo "Actions=$actions;"
    echo
    echo "[Desktop Action sample]"
    echo "Name=Sample project (sandbox)"
    echo "Exec=$exec sample"
    for i in "${!ids[@]}"; do
        echo
        echo "[Desktop Action ${ids[$i]}]"
        echo "Name=$(basename "${names[$i]}")"
        echo "Exec=$exec \"${names[$i]}\""
    done
}

case "${1:-}" in
--install-shortcut)
    mkdir -p "$apps"
    desktop_entry >"$apps/$launcher"
    chmod +x "$apps/$launcher"
    echo "app menu: $apps/$launcher"
    if [ -d "$desktop_dir" ]; then
        cp "$apps/$launcher" "$desktop_dir/$launcher"
        chmod +x "$desktop_dir/$launcher"
        # GNOME runs a desktop file only once it is trusted ("Allow Launching")
        gio set "$desktop_dir/$launcher" metadata::trusted true 2>/dev/null || true
        echo "desktop:  $desktop_dir/$launcher"
    fi
    update-desktop-database "$apps" 2>/dev/null || true
    exit 0
    ;;
--remove-shortcut)
    rm -f "$apps/$launcher" "$desktop_dir/$launcher"
    update-desktop-database "$apps" 2>/dev/null || true
    echo "removed"
    exit 0
    ;;
esac

if [ ! -x "$exe" ]; then
    build=(pwsh -NoProfile -File "$workspace/.claude/scripts/mk-build.ps1" -Config Editor -Repo MealKit)
    if [ ! -t 1 ] && have_gui; then
        # Started from the shortcut: no terminal, so show that it is building
        log="$userdir/editor_build.log"
        mkdir -p "$userdir"
        "${build[@]}" >"$log" 2>&1 &
        pid=$!
        (while kill -0 "$pid" 2>/dev/null; do sleep 1; done) |
            zenity --progress --pulsate --auto-close --no-cancel --title="MealKit Editor" --text="Building the editor..." 2>/dev/null || true
        wait "$pid" || fail "The editor build failed. Log: $log"
    else
        "${build[@]}"
    fi
    [ -x "$exe" ] || fail "The editor build finished without $exe"
fi

if [ "${1:-}" = "sample" ]; then
    shift
    exec "$exe" --project "$here/samples/sandbox" "$@"
fi
if [ -n "${1:-}" ] && [ "${1#-}" = "$1" ]; then
    project="$1"
    shift
    [ -f "$project" ] && [ "$(basename "$project")" = "mealkit.project" ] && project="$(dirname "$project")"
    is_project "$project" || fail "Not a MealKit project (no mealkit.project): $project"
    exec "$exe" --project "$(cd "$project" && pwd)" "$@"
fi
if [ -n "${1:-}" ] || [ -x "$manager" ] || ! have_gui; then
    exec "$exe" "$@" # the editor starts the Manager itself, or says it isn't installed
fi

# No Manager: pick a project here
rows=()
declare -A seen
add_row() {
    local dir; dir="$(cd "$1" 2>/dev/null && pwd)" || return 0
    is_project "$dir" || return 0
    [ -z "${seen[$dir]:-}" ] || return 0
    case "$dir" in /tmp/*) return 0 ;; esac # test projects
    seen[$dir]=1
    rows+=("$(basename "$dir")" "$2" "$dir")
}
if [ -f "$userdir/recent_projects.txt" ]; then
    while IFS= read -r line; do [ -n "$line" ] && add_row "$line" "recent"; done <"$userdir/recent_projects.txt"
fi
for dir in "$workspace"/games/*/; do add_row "$dir" "game"; done
add_row "$here/samples/sandbox" "sample"
rows+=("Open another folder..." "" "")

choice="$(zenity --list --title="MealKit Editor" --text="Open a project" --width=720 --height=420 \
    --column="Project" --column="" --column="Folder" --print-column=3 "${rows[@]}" 2>/dev/null)" || exit 0
if [ -z "$choice" ]; then
    choice="$(zenity --file-selection --directory --title="MealKit project folder" 2>/dev/null)" || exit 0
fi
is_project "$choice" || fail "Not a MealKit project (no mealkit.project): $choice"
exec "$exe" --project "$choice"
