# Plano — Data Logger STM32F107 (SD/SPI + FatFs + TinyUSB + App Python)

## Contexto

O projeto `stm32_datalogger` (STM32F107VCTx, Cortex-M3 @72 MHz, 256 KB Flash / 64 KB RAM, FreeRTOS) hoje só tem o scaffold CubeMX: ADC1 (potenciômetro PC4/CH14), RTC calendário (LSI), SPI1 já mapeado para um cartão SD (PA5/6/7 + CS PA4), botões (PD11–15, PB7) e LEDs (PE8–15). **Nenhuma lógica de aplicação existe ainda** e faltam FatFs e qualquer pilha USB.

O objetivo é transformá-lo num data logger real: registrar **mudanças de estado nas entradas digitais** e o **valor do ADC a 1 Hz**, ambos com **timestamp do RTC**, gravando em **cartão SD (SPI + FatFs)**; comunicação por **USB (TinyUSB CDC)** com um **protocolo** próprio; uma **aplicação Python** em `App/`; e o conjunto de **melhorias de mercado** escolhidas: LSE+VBAT, confiabilidade (IWDG/CRC/LEDs/self-test), logging inteligente (limiares, taxa configurável, oversampling, CONFIG.TXT) e **USB Mass-Storage + streaming ao vivo**.

### Decisões confirmadas com o usuário
- **Features:** todas as 4 (LSE+VBAT · Confiabilidade · Smart logging · USB-drive+stream).
- **Formato no cartão:** binário por padrão **+** CSV-direto, selecionável via `CONFIG.TXT`.
- **Hardware:** cristal **LSE populado**, **VBAT ligado**, **card-detect em PE0 ligado**; botões **não** são active-low → tratados como active-high / pull externo, log de transições agnóstico à polaridade.

### Decisões técnicas adotadas (internas)
- FreeRTOS: alocação **híbrida** (buffers grandes estáticos em `.bss`, tasks/filas dinâmicas), `configTOTAL_HEAP_SIZE = 0x6000` (24 KB), `configUSE_NEWLIB_REENTRANT = 0` (economiza ~150 B/task; usar formatação inteira pois nano libc não tem `%f`).
- USB: TinyUSB porta **dwc2** (`OPT_MCU_STM32F1`), init de pinos/clock/NVIC **à mão em USER CODE** (à prova de regeneração); **`HAL_PCD` permanece desabilitado**.
- diskio: adaptar o driver **ChaN mmc_spi** (ELM FatFs) — não inventar.
- Download em massa: **frames próprios com comprimento + CRC** (CRC16/frame, CRC32/arquivo).
- Sub-segundo: ler `RTC->DIVL/DIVH` (F1 não tem campo SubSeconds); com **LSE** → resolução ~30,5 µs, armazenada em **ms**.

---

## Mudanças de configuração (CubeMX / `.ioc`) — pré-requisito

Aplicar no CubeMX e regenerar (mantém `.cproject`/`sources.mk`/`.ioc` consistentes). Onde indicado, há alternativa **regen-proof** em `USER CODE` que eu posso usar sem CubeMX.

| # | Mudança | Onde | Alternativa regen-proof |
|---|---|---|---|
| 1 | **CS PA4 (`SDCARD_D3`) → `GPIO_OUTPUT_PP`, idle-high** (hoje é `INPUT`, bug p/ SD-SPI) | Pinout PA4 | re-init em `USER CODE BEGIN MX_GPIO_Init_2` |
| 2 | **RTC clock source LSI → LSE**; habilitar LSE no RCC; `AsynchPrediv = 32767` | RCC/RTC | `USER CODE BEGIN SysInit` (RCC OscConfig LSE + `__HAL_RCC_RTC_CONFIG`) |
| 3 | **USB OTG-FS 48 MHz**: adicionar `RCC_PERIPHCLK_USB` ao `PeriphClkInit` (hoje só RTC+ADC) | RCC | feito no `board_usb_init()` (USER CODE) |
| 4 | **ADC sampling time** 1.5 → ≥55.5 ciclos (impedância do pot) | ADC1 | re-config em `USER CODE BEGIN ADC1_Init 2` |
| 5 | **Pulls dos botões** PD11–15/PB7 conforme HW (default: pull-down, active-high) | Pinout | `USER CODE BEGIN MX_GPIO_Init_2` |
| 6 | **VBAT**: garantir retenção do RTC (escrita única protegida por `RTC_BKP_DRx` magic) | — | em `rtc_time.c` |

