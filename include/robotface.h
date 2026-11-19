#ifndef ROBOTFACE_H
#define ROBOTFACE_H

#include <U8g2lib.h>

class RobotEyes {
private:
    U8G2 &oledL;
    U8G2 &oledR;

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
        clear();
        oledL.drawRBox(30, 15, 68, 34, 8);
        oledR.drawRBox(30, 15, 68, 34, 8);
        update();
    }

    void blink() {
        clear();
        oledL.drawBox(30, 30, 68, 4);
        oledR.drawBox(30, 30, 68, 4);
        update();
        delay(120);
        normal();
    }

    void happy() {
        clear();
        oledL.drawCircle(64, 32, 22, U8G2_DRAW_UPPER_RIGHT | U8G2_DRAW_UPPER_LEFT);
        oledR.drawCircle(64, 32, 22, U8G2_DRAW_UPPER_RIGHT | U8G2_DRAW_UPPER_LEFT);
        update();
    }

    void angry() {
        clear();
        // Ojo izquierdo
        oledL.drawRBox(30, 20, 68, 30, 6);
        oledL.drawLine(30, 15, 98, 30);
        // Ojo derecho (espejado)
        oledR.drawRBox(30, 20, 68, 30, 6);
        oledR.drawLine(30, 30, 98, 15);
        update();
    }

    void sad() {
        clear();
        oledL.drawRBox(30, 20, 68, 30, 6);
        oledL.drawLine(30, 30, 98, 15);
        oledR.drawRBox(30, 20, 68, 30, 6);
        oledR.drawLine(30, 15, 98, 30);
        update();
    }

    void wink() {
        clear();
        oledL.drawRBox(30, 15, 68, 34, 8); // Abierto
        oledR.drawBox(30, 30, 68, 4);       // Cerrado
        update();
    }

    void heart() {
        clear();
        // Corazón en cada pantalla (centrado en X=64, Y=30)
        oledL.drawDisc(54, 25, 12);
        oledL.drawDisc(74, 25, 12);
        oledL.drawTriangle(42, 27, 86, 27, 64, 52);

        oledR.drawDisc(54, 25, 12);
        oledR.drawDisc(74, 25, 12);
        oledR.drawTriangle(42, 27, 86, 27, 64, 52);
        update();
    }

    void error() {
        clear();
        oledL.setFont(u8g2_font_ncenB18_tr);
        oledR.setFont(u8g2_font_ncenB18_tr);
        oledL.drawStr(50, 42, "X");
        oledR.drawStr(50, 42, "X");
        update();
    }
};

#endif