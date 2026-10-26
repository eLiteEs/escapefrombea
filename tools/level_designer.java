import javax.swing.*;
import javax.swing.filechooser.FileNameExtensionFilter;
import java.awt.*;
import java.awt.event.*;
import java.awt.image.BufferedImage;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;

record TileDef(char ch, Color fg, Color bg, String name) {}
record Change(int x, int y, char prevCell, int prevHeight) {}
record SignEntry(int x, int y, String text) {}
record NpcEntry(int x, int y, String name, String text) {}

// ==================== CONSTANTS ====================
final int MAX_HEIGHT = 4;
final int MAX_UNDO_OPS = 10000;
final int[] ZOOM_LEVELS = {16, 24, 32, 48, 64};
final String[] ZOOM_NAMES = {"16px", "24px", "32px", "48px", "64px"};

final TileDef[] TILES = {
    new TileDef('#', from256(250), from256(237), "muro"),
    new TileDef('.', from256(240), from256(233), "suelo"),
    new TileDef('s', from256(16),  from256(40),  "spawn"),
    new TileDef('i', from256(16),  from256(178), "item"),
    new TileDef('e', from256(15),  from256(124), "enemigo"),
    new TileDef('f', from256(16),  from256(44),  "finish"),
    new TileDef('L', from256(15),  from256(25),  "taquilla"),
    new TileDef('F', from256(16),  from256(45),  "fuente"),
    new TileDef('k', from256(220), from256(94),  "llave"),
    new TileDef('^', from256(220), from256(94),  "rampa N"),
    new TileDef('>', from256(220), from256(94),  "rampa E"),
    new TileDef('v', from256(220), from256(94),  "rampa S"),
    new TileDef('<', from256(220), from256(94),  "rampa W"),
    new TileDef('T', from256(220), from256(130), "cartel"),
    new TileDef('N', from256(16),  from256(130), "NPC"),
};

// ==================== STATE ====================
int mapW = 25, mapH = 20;
char[][] cells = new char[mapH][mapW];
int[][] heights = new int[mapH][mapW];
int cursorX = 0, cursorY = 0;

int selX0 = -1, selY0 = -1, selX1 = -1, selY1 = -1;
boolean selectionActive = false;
int anchorX = 0, anchorY = 0;

char brush = '#';
int brushHeight = 0;
boolean heightMode = false;
int zoomIdx = 2;
int activeFloor = 0;   // -1 = "Todos"

String currentPath = "assets/levels/level_01_map.dgn";
ArrayDeque<ArrayList<Change>> undoStack = new ArrayDeque<>();
ArrayList<Change> currentOp = null;
boolean dirty = false;

char[][] clipboardCells = null;
int[][] clipboardHeights = null;

final ArrayList<SignEntry> signs = new ArrayList<>();
final ArrayList<NpcEntry>  npcs  = new ArrayList<>();

JFrame frame;
JLabel mapLabel;
JLabel statusLabel;
BufferedImage buffer;
JButton[] paletteButtons;
JButton[] heightButtons;
JToggleButton btnCells, btnHeights;
JPanel palettePanel, heightPanel;
JComboBox<String> floorCombo;

// ==================== ENTRY ====================
void main() {
    for (int y = 0; y < mapH; y++) {
        Arrays.fill(cells[y], '.');
        Arrays.fill(heights[y], 0);
    }
    if (Files.exists(Paths.get(currentPath))) loadFromPath(currentPath);
    SwingUtilities.invokeLater(() -> buildUI());
}

