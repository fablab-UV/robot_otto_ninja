#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <ESP32Servo.h>

// =====================================================
// 1. PANTALLAS OLED (2 PANTALLAS)
// =====================================================

U8G2_SSD1306_128X64_NONAME_F_HW_I2C displayL(
  U8G2_R0,
  U8X8_PIN_NONE,
  22,
  21
);

U8G2_SSD1306_128X64_NONAME_F_SW_I2C displayR(
  U8G2_R0,
  17,
  16,
  U8X8_PIN_NONE
);

class RobotEyes {
private:
  U8G2 &oledL;
  U8G2 &oledR;
  bool parpadeando = false;
  unsigned long finParpadeoMs = 0;
  bool amor = false;
  bool amorGrande = false;
  unsigned long sigLatidoMs = 0;
  unsigned long finAmorMs = 0;

  void dibujarCorazon(U8G2 &d, float s) {
    int r  = (int)(12 * s);
    int dx = (int)(10 * s);
    d.drawDisc(64 - dx, 25, r);
    d.drawDisc(64 + dx, 25, r);
    d.drawTriangle(64 - (int)(22 * s), 27, 64 + (int)(22 * s), 27, 64, 27 + (int)(25 * s));
  }

  void dibujarAmor() {
    clear();
    float s = amorGrande ? 1.15 : 0.95;
    dibujarCorazon(oledL, s);
    dibujarCorazon(oledR, s);
    update();
  }

public:
  RobotEyes(U8G2 &leftDisplay, U8G2 &rightDisplay)
    : oledL(leftDisplay), oledR(rightDisplay) {}

  void begin() {
    oledL.begin();
    oledR.begin();
    normal();
  }

  void clear() {
    oledL.clearBuffer();
    oledR.clearBuffer();
  }

  void update() {
    oledL.sendBuffer();
    oledR.sendBuffer();
  }

  void normal() {
    parpadeando = false;
    amor = false;
    finAmorMs = 0;
    clear();
    oledL.drawRBox(30, 15, 68, 34, 8);
    oledR.drawRBox(30, 15, 68, 34, 8);
    update();
  }

  void happy() {
    parpadeando = false;
    amor = false;
    finAmorMs = 0;
    clear();
    oledL.drawCircle(64, 32, 22, U8G2_DRAW_UPPER_RIGHT | U8G2_DRAW_UPPER_LEFT);
    oledR.drawCircle(64, 32, 22, U8G2_DRAW_UPPER_RIGHT | U8G2_DRAW_UPPER_LEFT);
    update();
  }

  void surprise() {
    parpadeando = false;
    amor = false;
    finAmorMs = 0;
    clear();
    oledL.drawCircle(64, 32, 22);
    oledR.drawCircle(64, 32, 22);
    update();
  }

  // Ojos de corazón que laten. Si durMs > 0 vuelve solo a los ojos normales.
  void heart(unsigned long durMs = 0) {
    parpadeando = false;
    amor = true;
    amorGrande = true;
    sigLatidoMs = millis() + 350;
    finAmorMs = durMs > 0 ? millis() + durMs : 0;
    dibujarAmor();
  }

  void startBlink(unsigned long duracionMs = 120) {
    amor = false;
    finAmorMs = 0;
    clear();
    oledL.drawBox(30, 30, 68, 4);
    oledR.drawBox(30, 30, 68, 4);
    update();
    parpadeando = true;
    finParpadeoMs = millis() + duracionMs;
  }

  void actualizar() {
    if (parpadeando && millis() >= finParpadeoMs) {
      normal();
      return;
    }
    if (amor) {
      if (finAmorMs > 0 && millis() >= finAmorMs) {
        normal();
        return;
      }
      if (millis() >= sigLatidoMs) {
        amorGrande = !amorGrande;
        sigLatidoMs = millis() + 350;
        dibujarAmor();
      }
    }
  }

  bool estaParpadeando() {
    return parpadeando;
  }
};

RobotEyes ojos(displayL, displayR);

unsigned long sigAccionOjos = 0;
unsigned long finBloqueoExpresion = 0;

