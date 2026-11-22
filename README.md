# TimeGraph (Casio fx-9750GIII)

Add-in en C (usando **gint** y **fxSDK**) para graficar funciones matemáticas con **desplazamiento y variación temporal** $y = f(x, t)$ en bucle en tiempo real en la calculadora **Casio fx-9750GIII**.

## Características

- **Evaluador rápido (bytecode RPN)**: compila la expresión una sola vez y la evalúa en cada fotograma para lograr animaciones fluidas.
- **Sintaxis completa**:
  - Variables: `x`, `t`
  - Constantes: `pi`, `e`
  - Operadores: `+`, `-`, `*`, `/`, `^`, signos unarios y multiplicación implícita (ej. `2sin(x-t)`, `(x-t)(x+t)`)
  - Funciones: `sin`, `cos`, `tan`, `sqrt`, `abs`, `exp`, `ln`, `log`, `asin`, `acos`, `atan`, `floor`, `ceil`
- **3 modos de reproducción temporal**:
  - `LOOP`: bucle continuo de `Tmin` a `Tmax`.
  - `BNC`: rebote (ida y vuelta entre `Tmin` y `Tmax`).
  - `FREE`: avance continuo sin reinicio.
- **Ejemplos precargados**: onda viajera, onda estacionaria, interferencia, pulso gaussiano, sinc temporal y paquete de onda.

## Instalación

1. Descarga `TimeGraph.g1a` desde la sección **Releases** (`v1.0`).
2. Conecta la calculadora **Casio fx-9750GIII** por USB en modo memoria masiva (`F1: USB Flash`).
3. Copia el archivo `TimeGraph.g1a` en la raíz de la unidad de la calculadora.
4. Desconecta de forma segura y abre **TimeGraph** desde el menú principal (`MENU`).

## Instrucciones de uso

### 1. Editor de funciones (`y(x,t)=`)
- Usa el teclado físico para escribir números, operadores (`+`, `-`, `×`, `÷`, `^`, `x²`, `(`, `)`), `X,θ,T` (inserta `x`), `sin`, `cos`, `tan`, `ln`, `log`, `DEL` y `AC/ON`.
- Teclas de función:
  - `F1`: insertar `x`
  - `F2`: insertar `t`
  - `F3` (`FUNC`): menú de funciones y constantes
  - `F4` (`EJEM`): cargar ejemplos precargados
  - `F5` (`CONF`): configurar ventana (`Xmin`, `Xmax`, `Ymin`, `Ymax`) y parámetros temporales (`Tmin`, `Tmax`, `dt`, `Modo`)
  - `F6` (`GRAF`) o `EXE`: compilar y graficar en bucle

### 2. Pantalla de graficación animada
- `F1` o `EXE`: Pausar / Reanudar animación
- `F2` / `F3`: Disminuir / Aumentar velocidad (`dt`). Cuando está en pausa, retrocede o avanza `t` paso a paso
- `F4`: Cambiar modo de bucle (`LOOP` / `BNC` / `FREE`)
- `F5`: Reiniciar `t = Tmin`
- `F6`: Abrir configuración de ventana y tiempo
- Flechas (`▲` `▼` `◀` `▶`): Desplazar la vista
- `+` / `-`: Acercar / Alejar zoom
- `OPTN` / `VARS`: Ocultar o mostrar la barra de estado (HUD)
- `EXIT`: Volver al editor

## Cómo compilar desde el código fuente

Requiere tener instalado **fxSDK** y **gint** (por ejemplo, en Linux o WSL mediante `giteapc`).

```bash
git clone https://github.com/jbcgames/TimeGraph.git
cd TimeGraph
fxsdk build-fx
```

Esto generará el archivo `TimeGraph.g1a` en la raíz del proyecto.
