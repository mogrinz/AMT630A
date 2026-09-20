#include "AMT630A_OSD.h"
#include <string.h>

AMT630A_OSD::AMT630A_OSD()
  : _wire(&Wire), _begun(false), _osdVisible(true),
    _factoryOSDCoexistence(false), _originalC6(0xC2), _batchDepth(0),
    _blinkWindow(0), _blinkX(1), _blinkY(1), _blinkWidth(1), _blinkHeight(1),
    _blinkRateRaw(0x10), _blinkEnabled(false), _blinkManaged(false),
    _blendingEnabled(false), _opacity(7), _brightness(0),
    _fontMode(LargeFont), _displayWidth(480), _displayHeight(272),
    _customGlyphCapacity(MAX_CUSTOM_GLYPHS), _customGlyphLoadedCount(0),
    _bitmapStartWord(0), _bitmapWordsTotal(0), _bitmapWordsUsed(0),
    _bitmapNextWord(0), _bitmapStartTile(0), _bitmapStartConfigured(false),
    _bitmapCount(0), _bitmapPalette0Transparent(true),
    _factoryMenuPin(0), _factoryPlusPin(0), _factoryMinusPin(0),
    _factoryMenuControlConfigured(false), _factoryPressTimeMs(150),
    _factoryGapTimeMs(200), _factoryMenuSequenceCount(0),
    _serviceIntervalMs(100), _lastServiceMs(0), _lastError(NoError) {

  for (uint8_t i = 0; i < WINDOW_COUNT; ++i) {
    _windows[i].configured = false;
    _windows[i].visible = true;
    _windows[i].mode = TextMode;
    _windows[i].bitmapHandle = INVALID_BITMAP_HANDLE;
    _windows[i].width = 0;
    _windows[i].height = 0;
    _windows[i].x = 0;
    _windows[i].y = 0;
    _windows[i].scaleX = 1;
    _windows[i].scaleY = 1;
    _windows[i].columns = 0;
    _windows[i].rows = 0;
    _windows[i].indexStart = 0;
    _windows[i].cellCount = 0;
    _windows[i].cells = nullptr;
  }

  for (uint16_t i = 0; i < MAX_CUSTOM_GLYPHS; ++i) _customGlyphLoaded[i] = false;

  for (uint8_t i = 0; i < MAX_BITMAPS; ++i) {
    memset(&_bitmaps[i], 0, sizeof(BitmapResource));
    _bitmaps[i].loaded = false;
  }

  for (uint8_t i = 0; i < BITMAP_PALETTE_SIZE; ++i) _bitmapPalette[i] = 0;

  useDefaultFactoryMenuSequence();

  // Preserve v0.4.x's default custom-glyph reservation. This intentionally
  // leaves almost no bitmap space until the user calls configureFontRAM().
  configureFontRAM(MAX_CUSTOM_GLYPHS);
}

bool AMT630A_OSD::configureFactoryMenuControl(uint8_t menuPin, uint8_t plusPin, uint8_t minusPin) {
  _factoryMenuPin = menuPin;
  _factoryPlusPin = plusPin;
  _factoryMinusPin = minusPin;

  pinMode(_factoryMenuPin, OUTPUT);
  pinMode(_factoryPlusPin, OUTPUT);
  pinMode(_factoryMinusPin, OUTPUT);

  // 74HC4066 control LOW = switch open.
  digitalWrite(_factoryMenuPin, LOW);
  digitalWrite(_factoryPlusPin, LOW);
  digitalWrite(_factoryMinusPin, LOW);

  _factoryMenuControlConfigured = true;
  _lastError = NoError;
  return true;
}

bool AMT630A_OSD::setFactoryMenuTiming(uint16_t pressTimeMs, uint16_t gapTimeMs) {
  if (pressTimeMs == 0) return fail(InvalidDimensions);
  _factoryPressTimeMs = pressTimeMs;
  _factoryGapTimeMs = gapTimeMs;
  _lastError = NoError;
  return true;
}

bool AMT630A_OSD::setFactoryMenuSequence(const FactoryMenuStep *steps, uint8_t count) {
  if (!steps || count == 0) return fail(InvalidDimensions);
  if (count > MAX_FACTORY_MENU_STEPS) return fail(FactoryMenuSequenceTooLong);
  for (uint8_t i = 0; i < count; ++i) _factoryMenuSequence[i] = steps[i];
  _factoryMenuSequenceCount = count;
  _lastError = NoError;
  return true;
}

void AMT630A_OSD::useDefaultFactoryMenuSequence() {
  static const FactoryMenuStep sequence[] = {
    { FactoryMenu,  0 },
    { FactoryMenu,  0 },
    { FactoryMenu,  0 },
    { FactoryMinus, 0 },
    { FactoryMenu,  0 },  // Brightness should now be active.
    { FactoryPlus,  0 },
    { FactoryMinus, 0 },
    { FactoryMinus, 0 },
    { FactoryPlus,  0 },
    { FactoryPlus,  0 },
    { FactoryMenu,  0 }   // Open Factory screen.
  };
  const uint8_t count = sizeof(sequence) / sizeof(sequence[0]);
  for (uint8_t i = 0; i < count; ++i) _factoryMenuSequence[i] = sequence[i];
  _factoryMenuSequenceCount = count;
}

bool AMT630A_OSD::enterFactoryMode() {
  if (!_factoryMenuControlConfigured) return fail(FactoryMenuNotConfigured);
  if (_factoryMenuSequenceCount == 0) return fail(FactoryMenuNotConfigured);

  for (uint8_t i = 0; i < _factoryMenuSequenceCount; ++i) {
    uint8_t pin;
    switch (_factoryMenuSequence[i].button) {
      case FactoryMenu:  pin = _factoryMenuPin;  break;
      case FactoryPlus:  pin = _factoryPlusPin;  break;
      case FactoryMinus: pin = _factoryMinusPin; break;
      default: return fail(FactoryMenuNotConfigured);
    }

    digitalWrite(pin, HIGH);
    delay(_factoryPressTimeMs);
    digitalWrite(pin, LOW);

    const uint16_t gap = _factoryMenuSequence[i].delayAfterMs
                       ? _factoryMenuSequence[i].delayAfterMs
                       : _factoryGapTimeMs;
    delay(gap);
  }

  _lastError = NoError;
  return true;
}

AMT630A_OSD::~AMT630A_OSD() {
  for (uint8_t i = 0; i < WINDOW_COUNT; ++i) freeWindowBuffer(i);
}

bool AMT630A_OSD::fail(Error error) {
  _lastError = error;
  return false;
}

bool AMT630A_OSD::validWindow(uint8_t windowNumber) const {
  return windowNumber < WINDOW_COUNT;
}

bool AMT630A_OSD::validBitmapHandle(BitmapHandle handle) const {
  return handle >= 0 && handle < (BitmapHandle)MAX_BITMAPS && _bitmaps[(uint8_t)handle].loaded;
}

bool AMT630A_OSD::anyWindowConfigured() const {
  for (uint8_t i = 0; i < WINDOW_COUNT; ++i) if (_windows[i].configured) return true;
  return false;
}