USB (PA11/PA12 AF, clock, NVIC `OTG_FS_IRQn` prio 6) será feito **inteiramente em código** (`board_usb_init()`), sem habilitar USB no CubeMX, para evitar gerar o `MX_USB_OTG_FS_PCD_Init` que conflita com o TinyUSB.

---

## Integração de middleware

- **FatFs (ChaN):** adicionar `Middlewares/Third_Party/FatFs/src/**` + `ffconf.h`. Registrar a pasta em `Debug/sources.mk` (`SUBDIRS`) e os includes no `.cproject` (blocos Debug **e** Release). `ffconf.h`: `FF_FS_TINY=1` (janela única de 512 B — maior economia de RAM), `FF_USE_LFN=0` (nomes 8.3), `FF_USE_MKFS=1` (comando FORMAT), `FF_VOLUMES=1`, `FF_MAX_SS=512`, `get_fattime()` implementado a partir do RTC.
  - *Alternativa:* CubeMX → Middleware → FATFS "User-defined" (gera `FATFS/Target/user_diskio.c`); sobrevive a regen. Adotar se preferir CubeMX.
- **TinyUSB:** adicionar subset em `Middlewares/Third_Party/tinyusb/src/`: `tusb.c`, `common/*`, `device/usbd.c`, `device/usbd_control.c`, `class/cdc/cdc_device.c`, `class/msc/msc_device.c`, `portable/synopsys/dwc2/dcd_dwc2.c`, `osal` (FreeRTOS, header-only). Registrar pastas/includes em `sources.mk`+`.cproject`. Referência: exemplo `device/cdc_msc_freertos`.

---

## Layout de módulos do firmware (novos, em `Core/`)

| Arquivo | Responsabilidade |
|---|---|
| `app_datalogger.{h,c}` | Orquestrador: cria tasks/filas/sems, máquina de estados, struct de config global |
| `board.{h,c}` | Helpers de CS/LED/card-detect, leitura do **UID 96 bits**, troca de prescaler SPI, `board_usb_init()` |
| `rtc_time.{h,c}` | `get_timestamp()→{u32 sec,u16 ms}` (CNT+DIV, leitura atômica); set/get; init LSE+VBAT magic |
| `records.{h,c}` | Schema de header/registro no cartão, CRC8/16/32, encoders binário e CSV |
| `bsp/sd_spi.{h,c}` | Primitivas SD-SPI (init lento ≤400 kHz → rápido 18 MHz, 74 clocks idle, CMD0/ACMD41) |
| `user_diskio.c` | `disk_initialize/read/write/ioctl` (adaptação ChaN) chamando `sd_spi` |
| `task_sampler.{h,c}` | Amostragem ADC na taxa configurada (default 1 Hz) + oversampling/avg → registro |
| `task_gpio.{h,c}` | Varredura/debounce 5 ms de PD11–15/PB7 → eventos de transição confirmados |
| `task_storage.{h,c}` | **Único dono** de FatFs/SPI: drena buffer→arquivo, `f_sync`, rotação, CONFIG.TXT, LS/GET/DEL/FORMAT, ponte MSC |
| `task_usb.{h,c}` | `tud_task()` + callbacks TinyUSB (CDC e MSC) |
| `proto.{h,c}` | Parser de linha ASCII + dispatch + framer de download (glue CDC↔storage) |
| `tusb_config.h`, `usb_descriptors.c` | Config TinyUSB; descritores CDC+MSC; serial = UID hex |

Edições em `USER CODE` de arquivos gerados: `stm32f1xx_it.c` → `OTG_FS_IRQHandler(){ tud_int_handler(0); }`; `main.c` → criação de objetos/`board_usb_init()`/overrides de pinos; `stm32f1xx_hal_msp.c` → (opcional) CS output.

---

## FreeRTOS — tasks & IPC

