#include "sdl_window.hxx"
#include "ffi.h"
#include "logo.hxx"
#include "main.hxx"

#include <algorithm>
#include <string>
#include <vector>

#include <SDL2/SDL.h>
#include <string.h>

static bool win_init = false;
static struct CDrawWindow {
  SDL_mutex* screen_mutex;
  SDL_cond* screen_done_cond;
  SDL_Window* window;
  SDL_Palette* palette;
  SDL_Surface* surf;
  SDL_Renderer* rend;
  uint64_t sz_x, sz_y, margin_x, margin_y;
  ~CDrawWindow() {
    if (!win_init)
      return;
    // somehow segfaults idk lmao im just gonna leak memory for a
    // microsecond fuck you
    /*SDL_DestroyCond(screen_done_cond);
    SDL_DestroyMutex(screen_mutex);
    SDL_FreePalette(palette);
    SDL_FreeSurface(surf);
    SDL_DestroyRenderer(rend);*/
    SDL_DestroyWindow(window);
    SDL_Quit();
  }
} win;

void SetClipboard(char const* text) {
  SDL_SetClipboardText(text);
}

std::string const ClipboardText() {
  char* sdl_clip = SDL_GetClipboardText();
  if (sdl_clip == nullptr)
    return {};
  std::string s = sdl_clip;
  SDL_free(sdl_clip);
  if (sanitize_clipboard) {
    /*std::erase_if(s, [](uint8_t c) {
      return ' ' - 1 > c;
    }); C++20
    below is the C++17 equivalent because reasons*/
    // uint8 is important here since signed char
    // might become negative for utf-8 bytes
    auto it = std::remove_if(s.begin(), s.end(), [](uint8_t c) {
      return ' ' - 1 > c;
    });
    s.erase(it, s.end());
  }
  return s;
}

CDrawWindow* NewDrawWindow() {
  if (win_init)
    return &win;
  win_init = true;
  if (!SDL_WasInit(SDL_INIT_EVERYTHING)) {
    SDL_Init(SDL_INIT_EVERYTHING);
    // sdl disables compositor in kde by default
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0",
                            SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, "linear",
                            SDL_HINT_OVERRIDE);
  }
  win.screen_mutex = SDL_CreateMutex();
  win.screen_done_cond = SDL_CreateCond();
  win.window =
      SDL_CreateWindow("TempleOS", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_RESIZABLE);
  SDL_Surface* icon = SDL_CreateRGBSurfaceWithFormat(
      0, tos_logo.width, tos_logo.height,
      8 /*bits in a byte*/ * tos_logo.bytes_per_pixel, SDL_PIXELFORMAT_BGR888);
  SDL_LockSurface(icon);
  // icon->pixels = const_cast<void*>((void const*)tos_logo.pixel_data);
  // whatever, just copy it over lmao
  auto constexpr bytes =
      tos_logo.width * tos_logo.height * tos_logo.bytes_per_pixel;
  std::copy(tos_logo.pixel_data, tos_logo.pixel_data + bytes,
            static_cast<uint8_t*>(icon->pixels));
  SDL_UnlockSurface(icon);
  SDL_SetWindowIcon(win.window, icon);
  SDL_FreeSurface(icon);
  win.surf = SDL_CreateRGBSurface(0, 640, 480, 8, 0, 0, 0, 0);
  win.palette = SDL_AllocPalette(256);
  SDL_SetSurfacePalette(win.surf, win.palette);
  SDL_SetWindowMinimumSize(win.window, 640, 480);
  win.rend = SDL_CreateRenderer(win.window, -1, SDL_RENDERER_ACCELERATED);
  win.margin_y = win.margin_x = 0;
  win.sz_x = 640;
  win.sz_y = 480;
  // let templeos manage the cursor
  SDL_ShowCursor(SDL_DISABLE);
  return &win;
}

