# 21 — Fase 7: Paridade de Plataforma (plano completo)

**Objetivo**: fechar os subsistemas de plataforma que restam para um **release
candidato cross-platform** — **áudio** (SFX WAV polifônico via `soundlist.txt` +
BGM MP3 de 15 faixas), **input completo** (gestos de câmera fiéis ao original:
middle-drag/Alt+RMB, wheel com `fClose`, inversão; IME básico), **screenshot**
(PrintScreen → `ScreenShot/Capture%04d.bmp`), **config de vídeo** (Config.bin,
gamma como uniform de blit, MSAA, aniso, resolução), **rede HTTP mínima**
(`BASE_GetHttpRequest` + download de guild marks) e a absorção das pendências
pequenas das Fases 5/6. Vídeos de intro não entram: `TMVideoWnd` é stub
`E_NOTIMPL` no próprio build original — já estão cortados na fonte.

> **Referência**: doc 13 (`13-roadmap.md`) definiu a Fase 7 como "input completo
> (DirectInput→SDL), IME básico, áudio (DirectSound→SDL_audio/miniaudio),
> vídeos de intro, WinInet→libcurl, screenshot, config de vídeo". Este doc 21
> expande cada item com o estudo do código original e as decisões de port.

**Pré-requisito**: Fase 6 merged (`UIBatcher`, `UIRenderer`, `GLFont`,
`TMFont3`, `SControl` tree, `SControlContainer`, `UILoader`, `RenderGeomControl`,
UI textures 512+600, input SDL básico roteado à UI, `--ui`).

**Estado final (DoD)**:
- [ ] `AudioEngine` (miniaudio): Init/Shutdown, grupos SFX/BGM, volume master
       com mapeamento DS→linear, mute.
- [ ] `soundlist.txt` parse (`index path channels`, 1-511), lazy decode WAV,
       polifonia limitada por `nChannel`, `Play/PlayIfNot/Stop/IsPlaying`.
- [ ] BGM: 15 MP3 (`m_szMusicPath`), `PlayMusic(index)` com loop, volume de
       música separado (`30*n-3000` centi-dB), troca de faixa por cena.
- [ ] Wiring de sons: UI click (33), swing/bike (9), heal (4), skills (151/152/
       156/158/160), cannon (307), chuva (101 loop), neve (113), gate (57),
       dano (21-28), inventário/efeito (31-36).
- [ ] Input: key state array por frame; gestos de câmera — middle-drag/Alt+RMB
       (`dy*0.002` pitch clamp -0.9854..0.75, `dx*0.0049` yaw wrap 2π), wheel
       zoom com `fClose` (1.2 / 2.5 montado / +Con×0.00019), Alt sem botão
       (`wheel=3*dy`), inversão `m_bCameraRot` (Config[10]), quarter-view lock
       (Config[13]).
- [ ] IME básico: `SDL_EVENT_TEXT_EDITING` (composição com underline no
       SEditableText), `SDL_SetTextInputArea` posicionando popup do OS;
       candidatos desenhados pelo OS.
- [ ] Screenshot: PrintScreen (KEY_UP) → `ScreenShot/Capture%04d.bmp`
       auto-incremento, BMP via stb_image_write, flip Y.
- [ ] Config.bin (30B, `SaveUpdatAndConfig`): read no boot + write na saída;
       resolução (tabela 11), windowed, bright, sound/music, camRot, camView.
- [ ] Pipeline FBO: cena → FBO offscreen (RGBA8+depth) → blit fullscreen com
       `uBright` (ganho linear `bright*0.02`, fiel à rampa D3D); MSAA 2/4x via
       renderbuffer multisample + resolve; aniso por sampler.
- [ ] HTTP mínimo: GET blocking HTTP/1.0 sobre socket (timeout, cap 64KB),
       `BASE_GetHttpRequest` 1:1, guild mark 632B BMP em thread (fiel a
       `Guildmark_Download`) alimentando o pipeline de render da Fase 6.