bool AMT630A_OSD::anyBitmapWindowConfigured() const {
  for (uint8_t i = 0; i < WINDOW_COUNT; ++i)
    if (_windows[i].configured && _windows[i].mode == BitmapMode) return true;
  return false;
}

bool AMT630A_OSD::anyBitmapWindowVisible() const {
  if (!_osdVisible) return false;
  for (uint8_t i = 0; i < WINDOW_COUNT; ++i)
    if (_windows[i].configured && _windows[i].visible && _windows[i].mode == BitmapMode) return true;
  return false;
}

void AMT630A_OSD::characterGeometry(uint8_t &width, uint8_t &height) const {
  if (_fontMode == SmallFont) { width = 10; height = 16; }
  else { width = 16; height = 22; }
}

uint16_t AMT630A_OSD::alignUp(uint16_t value, uint16_t alignment) const {
  if (alignment == 0) return value;
  uint16_t rem = value % alignment;
  return rem ? (uint16_t)(value + alignment - rem) : value;
}

bool AMT630A_OSD::begin(int sdaPin, int sclPin, uint32_t i2cClock) {
  _lastError = NoError;
  _batchDepth = 0;
#if defined(ESP32)
  _wire->begin(sdaPin, sclPin);
#else
  (void)sdaPin; (void)sclPin;
  _wire->begin();
#endif
  _wire->setClock(i2cClock);
  delay(20);
  _originalC6 = readReg(DEV_SYS, 0xC6);
  if (_originalC6 == 0xFF) return fail(I2CError);
  _begun = true;

  // Snapshot the board's current global blend/alpha/brightness state so the
  // getters reflect hardware state before the application changes anything.
  unlockFast();
  const uint8_t initialFB06 = readReg(DEV_OSD, 0x06);
  const uint8_t initialFB0C = readReg(DEV_OSD, 0x0C);
  relockFast();
  _blendingEnabled = (initialFB06 & 0x40) != 0;
  _opacity = initialFB0C & 0x07;
  _brightness = (initialFB0C >> 3) & 0x1F;

  if (!configureGlobalHardware()) { _begun = false; return false; }
  if (!applyVisibility()) { _begun = false; return false; }
  _lastServiceMs = millis();
  return true;
}

bool AMT630A_OSD::setDisplaySize(uint16_t width, uint16_t height) {
  if (width == 0 || height == 0) return fail(InvalidDimensions);
  _displayWidth = width;
  _displayHeight = height;
  _lastError = NoError;
  return true;
}

bool AMT630A_OSD::setFontMode(FontMode fontMode) {
  if (anyWindowConfigured()) return fail(WindowsAlreadyConfigured);
  if (_bitmapCount && fontMode == SmallFont) return fail(BitmapUnsupportedInSmallFont);
  _fontMode = fontMode;
  if (_begun) {
    uint8_t cw, ch; characterGeometry(cw, ch);
    if (!writeOSDConservative(0x76, cw) || !writeOSDConservative(0x77, ch)) return fail(I2CError);
  }
  _lastError = NoError;
  return true;
}

bool AMT630A_OSD::configureWindowCells(uint8_t n, uint16_t width, uint16_t height,
                                       uint8_t columns, uint8_t rows, WindowMode mode,
                                       BitmapHandle bitmapHandle) {
  if (!validWindow(n)) return fail(InvalidWindow);
  if (!width || !height || !columns || !rows || columns > 127 || rows > 63) return fail(InvalidDimensions);
  uint32_t count32 = (uint32_t)columns * rows;
  if (count32 > INDEX_RAM_CELLS) return fail(IndexRAMFull);

  WindowState old = _windows[n];
  Cell *oldCells = old.cells;

  _windows[n].configured = true;
  _windows[n].mode = mode;
  _windows[n].bitmapHandle = bitmapHandle;
  _windows[n].width = width;
  _windows[n].height = height;
  _windows[n].scaleX = 1;
  _windows[n].scaleY = 1;
  _windows[n].columns = columns;
  _windows[n].rows = rows;
  _windows[n].cellCount = (uint16_t)count32;
  _windows[n].cells = oldCells;

  if (!reallocateIndexRAM()) { _windows[n] = old; return false; }
  if (!allocateWindowBuffer(n, (uint16_t)count32)) {
    _windows[n] = old;
    reallocateIndexRAM();
    return false;
  }

  if (_begun && !configureWindowHardware(n)) return false;
  if (_begun && !applyWindowScaleHardware(n)) return false;
  if (_begun && !applyVisibility()) return false;
  _lastError = NoError;
  return true;
}

bool AMT630A_OSD::configureWindow(uint8_t n, uint16_t width, uint16_t height, WindowMode mode) {
  if (mode == BitmapMode) return fail(InvalidBitmapHandle);
  uint8_t cw, ch; characterGeometry(cw, ch);
  uint16_t cols16 = width / cw;
  uint16_t rows16 = height / ch;
  if (!cols16 || !rows16 || cols16 > 127 || rows16 > 63) return fail(InvalidDimensions);
  return configureWindowCells(n, width, height, (uint8_t)cols16, (uint8_t)rows16,
                              TextMode, INVALID_BITMAP_HANDLE);
}

bool AMT630A_OSD::configureBitmapWindow(uint8_t n, BitmapHandle handle) {
  if (!validBitmapHandle(handle)) return fail(InvalidBitmapHandle);
  const BitmapResource &b = _bitmaps[(uint8_t)handle];
  if (!configureWindowCells(n, b.width, b.height, b.columns, b.rows, BitmapMode, handle)) return false;

  WindowState &w = _windows[n];
  for (uint16_t i = 0; i < w.cellCount; ++i) {
    w.cells[i].glyph = b.firstGlyph + (uint16_t)i * b.wordsPerRow;
    w.cells[i].attr = 0;
    w.cells[i].dirty = true;
  }

  if (_begun && !applyVisibility()) return false;
  _lastError = NoError;
  return true;
}

bool AMT630A_OSD::releaseWindow(uint8_t n) {
  if (!validWindow(n)) return fail(InvalidWindow);
  freeWindowBuffer(n);
  _windows[n].configured = false;
  _windows[n].bitmapHandle = INVALID_BITMAP_HANDLE;
  if (_blinkManaged && n == _blinkWindow) _blinkEnabled = false;
  _windows[n].cellCount = 0;
  _windows[n].scaleX = 1;
  _windows[n].scaleY = 1;
  _windows[n].columns = _windows[n].rows = 0;
  if (!reallocateIndexRAM()) return false;
  if (_begun && !applyVisibility()) return false;
  _lastError = NoError;
  return true;
}

bool AMT630A_OSD::isWindowConfigured(uint8_t n) const { return validWindow(n) && _windows[n].configured; }
AMT630A_OSD::WindowMode AMT630A_OSD::getWindowMode(uint8_t n) const { return validWindow(n) ? _windows[n].mode : TextMode; }
AMT630A_OSD::BitmapHandle AMT630A_OSD::getWindowBitmap(uint8_t n) const {
  if (!validWindow(n) || !_windows[n].configured || _windows[n].mode != BitmapMode) return INVALID_BITMAP_HANDLE;
  return _windows[n].bitmapHandle;
}