static void DrawWindowUpdate_pre(CDrawWindow* ul, uint8_t* colors,
                                 uint64_t internal_width, uint64_t h) {
  if (!SDL_WasInit(SDL_INIT_EVERYTHING))
    return;
  if (!win_init)
    return;
  SDL_Surface* s = win.surf;
  uint64_t x, y;
  uint8_t *src = colors, *dst = (uint8_t*)s->pixels;
  SDL_LockSurface(s);
  for (y = 0; y < h; ++y) {
    memcpy(dst, src, 640);
    src += internal_width;
    dst += s->pitch;
  }
  SDL_UnlockSurface(s);
  int ww, wh, w2, h2;
  int64_t margin = 0, margin2 = 0;
  SDL_Rect rct;
  SDL_GetWindowSize(win.window, &ww, &wh);
  if (wh < ww) {
    h2 = wh;
    w2 = 640. / 480 * h2;
    margin = (ww - w2) / 2;
    if (w2 > ww) {
      margin = 0;
      goto top_margin;
    }
  } else {
  top_margin:
    w2 = ww;
    h2 = 480 / 640. * w2;
    margin2 = (wh - h2) / 2;
  }
  win.margin_x = margin;
  win.margin_y = margin2;
  win.sz_x = w2;
  win.sz_y = h2;
  rct.y = margin2;
  rct.x = margin;
  rct.w = w2;
  rct.h = h2;
  SDL_Texture* t = SDL_CreateTextureFromSurface(win.rend, s);
  SDL_RenderClear(win.rend);
  SDL_RenderCopy(win.rend, t, NULL, &rct);
  SDL_RenderPresent(win.rend);
  SDL_DestroyTexture(t);
  SDL_CondBroadcast(win.screen_done_cond);
}

void DrawWindowUpdate(struct CDrawWindow* w, int8_t* colors,
                      int64_t internal_width, int64_t h) {
  // https://archive.md/yD5QL
  SDL_Event event;
  SDL_UserEvent userevent;

  /* In this example, our callback
  pushes an SDL_USEREVENT event into the
  queue, and causes our callback to be
  called again at the same interval: */

  userevent.type = SDL_USEREVENT;
  userevent.code = 0;
  userevent.data1 = colors;
  userevent.data2 = (void*)(uintptr_t)internal_width;

  event.type = SDL_USEREVENT;
  event.user = userevent;

  SDL_LockMutex(win.screen_mutex);
  SDL_PushEvent(&event);
  // If there are lots of events,it may
  // get lost
  SDL_CondWaitTimeout(win.screen_done_cond, win.screen_mutex, 30);
  SDL_UnlockMutex(win.screen_mutex);
  return;
}

static void UserEvHandler(void* a, SDL_UserEvent* ev) {
  if (ev->type == SDL_USEREVENT)
    DrawWindowUpdate_pre(NULL, (uint8_t*)ev->data1, (uintptr_t)ev->data2, 480);
}

enum : uint8_t {
  CH_CTRLA = 0x01,
  CH_CTRLB = 0x02,
  CH_CTRLC = 0x03,
  CH_CTRLD = 0x04,
  CH_CTRLE = 0x05,
  CH_CTRLF = 0x06,
  CH_CTRLG = 0x07,
  CH_CTRLH = 0x08,
  CH_CTRLI = 0x09,
  CH_CTRLJ = 0x0A,
  CH_CTRLK = 0x0B,
  CH_CTRLL = 0x0C,
  CH_CTRLM = 0x0D,
  CH_CTRLN = 0x0E,
  CH_CTRLO = 0x0F,
  CH_CTRLP = 0x10,
  CH_CTRLQ = 0x11,
  CH_CTRLR = 0x12,
  CH_CTRLS = 0x13,
  CH_CTRLT = 0x14,
  CH_CTRLU = 0x15,
  CH_CTRLV = 0x16,
  CH_CTRLW = 0x17,
  CH_CTRLX = 0x18,
  CH_CTRLY = 0x19,
  CH_CTRLZ = 0x1A,
  CH_CURSOR = 0x05,
  CH_BACKSPACE = 0x08,
  CH_ESC = 0x1B,
  CH_SHIFT_ESC = 0x1C,
  CH_SHIFT_SPACE = 0x1F,
  CH_SPACE = 0x20,
};

// Scan code flags
enum : uint8_t {
  SCf_E0_PREFIX = 7,
  SCf_KEY_UP = 8,
  SCf_SHIFT = 9,
  SCf_CTRL = 10,
  SCf_ALT = 11,
  SCf_CAPS = 12,
  SCf_NUM = 13,
  SCf_SCROLL = 14,
  SCf_NEW_KEY = 15,
  SCf_MS_L_DOWN = 16,
  SCf_MS_R_DOWN = 17,
  SCf_DELETE = 18,
  SCf_INS = 19,
  SCf_NO_SHIFT = 30,
  SCf_KEY_DESC = 31,
};