- [ ] `TMArrow` 13-tipos data-table (leftover Fase 5).
- [ ] Testes: +4 suítes (audio/http/config/input); ctest 100% verde.
- [ ] Docs 13/README atualizados; retrospectiva (§10) no doc 21.

**Duração estimada**: 2 semanas (10 dias úteis) — áudio + pipeline FBO são os
dois blocos grandes; input/HTTP/screenshot são curtos.

---

## 1. Escopo — o que entra e o que NÃO entra

### Entra

| Item | Fonte original | Linhas |
|---|---|---|
| `AudioEngine` (miniaudio: device + mixer) | `dsutil.cpp` (1076) + `DirShow.cpp` (493) | ~400 |
| `extern/miniaudio.h` (novo dep single-header, MIT/PD) | — | 1 header |
| soundlist parse + lazy WAV + polifonia | `dsutil.cpp:57-75,262-296` | ~120 |
| BGM MP3 (15 faixas, PlayMusic) | `DirShow.cpp:7-23`, `TMFieldScene.cpp:6850-6885` | ~80 |
| Sound API parity (`GetSoundAndPlay*`) | `TMUtil.cpp:47-89` | ~40 |
| Wiring de sons (call sites) | `TMFieldScene.cpp`, `TMRain/TMSnow/TMGate/TMBike`, skills | ~60 |
| Input: key array + gestos de câmera | `EventTranslator.cpp:193-461` | ~200 |
| IME básico (TEXT_EDITING + input area) | `EventTranslator.cpp` (IME fns) + SDL3 | ~80 |
| Clipboard (Ctrl+C/V/X no SEditableText) | novo (SDL clipboard) | ~30 |
| Screenshot (`CaptureScreen`) | `D3DDevice.cpp:1050-1075`, `NewApp.cpp:953` | ~80 |
| Config.bin read/write | `NewApp.cpp:131-220`, `NewApp.h:13-16` | ~120 |
| Pipeline FBO + blit gamma + MSAA + aniso | `RenderDevice.cpp:288-310,717-740` | ~250 |
| HTTP mínimo + guild mark thread | `Basedef.cpp:396-424`, `TMFieldScene.cpp:24422-24490` | ~150 |
| `TMArrow` 13-tipos data-table | `TMArrow.cpp` (811) | ~80 |
| Testes (test_audio, test_http, test_config, test_input) | — | ~400 |

### NÃO entra (defer explícito)

| Item | Motivo | Vai para |
|---|---|---|
| **Vídeos de intro** (libmpv) | `TMVideoWnd` é 100% `E_NOTIMPL` no build original (TMVideoWnd.cpp:12-51) — não há o que portar | — |
| **libcurl** | 3 call sites HTTP/1.0 simples (GET de ≤64KB, sem HTTPS); peso da dependência não se justifica | Fase 8 se surgir HTTPS |
| **Netcode de jogo** (login, packets, CPSock connect) | Sem servidor-alvo nesta fase | Fase 8 (rede) |
| **S3DObj / RENDER_3DOBJ** (ícone 3D na UI) | Precisa de dados de inventário (servidor); 95% da UI é 2D | Fase 8 |
| **SReelPanel, SListBoxBoard/Party/ServerItem** | Idem — features de UI com dados de servidor | Fase 8 |
| **GLFont fase 2** (atlas de glifos) | Modernização, não paridade | Fase 8 |
| **IME candidatos** (lista própria desenhada in-game) | OS desenha a janela de candidatos; basta para "básico" | Fase 8 se necessário |
| **Som 3D posicional** (listener DS3D) | Original cria o listener mas todos os `GetSoundAndPlay` tocam 2D (`Play(priority,flags)`) | — |
| `TMEffectSkinMesh` beast deslizante, `TMShip`, `TMEffectFirework` de evento | Casos raros de cena | Fase 8 |
| **Patch/updater** | Depende de fluxo de login | Fase 8 |

---

## 2. Áudio — AudioEngine (miniaudio)