// ==================== HELPERS ====================
Color from256(int idx) {
    if (idx < 16) {
        int[] std = {0x000000,0x800000,0x008000,0x808000,0x000080,0x800080,0x008080,0xC0C0C0,
                     0x808080,0xFF0000,0x00FF00,0xFFFF00,0x0000FF,0xFF00FF,0x00FFFF,0xFFFFFF};
        int rgb = std[idx];
        return new Color((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
    }
    if (idx < 232) {
        idx -= 16;
        return new Color((idx / 36) * 51, ((idx % 36) / 6) * 51, (idx % 6) * 51);
    }
    int v = 8 + (idx - 232) * 10;
    return new Color(v, v, v);
}

Color heightColor(int h) {
    return switch (h) {
        case 0 -> new Color(30, 34, 48);
        case 1 -> new Color(60, 80, 130);
        case 2 -> new Color(90, 130, 180);
        case 3 -> new Color(130, 170, 220);
        case 4 -> new Color(180, 210, 240);
        default -> Color.GRAY;
    };
}

boolean isWallTile(char c) { return c == '#' || c == 'L' || c == 'B'; }
boolean isRamp(char c)     { return c == '^' || c == '>' || c == 'v' || c == '<'; }
boolean isMarker(char c)   { return c == 'T' || c == 'N'; }

TileDef tileFor(char ch) {
    for (TileDef t : TILES) if (t.ch() == ch) return t;
    return null;
}

int cellPx() { return ZOOM_LEVELS[zoomIdx]; }

void clearSelection() {
    selectionActive = false;
    selX0 = selY0 = selX1 = selY1 = -1;
}

int selMinX() { return Math.min(selX0, selX1); }
int selMaxX() { return Math.max(selX0, selX1); }
int selMinY() { return Math.min(selY0, selY1); }
int selMaxY() { return Math.max(selY0, selY1); }
int selW()    { return selMaxX() - selMinX() + 1; }
int selH()    { return selMaxY() - selMinY() + 1; }

void newMap(int w, int h) {
    mapW = w; mapH = h;
    cells = new char[h][w];
    heights = new int[h][w];
    for (int y = 0; y < h; y++) {
        Arrays.fill(cells[y], '.');
        Arrays.fill(heights[y], 0);
    }
    cursorX = cursorY = 0;
    anchorX = anchorY = 0;
    clearSelection();
    undoStack.clear();
    signs.clear();
    npcs.clear();
    dirty = false;
    refreshBuffer();
    updateStatus();
}

// ==================== UNDO ====================
void beginOp() { currentOp = new ArrayList<>(); }
void commitOp() {
    if (currentOp != null && !currentOp.isEmpty()) {
        undoStack.push(currentOp);
        while (undoStack.size() > MAX_UNDO_OPS) undoStack.removeLast();
    }
    currentOp = null;
}
void touch(int x, int y) {
    if (currentOp == null) beginOp();
    currentOp.add(new Change(x, y, cells[y][x], heights[y][x]));
}
boolean doUndo() {
    if (undoStack.isEmpty()) return false;
    var op = undoStack.pop();
    for (var c : op) {
        cells[c.y()][c.x()] = c.prevCell();
        heights[c.y()][c.x()] = c.prevHeight();
    }
    dirty = true;
    return true;
}

// ==================== SIGN / NPC HELPERS ====================
SignEntry findSign(int x, int y) {
    for (SignEntry s : signs) if (s.x() == x && s.y() == y) return s;
    return null;
}
NpcEntry findNpc(int x, int y) {
    for (NpcEntry n : npcs) if (n.x() == x && n.y() == y) return n;
    return null;
}

String promptMultiline(String title, String initial) {
    JTextArea ta = new JTextArea(initial, 6, 42);
    ta.setLineWrap(true);
    ta.setWrapStyleWord(true);
    JScrollPane scroll = new JScrollPane(ta);
    Object[] msg = { title, scroll };
    int r = JOptionPane.showConfirmDialog(frame, msg, "Editor de texto",
        JOptionPane.OK_CANCEL_OPTION, JOptionPane.PLAIN_MESSAGE);
    return (r == JOptionPane.OK_OPTION) ? ta.getText() : null;
}

void createSignAt(int x, int y) {
    if (findSign(x, y) != null) return;
    String txt = promptMultiline("Texto del cartel en (" + x + "," + y + "):",
                                 "Texto del cartel");
    if (txt == null) {
        cells[y][x] = '.';   // cancelado
        return;
    }
    signs.add(new SignEntry(x, y, txt));
    dirty = true;
}

void createNpcAt(int x, int y) {
    if (findNpc(x, y) != null) return;
    String name = JOptionPane.showInputDialog(frame,
        "Nombre del NPC en (" + x + "," + y + "):", "NPC");
    if (name == null) { cells[y][x] = '.'; return; }
    String txt = promptMultiline("Diálogo de " + name + ":", "Hola.");
    if (txt == null) { cells[y][x] = '.'; return; }
    npcs.add(new NpcEntry(x, y, name, txt));
    dirty = true;
}

void editSignAt(int x, int y) {
    SignEntry s = findSign(x, y);
    if (s == null) { createSignAt(x, y); return; }
    String newTxt = promptMultiline("Editar cartel en (" + x + "," + y + "):",
                                    s.text());
    if (newTxt == null) return;
    signs.remove(s);
    signs.add(new SignEntry(x, y, newTxt));
    dirty = true;
    status("Cartel actualizado");
}

void editNpcAt(int x, int y) {
    NpcEntry n = findNpc(x, y);
    if (n == null) { createNpcAt(x, y); return; }
    String newName = JOptionPane.showInputDialog(frame,
        "Nombre del NPC en (" + x + "," + y + "):", n.name());
    if (newName == null) return;
    String newTxt = promptMultiline("Diálogo de " + newName + ":", n.text());
    if (newTxt == null) return;
    npcs.remove(n);
    npcs.add(new NpcEntry(x, y, newName, newTxt));
    dirty = true;
    status("NPC actualizado");
}

// ==================== PINTAR ====================
void paintCell(int x, int y) {
    if (x < 0 || y < 0 || x >= mapW || y >= mapH) return;
    touch(x, y);

    if (heightMode) {
        heights[y][x] = brushHeight;
    } else {
        char prev = cells[y][x];
        cells[y][x] = brush;

        // Limpiar entradas antiguas si dejamos de ser T / N
        if (prev == 'T' && brush != 'T') {
            SignEntry s = findSign(x, y);
            if (s != null) signs.remove(s);
        }
        if (prev == 'N' && brush != 'N') {
            NpcEntry n = findNpc(x, y);
            if (n != null) npcs.remove(n);
        }

        // Asignar piso activo si el brush es transitable
        if (activeFloor >= 0 && !isWallTile(brush)) {
            heights[y][x] = activeFloor;
        }

        // Crear cartel / NPC si es la primera vez
        if (brush == 'T' && findSign(x, y) == null) {
            createSignAt(x, y);
        } else if (brush == 'N' && findNpc(x, y) == null) {
            createNpcAt(x, y);
        }
    }
    dirty = true;
}

void fillSelection() {
    if (!selectionActive) return;
    beginOp();
    for (int y = selMinY(); y <= selMaxY(); y++) {
        for (int x = selMinX(); x <= selMaxX(); x++) {
            if (x < 0 || y < 0 || x >= mapW || y >= mapH) continue;
            touch(x, y);
            if (heightMode) {
                heights[y][x] = brushHeight;
            } else {
                char prev = cells[y][x];
                cells[y][x] = brush;
                if (prev == 'T' && brush != 'T') {
                    SignEntry s = findSign(x, y); if (s != null) signs.remove(s);
                }
                if (prev == 'N' && brush != 'N') {
                    NpcEntry n = findNpc(x, y); if (n != null) npcs.remove(n);
                }
                if (activeFloor >= 0 && !isWallTile(brush)) heights[y][x] = activeFloor;
            }
        }
    }
    commitOp();
    dirty = true;
    refreshBuffer();
    updateStatus();
}

void deleteSelection() {
    if (!selectionActive) return;
    beginOp();
    for (int y = selMinY(); y <= selMaxY(); y++) {
        for (int x = selMinX(); x <= selMaxX(); x++) {
            if (x < 0 || y < 0 || x >= mapW || y >= mapH) continue;
            touch(x, y);
            if (heightMode) {
                heights[y][x] = 0;
            } else {
                char prev = cells[y][x];
                cells[y][x] = '.';
                if (prev == 'T') {
                    SignEntry s = findSign(x, y); if (s != null) signs.remove(s);
                }
                if (prev == 'N') {
                    NpcEntry n = findNpc(x, y); if (n != null) npcs.remove(n);
                }
            }
        }
    }
    commitOp();
    dirty = true;
    refreshBuffer();
    updateStatus();
}

// ==================== COPY / PASTE ====================
void copySelection() {
    if (!selectionActive) return;
    int w = selW(), h = selH();
    clipboardCells = new char[h][w];
    clipboardHeights = new int[h][w];
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int mx = selMinX() + x, my = selMinY() + y;
            clipboardCells[y][x]   = (mx < mapW && my < mapH) ? cells[my][mx] : '.';
            clipboardHeights[y][x] = (mx < mapW && my < mapH) ? heights[my][mx] : 0;
        }
    status("Copiado: " + w + "x" + h);
}

void cutSelection() {
    if (!selectionActive) return;
    copySelection();
    deleteSelection();
    status("Cortado: " + selW() + "x" + selH());
}

void pasteClipboard() {
    if (clipboardCells == null) { status("Nada en el portapapeles"); return; }
    int w = clipboardCells[0].length, h = clipboardCells.length;
    beginOp();
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int mx = cursorX + x, my = cursorY + y;
            if (mx < 0 || my < 0 || mx >= mapW || my >= mapH) continue;
            touch(mx, my);
            cells[my][mx]   = clipboardCells[y][x];
            heights[my][mx] = clipboardHeights[y][x];
            // Marcadores pegados desde el clipboard no traen texto; los
            // eliminamos para no dejar huérfanos.
            if (cells[my][mx] == 'T') {
                SignEntry s = findSign(mx, my); if (s != null) signs.remove(s);
            }
            if (cells[my][mx] == 'N') {
                NpcEntry n = findNpc(mx, my); if (n != null) npcs.remove(n);
            }
        }
    commitOp();
    dirty = true;
    refreshBuffer();
    updateStatus();
    status("Pegado: " + w + "x" + h);
}

