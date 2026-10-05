# ESPHome AP33772S USB‑PD Output

🇬🇧 [English](#english) · 🇩🇪 [Deutsch](#deutsch)

---

## English

An [ESPHome](https://esphome.io) external component for the **AP33772S** USB Power Delivery sink controller. It exposes the AP33772S as a **float output**, so you can set the voltage a USB‑C PD power supply delivers from ESPHome / Home Assistant.

### Features

- Reads the source capabilities (fixed PDOs and PPS APDOs) from the AP33772S over I²C
- **PPS mode** (preferred): continuously adjustable voltage in 100 mV steps
- **Fixed mode** (fallback): switches between the fixed PDO voltages the charger offers
- Upper voltage limit (default 12 V); PDOs above the limit are ignored
- PPS keep‑alive: the PPS request is repeated every 500 ms
- Logs output voltage, current, power and negotiated voltage every 3 s
- Retries PDO detection up to 20 times (every 500 ms) at startup

### Installation

```yaml
external_components:
  - source: github://maggo1404/esphome-ap33772s
    components: [ap33772s]
```

### Configuration

```yaml
i2c:
  sda: GPIO21
  scl: GPIO22

output:
  - platform: ap33772s
    id: usb_pd
    address: 0x52   # optional, default 0x52

# Example: control the voltage with a slider in Home Assistant
light:
  - platform: monochromatic
    name: "USB-PD Voltage"
    output: usb_pd
    gamma_correct: 1.0
    default_transition_length: 0s
```

Any ESPHome component that drives a float output (light, fan, `output.set_level` action, …) can be used.

#### Options

| Option    | Required | Default | Description                    |
|-----------|----------|---------|--------------------------------|
| `id`      | yes      | –       | ID of the output               |
| `address` | no       | `0x52`  | I²C address of the AP33772S    |

Plus all standard [float output](https://esphome.io/components/output/) options (`min_power`, `max_power`, `inverted`, …).

### How the output level maps to voltage

**PPS mode** – active if the source offers a PPS APDO whose maximum is at least `min + 1 V`:

```
voltage = min + level × (min(PPS_max, max) − min)    rounded down to 100 mV
```

Level `0.0` → 5 V, level `1.0` → maximum PPS voltage (capped at 12 V).

**Fixed mode** – the usable fixed PDOs (≤ max) are sorted by voltage and the range 0…1 is split into equal steps. Example with 5 V / 9 V / 12 V: `0.00–0.33` → 5 V, `0.33–0.66` → 9 V, `0.66–1.00` → 12 V.

#### Defaults

| Parameter     | Default  |
|---------------|----------|
| Min. voltage  | 5000 mV  |
| Max. voltage  | 12000 mV |
| Prefer PPS    | yes      |

These values are currently set in the C++ code (`ap33772s.h`) and are not yet exposed as YAML options.

### Measurements

Every 3 s the component logs:

```
[I][ap33772s]: VOUT=9040 mV  I=480 mA  P=4339 mW  (VREQ=9000 mV)
```

| Register | Value                | Resolution |
|----------|----------------------|------------|
| `0x11`   | output voltage VOUT  | 80 mV      |
| `0x12`   | output current IOUT  | 24 mA      |
| `0x14`   | negotiated voltage   | 50 mV      |

The values are available in C++ via `get_measured_mv()`, `get_current_ma()`, `get_requested_mv()` and `get_power_mw()`, e.g. in a template sensor:

```yaml
sensor:
  - platform: template
    name: "USB-PD Voltage"
    unit_of_measurement: V
    accuracy_decimals: 2
    update_interval: 5s
    lambda: return id(usb_pd).get_measured_mv() / 1000.0;
```

### ⚠️ Safety

The voltage on VBUS can rise to the configured maximum. Make sure every connected load tolerates the highest possible voltage. Use at your own risk.

---

## Deutsch

Eine externe [ESPHome](https://esphome.io)-Komponente für den USB‑Power‑Delivery‑Sink‑Controller **Diodes AP33772S**. Der AP33772S wird als **Float‑Output** bereitgestellt, sodass die Spannung eines USB‑C‑PD‑Netzteils direkt aus ESPHome / Home Assistant eingestellt werden kann.

### Funktionen

- Liest die Fähigkeiten der Quelle (feste PDOs und PPS‑APDOs) per I²C aus dem AP33772S
- **PPS‑Modus** (bevorzugt): stufenlos einstellbare Spannung in 100‑mV‑Schritten
- **Fester Modus** (Fallback): Umschalten zwischen den festen PDO‑Spannungen des Netzteils
- Obere Spannungsgrenze (Standard 12 V); PDOs darüber werden ignoriert
- PPS‑Keep‑Alive: die PPS‑Anforderung wird alle 500 ms wiederholt
- Ausgangsspannung, Strom, Leistung und ausgehandelte Spannung werden alle 3 s geloggt
- Beim Start bis zu 20 Versuche (alle 500 ms), die PDOs zu erkennen

### Installation

```yaml
external_components:
  - source: github://maggo1404/esphome-ap33772s
    components: [ap33772s]
```

### Konfiguration

```yaml
i2c:
  sda: GPIO21
  scl: GPIO22

output:
  - platform: ap33772s
    id: usb_pd
    address: 0x52   # optional, Standard 0x52

# Beispiel: Spannung per Schieberegler in Home Assistant steuern
light:
  - platform: monochromatic
    name: "USB-PD Spannung"
    output: usb_pd
    gamma_correct: 1.0
    default_transition_length: 0s
```

Jede ESPHome‑Komponente, die einen Float‑Output ansteuert (Light, Fan, Aktion `output.set_level`, …), kann verwendet werden.

#### Optionen

| Option    | Pflicht | Standard | Beschreibung                 |
|-----------|---------|----------|------------------------------|
| `id`      | ja      | –        | ID des Outputs               |
| `address` | nein    | `0x52`   | I²C‑Adresse des AP33772S     |

Dazu alle Standardoptionen eines [Float‑Outputs](https://esphome.io/components/output/) (`min_power`, `max_power`, `inverted`, …).

### Zuordnung Ausgangswert → Spannung

**PPS‑Modus** – aktiv, wenn die Quelle einen PPS‑APDO anbietet, dessen Maximum mindestens `min + 1 V` beträgt:

```
Spannung = min + Wert × (min(PPS_max, max) − min)    auf 100 mV abgerundet
```

Wert `0.0` → 5 V, Wert `1.0` → maximale PPS‑Spannung (begrenzt auf 12 V).

**Fester Modus** – die nutzbaren festen PDOs (≤ max) werden nach Spannung sortiert und der Bereich 0…1 in gleich große Stufen aufgeteilt. Beispiel mit 5 V / 9 V / 12 V: `0,00–0,33` → 5 V, `0,33–0,66` → 9 V, `0,66–1,00` → 12 V.

#### Standardwerte

| Parameter       | Standard |
|-----------------|----------|
| Min. Spannung   | 5000 mV  |
| Max. Spannung   | 12000 mV |
| PPS bevorzugen  | ja       |

Diese Werte sind derzeit im C++‑Code (`ap33772s.h`) festgelegt und noch nicht als YAML‑Optionen verfügbar.

### Messwerte

Alle 3 s erscheint im Log:

```
[I][ap33772s]: VOUT=9040 mV  I=480 mA  P=4339 mW  (VREQ=9000 mV)
```

| Register | Wert                     | Auflösung |
|----------|--------------------------|-----------|
| `0x11`   | Ausgangsspannung VOUT    | 80 mV     |
| `0x12`   | Ausgangsstrom IOUT       | 24 mA     |
| `0x14`   | ausgehandelte Spannung   | 50 mV     |

Die Werte sind in C++ über `get_measured_mv()`, `get_current_ma()`, `get_requested_mv()` und `get_power_mw()` abrufbar, z. B. in einem Template‑Sensor:

```yaml
sensor:
  - platform: template
    name: "USB-PD Spannung"
    unit_of_measurement: V
    accuracy_decimals: 2
    update_interval: 5s
    lambda: return id(usb_pd).get_measured_mv() / 1000.0;
```

### ⚠️ Sicherheit

Die Spannung an VBUS kann bis zum eingestellten Maximum ansteigen. Stelle sicher, dass alle angeschlossenen Verbraucher die höchstmögliche Spannung vertragen. Nutzung auf eigene Gefahr.