void actualizarOjosAleatorios() {
  ojos.actualizar();
  if (millis() < finBloqueoExpresion) return;
  if (ojos.estaParpadeando()) return;

  if (millis() >= sigAccionOjos) {
    int r = random(1, 100);
    if (r <= 50)      ojos.heart(1500);   // antes era el guiño
    else if (r <= 75) ojos.happy();
    else              ojos.surprise();

    sigAccionOjos = millis() + random(3000, 6000);
  }
}

// =====================================================
// 2. SERVOS Y CONSTANTES CALIBRADAS
// =====================================================

Servo servoRightLeg;  // D18: Pierna Derecha (180°)
Servo servoRightFoot; // D13: Rueda Derecha (360°)
Servo servoLeftFoot;  // D4 : Rueda Izquierda (360°)
Servo servoLeftLeg;   // D27: Pierna Izquierda (180°)

const int PIN_RIGHT_LEG  = 18;
const int PIN_RIGHT_FOOT = 13;
const int PIN_LEFT_FOOT  = 4;
const int PIN_LEFT_LEG   = 27;

const int LNP = 90;
const int RNP = 90;
const int LFS = 87; // Freno real medido rueda izquierda
const int RFS = 90; // Freno medido rueda derecha

const int GAIN_L = 70; // rueda izquierda al 70% para igualar con la derecha

int angLL = 90;
int angRL = 90;

const int PIN_BUZZER = 25;
const int BUZZER_CH  = 15;

// Si al caminar las patas "aplauden" en vez de dar pasos, cambia esto:
// true  = pierna derecha en espejo (una pierna se mueve al revés de la otra)
// false = ambas piernas con el mismo signo
const bool ESPEJO_PIERNA_DER = true;

// =====================================================
// 3. MOVIMIENTO SUAVE Y CONTROL
// =====================================================

void escribirPiernas(int izq, int der) {
  angLL = constrain(izq, 0, 180);
  angRL = constrain(der, 0, 180);
  servoLeftLeg.write(angLL);
  servoRightLeg.write(angRL);
}

void moveServosSmooth(int end1, int end2, int steps, int delayTime) {
  int start1 = angLL;
  int start2 = angRL;
  int diff1 = end1 - start1;
  int diff2 = end2 - start2;
  for (int i = 0; i <= steps; i++) {
    escribirPiernas(start1 + (diff1 * i / steps), start2 + (diff2 * i / steps));
    delay(delayTime);
  }
}

void frenarRuedas() {
  servoLeftFoot.write(LFS);
  servoRightFoot.write(RFS);
}

// Velocidad de ruedas: positivo = adelante, negativo = atrás
void ruedas(int vIzq, int vDer) {
  servoLeftFoot.write(constrain(LFS + vIzq * GAIN_L / 100, 0, 180));
  servoRightFoot.write(constrain(RFS - vDer, 0, 180));
}

void returnToNeutral() {
  moveServosSmooth(LNP, RNP, 30, 15);
  frenarRuedas();
}

// Ángulos de las piernas para un paso de amplitud d (positivo o negativo)
int pasoL(int d) { return LNP + d; }
int pasoR(int d) { return RNP + (ESPEJO_PIERNA_DER ? -d : d); }

// =====================================================
// 4. CINEMÁTICA MECÁNICA SINCRONIZADA
// =====================================================

bool modoCaminar = false;
const int AMP_PASO = 25;

void roll() {
  moveServosSmooth(160, 20, 25, 15);
}

void steps() {
  modoCaminar = true;
  returnToNeutral();
  delay(80);

  for (int c = 0; c < 3; c++) {
    ruedas(24, 24);
    moveServosSmooth(pasoL(-AMP_PASO), pasoR(-AMP_PASO), 20, 15);
    delay(60);
    frenarRuedas();

    ruedas(24, 24);
    moveServosSmooth(pasoL(AMP_PASO), pasoR(AMP_PASO), 20, 15);
    delay(60);
    frenarRuedas();
  }
  returnToNeutral();
}

void moonWalk() {
  modoCaminar = true;
  returnToNeutral();
  delay(80);

  for (int i = 0; i < 3; i++) {
    ruedas(-24, -24);
    moveServosSmooth(pasoL(AMP_PASO), pasoR(AMP_PASO), 22, 15);
    delay(80);

    ruedas(-24, -24);
    moveServosSmooth(pasoL(-AMP_PASO), pasoR(-AMP_PASO), 22, 15);
    delay(80);
  }
  frenarRuedas();
  returnToNeutral();
}