bool AMT630A_OSD::setDisplayOrigin(uint8_t n, uint16_t x, uint16_t y) {
  if (!validWindow(n)) return fail(InvalidWindow);
  if (!_windows[n].configured) return fail(WindowNotConfigured);
  if (x > 0x7FF || y > 0x7FF) return fail(InvalidDimensions);
  _windows[n].x = x; _windows[n].y = y;
  if (_begun && !configureWindowHardware(n)) return false;
  _lastError = NoError;
  return true;
}


bool AMT630A_OSD::beginBatchUpdate() {
  if (!_begun) return fail(NotBegun);

  // Nested batches are supported. Only the outermost begin touches C6.
  if (_batchDepth == 0) {
    if (!writeReg(DEV_SYS, 0xC6, _originalC6 & 0x7F))
      return fail(I2CError);
  }

  ++_batchDepth;
  _lastError = NoError;
  return true;
}

bool AMT630A_OSD::endBatchUpdate() {
  // Treat an unmatched end as a harmless no-op. This keeps cleanup code
  // simple and, importantly, never creates a new unlock/relock cycle.
  if (_batchDepth == 0) {
    _lastError = NoError;
    return true;
  }

  --_batchDepth;

  // Nested batches remain open until the outermost end.
  if (_batchDepth > 0) {
    _lastError = NoError;
    return true;
  }

  if (!writeReg(DEV_SYS, 0xC6, _originalC6))
    return fail(I2CError);

  _lastError = NoError;
  return true;
}


bool AMT630A_OSD::moveWindow(uint8_t n, uint16_t x, uint16_t y) {
  if (!validWindow(n)) return fail(InvalidWindow);
  if (!_windows[n].configured) return fail(WindowNotConfigured);
  if (x > 0x7FF || y > 0x7FF) return fail(InvalidDimensions);

  WindowState &w = _windows[n];

  // If the library has not begun yet, just remember the requested position.
  // configureWindowHardware() will apply it later during normal setup.
  if (!_begun) {
    w.x = x;
    w.y = y;
    _lastError = NoError;
    return true;
  }

  const uint16_t oldX = w.x;
  const uint16_t oldY = w.y;

  if (oldX == x && oldY == y) {
    _lastError = NoError;
    return true;
  }

  // AMT630A position registers for windows 0..4.
  static const uint8_t posH[5] = {0x09, 0x14, 0x1A, 0x20, 0x26};
  static const uint8_t posX[5] = {0x0A, 0x15, 0x1B, 0x21, 0x27};
  static const uint8_t posY[5] = {0x0B, 0x16, 0x1C, 0x22, 0x28};

  const uint8_t oldHi = ((oldX >> 8) & 0x07) |
                        (((oldY >> 8) & 0x07) << 4) |
                        (n > 0 ? (((w.indexStart >> 8) & 0x01) << 7) : 0);
  const uint8_t newHi = ((x >> 8) & 0x07) |
                        (((y >> 8) & 0x07) << 4) |
                        (n > 0 ? (((w.indexStart >> 8) & 0x01) << 7) : 0);

  // Keep the C6 unlock window as short as possible and batch all position
  // writes into one transaction. For ordinary horizontal animation within the
  // same 256-pixel page, this is only one OSD register write (posX).
  unlockFast();
  bool ok = true;

  if (oldHi != newHi)
    ok = writeReg(DEV_OSD, posH[n], newHi);

  if (ok && ((oldX & 0xFF) != (x & 0xFF)))
    ok = writeReg(DEV_OSD, posX[n], x & 0xFF);

  if (ok && ((oldY & 0xFF) != (y & 0xFF)))
    ok = writeReg(DEV_OSD, posY[n], y & 0xFF);

  relockFast();

  if (!ok) return fail(I2CError);

  w.x = x;
  w.y = y;
  _lastError = NoError;
  return true;
}

#define GETW(expr) do { if (!validWindow(windowNumber) || !_windows[windowNumber].configured) return 0; return (expr); } while(0)
uint16_t AMT630A_OSD::getWindowX(uint8_t windowNumber) const { GETW(_windows[windowNumber].x); }
uint16_t AMT630A_OSD::getWindowY(uint8_t windowNumber) const { GETW(_windows[windowNumber].y); }
uint16_t AMT630A_OSD::getWindowWidth(uint8_t windowNumber) const { GETW(_windows[windowNumber].width); }
uint16_t AMT630A_OSD::getWindowHeight(uint8_t windowNumber) const { GETW(_windows[windowNumber].height); }
uint8_t AMT630A_OSD::getColumns(uint8_t windowNumber) const { GETW(_windows[windowNumber].columns); }
uint8_t AMT630A_OSD::getRows(uint8_t windowNumber) const { GETW(_windows[windowNumber].rows); }
uint16_t AMT630A_OSD::getWindowIndexStart(uint8_t windowNumber) const { GETW(_windows[windowNumber].indexStart); }
uint16_t AMT630A_OSD::getWindowIndexCells(uint8_t windowNumber) const { GETW(_windows[windowNumber].cellCount); }
#undef GETW

uint8_t AMT630A_OSD::getCharacterWidth(uint8_t windowNumber) const {
  if (!validWindow(windowNumber) || !_windows[windowNumber].configured) return 0;
  uint8_t w,h; characterGeometry(w,h); return w;
}
uint8_t AMT630A_OSD::getCharacterHeight(uint8_t windowNumber) const {
  if (!validWindow(windowNumber) || !_windows[windowNumber].configured) return 0;
  uint8_t w,h; characterGeometry(w,h); return h;
}
uint16_t AMT630A_OSD::getIndexRAMUsed() const {
  uint16_t used = 0;
  for (uint8_t i=0;i<WINDOW_COUNT;++i) if (_windows[i].configured) used += _windows[i].cellCount;
  return used;
}

bool AMT630A_OSD::allocateWindowBuffer(uint8_t n, uint16_t count) {
  Cell *p = new Cell[count];
  if (!p) return fail(AllocationFailed);
  delete[] _windows[n].cells;
  _windows[n].cells = p;
  for (uint16_t i=0;i<count;++i) { p[i].glyph=0; p[i].attr=0; p[i].dirty=true; }
  return true;
}
void AMT630A_OSD::freeWindowBuffer(uint8_t n) { delete[] _windows[n].cells; _windows[n].cells=nullptr; }

bool AMT630A_OSD::reallocateIndexRAM() {
  uint16_t next = 0;
  for (uint8_t i=0;i<WINDOW_COUNT;++i) {
    if (!_windows[i].configured) continue;
    if ((uint32_t)next + _windows[i].cellCount > INDEX_RAM_CELLS) return fail(IndexRAMFull);
    if (_windows[i].indexStart != next) markAllDirty(i);
    _windows[i].indexStart = next;
    next += _windows[i].cellCount;
  }
  if (_begun && !configureAllWindowsHardware()) return false;
  return true;
}