### Decisão de backend

| Opção | Prós | Contras |
|---|---|---|
| **miniaudio** (escolhida) | 1 header; device próprio (CoreAudio/ALSA/Pulse/WASAPI); decodifica **WAV e MP3**; `ma_sound` polifônico sobre o mesmo buffer; groups = volume por grupo (SFX/BGM) | Novo dep em `extern/` |
| SDL3 audio + dr_wav/dr_mp3 | Reusa device SDL | 2 headers extras; sem mixer — polyphony/volume/loop na mão; mais código |
| SDL3_mixer | Completo | Build pesado, overkill |

O original tem **dois** sistemas: `CSoundManager` (DirectSound, SFX WAV) e
`DS_SOUND_MANAGER` (DirectShow, BGM MP3). miniaudio cobre ambos com um device só.

### Modelo do original (estudo)

- `soundlist.txt` (raiz `sound\`): linhas `index path channels` — index 1-511,
  path relativo com `\`, `channels` = nº de buffers duplicados (polifonia 1-3)
  (`dsutil.cpp:57-75`).
- **Lazy load**: buffer só é criado no primeiro `GetSoundData(index)`
  (`dsutil.cpp:262-296`); `Create(..., DSBCAPS_CTRLVOLUME, ..., nChannel)`.
- `Play(priority, flags)` pega o primeiro buffer livre; `DSBPLAY_LOOPING` para
  ambiente (chuva 101, neve 113) via `GetSoundAndPlayIfNot`.
- Volume master: `m_nSound` (0-100) → centi-dB `25*n-2500`
  (`NewApp.cpp:418-424`); `-10000` = mudo (retorna null em `GetSoundData`).
- BGM: `DS_SOUND_MANAGER(1, 30*m_nMusic-3000)`; 15 faixas fixas
  (`music\login.mp3` … `music\CastleWar.mp3`, `DirShow.cpp:7-23`), uma por
  cena/estado; `PlayMusic(index)`; loop via evento de completion.

### Arquitetura do port

```cpp
// src/audio/AudioEngine.h
namespace tmx {
class AudioEngine {
public:
    bool Init();                                   // ma_context + ma_device + 2 groups
    void Shutdown();

    // SFX (soundlist.txt)
    bool LoadSoundList(const char* path);          // parse index/path/channels
    void PlaySound(int id);                        // GetSoundAndPlay
    void PlaySoundIfNot(int id);                   // GetSoundAndPlayIfNot
    void StopSound(int id);                        // GetSoundAndPause
    bool IsSoundPlaying(int id);
    void SetSoundVolume(int percent);              // 0-100 (slider do jogo)

    // BGM
    void PlayMusic(int index);                     // 15 faixas, loop
    void StopMusic();
    void SetMusicVolume(int percent);

    void SetMute(bool mute);
private:
    struct Entry { std::string path; int channels; std::vector<ma_sound*> pool;
                   ma_audio_buffer* decoded; };    // decoded compartilhado na pool
    Entry m_sfx[512];
    // ...
};
}
```

- **Decodificação**: WAV PCM via `ma_audio_buffer` (decode no load, memória);
  pool de `ma_sound` por entrada, cap = `channels`; `PlaySound` pega o primeiro
  `!ma_sound_is_playing` (fiel a `GetFreeBuffer`), senão rouba o mais antigo.
- **Mapeamento de volume** (bit-fiel): DS volume é centésimos de dB →
  `dB = 0.25*n - 25`; `linear = powf(10, dB/20)` aplicado como gain do grupo SFX.
  `n=100` → 1.0; `n=0` → ~0.056; mute = gain 0. Mesma fórmula para música com
  `0.30*n - 30`.
- **BGM**: `ma_sound` streaming (`MA_SOUND_FLAG_STREAM`) + loop; troca de faixa
  faz fade-out curto (o original é corte seco — manter corte, paridade).
- **Cena→faixa** (viewer): `--ui` → `login.mp3` (índice 0); campo →
  `field01.mp3` (índice 2). Flags: `--no-sound`, `--no-music`, `--volume N`.
- **Link**: Linux precisa `-lpthread -ldl -lm`; macOS/Windows nada extra.
  miniaudio implementa em 1 TU (`MINIAUDIO_IMPLEMENTATION` em
  `src/audio/miniaudio_impl.cpp`).

### Wiring (call sites do nosso port)

| ID | Som | Onde ligar |
|---|---|---|
| 33 | click UI | `SControlContainer` mouse-up em botão |
| 9 | swing | `--swing` demo / ataque |
| 4 | heal | `--skill heal` |
| 151/152/156/158/160 | fire/thunder/meteor/… | skills via CLI |
| 307 | cannon | `--arrow`/cannon |
| 101 | chuva (loop) | weather rain on/off (PlayIfNot/Stop) |
| 113 | neve (loop) | weather snow |
| 57 | gate | portão (quando houver) |
| 21-28 | dano | TMFont3 spawn de dano |
| 31-36 | inventário/efeito | UI demo |

---

## 3. Input completo

### Gestos de câmera (fiel a `EventTranslator.cpp:193-461`)

O original polla DirectInput por frame (`ReadInputEventData`) e aplica em
`CameraEventData`:

- **Rotação**: `button[2]` (meio) ou `Alt + button[1]` (direito) arrastando:
  `pitch -= dy * 0.002f` (clamp **-0.9854 … 0.75**), `yaw += dx * 0.0049f`
  (wrap 0…2π). Montado: pitch máx cai para 0.449/0.23 conforme `MountSkinMeshType`.
- **Zoom**: `wheel` (mouse wheel) ou **Alt sem botão** arrastando
  (`wheel = 3*dy`); distância mínima `fClose = 1.2` (2.5 montado) +
  `Con * 0.00019`.
- **Inversão**: `RenderDevice::m_bCameraRot` (Config[10]) troca sinal de dx/wheel.
- **Quarter view**: `m_nQuaterView` (Config[13]) trava rotação livre.

No port: `SDL_EVENT_MOUSE_MOTION` usa `xrel/yrel` durante drag; wheel já existe
(`followDist`); aplicar as constantes exatas na follow camera da Fase 3, não na
free-fly (que permanece via `--fly`).

### Teclado

- Key state array por frame (`SDL_GetKeyboardState`, já usado na free-fly) —
  expor em `Platform.h` para sistemas (ex.: Shift = andar/correr).
- Hotkeys originais relevantes ao viewer: **PrintScreen** (screenshot, no
  KEY_UP — `NewApp.cpp:953`), **F10**, **ESC** (fecha modal/quit do modo UI).
- Teclas demo da Fase 3-6 (1-9 motions, R, P, C) permanecem.

### IME básico

- Foco em `SEditableText` → `SDL_StartTextInput` + **`SDL_SetTextInputArea`**
  com o retângulo do controle (popup do OS segue o cursor de texto).
- **`SDL_EVENT_TEXT_EDITING`**: composição em andamento → renderizar no controle
  com underline (estilo padrão), sem commit até `SDL_EVENT_TEXT_INPUT`.
- Janela de candidatos: desenhada pelo OS (macOS/Windows/Linux IME nativo).
- **Clipboard**: Ctrl+C/X/V no SEditableText via `SDL_GetClipboardText` /
  `SDL_SetClipboardText` (o original não tem — melhoria barata e segura).

---

## 4. Screenshot

Fiel a `D3DDevice::CaptureScreen` (`D3DDevice.cpp:1050-1075`):

```
ScreenShot/Capture0000.bmp … Capture9999.bmp   (auto-incremento, primeiro livre)
```

- Disparo: `SDLK_PRINTSCREEN` no **KEY_UP** (paridade com `VK_SNAPSHOT` em
  `WM_KEYUP`).
- `glReadPixels` do framebuffer resolvido (pós-blit, ver §5) → flip Y →
  `stbi_write_bmp` (`extern/stb/stb_image_write.h` já vendored).
- Criar dir `ScreenShot/` se ausente; log com o path gravado.
- Formato BMP 24-bit (paridade com `D3DXIFF_BMP`).

---

## 5. Config de vídeo + pipeline FBO

### Config.bin (`NewApp.cpp:131-220`)

`SaveUpdatAndConfig { int16 Version; int16 Config[14]; }` (30 bytes):

| Slot | Significado | Uso no port |
|---|---|---|
| [0] | índice de resolução (tabela 11: 640×480…3200×2400) | SDL_SetWindowSize + FBO recreate |
| [1] | smooth skinmesh (LOD 0-2) | LOD bias de skinmesh (se trivial; senão só persistir) |
| [2] | som (0-100) | `AudioEngine::SetSoundVolume` |
| [3] | música (0-100) | `SetMusicVolume` |
| [5] | bright (0-100) | `uBright` (abaixo) |
| [6] | cursor type | cursor software (Fase 6) |
| [8] | windowed (0=fullscreen) | `SDL_SetWindowFullscreen` |
| [10] | camera rot invert | §3 |
| [13] | quarter view | §3 |

Read no boot (faltando → defaults do original: `Config[0]=7, [5]=57` etc.),
write na saída limpa. CLI overrides: `--res N`, `--bright N`, `--msaa N`,
`--aniso N`, `--windowed/--fullscreen`. `Graphics.ini [GRAPHICS] High/Camera`
lido por paridade (só 2 chaves).

### Gamma — a rampa do original é **ganho linear**

`RenderDevice.cpp:288-310,717-740`: `ramp[i] = min(65535, bright*0.02*i*256)` —
ou seja `out = in * (bright*0.02)`, com clamp. bright=50 → identidade. Não é
`pow`. Portanto o port é um **uniform de ganho no blit final**:

```glsl
// shaders/blit.frag — fullscreen triangle do FBO para o backbuffer
uniform sampler2D uScene;
uniform float uBright;          // = bright * 0.02  (Config[5])
out vec4 frag;
void main() { frag = vec4(texture(uScene, vUV).rgb * uBright, 1.0); }
```

### Pipeline FBO (habilita gamma + MSAA)

```
cena 3D + UI  →  FBO offscreen (RGBA8 + D24S8)
              →  resolve (se MSAA: blit FBO multi → single)
              →  blit fullscreen com uBright → backbuffer
```

- MSAA: `glRenderbufferStorageMultisample` 2/4x (config; 0 = caminho atual
  sem FBO? **não** — manter sempre o FBO para ter um único caminho de código;
  MSAA=0 → FBO single-sample sem resolve).
- Aniso: `GL_TEXTURE_MAX_ANISOTROPY_EXT` clamp a
  `GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT`, aplicado nos samplers de mundo
  (GLSamplers), config 1/2/4/8/16.
- Resize/setting change → `RecreateTargets()` central.
- Custo: 1 blit fullscreen/frame (~negligível) e destrava bloom/reflexão na
  Fase 8 sem retrabalho.

---

## 6. Rede — HTTP mínimo (sem libcurl)

Três call sites no original, todos GET HTTP/1.0 simples:

1. `BASE_GetHttpRequest(url, buf, max)` — helper genérico (`Basedef.cpp:396-424`).
2. `NewApp::GetHttpRequest` — duplicata do helper (`NewApp.cpp:716-739`).
3. `Guildmark_Download` — thread que baixa BMP de **632 bytes**,
   valida (`Guildmark_IsCorrectBMP`) e carrega textura
   (`TMFieldScene.cpp:24422-24490`).

### Port

```cpp
// src/net/HttpClient.h
namespace tmx {
// GET http://host[:port]/path — blocking, timeout 5s, body cap 64KB.
// Retorna bytes lidos (0 = erro). Sem HTTPS (URLs do jogo são http).
int HttpGet(const char* url, char* outBuf, int outCap);
}
```

- Sobre sockets BSD diretos (CPSock é orientado ao protocolo do jogo; não
  reutilizar). Parse de URL mínimo: `http://` + host + `:porta`? + path.
- `BASE_GetHttpRequest` 1:1 sobre `HttpGet`.
- Guild mark: `std::thread` (paridade com o original) → valida BMP →
  `LoadGuildTexture` (pipeline de render já existe desde a Fase 6) → flag de
  conclusão consumida no frame (nunca tocar GL da thread).
- **Por que não libcurl**: FetchContent do curl puxa TLS backend e dobra o
  tempo de build para atender 3 GETs de texto/BMP. Se HTTPS aparecer (Fase 8,
  patch/login), a troca é localizada em `HttpClient.cpp`.

---

## 7. Vídeos de intro — já cortados na fonte

`TMVideoWnd.cpp:12-51`: todos os métodos retornam `E_NOTIMPL`/0 no build
original (incluindo `PlayMovieInWindow`). DirectShow (`DirShow.cpp`) sobrevive
apenas como player de BGM (§2). **Nada a portar** — esta seção documenta o
corte para fechar o item 35 do roadmap.

---

## 8. Pendências absorvidas das Fases 5/6

| Pendência | Origem | Destino na Fase 7 |
|---|---|---|
| `TMArrow` 13-tipos data-table (mesh por tipo/level) | Fase 5 | §9 passo 9 — tabela estática + seleção por tipo |
| Sons de skills/clima/UI | Fase 5/6 | §2 wiring |
| Guild mark download | Fase 6 | §6 |
| IME | Fase 6 | §3 |
| S3DObj / RENDER_3DOBJ / SReelPanel / ListBox espec. | Fase 6 | **re-defer → Fase 8** (precisam de dados de servidor) |
| GLFont fase 2 (atlas) | Fase 6 | Fase 8 (modernização) |
| Beast deslizante / TMShip / Firework de evento | Fase 5 | Fase 8 (casos raros) |

---

## 9. Ordem de execução (10 passos)

1. **AudioEngine skeleton**: vendor `miniaudio.h`, impl TU, device + 2 groups,
   mapeamento de volume DS→linear. `test_audio` (parse soundlist, fórmula de
   ganho, pool bookkeeping headless — sem device real via backend null).
2. **SFX**: soundlist parse, WAV decode lazy, polifonia `nChannel`,
   Play/PlayIfNot/Stop/IsPlaying; wiring weather (chuva/neve) + UI click (33).
3. **BGM**: 15 MP3 streaming, `PlayMusic(index)`, faixa por cena
   (`--ui`→login, campo→field01), flags `--no-sound/--no-music/--volume`.
4. **Input câmera**: gestos middle-drag/Alt+RMB com constantes exatas, wheel
   `fClose`, inversão, quarter-view; key array exposto no Platform.
5. **IME + clipboard**: TEXT_EDITING com underline, SetTextInputArea,
   Ctrl+C/X/V no SEditableText. `test_input` (gesture math, keymap).
6. **Screenshot**: CaptureScreen + PrintScreen + dir auto-incremento.
7. **FBO pipeline**: offscreen + blit `uBright` + MSAA resolve + aniso +
   `RecreateTargets`; Config.bin read/write + CLI flags. `test_config`
   (roundtrip Config.bin golden 30B).
8. **HTTP**: HttpClient + BASE_GetHttpRequest + guild mark thread.
   `test_http` (parse URL/response contra socket local).
9. **TMArrow data-table** + leftovers pequenos.
10. **Fechamento**: sweep de `GetSoundAndPlay` restantes, ctest verde
    (alvo ≥31 suítes), docs 13/README, retrospectiva §10.

Cada passo termina com validação na tela/ouvido + testes verdes (padrão das
fases anteriores).

---

## 10. Retrospectiva (pós-execução)

_A preencher após a execução: desvios do plano, bugs encontrados, decisões
tomadas em runtime._