// =====================================================
// 5. CONTROL CONTINUO POR JOYSTICK (ambos modos)
// =====================================================

unsigned long ultimoComandoJoystick = 0;

// Marcha con joystick (modo caminar)
bool marchaActiva = false;
int marchaDir = 1;
int marchaFase = 0;
unsigned long marchaSigMs = 0;
unsigned long marchaPasoMs = 0;

void aplicarJoystick(int x, int y) {
  ultimoComandoJoystick = millis();

  if (abs(x) < 8 && abs(y) < 8) {
    frenarRuedas();
    marchaActiva = false;
    return;
  }

  float factorV = modoCaminar ? 0.25 : 0.40;
  int vAdelante = (int)(y * factorV);
  int vGiro     = (int)(x * factorV * 0.7);

  ruedas(vAdelante + vGiro, vAdelante - vGiro);

  if (modoCaminar) {
    marchaActiva = true;
    marchaDir = (y < -8) ? -1 : 1;
  }
}

// Mueve las piernas sin bloquear el loop
void actualizarMarcha() {
  if (!modoCaminar) return;
  unsigned long ahora = millis();

  if (marchaActiva && ahora >= marchaSigMs) {
    marchaFase ^= 1;
    marchaSigMs = ahora + 350;
  }

  int objL = LNP, objR = RNP;
  if (marchaActiva) {
    bool a = (marchaFase == 0) == (marchaDir > 0);
    int d = a ? -AMP_PASO : AMP_PASO;
    objL = pasoL(d);
    objR = pasoR(d);
  }

  if (ahora - marchaPasoMs >= 15) {
    marchaPasoMs = ahora;
    if (angLL != objL || angRL != objR) {
      escribirPiernas(angLL + constrain(objL - angLL, -3, 3),
                      angRL + constrain(objR - angRL, -3, 3));
    }
  }
}

void watchdogJoystick() {
  if (ultimoComandoJoystick > 0 && (millis() - ultimoComandoJoystick > 650)) {
    frenarRuedas();
    marchaActiva = false;
    ultimoComandoJoystick = 0;
  }
}

// =====================================================
// 6. MELODÍAS POPULARES
// =====================================================

struct Nota { int frec; int ms; };
struct Cancion { const char* nombre; const Nota* notas; int total; };

const Nota MARIO[] = {
  {659,120},{659,120},{0,90},{659,120},{0,90},{523,120},{659,160},{784,220},{0,120},{392,240},{0,150}
};
const Nota STAR_WARS[] = {
  {392,350},{392,350},{392,350},{311,250},{466,150},{392,350},{311,250},{466,150},{392,450},{0,150}
};
const Nota TETRIS[] = {
  {659,250},{494,125},{523,125},{587,250},{523,125},{494,125},{440,250},{440,125},{523,125},{659,250},{587,125},{523,125},
  {494,375},{523,125},{587,250},{659,250},{523,250},{440,250},{440,350},{0,150}
};
const Nota ASTRONOMIA[] = {
  {587,150},{587,150},{587,150},{587,150},{659,150},{659,150},{659,150},{659,150},
  {698,150},{698,150},{698,150},{698,150},{784,150},{784,150},{784,150},{784,150},
  {880,180},{0,60},{880,180},{698,180},{659,180},{587,250},{0,150}
};
const Nota PANTERA_ROSA[] = {
  {311,150},{330,250},{0,50},{370,150},{392,250},{0,50},{311,150},{330,200},{370,150},{392,200},
  {523,200},{494,300},{330,150},{392,200},{494,200},{466,350},{0,150}
};
const Nota PIRATAS[] = {
  {294,150},{330,150},{392,250},{392,150},{392,150},{440,150},{494,250},{494,150},
  {440,150},{392,150},{440,300},{0,100},{392,200},{330,200},{294,350},{0,150}
};