enum : uint32_t {
  SCF_E0_PREFIX = 1u << SCf_E0_PREFIX,
  SCF_KEY_UP = 1u << SCf_KEY_UP,
  SCF_SHIFT = 1u << SCf_SHIFT,
  SCF_CTRL = 1u << SCf_CTRL,
  SCF_ALT = 1u << SCf_ALT,
  SCF_CAPS = 1u << SCf_CAPS,
  SCF_NUM = 1u << SCf_NUM,
  SCF_SCROLL = 1u << SCf_SCROLL,
  SCF_NEW_KEY = 1u << SCf_NEW_KEY,
  SCF_MS_L_DOWN = 1u << SCf_MS_L_DOWN,
  SCF_MS_R_DOWN = 1u << SCf_MS_R_DOWN,
  SCF_DELETE = 1u << SCf_DELETE,
  SCF_INS = 1u << SCf_INS,
  SCF_NO_SHIFT = 1u << SCf_NO_SHIFT,
  SCF_KEY_DESC = 1u << SCf_KEY_DESC,
};

// TempleOS places a 1 in bit 7 for
// keys with an E0 prefix.
// See \dLK,"::/Doc/CharOverview.DD"\d
// and
// \dLK,"KbdHndlr",A="MN:KbdHndlr"\d().
enum : uint8_t {
  SC_ESC = 0x01,
  SC_BACKSPACE = 0x0E,
  SC_TAB = 0x0F,
  SC_ENTER = 0x1C,
  SC_SHIFT = 0x2A,
  SC_CTRL = 0x1D,
  SC_ALT = 0x38,
  SC_CAPS = 0x3A,
  SC_NUM = 0x45,
  SC_SCROLL = 0x46,
  SC_CURSOR_UP = 0x48,
  SC_CURSOR_DOWN = 0x50,
  SC_CURSOR_LEFT = 0x4B,
  SC_CURSOR_RIGHT = 0x4D,
  SC_PAGE_UP = 0x49,
  SC_PAGE_DOWN = 0x51,
  SC_HOME = 0x47,
  SC_END = 0x4F,
  SC_INS = 0x52,
  SC_DELETE = 0x53,
  SC_F1 = 0x3B,
  SC_F2 = 0x3C,
  SC_F3 = 0x3D,
  SC_F4 = 0x3E,
  SC_F5 = 0x3F,
  SC_F6 = 0x40,
  SC_F7 = 0x41,
  SC_F8 = 0x42,
  SC_F9 = 0x43,
  SC_F10 = 0x44,
  SC_F11 = 0x57,
  SC_F12 = 0x58,
  SC_PAUSE = 0x61,
  SC_GUI = 0xDB,
  SC_PRTSCRN1 = 0xAA,
  SC_PRTSCRN2 = 0xB7,
};

// this is templeos' keymap
static char constexpr keys[] = {
    0,   CH_ESC, '1',  '2', '3',  '4', '5', '6', '7', '8', '9', '0', '-',
    '=', '\b',   '\t', 'q', 'w',  'e', 'r', 't', 'y', 'u', 'i', 'o', 'p',
    '[', ']',    '\n', 0,   'a',  's', 'd', 'f', 'g', 'h', 'j', 'k', 'l',
    ';', '\'',   '`',  0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',',
    '.', '/',    0,    '*', 0,    ' ', 0,   0,   0,   0,   0,   0,   0,
    0,   0,      0,    0,   0,    0,   0,   0,   0,   '-', 0,   '5', 0,
    '+', 0,      0,    0,   0,    0,   0,   0,   0,   0,   0,   0};

static inline constexpr uint64_t K2SC(char ch) {
  for (size_t i = 0; i != sizeof(keys) / sizeof(*keys); i++) {
    if (keys[i] == ch)
      return i;
  }
  __builtin_unreachable();
}