| Task | Prio | Stack | Papel |
|---|---|---|---|
| `usb_device` | AboveNormal | 1024 B | bombeia TinyUSB (atender rápido evita NAK storm) |
| `proto` | Normal | 1024 B | parse/dispatch de comandos, framing de download |
| `storage` | Normal | 1536 B | dono do FatFs/SPI: grava, sync, rotação, comandos de arquivo, ponte MSC |
| `gpio_scan` | AboveNormal | 512 B | debounce 5 ms → eventos |
| `sampler` | Normal | 512 B | ADC na taxa configurada |

IPC: `q_events` (16×12 B, produtores→storage, política *drop-newest* + registro `LOSS`), `sb_log` StreamBuffer 2 KB (staging de gravação em lote), `q_cmd` (proto→storage, comandos de arquivo), `sb_dl` StreamBuffer 1 KB (storage→proto, chunks de download com back-pressure), `mtx_state` (config/status), `mtx_sd` (recursivo, protege `sd_spi` p/ coexistência logging↔MSC), `sem_card` (card-detect→storage).

**Single-owner**: só `storage` chama FatFs; `proto` nunca toca o cartão (envia `q_cmd`, recebe via `sb_dl`). Elimina reentrância de FatFs e a corrida "USB enquanto grava SD".

**Burst de bounce limitado por design**: `gpio_scan` por *polling* com debounce (nível estável por N amostras antes de emitir). Centenas de bordas mecânicas → **1** evento; taxa máx/pino = 1/janela ⇒ `q_events` nunca transborda por bounce.

---

## Subsistemas

### SD/SPI + FatFs
SPI1 em APB2=72 MHz. Init: prescaler **/256 = 281 kHz** (≤400 kHz) durante CMD0/ACMD41; depois **/4 = 18 MHz** (fallback /8=9 MHz se houver erro). Troca via campo `BR` de `CR1` (`__HAL_SPI_DISABLE`→altera→`ENABLE`). 74 clocks idle com CS alto antes de CMD0. **Card-detect PE0** (debounced em `gpio_scan` ou EXTI0): na remoção → parar log, `f_sync`/`f_close`, `f_mount(NULL)`, LED de erro; na inserção → remount + retomar. **Rotação**: `/LOGS/LOGnnnnn.BIN` (índice em CONFIG) ou por data; cap de tamanho configurável. **`f_sync` a cada 5 s** + em rotação + em `LOG STOP`/eject (perda máx ~5 s).

### USB / TinyUSB (CDC + MSC composto)
`CFG_TUSB_MCU=OPT_MCU_STM32F1`, `CFG_TUSB_OS=OPT_OS_FREERTOS`, `CFG_TUD_CDC=1`, `CFG_TUD_MSC=1`, FIFOs CDC RX/TX 512 B, EP0 64 B. `board_usb_init()`: PA11/PA12 `AF_PP` high-speed, `__HAL_RCC_USB_OTG_FS_CLK_ENABLE()`, seleção de clock 48 MHz, `OTG_FS_IRQn` prio 6 (faixa 5–15 p/ APIs FromISR), desabilitar VBUS sensing no `GCCFG`. ISR → `tud_int_handler(0)`.

**MSC ↔ logging (parte mais delicada, fica por último):** `tud_msc_read10_cb`/`write10_cb` acessam blocos via o mesmo `sd_spi` sob `mtx_sd`. Enquanto o host tiver a unidade montada, **logging é pausado** (flag) e o FatFs do firmware é desmontado, para não corromper a FAT; ao ejetar, remount + retoma. CDC e MSC coexistem no mesmo device composto.

### RTC — LSE + VBAT + sub-segundo
Trocar fonte p/ **LSE** (cristal confirmado): `AsynchPrediv=32767` ⇒ `DIV` resolve 1/32768 ≈ **30,5 µs**, precisão de ~ppm do cristal. `get_timestamp()` lê `CNT`,`DIV`,`CNT` (repete se virou o segundo). **VBAT** confirmado ⇒ inicializar data/hora **apenas uma vez**, guardado por magic em `RTC_BKP_DRx`, para a hora sobreviver a reset/queda. Armazenar fração em **ms**. Sempre `HAL_RTC_GetTime` **antes** de `HAL_RTC_GetDate` (quirk do F1).