const Cancion CANCIONES[] = {
  { "Mario Bros",      MARIO,        sizeof(MARIO)/sizeof(Nota) },
  { "Star Wars",       STAR_WARS,    sizeof(STAR_WARS)/sizeof(Nota) },
  { "Tetris",          TETRIS,       sizeof(TETRIS)/sizeof(Nota) },
  { "Coffin Dance",    ASTRONOMIA,   sizeof(ASTRONOMIA)/sizeof(Nota) },
  { "Pantera Rosa",    PANTERA_ROSA, sizeof(PANTERA_ROSA)/sizeof(Nota) },
  { "Piratas Caribe",  PIRATAS,      sizeof(PIRATAS)/sizeof(Nota) }
};
const int TOTAL_CANCIONES = sizeof(CANCIONES)/sizeof(Cancion);

const Nota BEEP_HAPPY[] = { {523,100}, {659,100}, {784,180} };
const Nota BEEP_WOW[]   = { {1500,260} };
const Nota BEEP_OK[]    = { {880,80} };

const Nota* melodiaActual = nullptr;
int notaIdx = 0, notaTotal = 0;
unsigned long notaFinMs = 0;
bool musicaActiva = false;

void escribirNota(int frec) {
  if (frec <= 0) ledcWrite(BUZZER_CH, 0);
  else           ledcWriteTone(BUZZER_CH, frec);
}

void tocarMelodia(const Nota* notas, int total) {
  melodiaActual = notas;
  notaTotal = total;
  notaIdx = 0;
  musicaActiva = true;
  escribirNota(notas[0].frec);
  notaFinMs = millis() + notas[0].ms;
}

void detenerMusica() {
  musicaActiva = false;
  ledcWrite(BUZZER_CH, 0);
}

void actualizarMusica() {
  if (!musicaActiva) return;
  if (millis() < notaFinMs) return;

  notaIdx++;
  if (notaIdx >= notaTotal) {
    detenerMusica();
    return;
  }
  escribirNota(melodiaActual[notaIdx].frec);
  notaFinMs = millis() + melodiaActual[notaIdx].ms;
}

// =====================================================
// 7. SERVIDOR WEB Y JOYSTICK TÁCTIL
// =====================================================

WebServer server(80);