static int32_t ScanKey(int64_t* ch, int64_t* sc, SDL_Event* ev) {
  SDL_Event e = *ev;
  int64_t mod = 0;
  if (e.type == SDL_KEYDOWN) {
  ent:
    *sc = e.key.keysym.scancode;
    if (e.key.keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT))
      mod |= SCF_SHIFT;
    else
      mod |= SCF_NO_SHIFT;
    if (e.key.keysym.mod & (KMOD_LCTRL | KMOD_RCTRL))
      mod |= SCF_CTRL;
    if (e.key.keysym.mod & (KMOD_LALT | KMOD_RALT))
      mod |= SCF_ALT;
    if (e.key.keysym.mod & (KMOD_CAPS))
      mod |= SCF_CAPS;
    if (e.key.keysym.mod & (KMOD_NUM))
      mod |= SCF_NUM;
    if (e.key.keysym.mod & KMOD_LGUI)
      mod |= SCF_MS_L_DOWN;
    if (e.key.keysym.mod & KMOD_RGUI)
      mod |= SCF_MS_R_DOWN;
    switch (e.key.keysym.scancode) {
    case SDL_SCANCODE_SPACE:
      return *sc = K2SC(' ') | mod;
    case SDL_SCANCODE_APOSTROPHE:
      return *sc = K2SC('\'') | mod;
    case SDL_SCANCODE_COMMA:
      return *sc = K2SC(',') | mod;
    case SDL_SCANCODE_MINUS:
      return *sc = K2SC('-') | mod;
    case SDL_SCANCODE_PERIOD:
      return *sc = K2SC('.') | mod;
    case SDL_SCANCODE_GRAVE:
      return *sc = K2SC('`') | mod;
    case SDL_SCANCODE_SLASH:
      return *sc = K2SC('/') | mod;
    case SDL_SCANCODE_0:
      return *sc = K2SC('0') | mod;
    case SDL_SCANCODE_1:
      return *sc = K2SC('1') | mod;
    case SDL_SCANCODE_2:
      return *sc = K2SC('2') | mod;
    case SDL_SCANCODE_3:
      return *sc = K2SC('3') | mod;
    case SDL_SCANCODE_4:
      return *sc = K2SC('4') | mod;
    case SDL_SCANCODE_5:
      return *sc = K2SC('5') | mod;
    case SDL_SCANCODE_6:
      return *sc = K2SC('6') | mod;
    case SDL_SCANCODE_7:
      return *sc = K2SC('7') | mod;
    case SDL_SCANCODE_8:
      return *sc = K2SC('8') | mod;
    case SDL_SCANCODE_9:
      return *sc = K2SC('9') | mod;
    case SDL_SCANCODE_SEMICOLON:
      return *sc = K2SC(';') | mod;
    case SDL_SCANCODE_EQUALS:
      return *sc = K2SC('=') | mod;
    case SDL_SCANCODE_LEFTBRACKET:
      return *sc = K2SC('[') | mod;
    case SDL_SCANCODE_RIGHTBRACKET:
      return *sc = K2SC(']') | mod;
    case SDL_SCANCODE_BACKSLASH:
      return *sc = K2SC('\\') | mod;
    case SDL_SCANCODE_Q:
      return *sc = K2SC('q') | mod;
    case SDL_SCANCODE_W:
      return *sc = K2SC('w') | mod;
    case SDL_SCANCODE_E:
      return *sc = K2SC('e') | mod;
    case SDL_SCANCODE_R:
      return *sc = K2SC('r') | mod;
    case SDL_SCANCODE_T:
      return *sc = K2SC('t') | mod;
    case SDL_SCANCODE_Y:
      return *sc = K2SC('y') | mod;
    case SDL_SCANCODE_U:
      return *sc = K2SC('u') | mod;
    case SDL_SCANCODE_I:
      return *sc = K2SC('i') | mod;
    case SDL_SCANCODE_O:
      return *sc = K2SC('o') | mod;
    case SDL_SCANCODE_P:
      return *sc = K2SC('p') | mod;
    case SDL_SCANCODE_A:
      return *sc = K2SC('a') | mod;
    case SDL_SCANCODE_S:
      return *sc = K2SC('s') | mod;
    case SDL_SCANCODE_D:
      return *sc = K2SC('d') | mod;
    case SDL_SCANCODE_F:
      return *sc = K2SC('f') | mod;
    case SDL_SCANCODE_G:
      return *sc = K2SC('g') | mod;
    case SDL_SCANCODE_H:
      return *sc = K2SC('h') | mod;
    case SDL_SCANCODE_J:
      return *sc = K2SC('j') | mod;
    case SDL_SCANCODE_K:
      return *sc = K2SC('k') | mod;
    case SDL_SCANCODE_L:
      return *sc = K2SC('l') | mod;
    case SDL_SCANCODE_Z:
      return *sc = K2SC('z') | mod;
    case SDL_SCANCODE_X:
      return *sc = K2SC('x') | mod;
    case SDL_SCANCODE_C:
      return *sc = K2SC('c') | mod;
    case SDL_SCANCODE_V:
      return *sc = K2SC('v') | mod;
    case SDL_SCANCODE_B:
      return *sc = K2SC('b') | mod;
    case SDL_SCANCODE_N:
      return *sc = K2SC('n') | mod;
    case SDL_SCANCODE_M:
      return *sc = K2SC('m') | mod;
    case SDL_SCANCODE_ESCAPE:
      *sc = mod | SC_ESC;
      return 1;
    case SDL_SCANCODE_BACKSPACE:
      *sc = mod | SC_BACKSPACE;
      return 1;
    case SDL_SCANCODE_TAB:
      *sc = mod | SC_TAB;
      return 1;
    case SDL_SCANCODE_RETURN:
      *sc = mod | SC_ENTER;
      return 1;
    case SDL_SCANCODE_LSHIFT:
    case SDL_SCANCODE_RSHIFT:
      *sc = mod | SC_SHIFT;
      return 1;
    case SDL_SCANCODE_LALT:
      *sc = mod | SC_ALT;
      return 1;
    case SDL_SCANCODE_RALT:
      *sc = mod | SC_ALT;
      return 1;
    case SDL_SCANCODE_LCTRL:
      *sc = mod | SC_CTRL;
      return 1;
    case SDL_SCANCODE_RCTRL:
      *sc = mod | SC_CTRL;
      return 1;
    case SDL_SCANCODE_CAPSLOCK:
      *sc = mod | SC_CAPS;
      return 1;
    case SDL_SCANCODE_NUMLOCKCLEAR:
      *sc = mod | SC_NUM;
      return 1;
    case SDL_SCANCODE_SCROLLLOCK:
      *sc = mod | SC_SCROLL;
      return 1;
    case SDL_SCANCODE_DOWN:
      *sc = mod | SC_CURSOR_DOWN;
      return 1;
    case SDL_SCANCODE_UP:
      *sc = mod | SC_CURSOR_UP;
      return 1;
    case SDL_SCANCODE_RIGHT:
      *sc = mod | SC_CURSOR_RIGHT;
      return 1;
    case SDL_SCANCODE_LEFT:
      *sc = mod | SC_CURSOR_LEFT;
      return 1;
    case SDL_SCANCODE_PAGEDOWN:
      *sc = mod | SC_PAGE_DOWN;
      return 1;
    case SDL_SCANCODE_PAGEUP:
      *sc = mod | SC_PAGE_UP;
      return 1;
    case SDL_SCANCODE_HOME:
      *sc = mod | SC_HOME;
      return 1;
    case SDL_SCANCODE_END:
      *sc = mod | SC_END;
      return 1;
    case SDL_SCANCODE_INSERT:
      *sc = mod | SC_INS;
      return 1;
    case SDL_SCANCODE_DELETE:
      *sc = mod | SC_DELETE;
      return 1;
    case SC_GUI:
      *sc = mod | SDL_SCANCODE_APPLICATION;
      return 1;
    case SDL_SCANCODE_PRINTSCREEN:
      *sc = mod | SC_PRTSCRN1;
      return 1;
    case SDL_SCANCODE_PAUSE:
      *sc = mod | SC_PAUSE;
      return 1;
    case SDL_SCANCODE_F1 ... SDL_SCANCODE_F12:
      *sc = mod | (SC_F1 + e.key.keysym.scancode - SDL_SCANCODE_F1);
      return 1;
    default:;
    }
  } else if (e.type == SDL_KEYUP) {
    mod |= SCF_KEY_UP;
    goto ent;
  }
  return -1;
}

