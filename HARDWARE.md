# La SEEDER con batería

## Customização T-Display-S3: `2.2.3-offline-sleep`

O firmware personalizado para a **S3** entra em sono profundo após **5 minutos
sem interação**. Aos 4min30s, esconde a página atual e avisa que os dados serão
perdidos. Pressionar qualquer botão cancela o aviso e reinicia o prazo; essa
pressão e sua soltura são consumidas, sem registrar moeda, dado ou navegação.

Para dormir manualmente, segure **MOVE + OK por 3 segundos**. Quando aparecer
`RELEASE BOTH`, solte os dois. Soltar antes dos 3 segundos cancela a operação.
Para despertar, pressione **MOVE (GPIO14)** e solte. O aparelho inicia no menu;
a pressão de despertar também é consumida.

Antes de dormir, o firmware chama `wipeSeed()`, apaga a retroiluminação GPIO38,
envia os comandos de desligamento/sono ao ST7789 e mantém GPIO15 baixo para
desligar a alimentação dos periféricos. Ao acordar, libera os estados RTC/hold
antes de reinicializar a tela. Nenhum estado de carteira é salvo em flash ou RTC.
Uma captura incompleta e uma seed ainda não anotada serão perdidas.

Isso é sono profundo, não corte físico da bateria. O consumo residual não foi
medido. Os rádios continuam excluídos pela configuração offline. Esta função
é específica da S3; o alvo da T-Display clássica mantém seu comportamento.

### Indicador de bateria da S3

Somente a tela principal mostra, no canto superior direito, um plug desenhado quando há alimentação de
5 V, ou uma bateria desenhada com até quatro barras de carga estimada. Não há
caixa, texto ou percentual numérico. O fundo do cabeçalho é preservado; no menu,
os desenhos são pretos sobre o verde existente. As demais telas, incluindo captura de entropia e QR, não exibem o indicador.

As barras são uma estimativa pela tensão (3,3 V a 4,2 V). Não há medidor de
capacidade/corrente: carga elétrica, temperatura, bateria e tolerâncias do ADC
afetam a leitura. A leitura ocorre a cada 5 segundos e não reinicia o timeout.

O GPIO4 lê um divisor 100k/100k da alimentação selecionada, não um medidor de
estado de carga independente. Com tensão acima de 4,35 V, o firmware mostra
um plug, sem inventar percentual ou estado de carga. Leituras inválidas/ambíguas
não exibem indicador. O sinal CHRG do TP4065 vai apenas ao LED
vermelho no esquema oficial; não está ligado a um GPIO. O estado real de
carregamento exige observar esse LED: aceso ao carregar, apagado ao completar.
USB presente também pode manter o LED aceso/piscando sem bateria.

Referências oficiais:
- https://github.com/Xinyuan-LilyGO/T-Display-S3
- https://github.com/Xinyuan-LilyGO/T-Display-S3/blob/main/schematic/T_Display_S3.pdf
- https://github.com/Xinyuan-LilyGO/T-Display-S3/blob/main/examples/GetBatteryVoltage/GetBatteryVoltage.ino

Teste da política, sem hardware:

```sh
g++ -std=c++11 -Wall -Wextra -Werror tests/power_policy_test.cpp -o /tmp/seeder-power-test
/tmp/seeder-power-test
```

**As seções abaixo descrevem a T-Display clássica. Os pinos e valores de consumo
citados nelas não devem ser aplicados à S3.**

La T-Display admite una LiPo por su conector JST 1.25, pero **no trae interruptor**:
la enchufas y está encendida para siempre. Este documento recoge lo que hay que
saber para montarla con batería, incluida una función de la placa que no está
documentada en ningún sitio.

---

## Sí se puede apagar: PWR_EN es GPIO14