bool AMT630A_OSD::clearWindow(uint8_t n) {
  if (!validWindow(n)) return fail(InvalidWindow);
  WindowState &w=_windows[n]; if (!w.configured) return fail(WindowNotConfigured);
  if (w.mode!=TextMode) return fail(WrongWindowMode);
  for(uint16_t i=0;i<w.cellCount;++i){w.cells[i].glyph=0;w.cells[i].attr=0;w.cells[i].dirty=true;}
  _lastError=NoError; return true;
}

bool AMT630A_OSD::clearRegion(uint8_t n,uint8_t x,uint8_t y,uint8_t width,uint8_t height){
  if(!validWindow(n)) return fail(InvalidWindow); WindowState&w=_windows[n]; if(!w.configured)return fail(WindowNotConfigured); if(w.mode!=TextMode)return fail(WrongWindowMode);
  for(uint16_t yy=y;yy<(uint16_t)y+height && yy<w.rows;++yy) for(uint16_t xx=x;xx<(uint16_t)x+width && xx<w.columns;++xx) setCell(n,(uint8_t)xx,(uint8_t)yy,0,Transparent,Transparent);
  _lastError=NoError; return true;
}

bool AMT630A_OSD::fillRect(uint8_t n,uint8_t x,uint8_t y,uint8_t width,uint8_t height,Color background){
  if(!validWindow(n)) return fail(InvalidWindow); WindowState&w=_windows[n]; if(!w.configured)return fail(WindowNotConfigured); if(w.mode!=TextMode)return fail(WrongWindowMode);
  for(uint16_t yy=y;yy<(uint16_t)y+height && yy<w.rows;++yy) for(uint16_t xx=x;xx<(uint16_t)x+width && xx<w.columns;++xx) setCell(n,(uint8_t)xx,(uint8_t)yy,0,Transparent,background);
  _lastError=NoError; return true;
}

bool AMT630A_OSD::setCell(uint8_t n,uint8_t x,uint8_t y,uint16_t glyph,Color fg,Color bg){
  if(!validWindow(n))return fail(InvalidWindow); WindowState&w=_windows[n]; if(!w.configured)return fail(WindowNotConfigured); if(w.mode!=TextMode)return fail(WrongWindowMode); if(!inBounds(w,x,y))return fail(InvalidDimensions); if(glyph>0x03FF)return fail(InvalidGlyph);
  uint16_t i=cellIndex(w,x,y); uint8_t a=makeAttr(fg,bg); if(w.cells[i].glyph!=glyph||w.cells[i].attr!=a){w.cells[i].glyph=glyph;w.cells[i].attr=a;w.cells[i].dirty=true;} _lastError=NoError;return true;
}
bool AMT630A_OSD::writeGlyph(uint8_t n,uint8_t x,uint8_t y,uint16_t glyph,Color fg,Color bg){return setCell(n,x,y,glyph,fg,bg);}

size_t AMT630A_OSD::writeString(uint8_t n,uint8_t x,uint8_t y,const char*text,Color fg,Color bg){
  if(!validWindow(n)){fail(InvalidWindow);return 0;} WindowState&w=_windows[n]; if(!w.configured){fail(WindowNotConfigured);return 0;} if(w.mode!=TextMode){fail(WrongWindowMode);return 0;} if(!text||y>=w.rows){fail(InvalidDimensions);return 0;}
  size_t si=0,written=0; uint8_t cx=x;
  while(text[si]&&cx<w.columns){
    uint16_t glyph; size_t consumed;
    if(parseGlyphToken(text,si,glyph,consumed)){setCell(n,cx,y,glyph,fg,bg);si+=consumed;}
    else {setCell(n,cx,y,asciiToGlyph(text[si]),fg,bg);++si;}
    ++cx; ++written;
  }
  _lastError=NoError; return written;
}
size_t AMT630A_OSD::writeString(uint8_t n,uint8_t x,uint8_t y,const String&text,Color fg,Color bg){return writeString(n,x,y,text.c_str(),fg,bg);}
size_t AMT630A_OSD::writeStringCentered(uint8_t n,uint8_t y,const char*text,Color fg,Color bg){
  if(!validWindow(n)){fail(InvalidWindow);return 0;} if(!_windows[n].configured){fail(WindowNotConfigured);return 0;} uint16_t px=getTextWidth(n,text);uint8_t cw=getCharacterWidth(n);uint16_t cells=cw?px/cw:0;uint8_t x=cells<_windows[n].columns?(_windows[n].columns-cells)/2:0;return writeString(n,x,y,text,fg,bg);
}
size_t AMT630A_OSD::writeStringCentered(uint8_t n,uint8_t y,const String&text,Color fg,Color bg){return writeStringCentered(n,y,text.c_str(),fg,bg);}
uint16_t AMT630A_OSD::getTextWidth(uint8_t n,const char*text)const{
  if(!validWindow(n)||!_windows[n].configured||!text||_windows[n].mode!=TextMode)return 0;size_t i=0,cells=0;while(text[i]){uint16_t g;size_t consumed;if(parseGlyphToken(text,i,g,consumed))i+=consumed;else ++i;++cells;}uint8_t cw,ch;characterGeometry(cw,ch);return(uint16_t)(cells*cw);
}
size_t AMT630A_OSD::writeBar(uint8_t n,uint8_t x,uint8_t y,uint16_t value,uint16_t maxValue,uint8_t width,Color fg,Color bg){
  if(maxValue==0){fail(InvalidDimensions);return 0;}uint16_t filled=((uint32_t)value*width)/maxValue;if(filled>width)filled=width;for(uint8_t i=0;i<width;++i)setCell(n,x+i,y,i<filled?0x03E:0x03B,fg,bg);return width;
}

bool AMT630A_OSD::configureFontRAM(uint16_t count) {
  if(count>MAX_CUSTOM_GLYPHS)return fail(InvalidGlyphSlot);
  if(_bitmapCount)return fail(BitmapInUse);
  for(uint16_t i=count;i<MAX_CUSTOM_GLYPHS;++i)if(_customGlyphLoaded[i])return fail(InvalidGlyphSlot);

  _customGlyphCapacity=count;

  // With zero custom-glyph reservation, "all of it" means all 4096 words.
  // With custom glyphs reserved, keep the established 1bpp custom region at
  // 0x024 and place the bitmap region after it, aligned to LargeFont height so
  // a 4bpp tileno maps to an exact Font RAM word boundary.
  if(count==0) _bitmapStartWord=0;
  else {
    uint32_t end=(uint32_t)LARGE_CUSTOM_FONT_RAM_BASE+(uint32_t)count*LARGE_CUSTOM_GLYPH_STRIDE;
    if(end>FONT_RAM_WORDS)return fail(FontRAMFull);
    _bitmapStartWord=alignUp((uint16_t)end,LARGE_CUSTOM_GLYPH_STRIDE);
  }
  if(_bitmapStartWord>FONT_RAM_WORDS)return fail(FontRAMFull);
  _bitmapWordsTotal=FONT_RAM_WORDS-_bitmapStartWord;
  _bitmapWordsUsed=0;
  _bitmapNextWord=_bitmapStartWord;
  _bitmapStartTile=0;
  _bitmapStartConfigured=false;
  _lastError=NoError;
  return true;
}