static uint32_t TimerCb(uint32_t interval, void* data) {
  FFI_CALL_TOS_1(data, interval);
  return interval;
}

static void* kb_cb = nullptr;
static void* kb_cb_data = nullptr;
static bool kb_init = false;
static bool ms_init = false;
static int SDLCALL KBCallback(void* d, SDL_Event* e) {
  int64_t c, s;
  if (kb_cb && (-1 != ScanKey(&c, &s, e)))
    FFI_CALL_TOS_2(kb_cb, c, s);
  return 0;
}
void SetKBCallback(void* fptr, void* data) {
  kb_cb = fptr;
  kb_cb_data = data;
  if (!kb_init) {
    kb_init = true;
    SDL_AddEventWatch(KBCallback, data);
  }
}
// x,y,z,(l<<1)|r
static void* ms_cb = nullptr;
static int SDLCALL MSCallback(void* d, SDL_Event* e) {
  static Sint32 x, y;
  static int state;
  static int z;
  int x2, y2;
  // return value is actually ignored
  if (!ms_cb)
    return 0;
  switch (e->type) {
  case SDL_MOUSEBUTTONDOWN:
    x = e->button.x, y = e->button.y;
    if (e->button.button == SDL_BUTTON_LEFT)
      state |= 2;
    else // right
      state |= 1;
    goto ent;
  case SDL_MOUSEBUTTONUP:
    x = e->button.x, y = e->button.y;
    if (e->button.button == SDL_BUTTON_LEFT)
      state &= ~2;
    else // right
      state &= ~1;
    goto ent;
  case SDL_MOUSEWHEEL:
    z -= e->wheel.y; //???,inverted
                     // otherwise
    goto ent;
  case SDL_MOUSEMOTION:
    x = e->motion.x, y = e->motion.y;
  ent:;
    if (x < win.margin_x)
      x2 = 0;
    else if (x > win.margin_x + win.sz_x)
      x2 = 640 - 1;
    else {
      x2 = (x - win.margin_x) * 640. / win.sz_x;
    }
    if (y < win.margin_y)
      y2 = 0;
    else if (y > win.margin_y + win.sz_y)
      y2 = 480 - 1;
    else {
      y2 = (y - win.margin_y) * 480. / win.sz_y;
    }
    FFI_CALL_TOS_4(ms_cb, x2, y2, z, state);
  default:;
  }
  return 0;
}