La T-Display tiene un MOSFET que corta la batería, y el ESP32 puede accionarlo.
Sale del [esquemático oficial](https://github.com/Xinyuan-LilyGO/TTGO-T-Display/blob/master/schematic/ESP32-TFT(6-26).pdf),
donde la red `PWR_EN` está en el **pin 17 del módulo, `MTMS/IO14`**:

```
PWR_EN (GPIO14) ──D6──┐
                      ├── R25 10K ──► base Q3 (PMBT3904)     R27 100K a GND
VBUS ─────────────D1──┘
                            colector Q3 ── R26 1K ──► puerta de Q4 y Q6

BAT ──► Q4 (SI2301, canal P) ──► IN del AP2112K-3.3V ──► VDD3V3
              R28 100K entre la puerta y el lado del regulador
```

`Q4` es el interruptor de la batería. Su puerta la manda `Q3`, que conduce si hay
`VBUS` **o** si `PWR_EN` está alto.

En la práctica:

```cpp
pinMode(14, OUTPUT);
digitalWrite(14, HIGH);   // engancha la alimentación al arrancar
...
digitalWrite(14, LOW);    // corta la batería
```

### Dos advertencias

**Por USB no se puede apagar, y esto es seguro.** `D1` mete VBUS directamente en
la base de `Q3`, así que mientras haya cable, `Q3` conduce pase lo que pase con
GPIO14. El corte sólo tiene efecto con la placa alimentada sólo por batería.

**El enganche es flojo, y esto está sin verificar.** `R28` lleva la puerta de `Q4`
al lado del regulador, no al de la batería. Eso hace que la placa arranque sola al
conectar la LiPo, pero también que al soltar `PWR_EN` la puerta suba siguiendo al
propio raíl que está alimentando: puede cortar limpiamente o quedarse en una zona
intermedia y rearrancar. En el repo de LilyGO hay un
[hilo abierto](https://github.com/Xinyuan-LilyGO/TTGO-T-Display/issues/6) de alguien
que reporta justo eso, sin respuesta.

**Si lo pruebas, cuéntanoslo.** Es la clase de dato que no está escrito en ninguna
parte.

---

## La alternativa segura: un interruptor

Para una unidad que va a estar guardada, lo que no falla es un **interruptor
deslizante en el positivo del JST**. Cero consumo, y en un generador de semillas
"apagado" de verdad vale más que "durmiendo". La carcasa de `3d files/` tiene sitio.

---

## Sueño profundo, y por qué aquí importa

Si no cortas la corriente, la otra vía es el *deep sleep* del ESP32, despertando con
un botón. El detalle que casi todo el mundo se salta: **si no apagas la pantalla se
siguen yendo unos 10 mA** aunque el chip duerma. Hay que apagarla a mano:

```cpp
digitalWrite(TFT_BL, LOW);                    // GPIO4, retroiluminación
tft.writecommand(ST7789_DISPOFF);             // apagar el panel
tft.writecommand(ST7789_SLPIN);               // dormir el controlador
esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);  // despertar con OK
esp_deep_sleep_start();
```

Con eso se llega a **~380 µA**, [medidos por la comunidad](https://www.paleotechnologist.net/?p=5041).
Con una LiPo de 500 mAh son unos **55 días** en reposo, frente a dos días si te
olvidas del panel. No es cero: para meses en un cajón, interruptor.

Los dos botones sirven para despertar. Los pines RTC del ESP32 son
[`0, 2, 4, 12-15, 25-27, 32-39`](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/sleep_modes.html),
y tanto `GPIO0` como `GPIO35` están dentro.

### Y para este aparato en concreto, dormir es borrar

La documentación de Espressif lo dice de las modalidades de sueño profundo: *las CPU,
la mayor parte de la RAM y todos los periféricos digitales se apagan*. Sólo sobrevive
la memoria RTC, y la SEEDER no guarda nada ahí.

O sea que **entrar en sueño profundo destruye la semilla por hardware**, sin depender
de que el `memzero` del firmware esté bien escrito. Para un aparato cuya premisa es
no fiarse del firmware, eso no es un detalle menor.

---

## Un fallo conocido

Hay un [hilo abierto](https://github.com/Xinyuan-LilyGO/TTGO-T-Display/issues/111)
donde, tras descargar la batería del todo, la placa "olvida" que podía despertar por
GPIO y sólo revive con un reset manual. Sin causa identificada. Tenlo presente si
montas una unidad para dejarla guardada mucho tiempo.

---

## Medir la batería

El divisor de tensión de la batería está en **GPIO34** (`ADC_IN` en el pinout de
LilyGO). No lo usamos, pero está ahí si alguien quiere un indicador de carga.
