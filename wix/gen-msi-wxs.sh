#!/bin/sh
# gen-msi-wxs.sh — Genera un WiX v3 .wxs desde un directorio.
# Uso: gen-msi-wxs.sh <source-dir> <version> <app-name> <game-title>
# Salida: WiX XML por stdout.

set -e

SRC="$1"
VERSION="$2"
APP_NAME="$3"
GAME_TITLE="$4"

if [ ! -d "$SRC" ]; then
    echo "ERROR: $SRC no es un directorio" >&2
    exit 1
fi

UPGRADE_GUID="A1B2C3D4-E5F6-7890-ABCD-EF1234567890"

sanitize() {
    echo "$1" | sed 's|[^a-zA-Z0-9_.]|_|g'
}

# Emite directorios anidados recursivamente
emit_directory_tree() {
    local rel="$1"
    local indent="$2"
    local fs="$SRC/${rel}"
    [ -z "$rel" ] && fs="$SRC"
    for sub in $(cd "$fs" && find . -maxdepth 1 -type d ! -name '.' | sed 's|^\./||' | sort); do
        local child
        [ -z "$rel" ] && child="$sub" || child="$rel/$sub"
        local id="Dir_$(sanitize "$child")"
        echo "${indent}<Directory Id=\"$id\" Name=\"$sub\">"
        emit_directory_tree "$child" "$indent  "
        echo "${indent}</Directory>"
    done
}

# Emite DirectoryRef + Components de una rama
emit_dir_components() {
    local rel="$1"
    local indent="$2"
    local fs="$SRC/${rel}"
    [ -z "$rel" ] && fs="$SRC"
    local dir_id
    [ -z "$rel" ] && dir_id="INSTALLFOLDER" || dir_id="Dir_$(sanitize "$rel")"

    local files=$(cd "$fs" && find . -maxdepth 1 -type f | sed 's|^\./||' | sort)
    if [ -n "$files" ]; then
        echo "${indent}<DirectoryRef Id=\"$dir_id\">"
        for f in $files; do
            local src
            [ -z "$rel" ] && src="$f" || src="$rel/$f"
            local comp_id="Comp_$(sanitize "$src")"
            local file_id="File_$(sanitize "$src")"
            echo "${indent}  <Component Id=\"$comp_id\" Guid=\"*\">"
            echo "${indent}    <File Id=\"$file_id\" Source=\"\$(var.SourceDir)/$src\" KeyPath=\"yes\"/>"
            echo "${indent}  </Component>"
        done
        echo "${indent}</DirectoryRef>"
    fi

    for sub in $(cd "$fs" && find . -maxdepth 1 -type d ! -name '.' | sed 's|^\./||' | sort); do
        local child
        [ -z "$rel" ] && child="$sub" || child="$rel/$sub"
        emit_dir_components "$child" "$indent"
    done
}

# Emite todos los ComponentRef para el <Feature>
emit_component_refs() {
    local rel="$1"
    local fs="$SRC/${rel}"
    [ -z "$rel" ] && fs="$SRC"
    for f in $(cd "$fs" && find . -maxdepth 1 -type f | sed 's|^\./||' | sort); do
        local src
        [ -z "$rel" ] && src="$f" || src="$rel/$f"
        local comp_id="Comp_$(sanitize "$src")"
        echo "      <ComponentRef Id=\"$comp_id\"/>"
    done
    for sub in $(cd "$fs" && find . -maxdepth 1 -type d ! -name '.' | sed 's|^\./||' | sort); do
        local child
        [ -z "$rel" ] && child="$sub" || child="$rel/$sub"
        emit_component_refs "$child"
    done
}

cat <<EOF
<?xml version="1.0" encoding="utf-8"?>
<Wix xmlns="http://schemas.microsoft.com/wix/2006/wi">
  <Product Id="*" Name="$GAME_TITLE" Language="1033" Version="$VERSION"
           Manufacturer="Escape From Bea" UpgradeCode="$UPGRADE_GUID">
    <Package InstallerVersion="500" Compressed="yes" InstallScope="perMachine"/>
    <MediaTemplate EmbedCab="yes"/>

    <Directory Id="TARGETDIR" Name="SourceDir">
      <Directory Id="ProgramFilesFolder">
        <Directory Id="INSTALLFOLDER" Name="Escape From Bea">
EOF

emit_directory_tree "" "          "

cat <<EOF
        </Directory>
      </Directory>
      <Directory Id="ProgramMenuFolder">
        <Directory Id="AppProgramMenuFolder" Name="Escape From Bea"/>
      </Directory>
      <Directory Id="DesktopFolder"/>
    </Directory>

    <DirectoryRef Id="AppProgramMenuFolder">
      <Component Id="StartMenuShortcut" Guid="*">
        <Shortcut Id="StartMenuShortcut" Name="$GAME_TITLE" Description="$GAME_TITLE"
                  Target="[INSTALLFOLDER]$APP_NAME.exe" WorkingDirectory="INSTALLFOLDER"/>
        <RemoveFolder Id="RemoveAppProgramMenuFolder" On="uninstall"/>
        <RegistryValue Root="HKCU" Key="Software\\EscapeFromBea" Name="installed"
                       Type="integer" Value="1" KeyPath="yes"/>
      </Component>
    </DirectoryRef>

    <DirectoryRef Id="DesktopFolder">
      <Component Id="DesktopShortcut" Guid="*">
        <Shortcut Id="DesktopShortcut" Name="$GAME_TITLE" Description="$GAME_TITLE"
                  Target="[INSTALLFOLDER]$APP_NAME.exe" WorkingDirectory="INSTALLFOLDER"/>
        <RegistryValue Root="HKCU" Key="Software\\EscapeFromBea" Name="desktop_installed"
                       Type="integer" Value="1" KeyPath="yes"/>
      </Component>
    </DirectoryRef>

EOF

emit_dir_components "" "    "

cat <<EOF
    <Feature Id="ProductFeature" Title="$GAME_TITLE" Level="1">
      <ComponentRef Id="StartMenuShortcut"/>
      <ComponentRef Id="DesktopShortcut"/>
EOF

emit_component_refs ""

cat <<EOF
    </Feature>
  </Product>
</Wix>
EOF