void SetMSCallback(void* fptr) {
  ms_cb = fptr;
  if (ms_init)
    return;
  ms_init = true;
  SDL_AddEventWatch(MSCallback, NULL);
}

static int ExitCb(void* off, SDL_Event* event) {
  if (event->type == SDL_QUIT)
    *(bool*)off = true;
  return 0;
}

void InputLoop(bool* off) {
  SDL_Event e;
  SDL_AddEventWatch(&ExitCb, off);
  while (!*off) {
    if (!SDL_WaitEvent(&e))
      continue;
    if (e.type == SDL_USEREVENT)
      UserEvHandler(NULL, (SDL_UserEvent*)&e);
  }
}

// please policeman am i under arrest? read me my rights please!
// I WANT MY PHONE CALL!!!
extern "C" union bgr_48 {
  struct __attribute__((packed)) {
    uint16_t b, g, r, pad;
  } c;
  uint64_t i;
};

void GrPaletteColorSet(uint64_t i, uint64_t bgr48) {
  if (!win_init)
    return;
  bgr_48 u;
  u.i = bgr48;
  // clang-format off
  // 0xffff is 100% so 0x7fff/0xffff would be about .50
  // this gets multiplied by 0xff to get 0x7f
  Uint8 b = u.c.b / (double)0xffff * 0xff,
        g = u.c.g / (double)0xffff * 0xff,
        r = u.c.r / (double)0xffff * 0xff;
  // clang-format on
  // i seriously need designated inits in c++
  SDL_Color sdl_c;
  sdl_c.r = r;
  sdl_c.g = g;
  sdl_c.b = b;
  sdl_c.a = 0xff;
  // set column
  for (int repeat = 0; repeat < 256 / 16; ++repeat)
    SDL_SetPaletteColors(win.palette, &sdl_c, i + repeat * 16, 1);
}