bool AMT630A_OSD::loadCustomGlyph(uint16_t slot,const uint16_t rows[CUSTOM_GLYPH_ROWS]){
  if(!_begun)return fail(NotBegun);
  if(_fontMode!=LargeFont)return fail(CustomGlyphUnsupportedInSmallFont);
  if(_bitmapCount)return fail(CustomGlyphBitmapConflict);
  if(!rows||slot>=_customGlyphCapacity)return fail(InvalidGlyphSlot);
  uint16_t start=LARGE_CUSTOM_FONT_RAM_BASE+slot*LARGE_CUSTOM_GLYPH_STRIDE;
  uint16_t reservedEnd = _customGlyphCapacity ? (LARGE_CUSTOM_FONT_RAM_BASE + _customGlyphCapacity * LARGE_CUSTOM_GLYPH_STRIDE) : 0;
  if((uint32_t)start+LARGE_CUSTOM_GLYPH_STRIDE>reservedEnd)return fail(FontRAMFull);
  if(!writeOSDConservative(0x05,0x00))return false;
  delay(20);
  unlockFast();
  for(uint8_t i=0;i<LARGE_CUSTOM_GLYPH_STRIDE;++i){
    uint16_t d=(i<CUSTOM_GLYPH_ROWS)?(rows[i]&0x0FFF):0;
    if(!writeFontRamWord(start+i,d)){relockFast();applyVisibility();return fail(I2CError);}
  }
  relockFast();
  if(!applyVisibility())return false;
  if(!_customGlyphLoaded[slot]){_customGlyphLoaded[slot]=true;++_customGlyphLoadedCount;}
  _lastError=NoError;return true;
}
uint16_t AMT630A_OSD::getCustomGlyphId(uint16_t slot) const{return slot<_customGlyphCapacity?LARGE_CUSTOM_GLYPH_BASE+slot:0xFFFF;}
bool AMT630A_OSD::writeCustomGlyph(uint8_t n,uint8_t x,uint8_t y,uint16_t slot,Color fg,Color bg){
  if(_fontMode!=LargeFont)return fail(CustomGlyphUnsupportedInSmallFont);
  if(_bitmapCount)return fail(CustomGlyphBitmapConflict);
  if(slot>=_customGlyphCapacity)return fail(InvalidGlyphSlot);
  return writeGlyph(n,x,y,getCustomGlyphId(slot),fg,bg);
}

uint16_t AMT630A_OSD::readProgramWord(const uint16_t *ptr) const {
#if defined(ARDUINO_ARCH_AVR)
  return pgm_read_word(ptr);
#else
  return *ptr;
#endif
}

uint16_t AMT630A_OSD::rgbToBitmapColor(uint8_t red,uint8_t green,uint8_t blue){
  uint16_t r=(uint16_t)((red+8)/17); if(r>15)r=15;
  uint16_t g=(uint16_t)((green+8)/17); if(g>15)g=15;
  uint16_t b=(uint16_t)((blue+8)/17); if(b>15)b=15;
  return (uint16_t)((b<<8)|(g<<4)|r);
}

bool AMT630A_OSD::setBitmapPaletteColorRaw(uint8_t index,uint16_t bgr12){
  if(index>=BITMAP_PALETTE_SIZE)return fail(InvalidPaletteIndex);
  bgr12&=0x0FFF;
  _bitmapPalette[index]=bgr12;
  if(_begun){
    uint8_t reg=0x36+index*2;
    if(!writeOSDConservative(reg,(bgr12>>8)&0x0F))return false;
    if(!writeOSDConservative(reg+1,bgr12&0xFF))return false;
  }
  _lastError=NoError;return true;
}

bool AMT630A_OSD::setBitmapPaletteColor(uint8_t index,uint8_t red,uint8_t green,uint8_t blue){
  return setBitmapPaletteColorRaw(index,rgbToBitmapColor(red,green,blue));
}

uint16_t AMT630A_OSD::getBitmapPaletteColorRaw(uint8_t index) const{
  return index<BITMAP_PALETTE_SIZE?_bitmapPalette[index]:0;
}

bool AMT630A_OSD::setBitmapPalette0Transparent(bool transparent){
  _bitmapPalette0Transparent=transparent;
  if(_begun){
    uint8_t v=readOSDReg(0x35); if(v==0xFF)return fail(I2CError);
    if(transparent)v&=~0x10; else v|=0x10;
    if(!writeOSDConservative(0x35,v))return false;
  }
  _lastError=NoError;return true;
}

bool AMT630A_OSD::applyBitmapPalette(const Bitmap &bitmap){
  if(!bitmap.palette || bitmap.paletteSize==0 || bitmap.paletteSize>BITMAP_PALETTE_SIZE)return fail(InvalidBitmap);
  for(uint8_t i=0;i<BITMAP_PALETTE_SIZE;++i){
    uint16_t c=(i<bitmap.paletteSize)?readProgramWord(bitmap.palette+i):0;
    if(!setBitmapPaletteColorRaw(i,c))return false;
  }
  return setBitmapPalette0Transparent(bitmap.palette0Transparent);
}

bool AMT630A_OSD::writeFontRamBlock(uint16_t startAddress,const uint16_t *data,uint16_t wordCount){
  if(!data || (uint32_t)startAddress+wordCount>FONT_RAM_WORDS)return fail(InvalidBitmap);
  if(!writeOSDConservative(0x05,0x00))return false;
  delay(20);

  const uint16_t CHUNK=32;
  uint16_t pos=0;
  while(pos<wordCount){
    uint16_t n=(wordCount-pos>CHUNK)?CHUNK:(wordCount-pos);
    unlockFast();
    for(uint16_t i=0;i<n;++i){
      if(!writeFontRamWord(startAddress+pos+i,readProgramWord(data+pos+i))){
        relockFast(); applyVisibility(); return fail(I2CError);
      }
    }
    relockFast();
    pos+=n;
    if(pos<wordCount)delay(20);
  }
  return true;
}

bool AMT630A_OSD::configureBitmapStartHardware(){
  if(!_begun || !_bitmapStartConfigured)return true;
  if(!writeOSDConservative(0x11,_bitmapStartTile&0xFF))return false;
  uint8_t high=readOSDReg(0x70); if(high==0xFF)return fail(I2CError);
  high=(high&0xFC)|((_bitmapStartTile>>8)&0x03);
  return writeOSDConservative(0x70,high);
}