void selectAll() {
    selX0 = 0; selY0 = 0; selX1 = mapW - 1; selY1 = mapH - 1;
    selectionActive = true;
    refreshBuffer();
    updateStatus();
}

void grabTile() {
    if (heightMode) {
        brushHeight = heights[cursorY][cursorX];
    } else {
        brush = cells[cursorY][cursorX];
    }
    updatePaletteVisuals();
    updateStatus();
}

// ==================== FILE I/O ====================
String escapeText(String s) {
    return s.replace("\n", "\\n").replace(",", ";");
}

void loadFromPath(String path) {
    try {
        var rawLines = Files.readAllLines(Paths.get(path), StandardCharsets.UTF_8);
        var lines = new ArrayList<String>();
        for (String l : rawLines) {
            String trimmed = l.stripTrailing();
            if (!trimmed.isEmpty()) lines.add(trimmed);
        }
        if (lines.isEmpty()) { status("Archivo vacío"); return; }

        // Encontrar dónde empiezan las secciones @
        int cellsEnd = lines.size();
        for (int i = 0; i < lines.size(); i++) {
            if (lines.get(i).startsWith("@")) { cellsEnd = i; break; }
        }
        if (cellsEnd < 3) { status("Mapa demasiado pequeño"); return; }

        int nw = lines.get(0).length();
        int nh = cellsEnd;
        mapW = nw; mapH = nh;
        cells = new char[nh][nw];
        heights = new int[nh][nw];
        signs.clear();
        npcs.clear();

        // Celdas
        for (int y = 0; y < nh; y++) {
            String l = lines.get(y);
            for (int x = 0; x < nw; x++)
                cells[y][x] = (x < l.length()) ? l.charAt(x) : '.';
        }

        // Secciones
        int i = cellsEnd;
        while (i < lines.size()) {
            String sec = lines.get(i);
            if (sec.equals("@HEIGHTS")) {
                i++;
                for (int y = 0; y < nh && i < lines.size(); y++, i++) {
                    String l = lines.get(i);
                    if (l.startsWith("@")) { i--; break; }
                    for (int x = 0; x < nw && x < l.length(); x++) {
                        char c = l.charAt(x);
                        if (c >= '0' && c <= '9')
                            heights[y][x] = Math.min(c - '0', MAX_HEIGHT);
                    }
                }
                i++;
            } else if (sec.equals("@SIGNS")) {
                i++;
                while (i < lines.size() && !lines.get(i).startsWith("@")) {
                    String[] parts = lines.get(i).split(",", 3);
                    if (parts.length >= 3) {
                        try {
                            int sx = Integer.parseInt(parts[0].trim());
                            int sy = Integer.parseInt(parts[1].trim());
                            String txt = parts[2].replace("\\n", "\n");
                            signs.add(new SignEntry(sx, sy, txt));
                        } catch (Exception ignored) {}
                    }
                    i++;
                }
            } else if (sec.equals("@NPCS")) {
                i++;
                while (i < lines.size() && !lines.get(i).startsWith("@")) {
                    String[] parts = lines.get(i).split(",", 4);
                    if (parts.length >= 4) {
                        try {
                            int nx = Integer.parseInt(parts[0].trim());
                            int ny = Integer.parseInt(parts[1].trim());
                            String nm  = parts[2];
                            String txt = parts[3].replace("\\n", "\n");
                            npcs.add(new NpcEntry(nx, ny, nm, txt));
                        } catch (Exception ignored) {}
                    }
                    i++;
                }
            } else {
                i++;
            }
        }

        cursorX = cursorY = 0;
        anchorX = anchorY = 0;
        clearSelection();
        undoStack.clear();
        dirty = false;
        currentPath = path;
        refreshBuffer();
        updateStatus();
        status("Cargado: " + path);
    } catch (Exception ex) {
        status("Error al cargar: " + ex.getMessage());
    }
}

boolean saveToPath(String path) {
    try (var w = Files.newBufferedWriter(Paths.get(path), StandardCharsets.UTF_8)) {
        for (int y = 0; y < mapH; y++) {
            w.write(cells[y]);
            w.write('\n');
        }

        boolean anyHeight = false;
        for (int y = 0; y < mapH && !anyHeight; y++)
            for (int x = 0; x < mapW && !anyHeight; x++)
                if (heights[y][x] > 0) anyHeight = true;

        if (anyHeight) {
            w.write("@HEIGHTS\n");
            for (int y = 0; y < mapH; y++) {
                for (int x = 0; x < mapW; x++)
                    w.write('0' + Math.min(heights[y][x], MAX_HEIGHT));
                w.write('\n');
            }
        }

        if (!signs.isEmpty()) {
            w.write("@SIGNS\n");
            for (SignEntry s : signs) {
                if (s.x() < 0 || s.x() >= mapW || s.y() < 0 || s.y() >= mapH) continue;
                w.write(s.x() + "," + s.y() + "," + escapeText(s.text()) + "\n");
            }
        }

        if (!npcs.isEmpty()) {
            w.write("@NPCS\n");
            for (NpcEntry n : npcs) {
                if (n.x() < 0 || n.x() >= mapW || n.y() < 0 || n.y() >= mapH) continue;
                w.write(n.x() + "," + n.y() + ","
                        + escapeText(n.name()) + ","
                        + escapeText(n.text()) + "\n");
            }
        }

        currentPath = path;
        dirty = false;
        updateStatus();
        status("Guardado: " + path);
        return true;
    } catch (Exception ex) {
        status("Error al guardar: " + ex.getMessage());
        return false;
    }
}

// ==================== STATUS / RENDER ====================
void status(String msg) { if (statusLabel != null) statusLabel.setText(msg); }

void updateStatus() {
    if (statusLabel == null) return;
    char cur = cells[cursorY][cursorX];
    TileDef curT = tileFor(cur);
    TileDef bruT = tileFor(brush);
    String curName = (curT != null) ? curT.name() : String.valueOf(cur);
    String bruName = (bruT != null) ? bruT.name() : String.valueOf(brush);
    String mode = heightMode ? "ALTURAS" : "CELDAS";
    String floor = (activeFloor < 0) ? "Todos" : ("P" + activeFloor);
    String sel = selectionActive
        ? String.format("  SEL %dx%d", selW(), selH()) : "";
    String extra = "";
    if (cur == 'T') {
        SignEntry s = findSign(cursorX, cursorY);
        if (s != null) extra = "  [cartel: " + s.text().replace("\n", " ").substring(0,
            Math.min(20, s.text().replace("\n", " ").length())) + "...]";
    } else if (cur == 'N') {
        NpcEntry n = findNpc(cursorX, cursorY);
        if (n != null) extra = "  [NPC: " + n.name() + "]";
    }
    String dirtyMark = dirty ? "  *" : "";
    statusLabel.setText(String.format(
        "[%s|%s] (%d,%d) '%c'(%s) H:%d  Brush '%c'(%s) H:%d%s%s  %s%s",
        mode, floor, cursorX, cursorY, cur, curName,
        heights[cursorY][cursorX], brush, bruName, brushHeight,
        sel, extra, currentPath, dirtyMark));
}