### Formato no cartão (binário + CSV, selecionável)
`CONFIG.TXT: format=bin|csv` (default `bin`). **Header (32 B)**: `magic "SDLG"|ver|rec_size|epoch_base|session_id|flags|fw_ver|reserved|hdr_crc16`. **Registro (12 B)**: `t_sec u32 | t_ms u16 | type u8 (0=ADC,1=GPIO,2=MARKER,3=LOSS) | chan u8 | value u16 | seq u8 | crc8`. `seq` detecta lacunas; `LOSS` carrega contagem perdida. Modo CSV-direto grava ISO-8601 (`timestamp,source,chan,value,event`) com formatação inteira (ms manual, sem `%f`). Python decodifica binário e exporta CSV idêntico.

### Protocolo (sobre CDC) — ASCII linha + download em frames
Pedido→`OK ...`/`ERR <cod> <msg>`, terminado em `\n`, comando ≤128 B.

| Comando | Resposta | Função |
|---|---|---|
| `*IDN?` | `OK <vendor>,<prod>,<fw>,<uid96hex>` | identidade + UID |
| `TIME?` / `TIME <epoch>` | `OK <epoch>.<ms>` / `OK` | ler / sincronizar RTC |
| `STAT?` | `OK logging=.. card=.. free=..KB used=.. file=.. rate=.. lost=..` | status/self-test |
| `LOG START`/`LOG STOP` | `OK` | liga/desliga logging |
| `RATE <ms>` | `OK` | intervalo de amostragem |
| `THR <ch> <lo> <hi>` | `OK` | limiar/alarme do ADC |
| `LS [path]` | `FILE <nome> <tam> <mtime>`… `OK` | listar |
| `GET <nome>` | `OK <tam> <crc32>` → frames → `DONE` | download |
| `DEL <nome>` / `FORMAT <token>` | `OK`/`ERR` | apagar / formatar (token de guarda) |
| `STREAM ON/OFF` | `OK` + linhas `#ADC`/`#EVT` | monitor ao vivo |
| `CFG?` / `CFG k=v` | `OK ...` | ler/gravar CONFIG.TXT |

Códigos de erro: 1 args, 2 sem-cartão, 3 não-encontrado, 4 ocupado, 5 io, 6 crc, 7 não-suportado. **Download**: frames `SOF(0x7E)|seq u32|len u16|payload|crc16`; host valida `crc16`/frame e `crc32`/arquivo, pede `RESEND <seq>` se falhar. Flux-control por `sb_dl`. Mensagens assíncronas de `STREAM` prefixadas com `#` para o parser separar de `OK/ERR`; nunca intercaladas em download.

### Features de mercado
- **Confiabilidade:** IWDG (refresh por task idle/baixa prio); CRC8+seq por registro & CRC32 por arquivo (já no schema); **LEDs PE8–15** (logging/erro/cartão/USB); **self-test no boot** reportado em `STAT?`.
- **Smart logging:** `THR` (eventos por limiar do ADC); `RATE` (taxa configurável); oversampling/média no `sampler`; `CONFIG.TXT` persistido no cartão (carregado no mount).
- **Identidade:** UID 96 bits no `*IDN?` e no serial USB (endereça múltiplas placas).

---

## Aplicação Python (`App/`)

```
App/
  pyproject.toml          deps: pyserial; opcional: matplotlib, pytest
  README.md
  datalogger/
    transport.py    abre serial; auto-detecta por VID/PID (list_ports); RW+timeout
    protocol.py     encoders + parser OK/ERR; erro→exceção; demux de linhas '#'
    download.py     recebe frames; CRC16/frame + CRC32/arquivo; RESEND/retry
    records.py      decodifica header+registros (struct)→dataclasses; export CSV; verifica CRC
    timesync.py     hora do PC → `TIME <epoch>` (correção de round-trip)
    liveplot.py     (opcional) plot ao vivo do STREAM
    cli.py          subcomandos → protocolo
  tests/  test_records.py, test_protocol.py
```
CLI: `info, status, time [--set], start, stop, rate, thr, ls, get <n> [--verify], del, stream [--plot], format, export <bin> <csv>`. VID/PID de desenvolvimento: **pid.codes 0x1209/0x0001** (trocar antes de distribuir). Serial USB = UID ⇒ `--serial` seleciona a placa.

---

## Orçamento de recursos (de 64 KB RAM / 256 KB Flash)

