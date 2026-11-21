# 🌗 Brisabella

Atenuador de pantalla **ultraligero** para Windows (estilo *DimScreen*). Oscurece **todo** el escritorio por encima del mínimo de brillo de tu monitor o laptop. Se queda en segundo plano junto al reloj y arranca con Windows.

- Un solo .exe de menos de 1 MB, sin instalador, sin dependencias, **0 % de CPU** en reposo
- Windows 7 / 8 / 10 / 11 — versiones de **32 y 64 bits**
- Funciona con varios monitores a la vez (cubre todo el escritorio)
- Atajos de teclado globales y menú desde el ícono de la barra

> Nombre: **Bri**llo + I**sabella** → *Brisabella*.

## Descargar

Ve a **[Releases](../../releases)** y baja el que corresponda:

| Archivo | Para |
|---|---|
| `Brisabella-x64.exe` | Windows de 64 bits (casi todos los equipos actuales) |
| `Brisabella-x86.exe` | Windows de 32 bits |

¿No sabes cuál es? Usa `x64`; si no abre, prueba `x86`.

**Instalación:** copia el `.exe` a una carpeta fija (por ejemplo `C:\Brisabella\`) y ábrelo con doble clic. No lo ejecutes desde *Descargas* si luego vas a moverlo. Por defecto se agrega solo al inicio de Windows (se puede quitar desde el menú).

> Windows puede mostrar *SmartScreen* («Windows protegió su PC») porque el `.exe` no está firmado digitalmente. Pulsa **Más información → Ejecutar de todos modos**. El código fuente está completo en este repositorio.

## Uso

Busca el ícono ◐ junto al reloj (si no lo ves, está en la flecha **^**; arrástralo a la barra para tenerlo siempre visible). Clic en el ícono para elegir el nivel:

`0 · 10 · 20 · 30 · 40 · 50 · 60 · 70 · 80 · 90 · 96 · 100`

- **0** = Windows normal (la ventana se oculta, no hace nada)
- **96** = casi negro, apenas visible
- **100** = prácticamente negro total (nunca es 100 % opaco, así siempre se alcanza a ver algo)

### Atajos

| Atajo | Acción |
|---|---|
| `Ctrl + 1` | Más oscuro (siguiente nivel) |
| `Ctrl + 2` | Más claro (nivel anterior, hacia lo normal) |
| `Ctrl + 0` | Volver a normal (0) |

Si te quedas con la pantalla muy oscura: pulsa `Ctrl + 0`, o clic en el ícono → **0 (normal)**.

Brisabella recuerda el último nivel y lo restaura al iniciar Windows.

### Cambiar los atajos

`Ctrl+1/2` también los usan los navegadores para cambiar de pestaña; mientras Brisabella esté abierta, esos atajos serán de ella. Para usar otros: ícono → **Editar atajos…**, cambia el archivo y luego ícono → **Recargar atajos**:

```ini
[Brisabella]
Level=0
Autostart=1
Modifiers=Ctrl          ; Ctrl, Alt, Shift, Win (combinables: Ctrl+Alt)
KeyDarker=1             ; dígito, letra o F1..F24; vacío = desactivado
KeyLighter=2
KeyReset=0
```

El archivo está en `%APPDATA%\Brisabella\config.ini`.

## ¿Se ve «negro brillante» o parece que no baja nada?

Brisabella oscurece la **imagen**, pero no apaga la **luz de fondo** de la pantalla. En paneles LCD/IPS un píxel negro sigue dejando pasar algo de luz, así que ese «negro» a brillo mínimo se ve como un gris luminoso. Es un límite físico del panel: ninguna aplicación (ni con permisos de administrador) puede bajar de ahí. En pantallas **OLED** no pasa, porque el negro es píxel apagado.

- Comprueba que funciona: con el nivel en 70–90, abre una ventana blanca. Debe verse **mucho** más oscura que con el nivel en 0.
- En la mayoría de paneles LCD, los niveles **96 y 100** quedan por debajo de ese piso: el contenido se pierde en el gris del panel. El rango útil suele ser **60–90**.
- Deja el brillo real (Windows / botón del monitor) en el **mínimo** y usa Brisabella encima.
- Para bajar de verdad el brillo de un LCD, lo que funciona es un **filtro físico** (lámina de densidad neutra / protector «dimming» para pantalla).

## Limitaciones

- Es una capa sobre la pantalla (como DimScreen), no cambia el brillo físico del monitor. Las capturas de pantalla salen oscurecidas.
- Juegos en pantalla completa *exclusiva*, la pantalla de control de cuentas (UAC) y la pantalla de bloqueo no se oscurecen.
- Algunos paneles del sistema (menú Inicio, centro de notificaciones) pueden aparecer sin atenuar.

## Compilar

Los `.exe` se generan solos con **GitHub Actions** (`.github/workflows/build.yml`):

1. Cada `push` compila y deja los `.exe` en *Actions → Artifacts*.
2. Para publicar una versión: `git tag v1.0.0 && git push --tags` → se crea el **Release** con ambos `.exe` y sus SHA-256.

Compilar a mano (Linux/WSL/MSYS2 con MinGW-w64):

```bash
sudo apt install gcc-mingw-w64-x86-64 gcc-mingw-w64-i686 binutils-mingw-w64
make            # → dist/Brisabella-x64.exe y dist/Brisabella-x86.exe
```

En MSYS2 sobre Windows, instala `mingw-w64-x86_64-gcc` y `mingw-w64-i686-gcc` y ejecuta `make` pasando los nombres de tus compiladores, p. ej. `make CC64=x86_64-w64-mingw32-gcc CC32=i686-w64-mingw32-gcc`.

## Cómo funciona

Una ventana negra, semitransparente (`WS_EX_LAYERED`), siempre encima y transparente a los clics (`WS_EX_TRANSPARENT`) cubre el escritorio virtual completo. Un hook de eventos (`SetWinEventHook`) vuelve a subirla cuando cambia la ventana activa, y solo existe mientras el nivel es mayor que 0. Todo el código está en [`src/main.c`](src/main.c).

## Licencia

MIT — ver [LICENSE](LICENSE).