void refreshBuffer() {
    int cp = cellPx();
    int pw = mapW * cp, ph = mapH * cp;
    if (buffer == null || buffer.getWidth() != pw || buffer.getHeight() != ph)
        buffer = new BufferedImage(pw, ph, BufferedImage.TYPE_INT_RGB);

    Graphics2D g2 = buffer.createGraphics();
    g2.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
    g2.setRenderingHint(RenderingHints.KEY_TEXT_ANTIALIASING, RenderingHints.VALUE_TEXT_ANTIALIAS_ON);
    int fontSize = Math.max(8, cp - 12);
    g2.setFont(new Font("Monospaced", Font.BOLD, fontSize));
    FontMetrics fm = g2.getFontMetrics();

    for (int y = 0; y < mapH; y++) {
        for (int x = 0; x < mapW; x++) {
            int px = x * cp, py = y * cp;
            Color bg, fg;
            String draw;
            int cellH = heights[y][x];

            if (heightMode) {
                bg = heightColor(cellH);
                fg = (cellH >= 3) ? Color.BLACK : Color.WHITE;
                draw = String.valueOf(cellH);
            } else {
                char ch = cells[y][x];
                TileDef t = tileFor(ch);
                bg = (t != null) ? t.bg() : new Color(60, 60, 70);
                fg = (t != null) ? t.fg() : Color.WHITE;
                draw = String.valueOf(ch);
            }

            // Atenuar si no es del piso activo (solo en modo celdas)
            if (!heightMode && activeFloor >= 0 && cellH != activeFloor) {
                bg = bg.darker().darker();
                fg = fg.darker();
            }

            g2.setColor(bg);
            g2.fillRect(px, py, cp, cp);

            int sw = fm.stringWidth(draw);
            int asc = fm.getAscent();
            g2.setColor(fg);
            g2.drawString(draw, px + (cp - sw) / 2, py + (cp + asc) / 2 - 2);

            // Indicador de altura si no estamos en modo altura
            if (!heightMode && cellH > 0) {
                g2.setColor(new Color(255, 220, 60));
                g2.setFont(new Font("Monospaced", Font.BOLD, Math.max(8, cp / 4)));
                g2.drawString(String.valueOf(cellH), px + 2, py + cp / 4 + 4);
                g2.setFont(new Font("Monospaced", Font.BOLD, fontSize));
            }

            // Asterisco para T / N con texto asignado
            if (!heightMode) {
                char ch = cells[y][x];
                boolean hasText = (ch == 'T' && findSign(x, y) != null) ||
                                  (ch == 'N' && findNpc(x, y) != null);
                if (hasText) {
                    g2.setColor(new Color(255, 255, 0, 200));
                    g2.setFont(new Font("Monospaced", Font.BOLD, Math.max(8, cp / 3)));
                    g2.drawString("*", px + cp - cp / 3, py + cp / 3);
                    g2.setFont(new Font("Monospaced", Font.BOLD, fontSize));
                }
            }
        }
    }

    // Grid
    g2.setColor(new Color(20, 20, 30));
    for (int x = 0; x <= mapW; x++) g2.drawLine(x * cp, 0, x * cp, ph);
    for (int y = 0; y <= mapH; y++) g2.drawLine(0, y * cp, pw, y * cp);

    // Selección
    if (selectionActive) {
        int sx = selMinX() * cp, sy = selMinY() * cp;
        int sw = selW() * cp, sh = selH() * cp;
        g2.setColor(new Color(255, 240, 80, 60));
        g2.fillRect(sx, sy, sw, sh);
        g2.setColor(new Color(255, 240, 80, 200));
        g2.setStroke(new BasicStroke(2f));
        g2.drawRect(sx, sy, sw - 1, sh - 1);
    }

    // Cursor
    g2.setColor(Color.YELLOW);
    g2.setStroke(new BasicStroke(2.5f));
    g2.drawRect(cursorX * cp, cursorY * cp, cp - 1, cp - 1);

    g2.dispose();

    if (mapLabel != null) {
        mapLabel.setIcon(new ImageIcon(buffer));
        mapLabel.setPreferredSize(new Dimension(pw, ph));
        mapLabel.revalidate();
    }
}