bool AMT630A_OSD::loadBitmap(const Bitmap &bitmap,BitmapHandle &handle,bool applyPalette){
  handle=INVALID_BITMAP_HANDLE;
  if(!_begun)return fail(NotBegun);
  if(_fontMode!=LargeFont)return fail(BitmapUnsupportedInSmallFont);
  if(_customGlyphLoadedCount)return fail(CustomGlyphBitmapConflict);
  if(!bitmap.data || !bitmap.palette || !bitmap.width || !bitmap.height || !bitmap.columns || !bitmap.rows)return fail(InvalidBitmap);

  uint8_t cw,ch;characterGeometry(cw,ch);
  if(bitmap.tileWidth!=cw || bitmap.tileHeight!=ch)return fail(BitmapGeometryMismatch);
  uint8_t wordsPerRow=(bitmap.tileWidth+3)/4;
  uint32_t expected=(uint32_t)bitmap.columns*bitmap.rows*wordsPerRow*bitmap.tileHeight;
  if(expected!=bitmap.wordCount || bitmap.paletteSize==0 || bitmap.paletteSize>16)return fail(InvalidBitmap);
  if((uint32_t)_bitmapWordsUsed+bitmap.wordCount>_bitmapWordsTotal)return fail(BitmapMemoryFull);

  int slot=-1;
  for(uint8_t i=0;i<MAX_BITMAPS;++i)if(!_bitmaps[i].loaded){slot=i;break;}
  if(slot<0)return fail(BitmapSlotsFull);

  uint16_t startWord=_bitmapNextWord;
  if(startWord%bitmap.tileHeight)return fail(BitmapGeometryMismatch);
  uint16_t startTile=startWord/bitmap.tileHeight;
  uint32_t lastTile=(uint32_t)startTile+(uint32_t)(bitmap.columns*bitmap.rows-1)*wordsPerRow;
  uint32_t firstGlyph=(uint32_t)CUSTOM_RAM_GLYPH_BASE+startTile;
  uint32_t lastGlyph=(uint32_t)CUSTOM_RAM_GLYPH_BASE+lastTile;
  if(firstGlyph>0x3FF || lastGlyph>0x3FF)return fail(BitmapMemoryFull);

  if(!writeFontRamBlock(startWord,bitmap.data,bitmap.wordCount))return false;

  if(!_bitmapStartConfigured){
    _bitmapStartTile=startTile;
    _bitmapStartConfigured=true;
    if(!configureBitmapStartHardware()){applyVisibility();return false;}
  }

  BitmapResource &b=_bitmaps[(uint8_t)slot];
  b.loaded=true;b.width=bitmap.width;b.height=bitmap.height;b.tileWidth=bitmap.tileWidth;b.tileHeight=bitmap.tileHeight;
  b.columns=bitmap.columns;b.rows=bitmap.rows;b.wordsPerRow=wordsPerRow;b.wordCount=bitmap.wordCount;
  b.startWord=startWord;b.startTile=startTile;b.firstGlyph=(uint16_t)firstGlyph;

  _bitmapNextWord+=bitmap.wordCount;
  _bitmapWordsUsed=_bitmapNextWord-_bitmapStartWord;
  ++_bitmapCount;

  if(applyPalette && !applyBitmapPalette(bitmap)){applyVisibility();return false;}
  if(!applyVisibility())return false;

  handle=(BitmapHandle)slot;
  _lastError=NoError;
  return true;
}

bool AMT630A_OSD::clearBitmaps(){
  if(anyBitmapWindowConfigured())return fail(BitmapInUse);
  for(uint8_t i=0;i<MAX_BITMAPS;++i){memset(&_bitmaps[i],0,sizeof(BitmapResource));_bitmaps[i].loaded=false;}
  _bitmapCount=0;_bitmapWordsUsed=0;_bitmapNextWord=_bitmapStartWord;_bitmapStartTile=0;_bitmapStartConfigured=false;
  if(_begun && !applyVisibility())return false;
  _lastError=NoError;return true;
}

bool AMT630A_OSD::isBitmapLoaded(BitmapHandle h) const{return validBitmapHandle(h);}
uint16_t AMT630A_OSD::getBitmapWidth(BitmapHandle h) const{return validBitmapHandle(h)?_bitmaps[(uint8_t)h].width:0;}
uint16_t AMT630A_OSD::getBitmapHeight(BitmapHandle h) const{return validBitmapHandle(h)?_bitmaps[(uint8_t)h].height:0;}
uint16_t AMT630A_OSD::getBitmapStorageBytes(BitmapHandle h) const{return validBitmapHandle(h)?(uint16_t)(_bitmaps[(uint8_t)h].wordCount*2U):0;}

bool AMT630A_OSD::setBlendingEnabled(bool enabled) {
  if (!_begun) return fail(NotBegun);

  unlockFast();
  uint8_t fb06 = readReg(DEV_OSD, 0x06);

  if (enabled) {
    // Hardware-verified mode: blend the complete OSD block with video.
    // Preserve the six blink-rate bits while enabling mix_en + mix_mode.
    fb06 |= 0xC0;
  } else {
    // Disable mixing while leaving the selected mix mode undisturbed.
    fb06 &= (uint8_t)~0x40;
  }

  const bool ok = writeReg(DEV_OSD, 0x06, fb06);
  relockFast();

  if (!ok) return fail(I2CError);
  _blendingEnabled = enabled;
  _lastError = NoError;
  return true;
}

bool AMT630A_OSD::setOpacity(uint8_t opacity) {
  if (!_begun) return fail(NotBegun);
  if (opacity > 7) return fail(InvalidOpacity);

  unlockFast();
  uint8_t fb0c = readReg(DEV_OSD, 0x0C);
  fb0c = (fb0c & 0xF8) | (opacity & 0x07);
  const bool ok = writeReg(DEV_OSD, 0x0C, fb0c);
  relockFast();

  if (!ok) return fail(I2CError);
  _opacity = opacity;
  _brightness = (fb0c >> 3) & 0x1F;
  _lastError = NoError;
  return true;
}

bool AMT630A_OSD::setBrightness(uint8_t brightness) {
  if (!_begun) return fail(NotBegun);
  if (brightness > 31) return fail(InvalidBrightness);

  unlockFast();
  uint8_t fb0c = readReg(DEV_OSD, 0x0C);
  fb0c = (uint8_t)((brightness << 3) | (fb0c & 0x07));
  const bool ok = writeReg(DEV_OSD, 0x0C, fb0c);
  relockFast();

  if (!ok) return fail(I2CError);
  _brightness = brightness;
  _opacity = fb0c & 0x07;
  _lastError = NoError;
  return true;
}