RAM alvo ≈ **40 KB** (≈24 KB de folga): kernel/CRT ~5,6 KB · app/config ~1 KB · TinyUSB ~2,5 KB · FatFs (TINY) ~1,5 KB · buffers IPC ~3,3 KB · **heap FreeRTOS 24 KB** · stack MSP `0x600` · heap newlib `0x200`. Verificar com `xPortGetFreeHeapSize()` + `uxTaskGetStackHighWaterMark()` e ajustar heap p/ baixo se sobrar. Flash atual 23,7 KB → estimado ~120–160 KB com tudo (folga confortável). Bump no linker: `_Min_Stack_Size 0x400→0x600`.

---

## Fases de implementação (incrementais, cada uma verificável)

1. **Fundação/HW**: CS→output, RTC→LSE+VBAT, USB clock no PeriphClk, ADC sampling time, pulls dos botões; crescer heap; `configUSE_NEWLIB_REENTRANT=0`. ✔ compila; RTC mantém hora no VBAT; ADC lê coerente.
2. **SD/FatFs**: `sd_spi` + `user_diskio` (troca de velocidade) + `ffconf.h` + `get_fattime`. ✔ monta cartão, cria/escreve/lê arquivo, `f_sync`.
3. **Engine de log**: `rtc_time` (sub-seg), `records` (bin+CSV+CRC), `sampler` (1 Hz), `gpio_scan` (debounce), `storage` (buffer→arquivo, rotação, CONFIG.TXT). ✔ arquivo cresce com ADC@1 Hz + eventos de botão, timestamps corretos.
4. **USB CDC + protocolo**: TinyUSB CDC, `task_usb`, `proto`, comandos, download em frames. ✔ enumera COM; `*IDN?/STAT?/TIME/LS/GET/DEL` via terminal e Python.
5. **App Python**: `transport/protocol/download/records/timesync/cli` + testes. ✔ fluxo ponta-a-ponta do PC (status, sync, download+CSV verificados por CRC).
6. **Confiabilidade + smart**: IWDG, LEDs, self-test, `THR`/alarmes, `RATE`, oversampling, `STREAM`+liveplot. ✔ cada um isolado.
7. **USB-MSC composto (último, mais difícil)**: CDC+MSC, arbitragem de cartão (pausa logging durante sessão MSC, `mtx_sd`, desmonta/remonta FatFs). ✔ cartão aparece como unidade, arquivos legíveis, logging retoma após eject, sem corrupção.

---

## Verificação ponta-a-ponta
- **Build/flash/debug** com os scripts existentes (`.\build.ps1`, `.\flash.ps1`, `.\debug.ps1`; probe J-Link). Trace de debug por **SWO/ITM** (PB3 já mapeado) para `printf` de diagnóstico.
- **Cartão**: gravar ≥10 min, remover, confirmar no PC (binário via `app export`; CSV abrindo direto) — checar continuidade de `seq` e CRCs.
- **Entradas**: acionar cada botão, conferir um evento por transição (sem bounce) com timestamp sub-segundo plausível.
- **ADC**: variar o potenciômetro, confirmar amostras a 1 Hz e disparo de alarme por `THR`.
- **USB**: enumeração CDC (COM) + Python (`info/status/time --set/ls/get --verify`); depois MSC (unidade no Explorer), com logging pausando/retomando no eject.
- **RAM**: logar `xPortGetFreeHeapSize()` e high-water das tasks no `STAT?`.

---

## Riscos & suposições
- **Botões**: assumidos active-high/pull externo (usuário não marcou active-low) e log agnóstico à polaridade; pull interno é ponto único de config — confirmar no bring-up.
- **MSC+logging** no mesmo cartão é o maior risco de corrupção → mutualmente exclusivos por design (fase 7).
- **Init SPI ≤400 kHz** e 74 clocks idle são a causa clássica de "cartão não inicializa" — usar sequência ChaN.
- **Regeneração CubeMX** pode remover TinyUSB do build (fora do conjunto gerenciado) e reverter overrides fora de `USER CODE`; tudo crítico fica em `USER CODE`/módulos próprios e está documentado.
- **nano libc sem `%f`** → toda formatação numérica é inteira (ms montados à mão).