// ==================== UI ====================
void buildUI() {
    try { UIManager.setLookAndFeel(UIManager.getSystemLookAndFeelClassName()); }
    catch (Exception ignored) {}

    frame = new JFrame("Map Editor - " + currentPath);
    frame.setDefaultCloseOperation(JFrame.DO_NOTHING_ON_CLOSE);

    // ----- Menú -----
    var menuBar = new JMenuBar();

    var fileMenu = new JMenu("Archivo");
    fileMenu.add(menuItem("Nuevo...",         KeyEvent.VK_N, this::doNew));
    fileMenu.add(menuItem("Abrir...",         KeyEvent.VK_O, this::doOpen));
    fileMenu.add(menuItem("Guardar",          KeyEvent.VK_S, this::doSave));
    fileMenu.add(menuItem("Guardar como...", -1,             this::doSaveAs));
    fileMenu.addSeparator();
    fileMenu.add(menuItem("Salir",            KeyEvent.VK_Q, this::tryClose));
    menuBar.add(fileMenu);

    var editMenu = new JMenu("Edición");
    editMenu.add(menuItem("Deshacer",         KeyEvent.VK_Z, () -> { if (doUndo()) { refreshBuffer(); updateStatus(); }}));
    editMenu.addSeparator();
    editMenu.add(menuItem("Cortar",           KeyEvent.VK_X, this::cutSelection));
    editMenu.add(menuItem("Copiar",           KeyEvent.VK_C, this::copySelection));
    editMenu.add(menuItem("Pegar",            KeyEvent.VK_V, this::pasteClipboard));
    editMenu.addSeparator();
    editMenu.add(menuItem("Seleccionar todo", KeyEvent.VK_A, this::selectAll));
    editMenu.add(menuItem("Deseleccionar",    -1,            this::clearSelectionAndRefresh));
    editMenu.addSeparator();
    editMenu.add(menuItem("Rellenar selección", -1, this::fillSelection));
    editMenu.add(menuItem("Borrar selección",  -1, this::deleteSelection));
    editMenu.addSeparator();
    editMenu.add(menuItem("Limpiar todo",     KeyEvent.VK_L, this::doClear));
    menuBar.add(editMenu);

    var viewMenu = new JMenu("Vista");
    viewMenu.add(menuItem("Zoom +",          KeyEvent.VK_EQUALS, () -> setZoom(zoomIdx + 1)));
    viewMenu.add(menuItem("Zoom -",          KeyEvent.VK_MINUS,  () -> setZoom(zoomIdx - 1)));
    viewMenu.addSeparator();
    for (int i = 0; i < ZOOM_NAMES.length; i++) {
        final int zi = i;
        viewMenu.add(menuItem("Zoom " + ZOOM_NAMES[i], -1, () -> setZoom(zi)));
    }
    menuBar.add(viewMenu);

    var toolsMenu = new JMenu("Herramientas");
    toolsMenu.add(menuItem("Añadir enemigos...",   -1, this::doEnemies));
    toolsMenu.add(menuItem("Añadir items...",      -1, this::doItems));
    toolsMenu.add(menuItem("Añadir taquillas...",  -1, this::doLockers));
    toolsMenu.addSeparator();
    toolsMenu.add(menuItem("Dibujar borde",        -1, this::doBorder));
    toolsMenu.add(menuItem("Rellenar todo...",     -1, this::doFill));
    toolsMenu.addSeparator();
    toolsMenu.add(menuItem("Editar cartel/NPC aquí", KeyEvent.VK_E,
                           this::editMarkerAtCursor));
    toolsMenu.add(menuItem("Eliminar cartel/NPC aquí", -1,
                           this::removeMarkerAtCursor));
    menuBar.add(toolsMenu);

    var helpMenu = new JMenu("Ayuda");
    helpMenu.add(menuItem("Atajos", -1, this::doHelp));
    menuBar.add(helpMenu);

    frame.setJMenuBar(menuBar);

    // ----- Barra superior -----
    var topBar = new JPanel(new BorderLayout());

    var modeBar = new JPanel(new FlowLayout(FlowLayout.LEFT, 4, 4));
    modeBar.setBorder(BorderFactory.createTitledBorder("Modo"));

    btnCells = new JToggleButton("Celdas", !heightMode);
    btnHeights = new JToggleButton("Alturas", heightMode);
    btnCells.setFocusable(false); btnHeights.setFocusable(false);
    btnCells.addActionListener(e -> { heightMode = false; btnCells.setSelected(true); btnHeights.setSelected(false); updatePaletteVisuals(); refreshBuffer(); updateStatus(); });
    btnHeights.addActionListener(e -> { heightMode = true; btnHeights.setSelected(true); btnCells.setSelected(false); updatePaletteVisuals(); refreshBuffer(); updateStatus(); });
    var bgGroup = new ButtonGroup();
    bgGroup.add(btnCells); bgGroup.add(btnHeights);
    modeBar.add(btnCells); modeBar.add(btnHeights);

    // Combo de piso
    floorCombo = new JComboBox<>(new String[]{
        "Piso 0", "Piso 1", "Piso 2", "Piso 3", "Piso 4", "Todos"});
    floorCombo.setSelectedIndex(0);
    floorCombo.addActionListener(e -> {
        int idx = floorCombo.getSelectedIndex();
        activeFloor = (idx == 5) ? -1 : idx;
        refreshBuffer();
        updateStatus();
    });
    modeBar.add(new JLabel("  Piso:"));
    modeBar.add(floorCombo);

    // Paleta de celdas
    palettePanel = new JPanel(new FlowLayout(FlowLayout.LEFT, 4, 4));
    palettePanel.setBorder(BorderFactory.createTitledBorder("Paleta celdas"));
    paletteButtons = new JButton[TILES.length];
    for (int i = 0; i < TILES.length; i++) {
        TileDef t = TILES[i];
        JButton btn = new JButton(String.valueOf(t.ch()));
        btn.setToolTipText((i + 1) + " — " + t.name());
        btn.setPreferredSize(new Dimension(44, 44));
        btn.setBackground(t.bg());
        btn.setForeground(t.fg());
        btn.setFont(new Font("Monospaced", Font.BOLD, 18));
        btn.setOpaque(true); btn.setFocusable(false);
        btn.setMargin(new Insets(0, 0, 0, 0));
        btn.addActionListener(e -> { brush = t.ch(); updatePaletteVisuals(); updateStatus(); });
        paletteButtons[i] = btn;
        palettePanel.add(btn);
    }

    // Paleta de alturas
    heightPanel = new JPanel(new FlowLayout(FlowLayout.LEFT, 4, 4));
    heightPanel.setBorder(BorderFactory.createTitledBorder("Alturas (0-" + MAX_HEIGHT + ")"));
    heightButtons = new JButton[MAX_HEIGHT + 1];
    for (int h = 0; h <= MAX_HEIGHT; h++) {
        JButton btn = new JButton(String.valueOf(h));
        btn.setPreferredSize(new Dimension(44, 44));
        btn.setBackground(heightColor(h));
        btn.setForeground((h >= 3) ? Color.BLACK : Color.WHITE);
        btn.setFont(new Font("Monospaced", Font.BOLD, 18));
        btn.setOpaque(true); btn.setFocusable(false);
        btn.setMargin(new Insets(0, 0, 0, 0));
        final int hv = h;
        btn.addActionListener(e -> { brushHeight = hv; updatePaletteVisuals(); updateStatus(); });
        heightButtons[h] = btn;
        heightPanel.add(btn);
    }

    var palettes = new JPanel(new FlowLayout(FlowLayout.LEFT, 4, 0));
    palettes.add(modeBar);
    palettes.add(palettePanel);
    palettes.add(heightPanel);
    topBar.add(palettes, BorderLayout.WEST);

    // ----- Mapa -----
    mapLabel = new JLabel(new ImageIcon(buffer));
    mapLabel.setHorizontalAlignment(SwingConstants.LEFT);
    mapLabel.setVerticalAlignment(SwingConstants.TOP);
    mapLabel.setPreferredSize(new Dimension(mapW * cellPx(), mapH * cellPx()));
    mapLabel.setFocusable(true);
    mapLabel.setOpaque(true);

    mapLabel.addMouseListener(new MouseAdapter() {
        @Override public void mousePressed(MouseEvent e) {
            int x = e.getX() / cellPx();
            int y = e.getY() / cellPx();
            if (x < 0 || y < 0 || x >= mapW || y >= mapH) return;
            mapLabel.requestFocusInWindow();

            char existing = cells[y][x];

            // Click derecho = editar cartel/NPC si existe
            if (SwingUtilities.isRightMouseButton(e)) {
                if (existing == 'T') { editSignAt(x, y); refreshBuffer(); updateStatus(); return; }
                if (existing == 'N') { editNpcAt(x, y); refreshBuffer(); updateStatus(); return; }
            }

            if (!SwingUtilities.isLeftMouseButton(e)) return;

            // Shift+click = selección
            if (e.isShiftDown()) {
                if (!selectionActive) { anchorX = x; anchorY = y; }
                selX0 = anchorX; selY0 = anchorY;
                selX1 = x; selY1 = y;
                selectionActive = true;
                refreshBuffer(); updateStatus();
                return;
            }

            // Click normal
            anchorX = x; anchorY = y;
            cursorX = x; cursorY = y;
            clearSelection();

            // Si el brush es distinto del tile y hay T/N, ¿editar?
            // Regla: si pintas con brush normal encima de T/N existente,
            // abre editor en vez de sustituir. Para sustituir, usa el
            // brush correspondiente ('T' o 'N').
            if ((existing == 'T' && brush != 'T' && brush != '.') ||
                (existing == 'N' && brush != 'N' && brush != '.')) {
                if (existing == 'T') editSignAt(x, y);
                else                 editNpcAt(x, y);
                refreshBuffer(); updateStatus();
                return;
            }

            beginOp();
            paintCell(x, y);
            commitOp();
            refreshBuffer(); updateStatus();
        }
        @Override public void mouseReleased(MouseEvent e) {
            commitOp();
            refreshBuffer();
            updateStatus();
        }
    });
    mapLabel.addMouseMotionListener(new MouseAdapter() {
        @Override public void mouseDragged(MouseEvent e) {
            int x = e.getX() / cellPx();
            int y = e.getY() / cellPx();
            if (x < 0 || y < 0 || x >= mapW || y >= mapH) return;

            if (e.isShiftDown()) {
                selX1 = x; selY1 = y;
                selectionActive = true;
                refreshBuffer(); updateStatus();
                return;
            }
            if (x == cursorX && y == cursorY) return;
            cursorX = x; cursorY = y;
            paintCell(x, y);
            refreshBuffer(); updateStatus();
        }
    });
    mapLabel.addKeyListener(new KeyAdapter() {
        @Override public void keyPressed(KeyEvent e) { handleKey(e); }
    });

    var scroll = new JScrollPane(mapLabel);
    scroll.getVerticalScrollBar().setUnitIncrement(20);
    scroll.getHorizontalScrollBar().setUnitIncrement(20);
    scroll.addMouseWheelListener(e -> {
        if (e.isControlDown()) {
            setZoom(zoomIdx + (e.getWheelRotation() < 0 ? 1 : -1));
            e.consume();
        }
    });

    statusLabel = new JLabel(" ");
    statusLabel.setBorder(BorderFactory.createEmptyBorder(4, 8, 4, 8));
    statusLabel.setFont(new Font("Monospaced", Font.PLAIN, 12));

    frame.setLayout(new BorderLayout());
    frame.add(topBar, BorderLayout.NORTH);
    frame.add(scroll, BorderLayout.CENTER);
    frame.add(statusLabel, BorderLayout.SOUTH);

    frame.setMinimumSize(new Dimension(600, 400));
    frame.setSize(
        Math.max(950, mapW * cellPx() + 100),
        Math.max(720, mapH * cellPx() + 220));
    frame.setLocationRelativeTo(null);
    frame.addWindowListener(new WindowAdapter() {
        @Override public void windowClosing(WindowEvent e) { tryClose(); }
    });

    frame.setVisible(true);
    mapLabel.requestFocusInWindow();
    updatePaletteVisuals();
    updateStatus();
}