const char HTTP_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no, maximum-scale=1.0">
  <title>Otto Ninja XL</title>
  <style>
    * { box-sizing: border-box; touch-action: manipulation; }
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; text-align: center; background: #121212; color: #fff; margin: 0; padding: 10px; user-select: none; }
    h1 { color: #00E676; margin: 5px 0 10px 0; font-size: 20px; }
    .btn { display: inline-flex; justify-content: center; align-items: center; font-weight: bold; color: white; background: #0288d1; border: none; border-radius: 10px; box-shadow: 0 4px #01579b; }
    .btn:active { background: #0277bd; box-shadow: 0 1px #01579b; transform: translateY(3px); }
    .btn-stop { width: 100%; max-width: 280px; height: 44px; background: #e53935; box-shadow: 0 4px #b71c1c; font-size: 15px; border-radius: 22px; margin: 8px auto; }
    .row { display: flex; justify-content: center; gap: 8px; flex-wrap: wrap; margin-top: 6px; }
    .btn-action { width: 130px; height: 44px; font-size: 13px; }
    .btn-mode-wheel { background: #ab47bc; box-shadow: 0 4px #7b1fa2; }
    .btn-mode-walk { background: #26a69a; box-shadow: 0 4px #00897b; }
    .btn-walk { background: #ff9800; box-shadow: 0 4px #e65100; }
    .btn-moon { background: #5c6bc0; box-shadow: 0 4px #3949ab; }
    .btn-music { background: #43a047; box-shadow: 0 4px #2e7d32; width: 100px; height: 38px; font-size: 12px; }
    .section { font-size: 11px; color: #888; text-transform: uppercase; letter-spacing: 1px; margin-top: 12px; margin-bottom: 4px; }

    #joyZone {
      width: 190px;
      height: 190px;
      background: #1f1f1f;
      border: 3px solid #333;
      border-radius: 50%;
      margin: 8px auto;
      position: relative;
      touch-action: none;
    }
    #joyStick {
      width: 70px;
      height: 70px;
      background: #00E676;
      border-radius: 50%;
      position: absolute;
      top: 60px;
      left: 60px;
      box-shadow: 0 4px 10px rgba(0,0,0,0.6);
      pointer-events: none;
    }
  </style>
</head>
<body>
  <h1>OTTO NINJA XL</h1>
  <button class="btn btn-stop" onclick="cmd('/stop')">PARAR TODO</button>

  <div class="section">Control Continuo</div>
  <div id="joyZone">
    <div id="joyStick"></div>
  </div>

  <div class="section">Postura del Robot</div>
  <div class="row">
    <button class="btn btn-action btn-mode-wheel" onclick="cmd('/pos_roll')">MODO RUEDA</button>
    <button class="btn btn-action btn-mode-walk" onclick="cmd('/pos_walk')">MODO CAMINAR</button>
  </div>

  <div class="section">Caminar / Acciones</div>
  <div class="row">
    <button class="btn btn-action btn-walk" onclick="cmd('/walk')">CAMINAR</button>
    <button class="btn btn-action btn-moon" onclick="cmd('/moonwalk')">MOONWALK</button>
  </div>

  <div class="section">Expresiones</div>
  <div class="row">
    <button class="btn btn-action" style="background:#00897b; box-shadow:0 4px #004d40; width:100px;" onclick="cmd('/happy')">FELIZ</button>
    <button class="btn btn-action" style="background:#00897b; box-shadow:0 4px #004d40; width:100px;" onclick="cmd('/surprise')">SORPRESA</button>
    <button class="btn btn-action" style="background:#e91e63; box-shadow:0 4px #c2185b; width:100px;" onclick="cmd('/heart')">CORAZON</button>
  </div>

  <div class="section">Musica</div>
  <div class="row">
    <button class="btn btn-action btn-music" onclick="cmd('/music?n=0')">MARIO</button>
    <button class="btn btn-action btn-music" onclick="cmd('/music?n=1')">STAR WARS</button>
    <button class="btn btn-action btn-music" onclick="cmd('/music?n=2')">TETRIS</button>
    <button class="btn btn-action btn-music" onclick="cmd('/music?n=3')">COFFIN DANCE</button>
    <button class="btn btn-action btn-music" onclick="cmd('/music?n=4')">PANTERA ROSA</button>
    <button class="btn btn-action btn-music" onclick="cmd('/music?n=5')">PIRATAS</button>
  </div>
  <button class="btn btn-stop" style="height:34px; font-size:12px; margin-top:8px;" onclick="cmd('/music_stop')">SILENCIAR MUSICA</button>

  <script>
    function cmd(url) { fetch(url).catch(function(){}); }

    const zone = document.getElementById('joyZone');
    const stick = document.getElementById('joyStick');
    const radioMax = 60;
    const centro = 95;
    let activo = false;
    let ultEnvio = 0;
    let lastX = 0, lastY = 0;

    function mover(e) {
      if (!activo) return;
      const rect = zone.getBoundingClientRect();
      const touch = e.touches ? e.touches[0] : e;
      let dx = touch.clientX - rect.left - centro;
      let dy = touch.clientY - rect.top - centro;
      let dist = Math.sqrt(dx*dx + dy*dy);

      if (dist > radioMax) {
        dx = (dx / dist) * radioMax;
        dy = (dy / dist) * radioMax;
      }

      stick.style.transform = `translate(${dx}px, ${dy}px)`;

      lastX = Math.round((dx / radioMax) * 100);
      lastY = Math.round((-dy / radioMax) * 100);

      let ahora = Date.now();
      if (ahora - ultEnvio > 80) {
        fetch(`/joy?x=${lastX}&y=${lastY}`).catch(()=>{});
        ultEnvio = ahora;
      }
    }

    function parar() {
      if (!activo) return;
      activo = false;
      stick.style.transform = 'translate(0px, 0px)';
      fetch('/joy?x=0&y=0').catch(()=>{});
    }

    // Reenvía la posición mientras mantienes el dedo quieto,
    // así el robot no se detiene solo por el watchdog
    setInterval(function() {
      if (activo) fetch(`/joy?x=${lastX}&y=${lastY}`).catch(()=>{});
    }, 250);

    zone.addEventListener('touchstart', (e)=>{ activo = true; mover(e); }, {passive: false});
    window.addEventListener('touchmove', (e)=>{ if(activo) { e.preventDefault(); mover(e); } }, {passive: false});
    window.addEventListener('touchend', parar);

    zone.addEventListener('mousedown', (e)=>{ activo = true; mover(e); });
    window.addEventListener('mousemove', (e)=>{ if(activo) mover(e); });
    window.addEventListener('mouseup', parar);
  </script>
</body>
</html>
)rawliteral";

void responderOK() {
  server.send(200, "text/plain; charset=utf-8", "OK");
}

// =====================================================
// 8. SETUP
// =====================================================

void setup() {
  Serial.begin(115200);

  ojos.begin();
  randomSeed(esp_random());
  sigAccionOjos = millis() + 3000;

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  servoRightLeg.setPeriodHertz(50);
  servoRightFoot.setPeriodHertz(50);
  servoLeftFoot.setPeriodHertz(50);
  servoLeftLeg.setPeriodHertz(50);

  servoRightLeg.attach(PIN_RIGHT_LEG);   // D18
  servoRightFoot.attach(PIN_RIGHT_FOOT); // D13
  servoLeftFoot.attach(PIN_LEFT_FOOT);   // D4
  servoLeftLeg.attach(PIN_LEFT_LEG);     // D27

  escribirPiernas(LNP, RNP);
  frenarRuedas();

  pinMode(PIN_BUZZER, OUTPUT);
  ledcSetup(BUZZER_CH, 2000, 8);
  ledcAttachPin(PIN_BUZZER, BUZZER_CH);
  ledcWrite(BUZZER_CH, 0);

  WiFi.softAP("Otto_Ninja_XL", "12345678");

  Serial.println("\nOtto Ninja XL Listo.");
  Serial.print("Conectate a 'Otto_Ninja_XL' y entra a: http://");
  Serial.println(WiFi.softAPIP());

  server.on("/", []() { server.send_P(200, "text/html; charset=utf-8", HTTP_PAGE); });

  // Joystick continuo (modo rueda y modo caminar)
  server.on("/joy", []() {
    int x = server.arg("x").toInt();
    int y = server.arg("y").toInt();
    aplicarJoystick(x, y);
    responderOK();
  });

  // Solo frena: no mueve las piernas
  server.on("/stop", []() {
    frenarRuedas();
    marchaActiva = false;
    detenerMusica();
    ojos.normal();
    finBloqueoExpresion = 0;
    responderOK();
  });

  // Posturas
  server.on("/pos_roll", []() {
    marchaActiva = false;
    modoCaminar = false;
    roll();
    tocarMelodia(BEEP_OK, 1);
    responderOK();
  });
  server.on("/pos_walk", []() {
    marchaActiva = false;
    modoCaminar = false;
    returnToNeutral();
    modoCaminar = true;
    tocarMelodia(BEEP_OK, 1);
    responderOK();
  });

  // Rutinas activas
  server.on("/walk",     []() { steps(); responderOK(); });
  server.on("/moonwalk", []() { moonWalk(); responderOK(); });

  // Expresiones
  server.on("/happy", []() {
    ojos.happy();
    finBloqueoExpresion = millis() + 4000;
    tocarMelodia(BEEP_HAPPY, 3);
    responderOK();
  });
  server.on("/surprise", []() {
    ojos.surprise();
    finBloqueoExpresion = millis() + 4000;
    tocarMelodia(BEEP_WOW, 1);
    responderOK();
  });
  server.on("/heart", []() {
    ojos.heart();
    finBloqueoExpresion = millis() + 4000;
    tocarMelodia(BEEP_HAPPY, 3);
    responderOK();
  });

  // Melodías
  server.on("/music", []() {
    int n = server.arg("n").toInt();
    if (n >= 0 && n < TOTAL_CANCIONES) {
      tocarMelodia(CANCIONES[n].notas, CANCIONES[n].total);
    }
    responderOK();
  });
  server.on("/music_stop", []() { detenerMusica(); responderOK(); });

  server.begin();
}

// =====================================================
// 9. LOOP
// =====================================================

void loop() {
  server.handleClient();
  watchdogJoystick();
  actualizarMarcha();
  actualizarMusica();
  actualizarOjosAleatorios();
}