bool AMT630A_OSD::applyWindowScaleHardware(uint8_t n) {
  if (!validWindow(n)) return fail(InvalidWindow);
  if (!_windows[n].configured) return fail(WindowNotConfigured);

  const uint8_t sx = _windows[n].scaleX;
  const uint8_t sy = _windows[n].scaleY;
  if (sx < 1 || sx > 4 || sy < 1 || sy > 4) return fail(InvalidScale);

  unlockFast();
  bool ok = true;

  if (n == 0) {
    // Window 0 is the AMT630A special case. Its coefficients encode 2x..5x,
    // while separate masks choose which source rows/pixels are expanded.
    // To provide the same simple API as Windows 1..4, enable every mask bit
    // for an axis whenever that axis is scaled above 1x. Clearing all mask
    // bits produces 1x regardless of the coefficient.
    const uint8_t hMask = (sx > 1) ? 0xFF : 0x00;
    const uint8_t vMask = (sy > 1) ? 0xFF : 0x00;

    ok = writeReg(DEV_OSD, 0x2B, vMask) &&
         writeReg(DEV_OSD, 0x2C, vMask) &&
         writeReg(DEV_OSD, 0x2D, vMask) &&
         writeReg(DEV_OSD, 0x2E, vMask) &&
         writeReg(DEV_OSD, 0x2F, hMask) &&
         writeReg(DEV_OSD, 0x30, hMask) &&
         writeReg(DEV_OSD, 0x31, hMask);

    if (ok) {
      uint8_t fb32 = readReg(DEV_OSD, 0x32);
      fb32 &= 0xF0;
      const uint8_t hx = (sx <= 1) ? 0 : (uint8_t)(sx - 2);
      const uint8_t vy = (sy <= 1) ? 0 : (uint8_t)(sy - 2);
      fb32 |= (hx & 0x03);
      fb32 |= (uint8_t)((vy & 0x03) << 2);
      ok = writeReg(DEV_OSD, 0x32, fb32);
    }
  } else {
    // Windows 1..4 encode 1x..4x directly as 0..3. Windows 1/2 share FB33
    // and Windows 3/4 share FB34, so preserve the neighboring window field.
    const uint8_t reg = (n <= 2) ? 0x33 : 0x34;
    const uint8_t shift = ((n == 1) || (n == 3)) ? 0 : 4;
    uint8_t value = readReg(DEV_OSD, reg);
    const uint8_t fieldMask = (uint8_t)(0x0F << shift);
    value &= (uint8_t)~fieldMask;
    value |= (uint8_t)(((sx - 1) & 0x03) << shift);
    value |= (uint8_t)(((sy - 1) & 0x03) << (shift + 2));
    ok = writeReg(DEV_OSD, reg, value);
  }

  relockFast();
  if (!ok) return fail(I2CError);
  _lastError = NoError;
  return true;
}

bool AMT630A_OSD::setWindowScale(uint8_t n, uint8_t scale) {
  return setWindowScale(n, scale, scale);
}

bool AMT630A_OSD::setWindowScale(uint8_t n, uint8_t sx, uint8_t sy) {
  if (!validWindow(n)) return fail(InvalidWindow);
  if (!_windows[n].configured) return fail(WindowNotConfigured);
  if (sx < 1 || sx > 4 || sy < 1 || sy > 4) return fail(InvalidScale);

  const uint8_t oldX = _windows[n].scaleX;
  const uint8_t oldY = _windows[n].scaleY;
  _windows[n].scaleX = sx;
  _windows[n].scaleY = sy;

  if (_begun && !applyWindowScaleHardware(n)) {
    _windows[n].scaleX = oldX;
    _windows[n].scaleY = oldY;
    return false;
  }

  _lastError = NoError;
  return true;
}

uint8_t AMT630A_OSD::getWindowScaleX(uint8_t n) const {
  return (validWindow(n) && _windows[n].configured) ? _windows[n].scaleX : 0;
}

uint8_t AMT630A_OSD::getWindowScaleY(uint8_t n) const {
  return (validWindow(n) && _windows[n].configured) ? _windows[n].scaleY : 0;
}

bool AMT630A_OSD::configureBlink(uint8_t n,uint8_t x,uint8_t y,uint8_t width,uint8_t height,uint8_t rawRate){
  if(!validWindow(n))return fail(InvalidWindow);
  if(!_windows[n].configured)return fail(WindowNotConfigured);
  // Hardware-verified blink coordinates are 1-based: first cell is (1,1).
  if(!width||!height||x<1||y<1||
     (uint16_t)x+width-1>_windows[n].columns||
     (uint16_t)y+height-1>_windows[n].rows)
    return fail(InvalidDimensions);
  if(rawRate>0x3F)return fail(InvalidDimensions);
  _blinkWindow=n;_blinkX=x;_blinkY=y;_blinkWidth=width;_blinkHeight=height;_blinkRateRaw=rawRate;_blinkManaged=true;
  if(_begun&&!applyBlinkHardware())return false;
  _lastError=NoError;return true;
}

bool AMT630A_OSD::setBlinkWindow(uint8_t n){
  if(!validWindow(n))return fail(InvalidWindow);
  if(!_windows[n].configured)return fail(WindowNotConfigured);
  if(_blinkWidth&&_blinkHeight&&
     (_blinkX<1||_blinkY<1||
      (uint16_t)_blinkX+_blinkWidth-1>_windows[n].columns||
      (uint16_t)_blinkY+_blinkHeight-1>_windows[n].rows))
    return fail(InvalidDimensions);
  _blinkWindow=n;_blinkManaged=true;
  if(_begun&&!applyBlinkHardware())return false;
  _lastError=NoError;return true;
}

bool AMT630A_OSD::setBlinkRegion(uint8_t x,uint8_t y,uint8_t width,uint8_t height){
  if(!validWindow(_blinkWindow))return fail(InvalidWindow);
  if(!_windows[_blinkWindow].configured)return fail(WindowNotConfigured);
  const WindowState&w=_windows[_blinkWindow];
  if(!width||!height||x<1||y<1||
     (uint16_t)x+width-1>w.columns||
     (uint16_t)y+height-1>w.rows)
    return fail(InvalidDimensions);
  _blinkX=x;_blinkY=y;_blinkWidth=width;_blinkHeight=height;_blinkManaged=true;
  if(_begun&&!applyBlinkHardware())return false;
  _lastError=NoError;return true;
}

bool AMT630A_OSD::setBlinkRateRaw(uint8_t rawRate){
  if(rawRate>0x3F)return fail(InvalidDimensions);
  _blinkRateRaw=rawRate;_blinkManaged=true;
  if(_begun&&!applyBlinkHardware())return false;
  _lastError=NoError;return true;
}

bool AMT630A_OSD::setBlinkEnabled(bool enabled){
  _blinkEnabled=enabled;_blinkManaged=true;
  if(_begun&&!applyVisibility())return false;
  if(_begun&&enabled&&!applyBlinkHardware())return false;
  _lastError=NoError;return true;
}

bool AMT630A_OSD::updateDisplay(){if(!_begun)return fail(NotBegun);for(uint8_t i=0;i<WINDOW_COUNT;++i)if(_windows[i].configured&&_windows[i].visible&&_osdVisible)if(!flushWindow(i,false))return false;_lastError=NoError;return true;}
bool AMT630A_OSD::updateDisplay(uint8_t n){if(!_begun)return fail(NotBegun);if(!validWindow(n))return fail(InvalidWindow);if(!_windows[n].configured)return fail(WindowNotConfigured);if(!_windows[n].visible||!_osdVisible)return true;return flushWindow(n,false);}
bool AMT630A_OSD::forceUpdate(){if(!_begun)return fail(NotBegun);for(uint8_t i=0;i<WINDOW_COUNT;++i)if(_windows[i].configured&&_windows[i].visible&&_osdVisible)if(!flushWindow(i,true))return false;_lastError=NoError;return true;}
bool AMT630A_OSD::forceUpdate(uint8_t n){if(!_begun)return fail(NotBegun);if(!validWindow(n))return fail(InvalidWindow);if(!_windows[n].configured)return fail(WindowNotConfigured);return flushWindow(n,true);}