// ==================== EDICIÓN DE MARCADORES ====================
void editMarkerAtCursor() {
    char c = cells[cursorY][cursorX];
    if (c == 'T') editSignAt(cursorX, cursorY);
    else if (c == 'N') editNpcAt(cursorX, cursorY);
    else status("No hay cartel/NPC aquí");
    refreshBuffer(); updateStatus();
}

void removeMarkerAtCursor() {
    char c = cells[cursorY][cursorX];
    if (c == 'T') {
        SignEntry s = findSign(cursorX, cursorY);
        if (s != null) { signs.remove(s); status("Cartel eliminado"); }
    } else if (c == 'N') {
        NpcEntry n = findNpc(cursorX, cursorY);
        if (n != null) { npcs.remove(n); status("NPC eliminado"); }
    } else {
        status("No hay cartel/NPC aquí");
        return;
    }
    beginOp(); touch(cursorX, cursorY); cells[cursorY][cursorX] = '.'; commitOp();
    dirty = true;
    refreshBuffer(); updateStatus();
}

void clearSelectionAndRefresh() {
    clearSelection();
    refreshBuffer();
    updateStatus();
}

void updatePaletteVisuals() {
    if (paletteButtons != null) {
        for (int i = 0; i < TILES.length; i++) {
            JButton b = paletteButtons[i];
            if (!heightMode && TILES[i].ch() == brush)
                b.setBorder(BorderFactory.createLineBorder(Color.YELLOW, 3));
            else
                b.setBorder(BorderFactory.createLineBorder(Color.DARK_GRAY, 1));
        }
    }
    if (heightButtons != null) {
        for (int h = 0; h < heightButtons.length; h++) {
            JButton b = heightButtons[h];
            if (heightMode && h == brushHeight)
                b.setBorder(BorderFactory.createLineBorder(Color.YELLOW, 3));
            else
                b.setBorder(BorderFactory.createLineBorder(Color.DARK_GRAY, 1));
        }
    }
}

void setZoom(int newIdx) {
    newIdx = Math.max(0, Math.min(ZOOM_LEVELS.length - 1, newIdx));
    if (newIdx == zoomIdx) return;
    zoomIdx = newIdx;
    refreshBuffer();
    updateStatus();
    status("Zoom: " + ZOOM_NAMES[zoomIdx]);
}

void tryClose() {
    if (!dirty) { frame.dispose(); System.exit(0); return; }
    int r = JOptionPane.showConfirmDialog(frame,
        "Hay cambios sin guardar. ¿Guardar antes de salir?",
        "Salir", JOptionPane.YES_NO_CANCEL_OPTION, JOptionPane.QUESTION_MESSAGE);
    if (r == JOptionPane.YES_OPTION) {
        if (doSave()) { frame.dispose(); System.exit(0); }
    } else if (r == JOptionPane.NO_OPTION) {
        frame.dispose();
        System.exit(0);
    }
}

JMenuItem menuItem(String label, int vk, Runnable action) {
    JMenuItem item = new JMenuItem(label);
    if (vk >= 0) item.setAccelerator(KeyStroke.getKeyStroke(vk, InputEvent.CTRL_DOWN_MASK));
    item.addActionListener(e -> action.run());
    return item;
}

// ==================== ARCHIVO ====================
void doNew() {
    String w = JOptionPane.showInputDialog(frame, "Ancho (5-200):", "25");
    if (w == null) return;
    int nw; try { nw = Integer.parseInt(w.trim()); } catch (Exception ex) { status("Ancho inválido"); return; }
    if (nw < 5 || nw > 200) { status("Ancho inválido"); return; }

    String h = JOptionPane.showInputDialog(frame, "Alto (5-200):", "20");
    if (h == null) return;
    int nh; try { nh = Integer.parseInt(h.trim()); } catch (Exception ex) { status("Alto inválido"); return; }
    if (nh < 5 || nh > 200) { status("Alto inválido"); return; }

    if (dirty) {
        int r = JOptionPane.showConfirmDialog(frame,
            "Hay cambios sin guardar. ¿Descartar?", "Nuevo",
            JOptionPane.YES_NO_OPTION, JOptionPane.WARNING_MESSAGE);
        if (r != JOptionPane.YES_OPTION) return;
    }
    newMap(nw, nh);
    status("Nuevo mapa " + nw + "x" + nh);
    if (mapLabel != null) {
        mapLabel.setPreferredSize(new Dimension(mapW * cellPx(), mapH * cellPx()));
        mapLabel.revalidate();
    }
    frame.setTitle("Map Editor - " + currentPath);
}

void doOpen() {
    var chooser = new JFileChooser("assets/levels");
    chooser.setFileFilter(new FileNameExtensionFilter("Mapas (.dgn)", "dgn"));
    if (chooser.showOpenDialog(frame) != JFileChooser.APPROVE_OPTION) return;
    if (dirty) {
        int r = JOptionPane.showConfirmDialog(frame,
            "Hay cambios sin guardar. ¿Descartar?", "Abrir",
            JOptionPane.YES_NO_OPTION, JOptionPane.WARNING_MESSAGE);
        if (r != JOptionPane.YES_OPTION) return;
    }
    loadFromPath(chooser.getSelectedFile().getPath());
    if (mapLabel != null) {
        mapLabel.setPreferredSize(new Dimension(mapW * cellPx(), mapH * cellPx()));
        mapLabel.revalidate();
    }
    frame.setTitle("Map Editor - " + currentPath);
}

boolean doSave() {
    if (currentPath == null || currentPath.isEmpty()) return doSaveAs();
    return saveToPath(currentPath);
}

boolean doSaveAs() {
    var chooser = new JFileChooser("assets/levels");
    chooser.setFileFilter(new FileNameExtensionFilter("Mapas (.dgn)", "dgn"));
    if (currentPath != null && !currentPath.isEmpty()) {
        try { chooser.setSelectedFile(new File(currentPath)); } catch (Exception ignored) {}
    }
    if (chooser.showSaveDialog(frame) != JFileChooser.APPROVE_OPTION) return false;
    String path = chooser.getSelectedFile().getPath();
    if (!path.endsWith(".dgn")) path += ".dgn";
    boolean ok = saveToPath(path);
    if (ok) frame.setTitle("Map Editor - " + path);
    return ok;
}

void doClear() {
    int r = JOptionPane.showConfirmDialog(frame,
        "¿Seguro que quieres vaciar el mapa?", "Limpiar",
        JOptionPane.YES_NO_OPTION, JOptionPane.WARNING_MESSAGE);
    if (r != JOptionPane.YES_OPTION) return;
    beginOp();
    for (int y = 0; y < mapH; y++) for (int x = 0; x < mapW; x++) touch(x, y);
    for (int y = 0; y < mapH; y++) for (int x = 0; x < mapW; x++)
        cells[y][x] = '.';
    signs.clear();
    npcs.clear();
    commitOp();
    dirty = true;
    refreshBuffer();
    updateStatus();
    status("Mapa vaciado");
}

// ==================== HERRAMIENTAS ====================
void doEnemies() {
    String s = JOptionPane.showInputDialog(frame, "Cantidad de enemigos:", "5");
    if (s == null) return;
    int n; try { n = Integer.parseInt(s.trim()); } catch (Exception ex) { status("Cantidad inválida"); return; }
    if (n <= 0) { status("Cantidad inválida"); return; }
    var floorCells = new ArrayList<int[]>();
    var protectedCells = new ArrayList<int[]>();
    for (int y = 0; y < mapH; y++) for (int x = 0; x < mapW; x++) {
        char c = cells[y][x];
        if (c == '.') floorCells.add(new int[]{x, y});
        else if (c == 's' || c == 'f') protectedCells.add(new int[]{x, y});
    }
    Collections.shuffle(floorCells);
    beginOp();
    int placed = 0;
    for (int[] c : floorCells) {
        if (placed >= n) break;
        boolean tooClose = false;
        for (int[] p : protectedCells) {
            int dx = c[0] - p[0], dy = c[1] - p[1];
            if (dx * dx + dy * dy < 25) { tooClose = true; break; }
        }
        if (tooClose) continue;
        touch(c[0], c[1]);
        cells[c[1]][c[0]] = 'e';
        placed++;
    }
    commitOp();
    dirty = true;
    refreshBuffer(); updateStatus();
    status("Enemigos: " + placed + "/" + n);
}

void doItems() {
    String s = JOptionPane.showInputDialog(frame, "Cantidad de items:", "5");
    if (s == null) return;
    int n; try { n = Integer.parseInt(s.trim()); } catch (Exception ex) { status("Cantidad inválida"); return; }
    if (n <= 0) { status("Cantidad inválida"); return; }
    var floorCells = new ArrayList<int[]>();
    for (int y = 0; y < mapH; y++) for (int x = 0; x < mapW; x++)
        if (cells[y][x] == '.') floorCells.add(new int[]{x, y});
    Collections.shuffle(floorCells);
    beginOp();
    int placed = 0;
    for (int[] c : floorCells) {
        if (placed >= n) break;
        touch(c[0], c[1]);
        cells[c[1]][c[0]] = 'i';
        placed++;
    }
    commitOp();
    dirty = true;
    refreshBuffer(); updateStatus();
    status("Items: " + placed + "/" + n);
}

void doLockers() {
    String s = JOptionPane.showInputDialog(frame, "Cantidad de taquillas:", "5");
    if (s == null) return;
    int n; try { n = Integer.parseInt(s.trim()); } catch (Exception ex) { status("Cantidad inválida"); return; }
    if (n <= 0) { status("Cantidad inválida"); return; }
    var candidates = new ArrayList<int[]>();
    for (int y = 0; y < mapH; y++) for (int x = 0; x < mapW; x++) {
        if (cells[y][x] != '#') continue;
        boolean adj = false;
        for (int dy = -1; dy <= 1 && !adj; dy++)
            for (int dx = -1; dx <= 1 && !adj; dx++) {
                if (dx == 0 && dy == 0) continue;
                int nx = x + dx, ny = y + dy;
                if (nx < 0 || ny < 0 || nx >= mapW || ny >= mapH) continue;
                char c = cells[ny][nx];
                if (c != '#' && c != 'L') adj = true;
            }
        if (adj) candidates.add(new int[]{x, y});
    }
    Collections.shuffle(candidates);
    beginOp();
    int placed = 0;
    for (int[] c : candidates) {
        if (placed >= n) break;
        touch(c[0], c[1]);
        cells[c[1]][c[0]] = 'L';
        placed++;
    }
    commitOp();
    dirty = true;
    refreshBuffer(); updateStatus();
    status("Taquillas: " + placed + "/" + n);
}

void doBorder() {
    beginOp();
    for (int x = 0; x < mapW; x++) {
        touch(x, 0); cells[0][x] = '#';
        touch(x, mapH - 1); cells[mapH - 1][x] = '#';
    }
    for (int y = 0; y < mapH; y++) {
        touch(0, y); cells[y][0] = '#';
        touch(mapW - 1, y); cells[y][mapW - 1] = '#';
    }
    commitOp();
    dirty = true;
    refreshBuffer(); updateStatus();
    status("Borde dibujado");
}