void AMT630A_OSD::setOSDVisible(bool visible){_osdVisible=visible;if(_begun)applyVisibility();}
bool AMT630A_OSD::setVisible(uint8_t n,bool visible){if(!validWindow(n))return fail(InvalidWindow);if(!_windows[n].configured)return fail(WindowNotConfigured);_windows[n].visible=visible;if(_begun&&!applyVisibility())return false;_lastError=NoError;return true;}
bool AMT630A_OSD::isVisible(uint8_t n) const{return validWindow(n)&&_windows[n].configured&&_windows[n].visible;}

void AMT630A_OSD::service(){if(!_begun)return;unsigned long now=millis();if((unsigned long)(now-_lastServiceMs)<_serviceIntervalMs)return;_lastServiceMs=now;applyVisibility();}
void AMT630A_OSD::setFactoryOSDCoexistence(bool enabled){_factoryOSDCoexistence=enabled;if(_begun)applyVisibility();}

uint8_t AMT630A_OSD::readReg(uint8_t dev,uint8_t reg){_wire->beginTransmission(dev);_wire->write(reg);if(_wire->endTransmission(false)!=0)return 0xFF;_wire->requestFrom((int)dev,1);return _wire->available()?_wire->read():0xFF;}
bool AMT630A_OSD::writeReg(uint8_t dev,uint8_t reg,uint8_t value){_wire->beginTransmission(dev);_wire->write(reg);_wire->write(value);return _wire->endTransmission()==0;}
void AMT630A_OSD::unlockFast(){
  if(_batchDepth>0)return;
  writeReg(DEV_SYS,0xC6,_originalC6&0x7F);
}
void AMT630A_OSD::relockFast(){
  if(_batchDepth>0)return;
  writeReg(DEV_SYS,0xC6,_originalC6);
}
uint8_t AMT630A_OSD::readOSDReg(uint8_t reg){unlockFast();uint8_t v=readReg(DEV_OSD,reg);relockFast();return v;}
bool AMT630A_OSD::writeOSDConservative(uint8_t reg,uint8_t value){unlockFast();bool ok=writeReg(DEV_OSD,reg,value);delayMicroseconds(500);relockFast();if(!ok)fail(I2CError);return ok;}

bool AMT630A_OSD::configureGlobalHardware(){
  if(!_begun)return fail(NotBegun);uint8_t cw,ch;characterGeometry(cw,ch);if(!writeOSDConservative(0x78,0x07))return false;if(!writeOSDConservative(0x76,cw))return false;if(!writeOSDConservative(0x77,ch))return false;
  const uint8_t palette[6][2]={{0x0F,0xFF},{0x00,0x0F},{0x00,0xF0},{0x0F,0x00},{0x0F,0xF0},{0x00,0xFF}};for(uint8_t i=0;i<6;++i){uint8_t r=0x56+i*2;if(!writeOSDConservative(r,palette[i][0])||!writeOSDConservative(r+1,palette[i][1]))return false;}
  if(!setBitmapPalette0Transparent(_bitmapPalette0Transparent))return false;
  if(_bitmapStartConfigured&&!configureBitmapStartHardware())return false;
  return true;
}

bool AMT630A_OSD::configureWindowHardware(uint8_t n){
  if(!_begun)return fail(NotBegun);if(!validWindow(n))return fail(InvalidWindow);WindowState&w=_windows[n];if(!w.configured)return fail(WindowNotConfigured);
  static const uint8_t sizeX[5]={0x07,0x12,0x18,0x1E,0x24};static const uint8_t sizeY[5]={0x08,0x13,0x19,0x1F,0x25};static const uint8_t posH[5]={0x09,0x14,0x1A,0x20,0x26};static const uint8_t posX[5]={0x0A,0x15,0x1B,0x21,0x27};static const uint8_t posY[5]={0x0B,0x16,0x1C,0x22,0x28};static const uint8_t idxL[5]={0x00,0x17,0x1D,0x23,0x29};
  // Hardware-verified: size registers store the number of displayed cells/tiles
  // directly. Writing N-1 / M-1 makes a requested 7x3 text window display as
  // 6x2, so both TextMode and BitmapMode use their direct counts.
  const uint8_t hwColumns = w.columns;
  const uint8_t hwRows    = w.rows;
  if(!writeOSDConservative(sizeX[n],hwColumns)||!writeOSDConservative(sizeY[n],hwRows))return false;
  uint8_t hi=((w.x>>8)&0x07)|(((w.y>>8)&0x07)<<4);if(n>0)hi|=((w.indexStart>>8)&1)<<7;if(!writeOSDConservative(posH[n],hi)||!writeOSDConservative(posX[n],w.x&0xFF)||!writeOSDConservative(posY[n],w.y&0xFF))return false;if(n>0&&!writeOSDConservative(idxL[n],w.indexStart&0xFF))return false;return true;
}
bool AMT630A_OSD::configureAllWindowsHardware(){if(!_begun)return true;for(uint8_t i=0;i<WINDOW_COUNT;++i)if(_windows[i].configured&&!configureWindowHardware(i))return false;return true;}

bool AMT630A_OSD::applyBlinkHardware(){
  if(!_begun)return fail(NotBegun);
  if(!validWindow(_blinkWindow))return fail(InvalidWindow);
  const WindowState&w=_windows[_blinkWindow];
  if(!w.configured)return fail(WindowNotConfigured);
  if(!_blinkWidth||!_blinkHeight||_blinkX<1||_blinkY<1||
     (uint16_t)_blinkX+_blinkWidth-1>w.columns||
     (uint16_t)_blinkY+_blinkHeight-1>w.rows)
    return fail(InvalidDimensions);

  unlockFast();
  bool ok=true;
  uint8_t fb35=readReg(DEV_OSD,0x35);
  uint8_t fb06=readReg(DEV_OSD,0x06);
  if(fb35==0xFF||fb06==0xFF)ok=false;
  if(ok){
    fb35=(fb35&0xF8)|(_blinkWindow&0x07);
    fb06=(fb06&0xC0)|(_blinkRateRaw&0x3F);
    ok=writeReg(DEV_OSD,0x35,fb35)&&
       writeReg(DEV_OSD,0x79,_blinkY)&&
       writeReg(DEV_OSD,0x7A,(uint8_t)(_blinkY+_blinkHeight-1))&&
       writeReg(DEV_OSD,0x7B,_blinkX)&&
       writeReg(DEV_OSD,0x7C,(uint8_t)(_blinkX+_blinkWidth-1))&&
       writeReg(DEV_OSD,0x06,fb06);
  }