void doFill() {
    String s = JOptionPane.showInputDialog(frame, "Carácter para rellenar:", "#");
    if (s == null || s.isEmpty()) return;
    char ch = s.charAt(0);
    beginOp();
    for (int y = 0; y < mapH; y++) for (int x = 0; x < mapW; x++) touch(x, y);
    for (int y = 0; y < mapH; y++) for (int x = 0; x < mapW; x++) cells[y][x] = ch;
    commitOp();
    dirty = true;
    refreshBuffer(); updateStatus();
    status("Relleno con '" + ch + "'");
}

void doHelp() {
    String msg = """
        <html><body style='font-family: monospace; padding: 10px;'>
        <h3>Atajos</h3>
        <b>Movimiento:</b> Flechas / WASD / HJKL<br>
        <b>Pintar:</b> Espacio o click izquierdo<br>
        <b>Grab:</b> G — copia el tile bajo el cursor al brush<br>
        <b>Undo:</b> U o Ctrl+Z (grupos de operaciones)<br>
        <b>Brush:</b> 1-9 o click en la paleta<br>
        <b>Piso activo:</b> combo en la barra superior<br>
        <b>Alturas:</b> Tab para cambiar de modo<br>
        <b>Selección:</b> Shift+click para anclar, Shift+drag para extender<br>
        <b>Copiar/Cortar/Pegar:</b> Ctrl+C / Ctrl+X / Ctrl+V<br>
        <b>Rellenar selección:</b> Espacio con selección activa<br>
        <b>Borrar selección:</b> Supr<br>
        <b>Deseleccionar:</b> Escape<br>
        <b>Seleccionar todo:</b> Ctrl+A<br>
        <b>Zoom:</b> Ctrl+Wheel, Ctrl+= / Ctrl+-, menú Vista<br>
        <b>Cartel / NPC:</b> pinta 'T' o 'N'. Click derecho sobre uno existente
        para editar. Herramientas → Editar cartel/NPC aquí.<br>
        <b>Rampas:</b> '^' sube al N, '>' al E, 'v' al S, '&lt;' al O.
        El vecino en esa dirección debe ser MÁS ALTO para que la rampa funcione.<br>
        </body></html>
        """;
    JOptionPane.showMessageDialog(frame, msg, "Ayuda", JOptionPane.INFORMATION_MESSAGE);
}

// ==================== TECLADO ====================
void handleKey(KeyEvent e) {
    boolean ctrl = e.isControlDown();
    boolean shift = e.isShiftDown();
    int kc = e.getKeyCode();

    switch (kc) {
        case KeyEvent.VK_UP -> moveCursor(0, -1, shift);
        case KeyEvent.VK_DOWN -> moveCursor(0, 1, shift);
        case KeyEvent.VK_LEFT -> moveCursor(-1, 0, shift);
        case KeyEvent.VK_RIGHT -> moveCursor(1, 0, shift);
        case KeyEvent.VK_W -> { if (!ctrl) moveCursor(0, -1, shift); }
        case KeyEvent.VK_S -> { if (ctrl) doSave(); else moveCursor(0, 1, shift); }
        case KeyEvent.VK_A -> { if (ctrl) selectAll(); else moveCursor(-1, 0, shift); }
        case KeyEvent.VK_D -> { if (!ctrl) moveCursor(1, 0, shift); }
        case KeyEvent.VK_H -> { if (!ctrl) moveCursor(-1, 0, shift); }
        case KeyEvent.VK_J -> { if (!ctrl) moveCursor(0, 1, shift); }
        case KeyEvent.VK_K -> { if (!ctrl) moveCursor(0, -1, shift); }
        case KeyEvent.VK_L -> { if (ctrl) doClear(); else if (!shift) moveCursor(1, 0, shift); }
        case KeyEvent.VK_SPACE -> {
            if (selectionActive) {
                fillSelection();
            } else {
                beginOp();
                paintCell(cursorX, cursorY);
                commitOp();
                refreshBuffer(); updateStatus();
            }
        }
        case KeyEvent.VK_G -> { if (!ctrl) grabTile(); }
        case KeyEvent.VK_U -> { if (!ctrl && doUndo()) { refreshBuffer(); updateStatus(); } }
        case KeyEvent.VK_Z -> { if (ctrl && doUndo()) { refreshBuffer(); updateStatus(); } }
        case KeyEvent.VK_N -> { if (ctrl) doNew(); }
        case KeyEvent.VK_O -> { if (ctrl) doOpen(); }
        case KeyEvent.VK_C -> { if (ctrl) copySelection(); }
        case KeyEvent.VK_X -> { if (ctrl) cutSelection(); }
        case KeyEvent.VK_V -> { if (ctrl) pasteClipboard(); }
        case KeyEvent.VK_DELETE -> {
            if (selectionActive) {
                deleteSelection();
            } else {
                beginOp();
                char prev = cells[cursorY][cursorX];
                if (prev == 'T') {
                    SignEntry s = findSign(cursorX, cursorY); if (s != null) signs.remove(s);
                }
                if (prev == 'N') {
                    NpcEntry n = findNpc(cursorX, cursorY); if (n != null) npcs.remove(n);
                }
                touch(cursorX, cursorY);
                if (heightMode) heights[cursorY][cursorX] = 0;
                else            cells[cursorY][cursorX] = '.';
                commitOp();
                refreshBuffer(); updateStatus();
            }
        }
        case KeyEvent.VK_ESCAPE -> clearSelectionAndRefresh();
        case KeyEvent.VK_TAB -> {
            heightMode = !heightMode;
            btnCells.setSelected(!heightMode);
            btnHeights.setSelected(heightMode);
            updatePaletteVisuals();
            refreshBuffer(); updateStatus();
        }
        case KeyEvent.VK_EQUALS -> { if (ctrl) setZoom(zoomIdx + 1); }
        case KeyEvent.VK_MINUS -> { if (ctrl) setZoom(zoomIdx - 1); }
        case KeyEvent.VK_F1 -> doHelp();
        case KeyEvent.VK_SLASH -> { if (shift) doHelp(); }
        default -> {
            char c = e.getKeyChar();
            if (c >= '1' && c <= '9') {
                int idx = c - '1';
                if (idx < TILES.length) {
                    brush = TILES[idx].ch();
                    updatePaletteVisuals(); updateStatus();
                }
            } else if (heightMode && c >= '0' && c <= (char)('0' + MAX_HEIGHT)) {
                brushHeight = c - '0';
                updatePaletteVisuals(); updateStatus();
            }
        }
    }
}

void moveCursor(int dx, int dy, boolean extendSel) {
    int nx = Math.max(0, Math.min(mapW - 1, cursorX + dx));
    int ny = Math.max(0, Math.min(mapH - 1, cursorY + dy));
    cursorX = nx; cursorY = ny;
    if (extendSel) {
        if (!selectionActive) { anchorX = cursorX - dx; anchorY = cursorY - dy; }
        selX0 = anchorX; selY0 = anchorY;
        selX1 = cursorX; selY1 = cursorY;
        selectionActive = true;
    }
    refreshBuffer();
    updateStatus();
}
